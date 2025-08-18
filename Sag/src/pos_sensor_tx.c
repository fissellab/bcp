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
#include <pthread.h>
#include <stdbool.h>  // ADD
#include <netinet/tcp.h>

// Add multi-rate sampling constants
#define ACCEL_SAMPLE_HZ        1000
#define SPI_GYRO_SAMPLE_HZ     1000
#define I2C_GYRO_SAMPLE_HZ      250

// ADXL355 Register definitions
#define ADXL355_DEVID_AD     0x00
#define ADXL355_DEVID_MST    0x01
#define ADXL355_PARTID       0x02
#define ADXL355_REVID        0x03
#define ADXL355_STATUS       0x04
#define ADXL355_FIFO_ENTRIES 0x05
#define ADXL355_TEMP2        0x06
#define ADXL355_TEMP1        0x07
#define ADXL355_XDATA3       0x08
#define ADXL355_XDATA2       0x09
#define ADXL355_XDATA1       0x0A
#define ADXL355_YDATA3       0x0B
#define ADXL355_YDATA2       0x0C
#define ADXL355_YDATA1       0x0D
#define ADXL355_ZDATA3       0x0E
#define ADXL355_ZDATA2       0x0F
#define ADXL355_ZDATA1       0x10
#define ADXL355_FIFO_DATA    0x11
#define ADXL355_RESET        0x2F

#define ADXL355_OFFSET_X_H   0x1E
#define ADXL355_OFFSET_X_L   0x1F
#define ADXL355_OFFSET_Y_H   0x20
#define ADXL355_OFFSET_Y_L   0x21
#define ADXL355_OFFSET_Z_H   0x22
#define ADXL355_OFFSET_Z_L   0x23
#define ADXL355_ACT_EN       0x24
#define ADXL355_ACT_THRESH_H 0x25
#define ADXL355_ACT_THRESH_L 0x26
#define ADXL355_ACT_COUNT    0x27
#define ADXL355_FILTER       0x28
#define ADXL355_FIFO_SAMPLES 0x29
#define ADXL355_INT_MAP      0x2A
#define ADXL355_SYNC         0x2B
#define ADXL355_RANGE        0x2C
#define ADXL355_POWER_CTL    0x2D
#define ADXL355_SELF_TEST    0x2E

#define ADXL355_RANGE_2G     0x01
#define ADXL355_ODR_1000     0x02
#define ADXL355_POWER_CTL_STANDBY 0x01
#define ADXL355_RESET_CODE   0x52

// ADXRS453 Register definitions
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

// IAM20380HT I2C Register definitions
#define I2C_DEV "/dev/i2c-1"
#define IAM20380HT_ADDR 0x69
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

#define EXPECTED_WHOAMI 0xFA
#define GYRO_SCALE_2000DPS 16.4f

// Choose device-specific SPI speeds
#define ADXL355_SPI_SPEED   1000000  
#define ADXRS453_SPI_SPEED  1000000  

// (Optional) keep the old macro if used elsewhere, but do not rely on it for init
// #define SPI_CLOCK_SPEED 5000000  // 5 MHz

#define PORT 65432
#define BUFFER_SIZE 1024
#define WATCHDOG_TIMEOUT 5
#define MAX_RETRIES 5
#define RETRY_DELAY 1000000
#define NUM_ACCELEROMETERS 3
#define NUM_SPI_GYROSCOPES 1
#define NUM_I2C_GYROSCOPES 1
#define TOTAL_SENSORS (NUM_ACCELEROMETERS + NUM_SPI_GYROSCOPES + NUM_I2C_GYROSCOPES)

float accel_scale_factor = 0.0000382; // 0.0000039 g/LSB * 9.81 m/s²/g
volatile sig_atomic_t keep_running = 1;
FILE *log_file = NULL;

// Global I2C variables
int i2c_fd = -1;
float gyro_i2c_offset[3] = {0.0f, 0.0f, 0.0f};
float temp_offset = 0.0f;

