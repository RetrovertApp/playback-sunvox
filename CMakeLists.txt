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
    PATCHES
        patches/sunvox-no-messagebox.patch
        patches/sunvox-config-null-terminator.patch
        patches/sunvox-no-audio-device.patch
)

# The engine is built from the distribution's own sources as a static library and
# linked in, so the artifact stays a single file: the plugin ships as
# <name>_playback alone, with no companion shared library beside it.
#
# MAKE_WITHOUT_AUDIO_DEVICE (added by patches/sunvox-no-audio-device.patch) drops
# the ALSA/OSS/JACK backends. The plugin renders through
# SV_INIT_FLAG_USER_AUDIO_CALLBACK and never opens a device, and without this the
# engine would put libasound.so.2 in the plugin's DT_NEEDED, outside the release
# dependency allowlist.
if(WIN32)
    set(SUNVOX_MAKE_OS "windows")
elseif(APPLE)
    set(SUNVOX_MAKE_OS "macos")
else()
    set(SUNVOX_MAKE_OS "linux")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(SUNVOX_MAKE_ARCH "arm64")
    set(SUNVOX_MAKE_SIMD "")
else()
    set(SUNVOX_MAKE_ARCH "x86_64")
    set(SUNVOX_MAKE_SIMD "MAKE_WITH_SSE_VER=sse3")
endif()

# The engine's own makefile only adds -fPIC for its dynamic-library target, and
# the static archive is linked into this plugin's shared object. It builds CXX
# from CC, so appending the flag there covers both.
set(SUNVOX_MAKE_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/make")
set(SUNVOX_STATIC_LIB_PATH
    "${sunvox_dll_SOURCE_DIR}/sunvox_lib/${SUNVOX_MAKE_OS}/lib_${SUNVOX_MAKE_ARCH}/sunvox.a")

add_custom_command(
    OUTPUT "${SUNVOX_STATIC_LIB_PATH}"
    COMMAND ${CMAKE_COMMAND} -E env
        make -j${CMAKE_BUILD_PARALLEL_LEVEL}
        "CC=${CMAKE_C_COMPILER} -fPIC"
        TARGET_OS=${SUNVOX_MAKE_OS}
        TARGET_ARCH=${SUNVOX_MAKE_ARCH}
        MAKE_STATIC_LIB=true
        MAKE_WITHOUT_AUDIO_DEVICE=true
        STYPE=PS_STYPE_FLOAT32
        ${SUNVOX_MAKE_SIMD}
    COMMAND ${CMAKE_COMMAND} -E env
        make install
        TARGET_OS=${SUNVOX_MAKE_OS}
        TARGET_ARCH=${SUNVOX_MAKE_ARCH}
        MAKE_STATIC_LIB=true
        MAKE_WITHOUT_AUDIO_DEVICE=true
        STYPE=PS_STYPE_FLOAT32
        ${SUNVOX_MAKE_SIMD}
    WORKING_DIRECTORY "${SUNVOX_MAKE_DIR}"
    COMMENT "Building the SunVox engine as a static library"
    VERBATIM
)
add_custom_target(sunvox_engine DEPENDS "${SUNVOX_STATIC_LIB_PATH}")

add_library(sunvox_playback SHARED
    sunvox_plugin.c
)
add_dependencies(sunvox_playback sunvox_engine)

target_include_directories(sunvox_playback PRIVATE
    ${sunvox_dll_SOURCE_DIR}/sunvox_lib/headers
)

# The engine is C++; the plugin is C. Link the C++ runtime statically so it does
# not appear in DT_NEEDED either.
target_link_libraries(sunvox_playback PRIVATE
    "${SUNVOX_STATIC_LIB_PATH}"
)
if(UNIX AND NOT APPLE)
    target_link_options(sunvox_playback PRIVATE -static-libstdc++)
    target_link_libraries(sunvox_playback PRIVATE m pthread)
endif()

target_compile_options(sunvox_playback PRIVATE
    -Wno-unused-parameter
)

rv_set_plugin_properties(sunvox_playback)
