#!/bin/bash

# Status Check Script for BCP + bvex-link Systems
echo "=== BCP + bvex-link System Status ==="
echo ""

# Check BCP Status
echo "🛰️  BCP Status:"
BCP_PIDS=$(pgrep -f "build/main.*bcp_Sag.config")
if [ ! -z "$BCP_PIDS" ]; then
    echo "   ✅ BCP: Running (PIDs: $BCP_PIDS)"
    for pid in $BCP_PIDS; do
        cpu_usage=$(ps -p $pid -o %cpu= 2>/dev/null | xargs)
        mem_usage=$(ps -p $pid -o %mem= 2>/dev/null | xargs)
        if [ ! -z "$cpu_usage" ]; then
            echo "      → PID $pid: CPU ${cpu_usage}%, Memory ${mem_usage}%"
        fi
    done
else
    echo "   ❌ BCP: Not running"
fi

# Check for spec scripts
SPEC_PIDS=$(pgrep -f "rfsoc_spec.*\.py")
if [ ! -z "$SPEC_PIDS" ]; then
    echo "   📊 Spec scripts: Running (PIDs: $SPEC_PIDS)"
else
    echo "   📊 Spec scripts: Not running"
fi

echo ""

# Check bvex-link Status using its own check script
echo "📡 bvex-link Status:"
if [ -f "/home/mayukh/bvex-link/check-bvex-cpp.sh" ]; then
    # Run the bvex-link status check (but suppress some output)
    /home/mayukh/bvex-link/check-bvex-cpp.sh 2>/dev/null | grep -E "(✅|❌|Running|Not running|Process ID)"
else
    echo "   ❌ bvex-link check script not found"
    
    # Manual check
    if sudo docker ps | grep -q redis; then
        echo "   ✅ Redis: Running (port 6379)"
    else
        echo "   ❌ Redis: Not running"
    fi
    
    if netstat -ulnp 2>/dev/null | grep -E "(3000|3001|3002|8080)" > /dev/null; then
        echo "   ✅ C++ Onboard Server: Running (UDP ports 3000, 3001, 3002, 8080)"
    else
        echo "   ❌ C++ Onboard Server: Not running"
    fi
fi

echo ""

# Network Status
echo "🌐 Network Status:"
echo "   BCP-related ports:"
netstat -tlnp 2>/dev/null | grep -E ":22|:80|:443" | head -3 | while read line; do
    echo "      $line"
done

echo "   bvex-link ports:"
netstat -ulnp 2>/dev/null | grep -E "(3000|3001|3002|8080)" | while read line; do
    echo "      $line"
done
netstat -tlnp 2>/dev/null | grep 6379 | while read line; do
    echo "      $line"
done

echo ""

# System Resources
echo "💻 System Resources:"
echo "   CPU Usage: $(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | sed 's/%us,//')"
echo "   Memory Usage: $(free | grep Mem | awk '{printf "%.1f%%", $3/$2 * 100.0}')"
echo "   Disk Usage: $(df -h . | tail -1 | awk '{print $5}')"

echo ""

# Log file status
echo "📝 Recent Log Activity:"
if [ -f "main_sag.log" ]; then
    log_size=$(du -sh main_sag.log | cut -f1)
    log_modified=$(stat -c %y main_sag.log | cut -d'.' -f1)
    echo "   BCP Log: $log_size (last modified: $log_modified)"
    
    # Show last few lines if the log is actively being written to
    if [ $(find main_sag.log -mmin -1 2>/dev/null | wc -l) -gt 0 ]; then
        echo "   📋 Recent BCP log entries:"
        tail -3 main_sag.log | sed 's/^/      /'
    fi
else
    echo "   BCP Log: Not found"
fi

echo ""
echo "🏁 Status check complete"
echo ""
echo "💡 Quick commands:"
echo "   Start systems: sudo ./start.sh"
echo "   Stop systems:  sudo ./stop.sh"
echo "   Check status:  ./status.sh" 