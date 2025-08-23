# Gyroscope Data Client Guide

## Overview
The BCP Sag system provides real-time gyroscope data via UDP telemetry server. The system supports:
- **SPI Gyroscope (ADXRS453)**: Single-axis rate gyroscope (primary telemetry data)

## Telemetry Server Configuration
- **Server IP**: `0.0.0.0` (listens on all interfaces)
- **Server Port**: `8082`
- **Protocol**: UDP
- **Data Rate**: Up to 10 Hz (client-controlled via request frequency)
- **Data Source**: Position sensor Pi at `172.20.4.209:65432`

## Available Gyroscope Channels

### SPI Gyroscope (ADXRS453) - Primary Telemetry Data
| Channel ID | Data Type | Units | Description |
|------------|-----------|-------|-------------|
| `pos_spi_gyro_rate` | float | deg/s | Angular velocity (single axis) |

### Position Sensor Status
| Channel ID | Data Type | Description |
|------------|-----------|-------------|
| `pos_status` | string | System status: "connected:yes,script:yes,data:yes" |
| `pos_running` | int | 1 if running, 0 if stopped |

## Client Implementation

### Python Example (10 Hz Gyro Data)
```python
import socket
import time
import json

class GyroClient:
    def __init__(self, server_ip="172.20.4.170", server_port=8082):
        self.server_ip = server_ip
        self.server_port = server_port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(1.0)  # 1 second timeout
    
    def request_data(self, channel):
        """Request data from specific gyro channel"""
        try:
            self.sock.sendto(channel.encode(), (self.server_ip, self.server_port))
            response, _ = self.sock.recvfrom(1024)
            return response.decode().strip()
        except socket.timeout:
            return "TIMEOUT"
        except Exception as e:
            return f"ERROR: {e}"
    
    def get_spi_gyro_rate(self):
        """Get SPI gyroscope rate data"""
        result = self.request_data("pos_spi_gyro_rate")
        try:
            return float(result)
        except ValueError:
            return None
    
    def check_sensor_status(self):
        """Check position sensor system status"""
        return self.request_data("pos_status")
    
    def stream_spi_gyro_10hz(self, duration=60):
        """Stream SPI gyro data at 10 Hz for specified duration"""
        print(f"Streaming SPI gyro data at 10 Hz for {duration} seconds...")
        print("Timestamp,SPI_Rate_deg_per_sec")
        
        start_time = time.time()
        while (time.time() - start_time) < duration:
            loop_start = time.time()
            
            # Get SPI gyro data
            spi_rate = self.get_spi_gyro_rate()
            
            # Print data
            timestamp = time.time()
            print(f"{timestamp:.3f},{spi_rate}")
            
            # Maintain 10 Hz rate (100ms intervals)
            elapsed = time.time() - loop_start
            sleep_time = 0.1 - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)
    
    def close(self):
        self.sock.close()

# Usage example
if __name__ == "__main__":
    client = GyroClient()
    
    # Test connection
    status = client.request_data("pos_status")
    print(f"Position sensor status: {status}")
    
    # Stream SPI gyro data for 30 seconds
    try:
        client.stream_spi_gyro_10hz(duration=30)
    except KeyboardInterrupt:
        print("\\nStopping stream...")
    finally:
        client.close()
```

### C/C++ Example
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
} gyro_client_t;

int gyro_client_init(gyro_client_t *client, const char *server_ip, int server_port) {
    client->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->sockfd < 0) return -1;
    
    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &client->server_addr.sin_addr);
    
    return 0;
}

float gyro_request_float(gyro_client_t *client, const char *channel) {
    char buffer[256];
    socklen_t addr_len = sizeof(client->server_addr);
    
    // Send request
    sendto(client->sockfd, channel, strlen(channel), 0, 
           (struct sockaddr*)&client->server_addr, addr_len);
    
    // Receive response
    int n = recvfrom(client->sockfd, buffer, sizeof(buffer)-1, 0, NULL, NULL);
    if (n > 0) {
        buffer[n] = '\\0';
        return atof(buffer);
    }
    return 0.0f;
}

void spi_gyro_stream_10hz(gyro_client_t *client, int duration_sec) {
    printf("Streaming SPI gyro data at 10 Hz for %d seconds...\\n", duration_sec);
    printf("Timestamp,SPI_Rate_deg_per_sec\\n");
    
    time_t start_time = time(NULL);
    while ((time(NULL) - start_time) < duration_sec) {
        struct timespec loop_start, loop_end;
        clock_gettime(CLOCK_MONOTONIC, &loop_start);
        
        // Get SPI gyro data
        float spi_rate = gyro_request_float(client, "pos_spi_gyro_rate");
        
        // Print data
        printf("%.3f,%.6f\\n", (double)time(NULL), spi_rate);
        
        // Maintain 10 Hz (100ms intervals)
        clock_gettime(CLOCK_MONOTONIC, &loop_end);
        long elapsed_ns = (loop_end.tv_sec - loop_start.tv_sec) * 1000000000L + 
                         (loop_end.tv_nsec - loop_start.tv_nsec);
        long sleep_ns = 100000000L - elapsed_ns;  // 100ms - elapsed
        if (sleep_ns > 0) {
            struct timespec sleep_time = {0, sleep_ns};
            nanosleep(&sleep_time, NULL);
        }
    }
}
```

## Data Flow Architecture
```
Position Sensor Pi (172.20.4.209:65432)
    ↓ ~1000 Hz raw sensor data
BCP Sag Position Sensor Client
    ↓ Latest data cached in memory
BCP Sag Telemetry Server (Port 8082)
    ↓ UDP requests/responses
Client Applications (10 Hz requests)
```

## Key Points

### Request-Response Model
- The telemetry server uses a **request-response** model
- Clients must request data at their desired frequency (up to 10 Hz)
- Each request returns the **latest** cached sensor data

### Data Freshness
- Sensor data is updated at ~1000 Hz from the Pi
- Telemetry responses contain the most recent data available
- Data timestamps are included when available

### Error Handling
- Servers returns `"N/A"` when data is not available
- Network timeouts should be handled by clients
- Position sensor system status can be checked via `pos_status`

### Performance Considerations
- Maximum recommended request rate: **10 Hz per channel**
- Multiple channels can be requested simultaneously
- Consider network latency in timing calculations
- Use UDP for low-latency, high-frequency data access

## Troubleshooting

### No Data Response (`"N/A"`)
1. Check if position sensors are running: `pos_running`
2. Verify position sensor status: `pos_status`
3. Ensure Pi connection is active (check logs)

### Network Issues
1. Verify server IP: `172.20.4.170` (Sag system)
2. Confirm telemetry server port: `8082`
3. Check firewall settings
4. Test with simple channel like `pos_status`

### Timing Issues
1. Account for network latency in 10 Hz loops
2. Monitor actual loop timing
3. Consider using system clock for precise intervals
4. Handle timeouts gracefully

## Example Applications
- **Balloon attitude monitoring**: Stream SPI gyro rate at 10 Hz for single-axis rotation detection
- **Platform stability analysis**: Monitor SPI gyro rate for angular velocity changes
- **System health monitoring**: Check position sensor status and connectivity
- **Data logging**: Record SPI gyro data to files with timestamps
- **Real-time control**: Use SPI gyro rate for feedback control systems
