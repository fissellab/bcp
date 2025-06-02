import os
import sys
import time
import struct
import numpy as np
from numpy import fft
import matplotlib.pyplot as plt
import matplotlib.animation as anim
import casperfpga
import atexit
from optparse import OptionParser
from collections import deque
import threading
import queue

"""
High Resolution RFSoC Spectrometer for 120 kHz Resolution with 16384-point FFT
------------------------------------------------------------------
IMPORTANT STABILITY FIXES:
1. Added wait_for_dump to ensure reading only happens after FPGA completes a full dump
2. Added consistent read_spectrum function to properly interleave data from BRAMs
3. Ensured transformations (flip + fftshift) are applied exactly once in the pipeline
4. Modified get_vacc_data to use the stable readout methods
5. Implemented proper FPGA connection cleanup
------------------------------------------------------------------
These changes prevent spectrum drift due to reading BRAMs during writing operations.
The drift issue occurs when BRAM addresses wrap at 2048 words, causing the splice
point to move with each read if not synchronized with the FPGA dumps.
"""

# FPGA Spectrometer Configuration for 16k-point FFT
NFFT = 16384
NPAR = 8
WORDS_PER_BRAM = NFFT // NPAR      # 2048
BYTES_PER_WORD = 8
BRAM_NAMES = [f"q{i}" for i in range(1, NPAR+1)]

# Helper functions for stable FPGA readout
def wait_for_dump(fpga, last_cnt, poll=0.0005):
    """
    Spin until 'acc_cnt' increments **and** remains unchanged for
    one extra poll → guarantees VACC has finished writing.
    Returns the new acc_cnt value.
    """
    while True:
        now = fpga.read_uint('acc_cnt')
        if now != last_cnt:
            time.sleep(poll)
            if fpga.read_uint('acc_cnt') == now:  # stable → done
                return now
        time.sleep(poll)

def read_spectrum(fpga):
    """Return float64[16384] linear power array."""
    per_bram = []
    for name in BRAM_NAMES:
        raw = fpga.read(name, WORDS_PER_BRAM*BYTES_PER_WORD, 0)
        per_bram.append(np.frombuffer(raw, dtype=">u8").astype(np.float64))

    interwoven = np.empty(NFFT, dtype=np.float64)
    idx = 0
    for w in range(WORDS_PER_BRAM):
        for b in range(NPAR):
            interwoven[idx] = per_bram[b][w]
            idx += 1

    return interwoven / (2.0**34)      # fixed-point → linear power

# Constants
DATA_SAVE_INTERVAL = 600  # Interval to save files in seconds (10 minutes)

# Global Variables
last_file_rotation_time = time.time()
spectrum_file = None
power_file = None

# System Parameters
SAMPLE_RATE = 3932.16  # Sample rate in MSPS
DECIMATION_FACTOR = 2  # Decimation factor
EFFECTIVE_SAMPLE_RATE = SAMPLE_RATE / DECIMATION_FACTOR  # Effective sample rate after decimation

# Local oscillator and frequency settings
LO_FREQUENCY = 19.0    # Local oscillator frequency in GHz
IF_LOWER = 20.96608    # Lower bound of frequency range in GHz
IF_UPPER = 22.93216    # Upper bound of frequency range in GHz

# Water maser specific settings
WATER_MASER_FREQ = 21.235  # Water maser frequency in GHz
WATER_MASER_WINDOW = 0.1  # Width of integration window in GHz (±50 MHz)

# Zoom window configuration - same for both zoomed spectrum and power plot
ZOOM_WINDOW_WIDTH = 0.012  # Width of zoom window in GHz (±12 MHz on each side)
ZOOM_CENTER_FREQ = 22.235  # Center frequency for zoom window in GHz (water maser)

# Visualization parameters
MAX_POWER_SAMPLES = 1000  # Maximum number of power samples to store
DISPLAY_WINDOW = 20  # Display window in seconds
UPDATE_INTERVAL = 50  # Animation update interval in ms

