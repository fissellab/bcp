#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <fcntl.h>
#include <stddef.h>

// ===== DEFINITIONS =====
#define PI_IP_ADDRESS      "192.168.0.23"  // Change to your Pi's IP
#define PORT               65432
#define BUFFER_SIZE        4096
#define CHUNK_DURATION     600              // 10 minutes in seconds
#define PRINT_INTERVAL     10000            // Print status every 10,000 samples
#define MAX_RETRIES        5
#define RETRY_DELAY        1000000          // 1 second in microseconds
#define NUM_SENSORS        5

// ===== PACKET FORMAT (MUST MATCH TRANSMITTER) =====
typedef struct __attribute__((packed)) {
    uint32_t magic;           // 0xDEADBEEF for validation
    uint32_t timestamp_sec;   // Unix timestamp seconds
    uint32_t timestamp_nsec;  // Nanoseconds
    uint16_t sequence;        // Packet sequence number
    uint8_t  sensor_mask;     // Bit flags: which sensors have data
    uint8_t  reserved;        // Padding for alignment
} packet_header_t;

typedef struct __attribute__((packed)) {
    uint8_t sensor_id;        // 1-3 for accelerometers
    float x, y, z;           // m/s²
} accel_sample_t;

typedef struct __attribute__((packed)) {
    float rate;              // degrees/sec
} gyro_spi_sample_t;

typedef struct __attribute__((packed)) {
    float x, y, z;           // degrees/sec
    float temperature;       // °C
} gyro_i2c_sample_t;

typedef struct __attribute__((packed)) {
    packet_header_t header;
    accel_sample_t accels[3];          // Always present (1000Hz)
    gyro_i2c_sample_t gyro_i2c;        // Always present (1000Hz)
    gyro_spi_sample_t gyro_spi;        // Present every 4th packet (250Hz)
} sensor_packet_t;

// ===== FILE HEADER FORMAT =====
typedef struct __attribute__((packed)) {
    uint32_t magic;           // 0xDADAFEED
    uint32_t sensor_type;     // Sensor type identifier
    uint32_t sample_count;    // Number of samples in file (updated at close)
    double   start_timestamp; // First sample time
    char     sensor_name[32]; // Human readable sensor name
    uint32_t reserved[4];     // Future expansion
} file_header_t;

// ===== SENSOR DATA STRUCTURES FOR STORAGE =====
typedef struct __attribute__((packed)) {
    double timestamp;         // Unix timestamp with nanoseconds
    float x, y, z;           // Sensor data
} accel_file_sample_t;

typedef struct __attribute__((packed)) {
    double timestamp;         // Unix timestamp with nanoseconds
    float rate;              // Rate in degrees/sec
} gyro_spi_file_sample_t;

typedef struct __attribute__((packed)) {
    double timestamp;         // Unix timestamp with nanoseconds
    float x, y, z;           // Gyro rates in degrees/sec
    float temperature;       // Temperature in °C
} gyro_i2c_file_sample_t;

// ===== GLOBAL VARIABLES =====
volatile sig_atomic_t keep_running = 1;
FILE *log_file = NULL;

// File management
FILE *current_files[NUM_SENSORS] = {NULL}; // accel1, accel2, accel3, gyro_i2c, gyro_spi
int chunk_numbers[NUM_SENSORS] = {1, 1, 1, 1, 1};
double chunk_start_times[NUM_SENSORS] = {0, 0, 0, 0, 0};
long samples_received[NUM_SENSORS] = {0, 0, 0, 0, 0};
double start_times[NUM_SENSORS] = {0, 0, 0, 0, 0};
char output_folders[NUM_SENSORS][256];

// Packet management
uint16_t last_sequence = 0;
uint32_t packets_received = 0;
uint32_t packets_lost = 0;

// ===== SENSOR NAMES =====
const char* sensor_names[NUM_SENSORS] = {
    "ADXL355_Accel_1",
    "ADXL355_Accel_2", 
    "ADXL355_Accel_3",
    "IAM20380HT_Gyro_I2C",
    "ADXRS453_Gyro_SPI"
};

const char* folder_names[NUM_SENSORS] = {
    "accel1",
    "accel2",
    "accel3", 
    "gyro_i2c",
    "gyro_spi"
};

// ===== SIGNAL HANDLING =====
void signal_handler(int signum) {
    keep_running = 0;
}

