#!/bin/bash

# Enhanced start script for BCP with PBoB system
set -e  # Exit on any error

echo "=== BCP Ophiuchus Startup Script ==="
echo "Starting at $(date)"

# Function to print colored output
print_status() {
    echo -e "\033[1;32m[INFO]\033[0m $1"
}

print_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1"
}

print_warning() {
    echo -e "\033[1;33m[WARNING]\033[0m $1"
}

# Check if running as root (needed for hardware access)
if [ "$EUID" -ne 0 ]; then
    print_error "This script must be run as root (use sudo)"
    exit 1
fi

# Store original user info for proper file ownership
ORIGINAL_USER=${SUDO_USER:-$USER}
ORIGINAL_UID=${SUDO_UID:-$(id -u $USER)}
ORIGINAL_GID=${SUDO_GID:-$(id -g $USER)}

print_status "Running as root, original user: $ORIGINAL_USER"

# Check for required system dependencies
print_status "Checking system dependencies..."

if ! command -v cmake &> /dev/null; then
    print_error "cmake not found. Please install cmake first."
    exit 1
fi

# Check for LabJackM library (critical for PBoB system)
if ! ldconfig -p | grep -q "libLabJackM"; then
    print_warning "LabJackM library not found in system. PBoB functionality may not work."
    print_warning "Please ensure LabJackM is properly installed."
fi

# Check submodules (if needed)
if [ ! -d "../bvex-link" ]; then
    print_status "Updating git submodules..."
    sudo -u $ORIGINAL_USER git submodule update --init --recursive
    if [ $? -ne 0 ]; then
        print_error "Failed to update submodules"
        exit 1
    fi
else
    print_status "Submodules already present"
fi

# Generate build files if needed
if [ ! -d "build" ]; then
    print_status "Generating build files..."
    sudo -u $ORIGINAL_USER cmake --preset=default
    if [ $? -ne 0 ]; then
        print_error "Failed to generate build files"
        exit 1
    fi
else
    print_status "Build directory exists"
fi

# Build the project
print_status "Building project..."
sudo -u $ORIGINAL_USER cmake --build build
if [ $? -ne 0 ]; then
    print_error "Build failed"
    exit 1
fi

# Check if executable exists
if [ ! -f "build/main" ]; then
    print_error "Executable build/main not found after build"
    exit 1
fi

# Check if config file exists
if [ ! -f "bcp_Oph.config" ]; then
    print_error "Configuration file bcp_Oph.config not found"
    exit 1
fi

# Test network connectivity for PBoB systems
print_status "Checking PBoB network connectivity..."
PBOB_IPS=$(grep -o '"172\.20\.3\.[0-9]*"' bcp_Oph.config 2>/dev/null || true)
if [ ! -z "$PBOB_IPS" ]; then
    for ip in $PBOB_IPS; do
        clean_ip=$(echo $ip | tr -d '"')
        if ping -c 1 -W 1 $clean_ip &> /dev/null; then
            print_status "PBoB at $clean_ip is reachable"
        else
            print_warning "PBoB at $clean_ip is not reachable"
        fi
    done
fi

# Create log directory if it doesn't exist
if [ ! -d "log" ]; then
    print_status "Creating log directory..."
    mkdir -p log
    chown -R $ORIGINAL_UID:$ORIGINAL_GID log
fi

# Final system checks before starting
print_status "Performing final system checks..."

# Check available disk space
DISK_USAGE=$(df . | awk 'NR==2 {print $5}' | sed 's/%//')
if [ $DISK_USAGE -gt 90 ]; then
    print_warning "Disk usage is at ${DISK_USAGE}%. Consider freeing up space."
fi

# Check if any BCP processes are already running
if pgrep -f "build/main" > /dev/null; then
    print_warning "BCP process may already be running. Consider stopping it first."
    print_status "Running processes:"
    pgrep -f "build/main" -l
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

print_status "All checks passed. Starting BCP..."
print_status "=== Starting BCP Main Application ==="

# Run the main application
./build/main bcp_Oph.config

print_status "BCP application finished at $(date)" 