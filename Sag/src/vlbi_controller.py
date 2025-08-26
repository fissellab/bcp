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
import re
from datetime import datetime

# Configuration
LISTEN_PORT = 8004
LISTEN_IP = "0.0.0.0"  # Listen on all interfaces
VLBI_SCRIPT_PATH = "/home/aquila/casper/4x2_100gbe_2bit/start_vlbi_logging.sh"
LOG_FILE = "/var/log/vlbi_controller.log"
PID_FILE = "/home/aquila/vlbi_controller.pid"

# Global variables
vlbi_process = None
server_socket = None
running = True
vlbi_status = {
    "stage": "stopped",
    "packets_captured": 0,
    "data_size_mb": 0.0,
    "pps_counter": None,
    "error_count": 0,
    "last_update": None,
    "connection_status": "disconnected",
    "recent_logs": []
}
connected_clients = []  # Track clients for status broadcasting

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
    PID_FILE = "/home/aquila/vlbi_controller.pid"
    if os.path.exists(PID_FILE):
        os.remove(PID_FILE)
    
    logging.info("VLBI Controller daemon stopped")
    sys.exit(0)

def monitor_vlbi_output():
    """Monitor VLBI script output in real-time and parse status"""
    global vlbi_process, vlbi_status
    
    if vlbi_process is None:
        return
    
    try:
        for line in iter(vlbi_process.stdout.readline, ''):
            if not line:
                break
                
            line = line.strip()
            if not line:
                continue
                
            # Log the output
            logging.info(f"VLBI: {line}")
            
            # Add to recent logs (keep last 50 lines)
            vlbi_status["recent_logs"].append({
                "timestamp": datetime.now().isoformat(),
                "message": line
            })
            if len(vlbi_status["recent_logs"]) > 50:
                vlbi_status["recent_logs"].pop(0)
            
            # Parse status information from output
            parse_vlbi_output(line)
            
            # Broadcast status to connected clients
            broadcast_status_update(line)
            
    except Exception as e:
        logging.error(f"Error monitoring VLBI output: {e}")
    finally:
        # Process ended
        vlbi_status["stage"] = "stopped"
        vlbi_status["connection_status"] = "disconnected"
        vlbi_status["last_update"] = datetime.now().isoformat()

def parse_vlbi_output(line):
    """Parse VLBI output line and extract status information"""
    global vlbi_status
    
    # Update last update time
    vlbi_status["last_update"] = datetime.now().isoformat()
    
    # Parse different types of status lines
    if "Connecting to" in line:
        vlbi_status["stage"] = "connecting"
        vlbi_status["connection_status"] = "connecting"
    elif "Programming FPGA" in line:
        vlbi_status["stage"] = "programming"
    elif "Starting VLBI data capture" in line:
        vlbi_status["stage"] = "capturing"
        vlbi_status["connection_status"] = "connected"
    elif "Packets:" in line:
        # Extract packet count: "Packets: 15420 (123.4/s)"
        try:
            match = re.search(r'Packets:\s*(\d+)', line)
            if match:
                vlbi_status["packets_captured"] = int(match.group(1))
        except:
            pass
    elif "Data size:" in line:
        # Extract data size: "Data size: 128.5 MB"
        try:
            match = re.search(r'Data size:\s*([\d.]+)\s*MB', line)
            if match:
                vlbi_status["data_size_mb"] = float(match.group(1))
        except:
            pass
    elif "Current VDIF time:" in line:
        # Extract PPS counter info
        try:
            match = re.search(r'(\d{4}-\d{2}-\d{2})', line)
            if match:
                vlbi_status["connection_status"] = "capturing"
        except:
            pass
    elif "Error" in line or "error" in line:
        vlbi_status["error_count"] += 1
    elif "Failed" in line or "failed" in line:
        vlbi_status["error_count"] += 1
        if vlbi_status["stage"] != "stopped":
            vlbi_status["stage"] = "error"

def broadcast_status_update(log_line):
    """Broadcast real-time status update to connected clients"""
    global connected_clients
    
    if not connected_clients:
        return
        
    status_update = {
        "type": "vlbi_status_update",
        "timestamp": datetime.now().isoformat(),
        "stage": vlbi_status["stage"],
        "packets_captured": vlbi_status["packets_captured"],
        "data_size_mb": vlbi_status["data_size_mb"],
        "connection_status": vlbi_status["connection_status"],
        "error_count": vlbi_status["error_count"],
        "log_line": log_line
    }
    
    # Send to all connected clients
    message = json.dumps(status_update) + "\n"
    for client in connected_clients[:]:  # Copy list to avoid modification during iteration
        try:
            client.send(message.encode('utf-8'))
        except:
            # Remove disconnected clients
            connected_clients.remove(client)

