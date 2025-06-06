#!/usr/bin/env python3
"""
Debug script to analyze spectrum data at different stages:
1. Raw FPGA data
2. Processed spectrum data  
3. Shared memory data
"""

import sys
import time
import struct
import numpy as np
import casperfpga
import mmap
from optparse import OptionParser

# Shared memory constants
SHM_NAME = "/bcp_spectrometer_data"
SPEC_TYPE_STANDARD = 1

def analyze_fpga_data(fpga, nchannels, nfft):
    """Analyze raw FPGA data"""
    print("=" * 60)
    print("FPGA DATA ANALYSIS")
    print("=" * 60)
    
    try:
        acc_n = fpga.read_uint('acc_cnt')
        print(f"Accumulation count: {acc_n}")
        
        chunk = nfft // nchannels
        print(f"Channels: {nchannels}, FFT points: {nfft}, Chunk size: {chunk}")
        
        raw = np.zeros((nchannels, chunk))
        for i in range(nchannels):
            raw_data = fpga.read('q{:d}'.format((i+1)), chunk*8, 0)
            raw[i, :] = struct.unpack('>{:d}Q'.format(chunk), raw_data)
            print(f"Channel {i+1} raw data sample (first 10): {raw[i, :10]}")
            print(f"Channel {i+1} min: {np.min(raw[i, :])}, max: {np.max(raw[i, :])}")
            print(f"Channel {i+1} non-zero count: {np.count_nonzero(raw[i, :])}")

        # Interleave data
        interleave_q = []
        for i in range(chunk):
            for j in range(nchannels):
                interleave_q.append(raw[j, i])

        spectrum_data = np.array(interleave_q, dtype=np.float64)
        
        print(f"\nInterleaved spectrum data:")
        print(f"Length: {len(spectrum_data)}")
        print(f"First 10 values: {spectrum_data[:10]}")
        print(f"Min: {np.min(spectrum_data)}, Max: {np.max(spectrum_data)}")
        print(f"Non-zero count: {np.count_nonzero(spectrum_data)}")
        print(f"Zero count: {np.sum(spectrum_data == 0)}")
        
        # Process the spectrum
        processed_spectrum = np.fft.fftshift(np.flip(spectrum_data))
        
        print(f"\nProcessed spectrum data:")
        print(f"Length: {len(processed_spectrum)}")
        print(f"First 10 values: {processed_spectrum[:10]}")
        print(f"Min: {np.min(processed_spectrum)}, Max: {np.max(processed_spectrum)}")
        print(f"Non-zero count: {np.count_nonzero(processed_spectrum)}")
        print(f"Zero count: {np.sum(processed_spectrum == 0)}")
        
        return processed_spectrum
        
    except Exception as e:
        print(f"Error analyzing FPGA data: {e}")
        return None

def analyze_shared_memory():
    """Analyze what's in shared memory"""
    print("\n" + "=" * 60)
    print("SHARED MEMORY ANALYSIS")
    print("=" * 60)
    
    try:
        # Open shared memory
        with open(f"/dev/shm{SHM_NAME}", "r+b") as fd:
            shm_size = 131096
            mm = mmap.mmap(fd.fileno(), shm_size)
            
            # Read header
            mm.seek(0)
            ready = struct.unpack('I', mm.read(4))[0]
            active_type = struct.unpack('I', mm.read(4))[0] 
            timestamp = struct.unpack('d', mm.read(8))[0]
            data_size = struct.unpack('I', mm.read(4))[0]
            
            print(f"Ready flag: {ready}")
            print(f"Active type: {active_type}")
            print(f"Timestamp: {timestamp}")
            print(f"Data size (bytes): {data_size}")
            print(f"Expected data points: {data_size // 8}")
            
            # Read data points
            data_points = data_size // 8
            data = []
            for i in range(min(data_points, 2048)):  # Limit to reasonable amount
                value = struct.unpack('d', mm.read(8))[0]
                data.append(value)
            
            data = np.array(data)
            print(f"\nShared memory data:")
            print(f"Length: {len(data)}")
            print(f"First 10 values: {data[:10]}")
            if len(data) > 10:
                print(f"Min: {np.min(data)}, Max: {np.max(data)}")
                print(f"Non-zero count: {np.count_nonzero(data)}")
                print(f"Zero count: {np.sum(data == 0)}")
            
            mm.close()
            
    except Exception as e:
        print(f"Error analyzing shared memory: {e}")

def main():
    p = OptionParser()
    p.set_usage('debug_spectrum.py <HOSTNAME_or_IP>')
    p.add_option('-c', '--channels', dest='num_channels', type=int, default=8,
                 help='Number of channels. Default is 8')
    p.add_option('-f', '--fft_points', dest='num_fft_points', type=int, default=2048,
                 help='Number of FFT points. Default is 2048')

    opts, args = p.parse_args(sys.argv[1:])
    if len(args) < 1:
        print('Specify a hostname or IP for your Casper platform')
        exit()
    
    hostname = args[0]
    print(f"Connecting to {hostname}...")
    
    try:
        fpga = casperfpga.CasperFpga(hostname)
        time.sleep(0.2)
        
        # Get current state
        fpga.get_system_information()
        
        # Wait for accumulation to change
        print("Waiting for new accumulation...")
        last_acc = fpga.read_uint('acc_cnt')
        start_time = time.time()
        
        while time.time() - start_time < 10:  # Wait up to 10 seconds
            current_acc = fpga.read_uint('acc_cnt')
            if current_acc != last_acc:
                break
            time.sleep(0.1)
        else:
            print("WARNING: No accumulation change detected in 10 seconds!")
        
        # Analyze FPGA data
        spectrum = analyze_fpga_data(fpga, opts.num_channels, opts.num_fft_points)
        
        # Analyze shared memory
        analyze_shared_memory()
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main() 