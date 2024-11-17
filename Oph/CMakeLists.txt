cmake_minimum_required(VERSION 3.24)

project(bcp_oph VERSION 0.1 LANGUAGES C)

# sometimes I have to uncomment this line to get cmake to find
# vcpkg installed packages. Not sure why.
# include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

add_subdirectory(
    ../bvex-link/telemetry-uplink
    ../bvex-link/telemetry-uplink/build
)
# we now have a target called telemetry-uplink

find_package(OpenMP REQUIRED)

# Gather all source filepaths into variable SOURCES
file(GLOB SOURCES
    "src/*.c"
    "src/*.h"
)

add_executable(main ${SOURCES})

target_include_directories(main PRIVATE
    /usr/include/glib-2.0
    /usr/lib/x86_64-linux-gnu/glib-2.0/include
)

target_link_libraries(main
    telemetry-uplink
    pthread
    config
    curses
    ueye_api  # requires omp5
    m
    astrometry
    glib-2.0
    /usr/local/lib/libsofa_c.a
    /usr/local/lib/libsoem.a
)