// ===== LOGGING FUNCTIONS =====
void log_message(const char *message) {
    time_t now;
    char timestamp[64];
    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Print to stderr for immediate feedback
    fprintf(stderr, "[%s] %s\n", timestamp, message);
    
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fflush(log_file);
    }
}

void create_log_file() {
    char log_folder[256] = "logs";
    char log_filename[512];
    char full_path[768];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    mkdir(log_folder, 0777);

    strftime(log_filename, sizeof(log_filename), "%Y-%m-%d_%H-%M-%S_pos_sensors_rx.log", t);
    snprintf(full_path, sizeof(full_path), "%s/%s", log_folder, log_filename);

    log_file = fopen(full_path, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Error creating log file '%s': %s\n", full_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// ===== DIRECTORY MANAGEMENT =====
void create_output_folders(char *base_output_folder) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char folder_name[256];

    strftime(folder_name, sizeof(folder_name), "outputs/%d-%m-%Y-%H-%M-pos-sensors-output", tm);
    sprintf(base_output_folder, "%s", folder_name);

    mkdir("outputs", 0777);
    mkdir(base_output_folder, 0777);

    for (int i = 0; i < NUM_SENSORS; i++) {
        snprintf(output_folders[i], 256, "%s/%s", base_output_folder, folder_names[i]);
        mkdir(output_folders[i], 0777);
    }

    log_message("Created output folders");
}

// ===== FILE MANAGEMENT =====
FILE* open_new_file(int sensor_id, int chunk_number, double start_time) {
    char filename[256];
    sprintf(filename, "%s/%.6f_chunk_%04d.bin", output_folders[sensor_id], start_time, chunk_number);
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Error opening file for %s: %s", 
                sensor_names[sensor_id], strerror(errno));
        log_message(error_msg);
        exit(EXIT_FAILURE);
    }

    // Write file header
    file_header_t header = {0};
    header.magic = 0xDADAFEED;
    header.sensor_type = sensor_id;
    header.sample_count = 0; // Will be updated when file is closed
    header.start_timestamp = start_time;
    strncpy(header.sensor_name, sensor_names[sensor_id], sizeof(header.sensor_name) - 1);
    
    fwrite(&header, sizeof(file_header_t), 1, file);
    fflush(file);

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Opened new file for %s: %s", sensor_names[sensor_id], filename);
    log_message(log_msg);
    return file;
}

void close_and_update_file(int sensor_id) {
    if (current_files[sensor_id] != NULL) {
        // Update sample count in header
        fseek(current_files[sensor_id], offsetof(file_header_t, sample_count), SEEK_SET);
        uint32_t sample_count = samples_received[sensor_id];
        fwrite(&sample_count, sizeof(uint32_t), 1, current_files[sensor_id]);
        
        fclose(current_files[sensor_id]);
        current_files[sensor_id] = NULL;
        
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Closed file for %s with %ld samples", 
                sensor_names[sensor_id], samples_received[sensor_id]);
        log_message(log_msg);
    }
}

// ===== DATA WRITING FUNCTIONS =====
void write_accel_data(int sensor_id, double timestamp, float x, float y, float z) {
    accel_file_sample_t sample = {
        .timestamp = timestamp,
        .x = x,
        .y = y, 
        .z = z
    };
    
    fwrite(&sample, sizeof(accel_file_sample_t), 1, current_files[sensor_id]);
    fflush(current_files[sensor_id]);
}

void write_gyro_spi_data(double timestamp, float rate) {
    int sensor_id = 4; // gyro_spi
    gyro_spi_file_sample_t sample = {
        .timestamp = timestamp,
        .rate = rate
    };
    
    fwrite(&sample, sizeof(gyro_spi_file_sample_t), 1, current_files[sensor_id]);
    fflush(current_files[sensor_id]);
}

void write_gyro_i2c_data(double timestamp, float x, float y, float z, float temperature) {
    int sensor_id = 3; // gyro_i2c
    gyro_i2c_file_sample_t sample = {
        .timestamp = timestamp,
        .x = x,
        .y = y,
        .z = z,
        .temperature = temperature
    };
    
    fwrite(&sample, sizeof(gyro_i2c_file_sample_t), 1, current_files[sensor_id]);
    fflush(current_files[sensor_id]);
}

// ===== PACKET VALIDATION =====
int validate_packet(const sensor_packet_t *packet) {
    if (packet->header.magic != 0xDEADBEEF) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Invalid packet magic: 0x%08X", packet->header.magic);
        log_message(error_msg);
        return 0;
    }
    return 1;
}

