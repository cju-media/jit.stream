cmake_minimum_required(VERSION 3.15)
project(jit.stream)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Force Apple Silicon build if on macOS to match Homebrew libs
if(APPLE)
    # Detect host architecture
    execute_process(COMMAND uname -m OUTPUT_VARIABLE HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOST_ARCH STREQUAL "arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for OS X" FORCE)
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures for OS X" FORCE)
    endif()

    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version" FORCE)
endif()

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
        LINK_FLAGS "-undefined dynamic_lookup"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.jit.stream"
        MACOSX_BUNDLE_BUNDLE_NAME "jit.stream"
        MACOSX_BUNDLE_INFO_STRING "jit.stream external"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_LONG_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_COPYRIGHT "Copyright 2024"
    )

    # Clean xattrs before signing to fix "resource fork" error
    add_custom_command(TARGET jit.stream POST_BUILD
        COMMAND xattr -rc "$<TARGET_BUNDLE_DIR:jit.stream>"
        COMMENT "Cleaning extended attributes"
    )

    # Ad-hoc codesign the bundle root
    add_custom_command(TARGET jit.stream POST_BUILD
        COMMAND codesign --force --deep -s - "$<TARGET_BUNDLE_DIR:jit.stream>"
        COMMENT "Ad-hoc codesigning the bundle"
    )

    # Verify Architecture
    add_custom_command(TARGET jit.stream POST_BUILD
        COMMAND lipo -info "$<TARGET_FILE_DIR:jit.stream>/jit.stream"
        COMMENT "Verifying built architecture"
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
