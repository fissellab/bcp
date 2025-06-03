#!/usr/bin/env python3
"""
Streamlined Water Maser Monitor using SSH streaming
"""

import subprocess
import threading
import queue
import time
import numpy as np
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
import matplotlib.animation as anim
from collections import deque
import sys
import signal

# Configuration
SAGGITARIUS_IP = '100.70.234.8'
SAGGITARIUS_USER = 'saggitarius'
REMOTE_DATA_PATH = '/media/saggitarius/T7'

# Spectrum processing parameters
SAMPLE_RATE = 3932.16
BANDWIDTH = SAMPLE_RATE / 2
FFT_SIZE = 16384

WATER_MASER_FREQ = 22.235
ZOOM_WINDOW_WIDTH = 0.010

IF_LOWER = 20.96608
IF_UPPER = 22.93216
FREQ_RANGE = IF_UPPER - IF_LOWER

BIN_WIDTH = FREQ_RANGE / FFT_SIZE
WATER_MASER_CENTER_BIN = int((WATER_MASER_FREQ - IF_LOWER) / BIN_WIDTH)
ZOOM_BINS = int(ZOOM_WINDOW_WIDTH / BIN_WIDTH)
ZOOM_START_BIN = max(0, WATER_MASER_CENTER_BIN - ZOOM_BINS)
ZOOM_END_BIN = min(FFT_SIZE - 1, WATER_MASER_CENTER_BIN + ZOOM_BINS)
ZOOM_WIDTH = ZOOM_END_BIN - ZOOM_START_BIN + 1

# Display parameters
MAX_POWER_SAMPLES = 1000
DISPLAY_WINDOW = 60
UPDATE_INTERVAL = 25

# Thread-safe queues
spectrum_queue = queue.Queue(maxsize=20)
stop_event = threading.Event()

class SSHStreamer:
    def __init__(self, folder):
        self.folder = folder
        self.spectrum_process = None
        
    def get_latest_spectrum_file(self):
        """Get latest spectrum file using SSH"""
        cmd = f"ssh {SAGGITARIUS_USER}@{SAGGITARIUS_IP} 'ls -t {REMOTE_DATA_PATH}/{self.folder}/*spectrum_data.txt | head -1'"
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        return result.stdout.strip()
    
    def start_spectrum_stream(self, spectrum_file):
        """Start streaming spectrum data using tail -f"""
        cmd = f"ssh {SAGGITARIUS_USER}@{SAGGITARIUS_IP} 'tail -f {spectrum_file}'"
        print(f"Starting spectrum stream: {cmd}")
        
        self.spectrum_process = subprocess.Popen(
            cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
            text=True, bufsize=1, universal_newlines=True
        )
        
        return self.spectrum_process
    
    def stop_streams(self):
        """Stop streaming process"""
        if self.spectrum_process:
            self.spectrum_process.terminate()
            self.spectrum_process.wait()

def parse_spectrum_line(line):
    """Parse a single spectrum line"""
    try:
        parts = line.strip().split()
        if len(parts) < FFT_SIZE + 1:
            return None
            
        timestamp = float(parts[0])
        raw_spectrum = np.array([float(x) for x in parts[1:FFT_SIZE+1]])
        
        # Process spectrum
        full_spectrum_db = 10 * np.log10(raw_spectrum + 1e-10)
        flipped_spectrum = np.flip(full_spectrum_db)
        spectrum_db = np.fft.fftshift(flipped_spectrum)
        zoomed_spectrum = spectrum_db[ZOOM_START_BIN:ZOOM_END_BIN+1]
        
        baseline = np.median(zoomed_spectrum)
        baseband_subtracted = zoomed_spectrum - baseline
        
        maser_power = np.sum(10**(baseband_subtracted/10))
        maser_power_db = 10 * np.log10(maser_power + 1e-10)
        
        return (timestamp, baseband_subtracted, maser_power_db, baseline)
    except Exception:
        return None

def spectrum_streaming_thread(streamer, spectrum_file):
    """Thread to stream spectrum data"""
    print("Starting spectrum streaming thread...")
    
    process = streamer.start_spectrum_stream(spectrum_file)
    
    while not stop_event.is_set() and process.poll() is None:
        try:
            line = process.stdout.readline()
            if line:
                result = parse_spectrum_line(line)
                if result:
                    # Clear queue if getting full
                    while spectrum_queue.qsize() >= spectrum_queue.maxsize - 1:
                        try:
                            spectrum_queue.get_nowait()
                        except queue.Empty:
                            break
                    spectrum_queue.put(result)
        except Exception as e:
            print(f"Error in spectrum streaming: {e}")
            break
    
    print("Spectrum streaming thread ended")

def get_available_folders():
    """Get available data folders via SSH"""
    cmd = f"ssh {SAGGITARIUS_USER}@{SAGGITARIUS_IP} 'ls {REMOTE_DATA_PATH} | grep _BVEX_120KHz_data'"
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    
    if result.returncode == 0:
        return result.stdout.strip().split('\n')
    else:
        print(f"Error getting folders: {result.stderr}")
        return []

def select_folder():
    """Let user select a data folder"""
    folders = get_available_folders()
    if not folders:
        print("No data folders found!")
        sys.exit(1)
    
    print("Available folders:")
    for i, folder in enumerate(folders, 1):
        print(f"{i}. {folder}")
    
    while True:
        try:
            choice = int(input("Select folder: ")) - 1
            if 0 <= choice < len(folders):
                return folders[choice]
            else:
                print("Invalid choice")
        except ValueError:
            print("Please enter a number")

