#!/bin/bash

# run_bcp_Sag.sh

# Set the compiler
CC=gcc

# Set compiler flags
CFLAGS="-Wall -Wextra -g"

# Set libraries to link
LIBS="-lconfig -lpthread"

# Set the name of the output executable
OUTPUT="bcp_Sag"

# Create necessary directories
sudo mkdir -p /home/saggitarius/flight_code_dev/log
sudo mkdir -p /media/saggitarius/T7/GPS_data

# Set permissions for GPS device
sudo chmod 666 /dev/ttyGPS

# Compile the source files
echo "Compiling source files..."
$CC $CFLAGS -c main_Sag.c -o main_Sag.o
$CC $CFLAGS -c file_io_Sag.c -o file_io_Sag.o
$CC $CFLAGS -c cli_Sag.c -o cli_Sag.o
$CC $CFLAGS -c gps.c -o gps.o

# Link the object files
echo "Linking object files..."
$CC $CFLAGS main_Sag.o file_io_Sag.o cli_Sag.o gps.o -o $OUTPUT $LIBS

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful."
    
    # Run the executable with sudo
    echo "Running BCP Saggitarius..."
    sudo ./$OUTPUT bcp_Sag.config
else
    echo "Compilation failed."
fi
