#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#include "position_sensors.h"
#include "file_io_Sag.h"

// Global variables
static bool client_initialized = false;
static pos_sensor_config_t client_config;
static pos_sensor_status_t sensor_status;
static pthread_t data_thread;
static pthread_t script_thread;
static bool data_thread_running = false;
static bool script_thread_running = false;
static pid_t script_pid = -1;
static FILE *pos_log_file = NULL;

// Forward declarations
static void *script_management_thread(void *arg);
static void *data_reception_thread(void *arg);
static void log_position_message(const char *message);
static bool validate_packet(const pos_sensor_packet_t *packet);
static void process_sensor_packet(const pos_sensor_packet_t *packet);
static void update_telemetry_data(const pos_sensor_packet_t *packet);

// Logging function
static void log_position_message(const char *message) {
    time_t now;
    char timestamp[64];
    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Print to stderr for immediate feedback
    fprintf(stderr, "[POSITION][%s] %s\n", timestamp, message);
    
    if (pos_log_file) {
        fprintf(pos_log_file, "[%s] %s\n", timestamp, message);
        fflush(pos_log_file);
    }
}

// Initialize position sensor client
int position_sensors_init(const pos_sensor_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&client_config, config, sizeof(pos_sensor_config_t));
    
    // Initialize status structure
    memset(&sensor_status, 0, sizeof(pos_sensor_status_t));
    sensor_status.connected = false;
    sensor_status.script_running = false;
    sensor_status.data_active = false;
    strcpy(sensor_status.last_error, "Not started");
    
    // Initialize mutexes
    if (pthread_mutex_init(&sensor_status.spi_gyro_mutex, NULL) != 0 ||
        pthread_mutex_init(&sensor_status.i2c_gyro_mutex, NULL) != 0 ||
        pthread_mutex_init(&sensor_status.accel_mutex, NULL) != 0) {
        log_position_message("Failed to initialize mutexes");
        return -1;
    }
    
    // Open log file
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "log/position_sensors.log");
    pos_log_file = fopen(log_path, "a");
    if (pos_log_file == NULL) {
        fprintf(stderr, "Warning: Could not open position sensor log file: %s\n", strerror(errno));
    }
    
    client_initialized = true;
    log_position_message("Position sensor client initialized");
    
    return 0;
}

// Start position sensor system
bool position_sensors_start(void) {
    if (!client_initialized || !client_config.enabled) {
        log_position_message("Position sensor client not initialized or disabled");
        return false;
    }
    
    if (script_thread_running || data_thread_running) {
        log_position_message("Position sensor system is already running");
        return false;
    }
    
    log_position_message("Starting position sensor system...");
    
    // Start script management thread
    script_thread_running = true;
    if (pthread_create(&script_thread, NULL, script_management_thread, NULL) != 0) {
        log_position_message("Failed to create script management thread");
        script_thread_running = false;
        return false;
    }
    
    // Start data reception thread  
    data_thread_running = true;
    if (pthread_create(&data_thread, NULL, data_reception_thread, NULL) != 0) {
        log_position_message("Failed to create data reception thread");
        data_thread_running = false;
        script_thread_running = false;
        pthread_cancel(script_thread);
        return false;
    }
    
    log_position_message("Position sensor system started successfully");
    return true;
}

// Stop position sensor system
void position_sensors_stop(void) {
    if (!script_thread_running && !data_thread_running) {
        return;
    }
    
    log_position_message("Stopping position sensor system...");
    
    // Stop threads
    script_thread_running = false;
    data_thread_running = false;
    
    // Wait for threads to finish
    if (data_thread_running) {
        pthread_join(data_thread, NULL);
    }
    if (script_thread_running) {
        pthread_join(script_thread, NULL);
    }
    
    // Kill script if running
    if (script_pid > 0) {
        log_position_message("Terminating position sensor script...");
        kill(script_pid, SIGTERM);
        
        // Wait for script to terminate gracefully
        int status;
        for (int i = 0; i < 10; i++) {
            pid_t result = waitpid(script_pid, &status, WNOHANG);
            if (result == script_pid || result == -1) {
                break;
            }
            sleep(1);
        }
        
        // Force kill if still running
        if (waitpid(script_pid, &status, WNOHANG) == 0) {
            log_position_message("Force killing position sensor script...");
            kill(script_pid, SIGKILL);
            waitpid(script_pid, NULL, 0);
        }
        
        script_pid = -1;
    }
    
    // Update status
    sensor_status.connected = false;
    sensor_status.script_running = false;
    sensor_status.data_active = false;
    strcpy(sensor_status.last_error, "Stopped");
    
    log_position_message("Position sensor system stopped");
}

// Check if position sensors are enabled
bool position_sensors_is_enabled(void) {
    return client_initialized && client_config.enabled;
}

// Check if position sensors are running
bool position_sensors_is_running(void) {
    return script_thread_running || data_thread_running;
}

