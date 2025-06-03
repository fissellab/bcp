#!/bin/sh

gcc -g main_Oph.c file_io_Oph.c cli_Oph.c bvexcam.c camera.c lens_adapter.c matrix.c astrometry.c accelerometer.c ec_motor.c motor_control.c lazisusan.c arduino.c coords.c lockpin.c\
    -lpthread -lconfig -lcurses -lueye_api -lm -lastrometry -lglib-2.0 -o bcp_Oph /usr/local/lib/libsofa_c.a /usr/local/lib/libsoem.a\
    -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include

if [ $? -eq 0 ]; then
    echo "Compilation successful. Executable 'bcp_Oph' created."
else
    echo "Compilation failed. Please check the error messages above."
fi
