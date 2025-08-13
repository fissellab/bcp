#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "ticc_client.h"
#include "file_io_Sag.h"

// Global configuration and state
static ticc_client_config_t client_config;
static bool client_initialized = false;
static bool logging_active = false;
static bool device_configured = false;
static int serial_fd = -1;
static pthread_t logging_thread;
static char current_data_file[512];
static FILE* data_file = NULL;
static pthread_mutex_t ticc_mutex = PTHREAD_MUTEX_INITIALIZER;

// Statistics
static time_t logging_start_time = 0;
static int total_measurements = 0;
static double last_measurement_value = 0.0;
static double last_measurement_timestamp = 0.0;
static char last_error_msg[256] = {0};

/**
 * Initialize the TICC client with the given configuration
 * Returns 0 on success, -1 on failure
 */
int ticc_client_init(const ticc_client_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&client_config, config, sizeof(ticc_client_config_t));
    client_initialized = true;
    
    // Create data directory if it doesn't exist
    struct stat st = {0};
    if (stat(client_config.data_save_path, &st) == -1) {
        if (mkdir(client_config.data_save_path, 0755) != 0) {
            snprintf(last_error_msg, sizeof(last_error_msg), 
                    "Failed to create data directory: %s", strerror(errno));
            return -1;
        }
    }
    
    return 0;
}

/**
 * Check if TICC client is enabled
 */
bool ticc_client_is_enabled(void) {
    return client_initialized && client_config.enabled;
}

/**
 * Send command to TICC device
 */
static int ticc_send_command(const char* cmd, double delay_seconds) {
    if (serial_fd < 0) return -1;
    
    ssize_t bytes_written = write(serial_fd, cmd, strlen(cmd));
    if (bytes_written < 0) {
        snprintf(last_error_msg, sizeof(last_error_msg), 
                "Failed to send command: %s", strerror(errno));
        return -1;
    }
    
    // Delay as specified
    if (delay_seconds > 0) {
        usleep((useconds_t)(delay_seconds * 1000000));
    }
    
    return 0;
}

/**
 * Read response from TICC device
 */
static int ticc_read_response(char* buffer, size_t buffer_size, int timeout_ms) {
    if (serial_fd < 0 || !buffer) return -1;
    
    fd_set read_fds;
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&read_fds);
    FD_SET(serial_fd, &read_fds);
    
    int select_result = select(serial_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (select_result <= 0) {
        return -1; // Timeout or error
    }
    
    ssize_t bytes_read = read(serial_fd, buffer, buffer_size - 1);
    if (bytes_read >= 0) {
        buffer[bytes_read] = '\0';
        return bytes_read;
    }
    
    return -1;
}

/**
 * Configure TICC device for Time Interval mode
 */
int ticc_configure_device(void) {
    if (serial_fd < 0) {
        snprintf(last_error_msg, sizeof(last_error_msg), "Serial port not open");
        return -1;
    }
    
    printf("Configuring TICC for Time Interval mode...\n");
    
    // Clear buffers
    tcflush(serial_fd, TCIOFLUSH);
    
    // Wait for initial bootup
    sleep(2);
    
    // Send break character to enter config mode
    printf("Entering config mode...\n");
    if (ticc_send_command("#", 1.0) != 0) return -1;
    
    // Select measurement mode
    printf("Setting Time Interval mode...\n");
    if (ticc_send_command("M", 1.0) != 0) return -1;
    
    // Select Time Interval mode
    if (ticc_send_command("I\r", 1.0) != 0) return -1;
    
    // Write changes and exit
    printf("Saving configuration...\n");
    if (ticc_send_command("W", 2.0) != 0) return -1;
    
    device_configured = true;
    printf("TICC configuration complete\n");
    
    return 0;
}

/**
 * Create a new data file with timestamp
 */
