#ifndef STARCAM_DOWNLINK_H
#define STARCAM_DOWNLINK_H

#include <stdint.h>
#include <time.h>

// Configuration structure
typedef struct {
    int enabled;
    char logfile[256];
    int port;
    int compression_quality;
    int chunk_size;
    int max_bandwidth_kbps;
    int image_timeout_sec;
    char workdir[256];
    char notification_file[256];
    char **udp_client_ips;
    int num_client_ips;
} starcam_downlink_config_t;

// Image data structure (replaces image_metadata_t)
typedef struct {
    char image_path[512];
    time_t timestamp;
    int blob_count;
    uint32_t compressed_size;
    uint32_t original_size;
    uint8_t compression_quality;
    int width;
    int height;
    time_t created_time;
    uint8_t *compressed_data;  // In-memory compressed data
    int valid;  // Whether this slot contains valid data
} image_data_t;

// Protocol message types
typedef enum {
    MSG_GET_LATEST_IMAGE = 1,
    MSG_GET_IMAGE_LIST = 2,
    MSG_GET_IMAGE_BY_TIMESTAMP = 3,
    MSG_GET_STATUS = 4,
    MSG_IMAGE_HEADER = 5,
    MSG_IMAGE_CHUNK = 6,
    MSG_IMAGE_COMPLETE = 7,
    MSG_ERROR = 8,
    MSG_STATUS_RESPONSE = 9
} message_type_t;

// Protocol structures
typedef struct {
    uint8_t type;
    uint32_t payload_size;
    uint32_t sequence;
} __attribute__((packed)) message_header_t;

typedef struct {
    message_header_t header;
    time_t timestamp;
} __attribute__((packed)) get_image_by_timestamp_msg_t;

typedef struct {
    message_header_t header;
    time_t timestamp;
    uint32_t total_size;
    uint32_t total_chunks;
    uint8_t compression_quality;
    int blob_count;
    int width;
    int height;
} __attribute__((packed)) image_header_msg_t;

typedef struct {
    message_header_t header;
    uint32_t chunk_id;
    uint32_t data_size;
    uint8_t data[0];  // Variable length data
} __attribute__((packed)) image_chunk_msg_t;

typedef struct {
    message_header_t header;
    time_t timestamp;
    uint32_t total_chunks_sent;
} __attribute__((packed)) image_complete_msg_t;

typedef struct {
    message_header_t header;
    uint32_t current_images_available;
    time_t latest_image_timestamp;
    uint32_t bandwidth_usage_kbps;
    int transmission_active;
} __attribute__((packed)) status_response_msg_t;

// Function declarations
int initStarcamDownlink(void);
void cleanupStarcamDownlink(void);
void notifyImageServer(const char* image_path, int blob_count, time_t timestamp, 
                      void* raw_data, int width, int height);
int startStarcamServer(void);
int getStarcamStatus(int *images_available_out, int *server_running_out, 
                    uint32_t *bandwidth_usage_out, int *transmission_active_out,
                    time_t *latest_timestamp_out);

// Global configuration
extern starcam_downlink_config_t starcam_config;

#endif // STARCAM_DOWNLINK_H 