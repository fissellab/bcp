# VLBI Setup Notes for Backend (Aquila)

## File Structure Required on Aquila

The following files need to be in `/home/aquila/casper/4x2_100gbe_2bit/`:

1. `start_vlbi_logging.sh` - Main VLBI logging script (copied from our repo)
2. `vlbi_data_logging_listener.py` - Data capture listener (copied from our repo)  
3. `tut_100g_catcher.py` - Packet catcher (should already exist on aquila)

## VLBI Controller Daemon

The VLBI controller daemon should be at:
- `/home/aquila/vlbi_controller.py` (copied from our src/vlbi_controller.py)

## Missing Files

The following file is referenced by start_vlbi_logging.sh but not in our repo:
- `tut_100g_catcher.py` - This should already exist on the aquila backend system

## Data Flow Verification

The VLBI data logging system works as follows:
1. BCP sends `start_vlbi_2` command to VLBI daemon
2. VLBI daemon calls `start_vlbi_logging.sh 2`
3. Start script sets `VLBI_DATA_DIR="/mnt/vlbi_data_2"`
4. Start script calls `vlbi_data_logging_listener.py` with `-d /mnt/vlbi_data_2`
5. Listener script writes data to `/mnt/vlbi_data_2/vlbi_capture_YYYYMMDD_HHMMSS/`

## Expected Directory Structure on Backend

After successful VLBI logging, data should appear in:
```
/mnt/vlbi_data_2/
├── vlbi_capture_20250813_163000/
│   ├── vlbi_data_20250813_163000.bin
│   └── vlbi_metadata_20250813_163000.json
└── vlbi_capture_20250813_164000/
    ├── vlbi_data_20250813_164000.bin
    └── vlbi_metadata_20250813_164000.json
```

## Setup Commands for Aquila

### 1. Copy VLBI files to aquila:
```bash
scp start_vlbi_logging.sh aquila@172.20.4.173:/home/aquila/casper/4x2_100gbe_2bit/
scp vlbi_data_logging_listener.py aquila@172.20.4.173:/home/aquila/casper/4x2_100gbe_2bit/
scp src/vlbi_controller.py aquila@172.20.4.173:/home/aquila/
```

### 2. Install Aquila System Monitor:
```bash
# Copy files to aquila
scp src/aquila_system_monitor.c aquila@172.20.4.173:/tmp/
scp Makefile.aquila aquila@172.20.4.173:/tmp/
scp aquila-system-monitor.service aquila@172.20.4.173:/tmp/

# On aquila, build and install
ssh aquila@172.20.4.173
cd /tmp
mkdir -p build
gcc -Wall -Wextra -O2 -std=c99 -D_GNU_SOURCE aquila_system_monitor.c -lpthread -o build/aquila_system_monitor

# Install system monitor
sudo cp build/aquila_system_monitor /usr/local/bin/
sudo chmod +x /usr/local/bin/aquila_system_monitor
sudo cp aquila-system-monitor.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable aquila-system-monitor.service
sudo systemctl start aquila-system-monitor.service
```

### 3. Restart VLBI controller service:
```bash
sudo systemctl restart vlbi-controller
```

### 4. Check services status:
```bash
sudo systemctl status vlbi-controller
sudo systemctl status aquila-system-monitor
```
