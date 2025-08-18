#!/usr/bin/env python3
"""
vlbi_data_logging_listener_fixed.py

VLBI Data Logging System for timestamp_gen_simple Design
UPDATED FOR CURRENT STATE: PPS reset debugging phase

Features:
- Captures packets with timestamp_gen_simple timestamps
- Handles CURRENT behavior: PPS increments work, cycles DON'T reset yet
- Logs raw packets with timestamp metadata
- Real-time monitoring of timestamp behavior
- Detects PPS reset issues and reports them
- Compatible with work-in-progress timestamp_gen_simple fixes

CURRENT STATUS:
-  PPS counter works (proper 2025 VDIF epoch, increments every second)
-  Cycles counter does NOT reset at PPS (continuously increases)
-  This will be fixed when updated VHDL with variable-based edge detection is deployed

Updated for the confirmed working timestamp layout:
- PPS counter: Header bytes 4-7 (Little Endian) - WORKING
- Cycles counter: Header bytes 8-11 (Little Endian) - NOT RESETTING YET
- VDIF epoch: 2025 compatible (789M+ seconds since Jan 1, 2000) - WORKING
"""

import sys, time, os
import struct
import numpy as np
import casperfpga
import signal
import atexit
import json
from datetime import datetime, timedelta
from multiprocessing.connection import Listener

# Updated constants based on analysis results  
VLBI_DATA_DIR = "/mnt/vlbi_data"  # Default fallback (overridden by -d parameter)
FILE_ROTATION_MINUTES = 10

# Confirmed packet structure from timestamp_gen_simple analysis
ACTUAL_PKT_SIZE = 8298  # Confirmed from analysis
ETH_IP_UDP_HEADER_SIZE = 42
PACKET_HEADER_SIZE = 64
PAYLOAD_SIZE = ACTUAL_PKT_SIZE - ETH_IP_UDP_HEADER_SIZE

# Timestamp layout (confirmed working for PPS, cycles not resetting yet)
TIMESTAMP_PPS_OFFSET = 4    # pps_cnt at header bytes 4-7 - WORKING
TIMESTAMP_CYCLES_OFFSET = 8 # cycles_per_pps at header bytes 8-11 - NOT RESETTING

# VDIF epoch constants
VDIF_EPOCH_UNIX = 946684800  # Jan 1, 2000 00:00:00 UTC
REASONABLE_2025_RANGE = (788000000, 800000000)  # Valid 2024-2025 VDIF range

# Expected system clock rate
SYSTEM_CLOCK_HZ = 256000000  # 256 MHz system clock

# Global FPGA object
fpga = None

def cleanup_qsfp():
    """Turn off QSFP port when script exits"""
    global fpga
    if fpga is not None and fpga.is_connected():
        print("\nShutting down QSFP port...")
        try:
            fpga.write_int('qsfp_rst', 0)
            time.sleep(0.1)
            print("QSFP port shutdown complete")
        except Exception as e:
            print(f"Error shutting down QSFP: {e}")

def signal_handler(sig, frame):
    """Handle keyboard interrupts and other signals"""
    print("\nReceived signal to exit. Cleaning up...")
    cleanup_qsfp()
    sys.exit(0)

def extract_timestamps(packet):
    """Extract timestamps from packet using confirmed working layout"""
    try:
        if len(packet) < ETH_IP_UDP_HEADER_SIZE + PACKET_HEADER_SIZE:
            return None, None, "Packet too short"

        # Extract packet header (skip Ethernet/IP/UDP)
        header_start = ETH_IP_UDP_HEADER_SIZE
        header = packet[header_start:header_start + PACKET_HEADER_SIZE]

        # Extract timestamps using confirmed working offsets (Little Endian)
        pps_bytes = header[TIMESTAMP_PPS_OFFSET:TIMESTAMP_PPS_OFFSET + 4]
        cycles_bytes = header[TIMESTAMP_CYCLES_OFFSET:TIMESTAMP_CYCLES_OFFSET + 4]

        pps_cnt = struct.unpack('<I', pps_bytes)[0]      # Little Endian 32-bit
        cycles_per_pps = struct.unpack('<I', cycles_bytes)[0]  # Little Endian 32-bit

        return pps_cnt, cycles_per_pps, "OK"

    except Exception as e:
        return None, None, f"Extraction error: {e}"

