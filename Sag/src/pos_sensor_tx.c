#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <math.h>

// ===== ADXL355 DEFINITIONS =====
#define ADXL355_DEVID_AD     0x00
#define ADXL355_DEVID_MST    0x01
#define ADXL355_PARTID       0x02
#define ADXL355_REVID        0x03
#define ADXL355_STATUS       0x04
#define ADXL355_XDATA3       0x08
#define ADXL355_RANGE        0x2C
#define ADXL355_POWER_CTL    0x2D
#define ADXL355_FILTER       0x28

#define ADXL355_RANGE_2G     0x01
#define ADXL355_ODR_1000     0x0002

// ===== ADXRS453 DEFINITIONS =====
#define ADXRS453_READ           (1 << 7)
#define ADXRS453_WRITE          (1 << 6)
#define ADXRS453_SENSOR_DATA    (1 << 5)
#define ADXRS453_REG_RATE       0x00
#define ADXRS453_REG_TEM        0x02
#define ADXRS453_REG_LOCST      0x04
#define ADXRS453_REG_HICST      0x06
#define ADXRS453_REG_QUAD       0x08
#define ADXRS453_REG_FAULT      0x0A
#define ADXRS453_REG_PID        0x0C
#define ADXRS453_REG_SN_HIGH    0x0E
#define ADXRS453_REG_SN_LOW     0x10

// ===== IAM-20380HT DEFINITIONS =====
#define REG_SELF_TEST_X_GYRO   0x00
#define REG_SELF_TEST_Y_GYRO   0x01
#define REG_SELF_TEST_Z_GYRO   0x02
#define REG_SMPLRT_DIV         0x19
#define REG_CONFIG             0x1A
#define REG_GYRO_CONFIG        0x1B
#define REG_ACCEL_CONFIG       0x1C
#define REG_FIFO_EN            0x23
#define REG_INT_PIN_CFG        0x37
#define REG_INT_ENABLE         0x38
#define REG_TEMP_OUT_H         0x41
#define REG_TEMP_OUT_L         0x42
#define REG_GYRO_XOUT_H        0x43
#define REG_GYRO_XOUT_L        0x44
#define REG_GYRO_YOUT_H        0x45
#define REG_GYRO_YOUT_L        0x46
#define REG_GYRO_ZOUT_H        0x47
#define REG_GYRO_ZOUT_L        0x48
#define REG_SIGNAL_PATH_RESET  0x68
#define REG_USER_CTRL          0x6A
#define REG_PWR_MGMT_1         0x6B
#define REG_PWR_MGMT_2         0x6C
#define REG_WHO_AM_I           0x75

#define EXPECTED_WHOAMI        0xFA
#define IAM20380HT_ADDR        0x69
#define I2C_DEV               "/dev/i2c-1"

// ===== SYSTEM DEFINITIONS =====
#define SPI_CLOCK_SPEED_ADXL   10000000  // 10 MHz for ADXL355
#define SPI_CLOCK_SPEED_ADXRS  1000000   // 1 MHz for ADXRS453
#define PORT                   65432
#define BUFFER_SIZE            1024
#define WATCHDOG_TIMEOUT       5
#define MAX_RETRIES            5
#define RETRY_DELAY            1000000
#define SAMPLING_RATE          1000      // 1000 Hz main loop
#define GYRO_SPI_DIVIDER       4         // 250 Hz for ADXRS453

// Gyroscope sensitivity scales
#define GYRO_SCALE_250DPS      131.0f
#define GYRO_SCALE_500DPS      65.5f
#define GYRO_SCALE_1000DPS     32.8f
#define GYRO_SCALE_2000DPS     16.4f

// ===== DATA STRUCTURES =====
struct spi_device {
    int fd;
    uint8_t mode;
    uint8_t bits_per_word;
    uint32_t speed;
    const char* device_path;
};

// Binary packet format
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

// ===== GLOBAL VARIABLES =====
volatile sig_atomic_t keep_running = 1;
FILE *log_file = NULL;
struct spi_device spi_devices[4];  // 3x ADXL355 + 1x ADXRS453
int i2c_fd;
float adxl355_scale_factor = 0.0000039; // For 2G range
float gyro_i2c_offsets[3] = {0.0f, 0.0f, 0.0f};
float gyro_i2c_temp_offset = 0.0f;
uint16_t packet_sequence = 0;

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
    
    strftime(log_filename, sizeof(log_filename), "%Y-%m-%d_%H-%M-%S_pos_sensor_tx.log", t);
    snprintf(full_path, sizeof(full_path), "%s/%s", log_folder, log_filename);
    
    log_file = fopen(full_path, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Error creating log file '%s': %s\n", full_path, strerror(errno));
    } else {
        log_message("Log file created successfully");
    }
}

