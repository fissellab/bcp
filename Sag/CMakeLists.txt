cmake_minimum_required(VERSION 3.24)

project(bcp_sag VERSION 0.1 LANGUAGES C)

include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED json-c)

add_subdirectory(
    ../bvex-link/bcp-fetch-client
    ../bvex-link/bcp-fetch-client/build
)
# we now have a target called bcp-fetch

add_executable(
    main
        src/file_io_Sag.c
        src/main_Sag.c
        src/cli_Sag.c
        src/gps.c
        src/spectrometer_server.c
        src/telemetry_server.c
        src/pbob_client.c
        src/ticc_client.c
        src/vlbi_client.c
        src/rfsoc_client.c
        src/pr59_interface.c
        src/heaters.c
        src/position_sensors.c
)

target_include_directories(main PRIVATE 
    include
    .
    ${JSON_C_INCLUDE_DIRS}
    ../bvex-link/bcp-fetch-client/include
    # /usr/include/glib-2.0
    # /usr/lib/x86_64-linux-gnu/glib-2.0/include
)


target_link_libraries(main
    bcp-fetch
    pthread
    config
    m
    ${JSON_C_LIBRARIES}
    LabJackM
)

# Add PR59 TEC controller executable
add_executable(
    tec_control_3
        PR59/tec_control_3.c
        src/pr59_interface.c
)

target_include_directories(tec_control_3 PRIVATE 
    include
)

target_link_libraries(tec_control_3
    config
    m
    pthread
)

# Add position sensor receiver executable
add_executable(
    pos_sensors_rx
        src/pos_sensor_rx.c
)

target_include_directories(pos_sensors_rx PRIVATE 
    include
)

target_link_libraries(pos_sensors_rx
    m
    pthread
)
