#ifndef PBOB_CLIENT_H
#define PBOB_CLIENT_H

#include <stdbool.h>

// PBoB client configuration structure
typedef struct {
    bool enabled;
    char ip[16];
    int port;
    int timeout;
} pbob_client_config_t;

// Function prototypes
int pbob_client_init(const pbob_client_config_t *config);
int pbob_send_command(int pbob_id, int relay_id);
void pbob_client_cleanup(void);
bool pbob_client_is_enabled(void);

#endif // PBOB_CLIENT_H 