# Integration settings
INTEGRATION_TIMES = [0, 1, 2, 3]  # Integration options in seconds (0 = no integration)
current_integration_idx = 0  # Start with no integration
integration_buffer = {}  # Buffer to store spectra for integration
integration_results = {}  # Store integrated results

# Thread-safe queues for data exchange
spectrum_queue = queue.Queue(maxsize=100)
maser_power_queue = queue.Queue(maxsize=1000)
stop_event = threading.Event()

# Define the square-law integrator function
def square_law_integrator(spectrum):
    power = np.abs(spectrum) ** 2
    return np.sum(power)

def get_vacc_data(fpga, nchannels=8, nfft=16384, debug=False, raw_mode=False, last_cnt=None):
    """
    Read accumulated spectral data from the FPGA with 16384-point FFT support.
    Now uses the safe wait_for_dump mechanism internally.
    """
    # Start with most recent acc_cnt if no previous value provided
    if last_cnt is None:
        last_cnt = fpga.read_uint('acc_cnt')
    
    # Wait for a complete FPGA dump
    acc_n = wait_for_dump(fpga, last_cnt)
    
    if debug:
        print(f"Current accumulation count: {acc_n}")
    
    # Use the stable read_spectrum function
    raw_spectrum = read_spectrum(fpga)
    
    # Write timestamp and data to file BEFORE any transformations
    timestamp = time.time()
    
    # No transformations applied here - return raw data with accumulation count
    # This ensures caller applies needed transformations exactly once
    return acc_n, raw_spectrum

def create_data_folder(base_path='data'):
    """Create a new directory for storing data files with a timestamp."""
    timestamp = time.strftime('%Y-%m-%d_%H-%M-%S', time.gmtime())
    folder_name = f"{timestamp}_BVEX_WaterMaser_data"
    folder_path = os.path.join(base_path, folder_name)
    if not os.path.exists(folder_path):
        os.makedirs(folder_path)
    return folder_path

def rotate_files(force_rotation=False):
    """Rotate the files based on a time interval."""
    global last_file_rotation_time, spectrum_file, power_file
    
    current_time = time.time()
    elapsed_time = current_time - last_file_rotation_time
    
    # Rotate files if the interval has passed or force_rotation is True
    if elapsed_time >= DATA_SAVE_INTERVAL or force_rotation:
        if spectrum_file is not None and not spectrum_file.closed:
            spectrum_file.close()
        if power_file is not None and not power_file.closed:
            power_file.close()
        
        timestamp = time.strftime('%Y-%m-%d_%H-%M-%S', time.gmtime())
        spectrum_filename = f"{timestamp}_spectrum_data.txt"
        power_filename = f"{timestamp}_integrated_power_data.txt"
        
        folder_path = create_data_folder()
        spectrum_file_path = os.path.join(folder_path, spectrum_filename)
        power_file_path = os.path.join(folder_path, power_filename)
        
        spectrum_file = open(spectrum_file_path, 'w')
        power_file = open(power_file_path, 'w')
        
        last_file_rotation_time = current_time

def save_integrated_power_data(integrated_power):
    # Append the integrated power data with a raw ctime timestamp to the file
    if power_file is not None and not power_file.closed:
        timestamp = time.time()
        power_file.write(f"{timestamp} {integrated_power}\n")
        power_file.flush()

def close_files():
    """Ensure files are closed properly."""
    if 'spectrum_file' in globals() and spectrum_file and not spectrum_file.closed:
        spectrum_file.close()
    if 'power_file' in globals() and power_file and not power_file.closed:
        power_file.close()

atexit.register(close_files)

