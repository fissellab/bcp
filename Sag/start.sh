#!/bin/bash

# Run script for BCP Saggitarius (requires sudo for GPS access)
echo "=== BCP Saggitarius Startup Script ==="

# Check if running with sudo (required for GPS access)
if [ "$EUID" -ne 0 ]; then
    echo "⚠️  This script requires sudo privileges for GPS device access."
    echo "   Please run with: sudo ./start.sh"
    exit 1
fi

# Check if the application has been built
if [ ! -f "build/main" ]; then
    echo "❌ Application not found. Please build it first:"
    echo "   ./build_and_run.sh"
    exit 1
fi

# Check if config file exists
if [ ! -f "bcp_Sag.config" ]; then
    echo "❌ Configuration file 'bcp_Sag.config' not found."
    exit 1
fi

echo "✅ Starting BCP Saggitarius with GPS privileges..."
echo ""

# Run the application
exec ./build/main bcp_Sag.config