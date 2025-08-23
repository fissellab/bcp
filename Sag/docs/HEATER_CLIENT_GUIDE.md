# Heater System Integration Guide

## Overview
The heater system is fully integrated into BCP Sag, providing automatic temperature control for critical components with individual manual override capabilities. The system manages 5 heaters via LabJack T7 hardware with real-time telemetry and BCP command interface.

## System Architecture

### Heater Configuration
| Heater ID | Type | Component | Control Mode | Temperature Range |
|-----------|------|-----------|--------------|-------------------|
| 0 | Automatic + Manual | Star Camera | Auto: 25.0°C - 30.0°C | Configurable |
| 1 | Automatic + Manual | Motor | Auto: 25.0°C - 30.0°C | Configurable |
| 2 | Automatic + Manual | Ethernet Switch | Auto: 25.0°C - 30.0°C | Configurable |
| 3 | Automatic + Manual | Lock Pin | Auto: 25.0°C - 30.0°C | Configurable |
| 4 | **Manual Only** | Pressure Vessel (PV) | Manual control only | No auto thresholds |

### Key Features
- **3A Total Current Limit** - System-wide protection (strictly enforced)
- **Individual Heater Control** - Each heater can be enabled/disabled independently
- **Priority-Based Control** - Coldest heaters get priority when multiple need heating
- **Real-Time Telemetry** - Temperature, current, and status monitoring
- **Immediate Response** - Manual commands take effect instantly
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
  port = 8006;                    # Internal UDP port (legacy - not used for control)
  server_ip = "0.0.0.0";          # Listen on all interfaces
  workdir = "/media/saggitarius/T7/heaters_data";  # Data directory
  current_cap = 3;                # Total current limit (amps) - STRICTLY ENFORCED
  timeout = 20000;                # Internal timeout (microseconds)
  
  # Temperature thresholds for automatic control (Celsius)
  temp_low_starcam = 25.0;        # Star camera heater ON below this temp
  temp_high_starcam = 30.0;       # Star camera heater OFF above this temp
  temp_low_motor = 25.0;          # Motor heater ON below this temp
  temp_high_motor = 30.0;         # Motor heater OFF above this temp
  temp_low_ethernet = 25.0;       # Ethernet heater ON below this temp
  temp_high_ethernet = 30.0;      # Ethernet heater OFF above this temp
  temp_low_lockpin = 25.0;        # Lock pin heater ON below this temp
  temp_high_lockpin = 30.0;       # Lock pin heater OFF above this temp
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

## BCP Command Interface

### Server Configuration
- **IP**: BCP Sag system (typically localhost or Sag computer IP)
- **Port**: `8090` (BCP main command interface)
- **Protocol**: BCP packet format over UDP
- **Legacy UDP Port**: `8006` (internal use only - not for control)

### Available Commands

#### Automatic Heater Control (Heaters 0-3)
| Command | Heater ID | Component | Behavior |
|---------|-----------|-----------|----------|
| `sc_heater_on` | 0 | Star Camera | Enable auto control (rejoin temperature loop) |
| `sc_heater_off` | 0 | Star Camera | Disable auto control (turn OFF and exit loop) |
| `motor_heater_on` | 1 | Motor | Enable auto control (rejoin temperature loop) |
| `motor_heater_off` | 1 | Motor | Disable auto control (turn OFF and exit loop) |
| `eth_heater_on` | 2 | Ethernet Switch | Enable auto control (rejoin temperature loop) |
| `eth_heater_off` | 2 | Ethernet Switch | Disable auto control (turn OFF and exit loop) |
| `lock_heater_on` | 3 | Lock Pin | Enable auto control (rejoin temperature loop) |
| `lock_heater_off` | 3 | Lock Pin | Disable auto control (turn OFF and exit loop) |

#### Manual Heater Control (Heater 4)
| Command | Heater ID | Component | Behavior |
|---------|-----------|-----------|----------|
| `pv_heater_on` | 4 | Pressure Vessel | Turn ON immediately (if current budget allows) |
| `pv_heater_off` | 4 | Pressure Vessel | Turn OFF immediately |

#### System Control Commands
| Command | Behavior |
|---------|----------|
| `start_heater_box` | Power up heater box via PBoB (wait 10s before starting heaters) |
| `start_heaters` | Start heater control system (enables auto heaters by default) |
| `stop_heaters` | Stop heater system and power down box (graceful 10s delay) |

### Control Logic

#### Automatic Heaters (0-3)
- **`*_heater_on`**: Heater rejoins automatic temperature control loop
  - Will turn ON/OFF based on current temperature vs thresholds
  - Respects 3A current limit
  - Takes effect on next loop iteration (~1 second)
  
- **`*_heater_off`**: Heater leaves automatic control loop
  - Immediately turns OFF and stays OFF
  - Ignores temperature readings
  - Frees up current budget for other heaters