def validate_timestamps(pps_cnt, cycles_per_pps, previous_pps=None, previous_cycles=None):
    """
    Validate extracted timestamps against expected ranges
    UPDATED: Handle current state where cycles don't reset properly
    """
    issues = []
    warnings = []

    # Check PPS counter (should be VDIF seconds in 2025 range)
    if pps_cnt is None:
        issues.append("PPS counter is None")
    elif not (REASONABLE_2025_RANGE[0] <= pps_cnt <= REASONABLE_2025_RANGE[1]):
        issues.append(f"PPS counter {pps_cnt} outside 2025 range")

    # Check cycles counter (adjusted for current broken behavior)
    if cycles_per_pps is None:
        issues.append("Cycles counter is None")
    elif cycles_per_pps > 500000000:  # Very high limit since it's not resetting
        warnings.append(f"Cycles counter {cycles_per_pps} very high (no reset detected)")

    # Check for expected PPS behavior
    if previous_pps is not None and pps_cnt is not None:
        pps_diff = pps_cnt - previous_pps
        if pps_diff > 1:
            warnings.append(f"PPS jumped by {pps_diff} (expected 1)")
        elif pps_diff < 0:
            issues.append(f"PPS went backwards: {previous_pps} -> {pps_cnt}")

    # Check for cycles reset behavior (currently expected to fail)
    if previous_cycles is not None and cycles_per_pps is not None and previous_pps is not None and pps_cnt is not None:
        if pps_cnt != previous_pps:  # PPS changed
            if cycles_per_pps > previous_cycles:
                warnings.append("Cycles did not reset at PPS change (expected until VHDL fix)")
            else:
                # This would be good news!
                warnings.append("Cycles reset detected! PPS fix may be working!")
        else:  # Same PPS
            if cycles_per_pps <= previous_cycles:
                warnings.append("Cycles not increasing within same second")

    return issues, warnings

def convert_vdif_to_datetime(vdif_seconds):
    """Convert VDIF seconds to human-readable datetime"""
    try:
        if vdif_seconds is None:
            return None
        unix_seconds = VDIF_EPOCH_UNIX + vdif_seconds
        return datetime.utcfromtimestamp(unix_seconds)
    except (ValueError, OSError, OverflowError):
        return None

