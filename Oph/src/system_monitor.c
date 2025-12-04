#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

#include "system_monitor.h"
#include "file_io_Oph.h"

// Global variables
system_monitor_data sys_monitor;
pthread_t system_monitor_thread;
int system_monitor_running = 0;
int stop_system_monitor = 0;
FILE* system_monitor_log = NULL;

// External config
extern struct conf_params config;

// Initialize system monitor
int init_system_monitor() {
    // Initialize mutex
    if (pthread_mutex_init(&sys_monitor.data_mutex, NULL) != 0) {
        return -1;
    }

    // Initialize data structure
    sys_monitor.cpu_temp_celsius = 0.0;
    sys_monitor.cpu_usage_percent = 0.0;
    sys_monitor.memory_used_gb = 0.0;
    sys_monitor.memory_total_gb = 0.0;
    strcpy(sys_monitor.memory_used_str, "N/A");
    strcpy(sys_monitor.memory_total_str, "N/A");
    strcpy(sys_monitor.ssd_status, "N/A");
    strcpy(sys_monitor.ssd_used, "N/A");
    strcpy(sys_monitor.ssd_total, "N/A");
    strcpy(sys_monitor.ssd_mount_path, "N/A");
    sys_monitor.ssd_mounted = 0;

    return 0;
}

// Shutdown system monitor
void shutdown_system_monitor() {
    stop_system_monitor = 1;
    if (system_monitor_running) {
        pthread_join(system_monitor_thread, NULL);
    }
    pthread_mutex_destroy(&sys_monitor.data_mutex);
    if (system_monitor_log) {
        fclose(system_monitor_log);
    }
}

// Get CPU temperature
float get_cpu_temperature() {
    FILE *fp;
    char buffer[64];
    float temp = 0.0;

    // Try /sys/class/thermal/thermal_zone0/temp first
    fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            int temp_millicelsius = atoi(buffer);
            temp = temp_millicelsius / 1000.0;
        }
        fclose(fp);
        return temp;
    }

    // Fallback to sensors command
    fp = popen("sensors 2>/dev/null | grep -E 'Core 0|Package id 0|Tctl' | head -n1 | grep -oE '[0-9]+\\.[0-9]+' | head -n1", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            temp = atof(buffer);
        }
        pclose(fp);
    }

    return temp;
}

// Get CPU usage percentage
float get_cpu_usage() {
    FILE *fp;
    char buffer[256];
    float usage = 0.0;

    fp = popen("top -bn1 | grep 'Cpu(s)' | sed 's/.*, *\\([0-9.]*\\)%* id.*/\\1/' | awk '{print 100 - $1}'", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            usage = atof(buffer);
        }
        pclose(fp);
    }

    return usage;
}

// Get memory usage
void get_memory_usage(float *used_gb, float *total_gb, char *used_str, char *total_str) {
    FILE *fp;
    char buffer[256];

    *used_gb = 0.0;
    *total_gb = 0.0;
    strcpy(used_str, "N/A");
    strcpy(total_str, "N/A");

    // Get memory info in human readable format
    fp = popen("free -h | grep Mem", "r");
    if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            char total[32], used[32];
            if (sscanf(buffer, "%*s %31s %31s", total, used) == 2) {
                strncpy(used_str, used, 31);
                strncpy(total_str, total, 31);
                used_str[31] = '\0';
                total_str[31] = '\0';

                // Convert to GB for numeric values
                float used_val, total_val;
                if (sscanf(used, "%f", &used_val) == 1 && sscanf(total, "%f", &total_val) == 1) {
                    // Simple conversion - assumes Gi units, adjust if needed
                    if (strstr(used, "Gi")) {
                        *used_gb = used_val;
                    } else if (strstr(used, "Mi")) {
                        *used_gb = used_val / 1024.0;
                    }
                    
                    if (strstr(total, "Gi")) {
                        *total_gb = total_val;
                    } else if (strstr(total, "Mi")) {
                        *total_gb = total_val / 1024.0;
                    }
                }
            }
        }
        pclose(fp);
    }
}

