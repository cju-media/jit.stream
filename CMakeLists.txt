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
        set(MAX_SDK_PATH "${CMAKE_SOURCE_DIR}/max-sdk-base")
    endif()
endif()

message(STATUS "MAX_SDK_PATH: ${MAX_SDK_PATH}")

if(NOT EXISTS "${MAX_SDK_PATH}")
    message(FATAL_ERROR "Max SDK not found at ${MAX_SDK_PATH}. Please set MAX_SDK_PATH correctly.")
endif()

# Explicitly add include directories to find ext.h, jit.common.h, etc.
include_directories(
    "${MAX_SDK_PATH}/c74support/max-includes"
    "${MAX_SDK_PATH}/c74support/msp-includes"
    "${MAX_SDK_PATH}/c74support/jit-includes"
)

# Platform specific definitions and libraries
if(APPLE)
    add_definitions(-DMAC_VERSION)

    # Manually check for Frameworks using explicit paths
    set(MAXAPI_FRAMEWORK "${MAX_SDK_PATH}/c74support/max-includes/MaxAPI.framework")
    set(MSPAPI_FRAMEWORK "${MAX_SDK_PATH}/c74support/msp-includes/MSPAPI.framework")
    set(JITAPI_FRAMEWORK "${MAX_SDK_PATH}/c74support/jit-includes/JitterAPI.framework")

    if(EXISTS "${MAXAPI_FRAMEWORK}")
        set(MAXAPI_LIB "${MAXAPI_FRAMEWORK}")
        message(STATUS "Found MaxAPI: ${MAXAPI_LIB}")
    else()
        message(FATAL_ERROR "MaxAPI.framework not found at ${MAXAPI_FRAMEWORK}")
    endif()

    if(EXISTS "${MSPAPI_FRAMEWORK}")
        set(MSPAPI_LIB "${MSPAPI_FRAMEWORK}")
        message(STATUS "Found MSPAPI: ${MSPAPI_LIB}")
    else()
        message(FATAL_ERROR "MSPAPI.framework not found at ${MSPAPI_FRAMEWORK}")
    endif()

    if(EXISTS "${JITAPI_FRAMEWORK}")
        set(JITAPI_LIB "${JITAPI_FRAMEWORK}")
        message(STATUS "Found JitterAPI: ${JITAPI_LIB}")
    else()
        message(FATAL_ERROR "JitterAPI.framework not found at ${JITAPI_FRAMEWORK}")
    endif()

    set(MAX_LIBS ${MAXAPI_LIB} ${MSPAPI_LIB} ${JITAPI_LIB})

elseif(WIN32)
    add_definitions(-DWIN_VERSION -DWIN32_LEAN_AND_MEAN)

    set(MAXAPI_LIB "${MAX_SDK_PATH}/c74support/max-includes/x64/MaxAPI.lib")
    set(MSPAPI_LIB "${MAX_SDK_PATH}/c74support/msp-includes/x64/MSPAPI.lib")
    set(JITAPI_LIB "${MAX_SDK_PATH}/c74support/jit-includes/x64/JitterAPI.lib")

    if (NOT EXISTS "${MAXAPI_LIB}")
         message(FATAL_ERROR "MaxAPI.lib not found at ${MAXAPI_LIB}")
    endif()

    set(MAX_LIBS ${MAXAPI_LIB} ${MSPAPI_LIB} ${JITAPI_LIB})
endif()

# Source files
set(SOURCES src/jit.stream.cpp)

# Create the library
add_library(jit.stream MODULE ${SOURCES})

# Link libraries
target_link_libraries(jit.stream
    PRIVATE
    PkgConfig::FFMPEG
    ${MAX_LIBS}
)

# Properties
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