struct spi_device {
    int fd;
    uint8_t mode;
    uint8_t bits_per_word;
    uint32_t speed;
    int sensor_type; // 0 = accelerometer, 1 = spi gyroscope
};

void signal_handler(int signum) {
    keep_running = 0;
}

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

// I2C Functions
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
        return;
    }
    usleep(10);
}

int16_t i2c_read_word(uint8_t reg) {
    uint8_t buf[2];
    buf[0] = i2c_read_byte(reg);
    buf[1] = i2c_read_byte(reg + 1);
    return (buf[0] << 8) | buf[1];
}

// Small helper to read multiple I2C bytes in one go
int i2c_read_block(uint8_t start_reg, uint8_t *buf, int len) {
    if (write(i2c_fd, &start_reg, 1) != 1) return -1;
    if (read(i2c_fd, buf, len) != len) return -1;
    return 0;
}

void read_i2c_gyroscope(float *gyro_x, float *gyro_y, float *gyro_z, float *temperature) {
    uint8_t b[6];
    if (i2c_read_block(REG_GYRO_XOUT_H, b, 6) == 0) {
        int16_t rx = (int16_t)((b[0] << 8) | b[1]);
        int16_t ry = (int16_t)((b[2] << 8) | b[3]);
        int16_t rz = (int16_t)((b[4] << 8) | b[5]);
        *gyro_x = (rx / GYRO_SCALE_2000DPS) - gyro_i2c_offset[0];
        *gyro_y = (ry / GYRO_SCALE_2000DPS) - gyro_i2c_offset[1];
        *gyro_z = (rz / GYRO_SCALE_2000DPS) - gyro_i2c_offset[2];
    } else {
        log_message("I2C gyro read failed");
        *gyro_x = *gyro_y = *gyro_z = 0.0f;
    }
    uint8_t t[2];
    if (i2c_read_block(REG_TEMP_OUT_H, t, 2) == 0) {
        int16_t tr = (int16_t)((t[0] << 8) | t[1]);
        *temperature = ((tr / 340.0f) + 36.53f) - temp_offset;
    } else {
        *temperature = 0.0f;
    }
}

// SPI Functions (same as before)
int spi_init(const char *device, struct spi_device *spi, int sensor_type) {
    spi->fd = open(device, O_RDWR);
    if (spi->fd < 0) {
        char msg[128];
        snprintf(msg, sizeof msg, "Failed to open SPI device %s: %s", device, strerror(errno));
        log_message(msg);
        return -1;
    }

    // Mode 0, MSB first, 8 bits
    spi->mode = SPI_MODE_0;
    spi->bits_per_word = 8;
    spi->speed = (sensor_type == 0) ? ADXL355_SPI_SPEED : ADXRS453_SPI_SPEED; // accel vs. SPI gyro
    spi->sensor_type = sensor_type;

    // Write mode (8-bit API)
    if (ioctl(spi->fd, SPI_IOC_WR_MODE, &spi->mode) < 0) {
        log_message("Failed to set SPI mode (WR_MODE)");
        return -1;
    }
    // Try 32-bit mode write to ensure extended flags are cleared (ignore if not supported)
    uint32_t mode32 = spi->mode;
    (void)ioctl(spi->fd, SPI_IOC_WR_MODE32, &mode32);

    // Force MSB-first
    uint8_t lsb_first = 0;
    if (ioctl(spi->fd, SPI_IOC_WR_LSB_FIRST, &lsb_first) < 0) {
        log_message("Failed to set LSB_FIRST=0");
        // continue; not fatal on some kernels
    }

    // Bits per word
    if (ioctl(spi->fd, SPI_IOC_WR_BITS_PER_WORD, &spi->bits_per_word) < 0) {
        log_message("Failed to set SPI bits per word");
        return -1;
    }

    // Max speed
    if (ioctl(spi->fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi->speed) < 0) {
        log_message("Failed to set SPI speed");
        return -1;
    }

    // Read back to confirm
    uint8_t confirm_mode = 0, confirm_bits = 0;
    uint32_t confirm_speed = 0;
    if (ioctl(spi->fd, SPI_IOC_RD_MODE, &confirm_mode) == 0 &&
        ioctl(spi->fd, SPI_IOC_RD_BITS_PER_WORD, &confirm_bits) == 0 &&
        ioctl(spi->fd, SPI_IOC_RD_MAX_SPEED_HZ, &confirm_speed) == 0) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "SPI %s configured: mode=%u, lsb_first=%u, bits=%u, speed=%u Hz",
                 device, confirm_mode, lsb_first, confirm_bits, confirm_speed);
        log_message(msg);
    } else {
        log_message("Warning: failed to read back SPI configuration");
    }

    return 0;
}

