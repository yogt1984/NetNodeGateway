add_library(nng_sensor_sim STATIC
    object_generator.cpp
    world_model.cpp
    measurement_generator.cpp
    fault_injector.cpp
    scenario_loader.cpp
)
target_link_libraries(nng_sensor_sim PUBLIC nng_common)
target_include_directories(nng_sensor_sim PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Sensor simulator CLI executable
add_executable(sensor_sim sensor_sim_main.cpp)
target_link_libraries(sensor_sim PRIVATE nng_sensor_sim nng_gateway_core)
