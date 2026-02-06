add_library(nng_replay STATIC
    replay_engine.cpp
)

target_include_directories(nng_replay PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(nng_replay PUBLIC
    nng_gateway_core
)

# Replay CLI executable
add_executable(replay replay_main.cpp)
target_link_libraries(replay PRIVATE nng_replay nng_gateway_core)