void spi_transfer(struct spi_device *spi, uint8_t *tx, uint8_t *rx, int len) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (uintptr_t)tx,   // was (unsigned long)
        .rx_buf = (uintptr_t)rx,
        .len = (uint32_t)len,
        .delay_usecs = 0,
        .speed_hz = spi->speed,
        .bits_per_word = spi->bits_per_word,
        .cs_change = 0,
    };

    if (ioctl(spi->fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        log_message("SPI transfer failed");
    }
}

// ADXL355 Functions
void adxl355_write_reg(struct spi_device *spi, uint8_t reg, uint8_t value) {
    uint8_t tx[2] = {reg << 1, value};
    uint8_t rx[2];
    spi_transfer(spi, tx, rx, 2);
    usleep(2000);
}

uint8_t adxl355_read_reg(struct spi_device *spi, uint8_t reg) {
    uint8_t tx[2] = {(reg << 1) | 0x01, 0};
    uint8_t rx[2];
    spi_transfer(spi, tx, rx, 2);
    return rx[1];
}

void adxl355_aggressive_init(struct spi_device *spi, int device_num) {
    char log_msg[256];
    
    adxl355_write_reg(spi, ADXL355_RESET, ADXL355_RESET_CODE);
    usleep(100000);
    
    adxl355_write_reg(spi, ADXL355_POWER_CTL, ADXL355_POWER_CTL_STANDBY);
    usleep(50000);
    
    adxl355_write_reg(spi, ADXL355_RANGE, ADXL355_RANGE_2G);
    adxl355_write_reg(spi, ADXL355_FILTER, ADXL355_ODR_1000);
    adxl355_write_reg(spi, ADXL355_FIFO_SAMPLES, 0x00);
    adxl355_write_reg(spi, ADXL355_SELF_TEST, 0x00);
    
    adxl355_write_reg(spi, ADXL355_POWER_CTL, 0x00);
    usleep(100000);
    
    uint8_t devid_ad = adxl355_read_reg(spi, ADXL355_DEVID_AD);
    uint8_t devid_mst = adxl355_read_reg(spi, ADXL355_DEVID_MST);
    uint8_t partid = adxl355_read_reg(spi, ADXL355_PARTID);
    uint8_t revid = adxl355_read_reg(spi, ADXL355_REVID);

    snprintf(log_msg, sizeof(log_msg), "Accelerometer %d: DEVID_AD=0x%02X, DEVID_MST=0x%02X, PARTID=0x%02X, REVID=0x%02X",
             device_num, devid_ad, devid_mst, partid, revid);
    log_message(log_msg);
}

