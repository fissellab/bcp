cmake_minimum_required(VERSION 3.24)

project(bcp-redis-client VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(cjson REQUIRED)

add_subdirectory(
    ../../codec/c
    ../../codec/c/build
)
# we now have target bvex_codec

add_library(bcp-redis-client
    src/send_sample.c
)

target_link_libraries(bcp-redis-client PRIVATE cjson::cjson bvex_codec)
target_include_directories(bcp-redis-client
    PUBLIC include
)

# ------------- START UNIT TESTS -------------
option(BCP_REDIS_CLIENT_TESTS "Build test executable with Google Test" OFF)

if(BCP_REDIS_CLIENT_TESTS)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED True)
    find_package(GTest REQUIRED)
    add_executable(gtest
        tests/gtest/codec.cpp
        tests/gtest/codec_c.cpp
    )
    set_target_properties(gtest PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(gtest PRIVATE 
        GTest::gtest
        GTest::gtest_main
        bcp-redis-client
        nlohmann_json::nlohmann_json
        # GTest::gmock
        # GTest::gmock_main
    )
    target_compile_definitions(gtest PRIVATE BCP_FETCH_BOUNDS_CHECKING)

    include(GoogleTest)
    gtest_discover_tests(gtest)
endif()

# ------------- END UNIT TESTS -------------