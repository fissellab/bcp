# Aquila System Monitor Integration

## Overview

The Aquila System Monitor provides real-time monitoring of the backend storage computer, focusing on SSD usage and system health. This integration allows Saggitarius to monitor storage capacity and switch SSDs as needed.

## Components

### 1. Aquila System Monitor (`aquila_system_monitor.c`)
- **Location**: Runs on aquila backend computer (172.20.4.173)
- **Function**: Monitors SSD1 (/mnt/vlbi_data), SSD2 (/mnt/vlbi_data_2), CPU temp, memory
- **Communication**: Sends UDP status updates to Saggitarius telemetry server (port 8082)
- **Frequency**: Updates every 10 seconds
- **Auto-start**: Systemd service starts on boot

### 2. Integration with BCP Telemetry System
- **Method**: Aquila monitor sends JSON status messages via UDP
- **Processing**: BCP telemetry server receives and stores latest aquila status
- **Access**: Ground station can query aquila status via telemetry requests

## Data Format

### Aquila Status Message (JSON over UDP)
```json
{
  "type": "aquila_system_status",
  "timestamp": 1692035400,
  "ssd1": {
    "mounted": 1,
    "used_gb": 850.5,
    "total_gb": 1000.0,
    "percent_used": 85.1
  },
  "ssd2": {
    "mounted": 1,
    "used_gb": 234.7,
    "total_gb": 1000.0,
    "percent_used": 23.5
  },
  "system": {
    "cpu_temp_celsius": 45.2,
    "memory_used_gb": 12.8,
    "memory_total_gb": 16.0,
    "memory_percent_used": 80.0
  }
}
```

### Telemetry Request Types
- `aquila_ssd1_status` - Get SSD1 usage information
- `aquila_ssd2_status` - Get SSD2 usage information  
- `aquila_system_status` - Get full system status
- `aquila_storage_summary` - Get summary for storage management

## Storage Management Logic

### Automatic SSD Selection
1. **Monitor both SSDs continuously**
2. **Switch criteria**:
   - Current SSD > 80% full → Switch to other SSD
   - Current SSD unmounted → Switch to other SSD
   - Manual override via ground station command

### Ground Station Display
- **Real-time storage status** in telemetry interface
- **Visual indicators** for SSD health and usage
- **Alerts** when storage reaches critical levels
- **Manual SSD selection** capability

## Installation

### On Aquila Backend
```bash
# Build and install
gcc -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE aquila_system_monitor.c -lpthread -o aquila_system_monitor
sudo cp aquila_system_monitor /usr/local/bin/
sudo cp aquila-system-monitor.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable aquila-system-monitor.service
sudo systemctl start aquila-system-monitor.service
```

### On Saggitarius
- No additional installation required
- BCP telemetry server automatically handles aquila status messages
- New telemetry request types available immediately

## Monitoring and Logs

### Aquila Monitor Logs
- **Location**: `/var/log/aquila_monitor.log`
- **Content**: Status updates, connection issues, system metrics
- **Systemd logs**: `journalctl -u aquila-system-monitor -f`

### BCP Integration
- **Telemetry logs**: Status reception logged in telemetry server logs
- **VLBI client**: Enhanced with automatic SSD selection based on aquila status
- **Ground station**: Real-time aquila status display

## Benefits

1. **Automatic Storage Management**: System switches SSDs before running out of space
2. **Real-time Monitoring**: Continuous visibility into backend storage status
3. **Proactive Alerts**: Early warning when storage capacity is low
4. **Manual Override**: Ground station can manually select SSDs if needed
5. **System Health**: Monitor CPU temperature and memory usage
6. **Boot-time Startup**: Monitor starts automatically when aquila powers on

## Future Enhancements

- **Predictive Storage**: Estimate time until storage full based on data rate
- **Automatic Cleanup**: Remove old data when storage is critically low
- **Health Monitoring**: SMART disk health monitoring
- **Network Monitoring**: Monitor network connectivity between systems