void adxl355_read_xyz_individual(struct spi_device *spi, float *x, float *y, float *z) {
    uint8_t status;
    int data_ready_attempts = 0;
    do {
        status = adxl355_read_reg(spi, ADXL355_STATUS);
        if (!(status & 0x01)) {
            usleep(100);
            data_ready_attempts++;
        }
    } while (!(status & 0x01) && data_ready_attempts < 50);
    
    usleep(50);
    
    uint8_t x3 = adxl355_read_reg(spi, ADXL355_XDATA3);
    usleep(10);
    uint8_t x2 = adxl355_read_reg(spi, ADXL355_XDATA2);
    usleep(10);
    uint8_t x1 = adxl355_read_reg(spi, ADXL355_XDATA1);
    usleep(10);
    
    uint8_t y3 = adxl355_read_reg(spi, ADXL355_YDATA3);
    usleep(10);
    uint8_t y2 = adxl355_read_reg(spi, ADXL355_YDATA2);
    usleep(10);
    uint8_t y1 = adxl355_read_reg(spi, ADXL355_YDATA1);
    usleep(10);
    
    uint8_t z3 = adxl355_read_reg(spi, ADXL355_ZDATA3);
    usleep(10);
    uint8_t z2 = adxl355_read_reg(spi, ADXL355_ZDATA2);
    usleep(10);
    uint8_t z1 = adxl355_read_reg(spi, ADXL355_ZDATA1);
    
    int32_t x_raw = ((int32_t)x3 << 12) | ((int32_t)x2 << 4) | (x1 >> 4);
    int32_t y_raw = ((int32_t)y3 << 12) | ((int32_t)y2 << 4) | (y1 >> 4);
    int32_t z_raw = ((int32_t)z3 << 12) | ((int32_t)z2 << 4) | (z1 >> 4);
    
    if (x_raw & 0x80000) x_raw |= (int32_t)0xFFF00000;
    if (y_raw & 0x80000) y_raw |= (int32_t)0xFFF00000;
    if (z_raw & 0x80000) z_raw |= (int32_t)0xFFF00000;
    
    *x = x_raw * accel_scale_factor;
    *y = y_raw * accel_scale_factor;
    *z = z_raw * accel_scale_factor;
}

// Optional: faster, atomic 9-byte burst for ADXL355 XYZ
void adxl355_read_xyz_burst(struct spi_device *spi, float *x, float *y, float *z) {
    uint8_t tx[1 + 9] = {(ADXL355_XDATA3 << 1) | 0x01};
    uint8_t rx[1 + 9] = {0};
    spi_transfer(spi, tx, rx, sizeof(tx));
    int32_t xr = ((int32_t)rx[1] << 12) | ((int32_t)rx[2] << 4) | (rx[3] >> 4);
    int32_t yr = ((int32_t)rx[4] << 12) | ((int32_t)rx[5] << 4) | (rx[6] >> 4);
    int32_t zr = ((int32_t)rx[7] << 12) | ((int32_t)rx[8] << 4) | (rx[9] >> 4);
    if (xr & 0x80000) xr |= 0xFFF00000;
    if (yr & 0x80000) yr |= 0xFFF00000;
    if (zr & 0x80000) zr |= 0xFFF00000;
    *x = xr * accel_scale_factor;
    *y = yr * accel_scale_factor;
    *z = zr * accel_scale_factor;
}

// ADXRS453 SPI Gyroscope Functions
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
    
    spi_transfer(spi, dataBuffer, valueBuffer, 4);
    
    registerValue = ((unsigned short)valueBuffer[1] << 11) |
                    ((unsigned short)valueBuffer[2] << 3) |
                    (valueBuffer[3] >> 5);
    
    return registerValue;
}

float adxrs453_get_gyro_rate(struct spi_device *spi) {
    unsigned short registerValue = 0;
    float rate = 0.0;
    
    registerValue = adxrs453_get_register_value(spi, ADXRS453_REG_RATE);
    
    if (registerValue < 0x8000) {
        rate = ((float)registerValue / 80.0);
    } else {
        rate = (-1) * ((float)(0xFFFF - registerValue + 1) / 80.0);
    }
    
    rate += 0.631;
    
    return rate;
}

void adxrs453_init(struct spi_device *spi) {
    log_message("ADXRS453 SPI gyroscope initialized");
}

int setup_socket() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    char log_buffer[256];
    
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
    
    snprintf(log_buffer, sizeof(log_buffer), "Attempting to bind to address: 0.0.0.0, port: %d", PORT);
    log_message(log_buffer);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        snprintf(log_buffer, sizeof(log_buffer), "Bind failed. Error: %s", strerror(errno));
        log_message(log_buffer);
        return -1;
    }
    
    log_message("Bind successful");
    
    if (listen(server_fd, 3) < 0) {
        log_message("Listen failed");
        return -1;
    }
    
    log_message("Socket setup complete. Waiting for connection...");
    return server_fd;
}

