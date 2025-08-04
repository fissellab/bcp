"""
PR59 Temperature Controller Client for BVEX Ground Station
Fetches telemetry data from the PR59 temperature controller via UDP
"""

import socket
import logging
import time
from typing import Dict, Any, Optional


class PR59Data:
    """Data container for PR59 telemetry"""
    
    def __init__(self):
        self.valid = False
        self.timestamp = 0.0
        
        # PID Parameters
        self.kp = 0.0
        self.ki = 0.0
        self.kd = 0.0
        
        # Temperature readings
        self.temp = 0.0
        self.fet_temp = 0.0
        
        # Power measurements
        self.current = 0.0
        self.voltage = 0.0
        self.power = 0.0
        
        # Status
        self.running = 0
        self.status = "N/A"


class PR59Client:
    """UDP client for PR59 temperature controller telemetry"""
    
    def __init__(self, server_ip="127.0.0.1", server_port=8082, timeout=2.0):
        self.logger = logging.getLogger(__name__)
        self.server_ip = server_ip
        self.server_port = server_port
        self.timeout = timeout
        
        # Current data
        self.current_data = PR59Data()
        self.last_update_time = 0.0
        
        # Connection tracking
        self.connection_attempts = 0
        self.last_connection_attempt = 0.0
        
        # PR59 telemetry channels
        self.channels = [
            "pr59_kp", "pr59_ki", "pr59_kd", "pr59_timestamp",
            "pr59_temp", "pr59_fet_temp", "pr59_current", 
            "pr59_voltage", "pr59_power", "pr59_running", "pr59_status"
        ]
        
        self.logger.info(f"PR59 client initialized - Server: {server_ip}:{server_port}")
    
    def get_telemetry(self, channel_name: str) -> str:
        """Request specific telemetry data from BCP server"""
        try:
            # Create UDP socket
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(self.timeout)
            
            # Send request
            request = channel_name.encode('utf-8')
            sock.sendto(request, (self.server_ip, self.server_port))
            
            # Receive response
            response, addr = sock.recvfrom(1024)
            sock.close()
            
            return response.decode('utf-8').strip()
        
        except socket.timeout:
            return "TIMEOUT"
        except Exception as e:
            self.logger.debug(f"Error getting {channel_name}: {e}")
            return f"ERROR: {e}"
    
    def update_data(self) -> bool:
        """Update all PR59 telemetry data"""
        try:
            current_time = time.time()
            
            # Rate limiting - don't update more than once per second
            if current_time - self.last_update_time < 1.0:
                return self.current_data.valid
            
            self.connection_attempts += 1
            self.last_connection_attempt = current_time
            
            # Fetch all channels
            data_dict = {}
            any_valid = False
            
            for channel in self.channels:
                value = self.get_telemetry(channel)
                if value not in ["TIMEOUT", "N/A"] and not value.startswith("ERROR"):
                    data_dict[channel] = value
                    any_valid = True
                else:
                    data_dict[channel] = None
                    # Store last error for status reporting
                    self._last_error = value
                    # Log unauthorized responses for debugging
                    if "UNAUTHORIZED" in value:
                        self.logger.debug(f"Server returned UNAUTHORIZED for {channel}")
                    elif "ERROR" in value:
                        self.logger.debug(f"Server error for {channel}: {value}")
            
            if any_valid:
                # Update data object
                self.current_data.valid = True
                self.current_data.timestamp = current_time
                
                # PID parameters
                try:
                    self.current_data.kp = float(data_dict.get("pr59_kp", 0.0) or 0.0)
                except (ValueError, TypeError):
                    self.current_data.kp = 0.0
                
                try:
                    self.current_data.ki = float(data_dict.get("pr59_ki", 0.0) or 0.0)
                except (ValueError, TypeError):
                    self.current_data.ki = 0.0
                
                try:
                    self.current_data.kd = float(data_dict.get("pr59_kd", 0.0) or 0.0)
                except (ValueError, TypeError):
                    self.current_data.kd = 0.0
                
                # Temperature readings
                try:
                    self.current_data.temp = float(data_dict.get("pr59_temp", 0.0) or 0.0)
                except (ValueError, TypeError):
                    self.current_data.temp = 0.0
                
                try:
                    self.current_data.fet_temp = float(data_dict.get("pr59_fet_temp", 0.0) or 0.0)
                except (ValueError, TypeError):
                    self.current_data.fet_temp = 0.0
                
                # Power measurements
                try:
                    self.current_data.current = float(data_dict.get("pr59_current", 0.0) or 0.0)
                except (ValueError, TypeError):
                    self.current_data.current = 0.0
                
                try:
                    self.current_data.voltage = float(data_dict.get("pr59_voltage", 0.0) or 0.0)
                except (ValueError, TypeError):
                    self.current_data.voltage = 0.0
                
                try:
                    self.current_data.power = float(data_dict.get("pr59_power", 0.0) or 0.0)
                except (ValueError, TypeError):
                    self.current_data.power = 0.0
                
                # Status
                try:
                    self.current_data.running = int(data_dict.get("pr59_running", 0) or 0)
                except (ValueError, TypeError):
                    self.current_data.running = 0
                
                self.current_data.status = str(data_dict.get("pr59_status", "N/A") or "N/A")
                
                self.last_update_time = current_time
                self.logger.debug("PR59 telemetry updated successfully")
                return True
            else:
                self.current_data.valid = False
                self.logger.debug("No valid PR59 telemetry data received")
                return False
                
        except Exception as e:
            self.logger.error(f"Error updating PR59 data: {e}")
            self.current_data.valid = False
            return False
    
    def get_data(self) -> PR59Data:
        """Get current PR59 data"""
        return self.current_data
    
    def is_connected(self) -> bool:
        """Check if connection is working"""
        return self.current_data.valid and (time.time() - self.last_update_time) < 10.0
    
    def is_server_responding(self) -> bool:
        """Check if server is responding (even if unauthorized)"""
        return hasattr(self, '_last_error') and ("UNAUTHORIZED" in str(self._last_error) or self.current_data.valid)
    
    def get_connection_status(self) -> str:
        """Get detailed connection status"""
        if self.is_connected():
            return "Connected"
        elif time.time() - self.last_connection_attempt < 5.0:
            return "Connecting..."
        else:
            # Check for specific error conditions
            if hasattr(self, '_last_error') and "UNAUTHORIZED" in str(self._last_error):
                return "Unauthorized"
            return "Disconnected"
    
    def cleanup(self):
        """Clean up resources"""
        self.logger.info("PR59 client cleanup completed") 