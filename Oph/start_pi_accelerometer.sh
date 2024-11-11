#!/bin/bash

PI_USER="bvex"
PI_HOST="192.168.0.23"
PI_C_FILE="/home/bvex/accl_c/accl_tx.c"
PI_EXECUTABLE="/home/bvex/accl_c/accl_tx"

# SSH options for key-based authentication
SSH_OPTIONS="-i /home/ophiuchus/.ssh/id_rsa -o BatchMode=yes -o StrictHostKeyChecking=no"

# Compile the C program on Raspberry Pi
ssh $SSH_OPTIONS ${PI_USER}@${PI_HOST} "gcc -o ${PI_EXECUTABLE} ${PI_C_FILE} -lm"

# Check if compilation was successful
if [ $? -ne 0 ]; then
    echo "Compilation on Raspberry Pi failed. Exiting."
    exit 1
fi

# Start the C program on Raspberry Pi
ssh $SSH_OPTIONS ${PI_USER}@${PI_HOST} "sudo ${PI_EXECUTABLE}" &

echo "Started accelerometer program on Raspberry Pi"