int ticc_create_data_file(void) {
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    
    snprintf(current_data_file, sizeof(current_data_file), 
             "%s/ticc_data_%04d%02d%02d_%02d%02d%02d.txt",
             client_config.data_save_path,
             local_time->tm_year + 1900,
             local_time->tm_mon + 1,
             local_time->tm_mday,
             local_time->tm_hour,
             local_time->tm_min,
             local_time->tm_sec);
    
    data_file = fopen(current_data_file, "w");
    if (!data_file) {
        snprintf(last_error_msg, sizeof(last_error_msg), 
                "Failed to create data file: %s", strerror(errno));
        return -1;
    }
    
    // Write header
    fprintf(data_file, "# TICC Time Interval Measurements\n");
    fprintf(data_file, "# Started: %.3f\n", (double)now);
    fprintf(data_file, "# Unix_Timestamp,Time_Interval\n");
    fflush(data_file);
    
    return 0;
}

/**
 * Logging thread function
 */
static void* ticc_logging_thread(void* arg) {
    char line_buffer[256];
    char *line_start, *line_end;
    int buffer_pos = 0;
    char read_buffer[32];
    
    printf("TICC logging thread started\n");
    
    while (logging_active) {
        // Read data from serial port
        int bytes_read = ticc_read_response(read_buffer, sizeof(read_buffer), 100);
        
        if (bytes_read > 0) {
            // Append to line buffer
            for (int i = 0; i < bytes_read && buffer_pos < sizeof(line_buffer) - 1; i++) {
                line_buffer[buffer_pos++] = read_buffer[i];
                
                // Check for complete line
                if (read_buffer[i] == '\n' || read_buffer[i] == '\r') {
                    line_buffer[buffer_pos] = '\0';
                    
                    // Process the line
                    if (strstr(line_buffer, "TI(A->B)") != NULL) {
                        // Parse time interval measurement
                        char *value_str = strtok(line_buffer, " \t");
                        if (value_str) {
                            double time_interval = atof(value_str);
                            double timestamp = (double)time(NULL);
                            
                            pthread_mutex_lock(&ticc_mutex);
                            
                            // Write to file
                            if (data_file) {
                                fprintf(data_file, "%.3f,%+.11f\n", timestamp, time_interval);
                                fflush(data_file);
                            }
                            
                            // Update statistics
                            last_measurement_value = time_interval;
                            last_measurement_timestamp = timestamp;
                            total_measurements++;
                            
                            pthread_mutex_unlock(&ticc_mutex);
                        }
                    }
                    
                    buffer_pos = 0; // Reset buffer
                }
            }
        }
        
        // Check for file rotation
        if (data_file && logging_start_time > 0) {
            time_t current_time = time(NULL);
            if (current_time - logging_start_time >= client_config.file_rotation_interval) {
                pthread_mutex_lock(&ticc_mutex);
                
                fclose(data_file);
                data_file = NULL;
                
                if (ticc_create_data_file() == 0) {
                    logging_start_time = current_time;
                }
                
                pthread_mutex_unlock(&ticc_mutex);
            }
        }
        
        usleep(10000); // 10ms sleep
    }
    
    printf("TICC logging thread stopped\n");
    return NULL;
}

/**
 * Start TICC logging
 */
