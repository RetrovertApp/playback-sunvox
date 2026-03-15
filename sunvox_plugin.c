///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SunVox Playback Plugin
//
// Implements RVPlaybackPlugin interface for SunVox modular synthesizer projects.
// Uses the SunVox library via dynamic loading (sv_load_dll/sv_unload_dll).
// The library is initialized in offline/single-threaded mode for waveform generation.
// SunVox uses global state managed via numbered slots.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// C11 nullptr compatibility
#ifndef nullptr
#define nullptr ((void*)0)
#endif

// Must be defined before including sunvox.h to get the dynamic loading implementation
#define SUNVOX_MAIN

// sunvox.h uses dlopen/dlsym; dladdr needs _GNU_SOURCE
#ifdef _WIN32
#include <windows.h>
#else
#define _GNU_SOURCE
#include <dlfcn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <retrovert/io.h>
#include <retrovert/log.h>
#include <retrovert/metadata.h>
#include <retrovert/playback.h>
#include <retrovert/service.h>

// Override ERROR_MSG before including sunvox.h to prevent MessageBoxA() on Windows.
// The default Windows definition shows a modal dialog that hangs in CI/headless environments.
// The sunvox-no-messagebox.patch adds #ifndef guards so this definition takes precedence.
#define ERROR_MSG(msg) fprintf(stderr, "sunvox error: %s\n", msg);

#include "sunvox.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define OUTPUT_SAMPLE_RATE 48000
#define BUFFER_SIZE 4096
// Use slot 0 for playback (SunVox supports multiple slots but we use one)
#define SUNVOX_SLOT 0

