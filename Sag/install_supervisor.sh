#!/bin/bash

# BCP Supervisor Installation Script
# Installs the supervisor systemd service for BCP Saggitarius

set -e

echo "=== BCP Supervisor Installation ==="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo "ERROR: This script must be run as root (use sudo)"
    echo "Usage: sudo ./install_supervisor.sh"
    exit 1
fi

SCRIPT_DIR="/home/mayukh/bcp/Sag"
SERVICE_FILE="$SCRIPT_DIR/bcp-supervisor.service"
SYSTEMD_DIR="/etc/systemd/system"

# Verify files exist
if [[ ! -f "$SCRIPT_DIR/bcp_supervisor.sh" ]]; then
    echo "ERROR: bcp_supervisor.sh not found in $SCRIPT_DIR"
    exit 1
fi

if [[ ! -f "$SERVICE_FILE" ]]; then
    echo "ERROR: bcp-supervisor.service not found in $SCRIPT_DIR"
    exit 1
fi

# Make sure supervisor script is executable
chmod +x "$SCRIPT_DIR/bcp_supervisor.sh"
echo "✓ Made bcp_supervisor.sh executable"

# Create log directory
mkdir -p /home/mayukh/bcp_supervisor_logs
chown mayukh:mayukh /home/mayukh/bcp_supervisor_logs
echo "✓ Created log directory: /home/mayukh/bcp_supervisor_logs"

# Copy service file to systemd
cp "$SERVICE_FILE" "$SYSTEMD_DIR/"
echo "✓ Installed service file: $SYSTEMD_DIR/bcp-supervisor.service"

# Reload systemd
systemctl daemon-reload
echo "✓ Reloaded systemd daemon"

# Note: NOT enabling auto-start on boot yet (will do this after testing)
# systemctl enable bcp-supervisor.service
echo "✓ Service installed but NOT enabled for auto-boot (as requested)"

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Control commands:"
echo "  Start:   sudo systemctl start bcp-supervisor"
echo "  Stop:    sudo systemctl stop bcp-supervisor"
echo "  Status:  sudo systemctl status bcp-supervisor"
echo "  Logs:    sudo journalctl -u bcp-supervisor -f"
echo "  Restart: sudo systemctl restart bcp-supervisor"
echo ""
echo "Supervisor logs: /home/mayukh/bcp_supervisor_logs/"
echo ""
echo "⚠️  IMPORTANT: The supervisor will wait for Ophiuchus 'OPH_READY' signal"
echo "   on UDP port 9001 before starting BCP. Make sure Oph supervisor"
echo "   is configured to send this signal."
echo ""
echo "🧪 TESTING MODE: Service installed but not auto-enabled"
echo "   After testing, run: sudo systemctl enable bcp-supervisor"
echo ""
echo "Ready for testing! 🎈"
