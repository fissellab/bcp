#!/bin/bash

# Path definitions
LMK_EXT="/lib/firmware/rfsoc4x2_lmk_CLKin0_extref_10M_PL_128M_LMXREF_256M.txt"
LMX_EXT="/lib/firmware/rfsoc4x2_lmx_inputref_256M_outputref_512M.txt"
PRG_RFPLL="/home/casper/bin/prg_rfpll"

echo "Attempting to configure external 10MHz reference..."

echo "Programming LMK..."
$PRG_RFPLL -lmk $LMK_EXT
sleep 2

echo "Programming LMX..."
$PRG_RFPLL -lmx $LMX_EXT
sleep 5

echo "Clock configuration complete. Please verify PLL lock LEDs"
