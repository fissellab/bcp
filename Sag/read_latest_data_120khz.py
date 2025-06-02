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

# System Parameters for 120 kHz resolution using 16384-point FFT
SAMPLE_RATE = 3932.16  # Sample rate in MSPS
BANDWIDTH = SAMPLE_RATE / 2  # MHz for complex sampling (Nyquist frequency)
FFT_SIZE = 16384  # Number of FFT points for 120 kHz resolution
FREQUENCY_RESOLUTION = BANDWIDTH / FFT_SIZE  # ~0.120 MHz (120 kHz)

# Water maser specific settings
WATER_MASER_FREQ = 22.235  # Water maser frequency in GHz
ZOOM_WINDOW_WIDTH = 0.012  # Width of zoom window in GHz (±12 MHz on each side)

# Frequency range calculation
IF_LOWER = 20.96608  # Lower bound of frequency range in GHz
IF_UPPER = 22.93216  # Upper bound of frequency range in GHz
FREQ_RANGE = IF_UPPER - IF_LOWER  # Total frequency range in GHz

# Calculate bin indices for the water maser frequency window
BIN_WIDTH = FREQ_RANGE / FFT_SIZE  # GHz per bin
WATER_MASER_CENTER_BIN = int((WATER_MASER_FREQ - IF_LOWER) / BIN_WIDTH)
ZOOM_BINS = int(ZOOM_WINDOW_WIDTH / BIN_WIDTH)  # Number of bins in each direction
ZOOM_START_BIN = max(0, WATER_MASER_CENTER_BIN - ZOOM_BINS)
ZOOM_END_BIN = min(FFT_SIZE - 1, WATER_MASER_CENTER_BIN + ZOOM_BINS)
ZOOM_WIDTH = ZOOM_END_BIN - ZOOM_START_BIN + 1

# Data buffering
MAX_POWER_SAMPLES = 1000  # Maximum number of power samples to store
MAX_SPECTRUM_SAMPLES = 5  # Number of spectrum samples to read at once
DISPLAY_WINDOW = 60  # Display window in seconds
UPDATE_INTERVAL = 100  # Animation update interval in ms

# Thread-safe queues for data exchange
spectrum_queue = queue.Queue(maxsize=100)
power_queue = queue.Queue(maxsize=1000)
stop_event = threading.Event()

ssh_client = None
sftp_client = None
last_read_position = 0  # Track file position for efficient reading

def get_ssh_client():
    global ssh_client, sftp_client
    if ssh_client is None or not ssh_client.get_transport().is_active():
        print("Establishing SSH connection...")
        ssh_client = paramiko.SSHClient()
        ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh_client.connect(SAGGITARIUS_IP, username=SAGGITARIUS_USER, password=SAGGITARIUS_PASSWORD)
        sftp_client = ssh_client.open_sftp()
    return ssh_client, sftp_client

def get_data_folders():
    _, sftp = get_ssh_client()
    folders = sftp.listdir(REMOTE_DATA_PATH)
    return [f for f in folders if f.endswith('_BVEX_120KHz_data')]

