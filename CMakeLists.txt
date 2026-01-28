cmake_minimum_required(VERSION 3.15)
project(jit.stream)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Find FFmpeg using PkgConfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale libswresample)

# Max SDK Path configuration
if(NOT DEFINED MAX_SDK_PATH)
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

include_directories(
    "${MAX_SDK_PATH}/c74support/max-includes"
    "${MAX_SDK_PATH}/c74support/msp-includes"
    "${MAX_SDK_PATH}/c74support/jit-includes"
)

set(SOURCES src/jit.stream.cpp)

add_library(jit.stream MODULE ${SOURCES})

# Ensure the binary name inside the bundle matches the bundle name
set_target_properties(jit.stream PROPERTIES OUTPUT_NAME "jit.stream")

if(APPLE)
    add_definitions(-DMAC_VERSION)

    set_target_properties(jit.stream PROPERTIES
        BUNDLE ON
        BUNDLE_EXTENSION "mxo"
        XCODE_ATTRIBUTE_WRAPPER_EXTENSION "mxo"
        # -bundle is usually added by MODULE, but -undefined dynamic_lookup is needed for Max
        LINK_FLAGS "-undefined dynamic_lookup"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.jit.stream"
        MACOSX_BUNDLE_BUNDLE_NAME "jit.stream"
        MACOSX_BUNDLE_INFO_STRING "jit.stream external"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_LONG_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_COPYRIGHT "Copyright 2024"
    )

    # Ad-hoc codesign the bundle root
    # Uses TARGET_BUNDLE_DIR to correctly resolve the .mxo path
    add_custom_command(TARGET jit.stream POST_BUILD
        COMMAND codesign --force --deep -s - "$<TARGET_BUNDLE_DIR:jit.stream>"
        COMMENT "Ad-hoc codesigning the bundle"
    )

elseif(WIN32)
    add_definitions(-DWIN_VERSION -DWIN32_LEAN_AND_MEAN)

    set(MAXAPI_LIB "${MAX_SDK_PATH}/c74support/max-includes/x64/MaxAPI.lib")
    set(MSPAPI_LIB "${MAX_SDK_PATH}/c74support/msp-includes/x64/MSPAPI.lib")
    set(JITAPI_LIB "${MAX_SDK_PATH}/c74support/jit-includes/x64/JitterAPI.lib")

    if (NOT EXISTS "${MAXAPI_LIB}")
         message(FATAL_ERROR "MaxAPI.lib not found at ${MAXAPI_LIB}")
    endif()

    target_link_libraries(jit.stream PRIVATE ${MAXAPI_LIB} ${MSPAPI_LIB} ${JITAPI_LIB})
endif()

target_link_libraries(jit.stream PRIVATE PkgConfig::FFMPEG)

target_compile_definitions(jit.stream PRIVATE C74_X64)
