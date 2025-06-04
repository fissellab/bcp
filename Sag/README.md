# BCP Saggitarius with GPS Telemetry

BCP (Basic Control Program) Saggitarius with integrated bvex-link GPS telemetry support.

## What's New: GPS Telemetry Integration

The GPS system now automatically sends telemetry data to a bvex-link server:

- **gps_lat**, **gps_lon**, **gps_alt** - Position data
- **gps_heading** - Compass heading 
- **gps_hour**, **gps_minute**, **gps_second** - GPS time
- **gps_valid_pos** - Position validity flag

## Quick Start

1. **Start bvex-link server**:
   ```bash
   cd /home/mayukh/bvex-link
   ./start-bvex-cpp.sh
   ```

2. **Build and run BCP**:
   ```bash
   ./build_and_run.sh
   sudo ./start.sh
   ```

## Telemetry Configuration

Edit `bcp_Sag.config` to configure GPS telemetry:

```
gps:
{
  enabled = 1;
  port = "/dev/ttyGPS";
  baud_rate = 19200;
  data_save_path = "/media/saggitarius/T7/GPS_data";
  file_rotation_interval = 14400;
  
  # Telemetry settings
  telemetry_enabled = 1;        # 1 = enabled, 0 = disabled
  telemetry_host = "localhost"; # bvex-link server host
  telemetry_port = "3000";      # bvex-link server port
};
```

## Testing

Run the test script to verify everything is working:

```bash
./test_telemetry.sh
```

## System Requirements

- Linux-based operating system
- GCC compiler
- CMake 3.24+
- vcpkg (with $VCPKG_ROOT set)
- libconfig library
- libjson-c-dev
- bvex-link built and available
- GPS device permissions

## Architecture

```
GPS Hardware → BCP GPS Module → bvex-link Client → bvex-link Server → Target System
```

When GPS data is received, it's automatically sent via UDP to the bvex-link server, which forwards it to the configured target system for telemetry monitoring.

## Building

The system uses CMake and includes bvex-link as a dependency:

```bash
./build_and_run.sh  # Builds everything including bvex-link integration
sudo ./start.sh     # Runs with GPS device permissions
```

## Command Line Interface

Available commands in the BCP application:
- `start spec` - Start the spectrometer
- `stop spec` - Stop the spectrometer  
- `start gps` - Start GPS logging
- `stop gps` - Stop GPS logging
- `gps status` - Show GPS status
- `print <msg>` - Print a message
- `exit` - Exit the program