def calculate_baseband_subtracted_power(spectrum, faxis):
    """
    Calculate integrated power in the water maser frequency range 
    using baseband subtracted values from the zoom window.
    """
    # Get the zoomed region around the water maser
    zoom_mask = np.abs(faxis - ZOOM_CENTER_FREQ) < ZOOM_WINDOW_WIDTH
    
    # Check if we found any valid points in our range
    if not np.any(zoom_mask):
        print(f"Warning: No frequency points found in the zoom window around {ZOOM_CENTER_FREQ} GHz (±{ZOOM_WINDOW_WIDTH*1000} MHz)")
        return 0.0
    
    # Extract the zoomed spectrum and frequencies
    zoomed_spectrum = spectrum[zoom_mask]
    
    # Calculate the median of just this zoomed region
    baseline = np.median(zoomed_spectrum)
    
    # Subtract the baseline to get baseband-subtracted values
    baseband_subtracted = zoomed_spectrum - baseline
    
    # Calculate integrated power from baseband-subtracted values (linear space, not dB)
    # Convert from dB to linear, sum, then back to dB
    maser_power = np.sum(10**(baseband_subtracted/10))
    
    # Convert back to dB
    return 10 * np.log10(maser_power + 1e-10)

def process_spectrum_with_integration(timestamp, spectrum, faxis):
    """
    Process spectrum with temporal integration if enabled.
    
    IMPORTANT: spectrum should already have flip and fftshift applied.
    No additional transformations will be applied here.
    """
    global integration_buffer, integration_results
    
    # Get current integration time setting
    integration_time = INTEGRATION_TIMES[current_integration_idx]
    
    # If no integration, just calculate power and return
    if integration_time == 0:
        maser_power = calculate_baseband_subtracted_power(spectrum, faxis)
        return timestamp, spectrum, faxis, maser_power
    
    # Initialize buffer for this integration time if it doesn't exist
    if integration_time not in integration_buffer:
        integration_buffer[integration_time] = []
    
    # Add spectrum to integration buffer
    integration_buffer[integration_time].append((timestamp, spectrum))
    
    # Remove spectra older than the integration window
    current_buffer = integration_buffer[integration_time]
    while current_buffer and (timestamp - current_buffer[0][0]) > integration_time:
        current_buffer.pop(0)
    
    # If we have spectra to integrate
    if current_buffer:
        # Average the spectra (in linear space, not dB)
        timestamps = [t for t, _ in current_buffer]
        avg_timestamp = sum(timestamps) / len(timestamps)
        
        # Convert from dB to linear, average, then back to dB
        linear_spectra = [10**(s/10) for _, s in current_buffer]
        avg_linear_spectrum = np.mean(linear_spectra, axis=0)
        avg_spectrum = 10 * np.log10(avg_linear_spectrum + 1e-10)
        
        # Calculate maser power from integrated spectrum
        maser_power = calculate_baseband_subtracted_power(avg_spectrum, faxis)
        
        # Store result
        integration_results[integration_time] = (avg_timestamp, avg_spectrum, maser_power)
        
        return avg_timestamp, avg_spectrum, faxis, maser_power
    
    # If no valid integration yet, return None
    return None

def key_press_handler(event, fig):
    """Handle keyboard events."""
    global current_integration_idx
    
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
    elif event.key == 'i':
        # Cycle through integration times
        current_integration_idx = (current_integration_idx + 1) % len(INTEGRATION_TIMES)
        integration_time = INTEGRATION_TIMES[current_integration_idx]
        if integration_time == 0:
            print(f"Integration disabled - using raw spectra")
        else:
            print(f"Integration time set to {integration_time} seconds")
        
        # Clear integration buffers when changing integration time
        integration_buffer.clear()
        integration_results.clear()

