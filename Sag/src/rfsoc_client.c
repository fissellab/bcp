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
#include <ctype.h>

#include "rfsoc_client.h"
#include "file_io_Sag.h"

// Global configuration
static rfsoc_client_config_t client_config;
static bool client_initialized = false;

/**
 * Simple JSON value extraction - allocates memory for result
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

static bool extract_json_bool(const char* json, const char* key) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* pos = strstr(json, search_key);
    if (!pos) return false;
    
    pos += strlen(search_key);
    while (*pos && isspace(*pos)) pos++;
    
    return (strncmp(pos, "true", 4) == 0);
}

/**
 * Send command to RFSoC daemon and receive response
 */
static int send_rfsoc_command(const char* command, char* response, size_t response_size) {
    int sockfd;
    struct sockaddr_in server_addr;
    struct timeval timeout;
    
    if (!client_initialized || !client_config.enabled) {
        printf("RFSoC client not initialized or disabled\n");
        return -1;
    }
    
    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Error creating socket for RFSoC communication: %s\n", strerror(errno));
        return -1;
    }
    
    // Set socket timeout
    timeout.tv_sec = client_config.timeout / 1000;
    timeout.tv_usec = (client_config.timeout % 1000) * 1000;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("Warning: Could not set socket receive timeout for RFSoC communication\n");
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("Warning: Could not set socket send timeout for RFSoC communication\n");
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client_config.rfsoc_port);
    
    if (inet_pton(AF_INET, client_config.rfsoc_ip, &server_addr.sin_addr) <= 0) {
        printf("Error: Invalid RFSoC IP address: %s\n", client_config.rfsoc_ip);
        close(sockfd);
        return -1;
    }
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error connecting to RFSoC daemon at %s:%d: %s\n", 
               client_config.rfsoc_ip, client_config.rfsoc_port, strerror(errno));
        close(sockfd);
        return -1;
    }
    
    // Send command
    ssize_t sent_bytes = send(sockfd, command, strlen(command), 0);
    if (sent_bytes < 0) {
        printf("Error sending RFSoC command: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    // Receive response
    ssize_t received_bytes = recv(sockfd, response, response_size - 1, 0);
    close(sockfd);
    
    if (received_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout waiting for RFSoC daemon response\n");
        } else {
            printf("Error receiving RFSoC response: %s\n", strerror(errno));
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
 * Initialize the RFSoC client with the given configuration
 */
int rfsoc_client_init(const rfsoc_client_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&client_config, config, sizeof(rfsoc_client_config_t));
    client_initialized = true;
    
    return 0;
}

/**
 * Check if RFSoC client is enabled
 */
bool rfsoc_client_is_enabled(void) {
    return client_initialized && client_config.enabled;
}

/**
 * Check connectivity to RFSoC daemon
 */
int rfsoc_check_connectivity(void) {
    char response[1024];
    
    printf("Checking RFSoC daemon connectivity at %s:%d...\n", 
           client_config.rfsoc_ip, client_config.rfsoc_port);
    
    if (send_rfsoc_command("ping", response, sizeof(response)) == 0) {
        char* status = extract_json_string(response, "status");
        if (status && strcmp(status, "success") == 0) {
            printf("RFSoC daemon is reachable and responding\n");
            free(status);
            return 1;
        } else {
            printf("RFSoC daemon responded but with error: %s\n", response);
            if (status) free(status);
            return 0;
        }
    } else {
        printf("Failed to connect to RFSoC daemon\n");
        return 0;
    }
}

/**
 * Configure RFSoC clock (execute clock_setup.sh)
 */
int rfsoc_configure_clock(void) {
    char response[2048];  // Larger buffer for script output
    
    printf("Sending clock configuration command to RFSoC...\n");
    
    if (send_rfsoc_command("configure_clock", response, sizeof(response)) == 0) {
        char* status = extract_json_string(response, "status");
        char* message = extract_json_string(response, "message");
        char* output = extract_json_string(response, "output");
        
        if (status && strcmp(status, "success") == 0) {
            printf("RFSoC clock configuration completed successfully!\n");
            if (message) {
                printf("Message: %s\n", message);
            }
            if (output && strlen(output) > 0) {
                printf("Script output:\n%s\n", output);
            }
            
            // Cleanup
            if (status) free(status);
            if (message) free(message);
            if (output) free(output);
            return 1;
        } else {
            printf("Failed to configure RFSoC clock\n");
            if (message) {
                printf("Error: %s\n", message);
            }
            if (output && strlen(output) > 0) {
                printf("Script output:\n%s\n", output);
            }
            
            // Cleanup
            if (status) free(status);
            if (message) free(message);
            if (output) free(output);
            return 0;
        }
    } else {
        printf("Failed to communicate with RFSoC daemon\n");
        return 0;
    }
}

/**
 * Get RFSoC clock status
 */
int rfsoc_get_clock_status(rfsoc_clock_status_t *status) {
    char response[1024];
    
    if (!status) return -1;
    
    // Initialize status structure
    memset(status, 0, sizeof(rfsoc_clock_status_t));
    
    if (send_rfsoc_command("clock_status", response, sizeof(response)) == 0) {
        char* resp_status = extract_json_string(response, "status");
        
        if (resp_status && strcmp(resp_status, "success") == 0) {
            // Extract status information
            status->script_available = extract_json_bool(response, "script_available");
            status->script_executable = extract_json_bool(response, "script_executable");
            
            char* script_path = extract_json_string(response, "script_path");
            if (script_path) {
                strncpy(status->script_path, script_path, sizeof(status->script_path) - 1);
                free(script_path);
            }
            
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
        strncpy(status->last_error, "Failed to communicate with RFSoC daemon", 
                sizeof(status->last_error) - 1);
        return 0;
    }
}

/**
 * Cleanup RFSoC client resources
 */
void rfsoc_client_cleanup(void) {
    client_initialized = false;
    memset(&client_config, 0, sizeof(client_config));
} 