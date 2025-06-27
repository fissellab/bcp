#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdarg.h>

#include "starcam_downlink.h"

// Global variables
starcam_downlink_config_t starcam_config;
static pthread_t server_thread;
static int server_socket = -1;
static int server_running = 0;
static FILE *log_file = NULL;

// Smart replacement image management
static image_data_t current_image = {0};     // Latest available image
static image_data_t pending_image = {0};     // Newer image waiting to replace current
static pthread_mutex_t image_mutex = PTHREAD_MUTEX_INITIALIZER;

// Current transmission state
static struct {
    int active;
    time_t image_timestamp;
    uint32_t total_chunks;
    uint32_t chunks_sent;
    struct sockaddr_in client_addr;
    time_t start_time;
} transmission_state = {0};

// Bandwidth limiting
static struct {
    time_t last_update;
    uint32_t bytes_sent;
    uint32_t current_bps;
} bandwidth_tracker = {0};

// Error handling for JPEG compression
struct jpeg_error_mgr_custom {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static void jpeg_error_exit(j_common_ptr cinfo) {
    struct jpeg_error_mgr_custom *err = (struct jpeg_error_mgr_custom *) cinfo->err;
    longjmp(err->setjmp_buffer, 1);
}

// Logging function
static void log_message(const char *level, const char *message, ...) {
    if (!log_file) return;
    
    time_t now = time(NULL);
    va_list args;
    va_start(args, message);
    
    fprintf(log_file, "[%ld][starcam_downlink][%s] ", now, level);
    vfprintf(log_file, message, args);
    fprintf(log_file, "\n");
    fflush(log_file);
    
    va_end(args);
}

// Compress raw image data to JPEG
static int compress_image_to_jpeg(void *raw_data, int width, int height, 
                                 uint8_t **output_buffer, uint32_t *output_size) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr_custom jerr;
    JSAMPROW row_pointer[1];
    uint8_t *jpeg_buffer = NULL;
    unsigned long jpeg_size = 0;
    
    // DEBUG: Check for obviously invalid image data
    uint8_t *data_ptr = (uint8_t*)raw_data;
    int zero_count = 0, nonzero_count = 0;
    uint32_t pixel_sum = 0;
    
    // Sample first 1000 pixels to check image validity
    int sample_size = (width * height > 1000) ? 1000 : width * height;
    for (int i = 0; i < sample_size; i++) {
        if (data_ptr[i] == 0) {
            zero_count++;
        } else {
            nonzero_count++;
            pixel_sum += data_ptr[i];
        }
    }
    
    log_message("DEBUG", "Image statistics: %dx%d, sample %d pixels: %d zeros, %d non-zeros, avg non-zero: %.1f", 
               width, height, sample_size, zero_count, nonzero_count, 
               nonzero_count > 0 ? (float)pixel_sum / nonzero_count : 0.0);
    
    // Set up error handling
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_compress(&cinfo);
        if (jpeg_buffer) free(jpeg_buffer);
        log_message("ERROR", "JPEG compression failed with libjpeg error");
        return -1;
    }
    
    // Initialize JPEG compression
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &jpeg_buffer, &jpeg_size);
    
    // Set compression parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 1;  // Grayscale
    cinfo.in_color_space = JCS_GRAYSCALE;
    
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, starcam_config.compression_quality, TRUE);
    
    // Start compression
    jpeg_start_compress(&cinfo, TRUE);
    
    // Compress line by line
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = (JSAMPROW)((uint8_t*)raw_data + cinfo.next_scanline * width);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    
    // Allocate output buffer and copy data
    *output_buffer = malloc(jpeg_size);
    if (!*output_buffer) {
        free(jpeg_buffer);
        log_message("ERROR", "Failed to allocate output buffer for compressed image");
        return -1;
    }
    
    memcpy(*output_buffer, jpeg_buffer, jpeg_size);
    *output_size = jpeg_size;
    
    free(jpeg_buffer);
    log_message("DEBUG", "JPEG compression successful: %u bytes output", jpeg_size);
    return 0;
}

