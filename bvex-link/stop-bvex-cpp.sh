#!/bin/bash
echo "Stopping bvex-link C++ system..."

# Stop C++ onboard server
echo "Stopping C++ onboard server..."
pkill -f "build/main"

# Stop Redis
echo "Stopping Redis..."
sudo docker stop redis

# Verify everything is stopped
sleep 2
if ! netstat -ulnp | grep -E "(3000|3001|3002|8080)" > /dev/null && ! netstat -tlnp | grep 6379 > /dev/null; then
    echo " bvex-link C++ system stopped successfully!"
else
    echo "  Some services may still be running:"
    netstat -ulnp | grep -E "(3000|3001|3002|8080)" 2>/dev/null
    netstat -tlnp | grep 6379 2>/dev/null
fi 