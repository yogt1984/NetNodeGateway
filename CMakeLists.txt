cmake_minimum_required(VERSION 3.16)
project(NetNodeGateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# GoogleTest via FetchContent
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Sub-projects
add_subdirectory(src/common)
add_subdirectory(src/gateway)
add_subdirectory(src/sensor_sim)
add_subdirectory(src/control_node)
add_subdirectory(src/cli)
add_subdirectory(src/replay)

# Tests
add_executable(test_crc32 tests/test_crc32.cpp)
target_link_libraries(test_crc32 PRIVATE nng_common gtest_main)
add_test(NAME test_crc32 COMMAND test_crc32)

add_executable(test_telemetry_parser tests/test_telemetry_parser.cpp)
target_link_libraries(test_telemetry_parser PRIVATE nng_gateway_core gtest_main)
add_test(NAME test_telemetry_parser COMMAND test_telemetry_parser)

add_executable(test_sequence_tracker tests/test_sequence_tracker.cpp)
target_link_libraries(test_sequence_tracker PRIVATE nng_gateway_core gtest_main)
add_test(NAME test_sequence_tracker COMMAND test_sequence_tracker)

add_executable(test_logger tests/test_logger.cpp)
target_link_libraries(test_logger PRIVATE nng_common gtest_main)
add_test(NAME test_logger COMMAND test_logger)

add_executable(test_event_bus tests/test_event_bus.cpp)
target_link_libraries(test_event_bus PRIVATE nng_common gtest_main)
add_test(NAME test_event_bus COMMAND test_event_bus)

add_executable(test_stats_manager tests/test_stats_manager.cpp)
target_link_libraries(test_stats_manager PRIVATE nng_gateway_core gtest_main)
add_test(NAME test_stats_manager COMMAND test_stats_manager)

add_executable(test_object_generator tests/test_object_generator.cpp)
target_link_libraries(test_object_generator PRIVATE nng_sensor_sim gtest_main)
add_test(NAME test_object_generator COMMAND test_object_generator)

add_executable(test_world_model tests/test_world_model.cpp)
target_link_libraries(test_world_model PRIVATE nng_sensor_sim gtest_main)
add_test(NAME test_world_model COMMAND test_world_model)

add_executable(test_measurement_generator tests/test_measurement_generator.cpp)
target_link_libraries(test_measurement_generator PRIVATE nng_sensor_sim nng_gateway_core gtest_main)
add_test(NAME test_measurement_generator COMMAND test_measurement_generator)

add_executable(test_fault_injector tests/test_fault_injector.cpp)
target_link_libraries(test_fault_injector PRIVATE nng_sensor_sim gtest_main)
add_test(NAME test_fault_injector COMMAND test_fault_injector)

add_executable(test_scenario_loader tests/test_scenario_loader.cpp)
target_link_libraries(test_scenario_loader PRIVATE nng_sensor_sim gtest_main)
add_test(NAME test_scenario_loader COMMAND test_scenario_loader)

add_executable(test_pipeline_integration tests/test_pipeline_integration.cpp)
target_link_libraries(test_pipeline_integration PRIVATE nng_sensor_sim nng_gateway_core gtest_main)
add_test(NAME test_pipeline_integration COMMAND test_pipeline_integration)

add_executable(test_tcp_framer tests/test_tcp_framer.cpp)
target_link_libraries(test_tcp_framer PRIVATE nng_control_node gtest_main)
add_test(NAME test_tcp_framer COMMAND test_tcp_framer)

add_executable(test_command_handler tests/test_command_handler.cpp)
target_link_libraries(test_command_handler PRIVATE nng_control_node gtest_main)
add_test(NAME test_command_handler COMMAND test_command_handler)

add_executable(test_udp_loopback tests/test_udp_loopback.cpp)
target_link_libraries(test_udp_loopback PRIVATE nng_gateway_core gtest_main)
add_test(NAME test_udp_loopback COMMAND test_udp_loopback)

add_executable(test_tcp_loopback tests/test_tcp_loopback.cpp)
target_link_libraries(test_tcp_loopback PRIVATE nng_cli nng_control_node gtest_main)
add_test(NAME test_tcp_loopback COMMAND test_tcp_loopback)

add_executable(test_frame_recorder tests/test_frame_recorder.cpp)
target_link_libraries(test_frame_recorder PRIVATE nng_gateway_core gtest_main)
add_test(NAME test_frame_recorder COMMAND test_frame_recorder)

add_executable(test_replay_engine tests/test_replay_engine.cpp)
target_link_libraries(test_replay_engine PRIVATE nng_replay gtest_main)
add_test(NAME test_replay_engine COMMAND test_replay_engine)

add_executable(test_replay_determinism tests/test_replay_determinism.cpp)
target_link_libraries(test_replay_determinism PRIVATE nng_replay nng_sensor_sim nng_gateway_core gtest_main)
add_test(NAME test_replay_determinism COMMAND test_replay_determinism)

add_executable(test_full_system tests/test_full_system.cpp)
target_link_libraries(test_full_system PRIVATE nng_gateway_core nng_replay nng_sensor_sim nng_control_node nng_cli gtest_main)
add_test(NAME test_full_system COMMAND test_full_system)

add_executable(test_full_system_faults tests/test_full_system_faults.cpp)
target_link_libraries(test_full_system_faults PRIVATE nng_gateway_core nng_replay nng_sensor_sim gtest_main)
add_test(NAME test_full_system_faults COMMAND test_full_system_faults)

add_executable(test_log_format tests/test_log_format.cpp)
target_link_libraries(test_log_format PRIVATE nng_common gtest_main)
add_test(NAME test_log_format COMMAND test_log_format)

add_executable(test_e2e_replay tests/test_e2e_replay.cpp)
target_link_libraries(test_e2e_replay PRIVATE nng_gateway_core nng_replay nng_sensor_sim gtest_main)
add_test(NAME test_e2e_replay COMMAND test_e2e_replay)
