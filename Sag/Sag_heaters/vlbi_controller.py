#!/usr/bin/env python3
"""
VLBI Controller Daemon for aquila
Listens for commands from BCP and controls VLBI logging scripts
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
LISTEN_PORT = 8004
LISTEN_IP = "0.0.0.0"  # Listen on all interfaces
VLBI_SCRIPT_PATH = "/home/aquila/casper/4x2_100gbe_2bit/start_vlbi_logging.sh"
LOG_FILE = "/var/log/vlbi_controller.log"
PID_FILE = "/home/aquila/vlbi_controller.pid"  # Changed to home directory to avoid permission issues

# Global variables
vlbi_process = None
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
    global vlbi_process
    
    # Stop VLBI process if running
    if vlbi_process and vlbi_process.poll() is None:
        logging.info("Stopping VLBI process...")
        try:
            vlbi_process.terminate()
            vlbi_process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            vlbi_process.kill()
    
    # Remove PID file
    if os.path.exists(PID_FILE):
        os.remove(PID_FILE)
    
    logging.info("VLBI Controller daemon stopped")
    sys.exit(0)

def get_vlbi_status():
    """Get current VLBI logging status"""
    global vlbi_process
    
    if vlbi_process is None:
        return "stopped"
    
    if vlbi_process.poll() is None:
        return "running"
    else:
        return "stopped"

def start_vlbi():
    """Start VLBI logging script"""
    global vlbi_process
    
    if vlbi_process and vlbi_process.poll() is None:
        return {"status": "error", "message": "VLBI logging already running"}
    
    try:
        # Check if script exists
        if not os.path.exists(VLBI_SCRIPT_PATH):
            return {"status": "error", "message": f"VLBI script not found: {VLBI_SCRIPT_PATH}"}
        
        logging.info("Starting VLBI logging script...")
        vlbi_process = subprocess.Popen(
            [VLBI_SCRIPT_PATH],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True
        )
        
        # Give it a moment to start
        time.sleep(2)
        
        if vlbi_process.poll() is None:
            logging.info("VLBI logging script started successfully")
            return {"status": "success", "message": "VLBI logging started", "pid": vlbi_process.pid}
        else:
            # Process exited immediately
            stdout, stderr = vlbi_process.communicate()
            error_msg = f"VLBI script failed to start. Error: {stderr}"
            logging.error(error_msg)
            return {"status": "error", "message": error_msg}
            
    except Exception as e:
        error_msg = f"Failed to start VLBI script: {str(e)}"
        logging.error(error_msg)
        return {"status": "error", "message": error_msg}

def stop_vlbi():
    """Stop VLBI logging script"""
    global vlbi_process
    
    if vlbi_process is None or vlbi_process.poll() is not None:
        return {"status": "error", "message": "VLBI logging not running"}
    
    try:
        logging.info("Stopping VLBI logging script...")
        
        # First try graceful termination (SIGTERM)
        vlbi_process.terminate()
        
        # Wait up to 10 seconds for graceful shutdown
        try:
            vlbi_process.wait(timeout=10)
            logging.info("VLBI logging script stopped gracefully")
            return {"status": "success", "message": "VLBI logging stopped"}
        except subprocess.TimeoutExpired:
            # Force kill if graceful shutdown failed
            logging.warning("Graceful shutdown timed out, force killing...")
            vlbi_process.kill()
            vlbi_process.wait()
            logging.info("VLBI logging script force killed")
            return {"status": "success", "message": "VLBI logging force stopped"}
            
    except Exception as e:
        error_msg = f"Failed to stop VLBI script: {str(e)}"
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
            if data == "start_vlbi":
                response = start_vlbi()
            elif data == "stop_vlbi":
                response = stop_vlbi()
            elif data == "status":
                status = get_vlbi_status()
                pid = vlbi_process.pid if vlbi_process and vlbi_process.poll() is None else None
                response = {
                    "status": "success", 
                    "vlbi_status": status,
                    "pid": pid,
                    "timestamp": datetime.now().isoformat()
                }
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
            logging.error(f"VLBI Controller already running with PID {old_pid}")
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
    
    logging.info("Starting VLBI Controller daemon...")
    logging.info(f"Listening on {LISTEN_IP}:{LISTEN_PORT}")
    
    try:
        # Create server socket
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((LISTEN_IP, LISTEN_PORT))
        server_socket.listen(5)
        
        logging.info("VLBI Controller daemon started successfully")
        
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