// ===== SPI FUNCTIONS =====
int spi_init(const char *device, struct spi_device *spi, uint32_t speed) {
    spi->fd = open(device, O_RDWR);
    if (spi->fd < 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to open SPI device %s", device);
        log_message(msg);
        return -1;
    }

    spi->mode = SPI_MODE_0;
    spi->bits_per_word = 8;
    spi->speed = speed;
    spi->device_path = device;

    if (ioctl(spi->fd, SPI_IOC_WR_MODE, &spi->mode) < 0) {
        log_message("Failed to set SPI mode");
        return -1;
    }

    if (ioctl(spi->fd, SPI_IOC_WR_BITS_PER_WORD, &spi->bits_per_word) < 0) {
        log_message("Failed to set SPI bits per word");
        return -1;
    }

    if (ioctl(spi->fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi->speed) < 0) {
        log_message("Failed to set SPI speed");
        return -1;
    }

    return 0;
}

void spi_transfer(struct spi_device *spi, uint8_t *tx, uint8_t *rx, int len) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = len,
        .delay_usecs = 0,
        .speed_hz = spi->speed,
        .bits_per_word = spi->bits_per_word,
    };

    if (ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        log_message("SPI transfer failed");
    }
}

// ===== I2C FUNCTIONS =====
uint8_t i2c_read_byte(uint8_t reg) {
    uint8_t value;
    if (write(i2c_fd, &reg, 1) != 1) {
        log_message("Failed to write to I2C bus");
        return 0;
    }
    if (read(i2c_fd, &value, 1) != 1) {
        log_message("Failed to read from I2C bus");
        return 0;
    }
    return value;
}

void i2c_write_byte(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    if (write(i2c_fd, buf, 2) != 2) {
        log_message("Failed to write to I2C bus");
    }
    usleep(10);
}

int16_t i2c_read_word(uint8_t reg) {
    uint8_t buf[2];
    buf[0] = i2c_read_byte(reg);
    buf[1] = i2c_read_byte(reg + 1);
    return (buf[0] << 8) | buf[1];
}

// ===== ADXL355 FUNCTIONS =====
void adxl355_write_reg(struct spi_device *spi, uint8_t reg, uint8_t value) {
    uint8_t tx[2] = {reg << 1, value};
    uint8_t rx[2];
    spi_transfer(spi, tx, rx, 2);
}

uint8_t adxl355_read_reg(struct spi_device *spi, uint8_t reg) {
    uint8_t tx[2] = {(reg << 1) | 0x01, 0};
    uint8_t rx[2];
    spi_transfer(spi, tx, rx, 2);
    return rx[1];
}

void adxl355_init(struct spi_device *spi, int sensor_id) {
    adxl355_write_reg(spi, ADXL355_RANGE, ADXL355_RANGE_2G);
    adxl355_write_reg(spi, ADXL355_FILTER, ADXL355_ODR_1000);
    adxl355_write_reg(spi, ADXL355_POWER_CTL, 0x00); // Measurement mode

    uint8_t devid_ad = adxl355_read_reg(spi, ADXL355_DEVID_AD);
    uint8_t devid_mst = adxl355_read_reg(spi, ADXL355_DEVID_MST);
    uint8_t partid = adxl355_read_reg(spi, ADXL355_PARTID);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "ADXL355 #%d (%s): DEVID_AD=0x%02X, DEVID_MST=0x%02X, PARTID=0x%02X",
             sensor_id, spi->device_path, devid_ad, devid_mst, partid);
    log_message(log_msg);
}

