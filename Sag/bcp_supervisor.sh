#!/bin/bash

# BCP Supervisor for Saggitarius
# Bulletproof supervisor system for stratospheric balloon operations
# 
# Requirements:
# - Waits for Ophiuchus "OPH_READY" signal on UDP port 9001
# - Starts BCP only when Oph is confirmed ready
# - Monitors BCP process and restarts on crashes
# - Handles SIGTERM gracefully for systemctl stop
# - Creates timestamped log files with combined supervisor+BCP output
#
# Control:
# - systemctl start bcp-supervisor
# - systemctl stop bcp-supervisor  
# - systemctl status bcp-supervisor

set -e  # Exit on any error during setup

# Configuration
SCRIPT_DIR="/home/mayukh/bcp/Sag"
LOG_DIR="/home/mayukh/bcp_supervisor_logs"
OPH_READY_PORT=9001
BCP_RESTART_DELAY=10
HEARTBEAT_CHECK_INTERVAL=30

# Global variables
BCP_PID=""
SUPERVISOR_RUNNING=true
LOG_FILE=""

# Create log directory if it doesn't exist
mkdir -p "$LOG_DIR"

# Generate timestamped log file
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
LOG_FILE="$LOG_DIR/bcp_supervisor_$TIMESTAMP.log"

# Function to log messages with timestamp
log_message() {
    local level="$1"
    local message="$2"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $level: $message" | tee -a "$LOG_FILE"
}

# Function to log BCP output (no timestamp prefix since BCP adds its own)
log_bcp_output() {
    while IFS= read -r line; do
        echo "$line" | tee -a "$LOG_FILE"
    done
}

# Signal handlers for graceful shutdown
cleanup_and_exit() {
    log_message "SUPERVISOR" "Received shutdown signal"
    SUPERVISOR_RUNNING=false
    
    if [[ -n "$BCP_PID" ]] && kill -0 "$BCP_PID" 2>/dev/null; then
        log_message "SUPERVISOR" "Stopping BCP process (PID: $BCP_PID)"
        
        # Try graceful shutdown first
        kill -TERM "$BCP_PID" 2>/dev/null || true
        
        # Wait up to 10 seconds for graceful shutdown
        for i in {1..10}; do
            if ! kill -0 "$BCP_PID" 2>/dev/null; then
                log_message "SUPERVISOR" "BCP stopped gracefully"
                break
            fi
            sleep 1
        done
        
        # Force kill if still running
        if kill -0 "$BCP_PID" 2>/dev/null; then
            log_message "SUPERVISOR" "Force killing BCP process"
            kill -KILL "$BCP_PID" 2>/dev/null || true
        fi
    fi
    
    log_message "SUPERVISOR" "Supervisor shutdown complete"
    exit 0
}

# Set up signal traps
trap cleanup_and_exit SIGTERM SIGINT

# Function to wait for Ophiuchus ready signal
wait_for_ophiuchus() {
    log_message "SUPERVISOR" "Waiting for Ophiuchus ready signal on port $OPH_READY_PORT..."
    
    while $SUPERVISOR_RUNNING; do
        # Use timeout to avoid hanging indefinitely on nc
        if timeout 10 nc -u -l 0.0.0.0 "$OPH_READY_PORT" 2>/dev/null | grep -q "OPH_READY"; then
            log_message "SUPERVISOR" "Received OPH_READY signal - Ophiuchus is operational!"
            return 0
        fi
        
        # Check if we should still be running
        if ! $SUPERVISOR_RUNNING; then
            return 1
        fi
        
        log_message "SUPERVISOR" "Still waiting for Ophiuchus ready signal..."
        sleep 5
    done
    
    return 1
}

