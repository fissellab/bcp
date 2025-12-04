# BCP System Monitor - Client Implementation Guide

## Overview
Both BCP Ophiuchus and BCP Saggitarius flight computers provide real-time system monitoring metrics via UDP telemetry. This guide explains how ground station clients can request and receive these metrics from both systems.

## Connection Details
### BCP Ophiuchus
- **Protocol**: UDP
- **Server IP**: As configured in `bcp_Oph.config` under `server.ip` (default: `0.0.0.0` - listens on all interfaces)
- **Port**: As configured in `bcp_Oph.config` under `server.port` (default: `8002`)
- **Timeout**: As configured in `bcp_Oph.config` under `server.timeout` (default: `50000` µs = 50ms)

### BCP Saggitarius
- **Protocol**: UDP
- **Server IP**: As configured in `bcp_Sag.config` under `server.ip`
- **Port**: As configured in `bcp_Sag.config` under `server.port`
- **Timeout**: As configured in `bcp_Sag.config` under `server.timeout`

## Available System Metrics

### BCP Ophiuchus Metrics

| Metric ID | Data Type | Description | Example Response |
|-----------|-----------|-------------|------------------|
| `oph_sys_cpu_temp` | Float | CPU temperature in Celsius | `45.2` |
| `oph_sys_cpu_usage` | Float | CPU usage percentage | `23.5` |
| `oph_sys_mem_used` | Float | Memory used in GB | `2.1` |
| `oph_sys_mem_total` | Float | Total memory in GB | `8.0` |
| `oph_sys_mem_used_str` | String | Memory used with units | `2.1Gi` |
| `oph_sys_mem_total_str` | String | Total memory with units | `8.0Gi` |
| `oph_sys_ssd_mounted` | Integer | SSD mount status (1=mounted, 0=not) | `1` |
| `oph_sys_ssd_used` | String | SSD used space with units | `125G` |
| `oph_sys_ssd_total` | String | SSD total space with units | `1.8T` |
| `oph_sys_ssd_path` | String | SSD mount path | `/media/ophiuchus/T7` |

### BCP Saggitarius Metrics

| Metric ID | Data Type | Description | Example Response |
|-----------|-----------|-------------|------------------|
| `sag_sys_cpu_temp` | Float | CPU temperature in Celsius | `42.8` |
| `sag_sys_cpu_usage` | Float | CPU usage percentage | `18.3` |
| `sag_sys_mem_used` | Float | Memory used in GB | `3.2` |
| `sag_sys_mem_total` | Float | Total memory in GB | `16.0` |
| `sag_sys_mem_used_str` | String | Memory used with units | `3.2Gi` |
| `sag_sys_mem_total_str` | String | Total memory with units | `16.0Gi` |
| `sag_sys_ssd_mounted` | Integer | SSD mount status (1=mounted, 0=not) | `1` |
| `sag_sys_ssd_used` | String | SSD used space with units | `250G` |
| `sag_sys_ssd_total` | String | SSD total space with units | `1.8T` |
| `sag_sys_ssd_path` | String | SSD mount path | `/media/saggitarius/T7` |

## Client Implementation

### 1. Request Format
Send a UDP packet containing the metric ID as a plain text string:

**For BCP Ophiuchus:**
```
oph_sys_cpu_temp
```

**For BCP Saggitarius:**
```
sag_sys_cpu_temp
```

### 2. Response Format
The server responds with the metric value as a string:
```
45.2
```

### 3. Error Handling
- If system monitor is disabled: Returns `-1` (for numeric) or `N/A` (for strings)
- If metric doesn't exist: No response or error logged on server

### 4. Sample Python Client
```python
import socket

def get_oph_metric(metric_id, server_ip, server_port, timeout=5):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    
    try:
        # Send metric request
        sock.sendto(metric_id.encode(), (server_ip, server_port))
        
        # Receive response
        data, addr = sock.recvfrom(1024)
        return data.decode().strip()
    
    except socket.timeout:
        return None
    finally:
        sock.close()

# Example usage for Ophiuchus
oph_cpu_temp = get_oph_metric("oph_sys_cpu_temp", "192.168.1.100", 8002)
print(f"Ophiuchus CPU Temperature: {oph_cpu_temp}°C")

# Example usage for Saggitarius  
sag_cpu_temp = get_oph_metric("sag_sys_cpu_temp", "192.168.1.101", 8080)
print(f"Saggitarius CPU Temperature: {sag_cpu_temp}°C")
```

### 5. Sample C Client
```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char* get_oph_metric(const char* metric_id, const char* server_ip, int port) {
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[1024];
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    
    // Send request
    sendto(sockfd, metric_id, strlen(metric_id), 0, 
           (struct sockaddr*)&servaddr, sizeof(servaddr));
    
    // Receive response
    int n = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0, NULL, NULL);
    buffer[n] = '\0';
    
    close(sockfd);
    return strdup(buffer);
}
```

## Tested Example (Ophiuchus)
**Working Example from Live System:**
```bash
# Connect to localhost on port 8002 and request CPU temperature
echo "oph_sys_cpu_temp" | nc -u localhost 8002
# Response: 26.799999

# Using Python (tested and working):
python3 -c "
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2)
sock.sendto(b'oph_sys_cpu_temp', ('localhost', 8002))
data, addr = sock.recvfrom(1024)
print(f'CPU Temp: {data.decode()}°C')
sock.close()
"
# Output: CPU Temp: 26.799999°C
```

**Actual Responses from Live Testing:**
- `oph_sys_cpu_temp`: `26.799999` (26.8°C)
- `oph_sys_cpu_usage`: `8.900000` (8.9%)
- `oph_sys_mem_used_str`: `831Mi`
- `oph_sys_ssd_mounted`: `1` (mounted)

## Update Frequency
- System metrics are updated every 10 seconds by default
- For Ophiuchus: Configure update interval in `bcp_Oph.config` under `system_monitor.update_interval_sec`
- For Saggitarius: Configure update interval in `bcp_Sag.config` under `system_monitor.update_interval_sec`

## Configuration Check
**Ophiuchus Default Configuration:**
```
server: {
  enabled = 1;
  logfile = "log/server.log";
  ip = "0.0.0.0";        # Listens on all interfaces
  port = 8002;           # Telemetry port
  timeout = 50000;       # 50ms timeout
};

system_monitor: {
  enabled = 1;                    # Must be enabled
  logfile = "log/system_monitor.log";
  update_interval_sec = 10;       # Update every 10 seconds
};
```

## Troubleshooting
1. **No Response**: Check if BCP is running with `ps aux | grep main`
2. **Port Check**: Verify port 8002 is listening with `netstat -ulnp | grep 8002`
3. **System Monitor Status**: Check log file `tail -f log/system_monitor.log`
4. **Test Connection**: Use the Python test script above to verify connectivity

## Notes
- Ophiuchus metric IDs are prefixed with `oph_` and Saggitarius metric IDs are prefixed with `sag_` to distinguish between flight computers
- Ensure system monitor is enabled in the respective BCP configuration files
- For Ophiuchus: Enable in `bcp_Oph.config` under `system_monitor.enabled`
- For Saggitarius: Enable in `bcp_Sag.config` under `system_monitor.enabled` 
- SSD metrics look for T7 drive mounts at `/media/ophiuchus/T7` or `/media/saggitarius/T7`
- Both systems use the same telemetry server framework but serve different metric sets
- Server listens on `0.0.0.0:8002` by default, so you can connect from any network interface