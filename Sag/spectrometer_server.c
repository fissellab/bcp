#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#include "spectrometer_server.h"
#include "file_io_Sag.h"

// Global variables
static spec_server_config_t current_config;
static spectrum_data_t current_spectrum_data;
static bool server_running = false;
static pthread_t udp_server_thread;
static int udp_server_socket = -1;
static FILE *spec_udp_log_file = NULL;

// Shared memory variables
static shared_spectrum_t *shared_memory = NULL;
static int shm_fd = -1;
static const char *SHM_NAME = "/bcp_spectrometer_data";

// Rate limiting structure
typedef struct {
    char client_ip[INET_ADDRSTRLEN];
    time_t last_request_time;
} client_rate_info_t;

static client_rate_info_t client_rates[MAX_UDP_CLIENTS];
static int active_clients = 0;

// Forward declarations
static void *udp_server_thread_func(void *arg);
static void format_standard_response(char *buffer, size_t buffer_size);
static void format_120khz_response(char *buffer, size_t buffer_size);
static void log_spec_message(const char *message);
static bool is_authorized_client(const char *client_ip);
static bool check_rate_limit(const char *client_ip);
static void process_120khz_spectrum(const double *raw_data, int raw_size);
static int calculate_zoom_bins(void);

// Initialize shared memory
static int init_shared_memory(void) {
    // Create shared memory object
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        log_spec_message("Error creating shared memory object");
        return -1;
    }
    
    // Set size of shared memory
    if (ftruncate(shm_fd, sizeof(shared_spectrum_t)) == -1) {
        log_spec_message("Error setting shared memory size");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }
    
    // Map shared memory
    shared_memory = mmap(NULL, sizeof(shared_spectrum_t), 
                        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        log_spec_message("Error mapping shared memory");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }
    
    // Initialize shared memory
    memset(shared_memory, 0, sizeof(shared_spectrum_t));
    shared_memory->active_type = SPEC_TYPE_NONE;
    shared_memory->ready = 0;
    
    log_spec_message("Shared memory initialized successfully");
    return 0;
}

// Cleanup shared memory
static void cleanup_shared_memory(void) {
    if (shared_memory != NULL && shared_memory != MAP_FAILED) {
        munmap(shared_memory, sizeof(shared_spectrum_t));
        shared_memory = NULL;
    }
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
        shm_fd = -1;
    }
    log_spec_message("Shared memory cleaned up");
}

// Calculate zoom bins for 120kHz spectrometer
static int calculate_zoom_bins(void) {
    double freq_range = current_config.if_upper - current_config.if_lower;
    double bin_width = freq_range / 16384.0;  // 16384 FFT points for 120kHz version
    int center_bin = (int)((current_config.water_maser_freq - current_config.if_lower) / bin_width);
    int zoom_bins = (int)(current_config.zoom_window_width / bin_width);
    int zoom_start = center_bin - zoom_bins > 0 ? center_bin - zoom_bins : 0;
    int zoom_end = center_bin + zoom_bins < 16383 ? center_bin + zoom_bins : 16383;
    return zoom_end - zoom_start + 1;
}

