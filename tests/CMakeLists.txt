# CMakeLists.txt - Build configuration for tests

cmake_minimum_required(VERSION 3.20)
project(SerialEchoTests C)

set(CMAKE_C_STANDARD 11)

add_executable(test_echo test_echo.c)
target_compile_options(test_echo PRIVATE /utf-8)
target_compile_definitions(test_echo PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)

enable_testing()
add_test(NAME test_echo COMMAND test_echo)