void adxl355_read_xyz(struct spi_device *spi, float *x, float *y, float *z) {
    uint8_t tx[10] = {(ADXL355_XDATA3 << 1) | 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t rx[10];
    spi_transfer(spi, tx, rx, 10);
    
    int32_t x_raw = ((int32_t)rx[1] << 12) | ((int32_t)rx[2] << 4) | (rx[3] >> 4);
    int32_t y_raw = ((int32_t)rx[4] << 12) | ((int32_t)rx[5] << 4) | (rx[6] >> 4);
    int32_t z_raw = ((int32_t)rx[7] << 12) | ((int32_t)rx[8] << 4) | (rx[9] >> 4);
    
    // Convert to signed values
    if (x_raw & 0x80000) x_raw |= ~0xFFFFF;
    if (y_raw & 0x80000) y_raw |= ~0xFFFFF;
    if (z_raw & 0x80000) z_raw |= ~0xFFFFF;
    
    // Convert to m/s^2
    *x = x_raw * adxl355_scale_factor * 9.81;
    *y = y_raw * adxl355_scale_factor * 9.81;
    *z = z_raw * adxl355_scale_factor * 9.81;
}

// ===== ADXRS453 FUNCTIONS =====
unsigned short adxrs453_get_register_value(struct spi_device *spi, unsigned char registerAddress) {
    unsigned char dataBuffer[4] = {0};
    unsigned char valueBuffer[4] = {0};
    unsigned long command = 0;
    unsigned char sum = 0;
    unsigned short registerValue = 0;
    
    dataBuffer[0] = ADXRS453_READ | (registerAddress >> 7);
    dataBuffer[1] = (registerAddress << 1);
    
    command = ((unsigned long)dataBuffer[0] << 24) |
              ((unsigned long)dataBuffer[1] << 16) |
              ((unsigned short)dataBuffer[2] << 8) |
              dataBuffer[3];
    
    for (int bitNo = 31; bitNo > 0; bitNo--) {
        sum += ((command >> bitNo) & 0x1);
    }
    
    if (!(sum % 2)) {
        dataBuffer[3] |= 1;
    }
    
    if (spi_transfer(spi, dataBuffer, valueBuffer, 4) < 0) {
        return 0;
    }
    
    registerValue = ((unsigned short)valueBuffer[1] << 11) |
                    ((unsigned short)valueBuffer[2] << 3) |
                    (valueBuffer[3] >> 5);
    
    return registerValue;
}

float adxrs453_get_gyro_rate(struct spi_device *spi) {
    unsigned short registerValue = adxrs453_get_register_value(spi, ADXRS453_REG_RATE);
    float rate = 0.0;
    
    if (registerValue < 0x8000) {
        rate = ((float)registerValue / 80.0);
    } else {
        rate = (-1) * ((float)(0xFFFF - registerValue + 1) / 80.0);
    }
    
    rate += 0.631; // Offset correction from original code
    return rate;
}

void adxrs453_init(struct spi_device *spi) {
    log_message("ADXRS453 initialized");
}

// ===== IAM-20380HT FUNCTIONS =====
void iam20380_init() {
    // Reset device
    i2c_write_byte(REG_PWR_MGMT_1, 0x80);
    usleep(100000);
    
    // Wake up the device
    i2c_write_byte(REG_PWR_MGMT_1, 0x01);
    usleep(10000);
    
    // Set gyro full scale range to ±2000 dps
    i2c_write_byte(REG_GYRO_CONFIG, 0x18);
    
    // No DLPF for maximum bandwidth
    i2c_write_byte(REG_CONFIG, 0x00);
    
    // Set sample rate divider to 0 for maximum rate
    i2c_write_byte(REG_SMPLRT_DIV, 0x00);
    
    log_message("IAM-20380HT initialized");
}

void iam20380_calculate_offsets(int samples) {
    float sum_gyro_x = 0, sum_gyro_y = 0, sum_gyro_z = 0;
    float sum_temp = 0;
    
    log_message("Calculating IAM-20380HT offsets - keep sensor still...");
    usleep(1000000);
    
    for (int i = 0; i < samples; i++) {
        int16_t gyro_x = i2c_read_word(REG_GYRO_XOUT_H);
        int16_t gyro_y = i2c_read_word(REG_GYRO_YOUT_H);
        int16_t gyro_z = i2c_read_word(REG_GYRO_ZOUT_H);
        int16_t temp_raw = i2c_read_word(REG_TEMP_OUT_H);
        
        sum_gyro_x += gyro_x / GYRO_SCALE_2000DPS;
        sum_gyro_y += gyro_y / GYRO_SCALE_2000DPS;
        sum_gyro_z += gyro_z / GYRO_SCALE_2000DPS;
        
        float temp_c = (temp_raw / 340.0f) + 36.53f;
        sum_temp += temp_c;
        
        usleep(5000);
    }
    
    gyro_i2c_offsets[0] = sum_gyro_x / samples;
    gyro_i2c_offsets[1] = sum_gyro_y / samples;
    gyro_i2c_offsets[2] = sum_gyro_z / samples;
    
    float avg_temp = sum_temp / samples;
    gyro_i2c_temp_offset = avg_temp - 25.0f;
    
    char msg[256];
    snprintf(msg, sizeof(msg), "IAM-20380HT offsets: X=%.2f, Y=%.2f, Z=%.2f, Temp=%.2f",
             gyro_i2c_offsets[0], gyro_i2c_offsets[1], gyro_i2c_offsets[2], gyro_i2c_temp_offset);
    log_message(msg);
}

void iam20380_read_xyz(float *x, float *y, float *z, float *temperature) {
    int16_t gyro_x = i2c_read_word(REG_GYRO_XOUT_H);
    int16_t gyro_y = i2c_read_word(REG_GYRO_YOUT_H);
    int16_t gyro_z = i2c_read_word(REG_GYRO_ZOUT_H);
    int16_t temp_raw = i2c_read_word(REG_TEMP_OUT_H);
    
    *x = gyro_x / GYRO_SCALE_2000DPS - gyro_i2c_offsets[0];
    *y = gyro_y / GYRO_SCALE_2000DPS - gyro_i2c_offsets[1];
    *z = gyro_z / GYRO_SCALE_2000DPS - gyro_i2c_offsets[2];
    
    *temperature = ((temp_raw / 340.0f) + 36.53f) - gyro_i2c_temp_offset;
}

// ===== NETWORK FUNCTIONS =====
int setup_tcp_server() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        log_message("Socket creation failed");
        return -1;
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        log_message("Setsockopt failed");
        return -1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        log_message("Bind failed");
        return -1;
    }
    
    if (listen(server_fd, 3) < 0) {
        log_message("Listen failed");
        return -1;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "TCP server listening on port %d", PORT);
    log_message(msg);
    return server_fd;
}

