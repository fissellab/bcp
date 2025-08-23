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
#include "pr59_interface.h"
#include "position_sensors.h"
#include "heaters.h"
#include "vlbi_client.h"
#include "system_monitor.h"
#include "ticc_client.h"
#include "aquila_status.h"

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

// Check if client IP is authorized - now accepts all clients
static bool is_authorized_client(const char *client_ip) {
    (void)client_ip;  // Suppress unused parameter warning
    return true;  // Accept all clients
}

// Process telemetry requests and send appropriate responses
void telemetry_send_metric(int sockfd, char* id) {
    // Get client IP for logging
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &tel_client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    // Note: Normal requests are no longer logged to reduce verbosity
    // Only errors and unknown requests will be logged

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
    } else if (strcmp(id, "gps_speed") == 0) {
        gps_data_t gps_data;
        if (gps_get_data(&gps_data) && gps_data.valid_speed) {
            telemetry_sendDouble(sockfd, gps_data.speed_ms);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "gps_sats") == 0) {
        gps_data_t gps_data;
        if (gps_get_data(&gps_data) && gps_data.valid_satellites) {
            telemetry_sendInt(sockfd, gps_data.num_satellites);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "GET_GPS") == 0) {
        // Handle GET_GPS command for compatibility with existing clients
        gps_data_t gps_data;
        if (gps_get_data(&gps_data)) {
            char gps_response[512];  // Increased buffer size for additional parameters
            
            // Build response with all GPS parameters
            char lat_str[32], lon_str[32], alt_str[32], head_str[32], speed_str[32], sats_str[16];
            
            // Format position data
            if (gps_data.valid_position) {
                snprintf(lat_str, sizeof(lat_str), "%.6f", gps_data.latitude);
                snprintf(lon_str, sizeof(lon_str), "%.6f", gps_data.longitude);
                snprintf(alt_str, sizeof(alt_str), "%.1f", gps_data.altitude);
            } else {
                strcpy(lat_str, "N/A");
                strcpy(lon_str, "N/A");
                strcpy(alt_str, "N/A");
            }
            
            // Format heading data
            if (gps_data.valid_heading) {
                snprintf(head_str, sizeof(head_str), "%.2f", gps_data.heading);
            } else {
                strcpy(head_str, "N/A");
            }
            
            // Format speed data
            if (gps_data.valid_speed) {
                snprintf(speed_str, sizeof(speed_str), "%.3f", gps_data.speed_ms);
            } else {
                strcpy(speed_str, "N/A");
            }
            
            // Format satellite data
            if (gps_data.valid_satellites) {
                snprintf(sats_str, sizeof(sats_str), "%d", gps_data.num_satellites);
            } else {
                strcpy(sats_str, "N/A");
            }
            
            snprintf(gps_response, sizeof(gps_response), 
                    "gps_lat:%s,gps_lon:%s,gps_alt:%s,gps_head:%s,gps_speed:%s,gps_sats:%s",
                    lat_str, lon_str, alt_str, head_str, speed_str, sats_str);
            
            telemetry_sendString(sockfd, gps_response);
        } else {
            telemetry_sendString(sockfd, "gps_lat:N/A,gps_lon:N/A,gps_alt:N/A,gps_head:N/A,gps_speed:N/A,gps_sats:N/A");
        }
    }
    
    // PR59 TEC controller telemetry channels
    else if (strcmp(id, "pr59_kp") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendFloat(sockfd, pr59_data.kp);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_ki") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendFloat(sockfd, pr59_data.ki);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_kd") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendFloat(sockfd, pr59_data.kd);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_timestamp") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendDouble(sockfd, (double)pr59_data.timestamp);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_temp") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendFloat(sockfd, pr59_data.temperature);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_fet_temp") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendFloat(sockfd, pr59_data.fet_temperature);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_current") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendFloat(sockfd, pr59_data.current);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_voltage") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendFloat(sockfd, pr59_data.voltage);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_power") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            telemetry_sendFloat(sockfd, pr59_data.power);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pr59_running") == 0) {
        telemetry_sendInt(sockfd, pr59_is_running() ? 1 : 0);
    } else if (strcmp(id, "pr59_status") == 0) {
        pr59_data_t pr59_data;
        if (pr59_get_data(&pr59_data)) {
            char status_str[64];
            const char* thermal_status = pr59_data.is_at_setpoint ? "setpoint" : 
                                       (pr59_data.is_heating ? "heating" : "cooling");
            snprintf(status_str, sizeof(status_str), "running:%s,thermal:%s",
                    pr59_data.is_running ? "yes" : "no", thermal_status);
            telemetry_sendString(sockfd, status_str);
        } else {
            telemetry_sendString(sockfd, "not_running");
        }
    }
    
    // Position sensor telemetry channels
    else if (strcmp(id, "pos_spi_gyro_rate") == 0) {
        pos_gyro_spi_sample_t spi_data;
        double timestamp;
        if (position_sensors_get_spi_gyro_data(&spi_data, &timestamp)) {
            telemetry_sendFloat(sockfd, spi_data.rate);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel1_x") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(0, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.x);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel1_y") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(0, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.y);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel1_z") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(0, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.z);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel2_x") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(1, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.x);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel2_y") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(1, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.y);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel2_z") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(1, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.z);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel3_x") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(2, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.x);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel3_y") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(2, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.y);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_accel3_z") == 0) {
        pos_accel_sample_t accel_data;
        double timestamp;
        if (position_sensors_get_accel_data(2, &accel_data, &timestamp)) {
            telemetry_sendFloat(sockfd, accel_data.z);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "pos_status") == 0) {
        if (position_sensors_is_enabled() && position_sensors_is_running()) {
            pos_sensor_status_t status;
            if (position_sensors_get_status(&status) == 0) {
                char status_str[128];
                snprintf(status_str, sizeof(status_str), "connected:%s,script:%s,data:%s",
                        status.connected ? "yes" : "no",
                        status.script_running ? "yes" : "no", 
                        status.data_active ? "yes" : "no");
                telemetry_sendString(sockfd, status_str);
            } else {
                telemetry_sendString(sockfd, "status_error");
            }
        } else {
            telemetry_sendString(sockfd, "disabled");
        }
    } else if (strcmp(id, "pos_running") == 0) {
        telemetry_sendInt(sockfd, position_sensors_is_running() ? 1 : 0);
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
    
    // Heater telemetry channels
    else if (strcmp(id, "heater_running") == 0) {
        telemetry_sendInt(sockfd, heaters_running);
    } else if (strcmp(id, "heater_starcam_temp") == 0) {
        if (heaters_running && heaters[0].temp_valid) {
            telemetry_sendFloat(sockfd, heaters[0].current_temp);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_starcam_current") == 0) {
        if (heaters_running) {
            telemetry_sendFloat(sockfd, heaters[0].current);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_starcam_state") == 0) {
        if (heaters_running) {
            telemetry_sendInt(sockfd, heaters[0].state ? 1 : 0);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_motor_temp") == 0) {
        if (heaters_running && heaters[1].temp_valid) {
            telemetry_sendFloat(sockfd, heaters[1].current_temp);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_motor_current") == 0) {
        if (heaters_running) {
            telemetry_sendFloat(sockfd, heaters[1].current);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_motor_state") == 0) {
        if (heaters_running) {
            telemetry_sendInt(sockfd, heaters[1].state ? 1 : 0);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_ethernet_temp") == 0) {
        if (heaters_running && heaters[2].temp_valid) {
            telemetry_sendFloat(sockfd, heaters[2].current_temp);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_ethernet_current") == 0) {
        if (heaters_running) {
            telemetry_sendFloat(sockfd, heaters[2].current);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_ethernet_state") == 0) {
        if (heaters_running) {
            telemetry_sendInt(sockfd, heaters[2].state ? 1 : 0);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_lockpin_temp") == 0) {
        if (heaters_running && heaters[3].temp_valid) {
            telemetry_sendFloat(sockfd, heaters[3].current_temp);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_lockpin_current") == 0) {
        if (heaters_running) {
            telemetry_sendFloat(sockfd, heaters[3].current);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_lockpin_state") == 0) {
        if (heaters_running) {
            telemetry_sendInt(sockfd, heaters[3].state ? 1 : 0);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_spare_temp") == 0) {
        if (heaters_running && heaters[4].temp_valid) {
            telemetry_sendFloat(sockfd, heaters[4].current_temp);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_spare_current") == 0) {
        if (heaters_running) {
            telemetry_sendFloat(sockfd, heaters[4].current);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_spare_state") == 0) {
        if (heaters_running) {
            telemetry_sendInt(sockfd, heaters[4].state ? 1 : 0);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "heater_total_current") == 0) {
        if (heaters_running) {
            float total_current = 0.0;
            for (int i = 0; i < NUM_HEATERS; i++) {
                total_current += heaters[i].current;
            }
            telemetry_sendFloat(sockfd, total_current);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    }
    
    // VLBI telemetry channels
    else if (strcmp(id, "vlbi_running") == 0) {
        if (vlbi_status_valid && vlbi_client_is_enabled()) {
            telemetry_sendInt(sockfd, global_vlbi_status.is_running ? 1 : 0);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "vlbi_stage") == 0) {
        if (vlbi_status_valid && vlbi_client_is_enabled()) {
            telemetry_sendString(sockfd, global_vlbi_status.stage[0] ? global_vlbi_status.stage : "unknown");
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "vlbi_packets") == 0) {
        if (vlbi_status_valid && vlbi_client_is_enabled()) {
            telemetry_sendInt(sockfd, global_vlbi_status.packets_captured);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "vlbi_data_mb") == 0) {
        if (vlbi_status_valid && vlbi_client_is_enabled()) {
            telemetry_sendDouble(sockfd, global_vlbi_status.data_size_mb);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "vlbi_connection") == 0) {
        if (vlbi_status_valid && vlbi_client_is_enabled()) {
            telemetry_sendString(sockfd, global_vlbi_status.connection_status[0] ? global_vlbi_status.connection_status : "unknown");
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "vlbi_errors") == 0) {
        if (vlbi_status_valid && vlbi_client_is_enabled()) {
            telemetry_sendInt(sockfd, global_vlbi_status.error_count);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "vlbi_pid") == 0) {
        if (vlbi_status_valid && vlbi_client_is_enabled() && global_vlbi_status.is_running) {
            telemetry_sendInt(sockfd, global_vlbi_status.pid);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "vlbi_last_update") == 0) {
        if (vlbi_status_valid && vlbi_client_is_enabled()) {
            telemetry_sendString(sockfd, global_vlbi_status.last_update[0] ? global_vlbi_status.last_update : global_vlbi_status.timestamp);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "GET_VLBI") == 0) {
        // Handle GET_VLBI command for comprehensive VLBI status
        if (vlbi_status_valid && vlbi_client_is_enabled()) {
            char vlbi_response[512];
            snprintf(vlbi_response, sizeof(vlbi_response), 
                    "vlbi_running:%d,vlbi_stage:%s,vlbi_packets:%d,vlbi_data_mb:%.2f,vlbi_connection:%s,vlbi_errors:%d",
                    global_vlbi_status.is_running ? 1 : 0,
                    global_vlbi_status.stage[0] ? global_vlbi_status.stage : "unknown",
                    global_vlbi_status.packets_captured,
                    global_vlbi_status.data_size_mb,
                    global_vlbi_status.connection_status[0] ? global_vlbi_status.connection_status : "unknown",
                    global_vlbi_status.error_count);
            telemetry_sendString(sockfd, vlbi_response);
        } else {
            telemetry_sendString(sockfd, "vlbi_running:N/A,vlbi_stage:N/A,vlbi_packets:N/A,vlbi_data_mb:N/A,vlbi_connection:N/A,vlbi_errors:N/A");
        }
    }
    
    // System Monitor telemetry channels
    else if (strcmp(id, "sag_sys_cpu_temp") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendFloat(sockfd, sys_monitor.cpu_temp_celsius);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_cpu_usage") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendFloat(sockfd, sys_monitor.cpu_usage_percent);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_mem_used") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendFloat(sockfd, sys_monitor.memory_used_gb);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_mem_total") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendFloat(sockfd, sys_monitor.memory_total_gb);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_mem_used_str") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendString(sockfd, sys_monitor.memory_used_str);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_mem_total_str") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendString(sockfd, sys_monitor.memory_total_str);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_ssd_mounted") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendInt(sockfd, sys_monitor.ssd_mounted);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_ssd_used") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendString(sockfd, sys_monitor.ssd_used);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_ssd_total") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendString(sockfd, sys_monitor.ssd_total);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "sag_sys_ssd_path") == 0) {
        if (system_monitor_running) {
            pthread_mutex_lock(&sys_monitor.data_mutex);
            telemetry_sendString(sockfd, sys_monitor.ssd_mount_path);
            pthread_mutex_unlock(&sys_monitor.data_mutex);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    }
    
    // TICC telemetry channels
    else if (strcmp(id, "ticc_timestamp") == 0) {
        if (ticc_client_is_enabled()) {
            ticc_status_t ticc_status;
            if (ticc_get_status(&ticc_status) == 0 && ticc_status.is_logging && ticc_status.last_measurement_timestamp > 0) {
                telemetry_sendDouble(sockfd, ticc_status.last_measurement_timestamp);
            } else {
                telemetry_sendString(sockfd, "N/A");
            }
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "ticc_interval") == 0) {
        if (ticc_client_is_enabled()) {
            ticc_status_t ticc_status;
            if (ticc_get_status(&ticc_status) == 0 && ticc_status.is_logging) {
                telemetry_sendDouble(sockfd, ticc_status.last_measurement);
            } else {
                telemetry_sendString(sockfd, "N/A");
            }
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "ticc_logging") == 0) {
        if (ticc_client_is_enabled()) {
            ticc_status_t ticc_status;
            if (ticc_get_status(&ticc_status) == 0) {
                telemetry_sendInt(sockfd, ticc_status.is_logging ? 1 : 0);
            } else {
                telemetry_sendString(sockfd, "N/A");
            }
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "ticc_measurement_count") == 0) {
        if (ticc_client_is_enabled()) {
            ticc_status_t ticc_status;
            if (ticc_get_status(&ticc_status) == 0) {
                telemetry_sendInt(sockfd, ticc_status.measurement_count);
            } else {
                telemetry_sendString(sockfd, "N/A");
            }
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "ticc_current_file") == 0) {
        if (ticc_client_is_enabled()) {
            ticc_status_t ticc_status;
            if (ticc_get_status(&ticc_status) == 0 && ticc_status.is_logging) {
                telemetry_sendString(sockfd, ticc_status.current_file[0] ? ticc_status.current_file : "N/A");
            } else {
                telemetry_sendString(sockfd, "N/A");
            }
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "ticc_status") == 0) {
        if (ticc_client_is_enabled()) {
            ticc_status_t ticc_status;
            if (ticc_get_status(&ticc_status) == 0) {
                char status_str[128];
                snprintf(status_str, sizeof(status_str), "logging:%s,configured:%s,measurements:%d",
                        ticc_status.is_logging ? "yes" : "no",
                        ticc_status.is_configured ? "yes" : "no",
                        ticc_status.measurement_count);
                telemetry_sendString(sockfd, status_str);
            } else {
                telemetry_sendString(sockfd, "error");
            }
        } else {
            telemetry_sendString(sockfd, "disabled");
        }
    } else if (strcmp(id, "GET_TICC") == 0) {
        // Handle GET_TICC command for comprehensive TICC status
        if (ticc_client_is_enabled()) {
            ticc_status_t ticc_status;
            if (ticc_get_status(&ticc_status) == 0) {
                char ticc_response[512];
                
                // Format timestamp and interval
                char timestamp_str[32], interval_str[32];
                if (ticc_status.is_logging && ticc_status.last_measurement_timestamp > 0) {
                    snprintf(timestamp_str, sizeof(timestamp_str), "%.3f", ticc_status.last_measurement_timestamp);
                    snprintf(interval_str, sizeof(interval_str), "%.11f", ticc_status.last_measurement);
                } else {
                    strcpy(timestamp_str, "N/A");
                    strcpy(interval_str, "N/A");
                }
                
                snprintf(ticc_response, sizeof(ticc_response), 
                        "ticc_timestamp:%s,ticc_interval:%s,ticc_logging:%d,ticc_measurement_count:%d",
                        timestamp_str, interval_str,
                        ticc_status.is_logging ? 1 : 0,
                        ticc_status.measurement_count);
                
                telemetry_sendString(sockfd, ticc_response);
            } else {
                telemetry_sendString(sockfd, "ticc_timestamp:N/A,ticc_interval:N/A,ticc_logging:N/A,ticc_measurement_count:N/A");
            }
        } else {
            telemetry_sendString(sockfd, "ticc_timestamp:N/A,ticc_interval:N/A,ticc_logging:N/A,ticc_measurement_count:N/A");
        }
    }
    
    // Aquila Backend System telemetry channels
    else if (strcmp(id, "aquila_ssd1_mounted") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendInt(sockfd, aquila_status.ssd1_mounted ? 1 : 0);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_ssd1_used_gb") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendFloat(sockfd, aquila_status.ssd1_used_gb);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_ssd1_total_gb") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendFloat(sockfd, aquila_status.ssd1_total_gb);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_ssd1_percent") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendFloat(sockfd, aquila_status.ssd1_percent_used);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_ssd2_mounted") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendInt(sockfd, aquila_status.ssd2_mounted ? 1 : 0);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_ssd2_used_gb") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendFloat(sockfd, aquila_status.ssd2_used_gb);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_ssd2_total_gb") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendFloat(sockfd, aquila_status.ssd2_total_gb);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_ssd2_percent") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendFloat(sockfd, aquila_status.ssd2_percent_used);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_cpu_temp") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendFloat(sockfd, aquila_status.cpu_temp_celsius);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_memory_percent") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            telemetry_sendFloat(sockfd, aquila_status.memory_percent_used);
        } else {
            telemetry_sendString(sockfd, "N/A");
        }
    } else if (strcmp(id, "aquila_status") == 0) {
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            char status_str[256];
            snprintf(status_str, sizeof(status_str), 
                    "ssd1:%s(%.1f%%), ssd2:%s(%.1f%%), cpu:%.1f°C, mem:%.1f%%",
                    aquila_status.ssd1_mounted ? "mounted" : "unmounted", aquila_status.ssd1_percent_used,
                    aquila_status.ssd2_mounted ? "mounted" : "unmounted", aquila_status.ssd2_percent_used,
                    aquila_status.cpu_temp_celsius, aquila_status.memory_percent_used);
            telemetry_sendString(sockfd, status_str);
        } else {
            telemetry_sendString(sockfd, "no_data");
        }
    } else if (strcmp(id, "GET_AQUILA") == 0) {
        // Handle GET_AQUILA command for comprehensive aquila status
        aquila_status_t aquila_status;
        if (aquila_status_get_data(&aquila_status) == 0) {
            char aquila_response[512];
            snprintf(aquila_response, sizeof(aquila_response), 
                    "aquila_ssd1_mounted:%d,aquila_ssd1_percent:%.1f,aquila_ssd1_used_gb:%.1f,aquila_ssd1_total_gb:%.1f,"
                    "aquila_ssd2_mounted:%d,aquila_ssd2_percent:%.1f,aquila_ssd2_used_gb:%.1f,aquila_ssd2_total_gb:%.1f,"
                    "aquila_cpu_temp:%.1f,aquila_memory_percent:%.1f",
                    aquila_status.ssd1_mounted, aquila_status.ssd1_percent_used, aquila_status.ssd1_used_gb, aquila_status.ssd1_total_gb,
                    aquila_status.ssd2_mounted, aquila_status.ssd2_percent_used, aquila_status.ssd2_used_gb, aquila_status.ssd2_total_gb,
                    aquila_status.cpu_temp_celsius, aquila_status.memory_percent_used);
            telemetry_sendString(sockfd, aquila_response);
        } else {
            telemetry_sendString(sockfd, "aquila_ssd1_mounted:N/A,aquila_ssd1_percent:N/A,aquila_ssd1_used_gb:N/A,aquila_ssd1_total_gb:N/A,"
                                        "aquila_ssd2_mounted:N/A,aquila_ssd2_percent:N/A,aquila_ssd2_used_gb:N/A,aquila_ssd2_total_gb:N/A,"
                                        "aquila_cpu_temp:N/A,aquila_memory_percent:N/A");
        }
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
                // Check if this is JSON data from aquila (starts with '{')
                if (buffer[0] == '{' && strstr(buffer, "aquila_system_status") != NULL) {
                    // Process aquila status update
                    aquila_status_update_from_json(buffer);
                } else {
                    // Process normal telemetry request
                    telemetry_send_metric(sockfd, buffer);
                }
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
    
    // Open log file in timestamped directory
    char log_path[512];
    get_timestamped_log_path("telemetry_server.log", log_path, sizeof(log_path));
    telemetry_server_log = fopen(log_path, "a");
    if (telemetry_server_log == NULL) {
        fprintf(stderr, "Warning: Could not open telemetry server log file: %s\n", strerror(errno));
    }
    
    // Initialize aquila status module
    if (aquila_status_init() != 0) {
        write_to_log(telemetry_server_log, "telemetry_server.c", "telemetry_server_init", "Failed to initialize aquila status module");
        return -1;
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
    // Cleanup aquila status module
    aquila_status_cleanup();
    
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