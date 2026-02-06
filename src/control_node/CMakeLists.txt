add_library(nng_control_node STATIC
    tcp_framer.cpp
    command_handler.cpp
    control_node.cpp
)

target_include_directories(nng_control_node PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(nng_control_node PUBLIC
    nng_common
    nng_gateway_core
)
