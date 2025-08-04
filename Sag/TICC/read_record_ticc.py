import serial
import time
import sys

def send_command(ser, cmd, delay=0.5):
    """Send a command and wait for specified delay"""
    ser.write(cmd.encode('ascii'))
    time.sleep(delay)
    
def read_response(ser):
    """Read and print response until timeout"""
    while ser.in_waiting:
        line = ser.readline().decode('ascii', errors='ignore').strip()
        if line:
            print(f"< {line}")
        time.sleep(0.1)

def setup_ticc(ser):
    """Configure TICC for Time Interval mode"""
    print("Starting TICC configuration...")
    
    # Clear any pending data
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    # Wait for initial bootup
    time.sleep(2)
    
    # Send break character
    print("Entering config mode...")
    send_command(ser, '#', 1)
    read_response(ser)
    
    # Select measurement mode
    print("Setting Time Interval mode...")
    send_command(ser, 'M', 1)
    read_response(ser)
    
    send_command(ser, 'I\r', 1)
    read_response(ser)
    
    # Write changes and exit
    print("Saving configuration...")
    send_command(ser, 'W', 2)
    read_response(ser)
    
    print("Setup complete")

def create_output_file():
    """Create a new output file with timestamp in name"""
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    filename = f"ticc_data_{timestamp}.txt"
    
    # Write header to file
    with open(filename, 'w') as f:
        f.write("# TICC Time Interval Measurements\n")
        f.write("# Started: " + str(time.time()) + "\n")
        f.write("# Unix_Timestamp,Time_Interval\n")
    
    return filename

def read_ticc():
    try:
        print("Opening serial port...")
        ser = serial.Serial(
            port='/dev/ttyACM0',
            baudrate=115200,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1
        )
        
        # Check if already in Time Interval mode
        ser.reset_input_buffer()
        time.sleep(0.5)
        start_data = ser.readline().decode('ascii', errors='ignore').strip()
        
        # If we don't see measurements, configure the device
        if not ("TI(A->B)" in start_data):
            setup_ticc(ser)
        
        # Create output file
        output_file = create_output_file()
        print(f"\nLogging data to: {output_file}")
        print("\nReading measurements (Ctrl+C to stop)...")
        
        start_time = time.time()
        
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('ascii', errors='ignore').strip()
                
                # Skip comment lines and empty lines
                if not line or line.startswith('#'):
                    continue
                    
                # Parse and record the measurement
                try:
                    parts = line.split()
                    if len(parts) >= 2 and "TI(A->B)" in line:
                        value = float(parts[0])
                        unix_timestamp = time.time()
                        elapsed_time = unix_timestamp - start_time
                        
                        # Write to file
                        with open(output_file, 'a') as f:
                            f.write(f"{unix_timestamp:.3f},{value:+.11f}\n")
                        
                        # Update display - overwrite current line
                        status_line = f"\rRunning: {elapsed_time:.1f}s | Latest: {value:+.11f}s | File: {output_file}"
                        sys.stdout.write(status_line)
                        sys.stdout.flush()
                        
                except (ValueError, IndexError):
                    continue
            else:
                time.sleep(0.1)
                
    except KeyboardInterrupt:
        print("\n\nStopping...")
    except serial.SerialException as e:
        print(f"\nSerial port error: {e}")
    finally:
        if 'ser' in locals():
            ser.close()
            print("\nSerial port closed")
            print(f"Data saved to: {output_file}")

if __name__ == "__main__":
    read_ticc()
