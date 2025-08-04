# BCP Spectrometer Client Implementation Guide

## Overview
This guide provides everything needed to implement a client that fetches spectrometer data from the BCP Spectrometer UDP Server.

## Server Information
- **Server IP**: `172.20.3.12` (Saggitarius system)
- **Port**: `8081`
- **Protocol**: UDP
- **Authorization**: IP-based (your IP must be in the authorized list)
- **Rate Limit**: 1 request per second per client

## Protocol Commands

### Request Types
- `GET_SPECTRA` - Request standard resolution spectrum (2048 points)
- `GET_SPECTRA_120KHZ` - Request high-resolution water maser spectrum (~167 points)

### Response Formats
**Standard Spectrum:**
```
SPECTRA_STD:timestamp:1673123456.789,points:2048,data:1.234,5.678,...
```

**High-Resolution Spectrum (Water Maser):**
```
SPECTRA_120KHZ:timestamp:1673123456.789,points:167,freq_start:22.225,freq_end:22.245,baseline:-45.2,data:1.234,5.678,...
```

**Error Responses:**
- `ERROR:SPECTROMETER_NOT_RUNNING`
- `ERROR:WRONG_SPECTROMETER_TYPE:current=STD,requested=120KHZ`
- `ERROR:WRONG_SPECTROMETER_TYPE:current=120KHZ,requested=STD`
- `ERROR:RATE_LIMITED`
- `ERROR:UNAUTHORIZED`
- `ERROR:UNKNOWN_REQUEST:invalid_command`

## Determining Active Spectrometer Type

To know which spectrometer is currently running, send either request and check the response:

### Method 1: Quick Check Function
```python
import socket

def get_active_spectrometer_type(server_ip="172.20.3.12", server_port=8081):
    """Returns 'STANDARD', '120KHZ', 'NONE', or 'ERROR'"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    
    try:
        # Try standard first
        sock.sendto(b"GET_SPECTRA", (server_ip, server_port))
        response = sock.recv(32768).decode('utf-8')
        
        if response.startswith("SPECTRA_STD:"):
            return "STANDARD"
        elif response.startswith("ERROR:WRONG_SPECTROMETER_TYPE:current=120KHZ"):
            return "120KHZ"
        elif response.startswith("ERROR:SPECTROMETER_NOT_RUNNING"):
            return "NONE"
        else:
            return "ERROR"
    except:
        return "ERROR"
    finally:
        sock.close()
```

### Method 2: Parse Error Messages
The error responses tell you the current state:
- `ERROR:SPECTROMETER_NOT_RUNNING` → No spectrometer active
- `ERROR:WRONG_SPECTROMETER_TYPE:current=STD,requested=120KHZ` → Standard is running
- `ERROR:WRONG_SPECTROMETER_TYPE:current=120KHZ,requested=STD` → 120kHz is running

## Complete Python Client Implementation

