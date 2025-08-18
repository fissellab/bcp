/**
 * Aquila System Status Storage
 * 
 * This header defines structures and functions for storing and accessing
 * system status information received from the aquila backend computer.
 */

#ifndef AQUILA_STATUS_H
#define AQUILA_STATUS_H

#include <pthread.h>
#include <time.h>

// Aquila system status structure
typedef struct {
    // Metadata
    time_t last_update;
    int data_valid;
    
    // SSD 1 status
    int ssd1_mounted;
    float ssd1_used_gb;
    float ssd1_total_gb;
    float ssd1_percent_used;
    
    // SSD 2 status  
    int ssd2_mounted;
    float ssd2_used_gb;
    float ssd2_total_gb;
    float ssd2_percent_used;
    
    // System status
    float cpu_temp_celsius;
    float memory_used_gb;
    float memory_total_gb;
    float memory_percent_used;
    
    // Synchronization
    pthread_mutex_t data_mutex;
} aquila_status_t;

// Global aquila status
extern aquila_status_t global_aquila_status;

// Function declarations
int aquila_status_init(void);
void aquila_status_cleanup(void);
int aquila_status_update_from_json(const char* json_data);
int aquila_status_get_data(aquila_status_t* status_copy);

#endif // AQUILA_STATUS_H