RV_PLUGIN_USE_IO_API();
RV_PLUGIN_USE_LOG_API();
RV_PLUGIN_USE_METADATA_API();
static int g_sunvox_initialized = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct SunvoxReplayerData {
    int slot_open;
    int playing;
    uint32_t song_length_frames;
    uint32_t elapsed_frames;
} SunvoxReplayerData;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* sunvox_plugin_supported_extensions(void) {
    return "sunvox";
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void* sunvox_plugin_create(const RVService* service_api) {
    SunvoxReplayerData* data = calloc(1, sizeof(SunvoxReplayerData));
    if (data == nullptr) {
        return nullptr;
    }

    return data;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sunvox_plugin_destroy(void* user_data) {
    SunvoxReplayerData* data = (SunvoxReplayerData*)user_data;

    if (data->playing) {
        sv_stop(SUNVOX_SLOT);
    }
    if (data->slot_open) {
        sv_close_slot(SUNVOX_SLOT);
    }

    free(data);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sunvox_plugin_open(void* user_data, const char* url, uint32_t subsong, const RVService* service_api) {
    (void)subsong;
    (void)service_api;

    SunvoxReplayerData* data = (SunvoxReplayerData*)user_data;

    // Clean up previous
    if (data->playing) {
        sv_stop(SUNVOX_SLOT);
        data->playing = 0;
    }
    if (data->slot_open) {
        sv_close_slot(SUNVOX_SLOT);
        data->slot_open = 0;
    }

    if (!g_sunvox_initialized) {
        rv_error("sunvox: Library not initialized");
        return -1;
    }

    RVIoReadUrlResult read_res = rv_io_read_url_to_memory(url);
    if (read_res.data == nullptr) {
        rv_error("sunvox: Failed to load %s to memory", url);
        return -1;
    }

    // Open a slot
    if (sv_open_slot(SUNVOX_SLOT) != 0) {
        rv_error("sunvox: Failed to open slot");
        rv_io_free_url_to_memory(read_res.data);
        return -1;
    }
    data->slot_open = 1;

    // Load project from memory
    if (sv_load_from_memory(SUNVOX_SLOT, read_res.data, (uint32_t)read_res.data_size) != 0) {
        rv_error("sunvox: Failed to load project: %s", url);
        rv_io_free_url_to_memory(read_res.data);
        sv_close_slot(SUNVOX_SLOT);
        data->slot_open = 0;
        return -1;
    }

    rv_io_free_url_to_memory(read_res.data);

    // Set volume and enable auto-stop
    sv_volume(SUNVOX_SLOT, 256);
    sv_set_autostop(SUNVOX_SLOT, 1);

    // Get song length
    data->song_length_frames = sv_get_song_length_frames(SUNVOX_SLOT);
    data->elapsed_frames = 0;

    // Start playback
    sv_play_from_beginning(SUNVOX_SLOT);
    data->playing = 1;

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sunvox_plugin_close(void* user_data) {
    SunvoxReplayerData* data = (SunvoxReplayerData*)user_data;

    if (data->playing) {
        sv_stop(SUNVOX_SLOT);
        data->playing = 0;
    }
    if (data->slot_open) {
        sv_close_slot(SUNVOX_SLOT);
        data->slot_open = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVProbeResult sunvox_plugin_probe_can_play(uint8_t* probe_data, uint64_t data_size, const char* url,
                                                  uint64_t total_size) {
    (void)total_size;

    // SunVox project magic: check for "SVOX" at offset 0.
    // NOTE: This magic is not officially documented in sunvox.h. If it doesn't work
    // for all .sunvox files, the extension check below will still catch them.
    if (data_size >= 4 && probe_data[0] == 'S' && probe_data[1] == 'V' && probe_data[2] == 'O'
        && probe_data[3] == 'X') {
        return RVProbeResult_Supported;
    }

    // Extension-based detection (primary method since magic bytes are unverified)
    if (url != nullptr) {
        const char* dot = strrchr(url, '.');
        if (dot != nullptr && strcasecmp(dot, ".sunvox") == 0) {
            // If we have probe data, return Unsure to allow other plugins a chance
            // If extension matches, it's very likely a SunVox file
            return data_size > 0 ? RVProbeResult_Unsure : RVProbeResult_Unsupported;
        }
    }

    return RVProbeResult_Unsupported;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVReadInfo sunvox_plugin_read_data(void* user_data, RVReadData dest) {
    SunvoxReplayerData* data = (SunvoxReplayerData*)user_data;

    RVAudioFormat format = { RVAudioStreamFormat_F32, 2, OUTPUT_SAMPLE_RATE };

    if (!data->playing || !g_sunvox_initialized) {
        return (RVReadInfo) { format, 0, RVReadStatus_Error};
    }

    // Check if song ended
    if (sv_end_of_song(SUNVOX_SLOT)) {
        return (RVReadInfo) { format, 0, RVReadStatus_Finished};
    }

    uint32_t max_frames = dest.channels_output_max_bytes_size / (sizeof(float) * 2);
    if (max_frames > BUFFER_SIZE) {
        max_frames = BUFFER_SIZE;
    }

    // Clamp to remaining song length
    if (data->song_length_frames > 0 && data->elapsed_frames + max_frames > data->song_length_frames) {
        max_frames = data->song_length_frames - data->elapsed_frames;
    }

    if (max_frames == 0) {
        return (RVReadInfo) { format, 0, RVReadStatus_Finished};
    }

    // sv_audio_callback outputs F32 stereo interleaved when initialized with AUDIO_FLOAT32
    float* output = (float*)dest.channels_output;
    int result = sv_audio_callback(output, (int)max_frames, 0, sv_get_ticks());

    data->elapsed_frames += max_frames;

    RVReadStatus status = RVReadStatus_Ok;
    if (sv_end_of_song(SUNVOX_SLOT)
        || (data->song_length_frames > 0 && data->elapsed_frames >= data->song_length_frames)) {
        status = RVReadStatus_Finished;
    }

    // sv_audio_callback returns 0 if buffer is silent (could mean no modules or end)
    (void)result;

    return (RVReadInfo) { format, max_frames, status};
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int64_t sunvox_plugin_seek(void* user_data, int64_t ms) {
    SunvoxReplayerData* data = (SunvoxReplayerData*)user_data;

    if (!data->playing) {
        return -1;
    }

    // Convert ms to line number (approximate)
    int bpm = sv_get_song_bpm(SUNVOX_SLOT);
    int tpl = sv_get_song_tpl(SUNVOX_SLOT);
    if (bpm <= 0 || tpl <= 0) {
        return -1;
    }

    // lines_per_second = bpm * 24 / (tpl * 60)
    double lines_per_ms = (double)(bpm * 24) / (double)(tpl * 60 * 1000);
    int line = (int)(ms * lines_per_ms);

    sv_rewind(SUNVOX_SLOT, line);
    data->elapsed_frames = (uint32_t)((ms * OUTPUT_SAMPLE_RATE) / 1000);

    return ms;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int sunvox_plugin_metadata(const char* url, const RVService* service_api) {
    (void)service_api;

    if (!g_sunvox_initialized) {
        return -1;
    }

    RVIoReadUrlResult read_res = rv_io_read_url_to_memory(url);
    if (read_res.data == nullptr) {
        return -1;
    }

    // Open a temporary slot for metadata extraction
    if (sv_open_slot(SUNVOX_SLOT) != 0) {
        rv_io_free_url_to_memory(read_res.data);
        return -1;
    }

    if (sv_load_from_memory(SUNVOX_SLOT, read_res.data, (uint32_t)read_res.data_size) != 0) {
        rv_io_free_url_to_memory(read_res.data);
        sv_close_slot(SUNVOX_SLOT);
        return -1;
    }

    rv_io_free_url_to_memory(read_res.data);

    RVMetadataId index = rv_metadata_create_url(url);

    const char* name = sv_get_song_name(SUNVOX_SLOT);
    if (name != nullptr && name[0] != '\0') {
        rv_metadata_set_tag(index, RV_METADATA_TITLE_TAG, name);
    }

    rv_metadata_set_tag(index, RV_METADATA_SONGTYPE_TAG, "SunVox");

    // Calculate duration from frame count
    uint32_t total_frames = sv_get_song_length_frames(SUNVOX_SLOT);
    if (total_frames > 0) {
        double duration = (double)total_frames / (double)OUTPUT_SAMPLE_RATE;
        rv_metadata_set_tag_f64(index, RV_METADATA_LENGTH_TAG, duration);
    }

    sv_close_slot(SUNVOX_SLOT);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sunvox_plugin_event(void* user_data, uint8_t* event_data, uint64_t len) {
    (void)user_data;
    (void)event_data;
    (void)len;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sunvox_plugin_static_init(const RVService* service_api) {
    rv_init_log_api(service_api);
    rv_init_io_api(service_api);
    rv_init_metadata_api(service_api);

    if (!g_sunvox_initialized) {
        // Find our own directory, then load sunvox library from there
        int load_result = -1;
        char lib_path[4096];
#ifndef _WIN32
        Dl_info dl_info;
        if (dladdr((void*)sunvox_plugin_static_init, &dl_info) && dl_info.dli_fname) {
            // Build path: dirname(our .so) + "/sunvox.so"
            strncpy(lib_path, dl_info.dli_fname, sizeof(lib_path) - 1);
            lib_path[sizeof(lib_path) - 1] = '\0';
            char* last_slash = strrchr(lib_path, '/');
            if (last_slash) {
                last_slash[1] = '\0';
                size_t dir_len = strlen(lib_path);
                snprintf(lib_path + dir_len, sizeof(lib_path) - dir_len, "sunvox.so");
                rv_info("sunvox: Loading library from %s", lib_path);
                load_result = sv_load_dll2(lib_path);
            }
        }
#else
        // Use GetModuleHandleExA to find our DLL directory, then load sunvox.dll from there.
        // This avoids sv_load_dll() which searches system paths and may hang or show dialogs.
        HMODULE self_module = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)sunvox_plugin_static_init, &self_module)) {
            DWORD path_len = GetModuleFileNameA(self_module, lib_path, sizeof(lib_path));
            if (path_len > 0 && path_len < sizeof(lib_path)) {
                char* last_slash = strrchr(lib_path, '\\');
                if (last_slash) {
                    last_slash[1] = '\0';
                    size_t dir_len = strlen(lib_path);
                    snprintf(lib_path + dir_len, sizeof(lib_path) - dir_len, "sunvox.dll");
                    rv_info("sunvox: Loading library from %s", lib_path);
                    load_result = sv_load_dll2(lib_path);
                }
            }
        }
#endif
        if (load_result != 0) {
            rv_error("sunvox: Failed to load sunvox library");
            return;
        }

        // Initialize in offline mode: no audio device, single-threaded, F32 output
        int flags = SV_INIT_FLAG_NO_DEBUG_OUTPUT | SV_INIT_FLAG_USER_AUDIO_CALLBACK | SV_INIT_FLAG_ONE_THREAD
                    | SV_INIT_FLAG_AUDIO_FLOAT32;

        int ver = sv_init(0, OUTPUT_SAMPLE_RATE, 2, flags);
        if (ver < 0) {
            rv_error("sunvox: sv_init failed (error %d)", ver);
            sv_unload_dll();
            return;
        }

        rv_info("sunvox: Initialized SunVox v%d.%d.%d", (ver >> 16) & 0xFF, (ver >> 8) & 0xFF, ver & 0xFF);
        g_sunvox_initialized = 1;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sunvox_plugin_static_destroy(void) {
    if (g_sunvox_initialized) {
        sv_deinit();
        sv_unload_dll();
        g_sunvox_initialized = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static RVPlaybackPlugin g_sunvox_plugin = {
    RV_PLAYBACK_PLUGIN_API_VERSION,
    "sunvox",
    "0.0.1",
    "SunVox Library 2.1.2b",
    sunvox_plugin_probe_can_play,
    sunvox_plugin_supported_extensions,
    sunvox_plugin_create,
    sunvox_plugin_destroy,
    sunvox_plugin_event,
    sunvox_plugin_open,
    sunvox_plugin_close,
    sunvox_plugin_read_data,
    sunvox_plugin_seek,
    sunvox_plugin_metadata,
    sunvox_plugin_static_init,
    nullptr, // settings_updated
    nullptr, // get_tracker_info
    nullptr, // get_pattern_cell
    nullptr, // get_pattern_num_rows
    nullptr, // get_scope_data
    sunvox_plugin_static_destroy,
    nullptr, // get_scope_channel_names
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RV_EXPORT RVPlaybackPlugin* rv_playback_plugin(void) {
    return &g_sunvox_plugin;
}
