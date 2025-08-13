# TICC Client Implementation Guide

## Overview
The TICC (Time Interval Counter) system measures PPS timing differences between GPS and OCXO signals. Data is available via UDP telemetry.

## Connection Details
- **Server IP**: Saggitarius BCP IP address
- **Port**: `8082` (UDP)
- **Protocol**: Send request string, receive response string

## Available Telemetry Channels

### Individual Measurements
```bash
# Current timestamp of last measurement (Unix time)
echo "ticc_timestamp" | nc -u <sag_ip> 8082
# Returns: 1754494408.000

# Current time interval measurement (seconds)
echo "ticc_interval" | nc -u <sag_ip> 8082
# Returns: -0.47321053853
```

### Status Information
```bash
# Is TICC actively logging? (1=yes, 0=no)
echo "ticc_logging" | nc -u <sag_ip> 8082

# Total measurements taken
echo "ticc_measurement_count" | nc -u <sag_ip> 8082

# Current data file path
echo "ticc_current_file" | nc -u <sag_ip> 8082

# Comprehensive status
echo "ticc_status" | nc -u <sag_ip> 8082
# Returns: logging:yes,configured:yes,measurements:1234
```

### Comprehensive Data (Recommended)
```bash
# Get all key TICC data in one request
echo "GET_TICC" | nc -u <sag_ip> 8082
# Returns: ticc_timestamp:1754494408.000,ticc_interval:-0.47321053853,ticc_logging:1,ticc_measurement_count:1234
```

## Example Client Implementation

### Python Example
```python
import socket

def get_ticc_data(sag_ip):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    
    # Get comprehensive TICC data
    sock.sendto(b"GET_TICC", (sag_ip, 8082))
    response = sock.recv(1024).decode().strip()
    
    # Parse response
    data = {}
    for item in response.split(','):
        key, value = item.split(':')
        data[key] = value
    
    return data

# Usage
ticc_data = get_ticc_data("172.20.4.XXX")
print(f"Timestamp: {ticc_data['ticc_timestamp']}")
print(f"Interval: {ticc_data['ticc_interval']}")
```

### Bash Example
```bash
#!/bin/bash
SAG_IP="172.20.4.XXX"

# Get latest measurement
TIMESTAMP=$(echo "ticc_timestamp" | nc -u $SAG_IP 8082)
INTERVAL=$(echo "ticc_interval" | nc -u $SAG_IP 8082)

echo "Latest TICC measurement:"
echo "Time: $TIMESTAMP"
echo "Interval: $INTERVAL seconds"
```

## Data Format
- **Timestamp**: Unix time with 3 decimal places (e.g., `1754494408.000`)
- **Interval**: Time difference in seconds with 11 decimal precision (e.g., `-0.47321053853`)
- **Status**: Returns `N/A` when TICC is not running or no data available

## Prerequisites
1. TICC must be enabled in BCP configuration (`ticc.enabled = 1`)
2. Timing chain must be powered on (`start_timing_chain`)
3. TICC logging must be active (`start_ticc`)

## Error Responses
- `N/A`: TICC not running, disabled, or no measurements available
- `ERROR:UNKNOWN_REQUEST`: Invalid telemetry channel name
- No response: Network timeout or BCP not running