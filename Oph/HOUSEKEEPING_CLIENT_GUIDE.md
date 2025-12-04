# BCP Housekeeping Client Guide

## Overview

This guide provides instructions for accessing housekeeping sensor data from the BCP Ophiuchus telemetry server for display in client applications and GUIs.

## Server Connection Details

- **Protocol**: UDP
- **IP Address**: `0.0.0.0` (listens on all interfaces)
- **Port**: `8002`
- **Timeout**: 50ms

## Connection Setup

```python
import socket

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_address = ('YOUR_BCP_IP', 8002)

def request_metric(metric_id):
    """Request a specific metric from BCP server"""
    sock.sendto(metric_id.encode(), server_address)
    data, address = sock.recvfrom(1024)
    return data.decode()
```

## Available Housekeeping Metrics

### System Status
| Metric ID | Description | Data Type | Notes |
|-----------|-------------|-----------|-------|
| `hk_powered` | Housekeeping system power status | Integer | 1=powered, 0=off |
| `hk_running` | Data collection running status | Integer | 1=running, 0=stopped |

### Temperature Sensors

#### I2C Temperature (OCXO)
| Metric ID | Description | Data Type | Units |
|-----------|-------------|-----------|-------|
| `hk_ocxo_temp` | OCXO temperature from TMP117 | Float | °C |
| `hk_ocxo_temp_ready` | Temperature data ready flag | Integer | 1=ready, 0=not ready |

#### Analog Frontend Temperatures (LM335)
| Metric ID | Description | Data Type | Units | Pin |
|-----------|-------------|-----------|-------|-----|
| `hk_ifamp_temp` | IF Amplifier temperature | Float | °C | AIN0 |
| `hk_lo_temp` | Local Oscillator temperature | Float | °C | AIN3 |
| `hk_tec_temp` | TEC temperature | Float | °C | AIN123 |

#### Analog Backend Temperatures (LM335)
| Metric ID | Description | Data Type | Units | Pin |
|-----------|-------------|-----------|-------|-----|
| `hk_backend_chassis_temp` | Backend Chassis temperature | Float | °C | AIN122 |
| `hk_nic_temp` | NIC temperature | Float | °C | AIN121 |
| `hk_rfsoc_chassis_temp` | RFSoC Chassis temperature | Float | °C | AIN126 |
| `hk_rfsoc_chip_temp` | RFSoC Chip temperature | Float | °C | AIN127 |

#### LNA Box Temperatures (LM335)
| Metric ID | Description | Data Type | Units | Pin |
|-----------|-------------|-----------|-------|-----|
| `hk_lna1_temp` | LNA1 temperature | Float | °C | AIN125 |
| `hk_lna2_temp` | LNA2 temperature | Float | °C | AIN124 |

### Pressure Sensors

#### I2C Pressure (Pump-down Valve)
| Metric ID | Description | Data Type | Units |
|-----------|-------------|-----------|-------|
| `hk_pv_pressure_bar` | Pump-down valve pressure | Float | bar |
| `hk_pv_pressure_psi` | Pump-down valve pressure | Float | PSI |
| `hk_pv_pressure_torr` | Pump-down valve pressure | Float | Torr |
| `hk_pressure_valid` | Pressure measurement validity | Integer | 1=valid, 0=invalid |

## Error Values

- **Temperature sensors**: `-999.0` indicates sensor error or system not running
- **Pressure sensors**: `-999.0` indicates sensor error or system not running
- **Status flags**: `0` indicates system not available or disabled

## Example Client Implementation

### Python Example

