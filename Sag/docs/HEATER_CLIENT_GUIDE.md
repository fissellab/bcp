# Heater System Integration Guide

## Overview
The heater system is now fully integrated into BCP Sag, providing automatic temperature control for critical components with manual override capabilities. The system manages 5 heaters via LabJack T7 hardware with real-time telemetry and UDP command interface.

## System Architecture

### Heater Configuration
| Heater ID | Type | Component | Control Mode | Temperature Range |
|-----------|------|-----------|--------------|-------------------|
| 0 | Automatic + Manual | Star Camera | Auto: 28.0°C - 30.0°C | Configurable |
| 1 | Automatic + Manual | Motor | Auto: 25.0°C - 27.0°C | Configurable |
| 2 | Automatic + Manual | Ethernet Switch | Auto: 20.0°C - 22.0°C | Configurable |
| 3 | Automatic + Manual | Lock Pin | Auto: 15.0°C - 17.0°C | Configurable |
| 4 | **Manual Only** | Spare/General | Manual control only | No auto thresholds |

### Key Features
- **3A Total Current Limit** - System-wide protection
- **Priority-Based Control** - Coldest heaters get priority
- **Real-Time Telemetry** - Temperature, current, and status monitoring
- **Graceful Shutdown** - 10-second delay before power cutoff
- **PBoB Integration** - Power control via relay system

## Configuration

### BCP Configuration File (`bcp_Sag.config`)
```
heaters:
{
  enabled = 1;                    # Enable heater system
  logfile = "log/heater.log";     # Log file path
  heater_ip = "172.20.4.178";     # LabJack T7 IP address
  pbob_id = 2;                    # PBoB ID for heater box power
  relay_id = 0;                   # Relay ID for heater box power
  port = 8006;                    # UDP server port for commands
  server_ip = "0.0.0.0";          # Listen on all interfaces
  workdir = "./heaters_data";     # Data directory
  current_cap = 3;                # Total current limit (amps)
  timeout = 20000;                # UDP timeout (microseconds)
  
  # Temperature thresholds for automatic control (Celsius)
  temp_low_starcam = 28.0;        # Star camera heater ON below this temp
  temp_high_starcam = 30.0;       # Star camera heater OFF above this temp
  temp_low_motor = 25.0;          # Motor heater ON below this temp
  temp_high_motor = 27.0;         # Motor heater OFF above this temp
  temp_low_ethernet = 20.0;       # Ethernet heater ON below this temp
  temp_high_ethernet = 22.0;      # Ethernet heater OFF above this temp
  temp_low_lockpin = 15.0;        # Lock pin heater ON below this temp
  temp_high_lockpin = 17.0;       # Lock pin heater OFF above this temp
};
```

## System Operations

### Startup Sequence
1. **Power up heater box**: Send `start_heater_box` command
2. **Wait for LabJack boot**: Allow ~10 seconds for hardware initialization
3. **Start heater control**: Send `start_heaters` command

### Shutdown Sequence
1. **Stop heaters**: Send `stop_heaters` command
2. **Graceful delay**: System waits 10 seconds for heaters to settle
3. **Power cutoff**: Heater box power is automatically turned OFF

## UDP Command Interface

### Server Configuration
- **IP**: `172.20.4.178` (LabJack T7 address)
- **Port**: `8006`
- **Protocol**: UDP
- **Timeout**: 20ms (20000μs)

### Available Commands
| Command | Heater ID | Component | Behavior |
|---------|-----------|-----------|----------|
| `toggle_lockpin` | 0 | Star Camera | Toggle auto control enable/disable |
| `toggle_starcamera` | 1 | Motor | Toggle auto control enable/disable |
| `toggle_PV` | 2 | Ethernet | Toggle auto control enable/disable |
| `toggle_motor` | 3 | Lock Pin | Toggle auto control enable/disable |
| `toggle_ethernet` | 4 | Spare (Manual) | **Direct ON/OFF toggle** |

