#ifndef VLBI_CLIENT_H
#define VLBI_CLIENT_H

#include <stdbool.h>

// VLBI client configuration structure
typedef struct {
    bool enabled;
    char aquila_ip[16];
    int aquila_port;
    int timeout;
    int ping_timeout;
    int status_check_interval;
} vlbi_client_config_t;

// VLBI detailed status structure
typedef struct {
    bool is_running;
    int pid;
    char timestamp[32];
    char last_error[256];
    
    // Detailed status from daemon
    char stage[32];              // stopped, connecting, programming, capturing, error
    int packets_captured;
    double data_size_mb;
    char connection_status[32];  // disconnected, connecting, connected, capturing
    int error_count;
    char last_update[32];
} vlbi_status_t;

// VLBI log entry structure
typedef struct {
    char timestamp[32];
    char message[512];
} vlbi_log_entry_t;

// Global VLBI status for telemetry access
extern vlbi_status_t global_vlbi_status;
extern bool vlbi_status_valid;

// Function prototypes  
int vlbi_client_init(const vlbi_client_config_t *config);
bool vlbi_client_is_enabled(void);
int vlbi_check_connectivity(void);
int vlbi_start_logging(void);
int vlbi_start_logging_with_ssd(int ssd_id);  // New: start VLBI with SSD parameter
int vlbi_stop_logging(void);
int vlbi_get_status(vlbi_status_t *status);
int vlbi_start_auto_streaming(void);  // New: automatic background streaming
void vlbi_client_cleanup(void);

#endif // VLBI_CLIENT_H 