// Helper time functions for precise periodic loops
static inline uint64_t now_ns() {
    const uint64_t NS_PER_SEC = 1000000000ull;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NS_PER_SEC + ts.tv_nsec;
}

static inline void sleep_until_ns(uint64_t t_next_ns) {
    struct timespec ts;
    ts.tv_sec = t_next_ns / 1000000000ull;
    ts.tv_nsec = t_next_ns % 1000000000ull;
    int ret;
    do {
        ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
    } while (ret == EINTR);
}

// Context shared by sampling threads during a client session
typedef struct {
    // devices[0..2] = accels (spidev0.0, 0.1, 1.0), devices[3] = SPI gyro (spidev1.1)
    struct spi_device spi_devices[NUM_ACCELEROMETERS + NUM_SPI_GYROSCOPES];
    int client_socket;
    volatile sig_atomic_t streaming;

    // Locks
    pthread_mutex_t send_lock;
    pthread_mutex_t spi0_lock;
    pthread_mutex_t spi1_lock;
} stream_ctx_t;

// Unified binary packet types aligned with position_sensors.c
#define PACKET_MAGIC 0xDEADBEEF

typedef struct { float x, y, z; } pos_accel_sample_t;
typedef struct { float rate; } pos_gyro_spi_sample_t;
typedef struct { float x, y, z, temp; } pos_gyro_i2c_sample_t;

typedef struct {
    uint32_t magic;            // 0xDEADBEEF
    uint16_t sequence;         // sequence number
    uint16_t sensor_mask;      // bitmask of active sensors
    uint32_t timestamp_sec;    // seconds
    uint32_t timestamp_nsec;   // nanoseconds
} pos_packet_header_t;

typedef struct {
    pos_packet_header_t header;
    pos_accel_sample_t  accels[3];
    pos_gyro_i2c_sample_t gyro_i2c;
    pos_gyro_spi_sample_t gyro_spi;
} pos_sensor_packet_t;

_Static_assert(sizeof(pos_packet_header_t) == 16, "header size mismatch");
_Static_assert(sizeof(pos_accel_sample_t) == 12, "accel size mismatch");
_Static_assert(sizeof(pos_gyro_i2c_sample_t) == 16, "i2c size mismatch");
_Static_assert(sizeof(pos_gyro_spi_sample_t) == 4,  "spi size mismatch");
_Static_assert(sizeof(pos_sensor_packet_t) == 72,   "packet size mismatch");

// globals used by threads
static uint16_t packet_sequence = 0;
static pthread_mutex_t packet_mutex = PTHREAD_MUTEX_INITIALIZER;
static pos_sensor_packet_t current_packet;
static bool packet_ready = false;

// forward declaration (needs stream_ctx_t defined above)
static void send_binary_packet(stream_ctx_t* ctx);

// Helper function to get the appropriate bus lock for a device index
static pthread_mutex_t* bus_lock_for_device_index(stream_ctx_t* ctx, int device_index) {
    // devices[0,1] are on SPI0, devices[2,3] are on SPI1
    if (device_index <= 1) {
        return &ctx->spi0_lock;
    } else {
        return &ctx->spi1_lock;
    }
}

// Accelerometer sampling thread @ 1 kHz - updates packet data
static void* accel_thread(void* arg) {
    stream_ctx_t* ctx = (stream_ctx_t*)arg;
    const uint64_t period = 1000000000ull / ACCEL_SAMPLE_HZ;
    uint64_t t_next = now_ns();

    while (keep_running && ctx->streaming) {
        t_next += period;

        // Read accelerometer data
        for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
            float ax, ay, az;
            
            pthread_mutex_t* bus_lock = bus_lock_for_device_index(ctx, i);
            pthread_mutex_lock(bus_lock);
            adxl355_read_xyz_burst(&ctx->spi_devices[i], &ax, &ay, &az);
            pthread_mutex_unlock(bus_lock);
            
            // Update packet with accelerometer data
            pthread_mutex_lock(&packet_mutex);
            current_packet.accels[i].x = ax;
            current_packet.accels[i].y = ay;
            current_packet.accels[i].z = az;
            pthread_mutex_unlock(&packet_mutex);
        }

        // Signal that a packet is ready to send if this is the primary thread
        pthread_mutex_lock(&packet_mutex);
        packet_ready = true;
        pthread_mutex_unlock(&packet_mutex);
        
        // Send the packet
        send_binary_packet(ctx);
        
        sleep_until_ns(t_next);
    }
    return NULL;
}