def get_latest_files(sftp, folder):
    files = sftp.listdir(os.path.join(REMOTE_DATA_PATH, folder))
    spectrum_files = [f for f in files if f.endswith('_120KHz_spectrum_data.txt')]
    power_files = [f for f in files if f.endswith('_120KHz_integrated_power_data.txt')]
    
    if not spectrum_files or not power_files:
        return None, None
    
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
    """
    Parse spectrum data focusing only on the water maser frequency range.
    Extracts timestamp and spectrum values, then applies necessary processing.
    """
    results = []
    for line in lines:
        try:
            parts = line.strip().split()
            if len(parts) < FFT_SIZE + 1:  # Timestamp + 16384 spectrum values
                print(f"Invalid spectrum data: {line[:50]}... (expected {FFT_SIZE+1} values, got {len(parts)})")
                continue
                
            timestamp = float(parts[0])
            
            # Extract only the relevant frequency range
            # IMPORTANT: The data is stored as linear power values, we'll convert to dB later
            raw_spectrum = np.array([float(x) for x in parts[1:FFT_SIZE+1]])
            
            # Apply 10*log10() to convert to dB
            full_spectrum_db = 10 * np.log10(raw_spectrum + 1e-10)  # Add small value to avoid log(0)
            
            # Flip the spectrum to account for 2nd Nyquist zone sampling
            flipped_spectrum = np.flip(full_spectrum_db)
            
            # Apply fftshift to center DC component
            spectrum_db = np.fft.fftshift(flipped_spectrum)
            
            # Extract only the water maser window region
            zoomed_spectrum = spectrum_db[ZOOM_START_BIN:ZOOM_END_BIN+1]
            
            # Compute the baseband-subtracted spectrum for the zoomed region
            baseline = np.median(zoomed_spectrum)
            baseband_subtracted = zoomed_spectrum - baseline
            
            # Calculate integrated power for the maser region
            maser_power = np.sum(10**(baseband_subtracted/10))
            maser_power_db = 10 * np.log10(maser_power + 1e-10)
            
            # Store results: timestamp, regular spectrum, baseband-subtracted spectrum, and maser power
            results.append((timestamp, zoomed_spectrum, baseband_subtracted, maser_power_db, baseline))
        except Exception as e:
            print(f"Error parsing spectrum line: {e}")
    
    return results

def parse_power_data(lines):
    """Parse integrated power data."""
    data = []
    for line in lines:
        try:
            parts = line.strip().split()
            if len(parts) == 2:
                timestamp, power = float(parts[0]), float(parts[1])
                # Apply 10*log10() to the power data
                power = 10 * np.log10(power + 1e-10)  # Add small value to avoid log(0)
                data.append((timestamp, power))
        except ValueError as e:
            print(f"Invalid data in power file: {line} - {e}")
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
                        # If queue is full, remove oldest items
                        if spectrum_queue.full():
                            try:
                                spectrum_queue.get_nowait()
                            except queue.Empty:
                                pass
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
                        if power_queue.full():
                            try:
                                power_queue.get_nowait()
                            except queue.Empty:
                                pass
                        power_queue.put(data)
            
            # Sleep briefly to avoid hammering the server
            time.sleep(0.2)
            
        except Exception as e:
            print(f"Error in data reader thread: {e}")
            time.sleep(1)  # Sleep longer on error

def select_folder():
    """Let the user select a data folder."""
    folders = get_data_folders()
    if not folders:
        print("No 120kHz data folders found. Make sure the 120kHz spectrometer is running.")
        sys.exit(1)
        
    print("Available 120kHz data folders:")
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

def key_press_handler(event, fig):
    """Handle keyboard events."""
    if event.key == 'q':
        # Quit application
        print("Quitting...")
        plt.close(fig)
        stop_event.set()
    elif event.key == 's':
        # Save current plot
        timestamp = time.strftime("%Y%m%d-%H%M%S")
        plt.savefig(f"maser_capture_{timestamp}.png", dpi=300)
        print(f"Plot saved as maser_capture_{timestamp}.png")

