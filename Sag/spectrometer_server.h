#ifndef SPECTROMETER_SERVER_H
#define SPECTROMETER_SERVER_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>

#define SPEC_BUFFER_SIZE 32768
#define MAX_UDP_CLIENTS 10
#define MAX_ZOOM_BINS 200

// Spectrometer types
typedef enum {
    SPEC_TYPE_NONE = 0,
    SPEC_TYPE_STANDARD = 1,    // 2048 points
    SPEC_TYPE_120KHZ = 2       // 16384 points (filtered to ~167)
} spec_type_t;

// Configuration structure for the spectrometer server
typedef struct {
    int udp_server_enabled;
    int udp_server_port;
    char udp_client_ips[MAX_UDP_CLIENTS][16];
    int udp_client_count;
    int udp_buffer_size;
    int max_request_rate;
    
    // High-res filtering parameters for water maser
    double water_maser_freq;      // 22.235 GHz
    double zoom_window_width;     // 0.010 GHz (±10 MHz)
    double if_lower;              // 20.96608 GHz
    double if_upper;              // 22.93216 GHz
} spec_server_config_t;

// Spectrum data structure for standard spectrometer
typedef struct {
    double timestamp;
    int num_points;               // 2048
    double data[2048];
} spectrum_standard_t;

// Spectrum data structure for high-resolution spectrometer (filtered)
typedef struct {
    double timestamp;
    int num_points;               // ~167 bins
    double freq_start;            // Start frequency of zoom window
    double freq_end;              // End frequency of zoom window
    double baseline;              // Baseline level for reference
    double data[MAX_ZOOM_BINS];   // Filtered spectrum data
} spectrum_120khz_t;

// Main spectrum data container
typedef struct {
    volatile sig_atomic_t ready;       // Data ready flag
    volatile spec_type_t active_type;  // Which spectrometer is running
    volatile double last_update;       // Timestamp of last update
    pthread_mutex_t mutex;             // Thread safety
    
    union {
        spectrum_standard_t standard;
        spectrum_120khz_t high_res;
    };
} spectrum_data_t;

// Shared memory structure for inter-process communication
typedef struct {
    volatile sig_atomic_t ready;       // Data ready flag
    volatile spec_type_t active_type;  // Which spectrometer is running
    volatile double timestamp;
    volatile int data_size;            // Actual data size in bytes
    volatile double data[16384];       // Max size buffer (for raw data transfer)
} shared_spectrum_t;

// Public API functions

// Initialize the spectrometer server
int spec_server_init(const spec_server_config_t *config);

// Start the UDP server
bool spec_server_start(void);

// Stop the UDP server
void spec_server_stop(void);

// Check if server is running
bool spec_server_is_running(void);

// Set the active spectrometer type (called by CLI)
void spec_server_set_active_type(spec_type_t type);

// Get the current active spectrometer type
spec_type_t spec_server_get_active_type(void);

// Update spectrum data (called by Python script via shared memory)
int spec_server_update_data(const void *data, size_t data_size, spec_type_t type);

// Get current spectrum data (thread-safe)
bool spec_server_get_spectrum_data(spectrum_data_t *data);

// Get shared memory name for Python scripts
const char* spec_server_get_shared_memory_name(void);

#endif // SPECTROMETER_SERVER_H 