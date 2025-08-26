# PR59 Fan Status Telemetry Client Guide

## New Telemetry Channel

The PR59 system now provides real-time fan status telemetry based on current override state.

### Request Channel
```
pr59_fan_status
```

### Response Values
- `"automatic"` - Fan in automatic cooling mode (normal operation)
- `"forced_on"` - Fan manually forced ON via `start_pr59_fan` command
- `"forced_off"` - Fan manually forced OFF via `stop_pr59_fan` command  
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

else:
    status = "PR59 not available"
```

### Notes
- Fan status reflects current override state set by ground station commands
- Status updates in real-time (1-second polling)
- Works independently of existing PR59 telemetry channels
- More reliable than hardware register polling (avoids communication conflicts)