def plot_data(folder):
    """Main plotting function with SSH streaming"""
    print(f"Starting SSH streaming monitor for folder: {folder}")
    
    streamer = SSHStreamer(folder)
    
    # Get latest spectrum file
    spectrum_file = streamer.get_latest_spectrum_file()
    print(f"Spectrum file: {spectrum_file}")
    
    # Start streaming thread
    spectrum_thread = threading.Thread(target=spectrum_streaming_thread, args=(streamer, spectrum_file))
    spectrum_thread.daemon = True
    spectrum_thread.start()
    
    # Create the plot
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    plt.subplots_adjust(left=0.1, right=0.95, top=0.92, bottom=0.08, hspace=0.4)
    
    fig.suptitle(f'Water Maser ({WATER_MASER_FREQ} GHz) - Real-Time SSH Stream', fontsize=16, y=0.98)
    
    zoom_frequencies = np.linspace(
        WATER_MASER_FREQ - ZOOM_WINDOW_WIDTH, 
        WATER_MASER_FREQ + ZOOM_WINDOW_WIDTH, 
        ZOOM_WIDTH
    )
    
    # Setup spectrum plot
    bbsub_spectrum_line, = ax1.plot(zoom_frequencies, np.zeros(ZOOM_WIDTH), '-', color='green', linewidth=1)
    ax1.set_xlabel('Frequency (GHz)')
    ax1.set_ylabel('Power - Baseline (dB)')
    ax1.set_title(f'Baseband Subtracted Spectrum (±{ZOOM_WINDOW_WIDTH*1000:.1f} MHz)', pad=10)
    ax1.grid(True, linestyle='--', alpha=0.7)
    ax1.axvline(x=WATER_MASER_FREQ, color='red', linestyle='--', alpha=0.7, label=f'H₂O Maser Line ({WATER_MASER_FREQ} GHz)')
    ax1.legend(loc='upper right')
    
    baseline_text = ax1.text(0.02, 0.95, 'Baseline: N/A', transform=ax1.transAxes,
                         fontsize=9, bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    
    # Setup power plot
    maser_power_line, = ax2.plot([], [], '-', color='blue', linewidth=1)
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Maser Integrated Power (dB)')
    ax2.set_title(f'H₂O Maser Integrated Power (±{ZOOM_WINDOW_WIDTH*1000:.1f} MHz)', pad=10)
    ax2.grid(True, linestyle='--', alpha=0.7)
    
    avg_power_text = ax2.text(0.02, 0.95, '', transform=ax2.transAxes,
                             fontsize=9, bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    
    # Data buffers
    power_times = deque(maxlen=MAX_POWER_SAMPLES)
    power_values = deque(maxlen=MAX_POWER_SAMPLES)
    start_time = None
    
    def update(frame):
        nonlocal start_time
        
        try:
            # Get latest spectrum data
            latest_spectrum = None
            
            while not spectrum_queue.empty():
                latest_spectrum = spectrum_queue.get_nowait()
            
            if latest_spectrum:
                timestamp, baseband_subtracted, maser_power_db, baseline = latest_spectrum
                
                # Update spectrum plot
                bbsub_spectrum_line.set_ydata(baseband_subtracted)
                baseline_text.set_text(f'Baseline: {baseline:.2f} dB')
                
                # Update y-axis
                bbsub_y_min, bbsub_y_max = baseband_subtracted.min(), baseband_subtracted.max()
                bbsub_y_range = max(bbsub_y_max - bbsub_y_min, 0.2)
                ax1.set_ylim([bbsub_y_min - 0.1*bbsub_y_range, bbsub_y_max + 0.1*bbsub_y_range])
                
                # Update title with timestamp
                time_str = time.strftime("%H:%M:%S", time.localtime(timestamp))
                fig.suptitle(f'Water Maser ({WATER_MASER_FREQ} GHz) - {time_str}', fontsize=16, y=0.98)
                
                # Add to power data
                if start_time is None:
                    start_time = timestamp
                power_times.append(timestamp - start_time)
                power_values.append(maser_power_db)
            
            # Update power plot
            if power_times:
                while power_times and power_times[-1] - power_times[0] > DISPLAY_WINDOW:
                    power_times.popleft()
                    power_values.popleft()
                
                maser_power_line.set_data(list(power_times), list(power_values))
                
                if power_values:
                    avg_power = sum(power_values) / len(power_values)
                    avg_power_text.set_text(f'Average Power: {avg_power:.2f} dB')
                
                current_time_rel = power_times[-1]
                ax2.set_xlim(max(0, current_time_rel - DISPLAY_WINDOW), current_time_rel + 1)
                
                y_min = min(power_values)
                y_max = max(power_values)
                y_range = max(y_max - y_min, 0.2)
                ax2.set_ylim(y_min - 0.1*y_range, y_max + 0.1*y_range)
        
        except Exception as e:
            print(f"Error in update: {e}")
        
        return bbsub_spectrum_line, maser_power_line, baseline_text, avg_power_text
    
    # Cleanup function
    def cleanup():
        print("Cleaning up SSH streams...")
        stop_event.set()
        streamer.stop_streams()
        plt.close(fig)
    
    # Setup signal handlers
    def signal_handler(sig, frame):
        cleanup()
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    fig.canvas.mpl_connect('close_event', lambda evt: cleanup())
    
    # Create animation
    ani = anim.FuncAnimation(fig, update, interval=UPDATE_INTERVAL, blit=True, cache_frame_data=False)
    
    fig.text(0.5, 0.01, "Real-Time SSH Streaming - Press Ctrl+C to quit", ha='center', fontsize=10)
    
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.show()
    
    cleanup()

if __name__ == "__main__":
    print("Water Maser Real-Time Monitor")
    print("=" * 40)
    print("SSH streaming for real-time data")
    print("=" * 40)
    
    try:
        folder = select_folder()
        plot_data(folder)
    except KeyboardInterrupt:
        print("\nShutting down...")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        stop_event.set()
