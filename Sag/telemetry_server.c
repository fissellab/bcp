#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "file_io_Sag.h"
#include "telemetry_server.h"
#include "gps.h"

// Global variables
struct sockaddr_in tel_client_addr;
int tel_server_running = 0;
int stop_telemetry_server = 0;
FILE* telemetry_server_log = NULL;

// Configuration and control variables
static telemetry_server_config_t server_config;
static pthread_t telemetry_thread;
static bool server_initialized = false;

// Forward declarations
static bool is_authorized_client(const char *client_ip);

// Helper function to send string data
void telemetry_sendString(int sockfd, const char* string_sample) {
    sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,
           (const struct sockaddr *) &tel_client_addr, sizeof(tel_client_addr));
}

// Helper function to send integer data
void telemetry_sendInt(int sockfd, int sample) {
    char string_sample[16];
    snprintf(string_sample, sizeof(string_sample), "%d", sample);
    sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,
           (const struct sockaddr *) &tel_client_addr, sizeof(tel_client_addr));
}

// Helper function to send float data
void telemetry_sendFloat(int sockfd, float sample) {
    char string_sample[32];
    snprintf(string_sample, sizeof(string_sample), "%.6f", sample);
    sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,
           (const struct sockaddr *) &tel_client_addr, sizeof(tel_client_addr));
}

// Helper function to send double data
void telemetry_sendDouble(int sockfd, double sample) {
    char string_sample[32];
    snprintf(string_sample, sizeof(string_sample), "%.6lf", sample);
    sendto(sockfd, (const char*) string_sample, strlen(string_sample), MSG_CONFIRM,
           (const struct sockaddr *) &tel_client_addr, sizeof(tel_client_addr));
}

// Initialize the telemetry server socket
int telemetry_init_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = server_config.timeout;

    if (sockfd < 0) {
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_init_socket", "Socket creation failed");
        return -1;
    }

    tel_server_running = 1;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_config.port);
    
    if (strcmp(server_config.ip, "0.0.0.0") == 0) {
        servaddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        servaddr.sin_addr.s_addr = inet_addr(server_config.ip);
    }
    
    // Bind the socket to the server address
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_init_socket", "Socket bind failed");
        tel_server_running = 0;
        close(sockfd);
        return -1;
    }
    
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Telemetry server started successfully on port %d", server_config.port);
    write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_init_socket", log_msg);

    return sockfd;
}

// Listen for incoming requests
void telemetry_sock_listen(int sockfd, char* buffer) {
    int n;
    socklen_t cliaddr_len = sizeof(tel_client_addr);
    
    n = recvfrom(sockfd, buffer, TELEMETRY_BUFFER_SIZE - 1, MSG_WAITALL, 
                 (struct sockaddr *) &tel_client_addr, &cliaddr_len);

    if (n > 0) {
        buffer[n] = '\0';
    } else {
        buffer[0] = '\0';
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_sock_listen", "Error receiving data");
        }
    }
}

