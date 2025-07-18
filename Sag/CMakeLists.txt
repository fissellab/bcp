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
        file_io_Sag.c
        main_Sag.c
        cli_Sag.c
        gps.c
        spectrometer_server.c
        telemetry_server.c
        pbob_client.c
        ticc_client.c
        vlbi_client.c
        rfsoc_client.c
        pr59_interface.c
)

target_include_directories(main PRIVATE 
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
)

# Add PR59 TEC controller executable
add_executable(
    tec_control_3
        PR59/tec_control_3.c
        pr59_interface.c
)

target_link_libraries(tec_control_3
    config
    m
    pthread
)