```python
import socket
import time
import json

class HousekeepingClient:
    def __init__(self, server_ip, server_port=8002):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.server_address = (server_ip, server_port)
        self.sock.settimeout(1.0)  # 1 second timeout
    
    def get_metric(self, metric_id):
        """Get a single metric value"""
        try:
            self.sock.sendto(metric_id.encode(), self.server_address)
            data, _ = self.sock.recvfrom(1024)
            return data.decode().strip()
        except socket.timeout:
            return None
        except Exception as e:
            print(f"Error getting {metric_id}: {e}")
            return None
    
    def get_all_temperatures(self):
        """Get all temperature readings"""
        temps = {}
        temp_metrics = [
            'hk_ocxo_temp', 'hk_ifamp_temp', 'hk_lo_temp', 'hk_tec_temp',
            'hk_backend_chassis_temp', 'hk_nic_temp', 'hk_rfsoc_chassis_temp',
            'hk_rfsoc_chip_temp', 'hk_lna1_temp', 'hk_lna2_temp'
        ]
        
        for metric in temp_metrics:
            value = self.get_metric(metric)
            if value is not None:
                try:
                    temps[metric] = float(value)
                except ValueError:
                    temps[metric] = None
        
        return temps
    
    def get_all_pressures(self):
        """Get all pressure readings"""
        pressures = {}
        pressure_metrics = [
            'hk_pv_pressure_bar', 'hk_pv_pressure_psi', 'hk_pv_pressure_torr'
        ]
        
        for metric in pressure_metrics:
            value = self.get_metric(metric)
            if value is not None:
                try:
                    pressures[metric] = float(value)
                except ValueError:
                    pressures[metric] = None
        
        return pressures
    
    def get_system_status(self):
        """Get system status"""
        status = {}
        status_metrics = ['hk_powered', 'hk_running', 'hk_pressure_valid', 'hk_ocxo_temp_ready']
        
        for metric in status_metrics:
            value = self.get_metric(metric)
            if value is not None:
                try:
                    status[metric] = int(value)
                except ValueError:
                    status[metric] = None
        
        return status
    
    def close(self):
        """Close the socket"""
        self.sock.close()

# Usage example
if __name__ == "__main__":
    client = HousekeepingClient("192.168.1.100")  # Replace with BCP IP
    
    try:
        # Check if housekeeping is running
        status = client.get_system_status()
        print("System Status:", status)
        
        if status.get('hk_running', 0):
            # Get all temperature data
            temps = client.get_all_temperatures()
            print("Temperatures:", temps)
            
            # Get pressure data
            pressures = client.get_all_pressures()
            print("Pressures:", pressures)
        else:
            print("Housekeeping system not running")
    
    finally:
        client.close()
```

### JavaScript/Node.js Example

```javascript
const dgram = require('dgram');

class HousekeepingClient {
    constructor(serverIp, serverPort = 8002) {
        this.client = dgram.createSocket('udp4');
        this.serverIp = serverIp;
        this.serverPort = serverPort;
    }

    getMetric(metricId) {
        return new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                reject(new Error('Timeout'));
            }, 1000);

            this.client.once('message', (msg, rinfo) => {
                clearTimeout(timeout);
                resolve(msg.toString().trim());
            });

            this.client.send(metricId, this.serverPort, this.serverIp, (err) => {
                if (err) {
                    clearTimeout(timeout);
                    reject(err);
                }
            });
        });
    }

    async getAllTemperatures() {
        const tempMetrics = [
            'hk_ocxo_temp', 'hk_ifamp_temp', 'hk_lo_temp', 'hk_tec_temp',
            'hk_backend_chassis_temp', 'hk_nic_temp', 'hk_rfsoc_chassis_temp',
            'hk_rfsoc_chip_temp', 'hk_lna1_temp', 'hk_lna2_temp'
        ];

        const temps = {};
        for (const metric of tempMetrics) {
            try {
                const value = await this.getMetric(metric);
                temps[metric] = parseFloat(value);
            } catch (error) {
                temps[metric] = null;
            }
        }
        return temps;
    }

    close() {
        this.client.close();
    }
}
```

## GUI Display Recommendations

### Temperature Display
- **Color coding**: 
  - Green: Normal operating range
  - Yellow: Warning range  
  - Red: Critical range
  - Gray: Sensor error (-999.0)

### Pressure Display
- **Units**: Provide unit selection (bar, PSI, Torr)
- **Ranges**: 
  - Normal: 0-25 PSI (0-1.72 bar)
  - Invalid: Show "INVALID" for invalid readings

### Status Indicators
- **Power Status**: LED-style indicator (green=on, red=off)
- **Data Collection**: LED-style indicator (green=running, red=stopped)
- **Data Ready Flags**: Small indicators next to sensor readings

### Update Frequency
- **Recommended**: 1-2 second intervals for live display
- **Avoid**: Polling faster than 0.5 seconds (may overwhelm server)

## Troubleshooting

### Common Issues

1. **Connection Refused**
   - Check if BCP is running
   - Verify IP address and port
   - Check firewall settings

2. **Timeout Errors**
   - Increase socket timeout
   - Check network connectivity
   - Verify server is responding

3. **Invalid Data (-999.0)**
   - Housekeeping system may not be powered on
   - Data collection may not be started
   - Sensor hardware issue

4. **Missing Metrics**
   - Check if housekeeping is enabled in config
   - Verify correct metric ID spelling
   - Check server logs

### Testing Connection

```bash
# Test server connectivity with netcat
echo "hk_powered" | nc -u BCP_IP_ADDRESS 8002

# Expected response: "1" (if powered) or "0" (if not powered)
```

## System Commands

While this guide focuses on telemetry access, housekeeping can be controlled via BCP commands:
- `housekeeping_on` - Power on the system
- `start_housekeeping` - Start data collection
- `stop_housekeeping` - Stop data collection  
- `housekeeping_off` - Power off the system

Refer to BCP command documentation for implementation details.