```python
#!/usr/bin/env python3
"""
BCP Spectrometer UDP Client
Fetches spectrum data from the BCP Spectrometer Server
"""

import socket
import time
import struct
import json
from typing import Optional, Dict, List, Tuple

class BCPSpectrometerClient:
    def __init__(self, server_ip: str = "172.20.3.12", server_port: int = 8081, timeout: float = 5.0):
        self.server_ip = server_ip
        self.server_port = server_port
        self.timeout = timeout
        self.last_request_time = 0.0
    
    def _send_request(self, request: str) -> Optional[str]:
        """Send UDP request and return response"""
        # Respect rate limiting (1 req/sec)
        current_time = time.time()
        time_since_last = current_time - self.last_request_time
        if time_since_last < 1.0:
            time.sleep(1.0 - time_since_last)
        
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(self.timeout)
        
        try:
            sock.sendto(request.encode('utf-8'), (self.server_ip, self.server_port))
            response = sock.recv(32768).decode('utf-8')
            self.last_request_time = time.time()
            return response
        except socket.timeout:
            print(f"Request timeout after {self.timeout}s")
            return None
        except Exception as e:
            print(f"Request failed: {e}")
            return None
        finally:
            sock.close()
    
    def get_active_spectrometer_type(self) -> str:
        """Returns 'STANDARD', '120KHZ', 'NONE', or 'ERROR'"""
        response = self._send_request("GET_SPECTRA")
        if not response:
            return "ERROR"
        
        if response.startswith("SPECTRA_STD:"):
            return "STANDARD"
        elif response.startswith("ERROR:WRONG_SPECTROMETER_TYPE:current=120KHZ"):
            return "120KHZ"
        elif response.startswith("ERROR:SPECTROMETER_NOT_RUNNING"):
            return "NONE"
        else:
            return "ERROR"
    
    def parse_standard_response(self, response: str) -> Optional[Dict]:
        """Parse standard spectrum response"""
        if not response.startswith("SPECTRA_STD:"):
            return None
        
        try:
            # Format: SPECTRA_STD:timestamp:1673123456.789,points:2048,data:1.234,5.678,...
            parts = response.split(':', 2)  # Split into max 3 parts
            if len(parts) < 3:
                return None
            
            timestamp = float(parts[1])
            
            # Parse metadata and data
            metadata_and_data = parts[2]
            
            # Find the data section
            data_start = metadata_and_data.find('data:')
            if data_start == -1:
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
                return None
            
            # Extract data
            data_str = metadata_and_data[data_start + 5:]  # Skip 'data:'
            data = [float(x) for x in data_str.split(',') if x.strip()]
            
            return {
                'type': 'STANDARD',
                'timestamp': timestamp,
                'points': points,
                'data': data
            }
        except Exception as e:
            print(f"Error parsing standard response: {e}")
            return None
    
    def parse_120khz_response(self, response: str) -> Optional[Dict]:
        """Parse 120kHz spectrum response"""
        if not response.startswith("SPECTRA_120KHZ:"):
            return None
        
        try:
            # Format: SPECTRA_120KHZ:timestamp:1673123456.789,points:167,freq_start:22.225,freq_end:22.245,baseline:-45.2,data:1.234,5.678,...
            parts = response.split(':', 2)  # Split into max 3 parts
            if len(parts) < 3:
                return None
            
            timestamp = float(parts[1])
            
            # Parse metadata and data
            metadata_and_data = parts[2]
            
            # Find the data section
            data_start = metadata_and_data.find('data:')
            if data_start == -1:
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
                return None
            
            # Extract data
            data_str = metadata_and_data[data_start + 5:]  # Skip 'data:'
            data = [float(x) for x in data_str.split(',') if x.strip()]
            
            return {
                'type': '120KHZ',
                'timestamp': timestamp,
                'points': points,
                'freq_start': freq_start,
                'freq_end': freq_end,
                'baseline': baseline,
                'data': data
            }
        except Exception as e:
            print(f"Error parsing 120kHz response: {e}")
            return None
    
    def get_standard_spectrum(self) -> Optional[Dict]:
        """Get standard resolution spectrum (2048 points)"""
        response = self._send_request("GET_SPECTRA")
        if not response:
            return None
        
        if response.startswith("ERROR:"):
            print(f"Server error: {response}")
            return None
        
        return self.parse_standard_response(response)
    
    def get_120khz_spectrum(self) -> Optional[Dict]:
        """Get high-resolution water maser spectrum (~167 points)"""
        response = self._send_request("GET_SPECTRA_120KHZ")
        if not response:
            return None
        
        if response.startswith("ERROR:"):
            print(f"Server error: {response}")
            return None
        
        return self.parse_120khz_response(response)
    
    def get_spectrum(self) -> Optional[Dict]:
        """Automatically get the appropriate spectrum based on active spectrometer"""
        spec_type = self.get_active_spectrometer_type()
        
        if spec_type == "STANDARD":
            return self.get_standard_spectrum()
        elif spec_type == "120KHZ":
            return self.get_120khz_spectrum()
        elif spec_type == "NONE":
            print("No spectrometer is currently running")
            return None
        else:
            print("Error determining spectrometer type")
            return None

# Example Usage
if __name__ == "__main__":
    client = BCPSpectrometerClient()
    
    print("BCP Spectrometer Client")
    print("=" * 30)
    
    # Check what's running
    spec_type = client.get_active_spectrometer_type()
    print(f"Active spectrometer: {spec_type}")
    
    if spec_type in ["STANDARD", "120KHZ"]:
        # Get spectrum data
        spectrum = client.get_spectrum()
        if spectrum:
            print(f"Got {spectrum['type']} spectrum:")
            print(f"  Timestamp: {spectrum['timestamp']}")
            print(f"  Data points: {spectrum['points']}")
            if spectrum['type'] == '120KHZ':
                print(f"  Frequency range: {spectrum['freq_start']:.3f} - {spectrum['freq_end']:.3f} GHz")
                print(f"  Baseline: {spectrum['baseline']:.2f} dB")
            print(f"  First 10 data points: {spectrum['data'][:10]}")
    
    # Continuous monitoring example
    print("\nContinuous monitoring (Ctrl+C to stop):")
    try:
        while True:
            spectrum = client.get_spectrum()
            if spectrum:
                data_range = f"{min(spectrum['data']):.2f} to {max(spectrum['data']):.2f}"
                print(f"[{time.strftime('%H:%M:%S')}] {spectrum['type']}: {spectrum['points']} points, range: {data_range}")
            time.sleep(1)  # Respect rate limiting
    except KeyboardInterrupt:
        print("\nMonitoring stopped")
```

