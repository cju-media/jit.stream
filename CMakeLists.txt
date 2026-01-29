cmake_minimum_required(VERSION 3.15)
project(jit.my.pass)

set(CMAKE_CXX_STANDARD 17)

# Platform check
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

# SDK Path
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

if(NOT EXISTS "${MAX_SDK_PATH}")
    message(FATAL_ERROR "Max SDK not found at ${MAX_SDK_PATH}")
endif()

include_directories(
    "${MAX_SDK_PATH}/c74support/max-includes"
    "${MAX_SDK_PATH}/c74support/msp-includes"
    "${MAX_SDK_PATH}/c74support/jit-includes"
)

# Source
set(SOURCES src/jit.my.pass.cpp)

# Target
add_library(jit_my_pass_core MODULE ${SOURCES})

# Properties to match Max External Bundle format exactly
set_target_properties(jit_my_pass_core PROPERTIES OUTPUT_NAME "jit.my.pass")
set_target_properties(jit_my_pass_core PROPERTIES PREFIX "")
set_target_properties(jit_my_pass_core PROPERTIES SUFFIX "")

if(APPLE)
    add_definitions(-DMAC_VERSION)
    set_target_properties(jit_my_pass_core PROPERTIES
        BUNDLE ON
        BUNDLE_EXTENSION "mxo"
        XCODE_ATTRIBUTE_WRAPPER_EXTENSION "mxo"
        LINK_FLAGS "-undefined dynamic_lookup"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.jit.my.pass"
        MACOSX_BUNDLE_BUNDLE_NAME "jit.my.pass"
        MACOSX_BUNDLE_EXECUTABLE_NAME "jit.my.pass"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        MACOSX_BUNDLE_PACKAGE_TYPE "iLaX"
    )

    # Post-build: Clean, Sign, Perms
    add_custom_command(TARGET jit_my_pass_core POST_BUILD
        COMMAND xattr -rc "$<TARGET_BUNDLE_DIR:jit_my_pass_core>"
        COMMAND codesign --force --deep -s - "$<TARGET_BUNDLE_DIR:jit_my_pass_core>"
        COMMAND chmod +x "$<TARGET_FILE:jit_my_pass_core>"
        COMMENT "Signing and fixing bundle"
    )

elseif(WIN32)
    add_definitions(-DWIN_VERSION -DWIN32_LEAN_AND_MEAN)
    set(MAXAPI_LIB "${MAX_SDK_PATH}/c74support/max-includes/x64/MaxAPI.lib")
    set(JITAPI_LIB "${MAX_SDK_PATH}/c74support/jit-includes/x64/JitterAPI.lib")
    target_link_libraries(jit_my_pass_core PRIVATE ${MAXAPI_LIB} ${JITAPI_LIB})
endif()

# Removed C74_X64 to avoid warning
# target_compile_definitions(jit_my_pass_core PRIVATE C74_X64)
