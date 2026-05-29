# CMakeLists.txt - Build configuration for SerialEcho
#
# Requires: CMake 3.20+, MSVC compiler
# Build (Release): mkdir build && cd build && cmake .. -G "NMake Makefiles" && cmake --build .
# Build (Debug):   mkdir build && cd build && cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug && cmake --build .
# Build (Trace):   mkdir build && cd build && cmake .. -G "NMake Makefiles" -DENABLE_TRACE=ON && cmake --build .
#
# Note: Uses custom manifest (res1.xml) embedded via resource.rc
# for Common Controls v6 and DPI awareness support.

cmake_minimum_required(VERSION 3.20)
project(SerialEcho C)

set(CMAKE_C_STANDARD 11)

# Source files
add_executable(SerialEcho WIN32
    main.c
    gui.c
    serial.c
    config.c
    trace.c
    resource.rc
)

# Disable automatic manifest generation (using custom manifest in resource.rc)
set_target_properties(SerialEcho PROPERTIES
    LINK_FLAGS "/MANIFEST:NO"
)

# Link libraries
target_link_libraries(SerialEcho PRIVATE
    comctl32
    uuid
)

# Compiler options
target_compile_options(SerialEcho PRIVATE /utf-8)

# Preprocessor definitions
target_compile_definitions(SerialEcho PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

# Trace logging option
option(ENABLE_TRACE "Enable trace logging to trace.log" OFF)
if(ENABLE_TRACE)
    target_compile_definitions(SerialEcho PRIVATE ENABLE_TRACE=1)
    message(STATUS "Trace logging enabled")
endif()
