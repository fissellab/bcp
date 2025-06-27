# Star Camera Downlink Integration Guide

## Summary
This guide shows exactly what we added for the starcam downlink functionality and how to integrate it into your existing build system.

## NEW FILES CREATED
These are completely new files that we created:

1. **`src/starcam_downlink.c`** - Main implementation (587 lines)
2. **`src/starcam_downlink.h`** - Header file (103 lines) 
3. **`test_client.c`** - Test client for ground station (complete working client)

## EXISTING FILES MODIFIED
We made minimal changes to existing files:

### 1. `bcp_Oph.config`
- **Added**: `starcam_downlink:` configuration section (lines 79-89)
- **Purpose**: Configuration parameters for the downlink server

### 2. `include/file_io_Oph.h`
- **Added**: `starcam_downlink_conf` struct definition
- **Added**: Entry in `conf_params` struct
- **Purpose**: Configuration structure definitions

### 3. `src/file_io_Oph.c`
- **Added**: Configuration parsing for starcam_downlink section
- **Added**: Print config for starcam_downlink
- **Purpose**: Read config from file

### 4. `src/camera.c`
- **Added**: 1 line: `#include "starcam_downlink.h"`
- **Added**: 1 line: Function call `notifyImageServer(...)` after image save
- **Purpose**: Notify downlink server when new image is available

### 5. `src/main_Oph.c`
- **Added**: `#include "starcam_downlink.h"`
- **Added**: Initialization block for starcam_downlink (when enabled)
- **Added**: Cleanup block for starcam_downlink (when enabled)
- **Purpose**: Start/stop the downlink server

### 6. `src/cli_Oph.c`
- **Added**: `#include "starcam_downlink.h"`
- **Added**: Status display in bvexcam_status command
- **Added**: New CLI command `starcam_downlink_status`
- **Purpose**: Monitor downlink server status

## HOW TO INTEGRATE

### Option 1: Add to existing build system
Add these lines to your existing build:

```bash
# Compile the starcam downlink
gcc -c -Wall -g -Iinclude src/starcam_downlink.c -o starcam_downlink.o

# Link it with your existing objects
gcc your_existing_objects.o starcam_downlink.o -o main -ljpeg -lpthread -lconfig
```

### Option 2: Test standalone
Our code compiles and works independently:

```bash
# Test that our code compiles
gcc -c -Wall -g -Iinclude src/starcam_downlink.c -o starcam_downlink.o

# Build the test client
gcc -Wall -g -Iinclude test_client.c -o test_client -ljpeg

# Test the client (when your main system is running)
./test_client <your_star_camera_ip>
```

## WHAT THE DOWNLINK DOES

1. **Receives notification** when camera.c saves a new image
2. **Compresses the image** to JPEG (~90% reduction: 2.3MB → ~200KB)
3. **Serves images over UDP** to ground station clients on port 8001
4. **Respects bandwidth limits** (configurable, default 180 kbps)
5. **Provides status monitoring** through CLI commands

## TESTING

1. Make sure `starcam_downlink.enabled = 1` in your config
2. Start your main system: `./main bcp_Oph.config`
3. Check status: Use CLI command `starcam_downlink_status` 
4. Test from ground station: `./test_client <balloon_ip>`

## CONFIGURATION

All settings are in `bcp_Oph.config` under the `starcam_downlink:` section:
- `port = 8001` - UDP server port
- `compression_quality = 60` - JPEG quality (0-100)
- `max_bandwidth_kbps = 180` - Bandwidth limit
- `max_image_queue = 10` - How many recent images to keep

The system is completely self-contained and doesn't interfere with existing functionality. 