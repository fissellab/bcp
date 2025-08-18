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
#include <fcntl.h>

#define PORT 65432
#define BUFFER_SIZE 4096
#define CHUNK_DURATION 600 // 10 minutes in seconds
#define PRINT_INTERVAL 10000 // Print status every 10,000 samples
#define MAX_RETRIES 5
#define RETRY_DELAY 1000000 // 1 second in microseconds
#define NUM_ACCELEROMETERS 3
#define NUM_SPI_GYROSCOPES 1
#define NUM_I2C_GYROSCOPES 1
#define TOTAL_SENSORS (NUM_ACCELEROMETERS + NUM_SPI_GYROSCOPES + NUM_I2C_GYROSCOPES)

volatile sig_atomic_t keep_running = 1;
FILE *log_file = NULL;

void signal_handler(int signum) {
    keep_running = 0;
}

void log_message(const char *message) {
    time_t now;
    char timestamp[64];
    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file);
}

void create_log_file() {
    char log_folder[256] = "logs";
    char log_filename[512];
    char full_path[768];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    mkdir(log_folder, 0777);
    
    strftime(log_filename, sizeof(log_filename), "%Y-%m-%d_%H-%M-%S_pos_sensor_rx.log", t);
    
    snprintf(full_path, sizeof(full_path), "%s/%s", log_folder, log_filename);
    
    log_file = fopen(full_path, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Error creating log file '%s': %s\n", full_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void create_output_folders(char *base_output_folder, 
                          char accel_folders[NUM_ACCELEROMETERS][256], 
                          char spi_gyro_folders[NUM_SPI_GYROSCOPES][256],
                          char i2c_gyro_folders[NUM_I2C_GYROSCOPES][256]) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char folder_name[256];
    
    strftime(folder_name, sizeof(folder_name), "outputs/%d-%m-%Y-%H-%M-pos-sensor-output", tm);
    sprintf(base_output_folder, "%s", folder_name);
    
    mkdir("outputs", 0777);
    mkdir(base_output_folder, 0777);
    
    // Create accelerometer folders
    for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
        snprintf(accel_folders[i], 256, "%s/accl%d", base_output_folder, i+1);
        mkdir(accel_folders[i], 0777);
    }
    
    // Create SPI gyroscope folders
    for (int i = 0; i < NUM_SPI_GYROSCOPES; i++) {
        snprintf(spi_gyro_folders[i], 256, "%s/gyro%d", base_output_folder, i+1);
        mkdir(spi_gyro_folders[i], 0777);
    }
    
    // Create I2C gyroscope folders
    for (int i = 0; i < NUM_I2C_GYROSCOPES; i++) {
        snprintf(i2c_gyro_folders[i], 256, "%s/gyro_i2c%d", base_output_folder, i+1);
        mkdir(i2c_gyro_folders[i], 0777);
    }
    
    log_message("Created output folders for all position sensors (3 accelerometers + 2 gyroscopes)");
}

FILE* open_new_file(const char *output_folder, int chunk_number, double start_time) {
    char filename[256];
    sprintf(filename, "%s/%.6f_chunk_%04d.bin", output_folder, start_time, chunk_number);
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Error opening file: %s", strerror(errno));
        log_message(error_msg);
        exit(EXIT_FAILURE);
    }
    setvbuf(file, NULL, _IOFBF, BUFFER_SIZE);  // Set to full buffering
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Opened new file: %s", filename);
    log_message(log_msg);
    return file;
}

