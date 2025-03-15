# BCP Saggitarius

BCP (Basic Control Program) Saggitarius is a data acquisition and control system designed for radio astronomy applications. It integrates an RFSoC spectrometer and GPS logging capabilities for precise timing and location data collection.

## Features

- RFSoC-based spectrometer control and data acquisition
- GPS logging with automatic file rotation
- Command-line interface for system control
- Configurable parameters via config file
- Automatic data saving with customizable intervals
- Multi-threaded architecture for concurrent operations

## System Requirements

- Linux-based operating system
- GCC compiler
- libconfig library
- Python 3.x
- CASPER FPGA tools (casperfpga)
- NumPy
- Appropriate permissions for GPS device access

## Directory Structure

```
.
├── bcp_Sag.config     # Configuration file
├── cli_Sag.c          # Command-line interface implementation
├── cli_Sag.h          # CLI header file
├── file_io_Sag.c      # File I/O operations
├── file_io_Sag.h      # File I/O header file
├── gps.c              # GPS functionality implementation
├── gps.h              # GPS header file
├── main_Sag.c         # Main program entry point
├── rfsoc_spec.py      # RFSoC spectrometer control script
└── run_bcp_Sag.sh     # Build and run script
```

## Installation

1. Clone the repository:
```bash
git clone https://github.com/fissellab/bcp
```

2. Install dependencies:
```bash
sudo apt-get install gcc libconfig-dev python3-pip
pip3 install numpy casperfpga
```

[Install Cmake >3.24](https://apt.kitware.com/)

[Install vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-bash)

3. Set up device permissions:
```bash
sudo chmod 666 /dev/ttyGPS
```

## Configuration

Edit `bcp_Sag.config` to set your system parameters:

- RFSoC spectrometer settings (IP address, mode, data paths)
- GPS settings (port, baud rate, save intervals)
- Log file locations
- Data save paths

## Building and Running

Use the provided shell script to build and run the program:

```bash
cd bcp/Sag
chmod +x start.sh
./start.sh
```

## Command Line Interface

The system provides the following commands:

- `start spec` - Start the spectrometer
- `stop spec` - Stop the spectrometer
- `start gps` - Start GPS logging
- `stop gps` - Stop GPS logging
- `print <message>` - Print a message to console
- `exit` - Exit the program

## Data Output

### Spectrometer Data
- Spectrum data files: `YYYY-MM-DD_HH-MM-SS_spectrum_data.txt`
- Integrated power data: `YYYY-MM-DD_HH-MM-SS_integrated_power_data.txt`

### GPS Data
- Binary GPS data files: `YYYYMMDD_HHMMSS_GPS_data/gps_log_YYYYMMDD_HHMMSS.bin`

## Logging

The system maintains two types of logs:
- Main system log: `/home/saggitarius/flight_code_dev/log/main_sag.log`
- Command log: `/home/saggitarius/flight_code_dev/log/cmds_sag.log`

## Development

### Adding New Features

1. Modify the relevant source files
2. Update the configuration file if needed
3. Update the CLI commands in `cli_Sag.c` if required


## License

This project is licensed under the MIT License - see the LICENSE file for details.


## Acknowledgments

- CASPER (Collaboration for Astronomy Signal Processing and Electronics Research)

## Support

For support, please open an issue in the GitHub repository or contact the maintainers directly.
