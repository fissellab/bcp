"""
Star Camera Client for BVEX Ground Station
Handles communication with the star camera downlink server
"""

import socket
import struct
import time
import logging
from typing import Optional, Tuple
from dataclasses import dataclass
from io import BytesIO

from src.config.settings import STAR_CAMERA


# Protocol message types (from server specification)
MSG_GET_LATEST_IMAGE = 1
MSG_GET_IMAGE_LIST = 2
MSG_GET_IMAGE_BY_TIMESTAMP = 3
MSG_GET_STATUS = 4
MSG_IMAGE_HEADER = 5       # Server response
MSG_IMAGE_CHUNK = 6        # Server response  
MSG_IMAGE_COMPLETE = 7     # Server response
MSG_ERROR = 8              # Server response
MSG_STATUS_RESPONSE = 9    # Server response


@dataclass
class StarCameraImage:
    """Data class for star camera image information"""
    timestamp: int
    width: int
    height: int
    total_size: int
    compression_quality: int
    blob_count: int
    image_data: bytes
    valid: bool = False


@dataclass 
class StarCameraStatus:
    """Data class for star camera server status"""
    queue_size: int
    bandwidth_kbps: int
    is_active: bool
    is_streaming: bool
    valid: bool = True


class StarCameraClient:
    """Client for communicating with star camera downlink server"""
    
    def __init__(self, host: str = None, port: int = None, timeout: float = None):
        self.host = host or STAR_CAMERA['host']
        self.port = port or STAR_CAMERA['port']
        self.timeout = timeout or 5.0  # Reduced from 10s to 5s for faster failure detection
        
        self.logger = logging.getLogger(__name__)
        self.last_request_time = 0.0
        self.sequence_number = 1
        
        # Data rate tracking
        self.bytes_received = 0
        self.last_rate_check = time.time()
        
        # Connection state tracking
        self.last_successful_connection = 0
        self.consecutive_failures = 0
        
    def _create_socket(self) -> socket.socket:
        """Create and configure UDP socket"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(self.timeout)
        return sock
        
    def _send_message(self, sock: socket.socket, msg_type: int, payload_size: int = 0, sequence: int = 1) -> bool:
        """Send a properly formatted message to the server"""
        try:
            # Server expects: 1 byte type + 4 bytes payload_size + 4 bytes sequence = 9 bytes total
            # Format: uint8_t + uint32_t + uint32_t (little-endian)
            message = struct.pack('<BII', msg_type, payload_size, sequence)
            
            self.logger.debug(f"Sending message: type={msg_type}, payload_size={payload_size}, sequence={sequence}")
            self.logger.debug(f"Message bytes ({len(message)}): {message.hex()}")
            
            sock.sendto(message, (self.host, self.port))
            return True
        except Exception as e:
            self.logger.error(f"Failed to send message: {e}")
            return False

    def _send_message_le(self, sock: socket.socket, msg_type: int, sequence: int = 1) -> bool:
        """Send a little-endian message (compatibility wrapper)"""
        return self._send_message(sock, msg_type, 0, sequence)
        
    def get_status(self) -> StarCameraStatus:
        """Get the current status from the star camera server"""
        sock = self._create_socket()
        
        try:
            # Use short timeout for status requests
            sock.settimeout(3.0)  # Short 3-second timeout
            
            self.logger.debug("Requesting status from star camera server")
            
            # Send status request using the correct 9-byte format
            if not self._send_message(sock, MSG_GET_STATUS):
                self.consecutive_failures += 1
                return StarCameraStatus(0, 0, False, False, False)
                
            # Receive response with short timeout
            data, addr = sock.recvfrom(1024)
            self.bytes_received += len(data)
            
            self.logger.debug(f"Received response: {len(data)} bytes")
            
            if len(data) < 9:
                self.logger.debug(f"Response too short: {len(data)} bytes")
                self.consecutive_failures += 1
                return StarCameraStatus(0, 0, False, False, False)
                
            # Parse response header
            msg_type, payload_size, sequence = struct.unpack('<BII', data[:9])
            self.logger.debug(f"Response header: type={msg_type}, payload_size={payload_size}, seq={sequence}")
            
            if msg_type == MSG_STATUS_RESPONSE and len(data) >= 25:  # 9 + 16 bytes minimum
                # Parse status payload: queue_size(4) + timestamp(8) + bandwidth(4)
                queue_size, timestamp, bandwidth = struct.unpack('<IQI', data[9:25])
                
                self.logger.debug(f"Status parsed: queue={queue_size}, timestamp={timestamp}, bandwidth={bandwidth}")
                
                # Reset failure count on success
                self.consecutive_failures = 0
                self.last_successful_connection = time.time()
                
                return StarCameraStatus(
                    queue_size=queue_size,
                    bandwidth_kbps=bandwidth,
                    is_active=True,
                    is_streaming=timestamp > 0,
                    valid=True
                )
            else:
                self.logger.debug(f"Unexpected response type {msg_type} or insufficient data")
                self.consecutive_failures += 1
                
        except socket.timeout:
            self.consecutive_failures += 1
            self.logger.debug(f"Status request timeout (failure #{self.consecutive_failures})")
        except ConnectionRefusedError:
            self.consecutive_failures += 1
            self.logger.debug("Connection refused - server may be down")
        except Exception as e:
            self.consecutive_failures += 1
            self.logger.debug(f"Status request error: {e}")
        finally:
            sock.close()
            
        return StarCameraStatus(0, 0, False, False, False)
        
    def get_latest_image(self) -> Optional[StarCameraImage]:
        """Request and download the latest image from star camera server"""
        # Skip image requests if we have too many consecutive failures
        if self.consecutive_failures > 3:
            self.logger.debug(f"Skipping image request due to {self.consecutive_failures} consecutive failures")
            return StarCameraImage(0, 0, 0, 0, 0, 0, b'', False)
            
        sock = self._create_socket()
        
        try:
            # Use shorter timeout for initial image request
            sock.settimeout(8.0)  # Reduced from 60s to 8s
            
            self.logger.debug("Requesting latest image from star camera server")
            
            # Send image request using the correct 9-byte format
            if not self._send_message(sock, MSG_GET_LATEST_IMAGE, sequence=2):
                self.consecutive_failures += 1
                return None
                
            # First, wait for image header (MSG_IMAGE_HEADER = 5)
            self.logger.debug("Waiting for image header...")
            data, addr = sock.recvfrom(4096)
            self.bytes_received += len(data)
            
            if len(data) < 9:
                self.logger.debug(f"Response too short: {len(data)} bytes")
                self.consecutive_failures += 1
                return StarCameraImage(0, 0, 0, 0, 0, 0, b'', False)
                
            msg_type, payload_size, sequence = struct.unpack('<BII', data[:9])
            self.logger.debug(f"Received message: type={msg_type}, payload_size={payload_size}, seq={sequence}")
            
            if msg_type == MSG_ERROR:
                self.logger.debug("Server returned error - no images available")
                # This is normal, not a connection failure
                return StarCameraImage(0, 0, 0, 0, 0, 0, b'', False)
                
            if msg_type != MSG_IMAGE_HEADER:
                self.logger.debug(f"Expected MSG_IMAGE_HEADER (5), got {msg_type}")
                self.consecutive_failures += 1
                return StarCameraImage(0, 0, 0, 0, 0, 0, b'', False)
                
            # Parse image header: timestamp(8) + total_size(4) + total_chunks(4) + quality(1) + blob_count(4) + width(4) + height(4)
            if len(data) < 38:  # 9 + 29 bytes
                self.logger.debug(f"Image header too short: {len(data)} bytes")
                self.consecutive_failures += 1
                return StarCameraImage(0, 0, 0, 0, 0, 0, b'', False)
                
            header_data = data[9:38]
            timestamp, total_size, total_chunks, quality, blob_count, width, height = struct.unpack('<QIIIBII', header_data)
            
            self.logger.info(f"Image header: {total_size} bytes, {total_chunks} chunks, {width}x{height}, quality={quality}")
            
            # Use longer timeout for chunk receiving, but not too long
            sock.settimeout(15.0)  # Reduced from no limit to 15s
            
            # Receive all image chunks
            image_data = bytearray()
            chunks_received = 0
            expected_chunks = total_chunks
            
            while chunks_received < expected_chunks:
                self.logger.debug(f"Waiting for chunk {chunks_received + 1}/{expected_chunks}")
                
                try:
                    data, addr = sock.recvfrom(4096)
                    self.bytes_received += len(data)
                    
                    if len(data) < 9:
                        self.logger.debug(f"Chunk response too short: {len(data)} bytes")
                        continue
                        
                    msg_type, payload_size, sequence = struct.unpack('<BII', data[:9])
                    
                    if msg_type == MSG_IMAGE_CHUNK:
                        # Parse chunk: chunk_id(4) + data_size(4) + data
                        if len(data) < 17:  # 9 + 8 bytes minimum
                            self.logger.debug(f"Chunk header too short: {len(data)} bytes")
                            continue
                            
                        chunk_id, data_size = struct.unpack('<II', data[9:17])
                        chunk_data = data[17:17 + data_size]
                        
                        if len(chunk_data) != data_size:
                            self.logger.debug(f"Chunk data size mismatch: expected {data_size}, got {len(chunk_data)}")
                            continue
                            
                        image_data.extend(chunk_data)
                        chunks_received += 1
                        
                        if chunks_received % 5 == 0 or chunks_received == expected_chunks:
                            progress = (chunks_received / expected_chunks) * 100
                            self.logger.info(f"Received {chunks_received}/{expected_chunks} chunks ({progress:.1f}%)")
                            
                    elif msg_type == MSG_IMAGE_COMPLETE:
                        self.logger.info(f"Received image complete message - got {chunks_received}/{expected_chunks} chunks")
                        break
                    elif msg_type == MSG_ERROR:
                        self.logger.debug("Received error during chunk transfer")
                        break
                    else:
                        self.logger.debug(f"Unexpected message type during chunk transfer: {msg_type}")
                        
                except socket.timeout:
                    self.logger.debug(f"Timeout waiting for chunk {chunks_received + 1}/{expected_chunks}")
                    break
                except Exception as e:
                    self.logger.debug(f"Error receiving chunk: {e}")
                    break
                    
            if chunks_received == expected_chunks:
                self.logger.info(f"Image download complete: {len(image_data)} bytes received")
                
                # Reset failure count on successful image download
                self.consecutive_failures = 0
                self.last_successful_connection = time.time()
                
                return StarCameraImage(
                    timestamp=timestamp,
                    width=width,
                    height=height,
                    total_size=len(image_data),
                    compression_quality=quality,
                    blob_count=blob_count,
                    image_data=bytes(image_data),
                    valid=True
                )
            else:
                self.logger.debug(f"Incomplete image: received {chunks_received}/{expected_chunks} chunks")
                self.consecutive_failures += 1
                
        except socket.timeout:
            self.consecutive_failures += 1
            self.logger.debug(f"Image request timeout (failure #{self.consecutive_failures})")
        except ConnectionRefusedError:
            self.consecutive_failures += 1
            self.logger.debug("Connection refused during image request - server may be down")
        except Exception as e:
            self.consecutive_failures += 1
            self.logger.debug(f"Error getting latest image: {e}")
        finally:
            sock.close()
            
        return StarCameraImage(0, 0, 0, 0, 0, 0, b'', False)
        
    def get_data_rate_kbps(self) -> float:
        """Calculate current data rate in kB/s"""
        current_time = time.time()
        time_diff = current_time - self.last_rate_check
        
        if time_diff >= 1.0:  # Update every second
            rate_kbps = (self.bytes_received / 1024.0) / time_diff
            self.bytes_received = 0
            self.last_rate_check = current_time
            return rate_kbps
            
        return 0.0
        
    def should_attempt_connection(self) -> bool:
        """Check if we should attempt to connect based on recent failures"""
        # If too many failures, wait longer before trying again
        if self.consecutive_failures > 5:
            time_since_last_attempt = time.time() - self.last_successful_connection
            # Wait 30 seconds before trying again after many failures
            return time_since_last_attempt > 30
        return True
    
    def is_connected(self) -> bool:
        """Check if server is responding"""
        if not self.should_attempt_connection():
            return False
            
        status = self.get_status()
        return status and status.valid 