class VLBIDataLogger:
    def __init__(self, output_dir=VLBI_DATA_DIR):
        self.output_dir = output_dir
        self.ensure_output_dir()
        self.current_file = None
        self.metadata_file = None
        self.next_rotation_time = None
        self.current_filename = None
        self.metadata_filename = None

        # Statistics tracking
        self.pkt_count = 0
        self.valid_timestamp_count = 0
        self.total_size_mb = 0
        self.last_pps_cnt = None
        self.last_cycles = None
        self.pps_changes = 0
        self.cycles_resets_detected = 0  # Track if PPS reset ever works
        self.timestamp_issues = []
        self.timestamp_warnings = []

        self.open_new_file()

    def ensure_output_dir(self):
        """Ensure base output directory exists"""
        if not os.path.exists(self.output_dir):
            os.makedirs(self.output_dir)
            print(f"Created output directory: {self.output_dir}")

    def create_timestamp_folder(self):
        """Create a timestamped folder for the current capture session"""
        folder_timestamp = datetime.utcnow().strftime('%Y%m%d_%H%M%S')
        folder_path = os.path.join(self.output_dir, f"vlbi_capture_{folder_timestamp}")
        if not os.path.exists(folder_path):
            os.makedirs(folder_path)
        return folder_path

    def open_new_file(self):
        """Open new files for data and metadata with rotation"""
        now = datetime.utcnow()

        # Set next rotation time
        self.next_rotation_time = (now + timedelta(minutes=FILE_ROTATION_MINUTES)).replace(
            second=0, microsecond=0)

        # Close current files if open
        if self.current_file is not None:
            self.current_file.close()
            print(f"Closed data file: {self.current_filename}")
            print(f"File statistics: {self.pkt_count} packets, {self.total_size_mb:.2f} MB")
            print(f"Valid timestamps: {self.valid_timestamp_count}/{self.pkt_count}")
            print(f"PPS changes: {self.pps_changes}, Cycles resets: {self.cycles_resets_detected}")

        if self.metadata_file is not None:
            # Write final statistics to metadata
            final_stats = {
                "file_end_time": now.isoformat(),
                "total_packets": self.pkt_count,
                "valid_timestamps": self.valid_timestamp_count,
                "pps_changes": self.pps_changes,
                "cycles_resets_detected": self.cycles_resets_detected,
                "timestamp_issues": self.timestamp_issues[-10:],  # Last 10 issues
                "timestamp_warnings": self.timestamp_warnings[-10:],  # Last 10 warnings
                "final_pps_cnt": self.last_pps_cnt,
                "final_cycles": self.last_cycles,
                "pps_reset_status": "WORKING" if self.cycles_resets_detected > 0 else "NOT_WORKING_YET"
            }
            json.dump(final_stats, self.metadata_file, indent=2)
            self.metadata_file.close()
            print(f"Closed metadata file: {self.metadata_filename}")

        # Reset statistics
        self.pkt_count = 0
        self.valid_timestamp_count = 0
        self.total_size_mb = 0
        self.pps_changes = 0
        self.cycles_resets_detected = 0
        self.timestamp_issues = []
        self.timestamp_warnings = []

        # Create folder with timestamp
        folder_path = self.create_timestamp_folder()

        # Create filenames with timestamp
        file_timestamp = now.strftime('%Y%m%d_%H%M%S')
        data_filename = os.path.join(folder_path, f'vlbi_data_{file_timestamp}.bin')
        metadata_filename = os.path.join(folder_path, f'vlbi_metadata_{file_timestamp}.json')

        # Open new files
        self.current_file = open(data_filename, 'wb')
        self.current_filename = data_filename

        self.metadata_file = open(metadata_filename, 'w')
        self.metadata_filename = metadata_filename

        # Write initial metadata
        initial_metadata = {
            "capture_info": {
                "start_time": now.isoformat(),
                "data_file": os.path.basename(data_filename),
                "packet_size": ACTUAL_PKT_SIZE,
                "header_size": PACKET_HEADER_SIZE,
                "timestamp_layout": {
                    "pps_offset": TIMESTAMP_PPS_OFFSET,
                    "cycles_offset": TIMESTAMP_CYCLES_OFFSET,
                    "format": "Little Endian 32-bit unsigned"
                },
                "design_info": "timestamp_gen_simple (PPS reset debugging phase)",
                "known_issues": {
                    "pps_counter": "WORKING - proper 2025 VDIF epoch",
                    "cycles_counter": "NOT_RESETTING_YET - awaiting VHDL fix",
                    "vlbi_readiness": "PARTIAL - needs cycles reset fix"
                }
            },
            "packet_log": []
        }
        json.dump(initial_metadata, self.metadata_file, indent=2)
        self.metadata_file.flush()

        print(f"Opened new VLBI data file: {data_filename}")
        print(f"Opened new metadata file: {metadata_filename}")
        print(f"Next file rotation at: {self.next_rotation_time}")

        return data_filename

    def check_rotation(self):
        """Check if it's time to rotate to a new file"""
        if datetime.utcnow() >= self.next_rotation_time:
            return self.open_new_file()
        return None

    def write_packet(self, packet):
        """Write packet with timestamp extraction and validation"""
        if len(packet) != ACTUAL_PKT_SIZE:
            return False, f"Wrong packet size: {len(packet)} (expected {ACTUAL_PKT_SIZE})"

        # Check if we need to rotate files
        self.check_rotation()

        # Extract and validate timestamps
        pps_cnt, cycles_per_pps, extract_status = extract_timestamps(packet)
        validation_issues, validation_warnings = validate_timestamps(
            pps_cnt, cycles_per_pps, self.last_pps_cnt, self.last_cycles)

        # Track timestamp statistics
        timestamp_valid = (extract_status == "OK" and len(validation_issues) == 0)
        if timestamp_valid:
            self.valid_timestamp_count += 1

            # Track PPS changes (seconds increments)
            if self.last_pps_cnt is not None and pps_cnt != self.last_pps_cnt:
                self.pps_changes += 1

                # Check for cycles reset (the fix we're waiting for)
                if self.last_cycles is not None and cycles_per_pps < self.last_cycles:
                    self.cycles_resets_detected += 1
                    print(f" CYCLES RESET DETECTED! {self.last_cycles} -> {cycles_per_pps}")

            self.last_pps_cnt = pps_cnt
            self.last_cycles = cycles_per_pps
        else:
            # Log timestamp issues
            issue_str = f"{extract_status}; {'; '.join(validation_issues)}"
            self.timestamp_issues.append({
                "packet": self.pkt_count,
                "time": datetime.utcnow().isoformat(),
                "issue": issue_str
            })

        # Log warnings (expected during debugging phase)
        if validation_warnings:
            warning_str = "; ".join(validation_warnings)
            self.timestamp_warnings.append({
                "packet": self.pkt_count,
                "time": datetime.utcnow().isoformat(),
                "warning": warning_str
            })

        # Write packet data to binary file
        if self.current_file is not None:
            self.current_file.write(packet)
            self.current_file.flush()

            # Update statistics
            self.pkt_count += 1
            self.total_size_mb += len(packet) / (1024 * 1024)

            # Log packet info to metadata (sample every 1000th packet)
            if self.pkt_count % 1000 == 0 and timestamp_valid:
                packet_info = {
                    "packet_num": self.pkt_count,
                    "timestamp": datetime.utcnow().isoformat(),
                    "pps_cnt": int(pps_cnt) if pps_cnt else None,
                    "cycles_per_pps": int(cycles_per_pps) if cycles_per_pps else None,
                    "vdif_datetime": convert_vdif_to_datetime(pps_cnt).isoformat() if pps_cnt else None,
                    "cycles_reset_working": self.cycles_resets_detected > 0
                }

                # Append to metadata file
                self.metadata_file.seek(0, 2)  # Go to end
                self.metadata_file.write(f",\n{json.dumps(packet_info, indent=4)}")
                self.metadata_file.flush()

        return timestamp_valid, extract_status

    def get_statistics(self):
        """Get current capture statistics"""
        return {
            "total_packets": self.pkt_count,
            "valid_timestamps": self.valid_timestamp_count,
            "timestamp_success_rate": self.valid_timestamp_count / max(1, self.pkt_count) * 100,
            "pps_changes": self.pps_changes,
            "cycles_resets_detected": self.cycles_resets_detected,
            "current_pps": self.last_pps_cnt,
            "current_cycles": self.last_cycles,
            "current_vdif_time": convert_vdif_to_datetime(self.last_pps_cnt),
            "recent_issues": len(self.timestamp_issues),
            "recent_warnings": len(self.timestamp_warnings),
            "data_size_mb": self.total_size_mb,
            "pps_reset_status": "WORKING" if self.cycles_resets_detected > 0 else "NOT_WORKING_YET"
        }

