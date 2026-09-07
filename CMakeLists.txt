# SunVox Playback Plugin
#
# This plugin plays SunVox modular synthesizer projects (.sunvox) using the
# SunVox engine, compiled from the distribution's sources and linked in, so the
# artifact stays a single file with no companion shared library beside it.
#
# The distribution ships its own GNU makefile. This build does not use it: it
# compiles the same sources directly, the way every other plugin in this repo
# treats its third-party code. Driving that makefile meant overriding its MinGW
# toolchain, routing every -D through its SUNDOG_DEFINES variable, and patching
# 122 of its rules that never named their output file -- all of which CMake
# does correctly by construction. The engine's own configuration is reproduced
# below, and was taken from "make -n" rather than by reading the makefiles.

cmake_minimum_required(VERSION 3.16)
project(playback_sunvox C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PlaybackPlugin)

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
        patches/sunvox-static-lib.patch
)

# The 109 sources the engine's makefile compiles for this configuration
# (MAKE_STATIC_LIB, no GUI, no main, no encoders). macOS adds one more.
set(SUNVOX_ENGINE_SOURCES
    # lib_sundog (12)
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/file/file.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/file/file_format.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/file/file_format_rw.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/log/log.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/main/main.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/memory/memory.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/misc/misc.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/net/net.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/sound/sound.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/thread/thread.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/time/time.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sundog/video/video.cpp
    # lib_sunvox (62)
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/midi_file/midi_file.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynth_dsp.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynth_gui_utils.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynth_net.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynth_net_midi_in.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynth_strings.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_adsr.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_amplifier.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_compressor.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_ctl2note.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_dc_blocker.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_delay.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_distortion.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_drumsynth.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_echo.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_eq.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_feedback.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_fft.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_filter.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_filter2.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_flanger.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_fm.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_fm2.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_generator.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_generator2.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_glide.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_gpio.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_input.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_kicker.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_lfo.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_loop.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_metamodule.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_modulator.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_multictl.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_multisynth.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_pitch2ctl.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_pitch_detector.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_pitch_shifter.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_reverb.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_sampler.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_smooth.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_sound2ctl.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_spectravoice.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_velocity2ctl.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_vibrato.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_vocal_filter.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_vorbis_player.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/psynth/psynths_waveshaper.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_action_handler.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_audio_callback.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_export.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_load_module.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_load_proj.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_patterns.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_record.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_save_module.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/sunvox_engine_save_proj.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/xm/xm_instrum.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/xm/xm_pattern.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/xm/xm_sample.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox/xm/xm_song.cpp
    # lib_dsp (2)
    ${sunvox_dll_SOURCE_DIR}/lib_dsp/dsp_functions.cpp
    ${sunvox_dll_SOURCE_DIR}/lib_dsp/dsp_tables.cpp
    # lib_vorbis (15)
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/libogg/bitwise.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/libogg/framing.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/bitwise.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/codebook.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/dsp.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/floor0.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/floor1.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/floor_lookup.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/framing.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/info.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/mapping0.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/mdct.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/misc.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/res012.c
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor/vorbisfile.c
    # lib_flac (17)
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/bitmath.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/bitreader.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/cpu.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/crc.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/fixed.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/fixed_intrin_sse2.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/fixed_intrin_sse42.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/fixed_intrin_ssse3.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/format.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/lpc.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/lpc_intrin_neon.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/lpc_intrin_sse2.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/lpc_intrin_sse41.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/md5.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/memory.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/stream_decoder.c
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC/win_utf8_io.c
    # sunvox_lib (1)
    ${sunvox_dll_SOURCE_DIR}/sunvox_lib/main/sunvox_lib.cpp)
if(APPLE)
    list(APPEND SUNVOX_ENGINE_SOURCES
        ${sunvox_dll_SOURCE_DIR}/lib_sundog/file/file_apple.mm)
endif()

add_library(sunvox_engine STATIC ${SUNVOX_ENGINE_SOURCES})
suppress_external_warnings(sunvox_engine)
set_target_properties(sunvox_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(sunvox_engine PUBLIC
    ${sunvox_dll_SOURCE_DIR}/lib_sundog
    ${sunvox_dll_SOURCE_DIR}/lib_sunvox
    ${sunvox_dll_SOURCE_DIR}/lib_dsp
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/tremor
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/libvorbis
    ${sunvox_dll_SOURCE_DIR}/lib_vorbis/libogg
    ${sunvox_dll_SOURCE_DIR}/lib_mp3
    ${sunvox_dll_SOURCE_DIR}/lib_flac/libFLAC
    ${sunvox_dll_SOURCE_DIR}/sunvox_lib/headers
)

# SUNVOX_STATIC_LIB is what tells the sources they are being compiled into
# someone else's binary: no __declspec(dllexport) on the API, and no LPCTSTR
# typedef for a runtime loader this build does not have.
target_compile_definitions(sunvox_engine PUBLIC
    SUNVOX_STATIC_LIB
)
target_compile_definitions(sunvox_engine PRIVATE
    APPNAME="sunvox"
    SUNVOX_LIB
    MIN_SAMPLE_RATE=44100
    PS_STYPE_FLOAT32
    NOMAIN NOGUI NOVIDEO NOVCAP NOLIST NOFILEUTILS NOIMAGEFORMATS NOMIDI
    NOOGGENC NOFLACENC
)

# No device backend on any platform: the plugin renders through
# SV_INIT_FLAG_USER_AUDIO_CALLBACK and never opens one, and each backend it
# would otherwise compile in drags a library past the release audit --
# libasound on Linux, dsound/dxguid/ole32 on Windows.
if(WIN32)
    target_compile_definitions(sunvox_engine PRIVATE
        COLOR32BITS NODSOUND NOASIO _USE_MATH_DEFINES)
    target_compile_options(sunvox_engine PRIVATE -fno-exceptions)
elseif(APPLE)
    target_compile_definitions(sunvox_engine PRIVATE
        COLOR32BITS OPENGL CPUMARK=0)
else()
    target_compile_definitions(sunvox_engine PRIVATE
        COLOR16BITS NOALSA NOOSS)
endif()

# The engine is a DSP library and its makefile builds it fast-math everywhere.
target_compile_options(sunvox_engine PRIVATE -ffast-math)
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_options(sunvox_engine PRIVATE -msse3)
endif()

add_library(sunvox_playback SHARED
    sunvox_plugin.c
)

# The engine is C++; the plugin is C. Link the C++ runtime statically so it
# does not appear in DT_NEEDED either.
target_link_libraries(sunvox_playback PRIVATE sunvox_engine)
if(WIN32)
    # winmm, for the Waveform Audio driver -- the one Windows backend with no
    # compile-out switch upstream -- and for sundog_global_init's
    # timeBeginPeriod call raising the system timer resolution.
    target_link_libraries(sunvox_playback PRIVATE winmm)
elseif(UNIX AND NOT APPLE)
    target_link_options(sunvox_playback PRIVATE -static-libstdc++)
    target_link_libraries(sunvox_playback PRIVATE m pthread)
elseif(APPLE)
    # The engine reaches for the system frameworks its own makefile links.
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
