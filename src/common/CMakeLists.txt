add_library(nng_common STATIC
    crc32.cpp
    logger.cpp
    event_bus.cpp
)
target_include_directories(nng_common PUBLIC ${CMAKE_SOURCE_DIR}/src)
