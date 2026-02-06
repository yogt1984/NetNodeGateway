add_library(nng_gateway_core STATIC
    telemetry_parser.cpp
    sequence_tracker.cpp
    stats_manager.cpp
    udp_socket.cpp
    frame_recorder.cpp
    gateway.cpp
)
target_link_libraries(nng_gateway_core PUBLIC nng_common)
target_include_directories(nng_gateway_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Gateway CLI executable
add_executable(gateway gateway_main.cpp)
target_link_libraries(gateway PRIVATE nng_gateway_core nng_replay)
