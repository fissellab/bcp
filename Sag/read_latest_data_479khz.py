import paramiko
import os
import time
import numpy as np
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
import matplotlib.animation as anim
from collections import deque
import threading
import queue
import sys

# Configuration
SAGGITARIUS_IP = '100.70.234.8'
SAGGITARIUS_USER = 'saggitarius'
SAGGITARIUS_PASSWORD = 'three_little_pigs!'
REMOTE_DATA_PATH = '/media/saggitarius/T7'

# Data buffering
MAX_POWER_SAMPLES = 1000  # Maximum number of power samples to store
MAX_SPECTRUM_SAMPLES = 10  # Number of spectrum samples to read at once
DISPLAY_WINDOW = 20  # Display window in seconds
UPDATE_INTERVAL = 50  # Animation update interval in ms

# Thread-safe queues for data exchange
spectrum_queue = queue.Queue(maxsize=100)
power_queue = queue.Queue(maxsize=1000)
stop_event = threading.Event()

# SSH and SFTP clients
ssh_client = None
sftp_client = None
last_read_position = 0  # Track file position for efficient reading

# 479 kHz spectrometer configuration
FREQ_START = 20.96608  # GHz
FREQ_END = 22.93216  # GHz
FFT_SIZE = 4096  # Number of FFT points

def get_ssh_client():
    """Establish SSH connection to Saggitarius"""
    global ssh_client, sftp_client
    if ssh_client is None or not ssh_client.get_transport().is_active():
        print("Establishing SSH connection to Saggitarius...")
        ssh_client = paramiko.SSHClient()
        ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh_client.connect(SAGGITARIUS_IP, username=SAGGITARIUS_USER, password=SAGGITARIUS_PASSWORD)
        sftp_client = ssh_client.open_sftp()
    return ssh_client, sftp_client

def get_data_folders():
    """Get all 479KHz data folders from remote host"""
    _, sftp = get_ssh_client()
    folders = sftp.listdir(REMOTE_DATA_PATH)
    return sorted([f for f in folders if f.endswith('_BVEX_479KHz_data')])

def get_latest_files(sftp, folder):
    """Get the latest spectrum and power files from the specified folder"""
    files = sftp.listdir(os.path.join(REMOTE_DATA_PATH, folder))
    spectrum_files = [f for f in files if f.endswith('_479KHz_spectrum_data.txt')]
    power_files = [f for f in files if f.endswith('_479KHz_integrated_power_data.txt')]
    
    if not spectrum_files or not power_files:
        return None, None
    
    # Get most recent files by modification time
    spectrum_file = max(spectrum_files, key=lambda f: sftp.stat(os.path.join(REMOTE_DATA_PATH, folder, f)).st_mtime)
    power_file = max(power_files, key=lambda f: sftp.stat(os.path.join(REMOTE_DATA_PATH, folder, f)).st_mtime)
    
    return spectrum_file, power_file

def read_file_from_position(sftp, file_path, position, max_lines=None):
    """Read file from the given position and update the position."""
    global last_read_position
    
    try:
        with sftp.open(file_path, 'r') as f:
            f.seek(position)
            lines = []
            line_count = 0
            
            while True:
                line = f.readline()
                if not line:
                    break
                    
                lines.append(line)
                line_count += 1
                
                if max_lines and line_count >= max_lines:
                    break
            
            last_read_position = f.tell()
            return lines, last_read_position
    except IOError as e:
        print(f"Error reading file {file_path}: {e}")
        return [], position

def parse_spectrum_data(lines):
    """Parse spectrum data from file lines"""
    results = []
    for line in lines:
        try:
            parts = line.strip().split()
            if len(parts) < 4097:  # Timestamp + 4096 spectrum values
                print(f"Invalid spectrum data (expected 4097 values, got {len(parts)})")
                continue
                
            timestamp = float(parts[0])
            # Get the raw spectrum values
            raw_spectrum = np.array([float(x) for x in parts[1:4097]])
            
            # PROCESS ONCE: Apply transformations in a fixed sequence
            # 1. First flip the spectrum (accounts for 2nd Nyquist zone sampling)
            # 2. Then apply fftshift to center the DC component
            flipped_spectrum = np.flip(raw_spectrum)  # Step 1
            fftshifted_spectrum = np.fft.fftshift(flipped_spectrum)  # Step 2
            
            # Convert to dB scale for visualization
            spectrum_db = 10 * np.log10(fftshifted_spectrum + 1e-10)
            
            results.append((timestamp, spectrum_db))
        except Exception as e:
            print(f"Error parsing spectrum line: {e}")
    
    return results