// ===== SENSOR INITIALIZATION =====
int init_all_sensors() {
    char msg[256];
    
    // Initialize SPI devices
    const char *spi_paths[4] = {
        "/dev/spidev0.0",  // ADXL355 #1
        "/dev/spidev0.1",  // ADXL355 #2
        "/dev/spidev1.0",  // ADXL355 #3
        "/dev/spidev1.1"   // ADXRS453
    };
    
    // Initialize ADXL355 sensors
    for (int i = 0; i < 3; i++) {
        if (spi_init(spi_paths[i], &spi_devices[i], SPI_CLOCK_SPEED_ADXL) < 0) {
            snprintf(msg, sizeof(msg), "Failed to initialize ADXL355 #%d", i+1);
            log_message(msg);
            return -1;
        }
        adxl355_init(&spi_devices[i], i+1);
    }
    
    // Initialize ADXRS453 gyro
    if (spi_init(spi_paths[3], &spi_devices[3], SPI_CLOCK_SPEED_ADXRS) < 0) {
        log_message("Failed to initialize ADXRS453");
        return -1;
    }
    adxrs453_init(&spi_devices[3]);
    
    // Initialize I2C for IAM-20380HT
    if ((i2c_fd = open(I2C_DEV, O_RDWR)) < 0) {
        log_message("Failed to open I2C device");
        return -1;
    }
    
    if (ioctl(i2c_fd, I2C_SLAVE, IAM20380HT_ADDR) < 0) {
        log_message("Failed to set I2C slave address");
        return -1;
    }
    
    uint8_t whoami = i2c_read_byte(REG_WHO_AM_I);
    snprintf(msg, sizeof(msg), "IAM-20380HT WHOAMI: 0x%02X (Expected: 0x%02X)", whoami, EXPECTED_WHOAMI);
    log_message(msg);
    
    if (whoami != EXPECTED_WHOAMI) {
        log_message("Warning: Unexpected WHOAMI value for IAM-20380HT");
    }
    
    iam20380_init();
    iam20380_calculate_offsets(200);
    
    log_message("All sensors initialized successfully");
    return 0;
}

// ===== TIMING FUNCTIONS =====
void maintain_1000hz_timing(struct timespec *loop_start, long loop_count) {
    struct timespec now, sleep_time;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    long elapsed_ns = (now.tv_sec - loop_start->tv_sec) * 1000000000L + 
                      (now.tv_nsec - loop_start->tv_nsec);
    long target_ns = (loop_count * 1000000L);  // 1ms = 1000000ns
    long sleep_ns = target_ns - elapsed_ns;
    
    if (sleep_ns > 0) {
        sleep_time.tv_sec = sleep_ns / 1000000000L;
        sleep_time.tv_nsec = sleep_ns % 1000000000L;
        nanosleep(&sleep_time, NULL);
    } else if (sleep_ns < -10000000L) {  // Warn if >10ms behind
        char timing_msg[256];
        snprintf(timing_msg, sizeof(timing_msg), "Timing falling behind by %.2f ms", -sleep_ns / 1000000.0);
        log_message(timing_msg);
    }
}

