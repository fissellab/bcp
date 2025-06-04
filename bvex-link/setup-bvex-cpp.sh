#!/bin/bash

# bvex-link C++ Build Setup Script
# This script sets up all dependencies and builds the C++ onboard server from scratch

set -e  # Exit on any error

SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="$(cd "$SCRIPT_DIR" && pwd)"
ONBOARD_SERVER_DIR="$PROJECT_ROOT/onboard-server"

echo "========================================="
echo "bvex-link C++ Build Setup"
echo "========================================="
echo "Project root: $PROJECT_ROOT"
echo ""

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if vcpkg is properly installed
check_vcpkg() {
    if command_exists vcpkg && [ -n "$VCPKG_ROOT" ] && [ -d "$VCPKG_ROOT" ]; then
        echo "vcpkg found at: $VCPKG_ROOT"
        return 0
    else
        return 1
    fi
}

# Step 1: Install system dependencies
echo "Step 1: Checking system dependencies..."
MISSING_DEPS=()

if ! command_exists cmake; then
    MISSING_DEPS+=("cmake")
fi

if ! command_exists git; then
    MISSING_DEPS+=("git")
fi

if ! command_exists curl; then
    MISSING_DEPS+=("curl")
fi

if ! command_exists pkg-config; then
    MISSING_DEPS+=("pkg-config")
fi

if ! command_exists docker; then
    MISSING_DEPS+=("docker.io")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo "Installing missing system dependencies: ${MISSING_DEPS[*]}"
    sudo apt update
    sudo apt install -y "${MISSING_DEPS[@]}" build-essential ninja-build zip unzip tar
else
    echo "All system dependencies are installed."
fi

# Step 2: Setup vcpkg
echo ""
echo "Step 2: Setting up vcpkg..."

if ! check_vcpkg; then
    echo "vcpkg not found or not properly configured. Installing..."
    
    # Determine vcpkg installation directory
    if [ -z "$VCPKG_ROOT" ]; then
        VCPKG_INSTALL_DIR="$HOME/vcpkg"
        echo "VCPKG_ROOT not set. Installing to: $VCPKG_INSTALL_DIR"
    else
        VCPKG_INSTALL_DIR="$VCPKG_ROOT"
        echo "Using existing VCPKG_ROOT: $VCPKG_INSTALL_DIR"
    fi
    
    # Clone and setup vcpkg if directory doesn't exist
    if [ ! -d "$VCPKG_INSTALL_DIR" ]; then
        echo "Cloning vcpkg..."
        git clone https://github.com/Microsoft/vcpkg.git "$VCPKG_INSTALL_DIR"
    fi
    
    cd "$VCPKG_INSTALL_DIR"
    
    # Bootstrap vcpkg if not already done
    if [ ! -f "./vcpkg" ]; then
        echo "Bootstrapping vcpkg..."
        ./bootstrap-vcpkg.sh
    fi
    
    # Set VCPKG_ROOT for current session
    export VCPKG_ROOT="$VCPKG_INSTALL_DIR"
    
    # Add vcpkg to PATH for current session
    export PATH="$VCPKG_ROOT:$PATH"
    
    echo ""
    echo "IMPORTANT: Add these lines to your ~/.bashrc for permanent setup:"
    echo "export VCPKG_ROOT=\"$VCPKG_INSTALL_DIR\""
    echo "export PATH=\"\$VCPKG_ROOT:\$PATH\""
    echo ""
    
    cd "$PROJECT_ROOT"
else
    echo "vcpkg is already properly configured."
fi

# Step 3: Setup Docker and Redis
echo ""
echo "Step 3: Setting up Redis with Docker..."

if ! command_exists docker; then
    echo "Error: Docker is required but not installed properly."
    exit 1
fi

# Check if user is in docker group
if ! groups | grep -q docker; then
    echo "Adding current user to docker group..."
    sudo usermod -aG docker "$USER"
    echo "WARNING: You may need to log out and log back in for docker group changes to take effect."
    echo "If docker commands fail, try running this script again after relogging."
fi

# Start docker service if not running
if ! sudo systemctl is-active --quiet docker; then
    echo "Starting Docker service..."
    sudo systemctl start docker
    sudo systemctl enable docker
fi

# Setup Redis container
echo "Setting up Redis container..."
if ! sudo docker ps -a | grep -q "redis"; then
    echo "Creating Redis container..."
    sudo docker run -d --name redis -p 6379:6379 redis:7.4
else
    echo "Redis container already exists. Starting if stopped..."
    sudo docker start redis || true
fi

# Verify Redis is running
sleep 2
if sudo docker ps | grep -q redis; then
    echo "Redis is running on port 6379"
else
    echo "Warning: Redis may not be running properly"
fi

# Step 4: Build the C++ project
echo ""
echo "Step 4: Building C++ onboard server..."

if [ ! -d "$ONBOARD_SERVER_DIR" ]; then
    echo "Error: onboard-server directory not found at $ONBOARD_SERVER_DIR"
    exit 1
fi

cd "$ONBOARD_SERVER_DIR"

# Install vcpkg dependencies
echo "Installing vcpkg dependencies..."
if ! "$VCPKG_ROOT/vcpkg" install; then
    echo "Error: Failed to install vcpkg dependencies"
    exit 1
fi

# Configure with CMake
echo "Configuring with CMake..."
if ! cmake --preset=debug; then
    echo "Error: CMake configuration failed"
    exit 1
fi

# Build the project
echo "Building the project..."
if ! cmake --build build; then
    echo "Error: Build failed"
    exit 1
fi

# Verify build output
if [ ! -f "build/main" ]; then
    echo "Error: Build completed but executable not found"
    exit 1
fi

echo ""
echo "========================================="
echo "BUILD COMPLETED SUCCESSFULLY!"
echo "========================================="
echo ""
echo "Project built at: $ONBOARD_SERVER_DIR/build/main"
echo "Executable size: $(ls -lh build/main | awk '{print $5}')"
echo ""
echo "Redis status:"
sudo docker ps | grep redis || echo "Redis not running"
echo ""
echo "To start the system: ./start-bvex-cpp.sh"
echo "To check status: ./check-bvex-cpp.sh"
echo "To stop system: ./stop-bvex-cpp.sh"
echo ""

if [ -n "$BASH_VERSION" ] && [[ "$VCPKG_ROOT" != *"$HOME/.bashrc"* ]]; then
    echo "REMINDER: Add these lines to ~/.bashrc for permanent vcpkg setup:"
    echo "export VCPKG_ROOT=\"$VCPKG_ROOT\""
    echo "export PATH=\"\$VCPKG_ROOT:\$PATH\""
fi

echo "Setup complete!" 