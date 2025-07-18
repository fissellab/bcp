#include "pr59_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

// Shared memory variables
static pr59_data_t *shared_pr59_data = NULL;
static int shm_fd = -1;
static const char *PR59_SHM_NAME = "/bcp_pr59_data";

// Initialize the PR59 interface with shared memory
int pr59_interface_init(void) {
    // Try to create or open shared memory
    shm_fd = shm_open(PR59_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        fprintf(stderr, "PR59 Interface: Failed to create shared memory: %s\n", strerror(errno));
        return -1;
    }
    
    // Set the size of the shared memory
    if (ftruncate(shm_fd, sizeof(pr59_data_t)) == -1) {
        fprintf(stderr, "PR59 Interface: Failed to set shared memory size: %s\n", strerror(errno));
        close(shm_fd);
        shm_unlink(PR59_SHM_NAME);
        return -1;
    }
    
    // Map the shared memory
    shared_pr59_data = mmap(NULL, sizeof(pr59_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_pr59_data == MAP_FAILED) {
        fprintf(stderr, "PR59 Interface: Failed to map shared memory: %s\n", strerror(errno));
        close(shm_fd);
        shm_unlink(PR59_SHM_NAME);
        return -1;
    }
    
    // Initialize the mutex
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_pr59_data->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    
    // Initialize default values
    pthread_mutex_lock(&shared_pr59_data->mutex);
    memset(shared_pr59_data, 0, sizeof(pr59_data_t));
    shared_pr59_data->is_running = false;
    shared_pr59_data->timestamp = time(NULL);
    shared_pr59_data->last_update = 0;
    pthread_mutex_unlock(&shared_pr59_data->mutex);
    
    return 0;
}

// Update PR59 data (called by TEC controller)
void pr59_update_data(float temp, float fet_temp, float current, float voltage, 
                      float kp, float ki, float kd, float setpoint) {
    if (shared_pr59_data == NULL) {
        return; // Interface not initialized
    }
    
    pthread_mutex_lock(&shared_pr59_data->mutex);
    
    // Update configuration parameters
    shared_pr59_data->kp = kp;
    shared_pr59_data->ki = ki;
    shared_pr59_data->kd = kd;
    shared_pr59_data->setpoint_temp = setpoint;
    
    // Update real-time telemetry
    shared_pr59_data->temperature = temp;
    shared_pr59_data->fet_temperature = fet_temp;
    shared_pr59_data->current = current;
    shared_pr59_data->voltage = voltage;
    shared_pr59_data->power = current * voltage;
    
    // Update status information
    shared_pr59_data->is_running = true;
    
    // Determine heating/cooling status based on temperature vs setpoint
    // Using a small deadband (0.1°C) for status determination
    const float status_deadband = 0.1f;
    if (temp < (setpoint - status_deadband)) {
        shared_pr59_data->is_heating = true;
        shared_pr59_data->is_at_setpoint = false;
    } else if (temp > (setpoint + status_deadband)) {
        shared_pr59_data->is_heating = false;
        shared_pr59_data->is_at_setpoint = false;
    } else {
        shared_pr59_data->is_at_setpoint = true;
    }
    
    // Update timestamps
    shared_pr59_data->timestamp = time(NULL);
    shared_pr59_data->last_update = shared_pr59_data->timestamp;
    
    pthread_mutex_unlock(&shared_pr59_data->mutex);
}

// Get current PR59 data (thread-safe)
bool pr59_get_data(pr59_data_t *data) {
    if (shared_pr59_data == NULL || data == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&shared_pr59_data->mutex);
    
    // Check if data is recent (within last 10 seconds)
    time_t now = time(NULL);
    if (shared_pr59_data->last_update == 0 || (now - shared_pr59_data->last_update) > 10) {
        // Mark as not running if data is stale
        shared_pr59_data->is_running = false;
    }
    
    // Copy all data except the mutex
    data->kp = shared_pr59_data->kp;
    data->ki = shared_pr59_data->ki;
    data->kd = shared_pr59_data->kd;
    data->setpoint_temp = shared_pr59_data->setpoint_temp;
    data->timestamp = shared_pr59_data->timestamp;
    data->temperature = shared_pr59_data->temperature;
    data->fet_temperature = shared_pr59_data->fet_temperature;
    data->current = shared_pr59_data->current;
    data->voltage = shared_pr59_data->voltage;
    data->power = shared_pr59_data->power;
    data->is_running = shared_pr59_data->is_running;
    data->is_heating = shared_pr59_data->is_heating;
    data->is_at_setpoint = shared_pr59_data->is_at_setpoint;
    data->last_update = shared_pr59_data->last_update;
    
    bool is_running = shared_pr59_data->is_running;
    
    pthread_mutex_unlock(&shared_pr59_data->mutex);
    
    return is_running;
}

// Check if PR59 is running
bool pr59_is_running(void) {
    if (shared_pr59_data == NULL) {
        return false;
    }
    
    pthread_mutex_lock(&shared_pr59_data->mutex);
    
    // Check if data is recent
    time_t now = time(NULL);
    bool running = shared_pr59_data->is_running && 
                   shared_pr59_data->last_update != 0 && 
                   (now - shared_pr59_data->last_update) <= 10;
    
    if (!running) {
        shared_pr59_data->is_running = false;
    }
    
    pthread_mutex_unlock(&shared_pr59_data->mutex);
    
    return running;
}

// Cleanup PR59 interface
void pr59_interface_cleanup(void) {
    if (shared_pr59_data != NULL) {
        // Mark as not running
        pthread_mutex_lock(&shared_pr59_data->mutex);
        shared_pr59_data->is_running = false;
        pthread_mutex_unlock(&shared_pr59_data->mutex);
        
        // Cleanup mutex
        pthread_mutex_destroy(&shared_pr59_data->mutex);
        
        // Unmap shared memory
        munmap(shared_pr59_data, sizeof(pr59_data_t));
        shared_pr59_data = NULL;
    }
    
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_unlink(PR59_SHM_NAME);
        shm_fd = -1;
    }
} 