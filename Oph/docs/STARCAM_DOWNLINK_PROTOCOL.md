# Star Camera Downlink Protocol Specification

## Overview
The Star Camera Downlink Protocol enables ground stations to request and receive star camera images from the balloon platform over UDP. The system is designed to work within bandwidth constraints (~200 kbps) and provides compressed JPEG images with metadata.

## Network Configuration
- **Protocol**: UDP
- **Default Port**: 8001
- **Max Bandwidth**: 180 kbps (configurable)
- **Chunk Size**: 1000 bytes (configurable)

## Message Format
All messages use a common header structure:

```c
typedef struct {
    uint8_t type;           // Message type (see below)
    uint32_t payload_size;  // Size of payload after header
    uint32_t sequence;      // Sequence number for tracking
} __attribute__((packed)) message_header_t;
```

## Message Types

### 1. GET_LATEST_IMAGE (Type 1)
Request the most recent image from the camera.

**Request**:
```c
message_header_t header = {
    .type = 1,
    .payload_size = 0,
    .sequence = <client_sequence>
};
```

**Response**: IMAGE_HEADER followed by IMAGE_CHUNKs and IMAGE_COMPLETE

### 2. GET_IMAGE_LIST (Type 2)
Request list of available images (not yet implemented).

### 3. GET_IMAGE_BY_TIMESTAMP (Type 3)
Request a specific image by timestamp.

**Request**:
```c
typedef struct {
    message_header_t header;
    time_t timestamp;       // Unix timestamp of desired image
} __attribute__((packed)) get_image_by_timestamp_msg_t;
```

### 4. GET_STATUS (Type 4)
Request server status information.

**Request**:
```c
message_header_t header = {
    .type = 4,
    .payload_size = 0,
    .sequence = <client_sequence>
};
```

**Response**: STATUS_RESPONSE

## Response Message Types

### 5. IMAGE_HEADER (Type 5)
Sent when starting image transmission.

```c
typedef struct {
    message_header_t header;
    time_t timestamp;           // Image timestamp
    uint32_t total_size;        // Total compressed image size
    uint32_t total_chunks;      // Number of chunks to follow
    uint8_t compression_quality;// JPEG quality (0-100)
    int blob_count;            // Number of stars detected
    int width;                 // Image width
    int height;                // Image height
} __attribute__((packed)) image_header_msg_t;
```

### 6. IMAGE_CHUNK (Type 6)
Contains compressed image data chunks.

```c
typedef struct {
    message_header_t header;
    uint32_t chunk_id;         // Chunk sequence number
    uint32_t data_size;        // Size of data in this chunk
    uint8_t data[0];           // Variable length data
} __attribute__((packed)) image_chunk_msg_t;
```

### 7. IMAGE_COMPLETE (Type 7)
Sent when image transmission is finished.

```c
typedef struct {
    message_header_t header;
    time_t timestamp;          // Image timestamp
    uint32_t total_chunks_sent;// Total chunks transmitted
} __attribute__((packed)) image_complete_msg_t;
```

### 8. ERROR (Type 8)
Sent when an error occurs.

```c
message_header_t error_msg = {
    .type = 8,
    .payload_size = 0,
    .sequence = <original_sequence>
};
```

### 9. STATUS_RESPONSE (Type 9)
Server status information.

```c
typedef struct {
    message_header_t header;
    uint32_t queue_size;               // Number of images in queue
    time_t current_transmission_timestamp; // Currently transmitting image (0 if none)
    uint32_t bandwidth_usage_kbps;     // Current bandwidth usage
} __attribute__((packed)) status_response_msg_t;
```

## Typical Communication Flow

### Requesting Latest Image
1. Client sends GET_LATEST_IMAGE
2. Server responds with IMAGE_HEADER
3. Server sends multiple IMAGE_CHUNK messages
4. Server sends IMAGE_COMPLETE when done

### Error Handling
- If no images are available, server sends ERROR response
- If image file is corrupted, server sends ERROR response
- Client should handle missing or out-of-order chunks