// Get position sensor status
int position_sensors_get_status(pos_sensor_status_t *status) {
    if (!status || !client_initialized) {
        return -1;
    }
    
    memcpy(status, &sensor_status, sizeof(pos_sensor_status_t));
    return 0;
}

// Get latest SPI gyro data for telemetry
bool position_sensors_get_spi_gyro_data(pos_gyro_spi_sample_t *data, double *timestamp) {
    if (!data || !timestamp || !client_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&sensor_status.spi_gyro_mutex);
    *data = sensor_status.latest_spi_gyro;
    *timestamp = sensor_status.spi_gyro_timestamp;
    bool valid = (sensor_status.spi_gyro_timestamp > 0);
    pthread_mutex_unlock(&sensor_status.spi_gyro_mutex);
    
    return valid;
}

// Get latest I2C gyro data for telemetry
bool position_sensors_get_i2c_gyro_data(pos_gyro_i2c_sample_t *data, double *timestamp) {
    if (!data || !timestamp || !client_initialized) {
        return false;
    }
    
    pthread_mutex_lock(&sensor_status.i2c_gyro_mutex);
    *data = sensor_status.latest_i2c_gyro;
    *timestamp = sensor_status.i2c_gyro_timestamp;
    bool valid = (sensor_status.i2c_gyro_timestamp > 0);
    pthread_mutex_unlock(&sensor_status.i2c_gyro_mutex);
    
    return valid;
}

// Get latest accelerometer data for telemetry
bool position_sensors_get_accel_data(int sensor_id, pos_accel_sample_t *data, double *timestamp) {
    if (!data || !timestamp || !client_initialized || sensor_id < 0 || sensor_id >= 3) {
        return false;
    }
    
    pthread_mutex_lock(&sensor_status.accel_mutex);
    *data = sensor_status.latest_accels[sensor_id];
    *timestamp = sensor_status.accel_timestamps[sensor_id];
    bool valid = (sensor_status.accel_timestamps[sensor_id] > 0);
    pthread_mutex_unlock(&sensor_status.accel_mutex);
    
    return valid;
}

// Script management thread
static void *script_management_thread(void *arg) {
    (void)arg;
    
    log_position_message("Script management thread started");
    
    while (script_thread_running) {
        if (!sensor_status.script_running && script_thread_running) {
            log_position_message("Starting position sensor script...");
            
            // Fork and execute the script
            script_pid = fork();
            
            if (script_pid == 0) {
                // Child process - execute the script
                char *args[] = {"/bin/bash", client_config.script_path, NULL};
                execv("/bin/bash", args);
                perror("execv failed");
                exit(1);
            } else if (script_pid > 0) {
                // Parent process
                sensor_status.script_running = true;
                strcpy(sensor_status.last_error, "Script started");
                log_position_message("Position sensor script started successfully");
                
                // Wait for script to finish or be terminated
                int status;
                pid_t result = waitpid(script_pid, &status, 0);
                
                if (result == script_pid) {
                    sensor_status.script_running = false;
                    if (WIFEXITED(status)) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Script exited with code %d", WEXITSTATUS(status));
                        strcpy(sensor_status.last_error, msg);
                        log_position_message(msg);
                    } else if (WIFSIGNALED(status)) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Script terminated by signal %d", WTERMSIG(status));
                        strcpy(sensor_status.last_error, msg);
                        log_position_message(msg);
                    }
                } else {
                    sensor_status.script_running = false;
                    strcpy(sensor_status.last_error, "Script wait failed");
                    log_position_message("Script wait failed");
                }
                
                script_pid = -1;
            } else {
                // Fork failed
                strcpy(sensor_status.last_error, "Failed to start script");
                log_position_message("Failed to fork script process");
            }
        }
        
        sleep(1); // Check every second
    }
    
    log_position_message("Script management thread stopped");
    return NULL;
}

