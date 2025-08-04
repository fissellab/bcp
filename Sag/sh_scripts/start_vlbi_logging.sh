#!/bin/bash

# Configuration for VLBI data logging
HOSTNAME="172.20.4.172"     # RFSoC IP address
INTERFACE="enp1s0f0np0"    # Network interface for 100 GbE
ADC_CHAN=0                 # ADC channel to use
NUMPKT=256                 # Number of packets per batch
DELAY=15                   # Delay before starting catcher

# Virtual environment path
VENV_PATH="/home/aquila/casper/cfpga_venv"
PYTHON_PATH="$VENV_PATH/bin/python3"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Function to cleanup processes
cleanup() {
    echo -e "\nCleaning up processes..."
    # Kill catcher first (it will send 'close' to listener)
    if [ ! -z "$CATCHER_PID" ]; then
        kill $CATCHER_PID 2>/dev/null
    fi
    # Kill listener if still running
    if [ ! -z "$LISTENER_PID" ]; then
        kill $LISTENER_PID 2>/dev/null
    fi
    exit 0
}

# Check root privileges
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root for raw socket access"
    exit 1
fi

# Check virtual environment
if [ ! -f "$PYTHON_PATH" ]; then
    echo "Error: Virtual environment Python not found at $PYTHON_PATH"
    exit 1
fi

# Check network interface
if ! ip link show "$INTERFACE" >/dev/null 2>&1; then
    echo "Error: Network interface $INTERFACE does not exist"
    echo "Available interfaces:"
    ip link show | grep -E '^[0-9]+:' | cut -d: -f2
    exit 1
fi

# Create VLBI data directory if needed
VLBI_DATA_DIR="/mnt/vlbi_data"
if [ ! -d "$VLBI_DATA_DIR" ]; then
    echo "Creating VLBI data directory at $VLBI_DATA_DIR"
    mkdir -p "$VLBI_DATA_DIR"
    chmod 777 "$VLBI_DATA_DIR"
fi

echo "Using Python from: $PYTHON_PATH"

# Set up trap for cleanup
trap cleanup SIGINT SIGTERM

echo "Starting VLBI data logging listener..."
cd "$SCRIPT_DIR"
$PYTHON_PATH "$SCRIPT_DIR/vlbi_data_logging_listener.py" $HOSTNAME -n $NUMPKT -a $ADC_CHAN &
LISTENER_PID=$!

# Check if listener started successfully
sleep 2
if ! kill -0 $LISTENER_PID 2>/dev/null; then
    echo "Error: Listener failed to start"
    cleanup
    exit 1
fi

echo "Waiting $DELAY seconds before starting catcher..."
sleep $DELAY

echo "Starting packet catcher..."
$PYTHON_PATH "$SCRIPT_DIR/tut_100g_catcher.py" $INTERFACE &
CATCHER_PID=$!

# Wait for catcher to finish (it will be terminated by Ctrl+C)
wait $CATCHER_PID
cleanup 