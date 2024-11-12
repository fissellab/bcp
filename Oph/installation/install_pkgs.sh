#!/bin/bash

echo "$(pwd)"

current_dir=$(pwd)

if [[ "$current_dir" != */bcp/Oph ]]; then
  echo "Error: The script must be run from the root of the Oph directory."
  exit 1
fi

mkdir -p pkgs
packages_dir=$(pwd)/pkgs

./installation/install_libsofa.sh $packages_dir

./installation/install_soem.sh $packages_dir