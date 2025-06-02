#!/usr/bin/env python3
"""
Simple script to read HEHDT sentences from Vega 40 GNSS and extract heading
"""

import serial
import time
import sys
import re

def get_current_heading():
    """Read current heading from GPS device"""
    try:
        # Open serial connection to GPS device
        ser = serial.Serial('/dev/ttyGPS', 19200, timeout=1)
        
        # Read lines for up to 10 seconds to find HEHDT sentence
        start_time = time.time()
        while time.time() - start_time < 10:
            line = ser.readline().decode('ascii', errors='ignore').strip()
            
            # Look for HEHDT sentence
            if line.startswith('$HEHDT'):
                # Parse HEHDT sentence: $HEHDT,heading,T*checksum
                parts = line.split(',')
                if len(parts) >= 2:
                    try:
                        heading = float(parts[1])
                        ser.close()
                        return heading
                    except ValueError:
                        continue
        
        ser.close()
        return None
        
    except Exception as e:
        print(f"Error reading GPS: {e}", file=sys.stderr)
        return None

if __name__ == "__main__":
    heading = get_current_heading()
    if heading is not None:
        print(f"{heading:.1f}")
    else:
        print("N/A") 