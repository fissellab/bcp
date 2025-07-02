#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "vlbi_client.h"
#include "file_io_Sag.h"

// JSON parsing (simple implementation)
#include <ctype.h>

// Global configuration
static vlbi_client_config_t client_config;
static bool client_initialized = false;

/**
 * Simple JSON value extraction (for our specific use case)
 * Note: Caller must free the returned string
 */
static char* extract_json_string(const char* json, const char* key) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* pos = strstr(json, search_key);
    if (!pos) return NULL;
    
    pos += strlen(search_key);
    while (*pos && isspace(*pos)) pos++;
    
    if (*pos == '"') {
        pos++;
        char* end = strchr(pos, '"');
        if (end) {
            int len = end - pos;
            char* result = malloc(len + 1);
            if (result) {
                strncpy(result, pos, len);
                result[len] = '\0';
                return result;
            }
        }
    }
    return NULL;
}

static int extract_json_int(const char* json, const char* key) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* pos = strstr(json, search_key);
    if (!pos) return -1;
    
    pos += strlen(search_key);
    while (*pos && isspace(*pos)) pos++;
    
    return atoi(pos);
}

/**
 * Send command to VLBI daemon and receive response
 */
static int send_vlbi_command(const char* command, char* response, size_t response_size) {
    int sockfd;
    struct sockaddr_in server_addr;
    struct timeval timeout;
    
    if (!client_initialized || !client_config.enabled) {
        printf("VLBI client not initialized or disabled\n");
        return -1;
    }
    
    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Error creating socket for VLBI communication: %s\n", strerror(errno));
        return -1;
    }
    
    // Set socket timeout
    timeout.tv_sec = client_config.timeout / 1000;
    timeout.tv_usec = (client_config.timeout % 1000) * 1000;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("Warning: Could not set socket receive timeout for VLBI communication\n");
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("Warning: Could not set socket send timeout for VLBI communication\n");
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client_config.aquila_port);
    
    if (inet_pton(AF_INET, client_config.aquila_ip, &server_addr.sin_addr) <= 0) {
        printf("Error: Invalid VLBI server IP address: %s\n", client_config.aquila_ip);
        close(sockfd);
        return -1;
    }
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error connecting to VLBI daemon at %s:%d: %s\n", 
               client_config.aquila_ip, client_config.aquila_port, strerror(errno));
        close(sockfd);
        return -1;
    }
    
    // Send command
    ssize_t sent_bytes = send(sockfd, command, strlen(command), 0);
    if (sent_bytes < 0) {
        printf("Error sending VLBI command: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    // Receive response
    ssize_t received_bytes = recv(sockfd, response, response_size - 1, 0);
    close(sockfd);
    
    if (received_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout waiting for VLBI daemon response\n");
        } else {
            printf("Error receiving VLBI response: %s\n", strerror(errno));
        }
        return -1;
    }
    
    response[received_bytes] = '\0';
    
    // Remove trailing newline if present
    if (received_bytes > 0 && response[received_bytes - 1] == '\n') {
        response[received_bytes - 1] = '\0';
    }
    
    return 0;
}

/**
 * Initialize the VLBI client with the given configuration
 */
int vlbi_client_init(const vlbi_client_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&client_config, config, sizeof(vlbi_client_config_t));
    client_initialized = true;
    
    return 0;
}

/**
 * Check if VLBI client is enabled
 */
bool vlbi_client_is_enabled(void) {
    return client_initialized && client_config.enabled;
}

/**
 * Check connectivity to VLBI daemon
 */
int vlbi_check_connectivity(void) {
    char response[1024];
    
    printf("Checking VLBI daemon connectivity at %s:%d...\n", 
           client_config.aquila_ip, client_config.aquila_port);
    
    if (send_vlbi_command("ping", response, sizeof(response)) == 0) {
        char* status = extract_json_string(response, "status");
        if (status && strcmp(status, "success") == 0) {
            printf("VLBI daemon is reachable and responding\n");
            free(status);
            return 1;
        } else {
            printf("VLBI daemon responded but with error: %s\n", response);
            if (status) free(status);
            return 0;
        }
    } else {
        printf("Failed to connect to VLBI daemon\n");
        return 0;
    }
}

/**
 * Start VLBI logging
 */
int vlbi_start_logging(void) {
    char response[1024];
    
    printf("Sending start command to VLBI daemon...\n");
    
    if (send_vlbi_command("start_vlbi", response, sizeof(response)) == 0) {
        char* status = extract_json_string(response, "status");
        char* message = extract_json_string(response, "message");
        
        if (status && strcmp(status, "success") == 0) {
            printf("VLBI logging started successfully");
            int pid = extract_json_int(response, "pid");
            if (pid > 0) {
                printf(" (PID: %d)", pid);
            }
            printf("\n");
            if (message) {
                printf("Message: %s\n", message);
            }
            if (status) free(status);
            if (message) free(message);
            return 1;
        } else {
            printf("Failed to start VLBI logging\n");
            if (message) {
                printf("Error: %s\n", message);
            }
            if (status) free(status);
            if (message) free(message);
            return 0;
        }
    } else {
        printf("Failed to communicate with VLBI daemon\n");
        return 0;
    }
}

/**
 * Stop VLBI logging
 */
int vlbi_stop_logging(void) {
    char response[1024];
    
    printf("Sending stop command to VLBI daemon...\n");
    
    if (send_vlbi_command("stop_vlbi", response, sizeof(response)) == 0) {
        char* status = extract_json_string(response, "status");
        char* message = extract_json_string(response, "message");
        
        if (status && strcmp(status, "success") == 0) {
            printf("VLBI logging stopped successfully\n");
            if (message) {
                printf("Message: %s\n", message);
            }
            if (status) free(status);
            if (message) free(message);
            return 1;
        } else {
            printf("Failed to stop VLBI logging\n");
            if (message) {
                printf("Error: %s\n", message);
            }
            if (status) free(status);
            if (message) free(message);
            return 0;
        }
    } else {
        printf("Failed to communicate with VLBI daemon\n");
        return 0;
    }
}

/**
 * Get VLBI status
 */
int vlbi_get_status(vlbi_status_t *status) {
    char response[1024];
    
    if (!status) return -1;
    
    // Initialize status structure
    memset(status, 0, sizeof(vlbi_status_t));
    
    if (send_vlbi_command("status", response, sizeof(response)) == 0) {
        char* resp_status = extract_json_string(response, "status");
        
        if (resp_status && strcmp(resp_status, "success") == 0) {
            char* vlbi_status_str = extract_json_string(response, "vlbi_status");
            if (vlbi_status_str) {
                status->is_running = (strcmp(vlbi_status_str, "running") == 0);
                free(vlbi_status_str);
            }
            
            status->pid = extract_json_int(response, "pid");
            
            char* timestamp = extract_json_string(response, "timestamp");
            if (timestamp) {
                strncpy(status->timestamp, timestamp, sizeof(status->timestamp) - 1);
                free(timestamp);
            }
            
            if (resp_status) free(resp_status);
            return 1;
        } else {
            char* message = extract_json_string(response, "message");
            if (message) {
                strncpy(status->last_error, message, sizeof(status->last_error) - 1);
                free(message);
            }
            if (resp_status) free(resp_status);
            return 0;
        }
    } else {
        strncpy(status->last_error, "Failed to communicate with VLBI daemon", 
                sizeof(status->last_error) - 1);
        return 0;
    }
}

/**
 * Cleanup VLBI client resources
 */
void vlbi_client_cleanup(void) {
    client_initialized = false;
    memset(&client_config, 0, sizeof(client_config));
} 