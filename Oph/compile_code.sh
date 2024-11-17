#!/bin/sh

gcc -g src/main_Oph.c src/file_io_Oph.c src/cli_Oph.c src/bvexcam.c src/camera.c src/lens_adapter.c src/matrix.c src/astrometry.c src/accelerometer.c src/ec_motor.c src/motor_control.c\
    -lpthread -lconfig -lcurses -lueye_api -lm -lastrometry -lglib-2.0 -o bcp_Oph /usr/local/lib/libsofa_c.a /usr/local/lib/libsoem.a\
    -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include

if [ $? -eq 0 ]; then
    echo "Compilation successful. Executable 'bcp_Oph' created."
else
    echo "Compilation failed. Please check the error messages above."
fi
