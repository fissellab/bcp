#!/bin/bash

# Function to get CPU temperature
get_cpu_temp() {
    if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
        temp=$(cat /sys/class/thermal/thermal_zone0/temp)
        temp_celsius=$((temp / 1000))
        echo "${temp_celsius}°C"
    elif command -v sensors >/dev/null 2>&1; then
        temp=$(sensors | grep -E "Core 0|Package id 0|Tctl" | head -n1 | grep -oE '[0-9]+\.[0-9]+°C' | head -n1)
        echo "${temp:-N/A}"
    else
        echo "N/A"
    fi
}

# Function to get CPU usage
get_cpu_usage() {
    cpu_usage=$(top -bn1 | grep "Cpu(s)" | sed "s/.*, *\([0-9.]*\)%* id.*/\1/" | awk '{print 100 - $1"%"}')
    echo "$cpu_usage"
}

# Function to get memory usage in Mi/Gi format
get_memory_usage() {
    memory_info=$(free -h | grep Mem)
    used=$(echo $memory_info | awk '{print $3}')
    total=$(echo $memory_info | awk '{print $2}')
    echo "${used}/${total}"
}

# Function to get external SSD info
get_external_ssd() {
    if [ -d "/media/saggitarius/T7" ]; then
        df -h /media/saggitarius/T7 | tail -n1
    elif [ -d "/media/ophiuchus/T7" ]; then
        df -h /media/ophiuchus/T7 | tail -n1
    else
        echo "T7 drive not mounted"
    fi
}

# Display the information
echo "CPU Temperature: $(get_cpu_temp)"
echo "CPU Usage: $(get_cpu_usage)"
echo "Memory Usage: $(get_memory_usage)"
echo "=== EXTERNAL SSD (T7) ==="
echo "  $(get_external_ssd)"
