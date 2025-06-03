#!/bin/bash

# Build and Run script for BCP Saggitarius
echo "=== BCP Saggitarius Build and Run Script ==="
echo "Checking prerequisites..."

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check CMake version
check_cmake_version() {
    if command_exists cmake; then
        CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
        CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
        CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)
        
        if [ "$CMAKE_MAJOR" -gt 3 ] || [ "$CMAKE_MAJOR" -eq 3 -a "$CMAKE_MINOR" -ge 24 ]; then
            echo "✅ CMake $CMAKE_VERSION found"
            return 0
        else
            echo "❌ CMake version $CMAKE_VERSION found, but 3.24+ required"
            return 1
        fi
    else
        echo "❌ CMake not found"
        return 1
    fi
}

# Check prerequisites
MISSING_DEPS=0

# Check Git
if command_exists git; then
    echo "✅ Git found"
else
    echo "❌ Git not found"
    echo "   Install with: sudo apt-get install git"
    MISSING_DEPS=1
fi

# Check GCC
if command_exists gcc; then
    echo "✅ GCC found"
else
    echo "❌ GCC not found"
    echo "   Install with: sudo apt-get install gcc"
    MISSING_DEPS=1
fi

# Check CMake
if ! check_cmake_version; then
    echo "   Install CMake 3.24+ from: https://apt.kitware.com/"
    MISSING_DEPS=1
fi

# Check vcpkg
if [ -n "$VCPKG_ROOT" ] && command_exists vcpkg; then
    echo "✅ vcpkg found at $VCPKG_ROOT"
else
    echo "❌ vcpkg not found or VCPKG_ROOT not set"
    echo "   Install from: https://learn.microsoft.com/en-us/vcpkg/get_started/get_started"
    echo "   Then set VCPKG_ROOT environment variable"
    MISSING_DEPS=1
fi

# Check json-c library
if pkg-config --exists json-c 2>/dev/null; then
    echo "✅ json-c library found"
else
    echo "❌ json-c library not found"
    echo "   Install with: sudo apt-get install libjson-c-dev"
    MISSING_DEPS=1
fi

# If missing dependencies, provide installation guide and exit
if [ $MISSING_DEPS -eq 1 ]; then
    echo ""
    echo "⚠️  Missing dependencies found. Please install them first:"
    echo ""
    echo "Quick install commands for Ubuntu/Debian:"
    echo "  sudo apt-get update"
    echo "  sudo apt-get install git gcc libjson-c-dev"
    echo ""
    echo "For CMake 3.24+:"
    echo "  wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null"
    echo "  sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'"
    echo "  sudo apt-get update && sudo apt-get install cmake"
    echo ""
    echo "For vcpkg:"
    echo "  git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg"
    echo "  cd ~/vcpkg && ./bootstrap-vcpkg.sh"
    echo "  echo 'export VCPKG_ROOT=~/vcpkg' >> ~/.bashrc"
    echo "  source ~/.bashrc"
    echo ""
    echo "After installing dependencies, run this script again."
    exit 1
fi

echo "✅ All prerequisites satisfied!"
echo ""

# Check if bvex-link submodule is initialized
if [ ! -d "../bvex-link/bcp-fetch-client" ]; then
    echo "Initializing git submodules..."
    cd .. && git submodule update --init --recursive
    cd Sag
fi

# Install vcpkg dependencies if needed
if [ ! -d "vcpkg_installed" ]; then
    echo "Installing vcpkg dependencies..."
    vcpkg install
fi

# Create log directory if it doesn't exist
mkdir -p log

# Clean and rebuild if requested
if [ "$1" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

# Configure build if needed
if [ ! -f "build/Makefile" ]; then
    echo "Configuring project with CMake..."
    cmake --preset=default
    if [ $? -ne 0 ]; then
        echo "Error: CMake configuration failed!"
        exit 1
    fi
fi

# Build the project
echo "Building project..."
cmake --build build
if [ $? -ne 0 ]; then
    echo "Error: Build failed!"
    exit 1
fi

echo "Build successful!"
echo "Executable: ./build/main"
echo "Config file: bcp_Sag.config"
echo ""
echo "To run the application:"
echo "  ./build/main bcp_Sag.config"
echo ""
echo "Available commands in the application:"
echo "  - start spec     : Start the spectrometer"
echo "  - stop spec      : Stop the spectrometer"
echo "  - start gps      : Start GPS logging"
echo "  - stop gps       : Stop GPS logging"
echo "  - gps status     : Show GPS status"
echo "  - print <msg>    : Print a message"
echo "  - exit           : Exit the program"
echo ""

# Ask if user wants to run it now
read -p "Do you want to run the application now? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Starting BCP Saggitarius..."
    ./build/main bcp_Sag.config
fi 