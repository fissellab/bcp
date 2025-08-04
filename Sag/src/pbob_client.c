#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#include "pbob_client.h"
#include "file_io_Sag.h"

// Global configuration
static pbob_client_config_t client_config;
static bool client_initialized = false;

/**
 * Initialize the PBoB client with the given configuration
 * Returns 0 on success, -1 on failure
 */
int pbob_client_init(const pbob_client_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&client_config, config, sizeof(pbob_client_config_t));
    client_initialized = true;
    
    return 0;
}

/**
 * Send command to PBoB server to toggle a specific relay
 * Returns 1 on success (relay toggled), 0 on failure
 */
int pbob_send_command(int pbob_id, int relay_id) {
    if (!client_initialized || !client_config.enabled) {
        printf("PBoB client not initialized or disabled\n");
        return 0;
    }
    
    int sockfd;
    struct sockaddr_in server_addr;
    char command[64];
    char response[64];
    struct timeval timeout;
    
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("Error creating socket for PBoB communication: %s\n", strerror(errno));
        return 0;
    }
    
    // Set socket timeout
    timeout.tv_sec = client_config.timeout / 1000;  // Convert ms to seconds
    timeout.tv_usec = (client_config.timeout % 1000) * 1000;  // Remaining ms to microseconds
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("Warning: Could not set socket timeout for PBoB communication\n");
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client_config.port);
    
    if (inet_pton(AF_INET, client_config.ip, &server_addr.sin_addr) <= 0) {
        printf("Error: Invalid PBoB server IP address: %s\n", client_config.ip);
        close(sockfd);
        return 0;
    }
    
    // Format command: "PBOB_ID;RELAY_ID"
    snprintf(command, sizeof(command), "%d;%d", pbob_id, relay_id);
    
    printf("Sending PBoB command: %s to %s:%d\n", command, client_config.ip, client_config.port);
    
    // Send command
    ssize_t sent_bytes = sendto(sockfd, command, strlen(command), 0,
                               (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (sent_bytes < 0) {
        printf("Error sending PBoB command: %s\n", strerror(errno));
        close(sockfd);
        return 0;
    }
    
    // Receive response
    socklen_t addr_len = sizeof(server_addr);
    ssize_t received_bytes = recvfrom(sockfd, response, sizeof(response) - 1, 0,
                                     (struct sockaddr*)&server_addr, &addr_len);
    
    close(sockfd);
    
    if (received_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout waiting for PBoB server response\n");
        } else {
            printf("Error receiving PBoB response: %s\n", strerror(errno));
        }
        return 0;
    }
    
    response[received_bytes] = '\0';
    
    // Parse response - expecting "1" for success
    int result = atoi(response);
    if (result == 1) {
        printf("PBoB command successful: PBOB %d Relay %d toggled\n", pbob_id, relay_id);
        return 1;
    } else {
        printf("PBoB command failed: received response '%s'\n", response);
        return 0;
    }
}

/**
 * Cleanup PBoB client resources
 */
void pbob_client_cleanup(void) {
    client_initialized = false;
    memset(&client_config, 0, sizeof(client_config));
}

/**
 * Check if PBoB client is enabled
 */
bool pbob_client_is_enabled(void) {
    return client_initialized && client_config.enabled;
} 