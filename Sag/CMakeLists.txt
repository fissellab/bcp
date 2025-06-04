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