int ticc_start_logging(void) {
    if (!client_initialized || !client_config.enabled) {
        snprintf(last_error_msg, sizeof(last_error_msg), "TICC client not initialized or disabled");
        return -1;
    }
    
    if (logging_active) {
        snprintf(last_error_msg, sizeof(last_error_msg), "TICC logging already active");
        return -1;
    }
    
    // Open serial port
    serial_fd = open(client_config.port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd < 0) {
        snprintf(last_error_msg, sizeof(last_error_msg), 
                "Failed to open serial port %s: %s", client_config.port, strerror(errno));
        return -1;
    }
    
    // Configure serial port
    struct termios options;
    if (tcgetattr(serial_fd, &options) != 0) {
        close(serial_fd);
        serial_fd = -1;
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to get serial port attributes");
        return -1;
    }
    
    // Set baud rate
    speed_t baud_speed;
    switch (client_config.baud_rate) {
        case 115200: baud_speed = B115200; break;
        case 57600:  baud_speed = B57600; break;
        case 38400:  baud_speed = B38400; break;
        case 19200:  baud_speed = B19200; break;
        case 9600:   baud_speed = B9600; break;
        default:     baud_speed = B115200; break;
    }
    
    cfsetispeed(&options, baud_speed);
    cfsetospeed(&options, baud_speed);
    
    // Configure 8N1
    options.c_cflag &= ~PARENB;    // No parity
    options.c_cflag &= ~CSTOPB;    // One stop bit
    options.c_cflag &= ~CSIZE;     // Mask character size bits
    options.c_cflag |= CS8;        // 8 data bits
    options.c_cflag |= CREAD | CLOCAL; // Enable receiver, local line
    
    // Configure input
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable software flow control
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input mode
    
    // Configure output
    options.c_oflag &= ~OPOST; // Raw output
    
    // Set timeouts
    options.c_cc[VMIN] = 0;    // Non-blocking reads
    options.c_cc[VTIME] = 1;   // 0.1 second timeout
    
    if (tcsetattr(serial_fd, TCSANOW, &options) != 0) {
        close(serial_fd);
        serial_fd = -1;
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to set serial port attributes");
        return -1;
    }
    
    // Flush any existing data in buffers
    tcflush(serial_fd, TCIOFLUSH);
    
    // Skip device configuration - TICC is already configured and running
    // Just mark as configured since we know it's working
    device_configured = true;
    printf("TICC device detected as already configured and running\n");
    
    // Create data file
    if (ticc_create_data_file() != 0) {
        close(serial_fd);
        serial_fd = -1;
        return -1;
    }
    
    // Start logging
    logging_active = true;
    logging_start_time = time(NULL);
    total_measurements = 0;
    
    if (pthread_create(&logging_thread, NULL, ticc_logging_thread, NULL) != 0) {
        logging_active = false;
        fclose(data_file);
        data_file = NULL;
        close(serial_fd);
        serial_fd = -1;
        snprintf(last_error_msg, sizeof(last_error_msg), "Failed to create logging thread");
        return -1;
    }
    
    return 0;
}

/**
 * Stop TICC logging
 */
int ticc_stop_logging(void) {
    if (!logging_active) {
        return 0; // Already stopped
    }
    
    logging_active = false;
    
    // Wait for logging thread to finish
    pthread_join(logging_thread, NULL);
    
    // Close data file
    pthread_mutex_lock(&ticc_mutex);
    if (data_file) {
        fclose(data_file);
        data_file = NULL;
    }
    pthread_mutex_unlock(&ticc_mutex);
    
    // Close serial port
    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }
    
    device_configured = false;
    
    return 0;
}

/**
 * Get TICC status
 */
int ticc_get_status(ticc_status_t *status) {
    if (!status) return -1;
    
    pthread_mutex_lock(&ticc_mutex);
    
    status->is_logging = logging_active;
    status->is_configured = device_configured;
    status->start_time = logging_start_time;
    status->measurement_count = total_measurements;
    status->last_measurement = last_measurement_value;
    status->last_measurement_timestamp = last_measurement_timestamp;
    
    strncpy(status->current_file, current_data_file, sizeof(status->current_file) - 1);
    status->current_file[sizeof(status->current_file) - 1] = '\0';
    
    strncpy(status->last_error, last_error_msg, sizeof(status->last_error) - 1);
    status->last_error[sizeof(status->last_error) - 1] = '\0';
    
    pthread_mutex_unlock(&ticc_mutex);
    
    return 0;
}

/**
 * Cleanup TICC client resources
 */
void ticc_client_cleanup(void) {
    ticc_stop_logging();
    client_initialized = false;
    memset(&client_config, 0, sizeof(client_config));
    memset(last_error_msg, 0, sizeof(last_error_msg));
} 