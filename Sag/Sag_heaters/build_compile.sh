#!/bin/bash

# Fresh Build Script for BCP Saggitarius
echo "=== BCP Saggitarius Fresh Build Script ==="
echo "This script will clean all build artifacts and compile from scratch."
echo ""

# Check if running as root and warn
if [ "$EUID" -eq 0 ]; then
    echo "WARNING: You are running this script as root (with sudo)."
    echo "   This can cause issues with environment variables like VCPKG_ROOT."
    echo "   Please run without sudo: ./build_compile.sh"
    echo ""
    read -p "Do you want to continue anyway? (y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Exiting. Run without sudo for best results."
        exit 1
    fi
fi

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
            echo "CMake $CMAKE_VERSION found"
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

echo "Step 1: Checking prerequisites..."

# Check prerequisites quickly
MISSING_DEPS=0

if ! command_exists git; then
    echo "Git not found"
    MISSING_DEPS=1
fi

if ! command_exists gcc; then
    echo "GCC not found" 
    MISSING_DEPS=1
fi

if ! check_cmake_version; then
    MISSING_DEPS=1
fi

if [ -z "$VCPKG_ROOT" ] || ! command_exists vcpkg; then
    echo "vcpkg not found or VCPKG_ROOT not set"
    MISSING_DEPS=1
fi

if ! pkg-config --exists json-c 2>/dev/null; then
    echo "json-c library not found"
    MISSING_DEPS=1
fi

if [ $MISSING_DEPS -eq 1 ]; then
    echo ""
    echo "Missing dependencies. Please install them first or run:"
    echo "   ./install_dependencies.sh"
    exit 1
fi

echo "All prerequisites satisfied!"
echo ""

echo "Step 2: Cleaning build artifacts..."
# Remove all build artifacts
echo "  - Removing build directory..."
rm -rf build/

echo "  - Removing old object files..."
rm -f *.o

echo "  - Cleaning vcpkg installed packages..."
rm -rf vcpkg_installed/

echo "Clean complete!"
echo ""

echo "Step 3: Initializing submodules..."
# Check if bvex-link submodule is initialized
if [ ! -d "../bvex-link/bcp-fetch-client" ]; then
    echo "  - Initializing git submodules..."
    cd .. && git submodule update --init --recursive
    cd Sag
else
    echo "  - Submodules already initialized"
fi

echo "Step 4: Installing vcpkg dependencies..."
vcpkg install
if [ $? -ne 0 ]; then
    echo " Error: vcpkg install failed!"
    exit 1
fi

echo "Step 5: Creating necessary directories..."
mkdir -p log

echo "Step 6: Configuring project with CMake..."
cmake --preset=default
if [ $? -ne 0 ]; then
    echo " Error: CMake configuration failed!"
    exit 1
fi

echo "Step 7: Building project..."
cmake --build build
if [ $? -ne 0 ]; then
    echo " Error: Build failed!"
    exit 1
fi

echo "Step 8: Copying executable to root directory..."
cp build/main bcp_Sag
if [ $? -ne 0 ]; then
    echo " Error: Failed to copy executable!"
    exit 1
fi

echo ""
echo " Fresh build completed successfully!"
echo ""
echo " Executable copied to: ./bcp_Sag"
echo " Source executable: ./build/main"
echo "  Config file: bcp_Sag.config"
echo ""
echo " To run the application:"
echo "   sudo ./start.sh"
echo ""
echo " To check system status:"
echo "   ./status.sh"
echo ""
echo " To stop all services:"
echo "   sudo ./stop.sh" 