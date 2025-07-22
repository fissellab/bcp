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

// VLBI status structure
typedef struct {
    bool is_running;
    int pid;
    char timestamp[32];
    char last_error[256];
} vlbi_status_t;

// Function prototypes
int vlbi_client_init(const vlbi_client_config_t *config);
bool vlbi_client_is_enabled(void);
int vlbi_check_connectivity(void);
int vlbi_start_logging(void);
int vlbi_stop_logging(void);
int vlbi_get_status(vlbi_status_t *status);
void vlbi_client_cleanup(void);

#endif // VLBI_CLIENT_H 