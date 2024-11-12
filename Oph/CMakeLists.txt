cmake_minimum_required(VERSION 3.10)

# Set the project name
project(bcp_Oph)

# Add the source files
set(SOURCES
    main_Oph.c
    file_io_Oph.c
    cli_Oph.c
    bvexcam.c
    camera.c
    lens_adapter.c
    matrix.c
    astrometry.c
    accelerometer.c
    ec_motor.c
    motor_control.c
)

# Include directories
include_directories(
    /usr/include/glib-2.0
    /usr/lib/x86_64-linux-gnu/glib-2.0/include
)

# Add the executable
add_executable(bcp_Oph ${SOURCES})

# Link the libraries
target_link_libraries(bcp_Oph
    pthread
    config
    curses
    ueye_api
    m
    astrometry
    glib-2.0
    /usr/local/lib/libsofa_c.a
    /usr/local/lib/libsoem.a
)