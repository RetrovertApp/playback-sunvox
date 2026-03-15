# SunVox Playback Plugin
#
# This plugin plays SunVox modular synthesizer projects (.sunvox) using the
# SunVox library. SunVox is distributed as a binary-only shared library.

cmake_minimum_required(VERSION 3.16)
project(playback_sunvox C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PlaybackPlugin)

# Download sunvox_dll distribution
download_library(
    NAME sunvox_dll
    URL "https://github.com/warmplace/sunvox_dll/archive/refs/tags/2.1.2b.tar.gz"
    FILENAME "sunvox_dll-2.1.2b.tar.gz"
    HASH "a37882305031078b2f5fd54f10d796fa249b5a5e8e0fcd26578affd05cb724c3"
    EXTRACT_DIR "sunvox_dll-2.1.2b"
    PATCHES patches/sunvox-no-messagebox.patch
)

# Determine platform-specific library path
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SUNVOX_LIB_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/windows/lib_x86_64")
    else()
        set(SUNVOX_LIB_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/windows/lib_x86")
    endif()
    set(SUNVOX_LIB_NAME "sunvox.dll")
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(SUNVOX_LIB_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/macos/lib_arm64")
    else()
        set(SUNVOX_LIB_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/macos/lib_x86_64")
    endif()
    set(SUNVOX_LIB_NAME "sunvox.dylib")
else()
    # Linux
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(SUNVOX_LIB_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/linux/lib_arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        set(SUNVOX_LIB_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/linux/lib_arm")
    else()
        set(SUNVOX_LIB_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/linux/lib_x86_64")
    endif()
    set(SUNVOX_LIB_NAME "sunvox.so")
endif()

# Plugin shared library (uses dynamic loading of sunvox, not linking)
add_library(sunvox_playback SHARED
    sunvox_plugin.c
)

target_include_directories(sunvox_playback PRIVATE
    ${sunvox_dll_SOURCE_DIR}/sunvox_lib/headers
)

# Link dl for dlopen/dlsym used by sunvox dynamic loading
if(UNIX AND NOT APPLE)
    target_link_libraries(sunvox_playback PRIVATE dl)
endif()

target_compile_options(sunvox_playback PRIVATE
    -Wno-unused-parameter
)

# Copy the sunvox shared library alongside the plugin
add_custom_command(TARGET sunvox_playback POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SUNVOX_LIB_DIR}/${SUNVOX_LIB_NAME}"
        "$<TARGET_FILE_DIR:sunvox_playback>/${SUNVOX_LIB_NAME}"
    COMMENT "Copying SunVox library to plugin directory"
)

rv_set_plugin_properties(sunvox_playback)
