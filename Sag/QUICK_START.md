# Quick Start Guide - BCP Saggitarius

## Prerequisites

Make sure you have:
- CMake 3.24+ installed
- vcpkg installed and `VCPKG_ROOT` environment variable set
- GCC compiler
- json-c library (`sudo apt-get install libjson-c-dev`)

## Building and Running

### Option 1: Using the automated script (recommended)

```bash
./build_and_run.sh
```

This script will:
1. Initialize git submodules if needed
2. Install vcpkg dependencies
3. Configure the project with CMake
4. Build the project
5. Ask if you want to run it immediately

### Option 2: Manual build steps

```bash
# 1. Initialize submodules (if not done already)
cd .. && git submodule update --init --recursive && cd Sag

# 2. Configure the project
cmake --preset=default

# 3. Build the project
cmake --build build

# 4. Run the application
./build/main bcp_Sag.config
```

### Clean Build

To start fresh:
```bash
./build_and_run.sh clean
```

## Application Commands

Once running, you can use these commands:
- `start spec` - Start the spectrometer
- `stop spec` - Stop the spectrometer  
- `start gps` - Start GPS logging
- `stop gps` - Stop GPS logging
- `gps status` - Show GPS status
- `print <message>` - Print a message
- `exit` - Exit the program

## Configuration

Edit `bcp_Sag.config` to modify:
- Log file paths
- RFSoC spectrometer settings
- GPS settings
- Data save paths

## Troubleshooting

- If you get "No such file or directory" errors, ensure submodules are initialized
- If CMake configuration fails, check that `VCPKG_ROOT` is set correctly
- GPS errors are normal if `/dev/ttyGPS` doesn't exist or is in use
- The application works fine even without GPS hardware for testing purposes 