def initialize_fpga(hostname, fpgfile, skip_programming=False):
    """Initialize FPGA with timestamp_gen_simple design"""
    global fpga
    print(f'Connecting to {hostname}... ')
    fpga = casperfpga.CasperFpga(hostname)
    time.sleep(0.2)

    if not skip_programming:
        print(f'Programming FPGA with {fpgfile}...')
        fpga.upload_to_ram_and_program(fpgfile)
        print('Programming complete')
        time.sleep(1.0)  # Give timestamp_gen_simple time to settle
    else:
        fpga.get_system_information()
        print('Skipped FPGA programming')

    # Clear packet reset
    print('Clearing packet reset...')
    fpga.write_int('pkt_rst', 0)
    time.sleep(0.1)

    # Enable QSFP port
    print('Enabling QSFP port...')
    fpga.write_int('qsfp_rst', 1)
    time.sleep(0.2)
    print('QSFP enabled')

    # Try to read timestamp_gen_simple registers
    print('Reading timestamp_gen_simple status...')
    try:
        # Look for timestamp registers (try different possible names)
        reg_names = fpga.listdev()
        timestamp_regs = [reg for reg in reg_names if 'timestamp' in reg.lower() or 'pps' in reg.lower()]

        if timestamp_regs:
            print("Found timestamp-related registers:")
            for reg in timestamp_regs[:5]:  # Show first 5
                try:
                    value = fpga.read_uint(reg)
                    print(f"  {reg}: {value}")
                except:
                    pass
        else:
            print("No obvious timestamp registers found")

    except Exception as e:
        print(f"Could not read timestamp registers: {e}")

    return fpga

