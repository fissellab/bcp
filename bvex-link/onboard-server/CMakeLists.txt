cmake_minimum_required(VERSION 3.26)

# set the project name and version
project(onboard-link VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(DEBUG "Use verbose debug logging" ON)

if(DEBUG)
    add_definitions(-DDEBUG)
endif()


find_package(boost_asio CONFIG REQUIRED)
find_package(nanopb CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# CODEC LIB
add_library(codec_lib INTERFACE)
target_include_directories(codec_lib INTERFACE src/codec)

add_library(codec ALIAS codec_lib)


# REQUEST / RESPONSE LIB
add_library(requests_lib
    src/codec/requests/request.cpp
    src/codec/requests/response.cpp
    src/codec/requests/pb_generated/primitive.pb.c
    src/codec/requests/pb_generated/request.pb.c
    src/codec/requests/pb_generated/response.pb.c
)
target_include_directories(requests_lib 
    PRIVATE src/codec
    PUBLIC
        src/codec/requests
        src/codec/requests/pb_generated
)
target_link_libraries(requests_lib
    PRIVATE nanopb::protobuf-nanopb-static
)



# ONBOARD TELEMETRY DECODING LIB
add_library(onboard_telemetry_lib
    src/codec/onboard-tm/decode.cpp
    src/codec/onboard-tm/pb_generated/primitive.pb.c
    src/codec/onboard-tm/pb_generated/sample.pb.c
)
target_include_directories(onboard_telemetry_lib 
    PRIVATE
        src/codec/onboard-tm/pb_generated
        src/codec
    PUBLIC src/codec/onboard-tm
)
target_link_libraries(onboard_telemetry_lib
    PRIVATE nanopb::protobuf-nanopb-static
)


# DOWNLINK ENCODING LIB
add_library(downlink_lib
    src/codec/downlink-tm-enc/file.cpp
    src/codec/downlink-tm-enc/primitive.cpp
    src/codec/downlink-tm-enc/sample_frame.cpp
)
target_include_directories(downlink_lib 
    PRIVATE src/codec
    PUBLIC src/codec/downlink-tm-enc
)
target_link_libraries(downlink_lib
    PRIVATE nlohmann_json::nlohmann_json
)



# ALIASES
add_library(codec::requests ALIAS requests_lib)
add_library(codec::downlink-tm-enc ALIAS onboard_telemetry_lib)
add_library(codec::onboard-tm ALIAS downlink_lib)


set(SOURCES
    src/main.cpp
    src/command.cpp
    src/sample.cpp
    src/server/recv_server.cpp
    src/server/request_server.cpp
    src/server/onboard_telemetry_recv_server.cpp
    src/server/telecommand_recv_server.cpp
    src/server/send_server.cpp
    src/telemetry.cpp
    src/utils/chunker.cpp
    src/utils/sample_transmitter.cpp
)

add_executable(main ${SOURCES} ${HEADERS})

target_include_directories(main PRIVATE
    src
    src/server
    src/utils
)

target_link_libraries(main PRIVATE 
    Boost::asio
    nlohmann_json::nlohmann_json
    codec::requests
    codec::onboard-tm
    codec::downlink-tm-enc
    codec

    # transitive dep from codec::requests and
    # codec::downlink-tm-enc
    nanopb::protobuf-nanopb-static
)

set(REQUEST_SERVER_PORT "\"8080\"")
target_compile_definitions(
    main PRIVATE 
        REQUEST_SERVER_PORT=${REQUEST_SERVER_PORT}
)

option(DEBUG_SAMPLE_RECEPTION "Log sample reception pipeline" OFF)
if(DEBUG_SAMPLE_RECEPTION)
    target_compile_definitions(main PRIVATE DEBUG_ADD_SAMPLE DEBUG_ONBOARD_RECV_SERVER)
endif()

option(DEBUG_REQUEST_RECEPTION "Log sample reception pipeline" OFF)
if(DEBUG_REQUEST_RECEPTION)
    target_compile_definitions(
        main
        PRIVATE
            DEBUG_RECV_REQUEST
            DEBUG_GET_LATEST_SAMPLE_RESPONSE
    )
endif()