## Simple Test Client

```python
#!/usr/bin/env python3
"""Simple test client for BCP Spectrometer Server"""

import socket
import time

def test_bcp_server():
    server_ip = "172.20.3.12"
    server_port = 8081
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    
    requests = ["GET_SPECTRA", "GET_SPECTRA_120KHZ", "INVALID_REQUEST"]
    
    for request in requests:
        try:
            print(f"\nTesting: {request}")
            sock.sendto(request.encode(), (server_ip, server_port))
            response = sock.recv(32768).decode('utf-8')
            
            # Print first 200 characters of response
            if len(response) > 200:
                print(f"Response: {response[:200]}...")
            else:
                print(f"Response: {response}")
                
            time.sleep(1)  # Rate limiting
            
        except Exception as e:
            print(f"Error: {e}")
    
    sock.close()

if __name__ == "__main__":
    test_bcp_server()
```

## C Client Example

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "172.20.3.12"
#define SERVER_PORT 8081
#define BUFFER_SIZE 32768

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char request[] = "GET_SPECTRA";
    
    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    // Send request
    if (sendto(sock, request, strlen(request), 0, 
               (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("sendto");
        close(sock);
        return 1;
    }
    
    // Receive response
    ssize_t received = recvfrom(sock, buffer, BUFFER_SIZE-1, 0, NULL, NULL);
    if (received < 0) {
        perror("recvfrom");
        close(sock);
        return 1;
    }
    
    buffer[received] = '\0';
    
    // Print response (first 200 chars)
    if (received > 200) {
        printf("Response: %.200s...\n", buffer);
    } else {
        printf("Response: %s\n", buffer);
    }
    
    close(sock);
    return 0;
}
```

## Error Handling

Always check for these error responses:

1. **`ERROR:SPECTROMETER_NOT_RUNNING`**: No spectrometer is active
2. **`ERROR:WRONG_SPECTROMETER_TYPE`**: Requested wrong type (check current type)
3. **`ERROR:RATE_LIMITED`**: Too many requests (wait 1 second)
4. **`ERROR:UNAUTHORIZED`**: Your IP is not authorized
5. **`ERROR:UNKNOWN_REQUEST`**: Invalid command sent

## Data Format Details

### Standard Spectrum Data
- **Points**: 2048
- **Data Type**: Floating point power values
- **Units**: Linear power (not dB)
- **Frequency**: Covers full IF bandwidth
- **Update Rate**: ~3.67 Hz

### 120kHz High-Resolution Data
- **Points**: ~167 (varies based on zoom window)
- **Data Type**: dB values (baseline subtracted)
- **Frequency Range**: 22.225 - 22.245 GHz (±10 MHz around 22.235 GHz)
- **Processing**: Pre-filtered by server (99% data reduction)
- **Baseline**: Median value provided separately
- **Update Rate**: ~4-5 Hz

## Testing Your Implementation

1. **Check Server Status**:
   ```bash
   # From Saggitarius system
   sudo ./start.sh
   # Look for: "Spectrometer UDP server started on port 8081"
   ```

2. **Test Basic Connectivity**:
   ```bash
   # From your client machine
   echo "GET_SPECTRA" | nc -u 172.20.3.12 8081
   ```

3. **Verify Authorization**:
   - Ensure your IP is in the authorized list in `bcp_Sag.config`
   - Contact system administrator to add your IP if needed

4. **Test Rate Limiting**:
   - Send requests faster than 1/second
   - Should receive `ERROR:RATE_LIMITED`

## Performance Considerations

- **Rate Limit**: Max 1 request per second per client
- **Timeout**: Set socket timeout to 5+ seconds
- **Data Size**: Standard ~16KB, 120kHz ~1.3KB
- **Latency**: Typically <100ms on local network
- **Reliability**: UDP - no guaranteed delivery

## Integration Examples

### Real-time Plotting with Matplotlib
```python
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

client = BCPSpectrometerClient()

def update_plot(frame):
    spectrum = client.get_spectrum()
    if spectrum:
        plt.cla()
        plt.plot(spectrum['data'])
        plt.title(f"{spectrum['type']} Spectrum - {spectrum['points']} points")
        plt.ylabel('Power')
        plt.xlabel('Channel')

ani = FuncAnimation(plt.gcf(), update_plot, interval=1000)
plt.show()
```

### Data Logging to CSV
```python
import csv
import datetime

with open('spectrum_log.csv', 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['timestamp', 'type', 'points', 'data'])
    
    while True:
        spectrum = client.get_spectrum()
        if spectrum:
            writer.writerow([
                datetime.datetime.fromtimestamp(spectrum['timestamp']),
                spectrum['type'],
                spectrum['points'],
                ','.join(map(str, spectrum['data']))
            ])
        time.sleep(1)
```

### JSON Export
```python
import json

def save_spectrum_json(filename):
    client = BCPSpectrometerClient()
    spectrum = client.get_spectrum()
    
    if spectrum:
        with open(filename, 'w') as f:
            json.dump(spectrum, f, indent=2)
        print(f"Spectrum saved to {filename}")
    else:
        print("No spectrum data available")

# Usage
save_spectrum_json("latest_spectrum.json")
```

## Command Line Usage Examples

### Quick Status Check
```bash
# Save the simple test client as test_client.py
python3 test_client.py
```

### Continuous Monitoring
```bash
# Save the full client as bcp_client.py
python3 bcp_client.py
```

### Using netcat for quick tests
```bash
# Standard spectrum request
echo "GET_SPECTRA" | nc -u 172.20.3.12 8081

# 120kHz spectrum request
echo "GET_SPECTRA_120KHZ" | nc -u 172.20.3.12 8081

# Invalid request (should return error)
echo "INVALID" | nc -u 172.20.3.12 8081
```

---
**Status**: ✅ COMPLETE  
**Languages**: Python, C  
**Features**: Auto-detection, Error handling, Rate limiting, Real-time monitoring 