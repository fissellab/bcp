# GPS Telemetry Parameters

## Available GPS Parameters

| Parameter | Description | Units | Precision |
|-----------|-------------|-------|-----------|
| `gps_lat` | Latitude | degrees | 6 decimal places |
| `gps_lon` | Longitude | degrees | 6 decimal places |
| `gps_alt` | Altitude | meters | 1 decimal place |
| `gps_head` | Heading | degrees | 2 decimal places |
| `gps_speed` | Speed | m/s | 3 decimal places |
| `gps_sats` | Satellite count | count | integer |
| `gps_time` | GPS time | string | YYYY-MM-DD HH:MM:SS |
| `gps_status` | Position/heading validity | string | pos:valid/invalid,head:valid/invalid |
| `gps_logging` | Logging status | integer | 1=active, 0=inactive |

## Server Ports

- **Main Telemetry Server**: Port `8082` (UDP)
- **GPS UDP Server**: Port `8080` (UDP)

## Client Requests

### Individual Parameters
Send parameter name as UDP message:
```
gps_lat
gps_speed
gps_sats
```

### Bulk GPS Data
Send bulk request:
```
GET_GPS
```

**Response format:**
```
gps_lat:44.225081,gps_lon:-76.497499,gps_alt:123.4,gps_head:85.23,gps_speed:2.571,gps_sats:8
```

## Invalid Data
Parameters return `N/A` when GPS data is invalid or unavailable.

## Example Client Code (Python)
```python
import socket

def get_gps_data(server_ip="127.0.0.1", server_port=8082):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    
    # Get all GPS data
    sock.sendto(b"GET_GPS", (server_ip, server_port))
    response, _ = sock.recvfrom(1024)
    
    # Get individual parameter
    sock.sendto(b"gps_speed", (server_ip, server_port))
    speed, _ = sock.recvfrom(1024)
    
    sock.close()
    return response.decode(), speed.decode()
```