def plot_data(selected_folder):
    """Create and update the visualization plot."""
    # Start the data reader thread
    reader_thread = threading.Thread(target=data_reader_thread, args=(selected_folder,))
    reader_thread.daemon = True
    reader_thread.start()
    
    # Create figure and axes with 3 subplots
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 12), 
                                      gridspec_kw={'height_ratios': [1, 1, 1]})
    
    # Create more space for titles and labels
    plt.subplots_adjust(left=0.1, right=0.95, top=0.92, bottom=0.08, hspace=0.4)
    
    # Set the main title with more top margin
    fig.suptitle(f'Water Maser ({WATER_MASER_FREQ} GHz) Observation - 120 kHz Resolution', fontsize=16, y=0.98)
    
    # Calculate frequencies for the zoomed region
    zoom_frequencies = np.linspace(
        WATER_MASER_FREQ - ZOOM_WINDOW_WIDTH, 
        WATER_MASER_FREQ + ZOOM_WINDOW_WIDTH, 
        ZOOM_WIDTH
    )
    
    # 1. Zoomed spectrum plot (±12 MHz around maser)
    spectrum_line, = ax1.plot(zoom_frequencies, np.zeros(ZOOM_WIDTH), '-', linewidth=1)
    ax1.set_xlabel('Frequency (GHz)')
    ax1.set_ylabel('Power (dB)')
    ax1.set_title(f'Spectrum (±{ZOOM_WINDOW_WIDTH*1000:.1f} MHz around {WATER_MASER_FREQ} GHz)', pad=10)
    ax1.grid(True, linestyle='--', alpha=0.7)
    
    # Add text for baseline level
    baseline_text = ax1.text(0.02, 0.95, 'Baseline: N/A', transform=ax1.transAxes,
                         fontsize=9, bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    
    # 2. Baseband subtracted spectrum
    bbsub_spectrum_line, = ax2.plot(zoom_frequencies, np.zeros(ZOOM_WIDTH), '-', color='green', linewidth=1)
    ax2.set_xlabel('Frequency (GHz)')
    ax2.set_ylabel('Power - Baseline (dB)')
    ax2.set_title(f'Baseband Subtracted Spectrum (±{ZOOM_WINDOW_WIDTH*1000:.1f} MHz)', pad=10)
    ax2.grid(True, linestyle='--', alpha=0.7)
    
    # 3. Maser power integration plot
    maser_power_line, = ax3.plot([], [], '-', color='blue', linewidth=1)
    ax3.set_xlabel('Time (s)')
    ax3.set_ylabel('Maser Integrated Power (dB)')
    ax3.set_title(f'H₂O Maser Integrated Power (±{ZOOM_WINDOW_WIDTH*1000:.1f} MHz)', pad=10)
    ax3.grid(True, linestyle='--', alpha=0.7)
    
    # Add text for displaying average maser power
    avg_power_text = ax3.text(0.02, 0.95, '', transform=ax3.transAxes,
                             fontsize=9, bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    
    # Buffers for maser power data visualization
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
                
            # Get all available maser power data
            new_power_data = []
            try:
                while not power_queue.empty():
                    new_power_data.append(power_queue.get_nowait())
            except queue.Empty:
                pass
                
            # Update spectrum plots if new data is available
            if latest_spectrum:
                timestamp, zoomed_spectrum, baseband_subtracted, maser_power, baseline = latest_spectrum
                
                # Update spectrum plots
                spectrum_line.set_ydata(zoomed_spectrum)
                bbsub_spectrum_line.set_ydata(baseband_subtracted)
                
                # Update baseline text
                baseline_text.set_text(f'Baseline: {baseline:.2f} dB')
                
                # Adjust y-axis limits for spectra with padding
                zoom_y_min, zoom_y_max = zoomed_spectrum.min(), zoomed_spectrum.max()
                zoom_y_range = zoom_y_max - zoom_y_min
                ax1.set_ylim([zoom_y_min - 0.05*zoom_y_range, zoom_y_max + 0.05*zoom_y_range])
                
                # Adjust y-axis limits for baseband subtracted plot
                bbsub_y_min, bbsub_y_max = baseband_subtracted.min(), baseband_subtracted.max()
                bbsub_y_range = max(bbsub_y_max - bbsub_y_min, 0.2)  # Ensure minimum range
                ax2.set_ylim([bbsub_y_min - 0.1*bbsub_y_range, bbsub_y_max + 0.1*bbsub_y_range])
                
                # Update main title with timestamp
                current_time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(timestamp))
                fig.suptitle(f'Water Maser ({WATER_MASER_FREQ} GHz) Observation - {current_time_str} - Resolution: 120 kHz', 
                           fontsize=16, y=0.98)
                
                # Add the maser power data for the power vs. time plot
                if start_time is None:
                    start_time = timestamp
                
                power_times.append(timestamp - start_time)
                power_values.append(maser_power)
                
            # Update maser power meter plot with additional data from power file
            if new_power_data:
                for t, p in new_power_data:
                    # Initialize start time if this is the first data point
                    if start_time is None:
                        start_time = t
                    
                    # Only add if newer than what we already have
                    if not power_times or t - start_time > power_times[-1]:
                        power_times.append(t - start_time)
                        power_values.append(p)
            
            # Update the power plot
            if power_times:
                # Clear old data outside the display window
                while power_times and power_times[-1] - power_times[0] > DISPLAY_WINDOW:
                    power_times.popleft()
                    power_values.popleft()
                
                # Update the plot lines
                maser_power_line.set_data(list(power_times), list(power_values))
                
                # Calculate and display average maser power
                if power_values:
                    avg_power = sum(power_values) / len(power_values)
                    avg_power_text.set_text(f'Average Maser Power: {avg_power:.2f} dB')
                
                # Dynamically adjust x-axis to keep the plot centered
                current_time = power_times[-1]
                ax3.set_xlim(max(0, current_time - DISPLAY_WINDOW), current_time + 1)
                
                # Dynamically adjust y-axis with some padding
                y_min = min(power_values)
                y_max = max(power_values)
                y_range = max(y_max - y_min, 0.2)  # Ensure minimum range
                ax3.set_ylim(y_min - 0.1*y_range, y_max + 0.1*y_range)
                    
        except Exception as e:
            print(f"Error updating plot: {e}")
            import traceback
            traceback.print_exc()
        
        return spectrum_line, bbsub_spectrum_line, maser_power_line, baseline_text, avg_power_text
    
    # Add instructions to the figure
    fig.text(0.5, 0.01, 
            "Press 's' to save plot | 'q' to quit", 
            ha='center', fontsize=10)
    
    # Add keyboard handler for save and quit functions
    fig.canvas.mpl_connect('key_press_event', lambda event: key_press_handler(event, fig))
    
    # Create the animation with faster update interval
    ani = anim.FuncAnimation(fig, update, interval=UPDATE_INTERVAL, blit=True, cache_frame_data=False)
    
    # Show the plot
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])  # Adjust layout to make room for text
    plt.show()
    
    # Cleanup
    stop_event.set()
    reader_thread.join(timeout=1.0)
    plt.close(fig)

