#!/bin/bash

# BCP Supervisor Installation Script for Ophiuchus
# Installs the supervisor systemd service for BCP Ophiuchus

set -e

echo "=== BCP Ophiuchus Supervisor Installation ==="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo "ERROR: This script must be run as root (use sudo)"
    echo "Usage: sudo ./install_supervisor_oph.sh"
    exit 1
fi

SCRIPT_DIR="/home/ophiuchus/bcp-mayukh/Oph"
SERVICE_FILE="$SCRIPT_DIR/bcp-supervisor-oph.service"
SYSTEMD_DIR="/etc/systemd/system"

# Verify files exist
if [[ ! -f "$SCRIPT_DIR/bcp_supervisor_oph.sh" ]]; then
    echo "ERROR: bcp_supervisor_oph.sh not found in $SCRIPT_DIR"
    exit 1
fi

if [[ ! -f "$SERVICE_FILE" ]]; then
    echo "ERROR: bcp-supervisor-oph.service not found in $SCRIPT_DIR"
    exit 1
fi

# Verify BCP executable exists
if [[ ! -f "$SCRIPT_DIR/build/main" ]]; then
    echo "ERROR: BCP executable build/main not found in $SCRIPT_DIR"
    echo "Please build BCP first"
    exit 1
fi

# Make sure supervisor script is executable
chmod +x "$SCRIPT_DIR/bcp_supervisor_oph.sh"
echo "✓ Made bcp_supervisor_oph.sh executable"

# Create log directory
mkdir -p /home/ophiuchus/bcp_supervisor_logs
chown ophiuchus:ophiuchus /home/ophiuchus/bcp_supervisor_logs
echo "✓ Created log directory: /home/ophiuchus/bcp_supervisor_logs"

# Copy service file to systemd
cp "$SERVICE_FILE" "$SYSTEMD_DIR/"
echo "✓ Installed service file: $SYSTEMD_DIR/bcp-supervisor-oph.service"

# Reload systemd
systemctl daemon-reload
echo "✓ Reloaded systemd daemon"

# Enable service for auto-start on boot (Oph needs to start automatically)
systemctl enable bcp-supervisor-oph.service
echo "✓ Enabled bcp-supervisor-oph service for auto-start on boot"

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Control commands:"
echo "  Start:   sudo systemctl start bcp-supervisor-oph"
echo "  Stop:    sudo systemctl stop bcp-supervisor-oph"
echo "  Status:  sudo systemctl status bcp-supervisor-oph"
echo "  Logs:    sudo journalctl -u bcp-supervisor-oph -f"
echo "  Restart: sudo systemctl restart bcp-supervisor-oph"
echo ""
echo "Supervisor logs: /home/ophiuchus/bcp_supervisor_logs/"
echo ""
echo "🚀 PRODUCTION MODE: Service enabled for auto-boot"
echo "   BCP will start automatically on system boot and send"
echo "   OPH_READY signals to Saggitarius (172.20.4.170:9001)"
echo ""
echo "Ready for stratospheric operations! 🎈"
