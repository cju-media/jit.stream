cmake_minimum_required(VERSION 3.15)
project(jit.stream)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Find FFmpeg using PkgConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale libswresample)

# Max SDK Path configuration
if(NOT DEFINED MAX_SDK_PATH)
    # Check common locations
    if(EXISTS "${CMAKE_SOURCE_DIR}/max-sdk-base")
        set(MAX_SDK_PATH "${CMAKE_SOURCE_DIR}/max-sdk-base")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/../max-sdk-base")
        set(MAX_SDK_PATH "${CMAKE_SOURCE_DIR}/../max-sdk-base")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/../max-sdk")
         set(MAX_SDK_PATH "${CMAKE_SOURCE_DIR}/../max-sdk")
    else()
        # Default
        set(MAX_SDK_PATH "${CMAKE_SOURCE_DIR}/max-sdk-base")
    endif()
endif()

message(STATUS "MAX_SDK_PATH: ${MAX_SDK_PATH}")

if(NOT EXISTS "${MAX_SDK_PATH}")
    message(FATAL_ERROR "Max SDK not found at ${MAX_SDK_PATH}. Please set MAX_SDK_PATH correctly.")
endif()

# Modern Max SDK CMake Integration
# Instead of manually finding libraries, we add the SDK support folder.
# This defines targets like c74::max, c74::msp, c74::jit
add_subdirectory("${MAX_SDK_PATH}/c74support" c74support)

# Source files
set(SOURCES src/jit.stream.cpp)

# Create the library
add_library(jit.stream MODULE ${SOURCES})

# Link libraries
# Use the targets defined by the SDK
target_link_libraries(jit.stream
    PRIVATE
    PkgConfig::FFMPEG
    c74::max
    c74::msp
    c74::jit
)

# Properties handled mostly by SDK, but we ensure module/bundle
if(APPLE)
    set_target_properties(jit.stream PROPERTIES
        BUNDLE ON
        BUNDLE_EXTENSION "mxo"
        XCODE_ATTRIBUTE_WRAPPER_EXTENSION "mxo"
    )
elseif(WIN32)
    set_target_properties(jit.stream PROPERTIES
        SUFFIX ".mxe64"
    )
endif()

target_compile_definitions(jit.stream PRIVATE C74_X64)
