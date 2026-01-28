cmake_minimum_required(VERSION 3.15)
project(jit.stream)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Find FFmpeg using PkgConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale libswresample)

# Max SDK Path configuration
if(NOT DEFINED MAX_SDK_PATH)
    # Default assumption: SDK is parallel to this project or in a standard location
    set(MAX_SDK_PATH "${CMAKE_SOURCE_DIR}/max-sdk-base")
    message(STATUS "MAX_SDK_PATH not set, defaulting to ${MAX_SDK_PATH}")
endif()

# Check if SDK path exists (optional warning)
if(NOT EXISTS "${MAX_SDK_PATH}")
    message(WARNING "Max SDK not found at ${MAX_SDK_PATH}. Please set MAX_SDK_PATH.")
endif()

# Include directories for Max SDK
include_directories(
    "${MAX_SDK_PATH}/c74support/max-includes"
    "${MAX_SDK_PATH}/c74support/msp-includes"
    "${MAX_SDK_PATH}/c74support/jit-includes"
)

# Platform specific definitions and libraries
if(APPLE)
    add_definitions(-DMAC_VERSION)
    find_library(MAXAPI_LIB MaxAPI PATHS "${MAX_SDK_PATH}/c74support/max-includes")
    find_library(MSPAPI_LIB MSPAPI PATHS "${MAX_SDK_PATH}/c74support/msp-includes")
    find_library(JITAPI_LIB JitterAPI PATHS "${MAX_SDK_PATH}/c74support/jit-includes")
    set(MAX_LIBS ${MAXAPI_LIB} ${MSPAPI_LIB} ${JITAPI_LIB})
elseif(WIN32)
    add_definitions(-DWIN_VERSION -DWIN32_LEAN_AND_MEAN)
    set(MAX_LIBS
        "${MAX_SDK_PATH}/c74support/max-includes/x64/MaxAPI.lib"
        "${MAX_SDK_PATH}/c74support/msp-includes/x64/MSPAPI.lib"
        "${MAX_SDK_PATH}/c74support/jit-includes/x64/JitterAPI.lib"
    )
endif()

# Source files
set(SOURCES src/jit.stream.cpp)

# Create the library
# Max externals are bundles on macOS and DLLs on Windows
add_library(jit.stream MODULE ${SOURCES})

# Link libraries
target_link_libraries(jit.stream
    PkgConfig::FFMPEG
    ${MAX_LIBS}
)

# Properties
if(APPLE)
    set_target_properties(jit.stream PROPERTIES
        BUNDLE ON
        BUNDLE_EXTENSION "mxo"
    )
elseif(WIN32)
    set_target_properties(jit.stream PROPERTIES
        SUFFIX ".mxe64"
    )
endif()

target_compile_definitions(jit.stream PRIVATE C74_X64)
