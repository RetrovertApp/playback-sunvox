# SunVox Playback Plugin
#
# This plugin plays SunVox modular synthesizer projects (.sunvox) using the
# SunVox library, built from the distribution's sources and linked statically.

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
        patches/sunvox-msvc-compat.patch
        patches/sunvox-explicit-object-names.patch
        patches/sunvox-static-no-dllexport.patch
        patches/sunvox-static-no-windows-h.patch
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

set(SUNVOX_MAKE_DIR "${sunvox_dll_SOURCE_DIR}/sunvox_lib/make")

# The archive the makefile's "all" target builds, where it builds it. The
# makefile's own "install" rule is not usable for a static build: only its
# Linux branch expands FILE_EXT, so macOS moves a sunvox.dylib and Windows a
# sunvox.dll, neither of which a MAKE_STATIC_LIB=true build ever produces.
set(SUNVOX_STATIC_LIB_PATH "${SUNVOX_MAKE_DIR}/sunvox.a")

# The compiler path is quoted inside the variable's value: the makefile expands
# $(CC) straight into a shell command, and on Windows the toolchain lives under
# a path with a space in it.
set(SUNVOX_MAKE_TOOLS "")
if(WIN32)
    # The makefile's Windows branch defaults to the MinGW toolchain; this build
    # uses the same clang the plugin is compiled with. -fPIC is a no-op here.
    set(SUNVOX_MAKE_CC "CC=\"${CMAKE_C_COMPILER}\"")
    list(APPEND SUNVOX_MAKE_TOOLS "AR=llvm-ar")
else()
    # The makefile only adds -fPIC for its dynamic-library target, and the static
    # archive is linked into this plugin's shared object. CXX is built from CC,
    # so appending the flag there covers both.
    set(SUNVOX_MAKE_CC "CC=\"${CMAKE_C_COMPILER}\" -fPIC")
    if(APPLE)
        # The makefile pins an SDK path by version, which will not be the one on
        # any given machine, and leaves LIBTOOL unset for its own macOS static
        # target. CMAKE_OSX_SYSROOT is often empty because the compiler resolves
        # the SDK itself, so ask for the path directly when it is.
        set(SUNVOX_MACOS_SDK "${CMAKE_OSX_SYSROOT}")
        if(NOT SUNVOX_MACOS_SDK)
            execute_process(
                COMMAND xcrun --sdk macosx --show-sdk-path
                OUTPUT_VARIABLE SUNVOX_MACOS_SDK
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        endif()
        if(NOT SUNVOX_MACOS_SDK)
            message(FATAL_ERROR "could not resolve a macOS SDK path for the SunVox engine build")
        endif()
        list(APPEND SUNVOX_MAKE_TOOLS
            "MACOS_SDK=${SUNVOX_MACOS_SDK}"
            "LIBTOOL=libtool"
            "LIBTOOL_FLAGS=-static")
    else()
        list(APPEND SUNVOX_MAKE_TOOLS "AR=ar")
    endif()
endif()

set(SUNVOX_MAKE_ARGS
    TARGET_OS=${SUNVOX_MAKE_OS}
    TARGET_ARCH=${SUNVOX_MAKE_ARCH}
    MAKE_STATIC_LIB=true
    MAKE_WITHOUT_AUDIO_DEVICE=true
    STYPE=PS_STYPE_FLOAT32
    ${SUNVOX_MAKE_SIMD}
)

add_custom_command(
    OUTPUT "${SUNVOX_STATIC_LIB_PATH}"
    COMMAND ${CMAKE_COMMAND} -E env
        make -j${CMAKE_BUILD_PARALLEL_LEVEL}
        "${SUNVOX_MAKE_CC}" ${SUNVOX_MAKE_TOOLS} ${SUNVOX_MAKE_ARGS}
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
if(WIN32)
    # winmm, for two things the engine does on Windows regardless of the audio
    # backend: the Waveform Audio (MMSYSTEM) driver, which is the one Windows
    # backend with no compile-out switch upstream, and sundog_global_init's
    # timeBeginPeriod call raising the system timer resolution.
    target_link_libraries(sunvox_playback PRIVATE winmm)
elseif(UNIX AND NOT APPLE)
    target_link_options(sunvox_playback PRIVATE -static-libstdc++)
    target_link_libraries(sunvox_playback PRIVATE m pthread)
elseif(APPLE)
    # The engine's macOS build reaches for the system frameworks its own
    # makefile links against.
    target_link_libraries(sunvox_playback PRIVATE
        "-framework Cocoa"
        "-framework Carbon"
        "-framework AudioUnit"
        "-framework CoreAudio"
    )
endif()

target_compile_options(sunvox_playback PRIVATE
    -Wno-unused-parameter
)

rv_set_plugin_properties(sunvox_playback)