if __name__ == "__main__":
    # Handle command line arguments
    if len(sys.argv) > 1 and sys.argv[1] == '--help':
        print("Usage: python read_latest_data_120khz.py [OPTIONS]")
        print("Options:")
        print("  --display-time SECONDS   Set the display window time (default: 60 seconds)")
        print("  --update-interval MS     Set the plot update interval (default: 100ms)")
        print("  --help                   Show this help message")
        sys.exit(0)
    
    # Parse any command line options
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == '--display-time' and i+1 < len(args):
            try:
                DISPLAY_WINDOW = int(args[i+1])
                print(f"Display window set to {DISPLAY_WINDOW} seconds")
                i += 2
            except ValueError:
                print(f"Invalid value for display time: {args[i+1]}")
                i += 2
        elif args[i] == '--update-interval' and i+1 < len(args):
            try:
                UPDATE_INTERVAL = int(args[i+1])
                print(f"Update interval set to {UPDATE_INTERVAL}ms")
                i += 2
            except ValueError:
                print(f"Invalid value for update interval: {args[i+1]}")
                i += 2
        else:
            i += 1
    
    # Print configuration information
    print(f"Water Maser monitoring at {WATER_MASER_FREQ} GHz")
    print(f"Zoom window: ±{ZOOM_WINDOW_WIDTH*1000:.1f} MHz")
    print(f"Frequency resolution: {FREQUENCY_RESOLUTION*1000:.1f} kHz")
    print(f"Monitoring bins {ZOOM_START_BIN}-{ZOOM_END_BIN} out of {FFT_SIZE} total")
    
    # Select folder and start visualization
    try:
        selected_folder = select_folder()
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