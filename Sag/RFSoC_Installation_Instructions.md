# RFSoC Clock Daemon Installation Instructions

This document provides step-by-step instructions for installing and configuring the RFSoC Clock Control daemon on casper@172.20.3.12.

## Prerequisites

- SSH access to casper@172.20.3.12
- `clock_setup.sh` script present at `/home/casper/clock_setup.sh`
- Root privileges for systemd service installation

## Files to Transfer

Transfer the following files to casper@172.20.3.12:

1. `rfsoc_daemon.py` → `/home/casper/rfsoc_daemon.py`
2. `rfsoc-daemon.service` → `/home/casper/rfsoc-daemon.service`

## Installation Steps

### 1. Transfer Files to casper

```bash
# From your local machine
scp rfsoc_daemon.py casper@172.20.3.12:/home/casper/
scp rfsoc-daemon.service casper@172.20.3.12:/home/casper/
```

### 2. Set Permissions on casper

```bash
ssh casper@172.20.3.12

# Make the daemon executable
chmod +x /home/casper/rfsoc_daemon.py

# Ensure clock script is executable
chmod +x /home/casper/clock_setup.sh

# Verify clock script exists and permissions
ls -la /home/casper/clock_setup.sh
```

### 3. Install Systemd Service

```bash
# Copy service file to systemd directory (requires root)
sudo cp /home/casper/rfsoc-daemon.service /etc/systemd/system/

# Reload systemd to recognize new service
sudo systemctl daemon-reload

# Enable service for automatic startup
sudo systemctl enable rfsoc-daemon.service

# Start the service
sudo systemctl start rfsoc-daemon.service
```

### 4. Verify Installation

```bash
# Check service status
sudo systemctl status rfsoc-daemon.service

# Check if daemon is listening on port 8005
sudo netstat -tlnp | grep :8005

# View daemon logs
sudo journalctl -u rfsoc-daemon.service -f

# Test connectivity from BCP system
# (Run this from 172.20.3.10)
telnet 172.20.3.12 8005
# Type 'ping' and press Enter - should receive {"status": "success", "message": "pong"}
```

## Configuration Details

- **Service**: Runs as root (required for hardware access)
- **Port**: 8005 (TCP)
- **Listen Interface**: All interfaces (0.0.0.0)
- **Clock Script**: `/home/casper/clock_setup.sh`
- **Log File**: `/var/log/rfsoc_daemon.log`
- **PID File**: `/home/casper/rfsoc_daemon.pid`

## Available Commands

The daemon accepts the following TCP commands:

1. **ping** - Connectivity test
2. **configure_clock** - Execute clock configuration script
3. **clock_status** - Check clock script availability

## Troubleshooting

### Service Won't Start
```bash
# Check detailed logs
sudo journalctl -u rfsoc-daemon.service --no-pager

# Check if port is already in use
sudo netstat -tlnp | grep :8005
```

### Permission Issues
```bash
# Ensure script is executable
chmod +x /home/casper/clock_setup.sh

# Check script dependencies
ls -la /lib/firmware/rfsoc4x2_lmk_CLKin0_extref_10M_PL_128M_LMXREF_256M.txt
ls -la /lib/firmware/rfsoc4x2_lmx_inputref_256M_outputref_512M.txt
ls -la /home/casper/bin/prg_rfpll
```

### Network Connectivity
```bash
# Check firewall rules
sudo iptables -L -n

# Test from BCP machine
ping 172.20.3.12
telnet 172.20.3.12 8005
```

## BCP Integration

Once the daemon is running on casper, the BCP system can use these commands:

- `rfsoc_configure_ocxo` - Configure RFSoC clock
- `rfsoc_clock_status` - Check clock configuration status  
- `rfsoc_check` - Test daemon connectivity

## Service Management

```bash
# Stop service
sudo systemctl stop rfsoc-daemon.service

# Start service
sudo systemctl start rfsoc-daemon.service

# Restart service
sudo systemctl restart rfsoc-daemon.service

# Disable automatic startup
sudo systemctl disable rfsoc-daemon.service

# View service configuration
sudo systemctl cat rfsoc-daemon.service
```

## Log Monitoring

```bash
# Follow real-time logs
sudo journalctl -u rfsoc-daemon.service -f

# View daemon log file
tail -f /var/log/rfsoc_daemon.log

# View recent logs
sudo journalctl -u rfsoc-daemon.service --since "1 hour ago"
``` 