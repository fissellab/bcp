# BCP + bvex-link Integrated System

## Overview
This directory now contains an integrated system that combines:
- **BCP (Binary Computer Program)** - Flight computer system with GPS, spectrometer control, and data logging
- **bvex-link** - Redis and C++ based telemetry server for communication between flight computers and ground station

## Architecture

### Startup Sequence
1. **bvex-link server starts first** (Redis + C++ onboard server)
   - Redis runs on port 6379 (Docker container)
   - C++ server listens on UDP ports 3000, 3001, 3002, 8080
   - Ready to receive telemetry data from BCP
2. **BCP starts second** 
   - Can immediately begin transmitting data through bvex-link
   - GPS, spectrometer, and logging systems available

### Shutdown Sequence
1. **BCP stops first** (graceful data production stop)
   - Uses existing `exit` command functionality
   - Stops spec scripts and GPS logging cleanly
2. **bvex-link server stops second** (clean telemetry shutdown)
   - Stops C++ server processes
   - Stops Redis container

## Usage

### Start Both Systems
```bash
sudo ./start.sh
```
- **Requirements**: sudo (for GPS access and Docker)
- **What it does**:
  - Starts bvex-link server (Redis + C++ telemetry)
  - Starts BCP with GPS privileges
  - Sets up signal handlers for graceful shutdown

### Stop Both Systems

#### Method 1: Ctrl+C (Recommended)
- Press `Ctrl+C` in the terminal running `./start.sh`
- Automatically triggers graceful shutdown of both systems

#### Method 2: Manual Stop Script
```bash
sudo ./stop.sh
```
- Can be run from any terminal
- Useful if start script was terminated unexpectedly
- Finds and stops all related processes

### Check System Status
```bash
./status.sh
```
- Shows status of both BCP and bvex-link
- Displays process IDs, resource usage, and port status
- No sudo required

## Files

### New/Modified Files
- `start.sh` - **Modified**: Integrated startup with bvex-link
- `stop.sh` - **New**: Manual stop script for both systems
- `status.sh` - **New**: Status monitoring for both systems
- `start.sh.backup` - **Backup**: Original BCP-only start script

### Key bvex-link Files (External)
- `/home/mayukh/bvex-link/start-bvex-cpp.sh` - Start bvex-link server
- `/home/mayukh/bvex-link/stop-bvex-cpp.sh` - Stop bvex-link server
- `/home/mayukh/bvex-link/check-bvex-cpp.sh` - Check bvex-link status

## Communication Flow

```
BCP → bvex-link C++ Server → Redis → Ground Station
```

1. **BCP** produces telemetry data (GPS, spectrometer, logs)
2. **bvex-link C++ Server** receives data on UDP ports 3000-3002, 8080
3. **Redis** stores/buffers the telemetry data
4. **Ground Station** can fetch data from Redis or receive forwarded UDP

## Error Handling

### If bvex-link fails to start:
- start.sh will exit with error message
- BCP will not start (prevents data loss)
- Check Docker and vcpkg dependencies

### If BCP fails to start:
- bvex-link continues running (can be stopped manually)
- Check GPS device permissions and config file

### If shutdown hangs:
- SIGTERM is tried first (10 second timeout)
- SIGKILL is used as fallback
- Manual cleanup with `sudo ./stop.sh`

## Troubleshooting

### Check what's running:
```bash
./status.sh
```

### Manual bvex-link control:
```bash
# Start only bvex-link
sudo /home/mayukh/bvex-link/start-bvex-cpp.sh

# Stop only bvex-link  
sudo /home/mayukh/bvex-link/stop-bvex-cpp.sh

# Check only bvex-link
/home/mayukh/bvex-link/check-bvex-cpp.sh
```

### Check ports manually:
```bash
# UDP ports for bvex-link C++ server
netstat -ulnp | grep -E "(3000|3001|3002|8080)"

# TCP port for Redis
netstat -tlnp | grep 6379

# Check Docker containers
sudo docker ps
```

### View logs:
```bash
# BCP logs
tail -f main_sag.log

# Docker logs for Redis
sudo docker logs redis
```

## Dependencies

### BCP Dependencies (existing):
- GPS device access (requires sudo)
- Build tools (cmake, gcc)
- Python for spec scripts

### bvex-link Dependencies (external):
- Docker (for Redis)
- vcpkg (C++ package manager) 
- CMake and C++ compiler
- Build dependencies in `/home/mayukh/bvex-link/`

## Benefits of Integration

✅ **Seamless startup** - One command starts everything  
✅ **Proper shutdown** - Graceful stop prevents data loss  
✅ **Immediate telemetry** - BCP can transmit data right away  
✅ **Error handling** - Robust startup/shutdown with fallbacks  
✅ **Status monitoring** - Easy system health checking  
✅ **Backwards compatibility** - Preserves existing BCP workflow 