// SPI gyro sampling thread @ 1 kHz - updates packet data
static void* spi_gyro_thread(void* arg) {
    stream_ctx_t* ctx = (stream_ctx_t*)arg;
    const uint64_t period = 1000000000ull / SPI_GYRO_SAMPLE_HZ;
    uint64_t t_next = now_ns();

    while (keep_running && ctx->streaming) {
        t_next += period;

        // SPI gyro is at devices[NUM_ACCELEROMETERS]
        int idx = NUM_ACCELEROMETERS;
        float rate;

        pthread_mutex_t* bus_lock = bus_lock_for_device_index(ctx, idx);
        pthread_mutex_lock(bus_lock);
        rate = adxrs453_get_gyro_rate(&ctx->spi_devices[idx]);
        pthread_mutex_unlock(bus_lock);

        // Update packet with SPI gyro data
        pthread_mutex_lock(&packet_mutex);
        current_packet.gyro_spi.rate = rate;
        pthread_mutex_unlock(&packet_mutex);
        
        sleep_until_ns(t_next);
    }
    return NULL;
}

// I2C gyro sampling thread @ 250 Hz - updates packet data
static void* i2c_gyro_thread(void* arg) {
    stream_ctx_t* ctx = (stream_ctx_t*)arg;
    const uint64_t period = 1000000000ull / I2C_GYRO_SAMPLE_HZ;
    uint64_t t_next = now_ns();

    while (keep_running && ctx->streaming) {
        t_next += period;

        float gx, gy, gz, tempc;
        read_i2c_gyroscope(&gx, &gy, &gz, &tempc);

        // Update packet with I2C gyro data
        pthread_mutex_lock(&packet_mutex);
        current_packet.gyro_i2c.x = gx;
        current_packet.gyro_i2c.y = gy;
        current_packet.gyro_i2c.z = gz;
        current_packet.gyro_i2c.temp = tempc;
        pthread_mutex_unlock(&packet_mutex);
        
        sleep_until_ns(t_next);
    }
    return NULL;
}