#### Manual Heater (4 - PV)
- **`pv_heater_on`**: Attempts immediate turn ON
  - Success: If current budget allows (≤3A total)
  - Failure: If insufficient current budget - command fails immediately
  - No queuing - retry when other heaters turn OFF

- **`pv_heater_off`**: Always immediate turn OFF
  - Always succeeds regardless of current state
  - Frees up current budget immediately

### Current Budget Management
- **3A System Limit**: Strictly enforced at all times
- **Auto Heaters**: Current limit checked within automatic control loop
- **PV Heater**: Current budget verified before manual ON command
- **Safety**: No command can exceed the 3A limit under any circumstances

## Telemetry Integration (Read-Only for GUI)

### Server Configuration  
- **IP**: BCP Sag system (typically localhost or Sag computer IP)
- **Port**: `8082` (BCP telemetry server)
- **Protocol**: UDP
- **Purpose**: **Display-only** - GUI clients use this for monitoring, not control

### Available Telemetry Channels
Connect to telemetry server on port `8082` and request these channels:

#### System Status
- `heater_running` - Returns 1 if heater system is active, 0 if stopped
- `heater_total_current` - Total current consumption (amps)

#### Individual Heater Data
For each heater (starcam, motor, ethernet, lockpin, spare):
- `heater_[name]_temp` - Temperature reading (°C)
- `heater_[name]_current` - Current consumption (amps) 
- `heater_[name]_state` - Heater state (1=ON, 0=OFF)

### Telemetry Client Example (GUI Read-Only)
```python
import socket

class HeaterTelemetryClient:
    """
    Read-only telemetry client for GUI monitoring.
    This client can ONLY read heater data - no control capabilities.
    """
    def __init__(self, host='localhost', port=8082, timeout=2.0):
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
        """Get comprehensive heater status for display"""
        status = {}
        status['running'] = self.get_telemetry('heater_running')
        status['total_current'] = self.get_telemetry('heater_total_current')
        
        # Note: Using correct telemetry channel names
        heaters = ['starcam', 'motor', 'ethernet', 'lockpin', 'spare']
        for heater in heaters:
            status[heater] = {
                'temp': self.get_telemetry(f'heater_{heater}_temp'),
                'current': self.get_telemetry(f'heater_{heater}_current'), 
                'state': self.get_telemetry(f'heater_{heater}_state')
            }
        return status

# Usage for GUI display
telemetry = HeaterTelemetryClient()
status = telemetry.get_heater_status()
print(f"System running: {status['running']}")
print(f"Total current: {status['total_current']} A")
print(f"Star camera: {status['starcam']['temp']}°C, {status['starcam']['state']}")
print(f"Motor: {status['motor']['temp']}°C, {status['motor']['state']}")
print(f"Ethernet: {status['ethernet']['temp']}°C, {status['ethernet']['state']}")
print(f"Lock pin: {status['lockpin']['temp']}°C, {status['lockpin']['state']}")
print(f"PV: {status['spare']['temp']}°C, {status['spare']['state']}")
```

## System Commands (Ground Station Control)

### BCP Command Integration
Send these commands to the main BCP command interface on **port 8090**:

#### System-Level Commands
```python
# Power up heater box (via PBoB) - wait 10 seconds before next command
send_bcp_command("start_heater_box")

# Start heater control system (enables all auto heaters by default)
send_bcp_command("start_heaters")

# Stop heater system (graceful shutdown with 10s delay)
send_bcp_command("stop_heaters")
```

#### Individual Heater Control Commands
```python
# Star Camera Heater (Auto + Manual)
send_bcp_command("sc_heater_on")    # Enable auto control
send_bcp_command("sc_heater_off")   # Disable auto control (turn OFF)

# Motor Heater (Auto + Manual)
send_bcp_command("motor_heater_on")   # Enable auto control
send_bcp_command("motor_heater_off")  # Disable auto control (turn OFF)

# Ethernet Switch Heater (Auto + Manual)
send_bcp_command("eth_heater_on")    # Enable auto control
send_bcp_command("eth_heater_off")   # Disable auto control (turn OFF)

# Lock Pin Heater (Auto + Manual)  
send_bcp_command("lock_heater_on")   # Enable auto control
send_bcp_command("lock_heater_off")  # Disable auto control (turn OFF)

# Pressure Vessel Heater (Manual Only)
send_bcp_command("pv_heater_on")     # Turn ON (if current budget allows)
send_bcp_command("pv_heater_off")    # Turn OFF (always succeeds)
```

### Command Usage Notes
- **Individual control**: Each heater can be controlled independently
- **Auto heaters (0-3)**: ON = rejoin auto loop, OFF = leave auto loop
- **PV heater (4)**: ON = immediate turn ON (if ≤3A), OFF = immediate turn OFF
- **Current safety**: All commands respect the 3A total current limit
- **Immediate response**: Commands take effect within ~1 second

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