def get_vlbi_status():
    """Get current VLBI logging status"""
    global vlbi_process
    
    if vlbi_process is None:
        return "stopped"
    
    if vlbi_process.poll() is None:
        return "running"
    else:
        return "stopped"

def start_vlbi(ssd_id=1):
    """Start VLBI logging script with real-time output capture"""
    global vlbi_process
    
    if vlbi_process and vlbi_process.poll() is None:
        return {"status": "error", "message": "VLBI logging already running"}
    
    try:
        # Check if script exists
        if not os.path.exists(VLBI_SCRIPT_PATH):
            return {"status": "error", "message": f"VLBI script not found: {VLBI_SCRIPT_PATH}"}
        
        # Validate SSD ID
        if ssd_id not in [1, 2]:
            return {"status": "error", "message": f"Invalid SSD ID: {ssd_id}. Must be 1 or 2."}
        
        logging.info(f"Starting VLBI logging script with SSD {ssd_id}...")
        
        # Start process with real-time output capture, passing SSD parameter
        vlbi_process = subprocess.Popen(
            ['sudo', VLBI_SCRIPT_PATH, str(ssd_id)],  # Pass SSD ID as argument
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout
            universal_newlines=True,
            bufsize=1  # Line buffered for real-time output
        )
        
        # Give it a moment to start
        time.sleep(2)
        
        if vlbi_process.poll() is None:
            logging.info(f"VLBI logging script started successfully with SSD {ssd_id}")
            
            # Start output monitoring thread
            output_thread = threading.Thread(target=monitor_vlbi_output, daemon=True)
            output_thread.start()
            
            return {"status": "success", "message": f"VLBI logging started on SSD {ssd_id}", "pid": vlbi_process.pid}
        else:
            # Process exited immediately - get output
            stdout, _ = vlbi_process.communicate()
            error_msg = f"VLBI script failed to start. Output: {stdout}"
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
    """Handle individual client connection with enhanced status support"""
    global connected_clients
    
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
                response = start_vlbi()  # Default to SSD 1
            elif data == "start_vlbi_1":
                response = start_vlbi(1)  # Explicitly use SSD 1
            elif data == "start_vlbi_2":
                response = start_vlbi(2)  # Explicitly use SSD 2
            elif data == "start_vlbi_stream":
                # Add client to streaming list
                if client_socket not in connected_clients:
                    connected_clients.append(client_socket)
                response = start_vlbi()
                response["streaming"] = True
            elif data == "stop_vlbi":
                response = stop_vlbi()
            elif data == "status" or data == "get_vlbi_status":
                status = get_vlbi_status()
                pid = vlbi_process.pid if vlbi_process and vlbi_process.poll() is None else None
                response = {
                    "status": "success", 
                    "vlbi_status": status,
                    "pid": pid,
                    "timestamp": datetime.now().isoformat(),
                    "detailed_status": vlbi_status
                }
            elif data == "get_vlbi_logs":
                response = {
                    "status": "success",
                    "logs": vlbi_status["recent_logs"][-20:],  # Last 20 log entries
                    "timestamp": datetime.now().isoformat()
                }
            elif data == "ping":
                response = {"status": "success", "message": "pong"}
            elif data == "stop_stream":
                # Remove client from streaming list
                if client_socket in connected_clients:
                    connected_clients.remove(client_socket)
                response = {"status": "success", "message": "Streaming stopped"}
            else:
                response = {"status": "error", "message": f"Unknown command: {data}"}
            
            # Send response
            response_json = json.dumps(response) + "\n"
            try:
                client_socket.send(response_json.encode('utf-8'))
                logging.info(f"Response sent successfully for command: {data}")
            except BrokenPipeError:
                logging.warning(f"Client disconnected before response could be sent for command: {data}")
                break
            except Exception as e:
                logging.error(f"Error sending response for command {data}: {e}")
                break
            
            # For streaming commands, keep connection open
            if data == "start_vlbi_stream":
                # Keep connection alive for streaming
                continue
                
            # For non-streaming commands, close connection after response
            break
            
    except Exception as e:
        logging.error(f"Error handling client {client_address}: {str(e)}")
    finally:
        # Remove from streaming clients
        if client_socket in connected_clients:
            connected_clients.remove(client_socket)
        client_socket.close()
        logging.info(f"Client {client_address} disconnected")

def main():
    """Main daemon function"""
    global server_socket, running
    
    setup_logging()
    
    # Simple PID file check (like the original)
    PID_FILE = "/home/aquila/vlbi_controller.pid"
    
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