// --- I2C init: adjust to 250 Hz ODR ---
// Assuming 8 kHz base when DLPF disabled (CONFIG=0x00): ODR = 8000/(1+SMPLRT_DIV)
// For 250 Hz target: SMPLRT_DIV = 31
int init_i2c_gyroscope() {
    // Open I2C device
    if ((i2c_fd = open(I2C_DEV, O_RDWR)) < 0) {
        log_message("Failed to open I2C device");
        return -1;
    }
    
    // Set I2C slave address
    if (ioctl(i2c_fd, I2C_SLAVE, IAM20380HT_ADDR) < 0) {
        log_message("Failed to set I2C slave address");
        close(i2c_fd);
        return -1;
    }
    
    // Check WHOAMI register
    uint8_t whoami = i2c_read_byte(REG_WHO_AM_I);
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "I2C Gyroscope WHOAMI: 0x%02X (Expected: 0x%02X)", whoami, EXPECTED_WHOAMI);
    log_message(log_msg);
    
    if (whoami != EXPECTED_WHOAMI) {
        log_message("Warning: I2C Gyroscope WHOAMI mismatch");
        if (whoami == 0) {
            log_message("No I2C response. Check connections and address.");
            return -1;
        }
    }
    
    // Reset device
    i2c_write_byte(REG_PWR_MGMT_1, 0x80);
    usleep(100000);  // 100ms delay
    
    // Wake up the device
    i2c_write_byte(REG_PWR_MGMT_1, 0x01);  // Use PLL with X gyro reference
    usleep(10000);  // 10ms delay
    
    // Set gyro full scale range to ±2000 dps
    i2c_write_byte(REG_GYRO_CONFIG, 0x18);
    
    // Disable DLPF for 8 kHz base rate
    i2c_write_byte(REG_CONFIG, 0x00);
    
    // Set sample rate divider for 250 Hz: 8000/(1+31) = 250 Hz
    i2c_write_byte(REG_SMPLRT_DIV, 0x1F);
    
    // Enable gyro and accel
    i2c_write_byte(REG_PWR_MGMT_2, 0x00);
    usleep(200000); // 200ms delay
    
    log_message("I2C Gyroscope initialized for ~250 Hz ODR");
    
    // Calculate offsets
    log_message("Calculating I2C gyroscope offsets...");
    float sum_gyro_x = 0, sum_gyro_y = 0, sum_gyro_z = 0;
    float sum_temp = 0;
    int samples = 200;
    
    usleep(1000000); // 1 second delay to settle
    
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
        
        usleep(5000); // 5ms delay between readings
    }
    
    gyro_i2c_offset[0] = sum_gyro_x / samples;
    gyro_i2c_offset[1] = sum_gyro_y / samples;
    gyro_i2c_offset[2] = sum_gyro_z / samples;
    
    float avg_temp = sum_temp / samples;
    temp_offset = avg_temp - 25.0f;
    
    snprintf(log_msg, sizeof(log_msg), "I2C Gyro offsets: X=%.3f, Y=%.3f, Z=%.3f deg/s", 
             gyro_i2c_offset[0], gyro_i2c_offset[1], gyro_i2c_offset[2]);
    log_message(log_msg);
    
    return 0;
}