def data_reader_thread(fpga, nchannels=8, nfft=16384):
    """Thread to continuously read data from the FPGA and put it in the queues."""
    
    # Calculate frequency axis correctly according to the actual hardware configuration
    df = (IF_UPPER - IF_LOWER) / nfft  # Frequency step per bin
    frequencies = np.linspace(IF_LOWER, IF_UPPER, nfft, endpoint=False)
    
    spectrum_buffer = np.zeros(nfft)
    current_time = time.time()
    
    # Initialize the last accumulation count
    last_acc_n = fpga.read_uint('acc_cnt')
    
    while not stop_event.is_set():
        try:
            # Get new spectrum data using safe readout (which handles wait_for_dump internally)
            acc_n, raw_spectrum = get_vacc_data(fpga, nchannels=nchannels, nfft=nfft, last_cnt=last_acc_n)
            last_acc_n = acc_n  # Update for next iteration
            
            # PROCESS ONCE: Apply all transformations in one fixed sequence
            # 1. Flip the spectrum to account for 2nd Nyquist zone sampling
            # 2. Apply fftshift to center DC component
            flipped_spectrum = np.flip(raw_spectrum)  # Step 1
            fftshifted_spectrum = fft.fftshift(flipped_spectrum)  # Step 2
            
            # Convert to dB scale for visualization
            spectrum_db = 10 * np.log10(fftshifted_spectrum + 1e-10)
            
            # Get timestamp for this sample
            timestamp = time.time()
            
            # Write processed spectrum to file
            if spectrum_file is not None and not spectrum_file.closed:
                spectrum_file.write(f"{timestamp} " + " ".join(map(str, spectrum_db)) + "\n")
                spectrum_file.flush()
            
            # Process spectrum with temporal integration
            result = process_spectrum_with_integration(timestamp, spectrum_db, frequencies)
            
            if result:
                processed_time, processed_spectrum, processed_faxis, maser_power = result
                
                # Put processed spectrum in queue
                try:
                    if spectrum_queue.full():
                        spectrum_queue.get_nowait()
                    spectrum_queue.put((processed_time, processed_spectrum, processed_faxis))
                except queue.Full:
                    pass
                
                # Put maser power in queue
                try:
                    if maser_power_queue.full():
                        maser_power_queue.get_nowait()
                    maser_power_queue.put((processed_time, maser_power))
                except queue.Full:
                    pass
            
            # Calculate the time difference since the last integration
            new_time = time.time()
            time_diff = new_time - current_time
            
            if time_diff >= 1.0:
                current_time = new_time
                # Use raw_spectrum for integrated power calculation (linear domain)
                integral_power = square_law_integrator(raw_spectrum)
                spectrum_buffer = np.zeros(nfft)  # Reset the buffer
                
                # Save the integrated power data
                save_integrated_power_data(integral_power)
            else:
                # Accumulate spectrum data in the buffer
                spectrum_buffer += raw_spectrum
                
            # Rotate files if needed
            rotate_files()
            
        except Exception as e:
            print(f"Error in data reader thread: {e}")
            import traceback
            traceback.print_exc()
            time.sleep(1)  # Sleep longer on error

