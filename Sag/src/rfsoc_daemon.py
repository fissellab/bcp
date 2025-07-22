#!/usr/bin/env python3
"""
RFSoC Clock Control Daemon for casper@172.20.3.12
Listens for commands from BCP and controls RFSoC clock configuration
"""

import socket
import subprocess
import json
import threading
import time
import logging
import signal
import sys
import os
from datetime import datetime

# Configuration
LISTEN_PORT = 8005
LISTEN_IP = "0.0.0.0"  # Listen on all interfaces
CLOCK_SCRIPT_PATH = "/home/casper/clock_setup.sh"
LOG_FILE = "/var/log/rfsoc_daemon.log"
PID_FILE = "/home/casper/rfsoc_daemon.pid"

# Global variables
server_socket = None
running = True

def setup_logging():
    """Setup logging configuration"""
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(LOG_FILE),
            logging.StreamHandler()
        ]
    )

def signal_handler(signum, frame):
    """Handle shutdown signals"""
    global running
    logging.info(f"Received signal {signum}, shutting down...")
    running = False
    if server_socket:
        server_socket.close()
    cleanup_and_exit()

def cleanup_and_exit():
    """Cleanup and exit"""
    # Remove PID file
    if os.path.exists(PID_FILE):
        os.remove(PID_FILE)
    
    logging.info("RFSoC Clock daemon stopped")
    sys.exit(0)

def configure_clock():
    """Execute clock configuration script"""
    try:
        # Check if script exists
        if not os.path.exists(CLOCK_SCRIPT_PATH):
            return {"status": "error", "message": f"Clock script not found: {CLOCK_SCRIPT_PATH}"}
        
        # Check if script is executable
        if not os.access(CLOCK_SCRIPT_PATH, os.X_OK):
            return {"status": "error", "message": f"Clock script is not executable: {CLOCK_SCRIPT_PATH}"}
        
        logging.info("Starting RFSoC clock configuration...")
        
        # Execute the script and capture output
        process = subprocess.run(
            [CLOCK_SCRIPT_PATH],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            timeout=30  # 30-second timeout
        )
        
        # Get script output
        stdout_output = process.stdout.strip() if process.stdout else ""
        stderr_output = process.stderr.strip() if process.stderr else ""
        
        if process.returncode == 0:
            # Success
            logging.info("RFSoC clock configuration completed successfully")
            return {
                "status": "success", 
                "message": "RFSoC clock configured successfully",
                "output": stdout_output,
                "timestamp": datetime.now().isoformat()
            }
        else:
            # Script failed
            error_msg = f"Clock script failed with exit code {process.returncode}"
            logging.error(error_msg)
            if stderr_output:
                error_msg += f". Error: {stderr_output}"
            
            return {
                "status": "error", 
                "message": error_msg,
                "output": stdout_output,
                "error": stderr_output
            }
            
    except subprocess.TimeoutExpired:
        error_msg = "Clock configuration script timed out (30s)"
        logging.error(error_msg)
        return {"status": "error", "message": error_msg}
        
    except Exception as e:
        error_msg = f"Failed to execute clock script: {str(e)}"
        logging.error(error_msg)
        return {"status": "error", "message": error_msg}

def get_clock_status():
    """Get basic clock configuration status"""
    try:
        # Check if script exists and is accessible
        script_exists = os.path.exists(CLOCK_SCRIPT_PATH)
        script_executable = os.access(CLOCK_SCRIPT_PATH, os.X_OK) if script_exists else False
        
        return {
            "status": "success",
            "script_available": script_exists,
            "script_executable": script_executable,
            "script_path": CLOCK_SCRIPT_PATH,
            "timestamp": datetime.now().isoformat()
        }
        
    except Exception as e:
        error_msg = f"Failed to get clock status: {str(e)}"
        logging.error(error_msg)
        return {"status": "error", "message": error_msg}

def handle_client(client_socket, client_address):
    """Handle individual client connection"""
    logging.info(f"Client connected from {client_address}")
    
    try:
        while True:
            # Receive data
            data = client_socket.recv(1024).decode('utf-8').strip()
            if not data:
                break
            
            logging.info(f"Received command: {data}")
            
            # Process command
            if data == "configure_clock":
                response = configure_clock()
            elif data == "clock_status":
                response = get_clock_status()
            elif data == "ping":
                response = {"status": "success", "message": "pong"}
            else:
                response = {"status": "error", "message": f"Unknown command: {data}"}
            
            # Send response
            response_json = json.dumps(response) + "\n"
            client_socket.send(response_json.encode('utf-8'))
            
    except Exception as e:
        logging.error(f"Error handling client {client_address}: {str(e)}")
    finally:
        client_socket.close()
        logging.info(f"Client {client_address} disconnected")

def main():
    """Main daemon function"""
    global server_socket, running
    
    setup_logging()
    
    # Check if already running
    if os.path.exists(PID_FILE):
        try:
            with open(PID_FILE, 'r') as f:
                old_pid = int(f.read().strip())
            # Check if process is still running
            os.kill(old_pid, 0)
            logging.error(f"RFSoC Clock daemon already running with PID {old_pid}")
            sys.exit(1)
        except (OSError, ValueError):
            # PID file exists but process is dead, remove it
            os.remove(PID_FILE)
    
    # Write current PID
    with open(PID_FILE, 'w') as f:
        f.write(str(os.getpid()))
    
    # Setup signal handlers
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    
    logging.info("Starting RFSoC Clock daemon...")
    logging.info(f"Listening on {LISTEN_IP}:{LISTEN_PORT}")
    logging.info(f"Clock script path: {CLOCK_SCRIPT_PATH}")
    
    try:
        # Create server socket
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((LISTEN_IP, LISTEN_PORT))
        server_socket.listen(5)
        
        logging.info("RFSoC Clock daemon started successfully")
        
        while running:
            try:
                # Accept client connection
                client_socket, client_address = server_socket.accept()
                
                # Handle client in separate thread
                client_thread = threading.Thread(
                    target=handle_client,
                    args=(client_socket, client_address)
                )
                client_thread.daemon = True
                client_thread.start()
                
            except socket.error as e:
                if running:  # Only log if we're not shutting down
                    logging.error(f"Socket error: {str(e)}")
                break
                
    except Exception as e:
        logging.error(f"Server error: {str(e)}")
    finally:
        cleanup_and_exit()

if __name__ == "__main__":
    main() 