// --- main(): spawn threads per client for multi-rate streaming ---
int main(void) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    struct spi_device spi_devices[NUM_ACCELEROMETERS + NUM_SPI_GYROSCOPES];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    create_log_file();
    log_message("Complete position sensor program started (3 accelerometers + 2 gyroscopes)");

    // Initialize SPI devices
    const char *spi_devices_paths[NUM_ACCELEROMETERS + NUM_SPI_GYROSCOPES] = {
        "/dev/spidev0.0",  // Accelerometer 1 (SPI0)
        "/dev/spidev0.1",  // Accelerometer 2 (SPI0)
        "/dev/spidev1.0",  // Accelerometer 3 (SPI1)
        "/dev/spidev1.1"   // SPI Gyroscope (SPI1)
    };

    for (int i = 0; i < NUM_ACCELEROMETERS + NUM_SPI_GYROSCOPES; i++) {
        int sensor_type = (i < NUM_ACCELEROMETERS) ? 0 : 1;
        if (spi_init(spi_devices_paths[i], &spi_devices[i], sensor_type) < 0) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Failed to initialize SPI device %d (%s)",
                     i, spi_devices_paths[i]);
            log_message(error_msg);
            return 1;
        }
        if (sensor_type == 0) {
            adxl355_aggressive_init(&spi_devices[i], i + 1); // ODR=1000 Hz
        } else {
            adxrs453_init(&spi_devices[i]);
        }
    }

    // Initialize I2C gyroscope (sets ~250 Hz ODR)
    if (init_i2c_gyroscope() < 0) {
        log_message("Failed to initialize I2C gyroscope");
        return 1;
    }

    server_fd = setup_socket();
    if (server_fd < 0) {
        log_message("Failed to set up socket");
        return 1;
    }

    while (keep_running) {
        log_message("Waiting for client connection...");
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Accept failed. Error: %s", strerror(errno));
            log_message(error_msg);
            sleep(1);
            continue;
        }

        // Enable TCP_NODELAY for immediate transmission
        int one = 1;
        setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        char connect_msg[256];
        snprintf(connect_msg, sizeof(connect_msg),
                 "Client connected from %s. Starting data streaming (3 accelerometers + 2 gyroscopes)...",
                 client_ip);
        log_message(connect_msg);

        // Prepare session context
        stream_ctx_t ctx = (stream_ctx_t){0};
        memcpy(ctx.spi_devices, spi_devices, sizeof(spi_devices));
        ctx.client_socket = client_socket;
        ctx.streaming = 1;
        pthread_mutex_init(&ctx.send_lock, NULL);
        pthread_mutex_init(&ctx.spi0_lock, NULL);
        pthread_mutex_init(&ctx.spi1_lock, NULL);

        // Initialize packet
        memset(&current_packet, 0, sizeof(pos_sensor_packet_t));

        // Launch sampling threads
        pthread_t th_accel, th_spi_gyro, th_i2c_gyro;
        if (pthread_create(&th_accel, NULL, accel_thread, &ctx) != 0) {
            log_message("Failed to create accelerometer thread");
            ctx.streaming = 0;
            close(client_socket);
            continue;
        }

        if (pthread_create(&th_spi_gyro, NULL, spi_gyro_thread, &ctx) != 0) {
            log_message("Failed to create SPI gyro thread");
            ctx.streaming = 0;
            pthread_join(th_accel, NULL);
            close(client_socket);
            continue;
        }

        if (pthread_create(&th_i2c_gyro, NULL, i2c_gyro_thread, &ctx) != 0) {
            log_message("Failed to create I2C gyro thread");
            ctx.streaming = 0;
            pthread_join(th_accel, NULL);
            pthread_join(th_spi_gyro, NULL);
            close(client_socket);
            continue;
        }

        // Wait until client disconnects or signal
        while (keep_running && ctx.streaming) {
            // Sleep a bit to reduce busy-waiting on the main thread
            usleep(100000);
        }

        // Stop threads
        ctx.streaming = 0;
        pthread_join(th_accel, NULL);
        pthread_join(th_spi_gyro, NULL);
        pthread_join(th_i2c_gyro, NULL);

        pthread_mutex_destroy(&ctx.send_lock);
        pthread_mutex_destroy(&ctx.spi0_lock);
        pthread_mutex_destroy(&ctx.spi1_lock);

        close(client_socket);
        log_message("Client disconnected");
    }

    log_message("Shutting down...");
    close(server_fd);
    for (int i = 0; i < NUM_ACCELEROMETERS + NUM_SPI_GYROSCOPES; i++) {
        close(spi_devices[i].fd);
    }
    if (i2c_fd >= 0) close(i2c_fd);
    if (log_file) fclose(log_file);
    return 0;
}

// Robust send: write exactly len bytes
static int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char*)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = send(fd, p + sent, len - sent, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) { usleep(1000); continue; }
            return -1;
        }
        if (r == 0) return -1;
        sent += (size_t)r;
    }
    return 0;
}

// New function to assemble and send a binary packet
static void send_binary_packet(stream_ctx_t* ctx) {
    pthread_mutex_lock(&packet_mutex);
    
    // Prepare packet header
    pos_sensor_packet_t packet;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    packet.header.magic = PACKET_MAGIC;
    packet.header.sequence = packet_sequence++;
    packet.header.timestamp_sec = ts.tv_sec;
    packet.header.timestamp_nsec = ts.tv_nsec;
    
    // Set sensor mask and copy data
    packet.header.sensor_mask = 0x1F;
    memcpy(&packet.accels, &current_packet.accels, sizeof(packet.accels));
    memcpy(&packet.gyro_i2c, &current_packet.gyro_i2c, sizeof(packet.gyro_i2c));
    memcpy(&packet.gyro_spi, &current_packet.gyro_spi, sizeof(packet.gyro_spi));
    
    pthread_mutex_unlock(&packet_mutex);
    
    // Send the binary packet
    pthread_mutex_lock(&ctx->send_lock);
    if (send_all(ctx->client_socket, &packet, sizeof(packet)) != 0) {
        log_message("Send failed (closing stream)");
        // Already holding the lock, just modify the flag
        ctx->streaming = 0;
    }
    pthread_mutex_unlock(&ctx->send_lock);
}