void check_sequence_number(uint16_t sequence) {
    if (packets_received > 0) {
        uint16_t expected = (last_sequence + 1) & 0xFFFF;
        if (sequence != expected) {
            if (sequence > expected) {
                packets_lost += (sequence - expected);
            } else {
                packets_lost += (65536 - expected + sequence);
            }
            char warning[256];
            snprintf(warning, sizeof(warning), "Packet loss detected: expected %u, got %u. Total lost: %u",
                    expected, sequence, packets_lost);
            log_message(warning);
        }
    }
    last_sequence = sequence;
    packets_received++;
}

// ===== CHUNK MANAGEMENT =====
void check_chunk_rotation(int sensor_id, double timestamp) {
    if (chunk_start_times[sensor_id] > 0 && 
        timestamp - chunk_start_times[sensor_id] >= CHUNK_DURATION) {
        
        close_and_update_file(sensor_id);
        chunk_numbers[sensor_id]++;
        chunk_start_times[sensor_id] = timestamp;
        current_files[sensor_id] = open_new_file(sensor_id, chunk_numbers[sensor_id], timestamp);
        
        // Reset sample count for new chunk
        samples_received[sensor_id] = 0;
    }
}

// ===== DATA PROCESSING =====
void process_sensor_packet(const sensor_packet_t *packet) {
    double timestamp = packet->header.timestamp_sec + packet->header.timestamp_nsec / 1000000000.0;
    
    // Process accelerometer data (sensors 0-2)
    for (int i = 0; i < 3; i++) {
        int sensor_id = i;
        
        // Initialize start time and file if needed
        if (start_times[sensor_id] == 0) {
            start_times[sensor_id] = timestamp;
            chunk_start_times[sensor_id] = timestamp;
            current_files[sensor_id] = open_new_file(sensor_id, chunk_numbers[sensor_id], timestamp);
            
            char msg[256];
            snprintf(msg, sizeof(msg), "First sample received for %s at timestamp %.6f",
                    sensor_names[sensor_id], timestamp);
            log_message(msg);
        }
        
        // Write data
        write_accel_data(sensor_id, timestamp, 
                        packet->accels[i].x, 
                        packet->accels[i].y, 
                        packet->accels[i].z);
        
        samples_received[sensor_id]++;
        
        // Check for chunk rotation
        check_chunk_rotation(sensor_id, timestamp);
        
        // Status logging
        if (samples_received[sensor_id] % PRINT_INTERVAL == 0) {
            time_t current_time_t = time(NULL);
            double elapsed_time = difftime(current_time_t, start_times[sensor_id]);
            double average_rate = samples_received[sensor_id] / (timestamp - start_times[sensor_id]);

            char status_msg[512];
            snprintf(status_msg, sizeof(status_msg), 
                    "%s - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz",
                    sensor_names[sensor_id], samples_received[sensor_id], elapsed_time, average_rate);
            log_message(status_msg);
        }
    }
    
    // Process I2C gyro data (sensor 3)
    int gyro_i2c_id = 3;
    if (start_times[gyro_i2c_id] == 0) {
        start_times[gyro_i2c_id] = timestamp;
        chunk_start_times[gyro_i2c_id] = timestamp;
        current_files[gyro_i2c_id] = open_new_file(gyro_i2c_id, chunk_numbers[gyro_i2c_id], timestamp);
        
        char msg[256];
        snprintf(msg, sizeof(msg), "First sample received for %s at timestamp %.6f",
                sensor_names[gyro_i2c_id], timestamp);
        log_message(msg);
    }
    
    write_gyro_i2c_data(timestamp,
                       packet->gyro_i2c.x,
                       packet->gyro_i2c.y, 
                       packet->gyro_i2c.z,
                       packet->gyro_i2c.temperature);
    
    samples_received[gyro_i2c_id]++;
    check_chunk_rotation(gyro_i2c_id, timestamp);
    
    if (samples_received[gyro_i2c_id] % PRINT_INTERVAL == 0) {
        time_t current_time_t = time(NULL);
        double elapsed_time = difftime(current_time_t, start_times[gyro_i2c_id]);
        double average_rate = samples_received[gyro_i2c_id] / (timestamp - start_times[gyro_i2c_id]);

        char status_msg[512];
        snprintf(status_msg, sizeof(status_msg), 
                "%s - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz",
                sensor_names[gyro_i2c_id], samples_received[gyro_i2c_id], elapsed_time, average_rate);
        log_message(status_msg);
    }
    
    // Process SPI gyro data (sensor 4) - only if present in packet
    if (packet->header.sensor_mask & 0x10) {
        int gyro_spi_id = 4;
        
        if (start_times[gyro_spi_id] == 0) {
            start_times[gyro_spi_id] = timestamp;
            chunk_start_times[gyro_spi_id] = timestamp;
            current_files[gyro_spi_id] = open_new_file(gyro_spi_id, chunk_numbers[gyro_spi_id], timestamp);
            
            char msg[256];
            snprintf(msg, sizeof(msg), "First sample received for %s at timestamp %.6f",
                    sensor_names[gyro_spi_id], timestamp);
            log_message(msg);
        }
        
        write_gyro_spi_data(timestamp, packet->gyro_spi.rate);
        
        samples_received[gyro_spi_id]++;
        check_chunk_rotation(gyro_spi_id, timestamp);
        
        if (samples_received[gyro_spi_id] % (PRINT_INTERVAL/4) == 0) { // Less frequent for 250Hz
            time_t current_time_t = time(NULL);
            double elapsed_time = difftime(current_time_t, start_times[gyro_spi_id]);
            double average_rate = samples_received[gyro_spi_id] / (timestamp - start_times[gyro_spi_id]);

            char status_msg[512];
            snprintf(status_msg, sizeof(status_msg), 
                    "%s - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz",
                    sensor_names[gyro_spi_id], samples_received[gyro_spi_id], elapsed_time, average_rate);
            log_message(status_msg);
        }
    }
}

