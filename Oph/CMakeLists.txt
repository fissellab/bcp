cmake_minimum_required(VERSION 3.10)

# Set the project name
project(bcp_Oph LANGUAGES C)

# Add the source files
file(GLOB SOURCES
    "*.c"
    "*.h")

# Include directories
include_directories(
    /usr/include/glib-2.0
    /usr/lib/x86_64-linux-gnu/glib-2.0/include
    
)

# Add the executable
add_executable(bcp_Oph ${SOURCES})

# Link the libraries
target_link_libraries(bcp_Oph PRIVATE
    pthread
    config
    curses
    ueye_api
    m
    astrometry
    glib-2.0
    ./pkgs/libsofa/build/src/libsofa.a
    ./pkgs/libsoem/build/libsoem.a
)