// Check if client IP is authorized
static bool is_authorized_client(const char *client_ip) {
    if (server_config.udp_client_count == 0) {
        return true; // No restrictions if no clients specified
    }
    
    for (int i = 0; i < server_config.udp_client_count; i++) {
        if (strcmp(client_ip, server_config.udp_client_ips[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Process telemetry requests and send appropriate responses
void telemetry_send_metric(int sockfd, char* id) {
    // Get client IP for logging and authorization
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &tel_client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    // Check authorization
    if (!is_authorized_client(client_ip)) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Rejected request '%s' from unauthorized client: %s", id, client_ip);
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_send_metric", log_msg);
        telemetry_sendString(sockfd, "ERROR:UNAUTHORIZED");
        return;
    }

    // GPS telemetry channels
    if (strcmp(id, "gps_lat") == 0) {
        gps_data_t gps_data;
        if (gps_get_data(&gps_data) && gps_data.valid_position) {
            telemetry_sendDouble(sockfd, gps_data.latitude);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "gps_lon") == 0) {
        gps_data_t gps_data;
        if (gps_get_data(&gps_data) && gps_data.valid_position) {
            telemetry_sendDouble(sockfd, gps_data.longitude);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "gps_alt") == 0) {
        gps_data_t gps_data;
        if (gps_get_data(&gps_data) && gps_data.valid_position) {
            telemetry_sendDouble(sockfd, gps_data.altitude);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "gps_head") == 0) {
        gps_data_t gps_data;
        if (gps_get_data(&gps_data) && gps_data.valid_heading) {
            telemetry_sendDouble(sockfd, gps_data.heading);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "gps_time") == 0) {
        gps_data_t gps_data;
        if (gps_get_data(&gps_data)) {
            char time_str[32];
            snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d",
                    gps_data.year, gps_data.month, gps_data.day,
                    gps_data.hour, gps_data.minute, gps_data.second);
            telemetry_sendString(sockfd, time_str);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "gps_status") == 0) {
        gps_data_t gps_data;
        if (gps_get_data(&gps_data)) {
            char status_str[64];
            snprintf(status_str, sizeof(status_str), "pos:%s,head:%s",
                    gps_data.valid_position ? "valid" : "invalid",
                    gps_data.valid_heading ? "valid" : "invalid");
            telemetry_sendString(sockfd, status_str);
        } else {
            telemetry_sendString(sockfd, "no_data");
        }
    } else if (strcmp(id, "gps_logging") == 0) {
        telemetry_sendInt(sockfd, gps_is_logging() ? 1 : 0);
    }
    // System status channels
    else if (strcmp(id, "uptime") == 0) {
        // Get system uptime
        FILE *uptime_file = fopen("/proc/uptime", "r");
        if (uptime_file) {
            float uptime_seconds;
            if (fscanf(uptime_file, "%f", &uptime_seconds) == 1) {
                telemetry_sendFloat(sockfd, uptime_seconds);
            } else {
                telemetry_sendString(sockfd, "N/A");
            }
            fclose(uptime_file);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "timestamp") == 0) {
        telemetry_sendDouble(sockfd, (double)time(NULL));
    }
    // Future telemetry channels can be added here
    // Examples:
    // else if (strcmp(id, "system_temp") == 0) { /* Add system temperature */ }
    // else if (strcmp(id, "battery_voltage") == 0) { /* Add battery monitoring */ }
    // else if (strcmp(id, "disk_usage") == 0) { /* Add disk usage monitoring */ }
    else {
        // Unknown request
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Received unknown request: '%s' from %s", id, client_ip);
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_send_metric", log_msg);
        telemetry_sendString(sockfd, "ERROR:UNKNOWN_REQUEST");
    }
}

// Main telemetry server thread function
void* telemetry_server_thread(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    int sockfd = telemetry_init_socket();
    char buffer[TELEMETRY_BUFFER_SIZE];

    if (tel_server_running) {
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_thread", "Telemetry server thread started");
        
        while (!stop_telemetry_server) {
            telemetry_sock_listen(sockfd, buffer);
            if (strlen(buffer) > 0) {
                telemetry_send_metric(sockfd, buffer);
            }
        }
        
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_thread", "Shutting down telemetry server");
        tel_server_running = 0;
        stop_telemetry_server = 0;
        close(sockfd);
    } else {
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_thread", "Could not start telemetry server");
    }
    
    return NULL;
}

// Initialize the telemetry server
int telemetry_server_init(const telemetry_server_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&server_config, config, sizeof(telemetry_server_config_t));
    
    // Open log file
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "/home/mayukh/bcp/Sag/log/telemetry_server.log");
    telemetry_server_log = fopen(log_path, "a");
    if (telemetry_server_log == NULL) {
        fprintf(stderr, "Warning: Could not open telemetry server log file: %s\n", strerror(errno));
    }
    
    server_initialized = true;
    write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_init", "Telemetry server initialized");
    
    return 0;
}

// Start the telemetry server
bool telemetry_server_start(void) {
    if (!server_initialized) {
        return false;
    }
    
    if (tel_server_running) {
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_start", "Telemetry server is already running");
        return false;
    }
    
    stop_telemetry_server = 0;
    
    if (pthread_create(&telemetry_thread, NULL, telemetry_server_thread, NULL) != 0) {
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_start", "Error creating telemetry server thread");
        return false;
    }
    
    write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_start", "Telemetry server started");
    return true;
}

// Stop the telemetry server
void telemetry_server_stop(void) {
    if (!tel_server_running) {
        return;
    }
    
    stop_telemetry_server = 1;
    
    // Wait for thread to finish
    pthread_join(telemetry_thread, NULL);
    
    write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_stop", "Telemetry server stopped");
}

// Cleanup telemetry server resources
void telemetry_server_cleanup(void) {
    if (telemetry_server_log) {
        fclose(telemetry_server_log);
        telemetry_server_log = NULL;
    }
    server_initialized = false;
}

// Check if telemetry server is running
bool telemetry_server_is_running(void) {
    return tel_server_running;
} 