def parse_power_data(lines):
    """Parse power data from file lines"""
    data = []
    for line in lines:
        try:
            parts = line.strip().split()
            if len(parts) == 2:
                timestamp, power = float(parts[0]), float(parts[1])
                # Apply 10*np.log10() to the power data
                power = 10 * np.log10(power + 1e-10)  # Add small value to avoid log(0)
                data.append((timestamp, power))
        except ValueError as e:
            print(f"Invalid data in power file: {e}")
    return data

def data_reader_thread(folder):
    """Thread to continuously read data from the remote files and put it in the queues."""
    global last_read_position
    
    spectrum_position = 0
    power_position = 0
    
    last_file_check = 0
    spectrum_path = None
    power_path = None
    
    while not stop_event.is_set():
        try:
            current_time = time.time()
            
            # Only check for new files every 5 seconds to reduce overhead
            if current_time - last_file_check > 5 or spectrum_path is None or power_path is None:
                ssh, sftp = get_ssh_client()
                files = get_latest_files(sftp, folder)
                
                if files[0] is None or files[1] is None:
                    time.sleep(1)
                    continue
                
                spectrum_file, power_file = files
                new_spectrum_path = os.path.join(REMOTE_DATA_PATH, folder, spectrum_file)
                new_power_path = os.path.join(REMOTE_DATA_PATH, folder, power_file)
                
                # If files have changed, reset positions
                if new_spectrum_path != spectrum_path:
                    spectrum_path = new_spectrum_path
                    spectrum_position = 0
                    print(f"New spectrum file detected: {spectrum_file}")
                
                if new_power_path != power_path:
                    power_path = new_power_path
                    power_position = 0
                    print(f"New power file detected: {power_file}")
                
                last_file_check = current_time
            
            # Read spectrum data
            if spectrum_path:
                ssh, sftp = get_ssh_client()
                spectrum_lines, spectrum_position = read_file_from_position(
                    sftp, spectrum_path, spectrum_position, MAX_SPECTRUM_SAMPLES
                )
                
                if spectrum_lines:
                    spectrum_data = parse_spectrum_data(spectrum_lines)
                    for data in spectrum_data:
                        spectrum_queue.put(data)
            
            # Read power data
            if power_path:
                ssh, sftp = get_ssh_client()
                power_lines, power_position = read_file_from_position(
                    sftp, power_path, power_position, MAX_SPECTRUM_SAMPLES
                )
                
                if power_lines:
                    power_data = parse_power_data(power_lines)
                    for data in power_data:
                        # If queue is full, remove oldest items
                        while power_queue.qsize() >= MAX_POWER_SAMPLES:
                            try:
                                power_queue.get_nowait()
                            except queue.Empty:
                                break
                        power_queue.put(data)
            
            # Sleep briefly to avoid hammering the server
            time.sleep(0.05)
            
        except Exception as e:
            print(f"Error in data reader thread: {e}")
            time.sleep(1)  # Sleep longer on error

def select_folder():
    """Interactive selection of data folder"""
    folders = get_data_folders()
    if not folders:
        print("No 479KHz data folders found.")
        return None
        
    print("Available data folders:")
    for i, folder in enumerate(folders, 1):
        print(f"{i}. {folder}")
    
    while True:
        try:
            choice = int(input("Select a folder number: ")) - 1
            if 0 <= choice < len(folders):
                return folders[choice]
            else:
                print("Invalid choice. Please try again.")
        except ValueError:
            print("Please enter a number.")

def key_press_handler(event):
    """Handle keyboard events"""
    if event.key == 'q':
        # Quit application
        print("Quitting...")
        plt.close('all')
        stop_event.set()
        sys.exit(0)
    elif event.key == 's':
        # Save current plot
        timestamp = time.strftime("%Y%m%d-%H%M%S")
        plt.savefig(f"spectrum_479khz_{timestamp}.png", dpi=300)
        print(f"Plot saved as spectrum_479khz_{timestamp}.png")

