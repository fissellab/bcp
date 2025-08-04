#ifndef TELEMETRY_SERVER_H
#define TELEMETRY_SERVER_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TELEMETRY_BUFFER_SIZE 1024
#define MAX_UDP_CLIENTS 10

// Telemetry server configuration structure
typedef struct {
    int enabled;
    char ip[16];
    int port;
    int timeout;
    int udp_buffer_size;
    char udp_client_ips[MAX_UDP_CLIENTS][16];  // Array of authorized client IPs
    int udp_client_count;                      // Number of authorized clients
} telemetry_server_config_t;

// Global variables (extern declarations)
extern struct sockaddr_in tel_client_addr;
extern int tel_server_running;
extern int stop_telemetry_server;
extern FILE* telemetry_server_log;

// Function prototypes

// Server management functions
int telemetry_server_init(const telemetry_server_config_t *config);
void* telemetry_server_thread(void* arg);
int telemetry_init_socket(void);
void telemetry_sock_listen(int sockfd, char* buffer);
void telemetry_send_metric(int sockfd, char* id);

// Helper functions for sending different data types
void telemetry_sendString(int sockfd, const char* string_sample);
void telemetry_sendInt(int sockfd, int sample);
void telemetry_sendFloat(int sockfd, float sample);
void telemetry_sendDouble(int sockfd, double sample);

// Server control functions
bool telemetry_server_start(void);
void telemetry_server_stop(void);
bool telemetry_server_is_running(void);
void telemetry_server_cleanup(void);

#endif // TELEMETRY_SERVER_H 