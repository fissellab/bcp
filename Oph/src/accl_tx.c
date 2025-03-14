#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

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

#define SPI_CLOCK_SPEED 10000000  // 10 MHz
#define PORT 65432
#define BUFFER_SIZE 1024
#define WATCHDOG_TIMEOUT 5 // 5 seconds
#define MAX_RETRIES 5
#define RETRY_DELAY 1000000 // 1 second in microseconds

#define NUM_ACCELEROMETERS 3

float scale_factor = 0.0000039; // For 2G range
volatile sig_atomic_t keep_running = 1;
FILE *log_file = NULL;

struct spi_device {
    int fd;
    uint8_t mode;
    uint8_t bits_per_word;
    uint32_t speed;
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
    
    strftime(log_filename, sizeof(log_filename), "%Y-%m-%d_%H-%M-%S_accl_tx.log", t);
    snprintf(full_path, sizeof(full_path), "%s/%s", log_folder, log_filename);
    
    log_file = fopen(full_path, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Error creating log file '%s': %s\n", full_path, strerror(errno));
    } else {
        log_message("Log file created successfully");
    }
}

int spi_init(const char *device, struct spi_device *spi) {
    spi->fd = open(device, O_RDWR);
    if (spi->fd < 0) {
        log_message("Failed to open SPI device");
        return -1;
    }

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

void adxl355_init(struct spi_device *spi) {
    adxl355_write_reg(spi, ADXL355_RANGE, ADXL355_RANGE_2G);
    adxl355_write_reg(spi, ADXL355_FILTER, ADXL355_ODR_1000);
    adxl355_write_reg(spi, ADXL355_POWER_CTL, 0x00); // Measurement mode

    uint8_t devid_ad = adxl355_read_reg(spi, ADXL355_DEVID_AD);
    uint8_t devid_mst = adxl355_read_reg(spi, ADXL355_DEVID_MST);
    uint8_t partid = adxl355_read_reg(spi, ADXL355_PARTID);
    uint8_t revid = adxl355_read_reg(spi, ADXL355_REVID);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Accelerometer on device %d: DEVID_AD=0x%02X, DEVID_MST=0x%02X, PARTID=0x%02X, REVID=0x%02X",
             spi->fd, devid_ad, devid_mst, partid, revid);
    log_message(log_msg);

    uint8_t range = adxl355_read_reg(spi, ADXL355_RANGE);
    uint8_t filter = adxl355_read_reg(spi, ADXL355_FILTER);
    uint8_t power_ctl = adxl355_read_reg(spi, ADXL355_POWER_CTL);

    snprintf(log_msg, sizeof(log_msg), "Accelerometer on device %d settings: RANGE=0x%02X, FILTER=0x%02X, POWER_CTL=0x%02X",
             spi->fd, range, filter, power_ctl);
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
    *x = x_raw * scale_factor * 9.81;
    *y = y_raw * scale_factor * 9.81;
    *z = z_raw * scale_factor * 9.81;
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

int main(void) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    struct timespec start, end, sleep_time;
    float x[NUM_ACCELEROMETERS], y[NUM_ACCELEROMETERS], z[NUM_ACCELEROMETERS];
    long loop_count = 0;
    struct timeval last_activity;
    struct spi_device spi_devices[NUM_ACCELEROMETERS];
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    create_log_file();
    log_message("Program started");

    // Initialize SPI devices
    const char *spi_devices_paths[NUM_ACCELEROMETERS] = {"/dev/spidev0.0", "/dev/spidev0.1", "/dev/spidev1.0"};
    for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
        spi_devices[i].mode = SPI_MODE_0;
        spi_devices[i].bits_per_word = 8;
        spi_devices[i].speed = SPI_CLOCK_SPEED;
        if (spi_init(spi_devices_paths[i], &spi_devices[i]) < 0) {
            log_message("Failed to initialize SPI device");
            return 1;
        }
        adxl355_init(&spi_devices[i]);
    }
    
    server_fd = setup_socket();
    if (server_fd < 0) {
        log_message("Failed to set up socket");
        return 1;
    }
    
    while (keep_running) {
        log_message("Waiting for client connection...");
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Accept failed. Error: %s", strerror(errno));
            log_message(error_msg);
            sleep(1);  // Add a small delay to prevent rapid logging
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        char connect_msg[256];
        snprintf(connect_msg, sizeof(connect_msg), "Client connected from %s. Starting data streaming at 1000 Hz...", client_ip);
        log_message(connect_msg);
        
        gettimeofday(&last_activity, NULL);
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        while (keep_running) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            
            for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
                adxl355_read_xyz(&spi_devices[i], &x[i], &y[i], &z[i]);
                
                // Create separate packet for each accelerometer
                snprintf(buffer, BUFFER_SIZE, "%d,%ld.%09ld,%.6f,%.6f,%.6f\n", 
                         i+1, ts.tv_sec, ts.tv_nsec, x[i], y[i], z[i]);
                
                int retry_count = 0;
                while (retry_count < MAX_RETRIES) {
                    if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
                        char send_error[256];
                        snprintf(send_error, sizeof(send_error), "Send failed. Error: %s. Retrying...", strerror(errno));
                        log_message(send_error);
                        usleep(RETRY_DELAY);
                        retry_count++;
                    } else {
                        break;
                    }
                }
                
                if (retry_count == MAX_RETRIES) {
                    log_message("Max retries reached. Closing connection.");
                    goto connection_closed;
                }
            }
            
            loop_count++;
            
            clock_gettime(CLOCK_MONOTONIC, &end);
            long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
            long target_ns = (loop_count * 1000000);  // 1000000 ns = 1 ms (1000 Hz)
            long sleep_ns = target_ns - elapsed_ns;
            
            if (sleep_ns > 0) {
                sleep_time.tv_sec = sleep_ns / 1000000000;
                sleep_time.tv_nsec = sleep_ns % 1000000000;
                nanosleep(&sleep_time, NULL);
            }
            
            // Check watchdog
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
        
connection_closed:
        close(client_socket);
        log_message("Client disconnected");
    }
    
    log_message("Shutting down...");
    close(server_fd);
    for (int i = 0; i < NUM_ACCELEROMETERS; i++) {
        close(spi_devices[i].fd);
    }
    if (log_file) {
        fclose(log_file);
    }
    return 0;
}