def plot_data(selected_folder):
    """Plot spectrum and power data with animation"""
    # Start the data reader thread
    reader_thread = threading.Thread(target=data_reader_thread, args=(selected_folder,))
    reader_thread.daemon = True
    reader_thread.start()
    
    # Create figure and axes
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
    fig.suptitle('479KHz Resolution Spectrometer Data', fontsize=16)
    plt.tight_layout(pad=3.0, rect=[0, 0.03, 1, 0.95])  # Add padding between subplots
    
    # Create frequency axis that properly maps the FFT bins after fftshift
    # Need to reverse the order of frequencies to match expected spectrum orientation
    faxis = np.linspace(FREQ_END, FREQ_START, FFT_SIZE)  # Reversed order
    
    # Initial empty spectrum plot - we're plotting all 4096 points
    spectrum_line, = ax1.plot(faxis, np.zeros(FFT_SIZE), '-', linewidth=1)
    ax1.set_xlabel('Frequency (GHz)')
    ax1.set_ylabel('Power (dB)')
    ax1.set_title('Spectrum (479KHz Resolution)')
    ax1.grid(True, linestyle='--', alpha=0.7)
    
    # Set x-axis limits to match our frequency range
    ax1.set_xlim(FREQ_END, FREQ_START)  # Reversed to match frequency axis
    
    # Add a vertical line at water maser rest frequency (22.23508 GHz)
    water_maser_freq = 22.23508  # GHz
    if FREQ_START <= water_maser_freq <= FREQ_END:
        ax1.axvline(water_maser_freq, color='r', linestyle='--', alpha=0.5)
        ax1.text(water_maser_freq, 0, ' H₂O', color='r', alpha=0.7)
    
    # Power plot
    power_line, = ax2.plot([], [], '-', color='blue', linewidth=1)
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Integral Power (dB)')
    ax2.set_title('Integral Power Plot')
    ax2.grid(True, linestyle='--', alpha=0.7)
    
    # Add text for displaying average power
    avg_power_text = ax2.text(0.02, 0.95, '', transform=ax2.transAxes,
                            bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    
    # Buffers for power data visualization
    power_times = deque(maxlen=MAX_POWER_SAMPLES)
    power_values = deque(maxlen=MAX_POWER_SAMPLES)
    
    # Reference start time for relative timestamps
    start_time = None
    
    def update(frame):
        nonlocal start_time
        
        try:
            # Get latest spectrum data
            latest_spectrum = None
            try:
                # Get the newest spectrum data, discarding older ones if multiple are available
                while not spectrum_queue.empty():
                    latest_spectrum = spectrum_queue.get_nowait()
            except queue.Empty:
                pass
                
            # Get all available power data
            new_power_data = []
            try:
                while not power_queue.empty():
                    new_power_data.append(power_queue.get_nowait())
            except queue.Empty:
                pass
                
            # Update spectrum plot if new data is available
            if latest_spectrum:
                spectrum_time, spectrum = latest_spectrum
                
                # Update current spectrum
                spectrum_line.set_ydata(spectrum)
                
                # Adjust y-axis limits with some padding
                y_min, y_max = spectrum.min(), spectrum.max()
                y_range = y_max - y_min
                ax1.set_ylim([y_min - 0.05*y_range, y_max + 0.05*y_range])
                
                # Update main title with timestamp
                current_time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(spectrum_time))
                fig.suptitle(f'479KHz Resolution Spectrometer Data - {current_time_str}', fontsize=16)
                
            # Update power meter plot with new data
            if new_power_data:
                for t, p in new_power_data:
                    # Initialize start time if this is the first data point
                    if start_time is None:
                        start_time = t
                    
                    # Store relative time and power value
                    power_times.append(t - start_time)
                    power_values.append(p)
                
                # Clear old data outside the display window
                while power_times and power_times[-1] - power_times[0] > DISPLAY_WINDOW:
                    power_times.popleft()
                    power_values.popleft()
                
                # Update the plot lines
                power_line.set_data(list(power_times), list(power_values))
                
                # Calculate and display average power
                if power_values:
                    avg_power = sum(power_values) / len(power_values)
                    avg_power_text.set_text(f'Average Power: {avg_power:.2f} dB')
                
                # Dynamically adjust x-axis to keep the plot centered
                if power_times:
                    current_time = power_times[-1]
                    ax2.set_xlim(max(0, current_time - DISPLAY_WINDOW), current_time + 1)
                
                # Dynamically adjust y-axis with some padding
                if power_values:
                    y_min = min(power_values)
                    y_max = max(power_values)
                    y_range = max(y_max - y_min, 1)  # Ensure minimum range
                    ax2.set_ylim(y_min - 0.1*y_range, y_max + 0.1*y_range)
                    
        except Exception as e:
            print(f"Error updating plot: {e}")
            import traceback
            traceback.print_exc()
        
        return spectrum_line, power_line, avg_power_text

    # Add instructions to the figure
    resolution_text = f"Frequency Resolution: 479 kHz ({(FREQ_END-FREQ_START)*1000/FFT_SIZE:.1f} kHz/bin)"
    fig.text(0.5, 0.01, 
             f"{resolution_text} | 's' to save plot | 'q' to quit", 
             ha='center', fontsize=10)
    
    # Add keyboard handler for save and quit functions
    fig.canvas.mpl_connect('key_press_event', key_press_handler)
    
    # Set window title for easier identification
    if hasattr(fig.canvas, 'manager') and fig.canvas.manager:
        fig.canvas.manager.set_window_title('Water Maser Viewer - 479KHz Resolution')
    
    # Create the animation with faster update interval
    ani = anim.FuncAnimation(fig, update, interval=UPDATE_INTERVAL, blit=True)
    
    # Show the plot
    plt.show()

