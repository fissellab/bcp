import serial
import time

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
    send_command(ser, 'M', 1)  # Enter measurement mode menu
    read_response(ser)
    
    send_command(ser, 'I\r', 1)  # Select Time Interval mode
    read_response(ser)
    
    # Write changes and exit
    print("Saving configuration...")
    send_command(ser, 'W', 2)
    read_response(ser)
    
    print("Setup complete")

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
        
        # Setup TICC for Time Interval mode
        setup_ticc(ser)
        
        print("\nReading measurements (Ctrl+C to stop)...")
        print("Waiting for data...\n")
        
        # Skip initial comments/setup text
        time.sleep(2)
        ser.reset_input_buffer()
        
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('ascii', errors='ignore').strip()
                
                # Skip comment lines and empty lines
                if not line or line.startswith('#'):
                    continue
                    
                # Parse and print the measurement
                try:
                    parts = line.split()
                    if len(parts) >= 2 and "TI(A->B)" in line:
                        value = float(parts[0])
                        print(f"Time Interval: {value:+.11f} seconds")
                except (ValueError, IndexError):
                    continue
            else:
                time.sleep(0.1)
                
    except KeyboardInterrupt:
        print("\nStopping...")
    except serial.SerialException as e:
        print(f"\nSerial port error: {e}")
    finally:
        if 'ser' in locals():
            ser.close()
            print("Serial port closed")

if __name__ == "__main__":
    read_ticc()