## Bandwidth Management
- Server limits transmission to configured bandwidth
- Chunks are sent with small delays to avoid network congestion
- If bandwidth limit is reached, transmission is temporarily paused

## Client Implementation Guidelines

### Basic Client Structure
```c
#include <sys/socket.h>
#include <netinet/in.h>

int client_socket = socket(AF_INET, SOCK_DGRAM, 0);
struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(8001),
    .sin_addr.s_addr = inet_addr("BALLOON_IP")
};

// Set receive timeout
struct timeval timeout = {10, 0}; // 10 second timeout
setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
```

### Requesting an Image
```c
// Send request
message_header_t request = {1, 0, sequence_number++};
sendto(client_socket, &request, sizeof(request), 0, 
       (struct sockaddr*)&server_addr, sizeof(server_addr));

// Receive response
uint8_t buffer[2048];
struct sockaddr_in from_addr;
socklen_t from_len = sizeof(from_addr);
ssize_t received = recvfrom(client_socket, buffer, sizeof(buffer), 0,
                           (struct sockaddr*)&from_addr, &from_len);

// Parse response
message_header_t *header = (message_header_t*)buffer;
switch (header->type) {
    case 5: // IMAGE_HEADER
        // Parse header and prepare for chunks
        break;
    case 8: // ERROR
        // Handle error
        break;
}
```

### Receiving Image Chunks
```c
uint8_t *image_buffer = malloc(total_size);
uint32_t chunks_received = 0;
bool chunk_received[total_chunks];
memset(chunk_received, 0, sizeof(chunk_received));

while (chunks_received < total_chunks) {
    ssize_t received = recvfrom(client_socket, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&from_addr, &from_len);
    
    if (received > 0) {
        message_header_t *header = (message_header_t*)buffer;
        
        if (header->type == 6) { // IMAGE_CHUNK
            image_chunk_msg_t *chunk = (image_chunk_msg_t*)buffer;
            
            if (!chunk_received[chunk->chunk_id]) {
                uint32_t offset = chunk->chunk_id * CHUNK_SIZE;
                memcpy(image_buffer + offset, chunk->data, chunk->data_size);
                chunk_received[chunk->chunk_id] = true;
                chunks_received++;
            }
        } else if (header->type == 7) { // IMAGE_COMPLETE
            break;
        }
    }
}

// Save JPEG image
FILE *output = fopen("received_image.jpg", "wb");
fwrite(image_buffer, 1, total_size, output);
fclose(output);
```

## Configuration Parameters

The server behavior can be configured via `bcp_Oph.config`:

```
starcam_downlink:
{
  enabled = 1;                     // Enable/disable downlink
  logfile = "log/starcam_downlink.log";
  port = 8001;                     // UDP port
  max_image_queue = 10;            // Max images to keep in memory
  compression_quality = 60;        // JPEG quality (0-100)
  chunk_size = 1000;              // UDP packet size
  max_bandwidth_kbps = 180;       // Bandwidth limit
  image_timeout_sec = 300;        // How long to keep images
  workdir = "/home/ophiuchus/bvexcam/pics";
  notification_file = "/tmp/starcam_new_image.notify";
};
```

## Performance Characteristics

### Compression Ratios
- Typical astronomical images: 80-95% size reduction
- 2.3MB raw → ~200KB compressed (at quality 60)
- Quality 60 preserves star visibility while maximizing compression

### Transmission Times
- 200KB image ÷ 25KB/s ≈ 8 seconds transmission time
- Allows for one image every 10 seconds comfortably

### Image Quality
- JPEG compression optimized for astronomical data
- Black space compresses extremely well
- Stars remain clearly visible at quality 60+
- Adjustable quality parameter for fine-tuning

## Error Recovery
- Client should implement timeout handling
- Missing chunks can be detected and re-requested (future enhancement)
- Server maintains image queue for recent images
- Automatic bandwidth throttling prevents network overload

## Security Considerations
- UDP protocol provides no encryption
- No authentication mechanism implemented
- Suitable for point-to-point balloon-to-ground communication
- Consider adding authentication if operating in shared spectrum 