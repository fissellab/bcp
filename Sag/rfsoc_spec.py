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

def setup_logging(logpath):
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s',
                        handlers=[logging.FileHandler(logpath), logging.StreamHandler()])

def signal_handler(sig, frame):
    global running
    logging.info("Received termination signal. Cleaning up...")
    running = False

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

        spectrum_data = np.array(interleave_q, dtype=np.float64)
        timestamp = time.time()

        if spectrum_file:
            spectrum_file.write(f"{timestamp} " + " ".join(map(str, np.fft.fftshift(np.flip(spectrum_data)))) + "\n")
            spectrum_file.flush()  # Flush the data to disk immediately
            os.fsync(spectrum_file.fileno())  # Ensure it's written to the physical disk
        return acc_n, spectrum_data
    except Exception as e:
        logging.error(f"Error in get_vacc_data: {e}")
        return None, None

def create_data_folder():
    try:
        timestamp = time.strftime('%Y-%m-%d_%H-%M', time.gmtime())
        folder_name = f"{timestamp}_BVEX_data"
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
        spectrum_filename = f"{timestamp}_spectrum_data.txt"
        power_filename = f"{timestamp}_integrated_power_data.txt"

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
            power_file.flush()  # Flush the data to disk immediately
            os.fsync(power_file.fileno())  # Ensure it's written to the physical disk
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
    logging.info("Data acquisition and saving in progress...")
    last_sync_time = time.time()
    last_acc_n = fpga.read_uint('acc_cnt')  # Get initial accumulation count
    
    data_folder = create_data_folder()
    if data_folder is None:
        logging.error("Failed to create data folder. Exiting.")
        return

    rotate_files(force_rotation=True)
    
    # Performance monitoring variables
    sample_count = 0
    start_monitor_time = time.time()
    actual_intervals = []

    while running:
        # Read current accumulation count
        current_acc_n = fpga.read_uint('acc_cnt')
        
        # Only process data when accumulation count changes
        if current_acc_n != last_acc_n:
            actual_intervals.append(time.time() - start_monitor_time if sample_count == 0 else time.time() - last_sample_time)
            last_sample_time = time.time()
            
            # Get and process spectrum data
            acc_n, spectrum = get_vacc_data(fpga, num_channels, num_fft_points)
            if spectrum is not None:
                integral_power = square_law_integrator(spectrum)
                save_integrated_power_data(integral_power)
                
                # Update accumulation counter
                last_acc_n = current_acc_n
                sample_count += 1
                
                # Log performance stats every 20 samples (about every 5 seconds at 3.67 Hz)
                if sample_count % 20 == 0:
                    duration = time.time() - start_monitor_time
                    avg_rate = sample_count / duration
                    avg_interval = sum(actual_intervals) / len(actual_intervals)
                    jitter = max(actual_intervals) - min(actual_intervals)
                   #logging.info(f"Performance: {avg_rate:.2f} Hz, Avg interval: {avg_interval*1000:.2f}ms, Jitter: {jitter*1000:.2f}ms")
                    
                    # Reset monitoring stats
                    sample_count = 0
                    start_monitor_time = time.time()
                    actual_intervals = []
            else:
                logging.warning("No spectrum data acquired. Skipping this interval.")
                last_acc_n = current_acc_n  # Update counter even on error
        
        # Perform file system operations less frequently
        current_time = time.time()
        if current_time - last_sync_time > 60:
            os.sync()
            last_sync_time = current_time
            rotate_files()
        
        # Sleep to avoid constantly polling the FPGA
        time.sleep(0.1)  # Check for new data 10 times per second
    
    logging.info("Data acquisition stopped.")

if __name__ == "__main__":
    p = OptionParser()
    p.set_usage('rfsoc_spec.py <HOSTNAME_or_IP> <log_path> <mode> [options]')
    p.set_description(__doc__)
    p.add_option('-l', '--acc_len', dest='acc_len', type='int', default=2*(2**28) // 2048,
                 help='Set the number of vectors to accumulate between dumps. Default is 2*(2^28)/2048')
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
    p.add_option('-f', '--fft_points', dest='num_fft_points', type=int, default=2048,
                 help='Number of FFT points. Default is 2048')

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
            fpg_prebuilt = './rfsoc4x2_tut_spec_cx/outputs/rfsoc4x2_tut_spec_cx_2023-04-03_1141.fpg'
        else:
            fpg_prebuilt = './rfsoc4x2_tut_spec/outputs/rfsoc4x2_tut_spec_2023-03-31_1109.fpg'

        logging.info(f'Using FPGA file at {fpg_prebuilt}')
        bitstream = fpg_prebuilt

    if opts.adc_chan_sel < 0 or opts.adc_chan_sel > 3:
        logging.error('ADC select must be 0, 1, 2, or 3')
        exit()

    logging.info(f'Connecting to {hostname}...')
    fpga = casperfpga.CasperFpga(hostname)
    time.sleep(0.2)

    if not opts.skip:
        logging.info(f'Programming FPGA with {bitstream}...')
        fpga.upload_to_ram_and_program(bitstream)
        logging.info('Done')
    else:
        fpga.get_system_information()
        logging.info('Skip programming FPGA...')

    logging.info('Configuring accumulation period...')
    fpga.write_int('acc_len', opts.acc_len)
    time.sleep(0.1)
    logging.info('Done')

    logging.info(f'Setting capture on ADC port {opts.adc_chan_sel}')
    fpga.write_int('adc_chan_sel', opts.adc_chan_sel)
    time.sleep(0.1)
    logging.info('Done')

    logging.info('Resetting counters...')
    fpga.write_int('cnt_rst', 1)
    fpga.write_int('cnt_rst', 0)
    time.sleep(5)
    logging.info('Done')

    try:
        logging.info('Starting data acquisition...')
        start_data_acquisition(fpga, mode, opts.num_channels, opts.num_fft_points)
    except Exception as e:
        logging.error(f"Unexpected error: {e}")
    finally:
        logging.info('Cleaning up...')
        close_files()
        logging.info('Script terminated.')

