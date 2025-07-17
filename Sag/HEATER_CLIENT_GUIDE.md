# Heater System UDP Client Implementation Guide

## Overview
The heater system runs a UDP server on **port 8006** that accepts relay toggle commands and responds with success/failure status.

## Server Configuration
- **IP**: `172.20.3.16` (configurable in `bcp_Sag.config`)
- **Port**: `8006`
- **Protocol**: UDP
- **Timeout**: 20ms (20000μs)

## Command Protocol

### Available Commands
| Command | Relay ID | Heater Type | Description |
|---------|----------|-------------|-------------|
| `toggle_lockpin` | 0 | Temp-controlled | Lock pin heater |
| `toggle_starcamera` | 1 | Temp-controlled | Star camera heater |
| `toggle_PV` | 2 | Temp-controlled | PV panel heater |
| `toggle_motor` | 3 | Temp-controlled | Motor heater |
| `toggle_ethernet` | 4 | Manual-only | Ethernet switch heater |

### Response Format
- **Success**: `"1"` (string)
- **Failure**: `"0"` (string)

## Python Client Example

```python
import socket
import time

class HeaterClient:
    def __init__(self, host='172.20.3.16', port=8006, timeout=1.0):
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
    
    def toggle_lockpin(self):
        return self.send_command("toggle_lockpin")
    
    def toggle_starcamera(self):
        return self.send_command("toggle_starcamera")
    
    def toggle_pv(self):
        return self.send_command("toggle_PV")
    
    def toggle_motor(self):
        return self.send_command("toggle_motor")
    
    def toggle_ethernet(self):
        return self.send_command("toggle_ethernet")
    
    def close(self):
        self.sock.close()

# Usage Example
if __name__ == "__main__":
    client = HeaterClient()
    
    # Toggle heaters
    if client.toggle_lockpin():
        print("Lock pin heater toggled successfully")
    
    if client.toggle_ethernet():
        print("Ethernet heater toggled successfully")
    
    client.close()
```

## Quick Test Script

```python
import socket

def test_heater_command(command, host='172.20.3.16', port=8006):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    try:
        sock.sendto(command.encode(), (host, port))
        response, _ = sock.recvfrom(1024)
        print(f"Command: {command} -> Response: {response.decode()}")
        return response.decode().strip() == "1"
    except Exception as e:
        print(f"Error: {e}")
        return False
    finally:
        sock.close()

# Test all commands
commands = ["toggle_lockpin", "toggle_starcamera", "toggle_PV", "toggle_motor", "toggle_ethernet"]
for cmd in commands:
    test_heater_command(cmd)
```

## Important Notes

1. **Commands are case-sensitive** - must match exactly
2. **Heaters 0-3** are temperature-controlled (28-30°C automatic + manual override)
3. **Heater 4** is manual-only control
4. **3A current limit** applies across all heaters
5. **Toggle behavior**: ON→OFF or OFF→ON regardless of current state
6. Server only accepts **one command per UDP packet**
7. **No status query** - server only responds to toggle commands

## System Requirements
- Heater system must be started with `start_heaters` command in BCP
- LabJack T7 hardware must be accessible at configured IP
- PBoB relay power must be enabled for heater box 