if __name__ == "__main__":
    # Register cleanup handlers
    atexit.register(cleanup_qsfp)
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    from optparse import OptionParser

    p = OptionParser()
    p.set_usage('vlbi_data_logging_listener_fixed.py <HOSTNAME_or_IP> [options]')
    p.set_description(__doc__)
    p.add_option('-n', '--numpkt', dest='numpkt', type='int', default=256,
        help='Number of packets per sequence (default 256)')
    p.add_option('-s', '--skip', dest='skip', action='store_true',
        help='Skip programming and begin data capture')
    p.add_option('-b', '--fpg', dest='fpgfile', type='str', default='',
        help='Specify the fpg file to load')
    p.add_option('-a', '--adc', dest='adc_chan_sel', type='int', default=0,
        help='ADC input to select (0,1,2,3)')
    p.add_option('-d', '--datadir', dest='datadir', type='str', default=VLBI_DATA_DIR,
        help='Data output directory')

    opts, args = p.parse_args(sys.argv[1:])
    if len(args) < 1:
        print('Specify hostname or IP for your CASPER platform')
        print('Usage: python vlbi_data_logging_listener_fixed.py <hostname> [options]')
        sys.exit()
    hostname = args[0]

    # Determine bitstream (updated for timestamp_gen_simple)
    if opts.fpgfile:
        bitstream = opts.fpgfile
    else:
        # Default to latest timestamp_gen_simple bitstream
        prebuilt = '4x2_100gbe_2bit_bitstream/rfsoc4x2_stream_rfdc_100g_4096_timestamping_2bit_9_2025-06-26_1605.fpg'
        print(f'Using default bitstream: {prebuilt}')
        print('  Make sure this is your timestamp_gen_simple design!')
        bitstream = prebuilt

    # Validate options
    NPKT = opts.numpkt
    if not (1 <= NPKT <= 4096):
        print('Number of packets should be between 1 and 4096')
        sys.exit()

    if not (0 <= opts.adc_chan_sel <= 3):
        print('ADC select must be 0, 1, 2, or 3')
        sys.exit()

    print("VLBI DATA LOGGING SYSTEM (timestamp_gen_simple debugging)")
    print("=" * 60)
    print(f"Target: {hostname}")
    print(f"Bitstream: {bitstream}")
    print(f"ADC Channel: {opts.adc_chan_sel}")
    print(f"Packets per sequence: {NPKT}")
    print(f"Expected packet size: {ACTUAL_PKT_SIZE} bytes")
    print(f"Output directory: {opts.datadir}")
    print()
    print("CURRENT STATUS:")
    print("   PPS counter: WORKING (proper 2025 VDIF epoch)")
    print("   Cycles counter: NOT RESETTING at PPS events yet")
    print("   Waiting for VHDL fix with variable-based edge detection")
    print("   This script will detect when PPS resets start working")

    # Initialize FPGA
    fpga = initialize_fpga(hostname, bitstream, opts.skip)
    print(f'Setting ADC channel to {opts.adc_chan_sel}')
    fpga.write_int('adc_chan_sel', opts.adc_chan_sel)
    time.sleep(0.1)

    # Initialize data logger
    data_logger = VLBIDataLogger(opts.datadir)

    # Set up listener for catcher
    address = ('localhost', 6000)
    listener = Listener(address, authkey=b'secret password')
    print('\nWaiting for catcher to connect...')
    conn = listener.accept()
    print('Connection accepted from', listener.last_accepted)

    # Send parameters to catcher
    print(f"\nSending parameters to catcher:")
    print(f"  Packets per sequence: {NPKT}")
    print(f"  Packet size: {ACTUAL_PKT_SIZE}")
    conn.send([NPKT, ACTUAL_PKT_SIZE, 0])  # Last parameter not used

    try:
        start_time = datetime.utcnow()
        last_status_time = start_time

        print(f"\nStarting VLBI data capture at {start_time.isoformat()}")
        print("Monitoring for PPS reset behavior...")

        while True:
            # Signal catcher we're ready for packets
            conn.send('ready')

            # Receive packet batch
            pkts = []
            for _ in range(NPKT):
                r = conn.recv()
                if r == 'close':
                    print("\nReceived close signal from catcher")
                    stats = data_logger.get_statistics()
                    print(f"Final statistics:")
                    print(f"  Total packets: {stats['total_packets']}")
                    print(f"  Valid timestamps: {stats['valid_timestamps']}")
                    print(f"  Success rate: {stats['timestamp_success_rate']:.1f}%")
                    print(f"  PPS changes: {stats['pps_changes']}")
                    print(f"  Cycles resets detected: {stats['cycles_resets_detected']}")
                    print(f"  PPS reset status: {stats['pps_reset_status']}")
                    listener.close()
                    sys.exit()
                pkts.append(r)

            # Process packets for VLBI storage
            valid_pkts = 0
            timestamp_valid_pkts = 0

            for p in pkts:
                # Basic IP source validation
                if len(p) >= 34:
                    ip_hdr = p[14:34]
                    src_ip = struct.unpack("4B", ip_hdr[-8:-4])
                    src_int = (src_ip[0]<<24)|(src_ip[1]<<16)|(src_ip[2]<<8)|src_ip[3]
                    expected = 10*(2**24) + 17*(2**16) + 16*(2**8) + 60  # 10.17.16.60

                    # Only process packets from expected source
                    if src_int == expected:
                        valid_pkts += 1
                        timestamp_valid, status = data_logger.write_packet(p)
                        if timestamp_valid:
                            timestamp_valid_pkts += 1

            # Print status update every 10 seconds
            now = datetime.utcnow()
            if (now - last_status_time).total_seconds() >= 10:
                stats = data_logger.get_statistics()
                elapsed = (now - start_time).total_seconds()
                rate = stats['total_packets'] / elapsed if elapsed > 0 else 0

                print(f"\nVLBI Capture Status:")
                print(f"  Time: {now.strftime('%H:%M:%S')} UTC")
                print(f"  Packets: {stats['total_packets']} ({rate:.1f}/s)")
                print(f"  Valid timestamps: {stats['valid_timestamps']} ({stats['timestamp_success_rate']:.1f}%)")
                print(f"  PPS changes: {stats['pps_changes']}")
                print(f"  🔧 Cycles resets detected: {stats['cycles_resets_detected']} (waiting for VHDL fix)")
                print(f"  Data size: {stats['data_size_mb']:.1f} MB")

                if stats['current_vdif_time']:
                    print(f"  Current VDIF time: {stats['current_vdif_time'].strftime('%Y-%m-%d %H:%M:%S')} UTC")

                # Highlight PPS reset status
                if stats['cycles_resets_detected'] > 0:
                    print(f"   PPS RESET STATUS: WORKING! ({stats['cycles_resets_detected']} resets detected)")
                else:
                    print(f"   PPS RESET STATUS: {stats['pps_reset_status']} (expected until VHDL fix)")

                if stats['recent_warnings'] > 0:
                    print(f"  Recent warnings: {stats['recent_warnings']} (expected during debugging)")

                last_status_time = now

    except KeyboardInterrupt:
        print("\nKeyboard interrupt received")
        stats = data_logger.get_statistics()
        print(f"Final capture statistics:")
        print(f"  Total packets: {stats['total_packets']}")
        print(f"  Valid timestamps: {stats['valid_timestamps']} ({stats['timestamp_success_rate']:.1f}%)")
        print(f"  PPS changes: {stats['pps_changes']}")
        print(f"  Cycles resets detected: {stats['cycles_resets_detected']}")
        print(f"  PPS reset status: {stats['pps_reset_status']}")
        print(f"  Data captured: {stats['data_size_mb']:.1f} MB")
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        try:
            conn.send('close')
        except:
            pass
        listener.close()
        cleanup_qsfp()
        print("VLBI data logging session ended")