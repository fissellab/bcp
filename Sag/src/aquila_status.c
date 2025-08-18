/**
 * Aquila System Status Implementation
 * 
 * This module handles parsing and storage of system status data
 * received from the aquila backend computer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include "aquila_status.h"
#include "file_io_Sag.h"

// Global aquila status
aquila_status_t global_aquila_status = {0};

// External log file (from telemetry_server.c)
extern FILE* telemetry_server_log;

/**
 * Initialize aquila status module
 */
int aquila_status_init(void) {
    // Initialize mutex
    if (pthread_mutex_init(&global_aquila_status.data_mutex, NULL) != 0) {
        return -1;
    }
    
    // Initialize data structure
    memset(&global_aquila_status, 0, sizeof(aquila_status_t));
    global_aquila_status.data_valid = 0;
    global_aquila_status.last_update = 0;
    
    return 0;
}

/**
 * Cleanup aquila status module
 */
void aquila_status_cleanup(void) {
    pthread_mutex_destroy(&global_aquila_status.data_mutex);
}

/**
 * Simple JSON value extractor
 * Extracts numeric value for a given key from JSON string
 */
static int extract_json_number(const char* json, const char* key, float* value) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* pos = strstr(json, search_key);
    if (!pos) {
        return 0; // Key not found
    }
    
    pos += strlen(search_key);
    
    // Skip whitespace
    while (*pos == ' ' || *pos == '\t') pos++;
    
    // Parse number
    if (sscanf(pos, "%f", value) == 1) {
        return 1; // Success
    }
    
    return 0; // Parse failed
}

/**
 * Simple JSON integer extractor
 */
static int extract_json_int(const char* json, const char* key, int* value) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    char* pos = strstr(json, search_key);
    if (!pos) {
        return 0; // Key not found
    }
    
    pos += strlen(search_key);
    
    // Skip whitespace
    while (*pos == ' ' || *pos == '\t') pos++;
    
    // Parse integer
    if (sscanf(pos, "%d", value) == 1) {
        return 1; // Success
    }
    
    return 0; // Parse failed
}

/**
 * Update aquila status from JSON data
 * Parses JSON message from aquila system monitor and updates global status
 */
int aquila_status_update_from_json(const char* json_data) {
    if (!json_data || strlen(json_data) < 10) {
        return -1;
    }
    
    // Check if this is an aquila system status message
    if (strstr(json_data, "aquila_system_status") == NULL) {
        return -1; // Not an aquila status message
    }
    
    // Log reception
    if (telemetry_server_log) {
        write_to_log(telemetry_server_log, "aquila_status.c", "aquila_status_update_from_json", "Received aquila status update");
    }
    
    // Lock mutex for update
    pthread_mutex_lock(&global_aquila_status.data_mutex);
    
    // Extract SSD1 data
    if (!extract_json_int(json_data, "mounted", &global_aquila_status.ssd1_mounted)) {
        global_aquila_status.ssd1_mounted = 0;
    }
    extract_json_number(json_data, "used_gb", &global_aquila_status.ssd1_used_gb);
    extract_json_number(json_data, "total_gb", &global_aquila_status.ssd1_total_gb);
    extract_json_number(json_data, "percent_used", &global_aquila_status.ssd1_percent_used);
    
    // Extract SSD2 data (look for second occurrence in ssd2 section)
    char* ssd2_section = strstr(json_data, "\"ssd2\":");
    if (ssd2_section) {
        extract_json_int(ssd2_section, "mounted", &global_aquila_status.ssd2_mounted);
        extract_json_number(ssd2_section, "used_gb", &global_aquila_status.ssd2_used_gb);
        extract_json_number(ssd2_section, "total_gb", &global_aquila_status.ssd2_total_gb);
        extract_json_number(ssd2_section, "percent_used", &global_aquila_status.ssd2_percent_used);
    }
    
    // Extract system data
    char* system_section = strstr(json_data, "\"system\":");
    if (system_section) {
        extract_json_number(system_section, "cpu_temp_celsius", &global_aquila_status.cpu_temp_celsius);
        extract_json_number(system_section, "memory_used_gb", &global_aquila_status.memory_used_gb);
        extract_json_number(system_section, "memory_total_gb", &global_aquila_status.memory_total_gb);
        extract_json_number(system_section, "memory_percent_used", &global_aquila_status.memory_percent_used);
    }
    
    // Update metadata
    global_aquila_status.last_update = time(NULL);
    global_aquila_status.data_valid = 1;
    
    pthread_mutex_unlock(&global_aquila_status.data_mutex);
    
    // Log successful update
    if (telemetry_server_log) {
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), 
                "Aquila status updated: SSD1=%.1f%%, SSD2=%.1f%%, CPU=%.1f°C",
                global_aquila_status.ssd1_percent_used,
                global_aquila_status.ssd2_percent_used,
                global_aquila_status.cpu_temp_celsius);
        write_to_log(telemetry_server_log, "aquila_status.c", "aquila_status_update_from_json", log_msg);
    }
    
    return 0; // Success
}

/**
 * Get copy of current aquila status data
 * Thread-safe copy of the current status
 */
int aquila_status_get_data(aquila_status_t* status_copy) {
    if (!status_copy) {
        return -1;
    }
    
    pthread_mutex_lock(&global_aquila_status.data_mutex);
    
    // Check if data is valid and not too old (older than 60 seconds)
    time_t now = time(NULL);
    if (!global_aquila_status.data_valid || 
        (now - global_aquila_status.last_update) > 60) {
        pthread_mutex_unlock(&global_aquila_status.data_mutex);
        return -1; // No valid data
    }
    
    // Copy data
    *status_copy = global_aquila_status;
    
    pthread_mutex_unlock(&global_aquila_status.data_mutex);
    
    return 0; // Success
}