// Helper function to free compressed data
static void free_image_data(image_data_t *image) {
    if (image->compressed_data) {
        free(image->compressed_data);
        image->compressed_data = NULL;
    }
    image->valid = 0;
}

// Proactively stream image to configured clients
static void stream_to_clients(image_data_t *img) {
    if (starcam_config.num_client_ips == 0) {
        return; // No clients configured for proactive streaming
    }
    
    for (int i = 0; i < starcam_config.num_client_ips; i++) {
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(starcam_config.port + 1); // Use port+1 for proactive streaming
        
        if (inet_aton(starcam_config.udp_client_ips[i], &client_addr.sin_addr) == 0) {
            log_message("ERROR", "Invalid client IP address: %s", starcam_config.udp_client_ips[i]);
            continue;
        }
        
        // Send image header to client
        image_header_msg_t img_header = {
            .header = {MSG_IMAGE_HEADER, sizeof(image_header_msg_t) - sizeof(message_header_t), 0},
            .timestamp = img->timestamp,
            .total_size = img->compressed_size,
            .total_chunks = (img->compressed_size + starcam_config.chunk_size - 1) / starcam_config.chunk_size,
            .compression_quality = img->compression_quality,
            .blob_count = img->blob_count,
            .width = img->width,
            .height = img->height
        };
        
        ssize_t sent = sendto(server_socket, &img_header, sizeof(img_header), 0,
                             (struct sockaddr*)&client_addr, sizeof(client_addr));
        
        if (sent > 0) {
            log_message("INFO", "Sent image header to client %s (%u bytes, %u chunks)", 
                       starcam_config.udp_client_ips[i], img->compressed_size, img_header.total_chunks);
        } else {
            log_message("ERROR", "Failed to send image header to client %s: %s", 
                       starcam_config.udp_client_ips[i], strerror(errno));
        }
    }
}

// Replace current image with new one (smart replacement logic)
static int replace_current_image(const char *image_path, int blob_count, 
                               time_t timestamp, void *raw_data, int width, int height) {
    pthread_mutex_lock(&image_mutex);
    
    // Compress the new image
    uint8_t *compressed_data;
    uint32_t compressed_size;
    if (compress_image_to_jpeg(raw_data, width, height, &compressed_data, &compressed_size) != 0) {
        log_message("ERROR", "Failed to compress image: %s", image_path);
        pthread_mutex_unlock(&image_mutex);
        return -1;
    }
    
    log_message("INFO", "Compressed image %s: %dx%d -> %u bytes (%.1f%% reduction)", 
               image_path, width, height, compressed_size, 
               100.0 * (1.0 - (double)compressed_size / (width * height)));
    
    if (transmission_state.active) {
        // Transmission in progress - update pending image
        free_image_data(&pending_image);
        
        strncpy(pending_image.image_path, image_path, sizeof(pending_image.image_path) - 1);
        pending_image.timestamp = timestamp;
        pending_image.blob_count = blob_count;
        pending_image.width = width;
        pending_image.height = height;
        pending_image.compressed_data = compressed_data;
        pending_image.compressed_size = compressed_size;
        pending_image.original_size = width * height;
        pending_image.compression_quality = starcam_config.compression_quality;
        pending_image.created_time = time(NULL);
        pending_image.valid = 1;
        
        log_message("INFO", "Stored pending image: %s (transmission in progress)", image_path);
    } else {
        // No transmission - update current image immediately
        free_image_data(&current_image);
        
        strncpy(current_image.image_path, image_path, sizeof(current_image.image_path) - 1);
        current_image.timestamp = timestamp;
        current_image.blob_count = blob_count;
        current_image.width = width;
        current_image.height = height;
        current_image.compressed_data = compressed_data;
        current_image.compressed_size = compressed_size;
        current_image.original_size = width * height;
        current_image.compression_quality = starcam_config.compression_quality;
        current_image.created_time = time(NULL);
        current_image.valid = 1;
        
        log_message("INFO", "Updated current image: %s", image_path);
        
        // PROACTIVELY STREAM TO CONFIGURED CLIENTS
        stream_to_clients(&current_image);
    }
    
    pthread_mutex_unlock(&image_mutex);
    return 0;
}

