import os
import sys
import time
import struct
import numpy as np
import casperfpga
import atexit
import logging
import signal
from optparse import OptionParser

# Global Variables
last_file_rotation_time = time.time()
spectrum_file = None
power_file = None
running = True
DATA_SAVE_INTERVAL = 600  # Default value, will be overwritten by command line argument
DATA_SAVE_PATH = '/media/saggitarius/T7'  # Default value, will be overwritten by command line argument
data_folder = None

# System Parameters for 120 kHz resolution using 16384-point FFT
SAMPLE_RATE = 3932.16  # Sample rate in MSPS
BANDWIDTH = SAMPLE_RATE / 2  # MHz for complex sampling (Nyquist frequency)
FFT_SIZE = 16384  # Number of FFT points for 120 kHz resolution
FREQUENCY_RESOLUTION = BANDWIDTH / FFT_SIZE  # 1966.08/16384 = ~0.120 MHz or ~120 kHz

# FPGA Spectrometer Configuration for 16k-point FFT
NFFT = 16384
NPAR = 8
WORDS_PER_BRAM = NFFT // NPAR      # 2048
BYTES_PER_WORD = 8
BRAM_NAMES = [f"q{i}" for i in range(1, NPAR+1)]

def setup_logging(logpath):
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s',
                        handlers=[logging.FileHandler(logpath), logging.StreamHandler()])

def signal_handler(sig, frame):
    global running
    logging.info("Received termination signal. Cleaning up...")
    running = False
    close_files()
    sys.exit(0)

signal.signal(signal.SIGTERM, signal_handler)
signal.signal(signal.SIGINT, signal_handler)

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

def square_law_integrator(spectrum):
    power = np.abs(spectrum) ** 2
    return np.sum(power)

def get_vacc_data(fpga, last_cnt=None):
    """
    Read accumulated spectral data from the FPGA with 16384-point FFT support.
    Uses the safe wait_for_dump mechanism internally.
    """
    try:
        # Start with most recent acc_cnt if no previous value provided
        if last_cnt is None:
            last_cnt = fpga.read_uint('acc_cnt')
        
        # Wait for a complete FPGA dump
        acc_n = wait_for_dump(fpga, last_cnt)
        
        # Use the stable read_spectrum function
        raw_spectrum = read_spectrum(fpga)
        
        # Write timestamp and data to file BEFORE any transformations
        timestamp = time.time()
        
        if spectrum_file:
            # Store RAW data with no transformations
            spectrum_file.write(f"{timestamp} " + " ".join(map(str, raw_spectrum)) + "\n")
            spectrum_file.flush()
            os.fsync(spectrum_file.fileno())
            
        return acc_n, raw_spectrum
    except Exception as e:
        logging.error(f"Error in get_vacc_data: {e}")
        return None, None

def create_data_folder():
    try:
        timestamp = time.strftime('%Y-%m-%d_%H-%M', time.gmtime())
        folder_name = f"{timestamp}_BVEX_120KHz_data"
        folder_path = os.path.join(DATA_SAVE_PATH, folder_name)
        if not os.path.exists(folder_path):
            os.makedirs(folder_path)
        return folder_path
    except Exception as e:
        logging.error(f"Error creating data folder: {e}")
        return None

def rotate_files(force_rotation=False):
    global last_file_rotation_time, spectrum_file, power_file, data_folder

    current_time = time.time()
    elapsed_time = current_time - last_file_rotation_time

    if data_folder is None:
        data_folder = create_data_folder()
        if data_folder is None:
            logging.error("Failed to create data folder. Exiting.")
            sys.exit(1)

    if elapsed_time >= DATA_SAVE_INTERVAL or force_rotation:
        if spectrum_file:
            spectrum_file.flush()
            os.fsync(spectrum_file.fileno())
        if power_file:
            power_file.flush()
            os.fsync(power_file.fileno())
        
        close_files()

        timestamp = time.strftime('%Y-%m-%d_%H-%M-%S', time.gmtime())
        spectrum_filename = f"{timestamp}_120KHz_spectrum_data.txt"
        power_filename = f"{timestamp}_120KHz_integrated_power_data.txt"

        spectrum_file_path = os.path.join(data_folder, spectrum_filename)
        power_file_path = os.path.join(data_folder, power_filename)

        spectrum_file = open(spectrum_file_path, 'w')
        power_file = open(power_file_path, 'w')

        last_file_rotation_time = current_time

def save_integrated_power_data(integrated_power):
    try:
        timestamp = time.time()
        if power_file:
            power_file.write(f"{timestamp} {integrated_power}\n")
            power_file.flush()
            os.fsync(power_file.fileno())
    except Exception as e:
        logging.error(f"Error saving integrated power data: {e}")

def close_files():
    global spectrum_file, power_file
    try:
        if spectrum_file and not spectrum_file.closed:
            spectrum_file.close()
            spectrum_file = None
        if power_file and not power_file.closed:
            power_file.close()
            power_file = None
    except Exception as e:
        logging.error(f"Error closing files: {e}")

atexit.register(close_files)

