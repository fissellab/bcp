#ifndef TICC_CLIENT_H
#define TICC_CLIENT_H

#include <stdbool.h>
#include <stdio.h>

// TICC client configuration structure
typedef struct {
    bool enabled;
    char port[256];
    int baud_rate;
    char data_save_path[256];
    int file_rotation_interval;
    int pbob_id;
    int relay_id;
} ticc_client_config_t;

// TICC status structure
typedef struct {
    bool is_logging;
    bool is_configured;
    char current_file[512];
    char last_error[256];
    double last_measurement;
    time_t start_time;
    int measurement_count;
} ticc_status_t;

// Function prototypes
int ticc_client_init(const ticc_client_config_t *config);
bool ticc_client_is_enabled(void);
int ticc_start_logging(void);
int ticc_stop_logging(void);
int ticc_get_status(ticc_status_t *status);
void ticc_client_cleanup(void);

// Internal functions for TICC communication
int ticc_configure_device(void);
int ticc_create_data_file(void);
int ticc_read_measurements(void);

#endif // TICC_CLIENT_H 