add_library(nng_cli STATIC
    cli_client.cpp
)

target_include_directories(nng_cli PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(nng_cli PUBLIC
    nng_control_node
)
