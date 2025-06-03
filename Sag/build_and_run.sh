#!/bin/bash

# Build and Run script for BCP Saggitarius
echo "=== BCP Saggitarius Build and Run Script ==="

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