### Quick Telemetry Test Script
```python
import socket
import time

def test_heater_telemetry():
    """Test heater telemetry interface (read-only)"""
    print("Testing heater telemetry interface...")
    
    # Test telemetry server
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    
    telemetry_channels = [
        "heater_running",
        "heater_total_current", 
        "heater_starcam_temp",
        "heater_starcam_state",
        "heater_motor_temp",
        "heater_motor_state",
        "heater_ethernet_temp", 
        "heater_ethernet_state",
        "heater_lockpin_temp",
        "heater_lockpin_state",
        "heater_spare_temp",
        "heater_spare_state"
    ]
    
    print(f"Connecting to telemetry server on port 8082...")
    
    for channel in telemetry_channels:
        try:
            sock.sendto(channel.encode(), ("localhost", 8082))
            response, _ = sock.recvfrom(1024)
            print(f"{channel}: {response.decode().strip()}")
        except Exception as e:
            print(f"{channel}: ERROR - {e}")
        time.sleep(0.1)
    
    sock.close()
    print("\nTelemetry test complete.")

def test_heater_display():
    """Generate a simple status display"""
    from datetime import datetime
    
    client = HeaterTelemetryClient()
    status = client.get_heater_status()
    
    print("\n" + "="*50)
    print(f"HEATER SYSTEM STATUS - {datetime.now().strftime('%H:%M:%S')}")
    print("="*50)
    print(f"System Running: {status['running']}")
    print(f"Total Current:  {status['total_current']} A / 3.0 A")
    print("-"*50)
    
    heaters = [
        ("Star Camera", "starcam"),
        ("Motor", "motor"), 
        ("Ethernet", "ethernet"),
        ("Lock Pin", "lockpin"),
        ("Pressure Vessel", "spare")
    ]
    
    for name, key in heaters:
        state = "ON" if status[key]['state'] == '1' else "OFF"
        print(f"{name:15}: {status[key]['temp']:>6}°C | {state:>3} | {status[key]['current']:>6} A")
    
    print("="*50)

if __name__ == "__main__":
    test_heater_telemetry()
    test_heater_display()
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

### PV Heater (Heater 4) Special Behavior
- **Manual-only operation** - no automatic temperature control
- **Current budget checking** - `pv_heater_on` fails if ≤3A limit would be exceeded
- **Immediate response** - no queuing, retry when other heaters turn OFF
- **Always monitored** - temperature and current telemetry available

### Network Configuration
- **BCP Command Interface**: Port `8090` (individual heater control)
- **Telemetry Server**: Port `8082` (read-only monitoring for GUI)
- **LabJack IP**: Must be accessible at `172.20.4.178`
- **Legacy UDP Port**: `8006` (internal use only - not for GUI control)

## Troubleshooting

### Common Issues
1. **"Heater system not running"**: Ensure `start_heaters` was called first
2. **"PV heater ON failed"**: Check current budget - total may be near 3A limit
3. **"Command not recognized"**: Verify command name spelling (e.g., `sc_heater_on`)
4. **"Telemetry timeout"**: Check telemetry server on port 8082
5. **"Temperature reads N/A"**: Heater system may not be running or hardware issue

### Debug Steps
1. **Check system status**: Request `heater_running` telemetry channel
2. **Verify LabJack connectivity**: `ping 172.20.4.178`
3. **Monitor current usage**: Request `heater_total_current` channel 
4. **Check individual states**: Request `heater_[name]_state` for each heater
5. **Review logs**: Check heater log files in `/media/saggitarius/T7/heaters_data/`
6. **Validate thresholds**: Ensure temperature ranges are reasonable (25-30°C)

### GUI Client Troubleshooting
- **GUI can only display data** - no control capabilities
- **Connect to port 8082** for telemetry (not 8006 or 8090)
- **Expected telemetry channels**: See "Available Telemetry Channels" section
- **No response**: Verify BCP Sag system is running and telemetry server enabled

## Summary

### New Individual Heater Control System
The heater system now provides **granular individual control** for each heater:

#### 🔧 **Control Interface**
- **Commands**: Individual `*_heater_on/off` commands via BCP (port 8090)
- **GUI Role**: **Display-only** telemetry monitoring (port 8082)
- **No UDP toggles**: Legacy toggle commands removed from GUI interface

#### 🎯 **Key Features**
- **Individual Control**: Each heater can be enabled/disabled independently
- **Simple Logic**: ON = rejoin auto loop, OFF = leave auto loop (auto heaters)
- **Current Safety**: 3A limit strictly enforced for all scenarios
- **Immediate Response**: Manual commands take effect within ~1 second
- **PV Heater**: Manual-only with current budget checking

#### 📊 **GUI Integration**
- **Port 8082**: Telemetry server for read-only monitoring
- **Available Data**: Temperature, current, and ON/OFF state for each heater
- **Real-time**: Updates reflect actual heater states and measurements
- **No Control**: GUI cannot send commands - control via ground station only

The heater system is now fully operational with individual control capabilities and ready for mission deployment! 🚀