void process_accelerometer_data(char *line, FILE **accel_files, int *accel_chunk_numbers, 
                               double *accel_chunk_start_times, long *accel_samples_received, 
                               double *accel_start_times, char accel_folders[NUM_ACCELEROMETERS][256],
                               time_t start_time_t) {
    int accel_id;
    double timestamp, x, y, z;
    
    if (sscanf(line, "%d,%lf,%lf,%lf,%lf", &accel_id, &timestamp, &x, &y, &z) == 5) {
        accel_id--; // Convert to 0-based index
        if (accel_id >= 0 && accel_id < NUM_ACCELEROMETERS) {
            if (accel_start_times[accel_id] == 0) accel_start_times[accel_id] = timestamp;
            
            if (accel_files[accel_id] == NULL) {
                accel_chunk_start_times[accel_id] = timestamp;
                accel_files[accel_id] = open_new_file(accel_folders[accel_id], accel_chunk_numbers[accel_id], accel_chunk_start_times[accel_id]);
            }
            
            // Write accelerometer data: timestamp + x + y + z (4 doubles)
            fwrite(&timestamp, sizeof(double), 1, accel_files[accel_id]);
            fwrite(&x, sizeof(double), 1, accel_files[accel_id]);
            fwrite(&y, sizeof(double), 1, accel_files[accel_id]);
            fwrite(&z, sizeof(double), 1, accel_files[accel_id]);
            
            accel_samples_received[accel_id]++;
            
            if (accel_samples_received[accel_id] % PRINT_INTERVAL == 0) {
                time_t current_time_t;
                time(&current_time_t);
                double elapsed_time = difftime(current_time_t, start_time_t);
                double average_rate = accel_samples_received[accel_id] / (timestamp - accel_start_times[accel_id]);
                
                char status_msg[512];
                snprintf(status_msg, sizeof(status_msg), "Accelerometer %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                        accel_id + 1, accel_samples_received[accel_id], elapsed_time, average_rate);
                log_message(status_msg);
            }
            
            if (timestamp - accel_chunk_start_times[accel_id] >= CHUNK_DURATION) {
                fclose(accel_files[accel_id]);
                accel_chunk_numbers[accel_id]++;
                accel_files[accel_id] = open_new_file(accel_folders[accel_id], accel_chunk_numbers[accel_id], timestamp);
                accel_chunk_start_times[accel_id] = timestamp;
            }
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Warning: Invalid accelerometer ID: %d", accel_id + 1);
            log_message(error_msg);
        }
    } else {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Warning: Invalid accelerometer data format: %s", line);
        log_message(error_msg);
    }
}

void process_spi_gyroscope_data(char *line, FILE **spi_gyro_files, int *spi_gyro_chunk_numbers, 
                               double *spi_gyro_chunk_start_times, long *spi_gyro_samples_received, 
                               double *spi_gyro_start_times, char spi_gyro_folders[NUM_SPI_GYROSCOPES][256],
                               time_t start_time_t) {
    char gyro_id_str[10];
    double timestamp, rate;
    
    if (sscanf(line, "%9[^,],%lf,%lf", gyro_id_str, &timestamp, &rate) == 3) {
        // Parse SPI gyro ID (expecting "G1", "G2", etc.)
        int gyro_id = -1;
        if (strncmp(gyro_id_str, "G", 1) == 0) {
            gyro_id = atoi(gyro_id_str + 1) - 1; // Convert to 0-based index
        }
        
        if (gyro_id >= 0 && gyro_id < NUM_SPI_GYROSCOPES) {
            if (spi_gyro_start_times[gyro_id] == 0) spi_gyro_start_times[gyro_id] = timestamp;
            
            if (spi_gyro_files[gyro_id] == NULL) {
                spi_gyro_chunk_start_times[gyro_id] = timestamp;
                spi_gyro_files[gyro_id] = open_new_file(spi_gyro_folders[gyro_id], spi_gyro_chunk_numbers[gyro_id], spi_gyro_chunk_start_times[gyro_id]);
            }
            
            // Write SPI gyroscope data: timestamp + rate (2 doubles)
            fwrite(&timestamp, sizeof(double), 1, spi_gyro_files[gyro_id]);
            fwrite(&rate, sizeof(double), 1, spi_gyro_files[gyro_id]);
            
            spi_gyro_samples_received[gyro_id]++;
            
            if (spi_gyro_samples_received[gyro_id] % PRINT_INTERVAL == 0) {
                time_t current_time_t;
                time(&current_time_t);
                double elapsed_time = difftime(current_time_t, start_time_t);
                double average_rate = spi_gyro_samples_received[gyro_id] / (timestamp - spi_gyro_start_times[gyro_id]);
                
                char status_msg[512];
                snprintf(status_msg, sizeof(status_msg), "SPI Gyroscope %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                        gyro_id + 1, spi_gyro_samples_received[gyro_id], elapsed_time, average_rate);
                log_message(status_msg);
            }
            
            if (timestamp - spi_gyro_chunk_start_times[gyro_id] >= CHUNK_DURATION) {
                fclose(spi_gyro_files[gyro_id]);
                spi_gyro_chunk_numbers[gyro_id]++;
                spi_gyro_files[gyro_id] = open_new_file(spi_gyro_folders[gyro_id], spi_gyro_chunk_numbers[gyro_id], timestamp);
                spi_gyro_chunk_start_times[gyro_id] = timestamp;
            }
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Warning: Invalid SPI gyroscope ID: %s", gyro_id_str);
            log_message(error_msg);
        }
    } else {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Warning: Invalid SPI gyroscope data format: %s", line);
        log_message(error_msg);
    }
}

void process_i2c_gyroscope_data(char *line, FILE **i2c_gyro_files, int *i2c_gyro_chunk_numbers, 
                               double *i2c_gyro_chunk_start_times, long *i2c_gyro_samples_received, 
                               double *i2c_gyro_start_times, char i2c_gyro_folders[NUM_I2C_GYROSCOPES][256],
                               time_t start_time_t) {
    char gyro_id_str[10];
    double timestamp, x, y, z, temp;
    
    if (sscanf(line, "%9[^,],%lf,%lf,%lf,%lf,%lf", gyro_id_str, &timestamp, &x, &y, &z, &temp) == 6) {
        // Parse I2C gyro ID (expecting "I1", "I2", etc.)
        int gyro_id = -1;
        if (strncmp(gyro_id_str, "I", 1) == 0) {
            gyro_id = atoi(gyro_id_str + 1) - 1; // Convert to 0-based index
        }
        
        if (gyro_id >= 0 && gyro_id < NUM_I2C_GYROSCOPES) {
            if (i2c_gyro_start_times[gyro_id] == 0) i2c_gyro_start_times[gyro_id] = timestamp;
            
            if (i2c_gyro_files[gyro_id] == NULL) {
                i2c_gyro_chunk_start_times[gyro_id] = timestamp;
                i2c_gyro_files[gyro_id] = open_new_file(i2c_gyro_folders[gyro_id], i2c_gyro_chunk_numbers[gyro_id], i2c_gyro_chunk_start_times[gyro_id]);
            }
            
            // Write I2C gyroscope data: timestamp + x + y + z + temp (5 doubles)
            fwrite(&timestamp, sizeof(double), 1, i2c_gyro_files[gyro_id]);
            fwrite(&x, sizeof(double), 1, i2c_gyro_files[gyro_id]);
            fwrite(&y, sizeof(double), 1, i2c_gyro_files[gyro_id]);
            fwrite(&z, sizeof(double), 1, i2c_gyro_files[gyro_id]);
            fwrite(&temp, sizeof(double), 1, i2c_gyro_files[gyro_id]);
            
            i2c_gyro_samples_received[gyro_id]++;
            
            if (i2c_gyro_samples_received[gyro_id] % PRINT_INTERVAL == 0) {
                time_t current_time_t;
                time(&current_time_t);
                double elapsed_time = difftime(current_time_t, start_time_t);
                double average_rate = i2c_gyro_samples_received[gyro_id] / (timestamp - i2c_gyro_start_times[gyro_id]);
                
                char status_msg[512];
                snprintf(status_msg, sizeof(status_msg), "I2C Gyroscope %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                        gyro_id + 1, i2c_gyro_samples_received[gyro_id], elapsed_time, average_rate);
                log_message(status_msg);
            }
            
            if (timestamp - i2c_gyro_chunk_start_times[gyro_id] >= CHUNK_DURATION) {
                fclose(i2c_gyro_files[gyro_id]);
                i2c_gyro_chunk_numbers[gyro_id]++;
                i2c_gyro_files[gyro_id] = open_new_file(i2c_gyro_folders[gyro_id], i2c_gyro_chunk_numbers[gyro_id], timestamp);
                i2c_gyro_chunk_start_times[gyro_id] = timestamp;
            }
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Warning: Invalid I2C gyroscope ID: %s", gyro_id_str);
            log_message(error_msg);
        }
    } else {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Warning: Invalid I2C gyroscope data format: %s", line);
        log_message(error_msg);
    }
}

#define PACKET_MAGIC 0xDEADBEEF

typedef struct {
    uint32_t magic;            // 0xDEADBEEF
    uint16_t sequence;         // sequence number
    uint16_t sensor_mask;      // bitmask of active sensors
    uint32_t timestamp_sec;    // seconds
    uint32_t timestamp_nsec;   // nanoseconds
} pos_packet_header_t;

typedef struct { float x, y, z; } pos_accel_sample_t;
typedef struct { float rate; } pos_gyro_spi_sample_t;
typedef struct { float x, y, z, temp; } pos_gyro_i2c_sample_t;

typedef struct {
    pos_packet_header_t header;
    pos_accel_sample_t  accels[NUM_ACCELEROMETERS];
    pos_gyro_i2c_sample_t gyro_i2c;
    pos_gyro_spi_sample_t gyro_spi;
} pos_sensor_packet_t;

// Compile-time checks (TX and RX must match these sizes)
_Static_assert(sizeof(pos_packet_header_t) == 16, "header size mismatch");
_Static_assert(sizeof(pos_accel_sample_t) == 12, "accel size mismatch");
_Static_assert(sizeof(pos_gyro_i2c_sample_t) == 16, "i2c size mismatch");
_Static_assert(sizeof(pos_gyro_spi_sample_t) == 4,  "spi size mismatch");
_Static_assert(sizeof(pos_sensor_packet_t) == 72,   "packet size mismatch");

// Robust read: get exactly len bytes or fail
static int recv_all(int sock, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t r = recv(sock, (char*)buf + got, len - got, 0);
        if (r == 0) return 0; // peer closed
        if (r < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
        got += (size_t)r;
    }
    return 1;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char base_output_folder[256];
    char accel_folders[NUM_ACCELEROMETERS][256];
    char spi_gyro_folders[NUM_SPI_GYROSCOPES][256];
    char i2c_gyro_folders[NUM_I2C_GYROSCOPES][256];
    char incomplete_line[256] = {0};
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    create_log_file();
    log_message("Complete position sensor receiver started (3 accelerometers + 2 gyroscopes)");
    
    create_output_folders(base_output_folder, accel_folders, spi_gyro_folders, i2c_gyro_folders);
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message("Socket creation error");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, "172.20.4.209", &serv_addr.sin_addr) <= 0) {
        log_message("Invalid address/ Address not supported");
        return -1;
    }
    
    int retry_count = 0;
    while (retry_count < MAX_RETRIES) {
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Connection Failed. Retrying... (%d/%d)", retry_count + 1, MAX_RETRIES);
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
    
    log_message("Connected to server. Starting position sensor data collection...");
    
    // Accelerometer file management
    FILE *accel_files[NUM_ACCELEROMETERS] = {NULL};
    int accel_chunk_numbers[NUM_ACCELEROMETERS] = {1, 1, 1};
    double accel_chunk_start_times[NUM_ACCELEROMETERS] = {0, 0, 0};
    long accel_samples_received[NUM_ACCELEROMETERS] = {0, 0, 0};
    double accel_start_times[NUM_ACCELEROMETERS] = {0, 0, 0};
    
    // SPI gyroscope file management
    FILE *spi_gyro_files[NUM_SPI_GYROSCOPES] = {NULL};
    int spi_gyro_chunk_numbers[NUM_SPI_GYROSCOPES] = {1};
    double spi_gyro_chunk_start_times[NUM_SPI_GYROSCOPES] = {0};
    long spi_gyro_samples_received[NUM_SPI_GYROSCOPES] = {0};
    double spi_gyro_start_times[NUM_SPI_GYROSCOPES] = {0};
    
    // I2C gyroscope file management
    FILE *i2c_gyro_files[NUM_I2C_GYROSCOPES] = {NULL};
    int i2c_gyro_chunk_numbers[NUM_I2C_GYROSCOPES] = {1};
    double i2c_gyro_chunk_start_times[NUM_I2C_GYROSCOPES] = {0};
    long i2c_gyro_samples_received[NUM_I2C_GYROSCOPES] = {0};
    double i2c_gyro_start_times[NUM_I2C_GYROSCOPES] = {0};
    
    time_t start_time_t;
    time(&start_time_t);
    
    pos_sensor_packet_t packet;

    while (keep_running) {
        int ok = recv_all(sock, &packet, sizeof(packet));
        if (ok <= 0) break;

        if (packet.header.magic != PACKET_MAGIC) {
            unsigned char *b = (unsigned char*)&packet;
            char hex[200]; int n = 0;
            for (int i = 0; i < 16 && i < (int)sizeof(packet); i++) {
                n += snprintf(hex + n, sizeof(hex) - n, "%02X ", b[i]);
            }
            char msg[256];
            snprintf(msg, sizeof(msg), "Invalid packet (bad magic). First 16 bytes: %s", hex);
            log_message(msg);
            continue;
        }

        double timestamp = packet.header.timestamp_sec +
                           packet.header.timestamp_nsec / 1000000000.0;

        // Accelerometers (mask bits 0..2)
        for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
            if (packet.header.sensor_mask & (1u << i)) {
                if (accel_files[i] == NULL) {
                    accel_chunk_start_times[i] = timestamp;
                    accel_files[i] = open_new_file(accel_folders[i],
                                                   accel_chunk_numbers[i],
                                                   accel_chunk_start_times[i]);
                }
                if (accel_start_times[i] == 0) accel_start_times[i] = timestamp; // ADD

                fwrite(&timestamp, sizeof(double), 1, accel_files[i]);
                fwrite(&packet.accels[i].x, sizeof(float), 1, accel_files[i]);
                fwrite(&packet.accels[i].y, sizeof(float), 1, accel_files[i]);
                fwrite(&packet.accels[i].z, sizeof(float), 1, accel_files[i]);

                accel_samples_received[i]++;
                
                if (accel_samples_received[i] % PRINT_INTERVAL == 0) {
                    time_t current_time_t;
                    time(&current_time_t);
                    double elapsed_time = difftime(current_time_t, start_time_t);
                    double average_rate = accel_samples_received[i] / (timestamp - accel_start_times[i]);
                    
                    char status_msg[512];
                    snprintf(status_msg, sizeof(status_msg), "Accelerometer %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                            i + 1, accel_samples_received[i], elapsed_time, average_rate);
                    log_message(status_msg);
                }
                
                if (timestamp - accel_chunk_start_times[i] >= CHUNK_DURATION) {
                    fclose(accel_files[i]);
                    accel_chunk_numbers[i]++;
                    accel_files[i] = open_new_file(accel_folders[i], accel_chunk_numbers[i], timestamp);
                    accel_chunk_start_times[i] = timestamp;
                }
            }
        }

        // I2C gyro (assume mask bit 3)
        if (packet.header.sensor_mask & (1u << (NUM_ACCELEROMETERS + 0))) {
            int i = 0; // only one I2C gyro
            if (i2c_gyro_files[i] == NULL) {
                i2c_gyro_chunk_start_times[i] = timestamp;
                i2c_gyro_files[i] = open_new_file(i2c_gyro_folders[i],
                                                  i2c_gyro_chunk_numbers[i],
                                                  i2c_gyro_chunk_start_times[i]);
            }
            if (i2c_gyro_start_times[i] == 0) i2c_gyro_start_times[i] = timestamp; // ADD

            fwrite(&timestamp, sizeof(double), 1, i2c_gyro_files[i]);
            fwrite(&packet.gyro_i2c.x, sizeof(float), 1, i2c_gyro_files[i]);
            fwrite(&packet.gyro_i2c.y, sizeof(float), 1, i2c_gyro_files[i]);
            fwrite(&packet.gyro_i2c.z, sizeof(float), 1, i2c_gyro_files[i]);
            fwrite(&packet.gyro_i2c.temp, sizeof(float), 1, i2c_gyro_files[i]);

            i2c_gyro_samples_received[i]++;
            
            if (i2c_gyro_samples_received[i] % PRINT_INTERVAL == 0) {
                time_t current_time_t;
                time(&current_time_t);
                double elapsed_time = difftime(current_time_t, start_time_t);
                double average_rate = i2c_gyro_samples_received[i] / (timestamp - i2c_gyro_start_times[i]);
                
                char status_msg[512];
                snprintf(status_msg, sizeof(status_msg), "I2C Gyroscope %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                        i + 1, i2c_gyro_samples_received[i], elapsed_time, average_rate);
                log_message(status_msg);
            }
            
            if (timestamp - i2c_gyro_chunk_start_times[i] >= CHUNK_DURATION) {
                fclose(i2c_gyro_files[i]);
                i2c_gyro_chunk_numbers[i]++;
                i2c_gyro_files[i] = open_new_file(i2c_gyro_folders[i], i2c_gyro_chunk_numbers[i], timestamp);
                i2c_gyro_chunk_start_times[i] = timestamp;
            }
        }

        // SPI gyro (assume mask bit 4)
        if (packet.header.sensor_mask & (1u << (NUM_ACCELEROMETERS + NUM_I2C_GYROSCOPES))) {
            int i = 0; // only one SPI gyro
            if (spi_gyro_files[i] == NULL) {
                spi_gyro_chunk_start_times[i] = timestamp;
                spi_gyro_files[i] = open_new_file(spi_gyro_folders[i],
                                                  spi_gyro_chunk_numbers[i],
                                                  spi_gyro_chunk_start_times[i]);
            }
            if (spi_gyro_start_times[i] == 0) spi_gyro_start_times[i] = timestamp; // ADD

            fwrite(&timestamp, sizeof(double), 1, spi_gyro_files[i]);
            fwrite(&packet.gyro_spi.rate, sizeof(float), 1, spi_gyro_files[i]);

            spi_gyro_samples_received[i]++;
            
            if (spi_gyro_samples_received[i] % PRINT_INTERVAL == 0) {
                time_t current_time_t;
                time(&current_time_t);
                double elapsed_time = difftime(current_time_t, start_time_t);
                double average_rate = spi_gyro_samples_received[i] / (timestamp - spi_gyro_start_times[i]);
                
                char status_msg[512];
                snprintf(status_msg, sizeof(status_msg), "SPI Gyroscope %d - Samples: %ld | Elapsed time: %.0f s | Avg rate: %.2f Hz", 
                        i + 1, spi_gyro_samples_received[i], elapsed_time, average_rate);
                log_message(status_msg);
            }
            
            if (timestamp - spi_gyro_chunk_start_times[i] >= CHUNK_DURATION) {
                fclose(spi_gyro_files[i]);
                spi_gyro_chunk_numbers[i]++;
                spi_gyro_files[i] = open_new_file(spi_gyro_folders[i], spi_gyro_chunk_numbers[i], timestamp);
                spi_gyro_chunk_start_times[i] = timestamp;
            }
        }
    }
    
    // Close all accelerometer files
    for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
        if (accel_files[i] != NULL) {
            fclose(accel_files[i]);
        }
    }
    
    // Close all SPI gyroscope files
    for (int i = 0; i < NUM_SPI_GYROSCOPES; i++) {
        if (spi_gyro_files[i] != NULL) {
            fclose(spi_gyro_files[i]);
        }
    }
    
    // Close all I2C gyroscope files
    for (int i = 0; i < NUM_I2C_GYROSCOPES; i++) {
        if (i2c_gyro_files[i] != NULL) {
            fclose(i2c_gyro_files[i]);
        }
    }
    
    close(sock);
    
    // Generate comprehensive final statistics
    char final_msg[2048];
    snprintf(final_msg, sizeof(final_msg), 
             "Position sensor data collection complete.\n"
             "Accelerometer samples received: 1: %ld, 2: %ld, 3: %ld\n"
             "SPI Gyroscope samples received: 1: %ld\n"
             "I2C Gyroscope samples received: 1: %ld\n"
             "Total samples: %ld", 
             accel_samples_received[0], accel_samples_received[1], accel_samples_received[2],
             spi_gyro_samples_received[0], i2c_gyro_samples_received[0],
             accel_samples_received[0] + accel_samples_received[1] + accel_samples_received[2] + 
             spi_gyro_samples_received[0] + i2c_gyro_samples_received[0]);
    log_message(final_msg);
    
    fclose(log_file);
    return 0;
}
