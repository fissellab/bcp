#!/bin/bash
echo "Starting bvex-link C++ system..."

# Start Redis
echo "Starting Redis..."
sudo docker start redis
sleep 2

# Check if Redis is running
if ! sudo docker ps | grep -q redis; then
    echo "Error: Redis failed to start"
    exit 1
fi

# Build and start C++ onboard server
echo "Building C++ onboard server (this may take a few minutes first time)..."
cd "$(dirname "$0")/onboard-server"

# Check if already built
if [ ! -d "build" ] || [ ! -f "build/main" ]; then
    echo "Building C++ dependencies and server..."
    if [ ! -d "vcpkg_installed" ]; then
        echo "Installing vcpkg dependencies..."
        vcpkg install
    fi
    
    cmake --preset=debug
    if [ $? -ne 0 ]; then
        echo "❌ Error: CMake configuration failed"
        exit 1
    fi
    
    cmake --build build
    if [ $? -ne 0 ]; then
        echo "❌ Error: Build failed"
        exit 1
    fi
fi

echo "Starting C++ onboard server..."
# C++ server expects: <target_address> <target_port>
# It listens on UDP ports 3000, 3001, 3002, 8080 and sends to target
./build/main localhost 9999 &
SERVER_PID=$!

# Wait a moment and check if server started
sleep 3
if netstat -ulnp | grep -E "(3000|3001|3002|8080)" > /dev/null; then
    echo "✅ bvex-link C++ system started successfully!"
    echo "   - Redis: running on port 6379"
    echo "   - C++ Onboard Server: listening on UDP ports 3000, 3001, 3002, 8080 (PID: $SERVER_PID)"
    echo "   - Target: localhost:9999"
    echo ""
    echo "To stop: ./stop-bvex-cpp.sh"
else
    echo "❌ Error: C++ onboard server failed to start"
    exit 1
fi 