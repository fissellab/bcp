# VLBI Telemetry Client Implementation Guide

## Overview

This guide shows how to access real-time VLBI status data from the Saggitarius telemetry server. When `start_vlbi` is executed, status automatically streams from aquila to Saggitarius every 5 seconds, making all VLBI metrics available via standard telemetry channels.

## Connection Details

- **Server**: Saggitarius BCP system
- **IP**: `172.20.4.170` (or your Saggitarius IP)
- **Port**: `8001` (telemetry server port)
- **Protocol**: UDP
- **Format**: Simple request/response text

## Quick Test

```bash
# Test basic connectivity
echo "vlbi_running" | nc -u 172.20.4.170 8001

# Get comprehensive VLBI status
echo "GET_VLBI" | nc -u 172.20.4.170 8001
```

## Available VLBI Channels

| Channel | Type | Description | Example Response |
|---------|------|-------------|------------------|
| `vlbi_running` | Integer | VLBI active status | `1` (running) or `0` (stopped) |
| `vlbi_stage` | String | Current processing stage | `"connecting"`, `"programming"`, `"capturing"`, `"stopped"` |
| `vlbi_packets` | Integer | Total packets captured | `15420` |
| `vlbi_data_mb` | Float | Data volume in MB | `128.5` |
| `vlbi_connection` | String | RFSoC connection status | `"connected"`, `"capturing"`, `"disconnected"` |
| `vlbi_errors` | Integer | Error count | `0` |
| `vlbi_pid` | Integer | Process ID (if running) | `3495` |
| `vlbi_last_update` | String | Last status timestamp | `"2025-08-02T12:50:27.058716"` |
| `GET_VLBI` | String | All VLBI data (CSV format) | See below |

## GET_VLBI Response Format

```
vlbi_running:1,vlbi_stage:capturing,vlbi_packets:15420,vlbi_data_mb:128.5,vlbi_connection:connected,vlbi_errors:0
```

## Implementation Examples

### Python Client

```python
import socket
import time

def get_vlbi_status(channel="GET_VLBI"):
    """Get VLBI telemetry data from Saggitarius server"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    
    try:
        # Send request
        sock.sendto(channel.encode(), ('172.20.4.170', 8001))
        
        # Receive response
        data, addr = sock.recvfrom(1024)
        return data.decode().strip()
    
    except socket.timeout:
        return "TIMEOUT"
    except Exception as e:
        return f"ERROR: {e}"
    finally:
        sock.close()

# Example usage
print("VLBI Status:", get_vlbi_status("vlbi_stage"))
print("Packets:", get_vlbi_status("vlbi_packets"))
print("Full Status:", get_vlbi_status("GET_VLBI"))

# Parse comprehensive status
status = get_vlbi_status("GET_VLBI")
if "vlbi_running:1" in status:
    print("✅ VLBI is running")
    # Parse individual fields
    fields = dict(item.split(':') for item in status.split(','))
    print(f"Stage: {fields.get('vlbi_stage', 'unknown')}")
    print(f"Packets: {fields.get('vlbi_packets', '0')}")
    print(f"Data: {fields.get('vlbi_data_mb', '0')} MB")
```

### Bash/Shell Script

```bash
#!/bin/bash

SAG_IP="172.20.4.170"
SAG_PORT="8001"

get_vlbi_data() {
    echo "$1" | nc -u -w 5 $SAG_IP $SAG_PORT 2>/dev/null
}

# Monitor VLBI in real-time
monitor_vlbi() {
    echo "=== VLBI Real-time Monitor ==="
    while true; do
        status=$(get_vlbi_data "GET_VLBI")
        
        if [[ $status == *"vlbi_running:1"* ]]; then
            echo "[$(date)] ✅ VLBI Active: $status"
        else
            echo "[$(date)] ❌ VLBI Inactive"
        fi
        
        sleep 10
    done
}

# Example usage
echo "VLBI Running: $(get_vlbi_data 'vlbi_running')"
echo "Current Stage: $(get_vlbi_data 'vlbi_stage')"
echo "Packets Captured: $(get_vlbi_data 'vlbi_packets')"
echo "Data Volume: $(get_vlbi_data 'vlbi_data_mb') MB"

# Uncomment to start monitoring
# monitor_vlbi
```

### JavaScript/Node.js Client

```javascript
const dgram = require('dgram');

function getVLBIStatus(channel = 'GET_VLBI') {
    return new Promise((resolve, reject) => {
        const client = dgram.createSocket('udp4');
        
        client.send(channel, 8001, '172.20.4.170', (err) => {
            if (err) reject(err);
        });
        
        client.on('message', (data) => {
            client.close();
            resolve(data.toString().trim());
        });
        
        // Timeout after 5 seconds
        setTimeout(() => {
            client.close();
            reject(new Error('Timeout'));
        }, 5000);
    });
}

// Example usage
async function monitorVLBI() {
    try {
        const status = await getVLBIStatus('GET_VLBI');
        console.log('VLBI Status:', status);
        
        // Parse status
        const fields = {};
        status.split(',').forEach(item => {
            const [key, value] = item.split(':');
            fields[key] = value;
        });
        
        if (fields.vlbi_running === '1') {
            console.log('✅ VLBI is running');
            console.log(`Stage: ${fields.vlbi_stage}`);
            console.log(`Packets: ${fields.vlbi_packets}`);
            console.log(`Data: ${fields.vlbi_data_mb} MB`);
        } else {
            console.log('❌ VLBI is not running');
        }
        
    } catch (error) {
        console.error('Error:', error.message);
    }
}

// Run every 10 seconds
setInterval(monitorVLBI, 10000);
monitorVLBI(); // Run immediately
```

## Error Handling

| Response | Meaning |
|----------|---------|
| `N/A` | VLBI client disabled or no data available |
| `ERROR:UNKNOWN_REQUEST` | Invalid channel name |
| No response/timeout | Saggitarius server unreachable |

## Data Refresh Rate

- **Auto-streaming**: Updates every 5 seconds from aquila
- **Client polling**: Query as frequently as needed
- **Recommended**: Poll every 10-30 seconds for dashboards

## Workflow Integration

1. **Start VLBI**: Execute `start_vlbi` command on Saggitarius
2. **Automatic streaming**: Status flows to telemetry server
3. **Client monitoring**: Query any channel for real-time data
4. **Stop VLBI**: Execute `stop_vlbi` to end streaming

## Troubleshooting

```bash
# Test server connectivity
nc -u -v 172.20.4.170 8001

# Check if telemetry server is running
netstat -un | grep :8001

# Verify VLBI client is enabled
echo "vlbi_running" | nc -u 172.20.4.170 8001
```

## Notes

- All VLBI telemetry is **read-only** - no control commands via telemetry
- Use BCP CLI commands (`start_vlbi`, `stop_vlbi`) for control
- Status updates automatically when VLBI script state changes
- Compatible with existing telemetry infrastructure (GPS, heaters, etc.)