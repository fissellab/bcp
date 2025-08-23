# PR59 Fan Status Telemetry Client Guide

## New Telemetry Channel

The PR59 system now provides real-time fan status telemetry by reading hardware registers.

### Request Channel
```
pr59_fan_status
```

### Response Values
- `"automatic"` - Fan in automatic cooling mode (normal operation)
- `"forced_on"` - Fan manually forced ON via command
- `"forced_off"` - Fan manually forced OFF via command  
- `"error"` - Error reading fan status from hardware
- `"N/A"` - PR59 not running or data unavailable

### Usage Example
```bash
# Request fan status
echo "pr59_fan_status" | nc -u 172.20.4.170 8082

# Expected responses:
# automatic    <- Normal operation
# forced_on    <- Fan override enabled
# forced_off   <- Fan override disabled
```

### Client Integration
```python
# Python example
response = telemetry_client.request("pr59_fan_status")
if response == "automatic":
    status = "Normal cooling mode"
elif response == "forced_on":
    status = "Fan manually enabled"
elif response == "forced_off":
    status = "Fan manually disabled"
elif response == "error":
    status = "Hardware read error"
else:
    status = "PR59 not available"
```

### Notes
- Fan status is read directly from TEC controller hardware registers (16 & 23)
- Status updates in real-time (1-second polling)
- Works independently of existing PR59 telemetry channels
