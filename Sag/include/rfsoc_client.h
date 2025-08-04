#ifndef RFSOC_CLIENT_H
#define RFSOC_CLIENT_H

#include <stdbool.h>

// RFSoC client configuration structure
typedef struct {
    bool enabled;
    char rfsoc_ip[16];
    int rfsoc_port;
    int timeout;
} rfsoc_client_config_t;

// RFSoC clock status structure
typedef struct {
    bool script_available;
    bool script_executable;
    char script_path[256];
    char timestamp[32];
    char last_error[256];
    char output[1024];
} rfsoc_clock_status_t;

// Function prototypes
int rfsoc_client_init(const rfsoc_client_config_t *config);
bool rfsoc_client_is_enabled(void);
int rfsoc_check_connectivity(void);
int rfsoc_configure_clock(void);
int rfsoc_get_clock_status(rfsoc_clock_status_t *status);
void rfsoc_client_cleanup(void);

#endif // RFSOC_CLIENT_H 