cmake_minimum_required(VERSION 3.10)
project(jit_rtmp)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Max SDK Path (user can override)
if(NOT DEFINED MAX_SDK_PATH)
    set(MAX_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../max-sdk-base")
endif()

message(STATUS "Using Max SDK at: ${MAX_SDK_PATH}")

# Include directories for Max SDK
include_directories(
    "${MAX_SDK_PATH}/c74support/max-includes"
    "${MAX_SDK_PATH}/c74support/jit-includes"
    "${MAX_SDK_PATH}/c74support/msp-includes"
)

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale)

# Source files
set(SOURCES
    jit.rtmp.c
    max.jit.rtmp.c
)

# Create library
add_library(jit_rtmp MODULE ${SOURCES})

# Platform specific settings
if(APPLE)
    set_target_properties(jit_rtmp PROPERTIES SUFFIX ".mxo")
    set_target_properties(jit_rtmp PROPERTIES BUNDLE TRUE)
    set(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
elseif(WIN32)
    set_target_properties(jit_rtmp PROPERTIES SUFFIX ".mxe64")
    # Link against Max/Jitter libs
    target_link_libraries(jit_rtmp
        "${MAX_SDK_PATH}/c74support/max-includes/x64/MaxAPI.lib"
        "${MAX_SDK_PATH}/c74support/jit-includes/x64/jitlib.lib"
    )
endif()

# Link FFmpeg
target_include_directories(jit_rtmp PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(jit_rtmp ${FFMPEG_LIBRARIES})