// Data reception thread
static void *data_reception_thread(void *arg) {
    (void)arg;
    
    log_position_message("Data reception thread started");
    
    int sockfd = -1;
    struct sockaddr_in server_addr;
    char buffer[4096];
    
    while (data_thread_running) {
        // Try to connect to Pi if not connected
        if (sockfd < 0) {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                strcpy(sensor_status.last_error, "Failed to create socket");
                sleep(5);
                continue;
            }
            
            // Set socket timeout
            struct timeval timeout;
            timeout.tv_sec = client_config.connection_timeout;
            timeout.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            
            // Setup server address
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(client_config.pi_port);
            
            if (inet_pton(AF_INET, client_config.pi_ip, &server_addr.sin_addr) <= 0) {
                strcpy(sensor_status.last_error, "Invalid Pi IP address");
                close(sockfd);
                sockfd = -1;
                sleep(5);
                continue;
            }
            
            // Connect to Pi
            if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Connection to Pi failed: %s", strerror(errno));
                strcpy(sensor_status.last_error, msg);
                close(sockfd);
                sockfd = -1;
                sensor_status.connected = false;
                sleep(5);
                continue;
            }
            
            sensor_status.connected = true;
            strcpy(sensor_status.last_error, "Connected to Pi");
            log_position_message("Connected to position sensor Pi");
        }
        
        // Receive data
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
        
        if (bytes_received > 0) {
            sensor_status.data_active = true;
            
            // Process received data (expecting complete packets)
            if (bytes_received >= (ssize_t)sizeof(pos_sensor_packet_t)) {
                pos_sensor_packet_t *packet = (pos_sensor_packet_t*)buffer;
                
                if (validate_packet(packet)) {
                    process_sensor_packet(packet);
                    update_telemetry_data(packet);
                    sensor_status.packets_received++;
                    sensor_status.last_packet_time = time(NULL);
                } else {
                    log_position_message("Received invalid packet");
                }
            }
        } else if (bytes_received == 0) {
            // Connection closed
            log_position_message("Pi closed connection");
            close(sockfd);
            sockfd = -1;
            sensor_status.connected = false;
            sensor_status.data_active = false;
            strcpy(sensor_status.last_error, "Connection closed by Pi");
        } else {
            // Error receiving data
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout - continue
                continue;
            } else {
                char msg[256];
                snprintf(msg, sizeof(msg), "Receive error: %s", strerror(errno));
                strcpy(sensor_status.last_error, msg);
                close(sockfd);
                sockfd = -1;
                sensor_status.connected = false;
                sensor_status.data_active = false;
            }
        }
    }
    
    if (sockfd >= 0) {
        close(sockfd);
    }
    
    log_position_message("Data reception thread stopped");
    return NULL;
}

// Validate received packet
static bool validate_packet(const pos_sensor_packet_t *packet) {
    return (packet->header.magic == 0xDEADBEEF);
}

// Process sensor packet (data logging handled by pos_sensor_rx)
static void process_sensor_packet(const pos_sensor_packet_t *packet) {
    // Log packet statistics periodically
    static uint32_t last_log_count = 0;
    if (sensor_status.packets_received - last_log_count >= 10000) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Received %u packets, seq=%u", 
                sensor_status.packets_received, packet->header.sequence);
        log_position_message(msg);
        last_log_count = sensor_status.packets_received;
    }
    
    // Check for packet loss
    static uint16_t last_sequence = 0;
    if (sensor_status.packets_received > 0) {
        uint16_t expected = (last_sequence + 1) & 0xFFFF;
        if (packet->header.sequence != expected) {
            uint32_t lost;
            if (packet->header.sequence > expected) {
                lost = packet->header.sequence - expected;
            } else {
                lost = (65536 - expected + packet->header.sequence);
            }
            sensor_status.packets_lost += lost;
        }
    }
    last_sequence = packet->header.sequence;
}

// Update telemetry data structures
static void update_telemetry_data(const pos_sensor_packet_t *packet) {
    double timestamp = packet->header.timestamp_sec + packet->header.timestamp_nsec / 1000000000.0;
    
    // Update accelerometer data
    pthread_mutex_lock(&sensor_status.accel_mutex);
    for (int i = 0; i < 3; i++) {
        sensor_status.latest_accels[i] = packet->accels[i];
        sensor_status.accel_timestamps[i] = timestamp;
        sensor_status.total_samples[i]++;
    }
    pthread_mutex_unlock(&sensor_status.accel_mutex);
    
    // Update I2C gyro data
    pthread_mutex_lock(&sensor_status.i2c_gyro_mutex);
    sensor_status.latest_i2c_gyro = packet->gyro_i2c;
    sensor_status.i2c_gyro_timestamp = timestamp;
    sensor_status.total_samples[3]++;
    pthread_mutex_unlock(&sensor_status.i2c_gyro_mutex);
    
    // Update SPI gyro data (if present in packet)
    if (packet->header.sensor_mask & 0x10) {
        pthread_mutex_lock(&sensor_status.spi_gyro_mutex);
        sensor_status.latest_spi_gyro = packet->gyro_spi;
        sensor_status.spi_gyro_timestamp = timestamp;
        sensor_status.total_samples[4]++;
        pthread_mutex_unlock(&sensor_status.spi_gyro_mutex);
    }
}

// Cleanup function
void position_sensors_cleanup(void) {
    if (!client_initialized) {
        return;
    }
    
    position_sensors_stop();
    
    // Destroy mutexes
    pthread_mutex_destroy(&sensor_status.spi_gyro_mutex);
    pthread_mutex_destroy(&sensor_status.i2c_gyro_mutex);
    pthread_mutex_destroy(&sensor_status.accel_mutex);
    
    if (pos_log_file) {
        fclose(pos_log_file);
        pos_log_file = NULL;
    }
    
    client_initialized = false;
    log_position_message("Position sensor client cleaned up");
} 