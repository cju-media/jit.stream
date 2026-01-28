cmake_minimum_required(VERSION 3.15)
project(jit.stream)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Build Universal Binary on macOS to support both Intel and Apple Silicon
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for OS X" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)
endif()

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

# Source files
set(SOURCES src/jit.stream.cpp)

# Create the library
add_library(jit.stream MODULE ${SOURCES})

# Platform specific definitions and libraries
if(APPLE)
    add_definitions(-DMAC_VERSION)

    # On macOS, we rely on the host application (Max) to provide symbols.
    # We use dynamic lookup instead of linking against static Frameworks/libs,
    # which avoids issues if the SDK repository does not contain the binary frameworks.
    set_target_properties(jit.stream PROPERTIES
        BUNDLE ON
        BUNDLE_EXTENSION "mxo"
        XCODE_ATTRIBUTE_WRAPPER_EXTENSION "mxo"
        LINK_FLAGS "-undefined dynamic_lookup"
    )

    # Ad-hoc codesign to bypass "system security policy" errors on Apple Silicon
    add_custom_command(TARGET jit.stream POST_BUILD
        COMMAND codesign --force --deep -s - "$<TARGET_FILE_DIR:jit.stream>/../"
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

# Link FFmpeg
target_link_libraries(jit.stream PRIVATE PkgConfig::FFMPEG)

target_compile_definitions(jit.stream PRIVATE C74_X64)
