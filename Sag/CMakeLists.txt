cmake_minimum_required(VERSION 3.24)

project(bcp_sag VERSION 0.1 LANGUAGES C)

include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

add_subdirectory(
    ../bvex-link/onboard-telemetry-client
    ../bvex-link/onboard-telemetry-client/build
)
# we now have a target called telemetry-client

add_executable(
    main
        file_io_Sag.c
        main_Sag.c
        cli_Sag.c
        gps.c
)

target_include_directories(main PRIVATE 
    .
    # /usr/include/glib-2.0
    # /usr/lib/x86_64-linux-gnu/glib-2.0/include
)


target_link_libraries(main
    telemetry-client
    pthread
    config
)
