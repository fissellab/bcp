#!/bin/bash

# Manual Stop Script for BCP + bvex-link Systems
echo "=== Manual Stop Script for BCP + bvex-link ==="

# Check if running with sudo (required for Docker stop)
if [ "$EUID" -ne 0 ]; then
    echo "⚠️  This script requires sudo privileges for Docker commands."
    echo "   Please run with: sudo ./stop.sh"
    exit 1
fi

echo ""
echo "🛑 Stopping BCP + bvex-link systems..."
echo ""

# Step 1: Stop BCP first (gracefully stop data production)
echo "🛰️  Stopping BCP processes..."

# Find BCP main processes
BCP_PIDS=$(pgrep -f "build/main.*bcp_Sag.config")

if [ ! -z "$BCP_PIDS" ]; then
    echo "   → Found BCP processes: $BCP_PIDS"
    
    # Send SIGTERM first for graceful shutdown
    for pid in $BCP_PIDS; do
        echo "   → Sending SIGTERM to BCP process $pid..."
        kill -TERM "$pid" 2>/dev/null
    done
    
    # Wait up to 10 seconds for graceful shutdown
    echo "   → Waiting for graceful shutdown..."
    timeout=10
    while [ $timeout -gt 0 ]; do
        remaining_pids=$(pgrep -f "build/main.*bcp_Sag.config")
        if [ -z "$remaining_pids" ]; then
            break
        fi
        sleep 1
        timeout=$((timeout - 1))
    done
    
    # Force kill any remaining processes
    remaining_pids=$(pgrep -f "build/main.*bcp_Sag.config")
    if [ ! -z "$remaining_pids" ]; then
        echo "   → Force stopping remaining BCP processes..."
        for pid in $remaining_pids; do
            kill -KILL "$pid" 2>/dev/null
        done
    fi
    
    echo "   ✅ BCP processes stopped"
else
    echo "   → No BCP processes found"
fi

echo ""

# Step 2: Stop bvex-link server (stop data transmission/storage)
echo "📡 Stopping bvex-link server..."

if [ -f "/home/mayukh/bvex-link/stop-bvex-cpp.sh" ]; then
    # Preserve original user's HOME for bvex-link scripts
    ORIGINAL_HOME="/home/$SUDO_USER"
    if sudo -u "$SUDO_USER" HOME="$ORIGINAL_HOME" /home/mayukh/bvex-link/stop-bvex-cpp.sh; then
        echo "   ✅ bvex-link server stopped successfully"
    else
        echo "   ⚠️  Warning: bvex-link stop script had issues"
    fi
else
    echo "   ❌ bvex-link stop script not found at /home/mayukh/bvex-link/stop-bvex-cpp.sh"
fi

echo ""

# Step 3: Cleanup any orphaned processes
echo "🧹 Cleaning up any orphaned processes..."

# Check for any remaining spec scripts
SPEC_PIDS=$(pgrep -f "rfsoc_spec.*\.py")
if [ ! -z "$SPEC_PIDS" ]; then
    echo "   → Stopping orphaned spec scripts: $SPEC_PIDS"
    for pid in $SPEC_PIDS; do
        kill -TERM "$pid" 2>/dev/null
    done
    sleep 2
    # Force kill if still running
    for pid in $SPEC_PIDS; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -KILL "$pid" 2>/dev/null
        fi
    done
fi

echo ""
echo "🏁 Manual stop complete"
echo ""

# Show final status
echo "📊 Final system status:"
if pgrep -f "build/main.*bcp_Sag.config" > /dev/null; then
    echo "   ❌ BCP: Still running"
else
    echo "   ✅ BCP: Stopped"
fi

if netstat -ulnp 2>/dev/null | grep -E "(3000|3001|3002|8080)" > /dev/null; then
    echo "   ❌ bvex-link C++ server: Still running"
else
    echo "   ✅ bvex-link C++ server: Stopped"
fi

if sudo docker ps | grep -q redis; then
    echo "   ❌ Redis: Still running"
else
    echo "   ✅ Redis: Stopped"
fi 