// Get external SSD information
void get_ssd_info(char *status, char *used, char *total, char *mount_path, int *mounted) {
    FILE *fp;
    char buffer[512];
    struct stat st;

    *mounted = 0;
    strcpy(status, "T7 drive not mounted");
    strcpy(used, "N/A");
    strcpy(total, "N/A");
    strcpy(mount_path, "N/A");

    // Check for T7 drive in different mount locations
    const char *possible_paths[] = {
        "/media/saggitarius/T7",
        "/media/ophiuchus/T7",
        NULL
    };

    for (int i = 0; possible_paths[i] != NULL; i++) {
        if (stat(possible_paths[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            *mounted = 1;
            strcpy(mount_path, possible_paths[i]);
            
            // Get disk usage for this mount point
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "df -h %s | tail -n1", possible_paths[i]);
            
            fp = popen(cmd, "r");
            if (fp != NULL) {
                if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                    char filesystem[64], size[32], used_space[32], avail[32], percent[16], mountpoint[256];
                    if (sscanf(buffer, "%63s %31s %31s %31s %15s %255s", 
                              filesystem, size, used_space, avail, percent, mountpoint) == 6) {
                        strncpy(total, size, 31);
                        strncpy(used, used_space, 31);
                        total[31] = '\0';
                        used[31] = '\0';
                        
                        snprintf(status, 255, "%s %s %s %s %s %s", 
                                filesystem, size, used_space, avail, percent, mountpoint);
                    }
                }
                pclose(fp);
            }
            break;
        }
    }
}

// System monitor thread function
void* run_system_monitor_thread(void* arg) {
    write_to_log(system_monitor_log, "system_monitor.c", "run_system_monitor_thread", "System monitor thread started");
    system_monitor_running = 1;

    while (!stop_system_monitor) {
        // Get system metrics
        float cpu_temp = get_cpu_temperature();
        float cpu_usage = get_cpu_usage();
        float mem_used_gb, mem_total_gb;
        char mem_used_str[32], mem_total_str[32];
        char ssd_status[256], ssd_used[32], ssd_total[32], ssd_mount[128];
        int ssd_mounted;

        get_memory_usage(&mem_used_gb, &mem_total_gb, mem_used_str, mem_total_str);
        get_ssd_info(ssd_status, ssd_used, ssd_total, ssd_mount, &ssd_mounted);

        // Update global data structure with mutex
        pthread_mutex_lock(&sys_monitor.data_mutex);
        
        sys_monitor.cpu_temp_celsius = cpu_temp;
        sys_monitor.cpu_usage_percent = cpu_usage;
        sys_monitor.memory_used_gb = mem_used_gb;
        sys_monitor.memory_total_gb = mem_total_gb;
        strncpy(sys_monitor.memory_used_str, mem_used_str, sizeof(sys_monitor.memory_used_str) - 1);
        strncpy(sys_monitor.memory_total_str, mem_total_str, sizeof(sys_monitor.memory_total_str) - 1);
        strncpy(sys_monitor.ssd_status, ssd_status, sizeof(sys_monitor.ssd_status) - 1);
        strncpy(sys_monitor.ssd_used, ssd_used, sizeof(sys_monitor.ssd_used) - 1);
        strncpy(sys_monitor.ssd_total, ssd_total, sizeof(sys_monitor.ssd_total) - 1);
        strncpy(sys_monitor.ssd_mount_path, ssd_mount, sizeof(sys_monitor.ssd_mount_path) - 1);
        sys_monitor.ssd_mounted = ssd_mounted;
        
        // Ensure null termination
        sys_monitor.memory_used_str[sizeof(sys_monitor.memory_used_str) - 1] = '\0';
        sys_monitor.memory_total_str[sizeof(sys_monitor.memory_total_str) - 1] = '\0';
        sys_monitor.ssd_status[sizeof(sys_monitor.ssd_status) - 1] = '\0';
        sys_monitor.ssd_used[sizeof(sys_monitor.ssd_used) - 1] = '\0';
        sys_monitor.ssd_total[sizeof(sys_monitor.ssd_total) - 1] = '\0';
        sys_monitor.ssd_mount_path[sizeof(sys_monitor.ssd_mount_path) - 1] = '\0';
        
        pthread_mutex_unlock(&sys_monitor.data_mutex);

        // Log the metrics
        if (system_monitor_log) {
            time_t now = time(NULL);
            fprintf(system_monitor_log, "[%ld] CPU_Temp=%.1f°C CPU_Usage=%.1f%% Memory=%s/%s SSD=%s:%s/%s\n",
                   now, cpu_temp, cpu_usage, mem_used_str, mem_total_str, 
                   ssd_mounted ? "mounted" : "unmounted", ssd_used, ssd_total);
            fflush(system_monitor_log);
        }

        // Sleep for configured interval
        int sleep_time = config.system_monitor.enabled ? config.system_monitor.update_interval_sec : 30;
        sleep(sleep_time);
    }

    system_monitor_running = 0;
    write_to_log(system_monitor_log, "system_monitor.c", "run_system_monitor_thread", "System monitor thread stopped");
    return NULL;
}