if __name__ == "__main__":
    # Handle command line arguments
    import argparse
    parser = argparse.ArgumentParser(description="479KHz Spectrometer Data Viewer")
    parser.add_argument("--display-time", type=int, default=20,
                      help="Display window time in seconds (default: 20)")
    parser.add_argument("--update-interval", type=int, default=50,
                      help="Plot update interval in ms (default: 50)")
    parser.add_argument("--auto", action="store_true",
                      help="Automatically use the latest data folder")
    parser.add_argument("--save-plot", action="store_true",
                      help="Save the plot to a file")
    parser.add_argument("--host", type=str, default=SAGGITARIUS_IP,
                      help=f"Hostname or IP of Saggitarius (default: {SAGGITARIUS_IP})")
    
    args = parser.parse_args()
    
    # Update settings from arguments
    if args.display_time:
        DISPLAY_WINDOW = args.display_time
        print(f"Display window set to {DISPLAY_WINDOW} seconds")
    
    if args.update_interval:
        UPDATE_INTERVAL = args.update_interval
        print(f"Update interval set to {UPDATE_INTERVAL}ms")
        
    if args.host:
        SAGGITARIUS_IP = args.host
        print(f"Using host: {SAGGITARIUS_IP}")
    
    # Select folder and start visualization
    try:
        if args.auto:
            folders = get_data_folders()
            if folders:
                selected_folder = folders[-1]
                print(f"Automatically selected folder: {selected_folder}")
            else:
                print("No data folders found. Exiting.")
                sys.exit(1)
        else:
            selected_folder = select_folder()
            if not selected_folder:
                sys.exit(1)
                
        plot_data(selected_folder)
    except KeyboardInterrupt:
        print("Plot closed by user.")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        # Clean up resources
        stop_event.set()
        plt.close('all')  # Ensure all plots are closed
        if ssh_client:
            ssh_client.close()
        if sftp_client:
            sftp_client.close()
        print("Resources cleaned up.")

    # Save plot if requested only when explicitly called with --save-plot
    if args.save_plot:
        try:
            timestamp = time.strftime("%Y%m%d_%H%M%S")
            saved_filename = f"maser_479KHz_{timestamp}.png"
            plt.savefig(saved_filename, dpi=300)
            print(f"Plot saved as {saved_filename}")
        except Exception as e:
            print(f"Error saving plot: {e}")

    # Set window title for easier identification
    if hasattr(fig.canvas, 'manager') and fig.canvas.manager:
        fig.canvas.manager.set_window_title('Water Maser Viewer - 479KHz Resolution')

    # Add title with timestamp
    time_str = time.strftime("%Y-%m-%d %H:%M:%S UTC")
    fig.suptitle(f'Water Maser (22.235 GHz) - 479KHz Resolution - {time_str}', fontsize=12)