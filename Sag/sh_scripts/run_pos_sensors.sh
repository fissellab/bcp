#!/bin/bash

# ===== CONFIGURATION =====
PI_USER="bvex"
PI_HOST="192.168.0.23"
PI_TX_PATH="/home/bvex/pos_sensor_tx"
PI_SSH_KEY="$HOME/.ssh/pi_sensor_key"
LOCAL_RX_EXECUTABLE="./build/pos_sensors_rx"
LOG_DIR="log"
DATA_DIR="/media/saggitarius/T7/position_tracking_data"

# SSH options
SSH_OPTIONS="-i ${PI_SSH_KEY} -o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=10"

# ===== SETUP =====
mkdir -p "$LOG_DIR"
mkdir -p "$DATA_DIR" 2>/dev/null || {
    log_message "Warning: Could not create data directory $DATA_DIR - may need manual creation"
}
SCRIPT_LOG="${LOG_DIR}/run_pos_sensors_$(date +%Y-%m-%d_%H-%M-%S).log"
RX_PID=""

# Check if RX executable exists
if [ ! -f "$LOCAL_RX_EXECUTABLE" ]; then
    echo "Error: RX executable not found at $LOCAL_RX_EXECUTABLE"
    echo "Please build the project first with: ./build_compile.sh"
    exit 1
fi

# ===== LOGGING =====
log_message() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$SCRIPT_LOG"
}

# ===== CLEANUP =====
cleanup() {
    log_message "Shutting down..."
    
    # Stop RX
    if [ -n "$RX_PID" ] && kill -0 "$RX_PID" 2>/dev/null; then
        log_message "Stopping RX program..."
        kill $RX_PID
        wait $RX_PID 2>/dev/null
    fi
    
    # Stop TX on Pi
    log_message "Stopping TX program on Pi..."
    ssh $SSH_OPTIONS ${PI_USER}@${PI_HOST} "sudo killall pos_sensor_tx" 2>/dev/null || true
    
    log_message "Shutdown complete"
    exit 0
}

trap cleanup EXIT INT TERM

# ===== MAIN EXECUTION =====
log_message "Starting position sensor data collection system"

# Start TX on Pi
log_message "Starting TX program on Pi..."
ssh $SSH_OPTIONS ${PI_USER}@${PI_HOST} "cd ~ && sudo nohup ${PI_TX_PATH} > /dev/null 2>&1 &"

if [ $? -ne 0 ]; then
    log_message "ERROR: Failed to start TX program on Pi"
    exit 1
fi

# Wait for TX to be ready
log_message "Waiting for TX to start listening on port 65432..."
for i in {1..30}; do
    if ssh $SSH_OPTIONS ${PI_USER}@${PI_HOST} "netstat -tuln 2>/dev/null | grep -q :65432" 2>/dev/null; then
        log_message "TX is ready"
        break
    fi
    
    if [ $i -eq 30 ]; then
        log_message "ERROR: Timeout waiting for TX to start"
        exit 1
    fi
    
    sleep 1
done

# Additional stabilization delay
log_message "System stabilization delay..."
sleep 3

# Start local RX
log_message "Starting local RX program..."
$LOCAL_RX_EXECUTABLE &
RX_PID=$!

if [ $? -ne 0 ]; then
    log_message "ERROR: Failed to start local RX program"
    exit 1
fi

log_message "RX program started with PID: $RX_PID"
log_message "Data collection system is running - Press Ctrl+C to stop"

# Monitor and display elapsed time
start_time=$(date +%s)
while true; do
    # Check if RX is still running
    if ! kill -0 "$RX_PID" 2>/dev/null; then
        log_message "RX program has stopped"
        break
    fi
    
    # Display elapsed time
    current_time=$(date +%s)
    elapsed=$((current_time - start_time))
    printf "\rData collection time: %02d:%02d:%02d" \
        $((elapsed/3600)) $((elapsed%3600/60)) $((elapsed%60))
    
    sleep 1
done

# Will trigger cleanup via trap