// Process 120kHz spectrum data (apply filtering like read_latest_data_120khz.py)
static void process_120khz_spectrum(const double *raw_data, int raw_size) {
    if (raw_size != 16384) {
        log_spec_message("Invalid 120kHz spectrum size");
        return;
    }
    
    // Calculate zoom parameters
    double freq_range = current_config.if_upper - current_config.if_lower;
    double bin_width = freq_range / 16384.0;
    int center_bin = (int)((current_config.water_maser_freq - current_config.if_lower) / bin_width);
    int zoom_bins = (int)(current_config.zoom_window_width / bin_width);
    int zoom_start = center_bin - zoom_bins > 0 ? center_bin - zoom_bins : 0;
    int zoom_end = center_bin + zoom_bins < 16383 ? center_bin + zoom_bins : 16383;
    int zoom_width = zoom_end - zoom_start + 1;
    
    if (zoom_width > MAX_ZOOM_BINS) {
        zoom_width = MAX_ZOOM_BINS;
        zoom_end = zoom_start + zoom_width - 1;
    }
    
    pthread_mutex_lock(&current_spectrum_data.mutex);
    
    // Convert to dB and apply transformations (same as read_latest_data_120khz.py)
    double *spectrum_db = malloc(16384 * sizeof(double));
    if (!spectrum_db) {
        pthread_mutex_unlock(&current_spectrum_data.mutex);
        return;
    }
    
    // Convert to dB
    for (int i = 0; i < 16384; i++) {
        spectrum_db[i] = 10.0 * log10(raw_data[i] + 1e-10);
    }
    
    // Apply flip and fftshift (mimic numpy operations)
    double *flipped = malloc(16384 * sizeof(double));
    if (!flipped) {
        free(spectrum_db);
        pthread_mutex_unlock(&current_spectrum_data.mutex);
        return;
    }
    
    // Flip the spectrum
    for (int i = 0; i < 16384; i++) {
        flipped[i] = spectrum_db[16383 - i];
    }
    
    // FFT shift
    double *shifted = malloc(16384 * sizeof(double));
    if (!shifted) {
        free(spectrum_db);
        free(flipped);
        pthread_mutex_unlock(&current_spectrum_data.mutex);
        return;
    }
    
    int half = 16384 / 2;
    for (int i = 0; i < half; i++) {
        shifted[i] = flipped[i + half];
        shifted[i + half] = flipped[i];
    }
    
    // Extract zoom window
    double *zoomed = malloc(zoom_width * sizeof(double));
    if (!zoomed) {
        free(spectrum_db);
        free(flipped);
        free(shifted);
        pthread_mutex_unlock(&current_spectrum_data.mutex);
        return;
    }
    
    for (int i = 0; i < zoom_width; i++) {
        zoomed[i] = shifted[zoom_start + i];
    }
    
    // Calculate baseline (median)
    double *sorted = malloc(zoom_width * sizeof(double));
    if (!sorted) {
        free(spectrum_db);
        free(flipped);
        free(shifted);
        free(zoomed);
        pthread_mutex_unlock(&current_spectrum_data.mutex);
        return;
    }
    
    memcpy(sorted, zoomed, zoom_width * sizeof(double));
    
    // Simple bubble sort for median calculation
    for (int i = 0; i < zoom_width - 1; i++) {
        for (int j = 0; j < zoom_width - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                double temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    double baseline = sorted[zoom_width / 2];  // Median
    
    // Update spectrum data
    current_spectrum_data.active_type = SPEC_TYPE_120KHZ;
    current_spectrum_data.high_res.timestamp = shared_memory->timestamp;
    current_spectrum_data.high_res.num_points = zoom_width;
    current_spectrum_data.high_res.freq_start = current_config.water_maser_freq - current_config.zoom_window_width;
    current_spectrum_data.high_res.freq_end = current_config.water_maser_freq + current_config.zoom_window_width;
    current_spectrum_data.high_res.baseline = baseline;
    
    // Apply baseline subtraction and store
    for (int i = 0; i < zoom_width; i++) {
        current_spectrum_data.high_res.data[i] = zoomed[i] - baseline;
    }
    
    current_spectrum_data.ready = 1;
    current_spectrum_data.last_update = time(NULL);
    
    // Cleanup
    free(spectrum_db);
    free(flipped);
    free(shifted);
    free(zoomed);
    free(sorted);
    
    pthread_mutex_unlock(&current_spectrum_data.mutex);
}

// Check if client is authorized
static bool is_authorized_client(const char *client_ip) {
    for (int i = 0; i < current_config.udp_client_count; i++) {
        if (strcmp(client_ip, current_config.udp_client_ips[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Check rate limiting for client
static bool check_rate_limit(const char *client_ip) {
    time_t current_time = time(NULL);
    
    // Find existing client or add new one
    int client_index = -1;
    for (int i = 0; i < active_clients; i++) {
        if (strcmp(client_rates[i].client_ip, client_ip) == 0) {
            client_index = i;
            break;
        }
    }
    
    if (client_index == -1 && active_clients < MAX_UDP_CLIENTS) {
        // New client
        client_index = active_clients++;
        strncpy(client_rates[client_index].client_ip, client_ip, INET_ADDRSTRLEN - 1);
        client_rates[client_index].client_ip[INET_ADDRSTRLEN - 1] = '\0';
        client_rates[client_index].last_request_time = 0;
    }
    
    if (client_index == -1) {
        return false;  // Too many clients
    }
    
    // Check rate limit
    if (current_time - client_rates[client_index].last_request_time < current_config.max_request_rate) {
        return false;  // Rate limited
    }
    
    client_rates[client_index].last_request_time = current_time;
    return true;
}

// Format standard spectrum response
static void format_standard_response(char *buffer, size_t buffer_size) {
    pthread_mutex_lock(&current_spectrum_data.mutex);
    
    if (current_spectrum_data.active_type != SPEC_TYPE_STANDARD || !current_spectrum_data.ready) {
        snprintf(buffer, buffer_size, "ERROR:NO_STANDARD_DATA_AVAILABLE");
        pthread_mutex_unlock(&current_spectrum_data.mutex);
        return;
    }
    
    // Start with header
    int offset = snprintf(buffer, buffer_size, 
        "SPECTRA_STD:timestamp:%.6f,points:%d,data:",
        current_spectrum_data.standard.timestamp,
        current_spectrum_data.standard.num_points);
    
    // Add data points
    for (int i = 0; i < current_spectrum_data.standard.num_points && offset < buffer_size - 20; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, 
            "%.2e%s", current_spectrum_data.standard.data[i],
            (i == current_spectrum_data.standard.num_points - 1) ? "" : ",");
    }
    
    pthread_mutex_unlock(&current_spectrum_data.mutex);
}

// Format 120kHz spectrum response
static void format_120khz_response(char *buffer, size_t buffer_size) {
    pthread_mutex_lock(&current_spectrum_data.mutex);
    
    if (current_spectrum_data.active_type != SPEC_TYPE_120KHZ || !current_spectrum_data.ready) {
        snprintf(buffer, buffer_size, "ERROR:NO_120KHZ_DATA_AVAILABLE");
        pthread_mutex_unlock(&current_spectrum_data.mutex);
        return;
    }
    
    // Start with header
    int offset = snprintf(buffer, buffer_size,
        "SPECTRA_120KHZ:timestamp:%.6f,points:%d,freq_start:%.6f,freq_end:%.6f,baseline:%.6f,data:",
        current_spectrum_data.high_res.timestamp,
        current_spectrum_data.high_res.num_points,
        current_spectrum_data.high_res.freq_start,
        current_spectrum_data.high_res.freq_end,
        current_spectrum_data.high_res.baseline);
    
    // Add data points
    for (int i = 0; i < current_spectrum_data.high_res.num_points && offset < buffer_size - 20; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "%.6f%s", current_spectrum_data.high_res.data[i],
            (i == current_spectrum_data.high_res.num_points - 1) ? "" : ",");
    }
    
    pthread_mutex_unlock(&current_spectrum_data.mutex);
}

// Log spectrometer server messages
static void log_spec_message(const char *message) {
    if (spec_udp_log_file != NULL) {
        time_t now;
        time(&now);
        char *date = ctime(&now);
        date[strlen(date) - 1] = '\0'; // Remove newline
        fprintf(spec_udp_log_file, "%s : spectrometer_server.c : %s\n", date, message);
        fflush(spec_udp_log_file);
    }
}

// UDP server thread function
static void *udp_server_thread_func(void *arg) {
    (void)arg;
    struct sockaddr_in server_addr, client_addr;
    char *buffer = malloc(current_config.udp_buffer_size);
    char *response = malloc(current_config.udp_buffer_size);
    socklen_t client_len = sizeof(client_addr);
    
    if (!buffer || !response) {
        log_spec_message("Error allocating UDP buffers");
        if (buffer) free(buffer);
        if (response) free(response);
        return NULL;
    }
    
    // Create UDP socket
    udp_server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_server_socket < 0) {
        log_spec_message("Error creating UDP socket");
        free(buffer);
        free(response);
        return NULL;
    }
    
    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(udp_server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_spec_message("Warning: Could not set socket timeout");
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(current_config.udp_server_port);
    
    // Bind socket
    if (bind(udp_server_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_spec_message("Error binding UDP socket");
        close(udp_server_socket);
        udp_server_socket = -1;
        free(buffer);
        free(response);
        return NULL;
    }
    
    char start_msg[256];
    snprintf(start_msg, sizeof(start_msg), "Spectrometer UDP server started on port %d", current_config.udp_server_port);
    log_spec_message(start_msg);
    
    while (server_running) {
        // Check for new data in shared memory
        if (shared_memory && shared_memory->ready) {
            if (shared_memory->active_type == SPEC_TYPE_STANDARD) {
                // Process standard spectrum
                pthread_mutex_lock(&current_spectrum_data.mutex);
                current_spectrum_data.active_type = SPEC_TYPE_STANDARD;
                current_spectrum_data.standard.timestamp = shared_memory->timestamp;
                current_spectrum_data.standard.num_points = shared_memory->data_size / sizeof(double);
                if (current_spectrum_data.standard.num_points > 2048) {
                    current_spectrum_data.standard.num_points = 2048;
                }
                memcpy(current_spectrum_data.standard.data, (void*)shared_memory->data, 
                       current_spectrum_data.standard.num_points * sizeof(double));
                current_spectrum_data.ready = 1;
                current_spectrum_data.last_update = time(NULL);
                pthread_mutex_unlock(&current_spectrum_data.mutex);
            } else if (shared_memory->active_type == SPEC_TYPE_120KHZ) {
                // Process 120kHz spectrum with filtering
                process_120khz_spectrum((double*)shared_memory->data, shared_memory->data_size / sizeof(double));
            }
            shared_memory->ready = 0;  // Mark as processed
        }
        
        // Receive request from client
        ssize_t n = recvfrom(udp_server_socket, buffer, current_config.udp_buffer_size - 1, 0,
                            (struct sockaddr *)&client_addr, &client_len);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // Timeout, check for shutdown
            }
            if (server_running) {
                log_spec_message("Error receiving UDP data");
            }
            break;
        }
        
        buffer[n] = '\0';
        
        // Get client IP
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        // Check authorization and rate limiting
        if (!is_authorized_client(client_ip)) {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Rejected request from unauthorized client: %s", client_ip);
            log_spec_message(log_msg);
            continue;
        }
        
        if (!check_rate_limit(client_ip)) {
            snprintf(response, current_config.udp_buffer_size, "ERROR:RATE_LIMITED");
        } else if (strcmp(buffer, "GET_SPECTRA") == 0) {
            if (current_spectrum_data.active_type == SPEC_TYPE_STANDARD) {
                format_standard_response(response, current_config.udp_buffer_size);
            } else if (current_spectrum_data.active_type == SPEC_TYPE_120KHZ) {
                snprintf(response, current_config.udp_buffer_size, 
                    "ERROR:WRONG_SPECTROMETER_TYPE:current=120KHZ,requested=STD");
            } else {
                snprintf(response, current_config.udp_buffer_size, "ERROR:SPECTROMETER_NOT_RUNNING");
            }
        } else if (strcmp(buffer, "GET_SPECTRA_120KHZ") == 0) {
            if (current_spectrum_data.active_type == SPEC_TYPE_120KHZ) {
                format_120khz_response(response, current_config.udp_buffer_size);
            } else if (current_spectrum_data.active_type == SPEC_TYPE_STANDARD) {
                snprintf(response, current_config.udp_buffer_size, 
                    "ERROR:WRONG_SPECTROMETER_TYPE:current=STD,requested=120KHZ");
            } else {
                snprintf(response, current_config.udp_buffer_size, "ERROR:SPECTROMETER_NOT_RUNNING");
            }
        } else {
            snprintf(response, current_config.udp_buffer_size, "ERROR:UNKNOWN_REQUEST:%s", buffer);
        }
        
        // Send response
        sendto(udp_server_socket, response, strlen(response), 0,
               (const struct sockaddr *)&client_addr, client_len);
        
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Processed request '%s' from %s", buffer, client_ip);
        log_spec_message(log_msg);
    }
    
    if (udp_server_socket >= 0) {
        close(udp_server_socket);
        udp_server_socket = -1;
    }
    
    free(buffer);
    free(response);
    log_spec_message("Spectrometer UDP server thread stopped");
    return NULL;
}

// Public API implementations

int spec_server_init(const spec_server_config_t *config) {
    if (!config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&current_config, config, sizeof(spec_server_config_t));
    
    // Initialize spectrum data structure
    memset(&current_spectrum_data, 0, sizeof(spectrum_data_t));
    pthread_mutex_init(&current_spectrum_data.mutex, NULL);
    current_spectrum_data.active_type = SPEC_TYPE_NONE;
    current_spectrum_data.ready = 0;
    
    // Initialize rate limiting
    active_clients = 0;
    memset(client_rates, 0, sizeof(client_rates));
    
    // Initialize shared memory
    if (init_shared_memory() < 0) {
        return -1;
    }
    
    log_spec_message("Spectrometer server initialized successfully");
    return 0;
}

bool spec_server_start(void) {
    if (server_running) {
        log_spec_message("Spectrometer UDP server is already running");
        return false;
    }
    
    // Open log file
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/spectrometer_udp_server.log", "/home/mayukh/bcp/Sag/log");
    spec_udp_log_file = fopen(log_path, "a");
    if (spec_udp_log_file == NULL) {
        fprintf(stderr, "Warning: Could not open spectrometer UDP log file: %s\n", strerror(errno));
    }
    
    server_running = true;
    
    if (pthread_create(&udp_server_thread, NULL, udp_server_thread_func, NULL) != 0) {
        log_spec_message("Error creating spectrometer UDP server thread");
        server_running = false;
        if (spec_udp_log_file != NULL) {
            fclose(spec_udp_log_file);
            spec_udp_log_file = NULL;
        }
        return false;
    }
    
    return true;
}

void spec_server_stop(void) {
    if (!server_running) {
        return;
    }
    
    log_spec_message("Spectrometer UDP server shutdown initiated");
    server_running = false;
    
    // Close socket to unblock recvfrom
    if (udp_server_socket >= 0) {
        close(udp_server_socket);
        udp_server_socket = -1;
    }
    
    // Wait for thread to finish
    pthread_join(udp_server_thread, NULL);
    
    // Cleanup shared memory
    cleanup_shared_memory();
    
    if (spec_udp_log_file != NULL) {
        log_spec_message("Spectrometer UDP server stopped");
        fclose(spec_udp_log_file);
        spec_udp_log_file = NULL;
    }
}

bool spec_server_is_running(void) {
    return server_running;
}

void spec_server_set_active_type(spec_type_t type) {
    pthread_mutex_lock(&current_spectrum_data.mutex);
    current_spectrum_data.active_type = type;
    current_spectrum_data.ready = 0;  // Reset ready flag when type changes
    if (shared_memory) {
        shared_memory->active_type = type;
    }
    pthread_mutex_unlock(&current_spectrum_data.mutex);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Active spectrometer type set to: %d", type);
    log_spec_message(msg);
}

spec_type_t spec_server_get_active_type(void) {
    return current_spectrum_data.active_type;
}

const char* spec_server_get_shared_memory_name(void) {
    return SHM_NAME;
} 