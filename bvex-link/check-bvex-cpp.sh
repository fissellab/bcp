#!/bin/bash
echo "Checking bvex-link C++ system status..."
echo

# Check Redis
if sudo docker ps | grep -q redis; then
    echo " Redis: Running (port 6379)"
else
    echo " Redis: Not running"
fi

# Check C++ Onboard Server
if netstat -ulnp | grep -E "(3000|3001|3002|8080)" > /dev/null; then
    echo " C++ Onboard Server: Running (UDP ports 3000, 3001, 3002, 8080)"
    PID=$(netstat -ulnp | grep 3000 | awk '{print $6}' | cut -d'/' -f1)
    echo "   Process ID: $PID"
else
    echo " C++ Onboard Server: Not running"
fi

echo
echo "Port status:"
echo "UDP ports:"
netstat -ulnp | grep -E "(3000|3001|3002|8080)" 2>/dev/null || echo "No C++ UDP services found"
echo "TCP ports:"
netstat -tlnp | grep 6379 2>/dev/null || echo "No Redis TCP service found"

echo
echo "C++ server processes:"
ps aux | grep "build/main" | grep -v grep || echo "No C++ server processes found" 