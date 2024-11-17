cmake_minimum_required(VERSION 3.24)

project(bcp_oph VERSION 0.1 LANGUAGES C)

add_subdirectory(
    ../bvex-link/telemetry-uplink
    ../bvex-link/telemetry-uplink/build
)

add_executable(main test.c)

target_link_libraries(main
    telemetry-uplink
)


# Add the source files
# file(GLOB SOURCES
#     "*.c"
#     "*.h")

# sometimes I have to uncomment this line to get cmake to find
# vcpkg installed packages. Not sure why.
# include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

# message(STATUS "Found telemetry-uplink: ${TELEMETRY_UPLINK_LIB}")

# Add the executable
# add_executable(bcp_Oph ${SOURCES})

# target_include_directories(bcp_Oph PRIVATE
#     /usr/include/glib-2.0
#     /usr/lib/x86_64-linux-gnu/glib-2.0/include
#     # /home/ophiuchus/bcp-oliver/bvex-link/telemetry-uplink/include
# )

# Link the libraries
# target_link_libraries(bcp_Oph
#     pthread
#     config
#     curses
#     ueye_api
#     m
#     astrometry
#     glib-2.0
#     /usr/local/lib/libsofa_c.a
#     /usr/local/lib/libsoem.a
#     telemetry-uplink
# )
