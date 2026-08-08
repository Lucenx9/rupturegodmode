cmake_minimum_required(VERSION 3.20)

project(RuptureGodModeTests LANGUAGES CXX)

enable_testing()

add_library(god_mode_controller
    plugin/god_mode_controller.cpp
)
target_include_directories(god_mode_controller PUBLIC plugin)
target_compile_features(god_mode_controller PUBLIC cxx_std_20)

add_executable(god_mode_controller_tests
    tests/test_god_mode_controller.cpp
)
target_link_libraries(god_mode_controller_tests PRIVATE god_mode_controller)

add_test(NAME god_mode_controller_tests COMMAND god_mode_controller_tests)