def start_data_acquisition(fpga):
    global running, data_folder
    
    # Log basic configuration information
    logging.info(f"Starting 120 kHz resolution spectrometer acquisition")
    logging.info(f"FFT size: {NFFT}, Frequency resolution: {FREQUENCY_RESOLUTION:.6f} MHz")
    logging.info("Data acquisition in progress...")
    
    # Initialize data structures
    last_acc_n = fpga.read_uint('acc_cnt')
    last_sync_time = time.time()
    
    data_folder = create_data_folder()
    if data_folder is None:
        logging.error("Failed to create data folder. Exiting.")
        return

    rotate_files(force_rotation=True)

    while running:
        try:
            # Get and process spectrum data using the stable method
            acc_n, spectrum = get_vacc_data(fpga, last_cnt=last_acc_n)
            
            if acc_n is not None and spectrum is not None:
                # Update for next iteration
                last_acc_n = acc_n
                
                # Calculate integrated power
                integral_power = square_law_integrator(spectrum)
                save_integrated_power_data(integral_power)
            
            # Perform file system operations less frequently
            current_time = time.time()
            if current_time - last_sync_time > 60:
                os.sync()
                last_sync_time = current_time
                rotate_files()
            
            # Short sleep to prevent CPU saturation
            time.sleep(0.05)
            
        except Exception as e:
            logging.error(f"Error in main acquisition loop: {e}")
            time.sleep(0.1)
    
    # Ensure clean shutdown
    close_files()
    logging.info("Data acquisition stopped.")

if __name__ == "__main__":
    p = OptionParser()
    p.set_usage('rfsoc_spec_120khz.py <HOSTNAME_or_IP> <log_path> <mode> [options]')
    p.set_description(__doc__)
    p.add_option('-l', '--acc_len', dest='acc_len', type='int', default=2*(2**28) // 16384,
                 help='Set the number of vectors to accumulate between dumps. Default is 2*(2^28)/16384')
    p.add_option('-s', '--skip', dest='skip', action='store_true',
                 help='Skip programming and begin to plot data')
    p.add_option('-b', '--fpg', dest='fpgfile', type='str', default='',
                 help='Specify the fpg file to load')
    p.add_option('-a', '--adc', dest='adc_chan_sel', type=int, default=0,
                 help='ADC input to select. Values are 0, 1, 2, or 3')
    p.add_option('-i', '--interval', dest='data_save_interval', type=int, default=600,
                 help='Data save interval in seconds. Default is 600')
    p.add_option('-p', '--path', dest='data_save_path', type='string', default='/media/saggitarius/T7',
                 help='Data save path. Default is /media/saggitarius/T7')

    opts, args = p.parse_args(sys.argv[1:])
    if len(args) < 3:
        print('Specify a hostname or IP for your Casper platform, log path, and mode (cx or real) to indicate the type of spectrometer design. Run with the -h flag to see all options.')
        exit()
    else:
        hostname = args[0]
        logpath = args[1]
        mode_str = args[2]
        setup_logging(logpath)
        if mode_str == 'cx':
            mode = 'cx'
        elif mode_str == 'real':
            mode = 'real'
        else:
            logging.error('Operation mode not recognized. Must be "cx" or "real".')
            exit()

    # Use the data_save_interval and data_save_path from command line options
    DATA_SAVE_INTERVAL = opts.data_save_interval
    DATA_SAVE_PATH = opts.data_save_path

    if opts.fpgfile != '':
        bitstream = opts.fpgfile
    else:
        if mode == 'cx':
            fpg_prebuilt = 'rfsoc4x2_tut_spec_cx/outputs/rfsoc4x2_tut_spec_cx_14pt_fft_2025-05-19_1245.fpg'
        else:
            logging.error('Real mode is not supported for 120kHz resolution')
            exit()

        logging.info(f'Using FPGA file at {fpg_prebuilt}')
        bitstream = fpg_prebuilt

    # Try to connect to the FPGA
    fpga = None
    logging.info(f'Connecting to FPGA at {hostname}...')
    try:
        fpga = casperfpga.CasperFpga(host=hostname, transport=casperfpga.KatcpTransport)
        time.sleep(0.2)
        if fpga.is_connected():
            logging.info('Connected to FPGA')
            if hasattr(fpga.transport, "close"):
                atexit.register(fpga.transport.close)
            elif hasattr(fpga.transport, "stop"):
                atexit.register(fpga.transport.stop)
        else:
            logging.error('Failed to connect to FPGA')
            exit()
    except Exception as e:
        logging.error(f'Exception connecting to FPGA: {e}')
        exit()

    # Program or skip programming the FPGA
    if opts.skip:
        logging.info('Skipping programming as requested')
    else:
        logging.info(f'Programming FPGA with {bitstream}')
        try:
            fpga.upload_to_ram_and_program(bitstream)
            logging.info('FPGA programmed successfully')
            time.sleep(2)
        except Exception as e:
            logging.error(f'Failed to program FPGA: {e}')
            exit()

    # Start data acquisition
    try:
        # Get system info
        logging.info("Reading FPGA system information")
        fpga.get_system_information()
        
        # Set accumulation length
        try:
            logging.info(f"Setting acc_len to {opts.acc_len}")
            fpga.write_int('acc_len', opts.acc_len)
            time.sleep(0.1)
        except Exception as e:
            logging.warning(f"Issue with acc_len register: {e}")
            
        # Set ADC channel
        try:
            logging.info(f"Setting adc_chan_sel to {opts.adc_chan_sel}")
            fpga.write_int('adc_chan_sel', opts.adc_chan_sel)
            time.sleep(0.1)
        except Exception as e:
            logging.warning(f"Issue with adc_chan_sel register: {e}")
            
        # Reset counters
        try:
            logging.info("Resetting counters")
            fpga.write_int('cnt_rst', 1)
            fpga.write_int('cnt_rst', 0)
            time.sleep(5)
        except Exception as e:
            logging.warning(f"Issue with counter reset: {e}")
        
        # Start acquisition
        start_data_acquisition(fpga)
    except KeyboardInterrupt:
        logging.info('Interrupted by user')
    except Exception as e:
        logging.error(f'Exception during data acquisition: {e}')

    logging.info('Script finished') 