### Response Format
- **Success**: `"1"` (string)
- **Failure**: `"0"` (string)

### Control Logic
- **Heaters 0-3**: Toggle commands enable/disable automatic temperature control
  - When disabled: Heater remains at current state but ignores temperature
  - When re-enabled: Resumes automatic temperature control
- **Heater 4**: Toggle command directly turns heater ON/OFF (no auto control)

## Python Client Implementation

### Basic Client Class
```python
import socket
import time

class HeaterClient:
    def __init__(self, host='172.20.4.178', port=8006, timeout=2.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(timeout)
    
    def send_command(self, command):
        """Send command and return success status"""
        try:
            self.sock.sendto(command.encode(), (self.host, self.port))
            response, _ = self.sock.recvfrom(1024)
            return response.decode().strip() == "1"
        except socket.timeout:
            print(f"Timeout sending command: {command}")
            return False
        except Exception as e:
            print(f"Error: {e}")
            return False
    
    # Automatic control heaters (toggle auto control)
    def toggle_starcam_auto(self):
        """Toggle automatic control for star camera heater"""
        return self.send_command("toggle_lockpin")
    
    def toggle_motor_auto(self):
        """Toggle automatic control for motor heater"""
        return self.send_command("toggle_starcamera")
    
    def toggle_ethernet_auto(self):
        """Toggle automatic control for ethernet heater"""  
        return self.send_command("toggle_PV")
    
    def toggle_lockpin_auto(self):
        """Toggle automatic control for lock pin heater"""
        return self.send_command("toggle_motor")
    
    # Manual-only heater (direct control)
    def toggle_spare_heater(self):
        """Toggle spare heater ON/OFF directly"""
        return self.send_command("toggle_ethernet")
    
    def close(self):
        self.sock.close()
```

### Usage Examples
```python
# Initialize client
client = HeaterClient()

# Disable automatic control for star camera heater
if client.toggle_starcam_auto():
    print("Star camera auto control toggled")

# Toggle spare heater ON/OFF
if client.toggle_spare_heater():
    print("Spare heater toggled")

# Re-enable automatic control for motor heater
if client.toggle_motor_auto():
    print("Motor auto control re-enabled")

client.close()
```

## Telemetry Integration

### Available Telemetry Channels
Connect to telemetry server on port `8081` and request these channels:

#### System Status
- `heater_running` - Returns 1 if heater system is active, 0 if stopped
- `heater_total_current` - Total current consumption (amps)

#### Individual Heater Data
For each heater (starcam, motor, ethernet, lockpin, spare):
- `heater_[name]_temp` - Temperature reading (°C)
- `heater_[name]_current` - Current consumption (amps) 
- `heater_[name]_state` - Heater state (1=ON, 0=OFF)

### Telemetry Client Example
```python
import socket

class HeaterTelemetryClient:
    def __init__(self, host='localhost', port=8081, timeout=2.0):
        self.host = host
        self.port = port
        self.timeout = timeout
    
    def get_telemetry(self, channel):
        """Get telemetry value for specified channel"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(self.timeout)
        try:
            sock.sendto(channel.encode(), (self.host, self.port))
            response, _ = sock.recvfrom(1024)
            return response.decode().strip()
        except Exception as e:
            return f"ERROR: {e}"
        finally:
            sock.close()
    
    def get_heater_status(self):
        """Get comprehensive heater status"""
        status = {}
        status['running'] = self.get_telemetry('heater_running')
        status['total_current'] = self.get_telemetry('heater_total_current')
        
        heaters = ['starcam', 'motor', 'ethernet', 'lockpin', 'spare']
        for heater in heaters:
            status[heater] = {
                'temp': self.get_telemetry(f'heater_{heater}_temp'),
                'current': self.get_telemetry(f'heater_{heater}_current'),
                'state': self.get_telemetry(f'heater_{heater}_state')
            }
        return status

# Usage
telemetry = HeaterTelemetryClient()
status = telemetry.get_heater_status()
print(f"System running: {status['running']}")
print(f"Total current: {status['total_current']} A")
print(f"Star camera temp: {status['starcam']['temp']}°C")
```

