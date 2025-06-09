"""
BCP Spectrometer UDP Client
Fetches spectrum data from the BCP Spectrometer Server
"""

import socket
import time
import logging
from typing import Optional, Dict
from dataclasses import dataclass
from src.data import DataRateTracker


@dataclass
class SpectrumData:
    """Data class for spectrum data"""
    type: str
    timestamp: float
    points: int
    data: list
    freq_start: Optional[float] = None
    freq_end: Optional[float] = None
    baseline: Optional[float] = None
    valid: bool = True


class BCPSpectrometerClient:
    """Client for BCP Spectrometer UDP Server"""
    
    def __init__(self, server_ip: str = "100.70.234.8", server_port: int = 8081, timeout: float = 5.0):
        self.server_ip = server_ip
        self.server_port = server_port
        self.timeout = timeout
        self.last_request_time = 0.0
        self.logger = logging.getLogger(__name__)
        self.connected = False
        self.active_spectrometer_type = 'STANDARD'  # Assume STANDARD initially
        self.socket = None  # Persistent socket for reuse
        
        # Data rate tracking
        self.data_rate_tracker = DataRateTracker(window_seconds=30)
        self.total_bytes_received = 0
        
    def _send_request(self, request: str) -> Optional[str]:
        """Send UDP request and return response"""
        # Create socket if not exists or if previous connection failed
        if self.socket is None:
            try:
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self.socket.settimeout(self.timeout)
            except Exception as e:
                self.logger.error(f"Failed to create socket: {e}")
                self.connected = False
                return None
        
        try:
            self.socket.sendto(request.encode('utf-8'), (self.server_ip, self.server_port))
            response_data = self.socket.recv(32768)
            response = response_data.decode('utf-8')
            self.last_request_time = time.time()
            self.connected = True
            
            # Track data rate
            bytes_received = len(response_data)
            self.data_rate_tracker.add_data(bytes_received)
            self.total_bytes_received += bytes_received
            
            # Debug logging to see actual response format
            if len(response) > 200:
                self.logger.debug(f"Response received: {response[:200]}...")
            else:
                self.logger.debug(f"Response received: {response}")
                
            return response
        except socket.timeout:
            self.logger.warning(f"Request timeout after {self.timeout}s")
            self.connected = False
            # Close and reset socket on timeout
            self._close_socket()
            return None
        except Exception as e:
            self.logger.error(f"Request failed: {e}")
            self.connected = False
            # Close and reset socket on error
            self._close_socket()
            return None
    
    def parse_standard_response(self, response: str) -> Optional[SpectrumData]:
        """Parse standard spectrum response"""
        if not response.startswith("SPECTRA_STD:"):
            return None
        
        try:
            self.logger.debug(f"Parsing standard response: {response[:100]}...")
            
            # Format: SPECTRA_STD:timestamp:1673123456.789,points:2048,data:1.234,5.678,...
            # But the actual format might be: SPECTRA_STD:1673123456.789,points:2048,data:1.234,5.678,...
            
            # Remove the SPECTRA_STD: prefix
            content = response[12:]  # Remove "SPECTRA_STD:"
            
            # Try to find timestamp - it should be the first number before the first comma
            if content.startswith("timestamp:"):
                # Format: timestamp:1673123456.789,points:2048,data:...
                parts = content.split(',', 1)
                timestamp_part = parts[0]
                timestamp = float(timestamp_part.split(':')[1])
                metadata_and_data = parts[1] if len(parts) > 1 else ""
            else:
                # Format: 1673123456.789,points:2048,data:...
                parts = content.split(',', 1)
                timestamp = float(parts[0])
                metadata_and_data = parts[1] if len(parts) > 1 else ""
            
            # Find the data section
            data_start = metadata_and_data.find('data:')
            if data_start == -1:
                self.logger.error("No 'data:' section found in response")
                return None
            
            # Extract metadata part
            metadata_part = metadata_and_data[:data_start].rstrip(',')
            
            # Parse points from metadata
            points = None
            for item in metadata_part.split(','):
                if 'points:' in item:
                    points = int(item.split(':')[1])
                    break
            
            if points is None:
                self.logger.error("No 'points:' found in metadata")
                return None
            
            # Extract data
            data_str = metadata_and_data[data_start + 5:]  # Skip 'data:'
            data = [float(x) for x in data_str.split(',') if x.strip()]
            
            self.logger.debug(f"Successfully parsed: timestamp={timestamp}, points={points}, data_len={len(data)}")
            
            return SpectrumData(
                type='STANDARD',
                timestamp=timestamp,
                points=points,
                data=data,
                valid=True
            )
        except Exception as e:
            self.logger.error(f"Error parsing standard response: {e}")
            self.logger.error(f"Response was: {response[:200]}...")
            return None
    
    def parse_120khz_response(self, response: str) -> Optional[SpectrumData]:
        """Parse 120kHz spectrum response"""
        if not response.startswith("SPECTRA_120KHZ:"):
            return None
        
        try:
            self.logger.debug(f"Parsing 120kHz response: {response[:100]}...")
            
            # Format: SPECTRA_120KHZ:timestamp:1673123456.789,points:167,freq_start:22.225,freq_end:22.245,baseline:-45.2,data:1.234,5.678,...
            # But the actual format might be: SPECTRA_120KHZ:1673123456.789,points:167,freq_start:22.225,freq_end:22.245,baseline:-45.2,data:1.234,5.678,...
            
            # Remove the SPECTRA_120KHZ: prefix
            content = response[15:]  # Remove "SPECTRA_120KHZ:"
            
            # Try to find timestamp - it should be the first number before the first comma
            if content.startswith("timestamp:"):
                # Format: timestamp:1673123456.789,points:167,freq_start:22.225,...
                parts = content.split(',', 1)
                timestamp_part = parts[0]
                timestamp = float(timestamp_part.split(':')[1])
                metadata_and_data = parts[1] if len(parts) > 1 else ""
            else:
                # Format: 1673123456.789,points:167,freq_start:22.225,...
                parts = content.split(',', 1)
                timestamp = float(parts[0])
                metadata_and_data = parts[1] if len(parts) > 1 else ""
            
            # Find the data section
            data_start = metadata_and_data.find('data:')
            if data_start == -1:
                self.logger.error("No 'data:' section found in 120kHz response")
                return None
            
            # Extract metadata part
            metadata_part = metadata_and_data[:data_start].rstrip(',')
            
            # Parse metadata
            points = None
            freq_start = None
            freq_end = None
            baseline = None
            
            for item in metadata_part.split(','):
                if 'points:' in item:
                    points = int(item.split(':')[1])
                elif 'freq_start:' in item:
                    freq_start = float(item.split(':')[1])
                elif 'freq_end:' in item:
                    freq_end = float(item.split(':')[1])
                elif 'baseline:' in item:
                    baseline = float(item.split(':')[1])
            
            if any(x is None for x in [points, freq_start, freq_end, baseline]):
                self.logger.error(f"Missing metadata: points={points}, freq_start={freq_start}, freq_end={freq_end}, baseline={baseline}")
                return None
            
            # Extract data
            data_str = metadata_and_data[data_start + 5:]  # Skip 'data:'
            data = [float(x) for x in data_str.split(',') if x.strip()]
            
            # Always log data statistics for debugging (using INFO level)
            if data:
                data_min, data_max = min(data), max(data)
                data_mean = sum(data) / len(data)
                print(f"[CLIENT DEBUG] 120kHz data stats: min={data_min:.6f}, max={data_max:.6f}, mean={data_mean:.6f}, points={len(data)}, baseline={baseline:.6f}")
                print(f"[CLIENT DEBUG] Sample values: [0]={data[0]:.6f}, [mid]={data[len(data)//2]:.6f}, [end]={data[-1]:.6f}")
                print(f"[CLIENT DEBUG] Data range: {data_max - data_min:.6f} dB, timestamp={timestamp}")
                print(f"[CLIENT DEBUG] Frequency range: {freq_start:.6f} to {freq_end:.6f} GHz")
                
                # Validate data looks reasonable
                if not (-1.0 <= data_min <= 1.0 and 5.0 <= data_max <= 20.0):
                    print(f"[CLIENT WARNING] Unusual data range: [{data_min:.6f}, {data_max:.6f}] dB")
                if not (-46.0 <= baseline <= -42.0):
                    print(f"[CLIENT WARNING] Unusual baseline: {baseline:.6f} dB")
            
            self.logger.info(f"Successfully parsed 120kHz: timestamp={timestamp}, points={points}, data_len={len(data)}, baseline={baseline:.6f}")
            
            return SpectrumData(
                type='120KHZ',
                timestamp=timestamp,
                points=points,
                data=data,
                freq_start=freq_start,
                freq_end=freq_end,
                baseline=baseline,
                valid=True
            )
        except Exception as e:
            self.logger.error(f"Error parsing 120kHz response: {e}")
            self.logger.error(f"Response was: {response[:200]}...")
            return None
    
    def get_spectrum(self) -> Optional[SpectrumData]:
        """
        Get the appropriate spectrum based on the active spectrometer type.
        This method is stateful and will switch between STANDARD and 120KHZ modes
        to avoid sending multiple requests and hitting rate limits.
        """
        # Determine which command to send based on our current state
        if self.active_spectrometer_type == '120KHZ':
            request_cmd = "GET_SPECTRA_120KHZ"
        else:  # 'STANDARD' or 'UNKNOWN'
            request_cmd = "GET_SPECTRA"
            
        # Send the single request
        response = self._send_request(request_cmd)
        
        if not response:
            return SpectrumData(type='ERROR', timestamp=time.time(), points=0, data=[], valid=False)

        # Handle successful responses and update state
        if response.startswith("SPECTRA_STD:"):
            self.active_spectrometer_type = 'STANDARD'
            return self.parse_standard_response(response)
        
        if response.startswith("SPECTRA_120KHZ:"):
            self.active_spectrometer_type = '120KHZ'
            result = self.parse_120khz_response(response)
            if result and result.valid:
                print(f"[CLIENT DEBUG] get_spectrum() returning 120kHz data: {len(result.data)} points, baseline={result.baseline:.6f}")
            return result

        # Handle errors that tell us to switch types for the *next* request
        if "ERROR:WRONG_SPECTROMETER_TYPE" in response:
            if "current=120KHZ" in response:
                self.logger.warning("Wrong spectrometer type. Switching to 120KHZ for next cycle.")
                self.active_spectrometer_type = '120KHZ'
                return SpectrumData(type='120KHZ', timestamp=time.time(), points=0, data=[], valid=False)
            else: # Assumes current=STANDARD
                self.logger.warning("Wrong spectrometer type. Switching to STANDARD for next cycle.")
                self.active_spectrometer_type = 'STANDARD'
                return SpectrumData(type='STANDARD', timestamp=time.time(), points=0, data=[], valid=False)

        if response.startswith("ERROR:SPECTROMETER_NOT_RUNNING"):
            self.logger.info("No spectrometer is currently running")
            self.active_spectrometer_type = 'NONE'
            return SpectrumData(type='NONE', timestamp=time.time(), points=0, data=[], valid=False)
            
        # Handle other errors
        self.logger.error(f"Unknown or error response: {response[:100]}...")
        return SpectrumData(type='ERROR', timestamp=time.time(), points=0, data=[], valid=False)

    def _close_socket(self):
        """Close and reset the socket"""
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass  # Ignore errors when closing
            self.socket = None
    
    def cleanup(self):
        """Clean up resources when shutting down"""
        self._close_socket()
        self.connected = False
        # Reset data rate tracking
        self.data_rate_tracker.reset()
        self.total_bytes_received = 0
        self.logger.info("BCP Spectrometer client cleaned up")
    
    def is_connected(self) -> bool:
        """Check if client is connected to server"""
        return self.connected
    
    def get_data_rate_kbps(self) -> float:
        """Get current data rate in KB/s"""
        if not self.connected:
            return 0.0
        return self.data_rate_tracker.get_rate_kbps()
    
    def get_total_bytes_received(self) -> int:
        """Get total bytes received since start"""
        return self.total_bytes_received 