// Get the latest available image for transmission
static image_data_t* get_latest_image(void) {
    pthread_mutex_lock(&image_mutex);
    
    if (!current_image.valid) {
        pthread_mutex_unlock(&image_mutex);
        return NULL;
    }
    
    // Return pointer to current image (caller should not free this)
    pthread_mutex_unlock(&image_mutex);
    return &current_image;
}

// Called when transmission completes - promote pending image if available
static void on_transmission_complete(void) {
    pthread_mutex_lock(&image_mutex);
    
    transmission_state.active = 0;
    
    if (pending_image.valid) {
        // Promote pending image to current
        free_image_data(&current_image);
        current_image = pending_image;
        
        // Clear pending slot
        memset(&pending_image, 0, sizeof(pending_image));
        
        log_message("INFO", "Promoted pending image to current: %s", current_image.image_path);
    }
    
    pthread_mutex_unlock(&image_mutex);
}

// Update bandwidth usage tracking
static void update_bandwidth_usage(uint32_t bytes_sent) {
    time_t now = time(NULL);
    
    if (now != bandwidth_tracker.last_update) {
        bandwidth_tracker.current_bps = bandwidth_tracker.bytes_sent;
        bandwidth_tracker.bytes_sent = bytes_sent;
        bandwidth_tracker.last_update = now;
    } else {
        bandwidth_tracker.bytes_sent += bytes_sent;
    }
}

// Check if we can send data without exceeding bandwidth limit
static int can_send_data(uint32_t bytes) {
    uint32_t max_bps = starcam_config.max_bandwidth_kbps * 1024 / 8;  // Convert to bytes per second
    return (bandwidth_tracker.bytes_sent + bytes) <= max_bps;
}

// Send message to client
static int send_message(int socket, struct sockaddr_in *client_addr, 
                       void *message, size_t size) {
    if (!can_send_data(size)) {
        log_message("WARN", "Bandwidth limit reached, delaying transmission");
        usleep(100000);  // Wait 100ms
        return 0;  // Return 0 to indicate delay, not error
    }
    
    ssize_t sent = sendto(socket, message, size, 0, 
                         (struct sockaddr*)client_addr, sizeof(*client_addr));
    if (sent > 0) {
        update_bandwidth_usage(sent);
    }
    
    return sent;
}

// Handle client requests
static void handle_client_request(int socket, struct sockaddr_in *client_addr, 
                                uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < sizeof(message_header_t)) {
        log_message("ERROR", "Received invalid message size: %zu", buffer_size);
        return;
    }
    
    message_header_t *header = (message_header_t*)buffer;
    
    switch (header->type) {
        case MSG_GET_LATEST_IMAGE: {
            image_data_t *img = get_latest_image();
            if (!img) {
                // Send error response
                message_header_t error_msg = {MSG_ERROR, 0, header->sequence};
                send_message(socket, client_addr, &error_msg, sizeof(error_msg));
                return;
            }
            
            // Start transmission using in-memory compressed data
            transmission_state.active = 1;
            transmission_state.image_timestamp = img->timestamp;
            transmission_state.total_chunks = (img->compressed_size + starcam_config.chunk_size - 1) / starcam_config.chunk_size;
            transmission_state.chunks_sent = 0;
            transmission_state.client_addr = *client_addr;
            transmission_state.start_time = time(NULL);
            
            // Send image header
            image_header_msg_t img_header = {
                .header = {MSG_IMAGE_HEADER, sizeof(image_header_msg_t) - sizeof(message_header_t), header->sequence},
                .timestamp = img->timestamp,
                .total_size = img->compressed_size,
                .total_chunks = transmission_state.total_chunks,
                .compression_quality = img->compression_quality,
                .blob_count = img->blob_count,
                .width = img->width,
                .height = img->height
            };
            
            send_message(socket, client_addr, &img_header, sizeof(img_header));
            
            log_message("INFO", "Started transmission of image %ld (%u bytes in %u chunks)",
                       img->timestamp, img->compressed_size, transmission_state.total_chunks);
            break;
        }
        
        case MSG_GET_STATUS: {
            pthread_mutex_lock(&image_mutex);
            int images_available = current_image.valid ? 1 : 0;
            time_t latest_timestamp = current_image.valid ? current_image.timestamp : 0;
            pthread_mutex_unlock(&image_mutex);
            
            status_response_msg_t status = {
                .header = {MSG_STATUS_RESPONSE, sizeof(status_response_msg_t) - sizeof(message_header_t), header->sequence},
                .current_images_available = images_available,
                .latest_image_timestamp = latest_timestamp,
                .bandwidth_usage_kbps = bandwidth_tracker.current_bps * 8 / 1024,
                .transmission_active = transmission_state.active
            };
            
            send_message(socket, client_addr, &status, sizeof(status));
            break;
        }
        
        case MSG_GET_IMAGE_BY_TIMESTAMP: {
            if (buffer_size < sizeof(get_image_by_timestamp_msg_t)) {
                message_header_t error_msg = {MSG_ERROR, 0, header->sequence};
                send_message(socket, client_addr, &error_msg, sizeof(error_msg));
                return;
            }
            
            get_image_by_timestamp_msg_t *req = (get_image_by_timestamp_msg_t*)buffer;
            image_data_t *img = get_latest_image();
            
            if (!img) {
                message_header_t error_msg = {MSG_ERROR, 0, header->sequence};
                send_message(socket, client_addr, &error_msg, sizeof(error_msg));
                return;
            }
            
            // Similar logic to MSG_GET_LATEST_IMAGE...
            // (Implementation would be similar to above)
            break;
        }
        
        default:
            log_message("WARN", "Unknown message type: %d", header->type);
            break;
    }
}

