#!/bin/bash

# Instructions from https://github.com/OpenEtherCATsociety/SOEM/blob/master/README.md

# Check if the first argument is provided
if [ -z "$1" ]; then
  echo "Error: No packages directory provided."
  echo "Usage: $0 <packages directory>"
  exit 1
fi

# Change to the provided directory
cd "$1" || { echo "Error: Could not change to directory $1"; exit 1; }


sudo apt-get install -y libnetcdf-c++4-dev libjson-c-dev doxygen graphviz

if [ -d "libsoem" ]; then
  echo "The libsoem directory already exists. Skipping the download."
else
  git clone https://github.com/OpenEtherCATsociety/SOEM libsoem
fi

cd libsoem

mkdir -p build && cd build

cmake ..

make