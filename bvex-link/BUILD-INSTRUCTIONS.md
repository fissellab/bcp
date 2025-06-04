# bvex-link C++ Build Instructions

This document provides complete instructions for building the bvex-link C++ onboard telemetry server from scratch on a fresh Ubuntu/Debian system.

## Quick Start (Automated)

For a completely automated setup on a fresh system:

```bash
# Clone the repository
git clone <repository-url>
cd bvex-link

# Run the automated setup script
./setup-bvex-cpp.sh
```

The script will:
- Install all system dependencies
- Set up vcpkg package manager
- Configure Docker and Redis
- Build the entire project
- Verify the installation

## Manual Installation

If you prefer to install manually or need to troubleshoot:

### Prerequisites

**System Requirements:**
- Ubuntu 20.04+ or Debian 11+
- At least 4GB RAM (for compilation)
- At least 2GB free disk space
- Internet connection

**Required Packages:**
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    docker.io
```

### Step 1: Install vcpkg

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg

# Bootstrap vcpkg
./bootstrap-vcpkg.sh

# Add to your shell profile
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc
echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Step 2: Setup Docker and Redis

```bash
# Add user to docker group
sudo usermod -aG docker $USER

# Log out and log back in, or run:
newgrp docker

# Start Docker service
sudo systemctl start docker
sudo systemctl enable docker

# Create Redis container
docker run -d --name redis -p 6379:6379 redis:7.4
```

### Step 3: Build the Project

```bash
# Navigate to project directory
cd /path/to/bvex-link/onboard-server

# Install dependencies
vcpkg install

# Configure CMake
cmake --preset=debug

# Build
cmake --build build
```

## Project Structure

```
bvex-link/
├── onboard-server/          # Main C++ server
│   ├── src/                 # Source code
│   ├── build/               # Build output (created during build)
│   ├── CMakeLists.txt       # CMake configuration
│   ├── CMakePresets.json    # CMake presets
│   └── vcpkg.json          # Dependencies manifest
├── start-bvex-cpp.sh       # Start system script
├── stop-bvex-cpp.sh        # Stop system script
├── check-bvex-cpp.sh       # Status check script
└── setup-bvex-cpp.sh       # Automated setup script
```

## Dependencies

The project uses the following C++ libraries (managed by vcpkg):

- **boost-asio**: Networking and async I/O
- **nanopb**: Protocol buffer implementation
- **nlohmann-json**: JSON handling

## Usage

### Starting the System
```bash
./start-bvex-cpp.sh
```

This will:
- Start Redis server
- Build the project (if not already built)
- Start the C++ onboard server
- The server listens on UDP ports: 3000, 3001, 3002, 8080
- Forwards telemetry to localhost:9999

### Checking Status
```bash
./check-bvex-cpp.sh
```

### Stopping the System
```bash
./stop-bvex-cpp.sh
```

### Manual Execution
```bash
cd onboard-server
./build/main <target_address> <target_port>

# Example:
./build/main localhost 9999
```

## Server Configuration

The server is configured through CMake definitions:

- **Debug Mode**: Enabled by default with verbose logging
- **Request Server Port**: 8080
- **UDP Listening Ports**: 3000, 3001, 3002, 8080
- **Sample Reception Debug**: Enabled
- **Request Reception Debug**: Enabled

## Troubleshooting

### vcpkg Issues
```bash
# Verify vcpkg installation
which vcpkg
echo $VCPKG_ROOT

# Reinstall dependencies
cd onboard-server
vcpkg install --clean-after-build
```

### Build Issues
```bash
# Clean build
cd onboard-server
rm -rf build/
cmake --preset=debug
cmake --build build
```

### Docker/Redis Issues
```bash
# Check Docker status
sudo systemctl status docker

# Restart Redis container
docker restart redis

# Check Redis logs
docker logs redis
```

### Permission Issues
```bash
# Ensure user is in docker group
groups $USER

# If docker group missing, add and relogin
sudo usermod -aG docker $USER
# Then logout and login again
```

## Development

### Debug Build
The default build is a debug build with:
- Debug symbols included
- Verbose logging enabled
- All debug flags active

### Release Build
For a release build:
```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build-release
```

### Code Generation
Protocol buffer headers are pre-generated. To regenerate:
```bash
# See onboard-server/README.md for nanopb generation commands
```

## System Requirements Verification

Before building, verify your system meets these requirements:

```bash
# Check CMake version (3.26+ required)
cmake --version

# Check GCC version (C++20 support required)
gcc --version

# Check available memory
free -h

# Check available disk space
df -h
```

## Support

For build issues:
1. Check this document first
2. Verify all prerequisites are installed
3. Try the automated setup script
4. Check the project's issue tracker 