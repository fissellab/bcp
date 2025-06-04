#!/bin/bash

echo "Testing GPS Telemetry Integration"
echo "================================="

# Check if remote bvex-link server is reachable
echo "Checking remote bvex-link server connectivity..."
if ping -c 1 -W 5 172.20.3.11 > /dev/null 2>&1; then
    echo "✅ Remote server 172.20.3.11 is reachable"
    
    # Check if the server is listening on port 3000
    if nc -z -w5 172.20.3.11 3000 2>/dev/null; then
        echo "✅ bvex-link server detected on 172.20.3.11:3000"
    else
        echo "❌ bvex-link server NOT responding on 172.20.3.11:3000"
        echo "   Make sure to start it on the remote computer:"
        echo "   ssh to 172.20.3.11 and run: cd /path/to/bvex-link && ./start-bvex-cpp.sh"
        exit 1
    fi
else
    echo "❌ Remote server 172.20.3.11 is not reachable"
    echo "   Check network connectivity and ensure the remote computer is running"
    exit 1
fi

# Build the project
echo "Building BCP with telemetry integration..."
./build_and_run.sh

if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo "✅ Build successful"
echo ""
echo "🚀 Ready to test!"
echo ""
echo "To run with telemetry:"
echo "  sudo ./start.sh"
echo ""
echo "The system will:"
echo "  1. Read telemetry config from bcp_Sag.config"
echo "  2. Connect to bvex-link server on 172.20.3.11:3000"
echo "  3. Send GPS data automatically: gps_lat, gps_lon, gps_alt, gps_heading, etc."
echo ""
echo "Monitor telemetry with:"
echo "  ssh 172.20.3.11  # Connect to remote server"
echo "  netstat -ulnp | grep 3000  # Check server connections" 