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

# System Parameters for 479 kHz resolution using 4096-point FFT
SAMPLE_RATE = 3932.16  # Sample rate in MSPS
BANDWIDTH = SAMPLE_RATE / 2  # MHz for complex sampling (Nyquist frequency)
FFT_SIZE = 4096  # Number of FFT points for 479 kHz resolution
FREQUENCY_RESOLUTION = BANDWIDTH / FFT_SIZE  # 1966.08/4096 = ~0.479 MHz or ~479 kHz

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

def square_law_integrator(spectrum):
    power = np.abs(spectrum) ** 2
    return np.sum(power)

def get_vacc_data(fpga, nchannels, nfft):
    try:
        acc_n = fpga.read_uint('acc_cnt')
        chunk = nfft // nchannels
        raw = np.zeros((nchannels, chunk))
        for i in range(nchannels):
            raw[i, :] = struct.unpack('>{:d}Q'.format(chunk), fpga.read('q{:d}'.format((i+1)), chunk*8, 0))

        interleave_q = []
        for i in range(chunk):
            for j in range(nchannels):
                interleave_q.append(raw[j, i])

        # Convert to float64 and apply fixed-point scaling
        spectrum_data = np.array(interleave_q, dtype=np.float64) / (2.0**34)
        timestamp = time.time()

        if spectrum_file:
            # Store RAW data with no transformations - transformations will be applied
            # consistently in the visualization code
            spectrum_file.write(f"{timestamp} " + " ".join(map(str, spectrum_data)) + "\n")
            spectrum_file.flush()
            os.fsync(spectrum_file.fileno())
        return acc_n, spectrum_data
    except Exception as e:
        logging.error(f"Error in get_vacc_data: {e}")
        return None, None

def create_data_folder():
    try:
        timestamp = time.strftime('%Y-%m-%d_%H-%M', time.gmtime())
        folder_name = f"{timestamp}_BVEX_479KHz_data"
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
        spectrum_filename = f"{timestamp}_479KHz_spectrum_data.txt"
        power_filename = f"{timestamp}_479KHz_integrated_power_data.txt"

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

def start_data_acquisition(fpga, mode, num_channels, num_fft_points):
    global running, data_folder
    
    # Log basic configuration information
    logging.info(f"Starting 479 kHz resolution spectrometer acquisition")
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
            # Read current accumulation count
            current_acc_n = fpga.read_uint('acc_cnt')
            
            # Only process data when accumulation count changes
            if current_acc_n != last_acc_n:
                # Get and process spectrum data
                acc_n, spectrum = get_vacc_data(fpga, num_channels, num_fft_points)
                if spectrum is not None:
                    integral_power = square_law_integrator(spectrum)
                    save_integrated_power_data(integral_power)
                    
                    # Update accumulation counter
                    last_acc_n = current_acc_n
                else:
                    # Update counter even on error
                    last_acc_n = current_acc_n
            
            # Perform file system operations less frequently
            current_time = time.time()
            if current_time - last_sync_time > 60:
                os.sync()
                last_sync_time = current_time
                rotate_files()
            
            # Sleep to avoid constantly polling the FPGA
            time.sleep(0.1)
            
        except Exception as e:
            logging.error(f"Error in main acquisition loop: {e}")
            time.sleep(0.1)
    
    # Ensure clean shutdown
    close_files()
    logging.info("Data acquisition stopped.")

if __name__ == "__main__":
    p = OptionParser()
    p.set_usage('rfsoc_spec_479khz.py <HOSTNAME_or_IP> <log_path> <mode> [options]')
    p.set_description(__doc__)
    p.add_option('-l', '--acc_len', dest='acc_len', type='int', default=2*(2**28) // 4096,
                 help='Set the number of vectors to accumulate between dumps. Default is 2*(2^28)/4096')
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
    p.add_option('-c', '--channels', dest='num_channels', type=int, default=8,
                 help='Number of channels. Default is 8')
    p.add_option('-f', '--fft_points', dest='num_fft_points', type=int, default=4096,
                 help='Number of FFT points. Default is 4096')

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
            fpg_prebuilt = 'rfsoc4x2_tut_spec_cx/outputs/rfsoc4x2_tut_spec_cx_479khz_2025-04-22_1723.fpg'
        else:
            fpg_prebuilt = './rfsoc4x2_tut_spec/outputs/rfsoc4x2_tut_spec_2023-03-31_1109.fpg'

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
        
        # Start acquisition
        start_data_acquisition(fpga, mode, opts.num_channels, opts.num_fft_points)
    except KeyboardInterrupt:
        logging.info('Interrupted by user')
    except Exception as e:
        logging.error(f'Exception during data acquisition: {e}')

    logging.info('Script finished')
