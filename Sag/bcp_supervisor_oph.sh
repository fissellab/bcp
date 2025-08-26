#!/bin/bash

# BCP Supervisor for Ophiuchus
# Bulletproof supervisor system for stratospheric balloon operations
# 
# Requirements:
# - Starts BCP Ophiuchus on boot
# - Monitors BCP process and restarts on crashes  
# - Sends "OPH_READY" signal to Saggitarius when BCP is operational
# - Handles SIGTERM gracefully for systemctl stop
# - Creates timestamped log files with combined supervisor+BCP output
#
# Control:
# - systemctl start bcp-supervisor-oph
# - systemctl stop bcp-supervisor-oph  
# - systemctl status bcp-supervisor-oph

set -e  # Exit on any error during setup

# Configuration
SCRIPT_DIR="/home/ophiuchus/bcp-mayukh/Oph"
LOG_DIR="/home/ophiuchus/bcp_supervisor_logs"
SAG_IP="172.20.4.170"  # Saggitarius IP address
OPH_READY_PORT=9001
BCP_RESTART_DELAY=10
HEARTBEAT_CHECK_INTERVAL=30
READY_SIGNAL_INTERVAL=30

# Global variables
BCP_PID=""
SUPERVISOR_RUNNING=true
LOG_FILE=""
BCP_READY=false

# Create log directory if it doesn't exist
mkdir -p "$LOG_DIR"

# Generate timestamped log file
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
LOG_FILE="$LOG_DIR/bcp_supervisor_oph_$TIMESTAMP.log"

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

# Function to send OPH_READY signal to Saggitarius
send_ready_signal() {
    echo "OPH_READY" | nc -u -w1 "$SAG_IP" "$OPH_READY_PORT" 2>/dev/null || true
}

# Function to periodically send ready signals
send_ready_signals() {
    while $SUPERVISOR_RUNNING && $BCP_READY; do
        send_ready_signal
        log_message "SUPERVISOR" "Sent OPH_READY signal to Saggitarius ($SAG_IP:$OPH_READY_PORT)"
        sleep "$READY_SIGNAL_INTERVAL"
    done
}

# Signal handlers for graceful shutdown
cleanup_and_exit() {
    log_message "SUPERVISOR" "Received shutdown signal"
    SUPERVISOR_RUNNING=false
    BCP_READY=false
    
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

# Function to start BCP
start_bcp() {
    log_message "SUPERVISOR" "Starting BCP Ophiuchus..."
    
    # Change to BCP directory
    cd "$SCRIPT_DIR"
    
    # Check prerequisites
    if [[ ! -f "build/main" ]]; then
        log_message "SUPERVISOR" "ERROR: build/main executable not found. Please build first."
        return 1
    fi
    
    if [[ ! -f "bcp_Oph.config" ]]; then
        log_message "SUPERVISOR" "ERROR: bcp_Oph.config not found"
        return 1
    fi
    
    # Verify we can run build/main (needs root privileges)
    if [[ $EUID -ne 0 ]]; then
        log_message "SUPERVISOR" "ERROR: Cannot execute build/main - not running as root"
        return 1
    fi
    
    # Start BCP and capture its output
    log_message "SUPERVISOR" "Executing: ./build/main bcp_Oph.config (running as root, EUID=$EUID)"
    
    # Start BCP in background, capturing both stdout and stderr
    ./build/main bcp_Oph.config 2>&1 | log_bcp_output &
    BCP_PID=$!
    
    # Verify BCP started successfully
    sleep 3
    if kill -0 "$BCP_PID" 2>/dev/null; then
        log_message "SUPERVISOR" "BCP started successfully (PID: $BCP_PID)"
        
        # Wait a bit more for BCP to fully initialize (PBOB servers, etc.)
        log_message "SUPERVISOR" "Waiting for BCP to fully initialize..."
        sleep 5
        
        # Mark as ready and start sending signals
        BCP_READY=true
        log_message "SUPERVISOR" "BCP is ready! Starting to send OPH_READY signals to Saggitarius"
        
        # Start background process to send ready signals
        send_ready_signals &
        
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
            BCP_READY=false
            
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
    log_message "SUPERVISOR" "=== BCP Ophiuchus Supervisor Starting ==="
    log_message "SUPERVISOR" "Log file: $LOG_FILE"
    log_message "SUPERVISOR" "Working directory: $SCRIPT_DIR"
    log_message "SUPERVISOR" "Saggitarius IP: $SAG_IP:$OPH_READY_PORT"
    
    # Check if running as root (required for hardware access)
    if [[ $EUID -ne 0 ]]; then
        log_message "SUPERVISOR" "ERROR: Supervisor must run as root for hardware access"
        exit 1
    fi
    
    log_message "SUPERVISOR" "Running as root (EUID=0) - BCP will execute properly"
    
    while $SUPERVISOR_RUNNING; do
        # Start BCP immediately (no waiting for external signals)
        if start_bcp; then
            # Monitor BCP until it crashes or we're told to stop
            monitor_bcp
        else
            log_message "SUPERVISOR" "Failed to start BCP, will retry after delay"
            sleep "$BCP_RESTART_DELAY"
        fi
        
        # If we get here, either BCP monitoring ended or start failed
        if $SUPERVISOR_RUNNING; then
            log_message "SUPERVISOR" "Restarting supervisor cycle..."
        fi
    done
    
    log_message "SUPERVISOR" "Supervisor main loop ended"
}

# Run main function
main "$@"
