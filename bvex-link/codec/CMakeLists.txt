cmake_minimum_required(VERSION 3.24)

project(bvex-codec VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(cJSON CONFIG REQUIRED)

add_library(bvex_codec
    src/src_c/sample.c
    src/src_c/primitive.c
)

target_link_libraries(bvex_codec PRIVATE cjson)
target_include_directories(bvex_codec
    PRIVATE src/src_c
    PUBLIC src/include_c
)

# build test executable
add_executable(bvex_codec_test test_c_lib.c)
target_link_libraries(bvex_codec_test PRIVATE bvex_codec)