def plot_spectrum(fpga, cx=True, num_acc_updates=None, debug=False):
    """Plot the spectrum data using the improved visualization."""
    # Add error handling to catch any issues with FPGA communication
    try:
        # Test FPGA connection first
        fpga.read_uint('acc_cnt')
    except Exception as e:
        print(f"Error connecting to FPGA: {e}")
        print("Please check your connection and try again.")
        return
    
    # Create figure and axes with 3 subplots
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 12), 
                                      gridspec_kw={'height_ratios': [1, 1, 1]})
    
    # Create more space for titles and labels
    plt.subplots_adjust(left=0.1, right=0.95, top=0.92, bottom=0.08, hspace=0.4)
    
    # Set the main title with more top margin
    fig.suptitle(f'Water Maser ({WATER_MASER_FREQ} GHz) Observation', fontsize=16, y=0.98)
    
    # Define FFT parameters
    if cx:
        print('Complex design with 16384-point FFT')
        Nfft = 16384
        nchannels = 8
    else:
        print('Real design - not supported with current parameters')
        return
    
    # Calculate frequency axis with correct mapping to actual frequencies
    frequencies = np.linspace(IF_LOWER, IF_UPPER, Nfft, endpoint=False)
    
    # Calculate the frequency resolution in kHz
    freq_resolution_khz = ((IF_UPPER - IF_LOWER) / Nfft) * 1000
    
    # Initial empty plots
    # 1. Full spectrum plot
    spectrum_line, = ax1.plot(frequencies, np.zeros(Nfft), '-', linewidth=1)
    ax1.set_xlabel('Frequency (GHz)')
    ax1.set_ylabel('Power (dB)')
    ax1.set_title('Full Spectrum', pad=10)
    ax1.grid(True, linestyle='--', alpha=0.7)
    
    # Add maser region highlighting
    maser_region = ax1.axvspan(WATER_MASER_FREQ - WATER_MASER_WINDOW, 
                              WATER_MASER_FREQ + WATER_MASER_WINDOW, 
                              alpha=0.3, color='r', 
                              label=f'H₂O Maser ({WATER_MASER_FREQ} GHz ± {WATER_MASER_WINDOW*1000:.1f} MHz)')
    
    ax1.legend(loc='upper right')
    
    # Add text for integration status
    integration_text = ax1.text(0.02, 0.95, 'Integration: OFF', transform=ax1.transAxes,
                         fontsize=9, bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    
    # 2. Zoomed-in baseband subtracted spectrum
    zoom_spectrum_line, = ax2.plot(frequencies, np.zeros(Nfft), '-', color='green', linewidth=1)
    ax2.set_xlabel('Frequency (GHz)')
    ax2.set_ylabel('Power - Baseline (dB)')
    ax2.set_title(f'Baseband Subtracted Spectrum (±{ZOOM_WINDOW_WIDTH*1000:.1f} MHz)', pad=10)
    ax2.grid(True, linestyle='--', alpha=0.7)
    
    # Set initial zoom range for baseband subtracted plot
    ax2.set_xlim(ZOOM_CENTER_FREQ - ZOOM_WINDOW_WIDTH, ZOOM_CENTER_FREQ + ZOOM_WINDOW_WIDTH)
    
    # Add text for baseline level
    baseline_text = ax2.text(0.02, 0.95, 'Baseline: N/A', transform=ax2.transAxes,
                           fontsize=9, bbox=dict(boxstyle="round,pad=0.3", fc="white", alpha=0.8))
    
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
    
    # Start the data reader thread
    reader_thread = threading.Thread(target=data_reader_thread, 
                                    args=(fpga, nchannels, Nfft))
    reader_thread.daemon = True
    reader_thread.start()
    
    def update(frame):
        nonlocal start_time
        
        try:
            # Get latest spectrum data
            latest_spectrum_data = None
            try:
                # Get the newest spectrum data, discarding older ones if multiple are available
                while not spectrum_queue.empty():
                    latest_spectrum_data = spectrum_queue.get_nowait()
            except queue.Empty:
                pass
                
            # Get all available maser power data
            new_maser_power_data = []
            try:
                while not maser_power_queue.empty():
                    new_maser_power_data.append(maser_power_queue.get_nowait())
            except queue.Empty:
                pass
                
            # Update spectrum plots if new data is available
            if latest_spectrum_data:
                spectrum_time, spectrum, freq_axis = latest_spectrum_data
                
                # Update current full spectrum plot
                spectrum_line.set_ydata(spectrum)
                
                # Get the zoomed region for baseband subtraction
                zoom_mask = np.abs(freq_axis - ZOOM_CENTER_FREQ) < ZOOM_WINDOW_WIDTH
                zoomed_spectrum = spectrum[zoom_mask]
                zoomed_freq = freq_axis[zoom_mask]
                
                if len(zoomed_spectrum) > 0:
                    # Compute the baseline (median of just the zoomed window)
                    baseline = np.median(zoomed_spectrum)
                    
                    # Create baseband subtracted spectrum for the zoomed region
                    baseband_subtracted = zoomed_spectrum - baseline
                    
                    # Create a full spectrum array with zeros except for our zoomed region
                    full_baseband_subtracted = np.zeros_like(spectrum)
                    full_baseband_subtracted[zoom_mask] = baseband_subtracted
                    
                    # Update the baseband subtracted plot
                    zoom_spectrum_line.set_ydata(full_baseband_subtracted)
                    
                    # Update baseline text
                    baseline_text.set_text(f'Baseline: {baseline:.2f} dB')
                
                # Adjust y-axis limits with some padding for full spectrum
                y_min, y_max = spectrum.min(), spectrum.max()
                y_range = y_max - y_min
                ax1.set_ylim([y_min - 0.05*y_range, y_max + 0.05*y_range])
                
                # Adjust y-axis limits for baseband subtracted plot
                if len(zoomed_spectrum) > 0:
                    zoomed_y_min, zoomed_y_max = baseband_subtracted.min(), baseband_subtracted.max()
                    zoomed_y_range = max(zoomed_y_max - zoomed_y_min, 0.2)  # Ensure minimum range
                    # Add a buffer of 10% on each side
                    ax2.set_ylim([zoomed_y_min - 0.1*zoomed_y_range, zoomed_y_max + 0.1*zoomed_y_range])
                
                # Update main title with timestamp and resolution
                current_time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(spectrum_time))
                fig.suptitle(f'Water Maser ({WATER_MASER_FREQ} GHz) Observation - {current_time_str} - Resolution: 120 kHz', 
                           fontsize=16, y=0.98)
                
                # Update integration status display
                integration_time = INTEGRATION_TIMES[current_integration_idx]
                if integration_time == 0:
                    integration_text.set_text('Integration: OFF')
                else:
                    integration_text.set_text(f'Integration: {integration_time}s')
                
            # Update maser power meter plot with new data
            if new_maser_power_data:
                for t, p in new_maser_power_data:
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
                maser_power_line.set_data(list(power_times), list(power_values))
                
                # Calculate and display average maser power
                if power_values:
                    avg_power = sum(power_values) / len(power_values)
                    avg_power_text.set_text(f'Average Maser Power: {avg_power:.2f} dB')
                
                # Dynamically adjust x-axis to keep the plot centered
                if power_times:
                    current_time = power_times[-1]
                    ax3.set_xlim(max(0, current_time - DISPLAY_WINDOW), current_time + 1)
                
                # Dynamically adjust y-axis with some padding
                if power_values:
                    y_min = min(power_values)
                    y_max = max(power_values)
                    y_range = max(y_max - y_min, 0.2)  # Ensure minimum range
                    ax3.set_ylim(y_min - 0.1*y_range, y_max + 0.1*y_range)
                    
        except Exception as e:
            print(f"Error updating plot: {e}")
            import traceback
            traceback.print_exc()
        
        return spectrum_line, zoom_spectrum_line, maser_power_line, avg_power_text, integration_text, baseline_text
    
    # Add instructions to the figure
    fig.text(0.5, 0.01, 
            "Press 's' to save plot | 'i' to cycle integration time | 'q' to quit", 
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
    p = OptionParser()
    p.set_usage('rfsoc4x2_tut_spec.py <HOSTNAME_or_IP> cx|real [options]')
    p.set_description(__doc__)
    p.add_option('-l', '--acc_len', dest='acc_len', type='int', default=32768,
        help='Set the number of vectors to accumulate between dumps. Default is 2*(2^28)/16384')
    p.add_option('-s', '--skip', dest='skip', action='store_true',
        help='Skip programming and begin to plot data')
    p.add_option('-b', '--fpg', dest='fpgfile', type='str', default='',
        help='Specify the fpg file to load')
    p.add_option('-a', '--adc', dest='adc_chan_sel', type=int, default=0,
        help='ADC input to select. Values are 0, 1, 2, or 3')
    p.add_option('-d', '--debug', dest='debug', action='store_true',
        help='Enable debug output')
    p.add_option('-W', '--water-maser-freq', dest='water_maser_freq', type='float', default=22.235,
        help='Set the water maser frequency in GHz (default: 22.235 GHz)')
    p.add_option('-I', '--integration-width', dest='integration_width', type='float', default=50.0,
        help='Set the integration width in MHz (default: ±50 MHz)')
    p.add_option('-Z', '--zoom-width', dest='zoom_width', type='float', default=12.0,
        help='Set the zoom window width in MHz (default: ±12 MHz)')
    p.add_option('-C', '--zoom-center', dest='zoom_center', type='float', default=22.235,
        help='Set the zoom window center frequency in GHz (default: 22.235 GHz)')

    opts, args = p.parse_args(sys.argv[1:])
    if len(args) < 2:
        print('Specify a hostname or IP for your CASPER platform and either cx or real to indicate the type of spectrometer design.')
        print('Run with the -h flag to see all options.')
        exit()
    else:
        hostname = args[0]
        mode_str = args[1]
        if mode_str == 'cx':
            mode = 1
        elif mode_str == 'real':
            mode = 0
        else:
            print('Operation mode not recognized. Must be "cx" or "real".')
            exit()

    if opts.fpgfile != '':
        bitstream = opts.fpgfile
    else:
        if mode == 1:
            fpg_prebuilt = 'rfsoc4x2_tut_spec_cx_14pt_fft/outputs/rfsoc4x2_tut_spec_cx_14pt_fft_2025-05-19_1245.fpg'
        else:
            fpg_prebuilt = './rfsoc4x2_tut_spec/outputs/rfsoc4x2_tut_spec_2023-03-31_1109.fpg'

        print(f'Using FPGA file at {fpg_prebuilt}')
        bitstream = fpg_prebuilt

    if opts.adc_chan_sel < 0 or opts.adc_chan_sel > 3:
        print('ADC select must be 0, 1, 2, or 3')
        exit()
        
    # Update global parameters from command line options
    WATER_MASER_FREQ = opts.water_maser_freq
    WATER_MASER_WINDOW = opts.integration_width / 1000.0  # Convert MHz to GHz
    ZOOM_WINDOW_WIDTH = opts.zoom_width / 1000.0  # Convert MHz to GHz
    ZOOM_CENTER_FREQ = opts.zoom_center

    print(f'Connecting to {hostname}...')
    fpga = casperfpga.CasperFpga(hostname)
    time.sleep(0.2)
    
    # Setup proper cleanup for FPGA connection
    if hasattr(fpga.transport, "close"):
        atexit.register(fpga.transport.close)
    elif hasattr(fpga.transport, "stop"):
        atexit.register(fpga.transport.stop)

    if not opts.skip:
        print(f'Programming FPGA with {bitstream}...')
        fpga.upload_to_ram_and_program(bitstream)
        print('Done')
    else:
        fpga.get_system_information()
        print('Skip programming FPGA...')

    print('Configuring accumulation period...')
    fpga.write_int('acc_len', opts.acc_len)
    time.sleep(0.1)
    print('Done')

    print(f'Setting capture on ADC port {opts.adc_chan_sel}')
    fpga.write_int('adc_chan_sel', opts.adc_chan_sel)
    time.sleep(0.1)
    print('Done')

    print('Resetting counters...')
    fpga.write_int('cnt_rst', 1)
    fpga.write_int('cnt_rst', 0)
    time.sleep(5)
    print('Done')
    
    rotate_files(force_rotation=True)

    try:
        plot_spectrum(fpga, cx=mode, debug=opts.debug)
    except KeyboardInterrupt:
        close_files()
        print("Program terminated by user")
        exit()
    except Exception as e:
        close_files()
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        exit()
    finally:
        # Clean up resources
        stop_event.set()
        close_files()
        print("Resources cleaned up.")
