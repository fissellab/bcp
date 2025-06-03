# Quick Start Guide - BCP Saggitarius

## Prerequisites

### Option 1: Automated Installation (Ubuntu/Debian only)

For Ubuntu/Debian systems, use our automated installer:

```bash
git clone https://github.com/fissellab/bcp.git
cd bcp/Sag
./install_dependencies.sh
source ~/.bashrc
./build_and_run.sh
```

This will automatically install:
- CMake 3.24+
- vcpkg and set up environment variables
- GCC compiler
- json-c library
- Git

### Option 2: Manual Installation

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
1. Check all prerequisites and provide installation instructions if missing
2. Initialize git submodules if needed
3. Install vcpkg dependencies
4. Configure the project with CMake
5. Build the project
6. Ask if you want to run it immediately

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

## Complete Setup Example

For a brand new Ubuntu/Debian system:

```bash
# Clone the repository
git clone https://github.com/fissellab/bcp.git
cd bcp/Sag

# Install all dependencies automatically
./install_dependencies.sh

# Reload environment
source ~/.bashrc

# Build and run
./build_and_run.sh
```

## Troubleshooting

- If you get "No such file or directory" errors, ensure submodules are initialized
- If CMake configuration fails, check that `VCPKG_ROOT` is set correctly
- GPS errors are normal if `/dev/ttyGPS` doesn't exist or is in use
- The application works fine even without GPS hardware for testing purposes
- On non-Ubuntu/Debian systems, use manual installation and refer to your system's package manager 