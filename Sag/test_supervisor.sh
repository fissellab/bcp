#!/bin/bash

# Test script for BCP Supervisor
# Simulates Ophiuchus sending "OPH_READY" signal for testing

echo "=== BCP Supervisor Test Utility ==="
echo ""

if [[ $# -eq 0 ]]; then
    echo "Usage:"
    echo "  $0 send_ready    - Simulate Oph sending OPH_READY signal"
    echo "  $0 monitor       - Monitor supervisor logs in real-time"
    echo "  $0 status        - Check supervisor service status"
    echo "  $0 install       - Install supervisor service"
    echo "  $0 start         - Start supervisor service"
    echo "  $0 stop          - Stop supervisor service"
    echo ""
    exit 1
fi

case "$1" in
    "send_ready")
        echo "Sending OPH_READY signal to port 9001..."
        echo "OPH_READY" | nc -u -w1 localhost 9001
        echo "Signal sent!"
        ;;
        
    "monitor")
        echo "Monitoring supervisor logs (Ctrl+C to stop)..."
        echo "Supervisor logs:"
        tail -f /home/mayukh/bcp_supervisor_logs/*.log 2>/dev/null || echo "No supervisor log files found yet"
        ;;
        
    "status")
        echo "Supervisor service status:"
        sudo systemctl status bcp-supervisor || echo "Service not installed/started"
        echo ""
        echo "Recent supervisor logs:"
        sudo journalctl -u bcp-supervisor --no-pager -n 20 || echo "No systemd logs found"
        ;;
        
    "install")
        echo "Installing supervisor service..."
        sudo ./install_supervisor.sh
        ;;
        
    "start")
        echo "Starting supervisor service..."
        sudo systemctl start bcp-supervisor
        echo "Service started. Check status with: $0 status"
        ;;
        
    "stop")
        echo "Stopping supervisor service..."
        sudo systemctl stop bcp-supervisor
        echo "Service stopped."
        ;;
        
    *)
        echo "Unknown command: $1"
        echo "Run $0 without arguments to see usage"
        exit 1
        ;;
esac
