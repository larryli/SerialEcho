# CMakeLists.txt - Build configuration for SerialEcho
#
# Supported toolchains:
#   - MSVC (Visual Studio Build Tools)   : any CMake >= 3.20
#   - Pelles C (compiler id "PellesC")    : CMake >= 4.4.0 (native support added in 4.4)
#
# The toolchain is auto-detected; no extra flags are required to select it.
#
# Both toolchains use the default "build" directory. Pick ONE toolchain per
# workspace. If you switch toolchains, remove the build directory first:
#   rmdir /s /q build
# (CMake cannot change the generator inside an existing build directory.)
#
# MSVC (multi-config generator):
#   cmake -B build
#   cmake --build build --config Release -j
#   cmake --build build --config Debug
#
# Pelles C (single-config generator, CMake >= 4.4.0 + Ninja):
#   cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build
#   cmake --build build
#
# Build (Trace): add -DENABLE_TRACE_FW=ON -DENABLE_TRACE_PROTO=ON

cmake_minimum_required(VERSION 3.20)
project(SerialEcho C)

# Pelles C gained native CMake support in 4.4.0 (compiler id "PellesC").
if(CMAKE_C_COMPILER_ID STREQUAL "PellesC" AND CMAKE_VERSION VERSION_LESS 4.4.0)
    message(FATAL_ERROR "Pelles C builds require CMake >= 4.4.0 (found ${CMAKE_VERSION}).")
endif()

set(CMAKE_C_STANDARD 11)

# Source files
add_executable(SerialEcho WIN32
    src/main.c
    src/serial.c
    src/example_echo_hal.c
    src/example_echo.c
    src/app_protocol.c
    src/app_logview.c
    src/utils/config.c
    src/utils/lang.c
    src/utils/trace.c
    src/utils/timer.c
    src/resource.rc
)

# Include directories
target_include_directories(SerialEcho PRIVATE src)

# Preprocessor definitions (shared by all toolchains)
target_compile_definitions(SerialEcho PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

# --- Toolchain specific settings -------------------------------------------

if(MSVC)
    # MSVC: disable automatic manifest (custom manifest lives in resource.rc)
    set_target_properties(SerialEcho PROPERTIES
        LINK_FLAGS "/MANIFEST:NO"
    )

    # MSVC compiler options
    target_compile_options(SerialEcho PRIVATE /utf-8)

    # MSVC resource compiler options
    set_source_files_properties(src/resource.rc PROPERTIES COMPILE_FLAGS "/utf-8 /nologo")

    # Link libraries (setupapi/shlwapi/Shell32 are pulled by #pragma comment
    # in source; comdlg32 is referenced via commdlg.h and linked explicitly
    # for clarity and Pelles C compatibility)
    target_link_libraries(SerialEcho PRIVATE
        comctl32
        uuid
        comdlg32
    )

    message(STATUS "Toolchain: MSVC (${CMAKE_C_COMPILER_VERSION})")
elseif(CMAKE_C_COMPILER_ID STREQUAL "PellesC")
    # Pelles C: full set of system import libraries (mirrors SerialEcho.ppj)
    target_link_libraries(SerialEcho PRIVATE
        kernel32
        user32
        gdi32
        comctl32
        comdlg32
        advapi32
        setupapi
        shlwapi
        uuid
        Shell32
        Version
    )

    # Pelles C resource compiler (porc) does not accept MSVC-style flags.
    # SerialEcho resources are English-only ASCII, so no UTF-8 flag is needed;
    # porc compiles pure-ASCII .rc without a BOM without issue.
    message(STATUS "Toolchain: Pelles C (${CMAKE_C_COMPILER_VERSION})")
else()
    message(WARNING "Unsupported toolchain (${CMAKE_C_COMPILER_ID}); build may fail.")
endif()

# --- Build type defaults ----------------------------------------------------

# Align default build type with typical single-config usage.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Trace logging options (enable one or both)
option(ENABLE_TRACE_FW "Enable framework trace logging" OFF)
option(ENABLE_TRACE_PROTO "Enable protocol trace logging" OFF)

if(ENABLE_TRACE_FW)
    target_compile_definitions(SerialEcho PRIVATE ENABLE_TRACE_FW=1)
    message(STATUS "Framework trace enabled")
endif()

if(ENABLE_TRACE_PROTO)
    target_compile_definitions(SerialEcho PRIVATE ENABLE_TRACE_PROTO=1)
    message(STATUS "Protocol trace enabled")
endif()
