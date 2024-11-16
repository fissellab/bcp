#!/bin/sh

gcc -g main_Oph.c file_io_Oph.c cli_Oph.c bvexcam.c camera.c lens_adapter.c matrix.c astrometry.c accelerometer.c ec_motor.c motor_control.c\
    -lpthread -lconfig -lcurses -lueye_api -lm -lastrometry -lglib-2.0 -o bcp_Oph /usr/local/lib/libsofa_c.a /usr/local/lib/libsoem.a\
    -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
    -L..bvex-link/telemetry-uplink/build/vcpkg_installed/x64-linux/lib \
    -I..bvex-link/telemetry-uplink/build/vcpkg_installed/x64-linux/include/nanopb \
    -llibprotobuf-nanopb \
    -I../bvex-link/telemetry-uplink/include -L../bvex-link/telemetry-uplink/build \
    -ltelemetry-uplink

if [ $? -eq 0 ]; then
    echo "Compilation successful. Executable 'bcp_Oph' created."
else
    echo "Compilation failed. Please check the error messages above."
fi