# Function to start BCP
start_bcp() {
    log_message "SUPERVISOR" "Starting BCP Saggitarius..."
    
    # Change to BCP directory
    cd "$SCRIPT_DIR"
    
    # Check prerequisites
    if [[ ! -f "start.sh" ]]; then
        log_message "SUPERVISOR" "ERROR: start.sh not found"
        return 1
    fi
    
    if [[ ! -f "bcp_Sag" ]]; then
        log_message "SUPERVISOR" "ERROR: bcp_Sag executable not found. Please build first with ./build_compile.sh"
        return 1
    fi
    
    if [[ ! -f "bcp_Sag.config" ]]; then
        log_message "SUPERVISOR" "ERROR: bcp_Sag.config not found"
        return 1
    fi
    
    # Verify we can run start.sh (needs root privileges)
    if [[ $EUID -ne 0 ]]; then
        log_message "SUPERVISOR" "ERROR: Cannot execute start.sh - not running as root"
        return 1
    fi
    
    # Start BCP using start.sh and capture its output
    log_message "SUPERVISOR" "Executing: ./start.sh (running as root, EUID=$EUID)"
    
    # Start BCP in background, capturing both stdout and stderr
    ./start.sh 2>&1 | log_bcp_output &
    BCP_PID=$!
    
    # Verify BCP started successfully
    sleep 2
    if kill -0 "$BCP_PID" 2>/dev/null; then
        log_message "SUPERVISOR" "BCP started successfully (PID: $BCP_PID)"
        return 0
    else
        log_message "SUPERVISOR" "ERROR: BCP failed to start"
        BCP_PID=""
        return 1
    fi
}

# Function to monitor BCP process
monitor_bcp() {
    log_message "SUPERVISOR" "Monitoring BCP process (PID: $BCP_PID)"
    
    while $SUPERVISOR_RUNNING; do
        # Check if BCP process is still running
        if [[ -n "$BCP_PID" ]] && kill -0 "$BCP_PID" 2>/dev/null; then
            # BCP is running, sleep and check again
            sleep "$HEARTBEAT_CHECK_INTERVAL"
        else
            # BCP has stopped/crashed
            if [[ -n "$BCP_PID" ]]; then
                log_message "SUPERVISOR" "BCP process (PID: $BCP_PID) has stopped/crashed"
            fi
            
            BCP_PID=""
            
            if $SUPERVISOR_RUNNING; then
                log_message "SUPERVISOR" "Waiting $BCP_RESTART_DELAY seconds before restart attempt..."
                sleep "$BCP_RESTART_DELAY"
                
                if $SUPERVISOR_RUNNING; then
                    log_message "SUPERVISOR" "Attempting to restart BCP..."
                    if start_bcp; then
                        log_message "SUPERVISOR" "BCP restarted successfully"
                    else
                        log_message "SUPERVISOR" "Failed to restart BCP, will retry in $BCP_RESTART_DELAY seconds"
                        sleep "$BCP_RESTART_DELAY"
                    fi
                fi
            fi
        fi
    done
}

# Main supervisor function
main() {
    log_message "SUPERVISOR" "=== BCP Supervisor Starting ==="
    log_message "SUPERVISOR" "Log file: $LOG_FILE"
    log_message "SUPERVISOR" "Working directory: $SCRIPT_DIR"
    log_message "SUPERVISOR" "Ophiuchus ready port: $OPH_READY_PORT"
    
    # Check if running as root (required for GPS access and start.sh)
    if [[ $EUID -ne 0 ]]; then
        log_message "SUPERVISOR" "ERROR: Supervisor must run as root for GPS device access and start.sh execution"
        exit 1
    fi
    
    log_message "SUPERVISOR" "Running as root (EUID=0) - start.sh will execute properly"
    
    while $SUPERVISOR_RUNNING; do
        # Step 1: Wait for Ophiuchus to be ready
        if wait_for_ophiuchus; then
            # Step 2: Start BCP
            if start_bcp; then
                # Step 3: Monitor BCP until it crashes or we're told to stop
                monitor_bcp
            else
                log_message "SUPERVISOR" "Failed to start BCP, will retry after waiting for Oph again"
            fi
        else
            # Supervisor was stopped while waiting for Oph
            break
        fi
        
        # If we get here, either BCP monitoring ended or start failed
        # Loop will restart the whole process (wait for Oph -> start BCP -> monitor)
        if $SUPERVISOR_RUNNING; then
            log_message "SUPERVISOR" "Restarting supervisor cycle..."
        fi
    done
    
    log_message "SUPERVISOR" "Supervisor main loop ended"
}

# Run main function
main "$@"