// Continue ongoing transmission
static void continue_transmission(int socket) {
    if (!transmission_state.active) return;
    
    // Get current image being transmitted
    pthread_mutex_lock(&image_mutex);
    if (!current_image.valid || current_image.timestamp != transmission_state.image_timestamp) {
        // Image no longer available or changed, abort transmission
        pthread_mutex_unlock(&image_mutex);
        log_message("WARN", "Aborting transmission - image no longer available");
        transmission_state.active = 0;
        on_transmission_complete();
        return;
    }
    
    uint8_t *compressed_data = current_image.compressed_data;
    uint32_t compressed_size = current_image.compressed_size;
    pthread_mutex_unlock(&image_mutex);
    
    while (transmission_state.chunks_sent < transmission_state.total_chunks) {
        uint32_t chunk_offset = transmission_state.chunks_sent * starcam_config.chunk_size;
        uint32_t chunk_size = starcam_config.chunk_size;
        
        // Last chunk might be smaller
        if (chunk_offset + chunk_size > compressed_size) {
            chunk_size = compressed_size - chunk_offset;
        }
        
        // Create chunk message
        size_t msg_size = sizeof(image_chunk_msg_t) + chunk_size;
        image_chunk_msg_t *chunk_msg = malloc(msg_size);
        if (!chunk_msg) {
            log_message("ERROR", "Failed to allocate chunk message");
            break;
        }
        
        chunk_msg->header.type = MSG_IMAGE_CHUNK;
        chunk_msg->header.payload_size = msg_size - sizeof(message_header_t);
        chunk_msg->header.sequence = transmission_state.chunks_sent;
        chunk_msg->chunk_id = transmission_state.chunks_sent;
        chunk_msg->data_size = chunk_size;
        
        memcpy(chunk_msg->data, compressed_data + chunk_offset, chunk_size);
        
        int sent = send_message(socket, &transmission_state.client_addr, chunk_msg, msg_size);
        free(chunk_msg);
        
        if (sent <= 0) {
            if (sent == 0) {
                // Bandwidth limited, try again later
                break;
            } else {
                log_message("ERROR", "Failed to send chunk %u: %s", 
                           transmission_state.chunks_sent, strerror(errno));
                break;
            }
        }
        
        transmission_state.chunks_sent++;
        
        // Small delay between chunks to avoid overwhelming the network
        usleep(1000);  // 1ms delay
    }
    
    // Check if transmission is complete
    if (transmission_state.chunks_sent >= transmission_state.total_chunks) {
        image_complete_msg_t complete_msg = {
            .header = {MSG_IMAGE_COMPLETE, sizeof(image_complete_msg_t) - sizeof(message_header_t), 0},
            .timestamp = transmission_state.image_timestamp,
            .total_chunks_sent = transmission_state.chunks_sent
        };
        
        send_message(socket, &transmission_state.client_addr, &complete_msg, sizeof(complete_msg));
        
        log_message("INFO", "Completed transmission of image %ld (%u chunks sent)",
                   transmission_state.image_timestamp, transmission_state.chunks_sent);
        
        // Clean up transmission state
        memset(&transmission_state, 0, sizeof(transmission_state));
        
        on_transmission_complete();
    }
}

