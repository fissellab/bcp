#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <stdio.h>
#include <pthread.h>
#include "file_io_Oph.h"

// System monitor data structure
typedef struct system_monitor_data {
    float cpu_temp_celsius;
    float cpu_usage_percent;
    float memory_used_gb;
    float memory_total_gb;
    char memory_used_str[32];  // Used memory with units (e.g., "2.1Gi")
    char memory_total_str[32]; // Total memory with units (e.g., "8.0Gi")
    char ssd_status[256];      // SSD mount status and usage
    char ssd_used[32];         // Used space on SSD
    char ssd_total[32];        // Total space on SSD
    char ssd_mount_path[128];  // Mount path of SSD
    int ssd_mounted;           // 1 if mounted, 0 if not
    pthread_mutex_t data_mutex; // Mutex to protect the data
} system_monitor_data;

// Function declarations
void* run_system_monitor_thread(void* arg);
int init_system_monitor();
void shutdown_system_monitor();
float get_cpu_temperature();
float get_cpu_usage();
void get_memory_usage(float *used_gb, float *total_gb, char *used_str, char *total_str);
void get_ssd_info(char *status, char *used, char *total, char *mount_path, int *mounted);

// Global variables
extern system_monitor_data sys_monitor;
extern pthread_t system_monitor_thread;
extern int system_monitor_running;
extern int stop_system_monitor;
extern FILE* system_monitor_log;

#endif