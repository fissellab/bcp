import serial
import time
import sys

def read_ticc(configure=False):
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
        
        # Create output file
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        output_file = f"ticc_data_{timestamp}.txt"
        
        with open(output_file, 'w') as f:
            f.write("# TICC Time Interval Measurements\n")
            f.write(f"# Started: {time.time()}\n")
            f.write("# Unix_Timestamp,Time_Interval\n")
        
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