// Main server thread
static void* server_thread_func(void *arg) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    uint8_t buffer[2048];
    
    // Create UDP socket
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) {
        log_message("ERROR", "Failed to create socket: %s", strerror(errno));
        return NULL;
    }
    
    // Set socket options
    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Set non-blocking
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
    
    // Bind socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(starcam_config.port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_message("ERROR", "Failed to bind socket: %s", strerror(errno));
        close(server_socket);
        return NULL;
    }
    
    log_message("INFO", "Starcam downlink server started on port %d", starcam_config.port);
    server_running = 1;
    
    while (server_running) {
        // Check for incoming requests
        ssize_t received = recvfrom(server_socket, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (received > 0) {
            handle_client_request(server_socket, &client_addr, buffer, received);
        } else if (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            log_message("ERROR", "Error receiving data: %s", strerror(errno));
        }
        
        // Continue any ongoing transmission
        continue_transmission(server_socket);
        
        // Small delay to avoid busy waiting
        usleep(10000);  // 10ms
    }
    
    close(server_socket);
    log_message("INFO", "Starcam downlink server stopped");
    return NULL;
}

// Public function implementations
int initStarcamDownlink(void) {
    // Open log file
    log_file = fopen(starcam_config.logfile, "a");
    if (!log_file) {
        fprintf(stderr, "Failed to open starcam downlink log file: %s\n", strerror(errno));
        return -1;
    }
    
    log_message("INFO", "Starcam downlink initialized");
    return 0;
}

void cleanupStarcamDownlink(void) {
    server_running = 0;
    
    if (server_thread) {
        pthread_join(server_thread, NULL);
    }
    
    // Clean up image data
    pthread_mutex_lock(&image_mutex);
    free_image_data(&current_image);
    free_image_data(&pending_image);
    pthread_mutex_unlock(&image_mutex);
    
    if (log_file) {
        log_message("INFO", "Starcam downlink shutdown");
        fclose(log_file);
        log_file = NULL;
    }
}

void notifyImageServer(const char* image_path, int blob_count, time_t timestamp, 
                      void* raw_data, int width, int height) {
    if (!starcam_config.enabled || !server_running) {
        return;
    }
    
    replace_current_image(image_path, blob_count, timestamp, raw_data, width, height);
}

int startStarcamServer(void) {
    if (!starcam_config.enabled) {
        return 0;  // Not enabled, but not an error
    }
    
    if (pthread_create(&server_thread, NULL, server_thread_func, NULL) != 0) {
        log_message("ERROR", "Failed to create server thread: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

// Get server status for CLI
int getStarcamStatus(int *images_available_out, int *server_running_out, 
                    uint32_t *bandwidth_usage_out, int *transmission_active_out,
                    time_t *latest_timestamp_out) {
    if (!starcam_config.enabled) {
        return 0;
    }
    
    pthread_mutex_lock(&image_mutex);
    *images_available_out = current_image.valid ? 1 : 0;
    *latest_timestamp_out = current_image.valid ? current_image.timestamp : 0;
    pthread_mutex_unlock(&image_mutex);
    
    *server_running_out = server_running;
    *bandwidth_usage_out = bandwidth_tracker.current_bps * 8 / 1024; // kbps
    *transmission_active_out = transmission_state.active;
    
    return 1;
} 