// ===== PACKET FUNCTIONS =====
void create_and_send_packet(int client_socket, int gyro_counter) {
    static sensor_packet_t packet;
    struct timespec ts;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Fill packet header
    packet.header.magic = 0xDEADBEEF;
    packet.header.timestamp_sec = ts.tv_sec;
    packet.header.timestamp_nsec = ts.tv_nsec;
    packet.header.sequence = packet_sequence++;
    packet.header.sensor_mask = 0x0F;  // Accel 1-3 + I2C gyro always present
    packet.header.reserved = 0;
    
    // Read all ADXL355 accelerometers (1000Hz)
    for (int i = 0; i < 3; i++) {
        packet.accels[i].sensor_id = i + 1;
        adxl355_read_xyz(&spi_devices[i], 
                        &packet.accels[i].x, 
                        &packet.accels[i].y, 
                        &packet.accels[i].z);
    }
    
    // Read IAM-20380HT gyro (1000Hz)
    iam20380_read_xyz(&packet.gyro_i2c.x, 
                     &packet.gyro_i2c.y, 
                     &packet.gyro_i2c.z, 
                     &packet.gyro_i2c.temperature);
    
    // Read ADXRS453 gyro (250Hz - every 4th cycle)
    if (gyro_counter == 0) {
        packet.gyro_spi.rate = adxrs453_get_gyro_rate(&spi_devices[3]);
        packet.header.sensor_mask |= 0x10;  // Set SPI gyro bit
    }
    
    // Send packet
    int packet_size = sizeof(packet_header_t) + sizeof(accel_sample_t) * 3 + sizeof(gyro_i2c_sample_t);
    if (packet.header.sensor_mask & 0x10) {
        packet_size += sizeof(gyro_spi_sample_t);
    }
    
    int retry_count = 0;
    while (retry_count < MAX_RETRIES) {
        if (send(client_socket, &packet, packet_size, 0) < 0) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Send failed: %s. Retrying...", strerror(errno));
            log_message(error_msg);
            usleep(RETRY_DELAY);
            retry_count++;
        } else {
            break;
        }
    }
    
    if (retry_count == MAX_RETRIES) {
        log_message("Max retries reached for packet send");
    }
}

// ===== MAIN FUNCTION =====
int main(void) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    struct timespec loop_start;
    long loop_count = 0;
    int gyro_counter = 0;
    struct timeval last_activity;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    create_log_file();
    log_message("Position sensor transmitter started");
    
    // Initialize all sensors
    if (init_all_sensors() < 0) {
        log_message("Failed to initialize sensors");
        return 1;
    }
    
    // Setup TCP server
    server_fd = setup_tcp_server();
    if (server_fd < 0) {
        log_message("Failed to set up TCP server");
        return 1;
    }
    
    while (keep_running) {
        log_message("Waiting for client connection...");
        
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Accept failed: %s", strerror(errno));
            log_message(error_msg);
            sleep(1);
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        char connect_msg[256];
        snprintf(connect_msg, sizeof(connect_msg), "Client connected from %s. Starting data streaming at %d Hz...",
                client_ip, SAMPLING_RATE);
        log_message(connect_msg);
        
        gettimeofday(&last_activity, NULL);
        clock_gettime(CLOCK_MONOTONIC, &loop_start);
        loop_count = 0;
        gyro_counter = 0;
        
        // Main sampling loop
        while (keep_running) {
            // Create and send sensor packet
            create_and_send_packet(client_socket, gyro_counter);
            
            loop_count++;
            gyro_counter = (gyro_counter + 1) % GYRO_SPI_DIVIDER;
            
            // Maintain precise 1000Hz timing
            maintain_1000hz_timing(&loop_start, loop_count);
            
            // Watchdog check
            struct timeval now;
            gettimeofday(&now, NULL);
            double elapsed = (now.tv_sec - last_activity.tv_sec) +
                           (now.tv_usec - last_activity.tv_usec) / 1000000.0;
            if (elapsed > WATCHDOG_TIMEOUT) {
                log_message("Watchdog timeout. Resetting connection.");
                break;
            }
            
            gettimeofday(&last_activity, NULL);
        }
        
        close(client_socket);
        log_message("Client disconnected");
    }
    
    // Cleanup
    log_message("Shutting down...");
    close(server_fd);
    
    for (int i = 0; i < 4; i++) {
        if (spi_devices[i].fd >= 0) {
            close(spi_devices[i].fd);
        }
    }
    
    if (i2c_fd >= 0) {
        close(i2c_fd);
    }
    
    if (log_file) {
        fclose(log_file);
    }
    
    return 0;
}