## System Commands (Ground Station)

### BCP Command Integration
Send these commands to the main BCP command interface:

```python
# Power up heater box (via PBoB)
send_bcp_command("start_heater_box")

# Start heater control system  
send_bcp_command("start_heaters")

# Stop heater system (graceful shutdown)
send_bcp_command("stop_heaters")
```

## Advanced Features

### Current Limiting
- **3A system-wide limit** strictly enforced
- **Priority algorithm**: Heaters with greatest temperature deficit get priority
- **Smart allocation**: System prevents overcurrent conditions automatically

### Temperature Control Logic
- **Deadband control**: Heaters turn ON below `temp_low`, OFF above `temp_high`
- **Hysteresis**: Prevents rapid ON/OFF cycling
- **Individual thresholds**: Each heater type has optimized temperature ranges

### Error Handling
- **Connection timeouts**: 2-second default timeout for all operations
- **Invalid commands**: Server responds with "0" for malformed requests
- **Hardware failures**: System continues operating with remaining functional heaters
- **Logging**: All operations logged for debugging

## Testing and Validation

### Quick Test Script
```python
import socket
import time

def test_heater_system():
    # Test UDP command interface
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    
    commands = ["toggle_lockpin", "toggle_starcamera", "toggle_PV", 
                "toggle_motor", "toggle_ethernet"]
    
    print("Testing heater commands...")
    for cmd in commands:
        try:
            sock.sendto(cmd.encode(), ("172.20.4.178", 8006))
            response, _ = sock.recvfrom(1024)
            print(f"{cmd}: {'SUCCESS' if response.decode() == '1' else 'FAILED'}")
        except Exception as e:
            print(f"{cmd}: ERROR - {e}")
        time.sleep(0.5)
    
    sock.close()
    
    # Test telemetry interface
    print("\nTesting telemetry...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    
    try:
        sock.sendto(b"heater_running", ("localhost", 8081))
        response, _ = sock.recvfrom(1024)
        print(f"Heater system status: {response.decode()}")
    except Exception as e:
        print(f"Telemetry test failed: {e}")
    
    sock.close()

if __name__ == "__main__":
    test_heater_system()
```

## Important Notes

### Safety Considerations
1. **Always use `start_heater_box` before `start_heaters`**
2. **Allow 10 seconds between heater box power-up and starting control**
3. **Use `stop_heaters` for graceful shutdown** - never cut power directly
4. **Monitor total current** to ensure it stays below 3A limit

### Command Sequence
✅ **Correct startup**: `start_heater_box` → wait 10s → `start_heaters`
❌ **Incorrect**: Starting heaters before powering box will fail

### Heater 4 Special Behavior
- **No automatic temperature control** - purely manual operation
- **Direct state control** - toggle commands immediately turn ON/OFF
- **Still monitored** - temperature and current telemetry available

### Network Configuration
- **LabJack IP**: Must be accessible at `172.20.4.178`
- **UDP ports**: 8006 (commands), 8081 (telemetry)
- **Firewall**: Ensure UDP ports are not blocked

## Troubleshooting

### Common Issues
1. **"Command timeout"**: Check LabJack network connectivity
2. **"Heater system not running"**: Ensure `start_heaters` was called
3. **"Current limit exceeded"**: Some heaters may be disabled automatically
4. **"Invalid relay id"**: Check command spelling and case sensitivity

### Debug Steps
1. Verify LabJack connectivity: `ping 172.20.4.178`
2. Check telemetry status: Request `heater_running` channel
3. Monitor logs: Check heater log files in configured work directory
4. Validate configuration: Ensure all temperature thresholds are reasonable

The heater system is now fully operational and ready for mission deployment! 🚀 