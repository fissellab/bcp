# Star Camera Downlink Implementation Plan

## Summary
This document outlines the implementation of a UDP-based image downlink system for the star camera. The system addresses the challenge of transmitting 2.3MB images over a 200 kbps link by using JPEG compression and chunked transmission.

## Key Design Decisions

### 1. **Compression Strategy**
- **JPEG compression** optimized for astronomical images
- **Quality level 60** provides good balance (80-95% compression)
- **Grayscale format** since star cameras typically don't need color
- **Expected compression**: 2.3MB → ~200KB

### 2. **Transmission Strategy**
- **UDP protocol** for simplicity and speed
- **Chunked transmission** with 1000-byte packets
- **Bandwidth limiting** to stay within 180 kbps limit
- **Priority queue** where newer images get priority

### 3. **Integration Strategy**
- **File-based notification** from camera.c (Option A)
- **Process processed image data** (`output_buffer`)
- **Minimal camera.c changes** for easy integration

## Files Created/Modified

### New Files
1. **`src/starcam_downlink.h`** - Header with protocol definitions
2. **`src/starcam_downlink.c`** - Main server implementation
3. **`docs/STARCAM_DOWNLINK_PROTOCOL.md`** - Protocol specification
4. **`docs/IMPLEMENTATION_PLAN.md`** - This document

### Modified Files
1. **`bcp_Oph.config`** - Added starcam_downlink configuration section
2. **`src/camera.c`** - Added notification call after image processing

## Configuration Added to bcp_Oph.config

```
starcam_downlink:
{
 enabled = 1;
 logfile = "log/starcam_downlink.log";
 port = 8001;                    // UDP port for ground station requests
 max_image_queue = 10;           // How many recent images to keep available
 compression_quality = 60;       // JPEG quality (0-100, lower = more compression)
 chunk_size = 1000;             // UDP packet size in bytes
 max_bandwidth_kbps = 180;      // Max bandwidth usage (leave headroom from 200)
 image_timeout_sec = 300;       // How long to keep images available
 workdir = "/home/ophiuchus/bvexcam/pics";
 notification_file = "/tmp/starcam_new_image.notify";
};
```

## Camera.c Integration

### Added Function Declaration
```c
void notifyImageServer(const char* image_path, int blob_count, time_t timestamp, 
                      void* raw_data, int width, int height);
```

### Added Notification Call
The notification is called right after the symlink operation in `doCameraAndAstrometry()`:
```c
symlink(date, join_path(config.bvexcam.workdir,"/latest_saved_image.bmp"));

// Notify image server of new image for downlink
notifyImageServer(date, blob_count, tv.tv_sec, output_buffer, CAMERA_WIDTH, CAMERA_HEIGHT);
```

## Protocol Overview

### Message Types
1. **GET_LATEST_IMAGE** - Client requests newest image
2. **GET_STATUS** - Client requests server status
3. **IMAGE_HEADER** - Server sends image metadata
4. **IMAGE_CHUNK** - Server sends image data chunks
5. **IMAGE_COMPLETE** - Server signals transmission complete
6. **ERROR** - Server reports error

### Typical Flow
1. Ground station sends `GET_LATEST_IMAGE`
2. Server responds with `IMAGE_HEADER` (metadata)
3. Server sends multiple `IMAGE_CHUNK` messages
4. Server sends `IMAGE_COMPLETE` when done

## Performance Analysis

### Bandwidth Usage
- **Available**: 200 kbps = ~25 kB/s
- **Configured limit**: 180 kbps = ~22.5 kB/s (leaves headroom)
- **Transmission time**: 200KB ÷ 22.5 kB/s ≈ 9 seconds
- **Target interval**: 10 seconds (comfortable margin)

### Compression Analysis
- **Raw image**: CAMERA_WIDTH × CAMERA_HEIGHT bytes (grayscale)
- **Typical astronomical compression**: 80-95% reduction
- **Quality 60 JPEG**: Good balance of size vs. star visibility
- **Expected result**: 2.3MB → 150-250KB

## Implementation Status

### ✅ Completed
1. Configuration structure and integration
2. Protocol specification and documentation
3. Basic server framework with logging
4. Camera.c integration points
5. Header file with all protocol structures

### 🚧 Partially Implemented
1. Basic server structure (needs full UDP implementation)
2. Notification function (placeholder exists)

### ⏳ TODO for Full Implementation
1. **JPEG compression functionality**
2. **Complete UDP server implementation**
3. **Image queue management**
4. **Bandwidth limiting logic**
5. **Chunked transmission logic**
6. **Configuration file parsing integration**
7. **Thread management for server**

## Next Steps

### Phase 1: Core Functionality
1. Implement JPEG compression in `starcam_downlink.c`
2. Complete UDP server thread implementation
3. Add image queue management
4. Test basic image transmission

### Phase 2: Optimization
1. Implement bandwidth limiting
2. Add error handling and recovery
3. Optimize compression settings for star images
4. Add performance monitoring

### Phase 3: Integration
1. Integrate with bcp configuration parsing
2. Add server startup/shutdown to main program
3. Test with real camera data
4. Document ground station client examples

## Dependencies

### System Libraries
- **jpeglib** - For JPEG compression (`apt install libjpeg-dev`)
- **pthread** - For threading (usually built-in)
- **socket libraries** - For UDP communication (built-in)

### Build System
The implementation will need to be added to the existing build system with:
```makefile
LIBS += -ljpeg -lpthread
SOURCES += src/starcam_downlink.c
```

## Testing Strategy

### Unit Testing
1. Test JPEG compression with sample images
2. Test protocol message parsing
3. Test bandwidth limiting calculations

### Integration Testing
1. Test with camera.c notification system
2. Test configuration file parsing
3. Test server startup/shutdown

### System Testing
1. Test complete image transmission flow
2. Test with bandwidth constraints
3. Test error conditions and recovery

## Risk Mitigation

### Technical Risks
1. **JPEG compression quality** - Adjustable quality parameter allows tuning
2. **Bandwidth overruns** - Built-in bandwidth limiting and monitoring
3. **UDP packet loss** - Chunked transmission allows for future retry logic

### Integration Risks
1. **Camera.c changes** - Minimal changes reduce integration risk
2. **Configuration conflicts** - New dedicated section avoids conflicts
3. **Thread safety** - Proper mutex usage for shared data structures

## Performance Monitoring

The system includes built-in monitoring for:
- **Queue size** - Number of images waiting to transmit
- **Bandwidth usage** - Current transmission rate
- **Compression ratios** - Actual vs. expected compression
- **Transmission times** - Time to complete image transfer

This monitoring data is available via the `GET_STATUS` protocol message and logged to the dedicated log file. 