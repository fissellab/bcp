# Smart Replacement Design for Starcam Downlink

## Overview
The starcam downlink system has been refactored from a queue-based approach to a **smart replacement** approach that prioritizes the latest images.

## Rationale

### Original Problem with Queue System:
- **Images captured**: 10 Hz (every 0.1 seconds)
- **Client requests**: Every 10 seconds  
- **Downlink time**: ~1-2 seconds per image (after proper compression)
- **Goal**: Always send the **latest** image, not old queued images

The queue system would send outdated images when newer ones were available, which defeats the purpose of real-time imaging.

## Smart Replacement Design

### Core Components:
1. **Current Image Slot**: Holds the latest available image
2. **Pending Image Slot**: Holds newer image when transmission is active
3. **Transmission State**: Tracks ongoing transmission progress

### State Machine:
```
IDLE → New image arrives → Update current image → Ready for transmission

TRANSMITTING → New image arrives → Store in pending slot → Continue transmission

TRANSMISSION_COMPLETE → Pending image exists → Promote pending to current → Ready for new transmission
```

### Key Benefits:
1. **Always Latest**: Clients always get the most recent image available
2. **No Stale Data**: Old images are automatically replaced
3. **Efficient Memory**: Only 2 images in memory maximum (current + pending)
4. **Smart Interruption**: New images don't interrupt ongoing transmission but are queued for next request

## Performance Estimates

### Compression Performance:
- **Original size**: 2.3MB (raw grayscale)
- **Compressed size**: ~27-40KB (JPEG quality 60)
- **Compression ratio**: 98-99% size reduction
- **Compression time**: ~50-100ms

### Transmission Performance:
- **Bandwidth limit**: 180 kbps (safety margin below 200 kbps)
- **Transmission time**: 40KB × 8 / 180 kbps = **~1.8 seconds**
- **Client interval**: 10 seconds
- **Efficiency**: ~18% bandwidth utilization, leaving 82% margin

## Implementation Changes

### Removed:
- Image queue system and associated management
- File-based temporary storage for compressed images
- `max_image_queue` configuration parameter
- Complex priority-based queue sorting

### Added:
- `image_data_t` structure with in-memory compressed data
- Smart replacement logic with current/pending slots
- Transmission state management
- Automatic pending image promotion on completion

### Modified:
- `getStarcamStatus()` function signature to return image availability
- CLI status display to show current image timestamp
- Protocol status messages to include latest image info
- Configuration parsing to remove queue-related parameters

## Protocol Impact

### Status Response Changes:
```c
// Old
uint32_t queue_size;
time_t current_transmission_timestamp;

// New  
uint32_t current_images_available;  // 0 or 1
time_t latest_image_timestamp;      // Timestamp of available image
int transmission_active;            // Current transmission status
```

### Client Benefits:
- Always receive the freshest image data
- Clear indication of image availability and recency
- Improved transmission reliability and performance
- Simplified protocol interaction

## Testing Verification

The smart replacement system can be tested by:
1. Starting the server with `./main bcp_Oph.config`
2. Using CLI command `starcam_downlink_status` to check system state
3. Observing image replacement behavior in logs
4. Measuring actual compression ratios and transmission times

This design ensures optimal performance for the balloon mission's real-time imaging requirements. 