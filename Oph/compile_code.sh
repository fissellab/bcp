#!/bin/sh

# Create build directory if it doesn't exist
if [ ! -d "./build" ]; then
    echo "Creating build directory..."
    mkdir -p build
fi

# Configure the project if CMakeCache.txt doesn't exist
if [ ! -f "./build/CMakeCache.txt" ]; then
    echo "Configuring project with CMake..."
    cd build
    cmake ..
    cd ..
fi

# Build the project
echo "Building project..."
cmake --build ./build

if [ $? -eq 0 ]; then
    echo "Compilation successful. Executable 'build/main' created."
else
    echo "Compilation failed. Please check the error messages above."
fi