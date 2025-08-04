# Quick Start Guide - BCP Saggitarius

## Prerequisites

### Option 1: Automated Installation (Ubuntu/Debian only)

For Ubuntu/Debian systems, use our automated installer:

```bash
git clone https://github.com/fissellab/bcp.git
cd bcp/Sag
./install_dependencies.sh
source ~/.bashrc
./build_and_run.sh     # Build (no sudo)
sudo ./start.sh        # Run (with sudo for GPS)
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

### Two-Step Process

**Step 1: Build (no sudo required)**
```bash
./build_and_run.sh
```

**Step 2: Run (sudo required for GPS access)**
```bash
sudo ./start.sh
```

### What each script does:

**`build_and_run.sh`** (run without sudo):
1. Checks all prerequisites and provides installation instructions if missing
2. Initializes git submodules if needed
3. Installs vcpkg dependencies
4. Configures the project with CMake
5. Builds the project

**`start.sh`** (requires sudo):
1. Checks if application was built
2. Starts the application with GPS device access privileges

### Manual build steps (alternative)

```bash
# 1. Initialize submodules (if not done already)
cd .. && git submodule update --init --recursive && cd Sag

# 2. Configure the project
cmake --preset=default

# 3. Build the project
cmake --build build

# 4. Run the application (with sudo for GPS)
sudo ./start.sh
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

# Build the application (no sudo)
./build_and_run.sh

# Run the application (with sudo for GPS)
sudo ./start.sh
```

## Why Two Separate Scripts?

- **Building** doesn't need root privileges and should preserve user environment variables (like `VCPKG_ROOT`)
- **Running** requires root privileges to access GPS hardware (`/dev/ttyGPS`)
- This separation prevents environment variable issues while maintaining hardware access

## Troubleshooting

- **Don't use `sudo` with `build_and_run.sh`** - Run it as a regular user to preserve environment variables
- **Do use `sudo` with `start.sh`** - Required for GPS device access
- If you get "No such file or directory" errors, ensure submodules are initialized
- If CMake configuration fails, check that `VCPKG_ROOT` is set correctly
- GPS errors are normal if `/dev/ttyGPS` doesn't exist or is in use
- The application works fine even without GPS hardware for testing purposes
- On non-Ubuntu/Debian systems, use manual installation and refer to your system's package manager
- If you get "vcpkg not found" errors when using sudo, run the build script without sudo first 