// ===== MAIN FUNCTION =====
int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    char base_output_folder[256];
    sensor_packet_t packet;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    create_log_file();
    log_message("Position sensors receiver started");

    create_output_folders(base_output_folder);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, PI_IP_ADDRESS, &serv_addr.sin_addr) <= 0) {
        log_message("Invalid address/ Address not supported");
        return -1;
    }

    // Connect with retries
    int retry_count = 0;
    while (retry_count < MAX_RETRIES && keep_running) {
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Connection Failed. Retrying... (%d/%d)", 
                    retry_count + 1, MAX_RETRIES);
            log_message(error_msg);
            usleep(RETRY_DELAY);
            retry_count++;
        } else {
            break;
        }
    }

    if (retry_count == MAX_RETRIES) {
        log_message("Max retries reached. Exiting.");
        return -1;
    }

    log_message("Connected to Pi. Starting data collection...");

    // Main reception loop
    while (keep_running) {
        int bytes_received = 0;
        int total_bytes = 0;
        int packet_size = sizeof(sensor_packet_t);
        
        // Receive complete packet (may need multiple recv calls)
        while (total_bytes < packet_size && keep_running) {
            bytes_received = recv(sock, buffer + total_bytes, packet_size - total_bytes, 0);
            
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    log_message("Pi closed the connection");
                } else {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg), "recv failed: %s", strerror(errno));
                    log_message(error_msg);
                }
                keep_running = 0;
                break;
            }
            
            total_bytes += bytes_received;
        }
        
        if (total_bytes == packet_size) {
            // Copy to packet structure
            memcpy(&packet, buffer, sizeof(sensor_packet_t));
            
            // Validate and process packet
            if (validate_packet(&packet)) {
                check_sequence_number(packet.header.sequence);
                process_sensor_packet(&packet);
            }
        }
    }

    // Cleanup
    for (int i = 0; i < NUM_SENSORS; i++) {
        close_and_update_file(i);
    }

    close(sock);

    // Final statistics
    char final_msg[1024];
    snprintf(final_msg, sizeof(final_msg), 
            "Data collection complete.\n"
            "Total packets: %u, Lost packets: %u (%.2f%% loss)\n"
            "Samples received - %s: %ld, %s: %ld, %s: %ld, %s: %ld, %s: %ld",
            packets_received, packets_lost, 
            packets_received > 0 ? (packets_lost * 100.0 / packets_received) : 0.0,
            sensor_names[0], samples_received[0],
            sensor_names[1], samples_received[1], 
            sensor_names[2], samples_received[2],
            sensor_names[3], samples_received[3],
            sensor_names[4], samples_received[4]);
    log_message(final_msg);

    if (log_file) {
        fclose(log_file);
    }
    
    return 0;
}