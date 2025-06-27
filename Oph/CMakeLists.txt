cmake_minimum_required(VERSION 3.24)

project(bcp_oph VERSION 0.1 LANGUAGES C)

# Enable vcpkg integration
#find_package(nanopb CONFIG REQUIRED)

# sometimes I have to uncomment this line to get cmake to find
# vcpkg installed packages. Not sure why.
# include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

#add_subdirectory(
#    ../bvex-link/bcp-fetch-client
#    ../bvex-link/bcp-fetch-client/build
#)
# we now have a target called bcp-fetch

# gather all source files from src/ into variable _srcFiles
aux_source_directory(src _srcFiles)

# exclude these files
list(REMOVE_ITEM _srcFiles
    "src/accl_rx.c"
    "src/accl_tx.c"
)

message(STATUS "Source files: ${_srcFiles}")

add_executable(main ${_srcFiles})

target_include_directories(main PRIVATE 
    include 
    /usr/include/glib-2.0
    /usr/lib/x86_64-linux-gnu/glib-2.0/include
)


target_link_libraries(main
    pthread
    config
    curses
    ueye_api  # requires omp5
    m
    astrometry
    LabJackM
    glib-2.0
    jpeg
    /usr/local/lib/libsofa_c.a
    /usr/local/lib/libsoem.a
)
