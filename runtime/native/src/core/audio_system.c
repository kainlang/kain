/*
 * KAIN Native Audio System — Implementation
 *
 * Platform backends:
 *   _WIN32    WASAPI shared-mode stereo streaming + WinMM MIDI input
 *   __APPLE__ CoreAudio AudioUnit + CoreMIDI input
 *   __linux__ ALSA PCM streaming + ALSA raw MIDI input
 *   else      Stub (returns ABI_AUDIO_ERR_NO_DEVICE)
 *
 * Design contract (from research/audio/RUNTIME_C.md):
 *  - f32 only at the ABI boundary. The OS audio callback receives/produces
 *    interleaved float* samples.
 *  - Event-driven. WASAPI uses SetEventHandle + WaitForSingleObject. macOS
 *    uses the HAL output AudioUnit render callback. Linux uses poll() on
 *    ALSA's poll descriptors. No spin-waiting anywhere.
 *  - Shared-mode only on Windows (no exclusive mode negotiation).
 *  - No device hot-plug (snapshot enumeration).
 *  - MIDI: 1.0 short messages (status, data1, data2). Running status not
 *    required for Phase 1.
 *
 * Threading:
 *  - Each open audio stream owns one background thread. It calls the user
 *    callback when the OS is ready for a buffer.
 *  - MIDI input is callback-based (WinMM CALLBACK_FUNCTION, CoreMIDI read
 *    proc, or a dedicated ALSA read thread).
 *  - All callbacks run on platform-internal threads. No locks in hot paths;
 *    cross-thread state is set via atomic flags.
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/audio_system.h"
#include "../../include/base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <propsys.h>
/* GUIDs from Mmdeviceapi.h are pulled in above; declare IIDs locally to
   keep the surface stable across Windows SDK revisions. */
#ifndef KAIN_AUDIO_DECLARE_IIDS
#define KAIN_AUDIO_DECLARE_IIDS
#endif
#else
#include <strings.h>
#include <pthread.h>
#include <time.h>
#endif

/* ─────────────────────────────────────────────────────────────────────────
 * Common State
 * ──────────────────────────────────────────────────────────────────────── */

static int64_t g_last_status = ABI_AUDIO_OK;
static char    g_last_error_kind[ABI_AUDIO_MAX_DEVICE_NAME] = "ok";
static char    g_last_error_message[ABI_AUDIO_MAX_DEVICE_NAME] = "";

/* Stream and MIDI input tables are platform-agnostic wrappers. The platform
   block below extends the struct with handles/threads specific to its
   backend. */

typedef struct KainNativeAudioStreamSlot {
    int in_use;
    int64_t id;
    int32_t sample_rate;
    int32_t buffer_size_frames;
    int32_t output_channels;
    int32_t input_channels;
    KainNativeAudioCallback callback;
    void* user_data;

    /* Cross-thread state — set via atomics. */
    atomic_int is_running;
    atomic_int should_stop;
    atomic_int stop_completed;

    /* Backend-specific state. */
#ifdef _WIN32
    IAudioClient*          audio_client;
    IAudioRenderClient*    render_client;
    HANDLE                 event_handle;
    HANDLE                 thread_handle;
    DWORD                  thread_id;
    /* Scratch buffer for any needed conversion (not used in f32 path). */
    float*                 scratch;
    size_t                 scratch_frames;
#endif
#ifdef __APPLE__
    AudioComponentInstance audio_unit;
    pthread_t              thread;
    int                    thread_started;
    /* Buffer used by the render callback. */
    float*                 scratch;
    int32_t                scratch_frames;
    int32_t                scratch_channels;
    atomic_int             callback_inflight;
#endif
#ifdef __linux__
    snd_pcm_t*             pcm;
    snd_pcm_uframes_t      period_size;
    pthread_t              thread;
    int                    thread_started;
    int                    poll_fd_count;
    struct pollfd*         poll_fds;
    float*                 scratch;
    int32_t                scratch_frames;
    int32_t                scratch_channels;
#endif
} KainNativeAudioStreamSlot;

static KainNativeAudioStreamSlot g_streams[ABI_AUDIO_MAX_STREAMS];
static int64_t g_next_stream_id = 1;

typedef struct KainNativeMidiInputSlot {
    int in_use;
    int64_t id;
    KainNativeMidiCallback callback;
    void* user_data;
#ifdef _WIN32
    HMIDIIN    handle;
    int        device_index;
    DWORD      callback_thread_id;
    HANDLE     thread_handle;
    int        thread_started;
    int        shutdown;
    /* Single-message handoff from WinMM callback to dispatch thread. */
    KainNativeMidiEvent pending_event;
    int                  pending_ready;
    CRITICAL_SECTION     lock;
    HANDLE               data_event;
#endif
#ifdef __APPLE__
    MIDIClientRef client;
    MIDIPortRef   port;
    MIDIEndpointRef endpoint;
    int           device_index;
#endif
#ifdef __linux__
    snd_rawmidi_t* midi_in;
    pthread_t      thread;
    int            thread_started;
    int            shutdown;
    int            device_index;
#endif
} KainNativeMidiInputSlot;

static KainNativeMidiInputSlot g_midi_inputs[ABI_AUDIO_MAX_MIDI_INPUTS];
static int64_t g_next_midi_input_id = 1;

/* ─────────────────────────────────────────────────────────────────────────
 * Common Helpers
 * ──────────────────────────────────────────────────────────────────────── */

static void abi_audio_copy_text(char* dest, size_t cap, const char* src) {
    if (!dest || cap == 0u) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    snprintf(dest, cap, "%s", src);
}

static int64_t abi_audio_ok(void) {
    g_last_status = ABI_AUDIO_OK;
    abi_audio_copy_text(g_last_error_kind, sizeof(g_last_error_kind), "ok");
    abi_audio_copy_text(g_last_error_message, sizeof(g_last_error_message), "");
    return ABI_AUDIO_OK;
}

static int64_t abi_audio_fail(int64_t status, const char* kind, const char* message) {
    g_last_status = status;
    abi_audio_copy_text(g_last_error_kind, sizeof(g_last_error_kind), kind ? kind : "error");
    abi_audio_copy_text(g_last_error_message, sizeof(g_last_error_message), message ? message : "");
    return status;
}

static KainNativeMidiInputSlot* abi_audio_midi_slot_locked(int64_t handle) {
    int i;
    if (handle <= 0) {
        return NULL;
    }
    for (i = 0; i < ABI_AUDIO_MAX_MIDI_INPUTS; i++) {
        if (g_midi_inputs[i].in_use && g_midi_inputs[i].id == handle) {
            return &g_midi_inputs[i];
        }
    }
    return NULL;
}

static int64_t abi_audio_ms_now(void) {
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
#endif
}

/* ─────────────────────────────────────────────────────────────────────────
 * Windows — WASAPI shared-mode + WinMM MIDI
 * ──────────────────────────────────────────────────────────────────────── */
#ifdef _WIN32

/* IIDs we need. We declare them locally to avoid pulling in <initguid.h>
   and to keep the surface stable across Windows SDK versions. */
static const GUID kain_iid_immdevice_enumerator = {
    0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}
};
static const GUID kain_iid_iaudio_client = {
    0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}
};
static const GUID kain_iid_iaudio_render_client = {
    0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}
};
static const GUID kain_class_immdevice_enumerator = {
    0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x0e, 0x13}
};
/* PKEY_Device_FriendlyName = {a45c254e-df1c-4efd-8020-67d146a850e0}, pid=14.
   We declare the PROPERTYKEY directly so the surface is stable across SDKs. */
static const PROPERTYKEY kain_pkey_device_friendly_name = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } },
    14u
};

/* Wide-string <-> int64 device id hash. We hash the device id to fit into
   the int64_t slot in KainNativeAudioDeviceInfo. The hash is deterministic
   so repeat calls return the same id. */
static int64_t abi_audio_win_hash_wide(const wchar_t* wide) {
    /* FNV-1a 64 on UTF-16 code units, low 63 bits to keep sign clear. */
    uint64_t h = 0xcbf29ce484222325ULL;
    if (!wide) {
        return 0;
    }
    while (*wide) {
        h ^= (uint64_t)(*wide & 0xFFFF);
        h *= 0x00000100000001B3ULL;
        wide++;
    }
    return (int64_t)(h & 0x7FFFFFFFFFFFFFFFu);
}

static void abi_audio_win_release_com(IAudioClient** client, IAudioRenderClient** render, IMMDevice** device) {
    if (render && *render) {
        (*render)->lpVtbl->Release(*render);
        *render = NULL;
    }
    if (client && *client) {
        (*client)->lpVtbl->Release(*client);
        *client = NULL;
    }
    if (device && *device) {
        (*device)->lpVtbl->Release(*device);
        *device = NULL;
    }
}

static int abi_audio_win_init_com(void) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (hr == S_OK || hr == S_FALSE) {
        return 0;
    }
    /* RPC_E_CHANGED_MODE means the apartment is already set. MTA is fine. */
    if (hr == RPC_E_CHANGED_MODE) {
        return 0;
    }
    return -1;
}

static int abi_audio_win_enumerate(KainNativeAudioDeviceInfo* devices, int64_t max_devices) {
    IMMDeviceEnumerator* enumerator = NULL;
    IMMDeviceCollection* collection = NULL;
    UINT count = 0;
    UINT i;
    int64_t written = 0;
    HRESULT hr;

    if (abi_audio_win_init_com() != 0) {
        abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "com_init", "CoInitializeEx failed");
        return 0;
    }

    hr = CoCreateInstance(
        &kain_class_immdevice_enumerator,
        NULL,
        CLSCTX_ALL,
        &kain_iid_immdevice_enumerator,
        (LPVOID*)&enumerator
    );
    if (FAILED(hr) || !enumerator) {
        abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "enumerator", "CoCreateInstance(IMMDeviceEnumerator) failed");
        return 0;
    }

    hr = enumerator->lpVtbl->EnumAudioEndpoints(enumerator, eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr) || !collection) {
        enumerator->lpVtbl->Release(enumerator);
        abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "enum_endpoints", "EnumAudioEndpoints failed");
        return 0;
    }
    hr = collection->lpVtbl->GetCount(collection, &count);
    if (FAILED(hr)) {
        count = 0;
    }

    for (i = 0; i < count && written < max_devices; i++) {
        IMMDevice* device = NULL;
        IPropertyStore* props = NULL;
        LPWSTR id_wide = NULL;
        PROPVARIANT name_var;
        if (collection->lpVtbl->Item(collection, i, &device) != S_OK || !device) {
            continue;
        }
        if (device->lpVtbl->GetId(device, &id_wide) != S_OK || !id_wide) {
            device->lpVtbl->Release(device);
            continue;
        }
        if (device->lpVtbl->OpenPropertyStore(device, STGM_READ, &props) != S_OK || !props) {
            CoTaskMemFree(id_wide);
            device->lpVtbl->Release(device);
            continue;
        }
        PropVariantInit(&name_var);
        if (props->lpVtbl->GetValue(props, &kain_pkey_device_friendly_name, &name_var) != S_OK) {
            name_var.vt = VT_EMPTY;
        }

        if (devices) {
            KainNativeAudioDeviceInfo* d = &devices[written];
            memset(d, 0, sizeof(*d));
            d->device_id = abi_audio_win_hash_wide(id_wide);
            if (name_var.vt == VT_LPWSTR && name_var.pwszVal) {
                WideCharToMultiByte(CP_UTF8, 0, name_var.pwszVal, -1,
                                    d->name, ABI_AUDIO_MAX_DEVICE_NAME - 1, NULL, NULL);
                d->name[ABI_AUDIO_MAX_DEVICE_NAME - 1] = '\0';
            } else {
                snprintf(d->name, sizeof(d->name), "WASAPI Device %u", (unsigned)i);
            }
            snprintf(d->api_name, sizeof(d->api_name), "wasapi");
            d->max_output_channels = 2; /* shared-mode is stereo; safe default */
            d->max_input_channels = 0;
            d->default_sample_rate = ABI_AUDIO_DEFAULT_SAMPLE_RATE;
            d->is_default = 0;
        }

        PropVariantClear(&name_var);
        props->lpVtbl->Release(props);
        CoTaskMemFree(id_wide);
        device->lpVtbl->Release(device);
        written += 1;
    }

    if (collection) collection->lpVtbl->Release(collection);
    if (enumerator) enumerator->lpVtbl->Release(enumerator);

    return written;
}

static int64_t abi_audio_win_default_device(KainNativeAudioDeviceInfo* out_device) {
    IMMDeviceEnumerator* enumerator = NULL;
    IMMDevice* device = NULL;
    IPropertyStore* props = NULL;
    LPWSTR id_wide = NULL;
    PROPVARIANT name_var;
    int64_t device_id = 0;
    if (!out_device) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out_device is null");
    }
    if (abi_audio_win_init_com() != 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "com_init", "CoInitializeEx failed");
    }
    if (CoCreateInstance(&kain_class_immdevice_enumerator, NULL, CLSCTX_ALL,
                         &kain_iid_immdevice_enumerator, (LPVOID*)&enumerator) != S_OK || !enumerator) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "enumerator", "CoCreateInstance failed");
    }
    if (enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device) != S_OK || !device) {
        enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "no default render device");
    }
    if (device->lpVtbl->GetId(device, &id_wide) != S_OK) {
        device->lpVtbl->Release(device);
        enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "device_id", "IMMDevice::GetId failed");
    }
    if (device->lpVtbl->OpenPropertyStore(device, STGM_READ, &props) != S_OK) {
        props = NULL;
    }
    PropVariantInit(&name_var);
    if (props) {
        props->lpVtbl->GetValue(props, &kain_pkey_device_friendly_name, &name_var);
    }
    memset(out_device, 0, sizeof(*out_device));
    out_device->device_id = abi_audio_win_hash_wide(id_wide);
    out_device->max_output_channels = 2;
    out_device->max_input_channels = 0;
    out_device->default_sample_rate = ABI_AUDIO_DEFAULT_SAMPLE_RATE;
    out_device->is_default = 1;
    if (name_var.vt == VT_LPWSTR && name_var.pwszVal) {
        WideCharToMultiByte(CP_UTF8, 0, name_var.pwszVal, -1,
                            out_device->name, ABI_AUDIO_MAX_DEVICE_NAME - 1, NULL, NULL);
        out_device->name[ABI_AUDIO_MAX_DEVICE_NAME - 1] = '\0';
    } else {
        snprintf(out_device->name, sizeof(out_device->name), "WASAPI Default");
    }
    snprintf(out_device->api_name, sizeof(out_device->api_name), "wasapi");
    PropVariantClear(&name_var);
    if (props) props->lpVtbl->Release(props);
    CoTaskMemFree(id_wide);
    device->lpVtbl->Release(device);
    enumerator->lpVtbl->Release(enumerator);
    device_id = out_device->device_id;
    abi_audio_ok();
    return device_id;
}

/* WASAPI audio thread — drives the user callback. */
static DWORD WINAPI abi_audio_win_thread_proc(LPVOID param) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)param;
    HRESULT hr;
    if (!s) {
        return 1;
    }
    /* Each thread that touches COM needs to be in the same apartment as the
       thread that created the IAudioClient. We initialized on the calling
       thread; the audio thread inherits. Calling CoInitializeEx here is
       safe — it returns S_FALSE if the apartment is already set. */
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    while (atomic_load_explicit(&s->should_stop, memory_order_acquire) == 0) {
        DWORD wait = WaitForSingleObject(s->event_handle, 1000);
        if (wait != WAIT_OBJECT_0) {
            /* Timeout or error — loop and re-check should_stop. */
            continue;
        }
        if (atomic_load_explicit(&s->should_stop, memory_order_acquire) != 0) {
            break;
        }
        if (!s->audio_client || !s->render_client) {
            break;
        }
        UINT32 padding_frames = 0;
        hr = s->audio_client->lpVtbl->GetCurrentPadding(s->audio_client, &padding_frames);
        if (FAILED(hr)) {
            continue;
        }
        UINT32 available = 0u;
        if ((UINT32)s->buffer_size_frames > padding_frames) {
            available = (UINT32)s->buffer_size_frames - padding_frames;
        }
        if (available == 0u) {
            continue;
        }
        BYTE* data = NULL;
        hr = s->render_client->lpVtbl->GetBuffer(s->render_client, available, &data);
        if (FAILED(hr) || !data) {
            continue;
        }
        /* Fill via the user callback. f32 interleaved. */
        if (s->callback) {
            s->callback(NULL, (float*)data, (int32_t)available, s->output_channels, s->user_data);
        } else {
            memset(data, 0, sizeof(float) * (size_t)available * (size_t)s->output_channels);
        }
        hr = s->render_client->lpVtbl->ReleaseBuffer(s->render_client, available, 0);
        if (FAILED(hr)) {
            /* Buffer underrun-like failure; loop continues. */
        }
    }

    CoUninitialize();
    atomic_store_explicit(&s->stop_completed, 1, memory_order_release);
    return 0;
}

static int64_t abi_audio_win_open_stream(
    int64_t device_id,
    int32_t sample_rate,
    int32_t buffer_size_frames,
    int32_t output_channels,
    int32_t input_channels,
    KainNativeAudioCallback callback,
    void* user_data,
    KainNativeAudioStreamSlot* slot
) {
    IMMDeviceEnumerator* enumerator = NULL;
    IMMDevice* device = NULL;
    IAudioClient* client = NULL;
    IAudioRenderClient* render = NULL;
    WAVEFORMATEX* mix = NULL;
    HRESULT hr;
    int64_t result;
    (void)device_id;
    (void)input_channels; /* Phase 1 is output-only. */

    if (output_channels <= 0) {
        output_channels = ABI_AUDIO_DEFAULT_CHANNELS;
    }
    if (output_channels > ABI_AUDIO_MAX_CHANNELS) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "channels",
                              "output_channels exceeds ABI_AUDIO_MAX_CHANNELS");
    }
    if (sample_rate <= 0) {
        sample_rate = ABI_AUDIO_DEFAULT_SAMPLE_RATE;
    }
    if (buffer_size_frames <= 0) {
        buffer_size_frames = ABI_AUDIO_DEFAULT_BUFFER_SIZE;
    }

    if (abi_audio_win_init_com() != 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "com_init", "CoInitializeEx failed");
    }

    hr = CoCreateInstance(&kain_class_immdevice_enumerator, NULL, CLSCTX_ALL,
                          &kain_iid_immdevice_enumerator, (LPVOID*)&enumerator);
    if (FAILED(hr) || !enumerator) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "enumerator", "CoCreateInstance failed");
    }
    /* Get the default render endpoint. Phase 1 ignores the device_id slot
       and always opens the default — device_id remains an API for future
       device-targeted streams. */
    hr = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device);
    if (FAILED(hr) || !device) {
        enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "no default render endpoint");
    }
    hr = device->lpVtbl->Activate(device, &kain_iid_iaudio_client, CLSCTX_ALL, NULL, (LPVOID*)&client);
    if (FAILED(hr) || !client) {
        device->lpVtbl->Release(device);
        enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "audio_client", "IMMDevice::Activate failed");
    }
    hr = client->lpVtbl->GetMixFormat(client, &mix);
    if (FAILED(hr) || !mix) {
        abi_audio_win_release_com(&client, &render, &device);
        if (enumerator) enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "mix_format", "IAudioClient::GetMixFormat failed");
    }
    /* Demand f32. WASAPI mix format is usually WAVE_FORMAT_IEEE_FLOAT. */
    if (mix->wFormatTag != WAVE_FORMAT_IEEE_FLOAT || mix->wBitsPerSample != 32) {
        CoTaskMemFree(mix);
        abi_audio_win_release_com(&client, &render, &device);
        if (enumerator) enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_UNSUPPORTED_FMT, "format",
                              "device mix format is not IEEE float 32-bit");
    }
    /* Initialize in shared mode with event callbacks. */
    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    REFERENCE_TIME buffer_duration = (REFERENCE_TIME)((10000000LL * (LONGLONG)buffer_size_frames) / (LONGLONG)sample_rate);
    hr = client->lpVtbl->Initialize(client,
                                    AUDCLNT_SHAREMODE_SHARED,
                                    flags,
                                    buffer_duration,
                                    0,
                                    mix,
                                    NULL);
    CoTaskMemFree(mix);
    if (FAILED(hr)) {
        abi_audio_win_release_com(&client, &render, &device);
        if (enumerator) enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "initialize", "IAudioClient::Initialize failed");
    }
    UINT32 actual_buffer_frames = 0;
    hr = client->lpVtbl->GetBufferSize(client, &actual_buffer_frames);
    if (FAILED(hr) || actual_buffer_frames == 0u) {
        abi_audio_win_release_com(&client, &render, &device);
        if (enumerator) enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "buffer_size", "IAudioClient::GetBufferSize failed");
    }
    hr = client->lpVtbl->GetService(client, &kain_iid_iaudio_render_client, (LPVOID*)&render);
    if (FAILED(hr) || !render) {
        abi_audio_win_release_com(&client, &render, &device);
        if (enumerator) enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "render_client", "GetService(IAudioRenderClient) failed");
    }
    HANDLE event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!event_handle) {
        abi_audio_win_release_com(&client, &render, &device);
        if (enumerator) enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "event", "CreateEvent failed");
    }
    hr = client->lpVtbl->SetEventHandle(client, event_handle);
    if (FAILED(hr)) {
        CloseHandle(event_handle);
        abi_audio_win_release_com(&client, &render, &device);
        if (enumerator) enumerator->lpVtbl->Release(enumerator);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "set_event", "SetEventHandle failed");
    }
    if (enumerator) enumerator->lpVtbl->Release(enumerator);
    enumerator = NULL;
    device->lpVtbl->Release(device);
    device = NULL;

    slot->in_use = 1;
    slot->id = g_next_stream_id++;
    slot->sample_rate = sample_rate;
    slot->buffer_size_frames = (int32_t)actual_buffer_frames;
    slot->output_channels = output_channels;
    slot->input_channels = 0;
    slot->callback = callback;
    slot->user_data = user_data;
    slot->audio_client = client;
    slot->render_client = render;
    slot->event_handle = event_handle;
    atomic_store(&slot->is_running, 0);
    atomic_store(&slot->should_stop, 0);
    atomic_store(&slot->stop_completed, 0);
    slot->scratch = NULL;
    slot->scratch_frames = 0;
    slot->thread_handle = NULL;
    slot->thread_id = 0;

    result = (int64_t)slot->id;
    abi_audio_ok();
    return result;
}

static int64_t abi_audio_win_start_stream(KainNativeAudioStreamSlot* s) {
    if (!s || !s->audio_client) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    if (atomic_load(&s->is_running) != 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_STREAM_ACTIVE, "active", "stream is already running");
    }
    atomic_store(&s->should_stop, 0);
    atomic_store(&s->stop_completed, 0);
    s->thread_handle = CreateThread(NULL, 0, abi_audio_win_thread_proc, s, 0, &s->thread_id);
    if (!s->thread_handle) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "thread", "CreateThread failed");
    }
    HRESULT hr = s->audio_client->lpVtbl->Start(s->audio_client);
    if (FAILED(hr)) {
        atomic_store(&s->should_stop, 1);
        WaitForSingleObject(s->thread_handle, INFINITE);
        CloseHandle(s->thread_handle);
        s->thread_handle = NULL;
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "start", "IAudioClient::Start failed");
    }
    atomic_store(&s->is_running, 1);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

static int64_t abi_audio_win_stop_stream(KainNativeAudioStreamSlot* s) {
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    if (atomic_load(&s->is_running) == 0) {
        return abi_audio_ok();
    }
    atomic_store(&s->should_stop, 1);
    if (s->audio_client) {
        s->audio_client->lpVtbl->Stop(s->audio_client);
        s->audio_client->lpVtbl->Reset(s->audio_client);
    }
    if (s->thread_handle) {
        WaitForSingleObject(s->thread_handle, 5000);
        CloseHandle(s->thread_handle);
        s->thread_handle = NULL;
    }
    atomic_store(&s->is_running, 0);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

static int64_t abi_audio_win_close_stream(KainNativeAudioStreamSlot* s) {
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    if (atomic_load(&s->is_running) != 0) {
        abi_audio_win_stop_stream(s);
    }
    if (s->audio_client) {
        s->audio_client->lpVtbl->Release(s->audio_client);
        s->audio_client = NULL;
    }
    if (s->render_client) {
        s->render_client->lpVtbl->Release(s->render_client);
        s->render_client = NULL;
    }
    if (s->event_handle) {
        CloseHandle(s->event_handle);
        s->event_handle = NULL;
    }
    s->callback = NULL;
    s->user_data = NULL;
    s->in_use = 0;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

/* ── WinMM MIDI ───────────────────────────────────────────────────────── */

static int64_t abi_audio_win_midi_count(void) {
    return (int64_t)midiInGetNumDevs();
}

static int64_t abi_audio_win_midi_name(int64_t device_id, char* out, int64_t cap) {
    MIDIINCAPSA caps;
    MMRESULT r;
    if (!out || cap <= 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out buffer is null");
    }
    r = midiInGetDevCapsA((UINT)device_id, &caps, sizeof(caps));
    if (r != MMSYSERR_NOERROR) {
        return abi_audio_fail(ABI_AUDIO_ERR_MIDI_NO_DEVICE, "no_device", "midiInGetDevCaps failed");
    }
    snprintf(out, (size_t)cap, "%s", caps.szPname);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

/* WinMM callback runs on the OS MIDI thread. We translate the DWORD into
   a KainNativeMidiEvent and forward to the user's KainNativeMidiCallback. */
static void CALLBACK abi_audio_win_midi_proc(
    HMIDIIN hMidiIn,
    UINT wMsg,
    DWORD_PTR dwInstance,
    DWORD_PTR dwParam1,
    DWORD_PTR dwParam2
) {
    KainNativeMidiInputSlot* slot = (KainNativeMidiInputSlot*)dwInstance;
    KainNativeMidiEvent event;
    (void)hMidiIn;
    (void)dwParam2;
    if (!slot || wMsg != MIM_DATA) {
        return;
    }
    /* dwParam1 is the MIDI short message packed little-endian. */
    DWORD packed = (DWORD)dwParam1;
    event.timestamp_ms = abi_audio_ms_now();
    event.status = (uint8_t)(packed & 0xFFu);
    event.data1  = (uint8_t)((packed >> 8)  & 0xFFu);
    event.data2  = (uint8_t)((packed >> 16) & 0xFFu);
    if (slot->callback) {
        slot->callback(&event, slot->user_data);
    }
}

static int64_t abi_audio_win_midi_open(
    int64_t device_id,
    KainNativeMidiCallback callback,
    void* user_data,
    KainNativeMidiInputSlot* slot
) {
    MMRESULT r = midiInOpen(&slot->handle, (UINT)device_id,
                            (DWORD_PTR)abi_audio_win_midi_proc,
                            (DWORD_PTR)slot,
                            CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "midi_open", "midiInOpen failed");
    }
    r = midiInStart(slot->handle);
    if (r != MMSYSERR_NOERROR) {
        midiInClose(slot->handle);
        slot->handle = NULL;
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "midi_start", "midiInStart failed");
    }
    slot->in_use = 1;
    slot->id = g_next_midi_input_id++;
    slot->callback = callback;
    slot->user_data = user_data;
    slot->device_index = (int)device_id;
    abi_audio_ok();
    return slot->id;
}

static int64_t abi_audio_win_midi_close(KainNativeMidiInputSlot* slot) {
    if (!slot || !slot->handle) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "midi handle is null");
    }
    midiInStop(slot->handle);
    midiInReset(slot->handle);
    midiInClose(slot->handle);
    slot->handle = NULL;
    slot->in_use = 0;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

#endif /* _WIN32 */

/* ─────────────────────────────────────────────────────────────────────────
 * macOS — CoreAudio AudioUnit + CoreMIDI
 * ──────────────────────────────────────────────────────────────────────── */
#ifdef __APPLE__

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>

static int64_t abi_audio_mac_enumerate(KainNativeAudioDeviceInfo* devices, int64_t max_devices) {
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    UInt32 data_size = 0;
    OSStatus s = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, NULL, &data_size);
    if (s != noErr || data_size == 0) {
        return 0;
    }
    int device_count = (int)(data_size / sizeof(AudioDeviceID));
    if (device_count <= 0 || device_count > (int)max_devices) {
        device_count = (int)max_devices;
    }
    AudioDeviceID ids[ABI_AUDIO_MAX_DEVICES];
    UInt32 fetched = sizeof(ids);
    s = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &fetched, ids);
    if (s != noErr) {
        return 0;
    }
    int written = 0;
    for (int i = 0; i < device_count && i < ABI_AUDIO_MAX_DEVICES; i++) {
        AudioObjectPropertyAddress name_addr = {
            kAudioDevicePropertyDeviceName,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        CFStringRef name = NULL;
        UInt32 name_size = sizeof(name);
        s = AudioObjectGetPropertyData(ids[i], &name_addr, 0, NULL, &name_size, &name);
        if (s != noErr || !name) {
            continue;
        }
        if (devices) {
            KainNativeAudioDeviceInfo* d = &devices[written];
            memset(d, 0, sizeof(*d));
            d->device_id = (int64_t)ids[i];
            if (CFStringGetCString(name, d->name, sizeof(d->name), kCFStringEncodingUTF8)) {
                /* ok */
            } else {
                snprintf(d->name, sizeof(d->name), "CoreAudio Device %d", i);
            }
            snprintf(d->api_name, sizeof(d->api_name), "coreaudio");
            d->max_output_channels = 2;
            d->max_input_channels = 0;
            d->default_sample_rate = ABI_AUDIO_DEFAULT_SAMPLE_RATE;
            d->is_default = 0;
        }
        CFRelease(name);
        written += 1;
    }
    return written;
}

static int64_t abi_audio_mac_default_device(KainNativeAudioDeviceInfo* out_device) {
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    AudioDeviceID id = kAudioDeviceUnknown;
    UInt32 size = sizeof(id);
    OSStatus s = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, &id);
    if (s != noErr || id == kAudioDeviceUnknown) {
        return abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "no default output device");
    }
    if (!out_device) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out_device is null");
    }
    memset(out_device, 0, sizeof(*out_device));
    out_device->device_id = (int64_t)id;
    out_device->max_output_channels = 2;
    out_device->max_input_channels = 0;
    out_device->default_sample_rate = ABI_AUDIO_DEFAULT_SAMPLE_RATE;
    out_device->is_default = 1;
    snprintf(out_device->api_name, sizeof(out_device->api_name), "coreaudio");
    AudioObjectPropertyAddress name_addr = {
        kAudioDevicePropertyDeviceName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    CFStringRef name = NULL;
    UInt32 name_size = sizeof(name);
    if (AudioObjectGetPropertyData(id, &name_addr, 0, NULL, &name_size, &name) == noErr && name) {
        CFStringGetCString(name, out_device->name, sizeof(out_device->name), kCFStringEncodingUTF8);
        CFRelease(name);
    } else {
        snprintf(out_device->name, sizeof(out_device->name), "CoreAudio Default");
    }
    abi_audio_ok();
    return (int64_t)id;
}

static OSStatus abi_audio_mac_render_proc(
    AudioUnitRef inUnit,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData
) {
    KainNativeAudioStreamSlot* s = NULL;
    OSStatus s2;
    (void)inUnit;
    (void)ioActionFlags;
    (void)inTimeStamp;
    (void)inBusNumber;
    s2 = AudioUnitGetProperty(inUnit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &s, NULL);
    if (s2 != noErr || !s) {
        return noErr;
    }
    if (!ioData || ioData->mNumberBuffers == 0) {
        return noErr;
    }
    int32_t channels = s->output_channels;
    int32_t frames = (int32_t)inNumberFrames;
    float* out = (float*)ioData->mBuffers[0].mData;
    if (!out) {
        return noErr;
    }
    if (s->callback) {
        s->callback(NULL, out, frames, channels, s->user_data);
    } else {
        memset(out, 0, sizeof(float) * (size_t)frames * (size_t)channels);
    }
    return noErr;
}

static int64_t abi_audio_mac_open_stream(
    int64_t device_id,
    int32_t sample_rate,
    int32_t buffer_size_frames,
    int32_t output_channels,
    int32_t input_channels,
    KainNativeAudioCallback callback,
    void* user_data,
    KainNativeAudioStreamSlot* slot
) {
    AudioComponentDescription desc = {
        kAudioUnitType_Output,
        kAudioUnitSubType_HALOutput,
        kAudioUnitManufacturer_Apple,
        0, 0
    };
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (!comp) {
        return abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "AudioComponentFindNext failed");
    }
    if (AudioComponentInstanceNew(comp, &slot->audio_unit) != noErr || !slot->audio_unit) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "instance", "AudioComponentInstanceNew failed");
    }
    AudioStreamBasicDescription asbd;
    memset(&asbd, 0, sizeof(asbd));
    asbd.mSampleRate = (Float64)(sample_rate > 0 ? sample_rate : ABI_AUDIO_DEFAULT_SAMPLE_RATE);
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    asbd.mBitsPerChannel = 32;
    asbd.mChannelsPerFrame = (UInt32)(output_channels > 0 ? output_channels : ABI_AUDIO_DEFAULT_CHANNELS);
    asbd.mFramesPerPacket = 1;
    asbd.mBytesPerFrame = asbd.mChannelsPerFrame * 4u;
    asbd.mBytesPerPacket = asbd.mBytesPerFrame;
    if (AudioUnitSetProperty(slot->audio_unit, kAudioUnitProperty_StreamFormat,
                              kAudioUnitScope_Input, 0, &asbd, sizeof(asbd)) != noErr) {
        AudioComponentInstanceDispose(slot->audio_unit);
        slot->audio_unit = NULL;
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "stream_format", "AudioUnitSetProperty(StreamFormat) failed");
    }
    AURenderCallbackStruct cb = { abi_audio_mac_render_proc, slot };
    if (AudioUnitSetProperty(slot->audio_unit, kAudioUnitProperty_SetRenderCallback,
                              kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr) {
        AudioComponentInstanceDispose(slot->audio_unit);
        slot->audio_unit = NULL;
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "render_callback", "SetRenderCallback failed");
    }
    if (AudioUnitInitialize(slot->audio_unit) != noErr) {
        AudioComponentInstanceDispose(slot->audio_unit);
        slot->audio_unit = NULL;
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "initialize", "AudioUnitInitialize failed");
    }
    slot->in_use = 1;
    slot->id = g_next_stream_id++;
    slot->sample_rate = (int32_t)asbd.mSampleRate;
    slot->buffer_size_frames = buffer_size_frames > 0 ? buffer_size_frames : ABI_AUDIO_DEFAULT_BUFFER_SIZE;
    slot->output_channels = (int32_t)asbd.mChannelsPerFrame;
    slot->input_channels = 0; /* Phase 1 output-only */
    slot->callback = callback;
    slot->user_data = user_data;
    slot->thread_started = 0;
    slot->scratch = NULL;
    slot->scratch_frames = 0;
    slot->scratch_channels = slot->output_channels;
    atomic_store(&slot->is_running, 0);
    atomic_store(&slot->should_stop, 0);
    atomic_store(&slot->stop_completed, 0);
    atomic_store(&slot->callback_inflight, 0);
    (void)device_id;
    (void)input_channels;
    abi_audio_ok();
    return slot->id;
}

static int64_t abi_audio_mac_start_stream(KainNativeAudioStreamSlot* s) {
    if (!s || !s->audio_unit) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    if (atomic_load(&s->is_running) != 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_STREAM_ACTIVE, "active", "stream already running");
    }
    if (AudioOutputUnitStart(s->audio_unit) != noErr) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "start", "AudioOutputUnitStart failed");
    }
    atomic_store(&s->is_running, 1);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

static int64_t abi_audio_mac_stop_stream(KainNativeAudioStreamSlot* s) {
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    if (atomic_load(&s->is_running) == 0) {
        return abi_audio_ok();
    }
    if (s->audio_unit) {
        AudioOutputUnitStop(s->audio_unit);
    }
    atomic_store(&s->is_running, 0);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

static int64_t abi_audio_mac_close_stream(KainNativeAudioStreamSlot* s) {
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    abi_audio_mac_stop_stream(s);
    if (s->audio_unit) {
        AudioUnitUninitialize(s->audio_unit);
        AudioComponentInstanceDispose(s->audio_unit);
        s->audio_unit = NULL;
    }
    if (s->scratch) {
        free(s->scratch);
        s->scratch = NULL;
    }
    s->callback = NULL;
    s->user_data = NULL;
    s->in_use = 0;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

/* ── CoreMIDI ─────────────────────────────────────────────────────────── */

static int64_t abi_audio_mac_midi_count(void) {
    return (int64_t)MIDIGetNumberOfSources();
}

static int64_t abi_audio_mac_midi_name(int64_t device_id, char* out, int64_t cap) {
    MIDIEndpointRef src = (MIDIEndpointRef)device_id;
    if (!out || cap <= 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out is null");
    }
    CFStringRef name = NULL;
    OSStatus s = MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &name);
    if (s != noErr || !name) {
        return abi_audio_fail(ABI_AUDIO_ERR_MIDI_NO_DEVICE, "no_device", "MIDIObjectGetStringProperty failed");
    }
    if (!CFStringGetCString(name, out, (CFIndex)cap, kCFStringEncodingUTF8)) {
        snprintf(out, (size_t)cap, "MIDI Device %lld", (long long)device_id);
    }
    CFRelease(name);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

static void abi_audio_mac_midi_read_proc(const MIDIPacketList* pktlist, void* readProcRefCon, void* srcConnRefCon) {
    KainNativeMidiInputSlot* slot = (KainNativeMidiInputSlot*)readProcRefCon;
    const MIDIPacket* pkt;
    int i;
    int j;
    (void)srcConnRefCon;
    if (!slot || !slot->callback) {
        return;
    }
    pkt = pktlist->packet;
    for (i = 0; i < pktlist->numPackets; i++) {
        const UInt8* data = pkt->data;
        UInt16 length = pkt->length;
        for (j = 0; j + 1 < length; j += 2) {
            KainNativeMidiEvent ev;
            ev.timestamp_ms = abi_audio_ms_now();
            ev.status = data[j];
            ev.data1  = data[j + 1];
            ev.data2  = (j + 2 < length) ? data[j + 2] : 0;
            slot->callback(&ev, slot->user_data);
        }
        pkt = (const MIDIPacket*)(((const UInt8*)pkt) + sizeof(MIDIPacket) + pkt->length);
    }
}

static int64_t abi_audio_mac_midi_open(
    int64_t device_id,
    KainNativeMidiCallback callback,
    void* user_data,
    KainNativeMidiInputSlot* slot
) {
    CFStringRef client_name = CFStringCreateWithCString(NULL, "KainAudio", kCFStringEncodingUTF8);
    OSStatus s = MIDIClientCreate(client_name, NULL, NULL, &slot->client);
    if (client_name) CFRelease(client_name);
    if (s != noErr) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "midi_client", "MIDIClientCreate failed");
    }
    CFStringRef port_name = CFStringCreateWithCString(NULL, "Input", kCFStringEncodingUTF8);
    s = MIDIInputPortCreate(slot->client, port_name, abi_audio_mac_midi_read_proc, slot, &slot->port);
    if (port_name) CFRelease(port_name);
    if (s != noErr) {
        MIDIClientDispose(slot->client);
        slot->client = 0;
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "midi_port", "MIDIInputPortCreate failed");
    }
    slot->endpoint = (MIDIEndpointRef)device_id;
    s = MIDIPortConnectSource(slot->port, slot->endpoint, NULL);
    if (s != noErr) {
        MIDIPortDispose(slot->port);
        MIDIClientDispose(slot->client);
        slot->port = 0;
        slot->client = 0;
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "midi_connect", "MIDIPortConnectSource failed");
    }
    slot->in_use = 1;
    slot->id = g_next_midi_input_id++;
    slot->callback = callback;
    slot->user_data = user_data;
    slot->device_index = (int)device_id;
    abi_audio_ok();
    return slot->id;
}

static int64_t abi_audio_mac_midi_close(KainNativeMidiInputSlot* slot) {
    if (!slot) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "midi slot is null");
    }
    if (slot->port) {
        if (slot->endpoint) {
            MIDIPortDisconnectSource(slot->port, slot->endpoint);
        }
        MIDIPortDispose(slot->port);
        slot->port = 0;
    }
    if (slot->client) {
        MIDIClientDispose(slot->client);
        slot->client = 0;
    }
    slot->endpoint = 0;
    slot->in_use = 0;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

#endif /* __APPLE__ */

/* ─────────────────────────────────────────────────────────────────────────
 * Linux — ALSA PCM + ALSA raw MIDI
 * ──────────────────────────────────────────────────────────────────────── */
#ifdef __linux__

#include <alsa/asoundlib.h>

/* Enumerate playback PCM devices. We probe cards 0..7 with device 0.
   This is a Phase 1 simplification — a full enumeration would walk
   snd_card_next() + snd_ctl_open() per card. */
static int64_t abi_audio_linux_enumerate(KainNativeAudioDeviceInfo* devices, int64_t max_devices) {
    int card = -1;
    int written = 0;
    if (snd_card_next(&card) < 0 || card < 0) {
        return 0;
    }
    while (card >= 0 && written < max_devices && written < ABI_AUDIO_MAX_DEVICES) {
        snd_ctl_t* ctl = NULL;
        char ctl_name[32];
        snprintf(ctl_name, sizeof(ctl_name), "hw:%d", card);
        if (snd_ctl_open(&ctl, ctl_name, 0) == 0 && ctl) {
            snd_ctl_card_info_t* info = NULL;
            snd_ctl_card_info_alloca(&info);
            if (snd_ctl_card_info(ctl, info) == 0 && devices) {
                KainNativeAudioDeviceInfo* d = &devices[written];
                memset(d, 0, sizeof(*d));
                d->device_id = (int64_t)card;
                snprintf(d->name, sizeof(d->name), "ALSA: %s",
                         snd_ctl_card_info_get_name(info));
                snprintf(d->api_name, sizeof(d->api_name), "alsa");
                d->max_output_channels = 2;
                d->max_input_channels = 0;
                d->default_sample_rate = ABI_AUDIO_DEFAULT_SAMPLE_RATE;
                d->is_default = (card == 0) ? 1 : 0;
            }
            snd_ctl_close(ctl);
            written += 1;
        }
        if (snd_card_next(&card) < 0) {
            break;
        }
    }
    return written;
}

static int64_t abi_audio_linux_default_device(KainNativeAudioDeviceInfo* out_device) {
    if (!out_device) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out_device is null");
    }
    memset(out_device, 0, sizeof(*out_device));
    out_device->device_id = 0;
    out_device->max_output_channels = 2;
    out_device->max_input_channels = 0;
    out_device->default_sample_rate = ABI_AUDIO_DEFAULT_SAMPLE_RATE;
    out_device->is_default = 1;
    snprintf(out_device->api_name, sizeof(out_device->api_name), "alsa");
    snprintf(out_device->name, sizeof(out_device->name), "ALSA Default");
    abi_audio_ok();
    return 0;
}

static void* abi_audio_linux_thread_proc(void* arg) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)arg;
    if (!s) {
        return NULL;
    }
    while (atomic_load_explicit(&s->should_stop, memory_order_acquire) == 0) {
        if (s->poll_fds && s->poll_fd_count > 0) {
            /* Wait briefly for the PCM to be writable. */
            int r = poll(s->poll_fds, (nfds_t)s->poll_fd_count, 100);
            if (r < 0) {
                if (errno == EINTR) continue;
                break;
            }
        } else {
            /* Fall back to a coarse sleep. */
            struct timespec ts = { 0, 1000000 }; /* 1ms */
            nanosleep(&ts, NULL);
        }
        if (atomic_load_explicit(&s->should_stop, memory_order_acquire) != 0) {
            break;
        }
        if (!s->scratch || s->scratch_frames <= 0) {
            break;
        }
        if (s->callback) {
            s->callback(NULL, s->scratch, s->scratch_frames, s->scratch_channels, s->user_data);
        } else {
            memset(s->scratch, 0, sizeof(float) * (size_t)s->scratch_frames * (size_t)s->scratch_channels);
        }
        snd_pcm_sframes_t written = snd_pcm_writei(s->pcm, s->scratch, (snd_pcm_uframes_t)s->scratch_frames);
        if (written < 0) {
            if (written == -EPIPE) {
                snd_pcm_prepare(s->pcm);
            } else if (written == -EAGAIN) {
                continue;
            } else {
                break;
            }
        }
    }
    atomic_store_explicit(&s->stop_completed, 1, memory_order_release);
    return NULL;
}

static int64_t abi_audio_linux_open_stream(
    int64_t device_id,
    int32_t sample_rate,
    int32_t buffer_size_frames,
    int32_t output_channels,
    int32_t input_channels,
    KainNativeAudioCallback callback,
    void* user_data,
    KainNativeAudioStreamSlot* slot
) {
    char pcm_name[32];
    snd_pcm_hw_params_t* hw = NULL;
    snd_pcm_sw_params_t* sw = NULL;
    int err;
    snd_pcm_uframes_t period = (snd_pcm_uframes_t)(buffer_size_frames > 0 ? buffer_size_frames : ABI_AUDIO_DEFAULT_BUFFER_SIZE);
    unsigned int rate;
    (void)input_channels;

    if (output_channels <= 0) output_channels = ABI_AUDIO_DEFAULT_CHANNELS;
    if (sample_rate <= 0) sample_rate = ABI_AUDIO_DEFAULT_SAMPLE_RATE;

    snprintf(pcm_name, sizeof(pcm_name), "hw:%ld,0", (long)device_id);
    err = snd_pcm_open(&slot->pcm, pcm_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", snd_strerror(err));
    }
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(slot->pcm, hw);
    snd_pcm_hw_params_set_access(slot->pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(slot->pcm, hw, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_hw_params_set_channels(slot->pcm, hw, (unsigned int)output_channels);
    rate = (unsigned int)sample_rate;
    snd_pcm_hw_params_set_rate_near(slot->pcm, hw, &rate, NULL);
    snd_pcm_hw_params_set_period_size_near(slot->pcm, hw, &period, NULL);
    snd_pcm_hw_params(slot->pcm, hw);

    snd_pcm_sw_params_alloca(&sw);
    snd_pcm_sw_params_current(slot->pcm, sw);
    snd_pcm_sw_params_set_start_threshold(slot->pcm, sw, period);
    snd_pcm_sw_params(slot->pcm, sw);

    slot->period_size = period;
    slot->scratch = (float*)calloc((size_t)period * (size_t)output_channels, sizeof(float));
    if (!slot->scratch) {
        snd_pcm_close(slot->pcm);
        slot->pcm = NULL;
        return abi_audio_fail(ABI_AUDIO_ERR_OUT_OF_MEMORY, "oom", "scratch buffer alloc failed");
    }
    slot->scratch_frames = (int32_t)period;
    slot->scratch_channels = output_channels;

    slot->poll_fd_count = snd_pcm_poll_descriptors_count(slot->pcm);
    if (slot->poll_fd_count > 0) {
        slot->poll_fds = (struct pollfd*)calloc((size_t)slot->poll_fd_count, sizeof(struct pollfd));
        if (slot->poll_fds) {
            snd_pcm_poll_descriptors(slot->pcm, slot->poll_fds, (unsigned int)slot->poll_fd_count);
        } else {
            slot->poll_fd_count = 0;
        }
    }

    slot->in_use = 1;
    slot->id = g_next_stream_id++;
    slot->sample_rate = (int32_t)rate;
    slot->buffer_size_frames = (int32_t)period;
    slot->output_channels = output_channels;
    slot->input_channels = 0;
    slot->callback = callback;
    slot->user_data = user_data;
    slot->thread_started = 0;
    atomic_store(&slot->is_running, 0);
    atomic_store(&slot->should_stop, 0);
    atomic_store(&slot->stop_completed, 0);
    abi_audio_ok();
    return slot->id;
}

static int64_t abi_audio_linux_start_stream(KainNativeAudioStreamSlot* s) {
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    if (atomic_load(&s->is_running) != 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_STREAM_ACTIVE, "active", "stream already running");
    }
    atomic_store(&s->should_stop, 0);
    atomic_store(&s->stop_completed, 0);
    if (pthread_create(&s->thread, NULL, abi_audio_linux_thread_proc, s) != 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "thread", "pthread_create failed");
    }
    s->thread_started = 1;
    atomic_store(&s->is_running, 1);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

static int64_t abi_audio_linux_stop_stream(KainNativeAudioStreamSlot* s) {
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    if (atomic_load(&s->is_running) == 0) {
        return abi_audio_ok();
    }
    atomic_store(&s->should_stop, 1);
    if (s->thread_started) {
        pthread_join(s->thread, NULL);
        s->thread_started = 0;
    }
    if (s->pcm) {
        snd_pcm_drain(s->pcm);
    }
    atomic_store(&s->is_running, 0);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

static int64_t abi_audio_linux_close_stream(KainNativeAudioStreamSlot* s) {
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
    abi_audio_linux_stop_stream(s);
    if (s->pcm) {
        snd_pcm_close(s->pcm);
        s->pcm = NULL;
    }
    if (s->scratch) {
        free(s->scratch);
        s->scratch = NULL;
    }
    if (s->poll_fds) {
        free(s->poll_fds);
        s->poll_fds = NULL;
    }
    s->poll_fd_count = 0;
    s->callback = NULL;
    s->user_data = NULL;
    s->in_use = 0;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

/* ── ALSA raw MIDI ─────────────────────────────────────────────────────── */

static int64_t abi_audio_linux_midi_count(void) {
    int card = -1;
    int count = 0;
    if (snd_card_next(&card) < 0) return 0;
    while (card >= 0 && count < ABI_AUDIO_MAX_MIDI_DEVICES) {
        snd_ctl_t* ctl = NULL;
        char name[32];
        snprintf(name, sizeof(name), "hw:%d", card);
        if (snd_ctl_open(&ctl, name, 0) == 0 && ctl) {
            snd_rawmidi_info_t* info = NULL;
            snd_rawmidi_info_alloca(&info);
            snd_rawmidi_info_set_device(info, 0);
            snd_rawmidi_info_set_subdevice(info, 0);
            snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
            if (snd_ctl_rawmidi_info(ctl, info) >= 0) {
                count += 1;
            }
            snd_ctl_close(ctl);
        }
        if (snd_card_next(&card) < 0) break;
    }
    return count;
}

static int64_t abi_audio_linux_midi_name(int64_t device_id, char* out, int64_t cap) {
    snd_ctl_t* ctl = NULL;
    char name[32];
    if (!out || cap <= 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out is null");
    }
    snprintf(name, sizeof(name), "hw:%ld", (long)device_id);
    if (snd_ctl_open(&ctl, name, 0) < 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_MIDI_NO_DEVICE, "no_device", "snd_ctl_open failed");
    }
    snd_ctl_card_info_t* info = NULL;
    snd_ctl_card_info_alloca(&info);
    if (snd_ctl_card_info(ctl, info) < 0) {
        snd_ctl_close(ctl);
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "card_info", "snd_ctl_card_info failed");
    }
    snprintf(out, (size_t)cap, "ALSA MIDI: %s", snd_ctl_card_info_get_name(info));
    snd_ctl_close(ctl);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

static void* abi_audio_linux_midi_thread_proc(void* arg) {
    KainNativeMidiInputSlot* slot = (KainNativeMidiInputSlot*)arg;
    unsigned char buf[4];
    if (!slot || !slot->midi_in) {
        return NULL;
    }
    while (!slot->shutdown) {
        int n = snd_rawmidi_read(slot->midi_in, buf, sizeof(buf));
        if (n <= 0) {
            continue;
        }
        KainNativeMidiEvent ev;
        ev.timestamp_ms = abi_audio_ms_now();
        ev.status = buf[0];
        ev.data1  = (n >= 2) ? buf[1] : 0;
        ev.data2  = (n >= 3) ? buf[2] : 0;
        if (slot->callback) {
            slot->callback(&ev, slot->user_data);
        }
    }
    return NULL;
}

static int64_t abi_audio_linux_midi_open(
    int64_t device_id,
    KainNativeMidiCallback callback,
    void* user_data,
    KainNativeMidiInputSlot* slot
) {
    char name[32];
    int err;
    snprintf(name, sizeof(name), "hw:%ld,0", (long)device_id);
    err = snd_rawmidi_open(&slot->midi_in, NULL, name, SND_RAWMIDI_READ);
    if (err < 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_MIDI_NO_DEVICE, "no_device", snd_strerror(err));
    }
    slot->in_use = 1;
    slot->id = g_next_midi_input_id++;
    slot->callback = callback;
    slot->user_data = user_data;
    slot->device_index = (int)device_id;
    slot->shutdown = 0;
    if (pthread_create(&slot->thread, NULL, abi_audio_linux_midi_thread_proc, slot) != 0) {
        snd_rawmidi_close(slot->midi_in);
        slot->midi_in = NULL;
        slot->in_use = 0;
        return abi_audio_fail(ABI_AUDIO_ERR_PLATFORM, "thread", "pthread_create failed");
    }
    slot->thread_started = 1;
    abi_audio_ok();
    return slot->id;
}

static int64_t abi_audio_linux_midi_close(KainNativeMidiInputSlot* slot) {
    if (!slot) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "midi slot is null");
    }
    if (slot->midi_in) {
        slot->shutdown = 1;
        if (slot->thread_started) {
            pthread_join(slot->thread, NULL);
            slot->thread_started = 0;
        }
        snd_rawmidi_close(slot->midi_in);
        slot->midi_in = NULL;
    }
    slot->in_use = 0;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

#endif /* __linux__ */

/* ─────────────────────────────────────────────────────────────────────────
 * Public API — platform-agnostic dispatch
 * ──────────────────────────────────────────────────────────────────────── */

int64_t abi_audio_device_count(void) {
#ifdef _WIN32
    int n = abi_audio_win_enumerate(NULL, 0);
    abi_audio_ok();
    return (int64_t)n;
#elif defined(__APPLE__)
    int64_t n = abi_audio_mac_enumerate(NULL, 0);
    abi_audio_ok();
    return n;
#elif defined(__linux__)
    int64_t n = abi_audio_linux_enumerate(NULL, 0);
    abi_audio_ok();
    return n;
#else
    abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "audio not supported on this platform");
    return 0;
#endif
}

int64_t abi_audio_enumerate_devices(KainNativeAudioDeviceInfo* devices, int64_t max_devices) {
    if (!devices || max_devices <= 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "devices buffer is null");
    }
#ifdef _WIN32
    return (int64_t)abi_audio_win_enumerate(devices, max_devices);
#elif defined(__APPLE__)
    return abi_audio_mac_enumerate(devices, max_devices);
#elif defined(__linux__)
    return abi_audio_linux_enumerate(devices, max_devices);
#else
    (void)devices;
    (void)max_devices;
    abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "audio not supported on this platform");
    return 0;
#endif
}

int64_t abi_audio_default_output_device(KainNativeAudioDeviceInfo* out_device) {
    if (!out_device) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out_device is null");
    }
#ifdef _WIN32
    return abi_audio_win_default_device(out_device);
#elif defined(__APPLE__)
    return abi_audio_mac_default_device(out_device);
#elif defined(__linux__)
    return abi_audio_linux_default_device(out_device);
#else
    abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "audio not supported on this platform");
    return 0;
#endif
}

int64_t abi_audio_stream_open(
    int64_t device_id,
    int32_t sample_rate,
    int32_t buffer_size_frames,
    int32_t output_channels,
    int32_t input_channels,
    KainNativeAudioCallback callback,
    void* user_data,
    KainNativeAudioStream** out_stream
) {
    int i;
    KainNativeAudioStreamSlot* slot = NULL;
    int64_t result;

    if (!out_stream) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out_stream is null");
    }
    if (!callback) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "callback is null");
    }

    for (i = 0; i < ABI_AUDIO_MAX_STREAMS; i++) {
        if (!g_streams[i].in_use) {
            slot = &g_streams[i];
            break;
        }
    }
    if (!slot) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "capacity", "stream capacity exceeded");
    }
    memset(slot, 0, sizeof(*slot));

#ifdef _WIN32
    result = abi_audio_win_open_stream(device_id, sample_rate, buffer_size_frames,
                                        output_channels, input_channels, callback, user_data, slot);
#elif defined(__APPLE__)
    result = abi_audio_mac_open_stream(device_id, sample_rate, buffer_size_frames,
                                       output_channels, input_channels, callback, user_data, slot);
#elif defined(__linux__)
    result = abi_audio_linux_open_stream(device_id, sample_rate, buffer_size_frames,
                                         output_channels, input_channels, callback, user_data, slot);
#else
    (void)device_id; (void)sample_rate; (void)buffer_size_frames;
    (void)output_channels; (void)input_channels; (void)callback; (void)user_data;
    result = abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "audio not supported on this platform");
#endif
    if (result < 0) {
        return result;
    }
    *out_stream = (KainNativeAudioStream*)slot;
    return result;
}

int64_t abi_audio_stream_start(KainNativeAudioStream* stream) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)stream;
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
#ifdef _WIN32
    return abi_audio_win_start_stream(s);
#elif defined(__APPLE__)
    return abi_audio_mac_start_stream(s);
#elif defined(__linux__)
    return abi_audio_linux_start_stream(s);
#else
    return abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "audio not supported on this platform");
#endif
}

int64_t abi_audio_stream_stop(KainNativeAudioStream* stream) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)stream;
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
#ifdef _WIN32
    return abi_audio_win_stop_stream(s);
#elif defined(__APPLE__)
    return abi_audio_mac_stop_stream(s);
#elif defined(__linux__)
    return abi_audio_linux_stop_stream(s);
#else
    return abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "audio not supported on this platform");
#endif
}

int64_t abi_audio_stream_close(KainNativeAudioStream* stream) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)stream;
    if (!s) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "stream is null");
    }
#ifdef _WIN32
    return abi_audio_win_close_stream(s);
#elif defined(__APPLE__)
    return abi_audio_mac_close_stream(s);
#elif defined(__linux__)
    return abi_audio_linux_close_stream(s);
#else
    return abi_audio_fail(ABI_AUDIO_ERR_NO_DEVICE, "no_device", "audio not supported on this platform");
#endif
}

int64_t abi_audio_stream_is_running(KainNativeAudioStream* stream, int32_t* out_running) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)stream;
    if (!s || !out_running) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "stream or out_running is null");
    }
    *out_running = atomic_load(&s->is_running);
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

int64_t abi_audio_stream_sample_rate(KainNativeAudioStream* stream, int32_t* out_rate) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)stream;
    if (!s || !out_rate) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "stream or out_rate is null");
    }
    *out_rate = s->sample_rate;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

int64_t abi_audio_stream_buffer_size(KainNativeAudioStream* stream, int32_t* out_size) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)stream;
    if (!s || !out_size) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "stream or out_size is null");
    }
    *out_size = s->buffer_size_frames;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

int64_t abi_audio_stream_channels(KainNativeAudioStream* stream, int32_t* out_channels) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)stream;
    if (!s || !out_channels) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "stream or out_channels is null");
    }
    *out_channels = s->output_channels;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

int64_t abi_audio_stream_cpu_load(KainNativeAudioStream* stream, double* out_load) {
    KainNativeAudioStreamSlot* s = (KainNativeAudioStreamSlot*)stream;
    if (!s || !out_load) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "stream or out_load is null");
    }
    /* Shared-mode WASAPI/CoreAudio/ALSA do not expose CPU load directly. */
    *out_load = 0.0;
    abi_audio_ok();
    return ABI_AUDIO_OK;
}

/* ── MIDI public API ──────────────────────────────────────────────────── */

int64_t abi_audio_midi_device_count(void) {
#ifdef _WIN32
    return abi_audio_win_midi_count();
#elif defined(__APPLE__)
    return abi_audio_mac_midi_count();
#elif defined(__linux__)
    return abi_audio_linux_midi_count();
#else
    abi_audio_fail(ABI_AUDIO_ERR_MIDI_NO_DEVICE, "no_midi", "midi not supported on this platform");
    return 0;
#endif
}

int64_t abi_audio_midi_device_name(int64_t device_id, char* out_name, int64_t out_name_capacity) {
    if (!out_name || out_name_capacity <= 0) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out_name is null");
    }
#ifdef _WIN32
    return abi_audio_win_midi_name(device_id, out_name, out_name_capacity);
#elif defined(__APPLE__)
    return abi_audio_mac_midi_name(device_id, out_name, out_name_capacity);
#elif defined(__linux__)
    return abi_audio_linux_midi_name(device_id, out_name, out_name_capacity);
#else
    (void)device_id;
    abi_audio_fail(ABI_AUDIO_ERR_MIDI_NO_DEVICE, "no_midi", "midi not supported on this platform");
    return 0;
#endif
}

int64_t abi_audio_midi_open_input(
    int64_t device_id,
    KainNativeMidiCallback callback,
    void* user_data,
    int64_t* out_handle
) {
    int i;
    KainNativeMidiInputSlot* slot = NULL;
    int64_t result;

    if (!out_handle) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "out_handle is null");
    }
    if (!callback) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "invalid_arg", "callback is null");
    }
    for (i = 0; i < ABI_AUDIO_MAX_MIDI_INPUTS; i++) {
        if (!g_midi_inputs[i].in_use) {
            slot = &g_midi_inputs[i];
            break;
        }
    }
    if (!slot) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_ARG, "capacity", "midi input capacity exceeded");
    }
    memset(slot, 0, sizeof(*slot));

#ifdef _WIN32
    result = abi_audio_win_midi_open(device_id, callback, user_data, slot);
#elif defined(__APPLE__)
    result = abi_audio_mac_midi_open(device_id, callback, user_data, slot);
#elif defined(__linux__)
    result = abi_audio_linux_midi_open(device_id, callback, user_data, slot);
#else
    (void)device_id; (void)callback; (void)user_data;
    result = abi_audio_fail(ABI_AUDIO_ERR_MIDI_NO_DEVICE, "no_midi", "midi not supported on this platform");
#endif
    if (result < 0) {
        return result;
    }
    *out_handle = slot->id;
    return result;
}

int64_t abi_audio_midi_close_input(int64_t handle) {
    KainNativeMidiInputSlot* slot = abi_audio_midi_slot_locked(handle);
    if (!slot) {
        return abi_audio_fail(ABI_AUDIO_ERR_INVALID_HANDLE, "invalid_handle", "midi handle not found");
    }
#ifdef _WIN32
    return abi_audio_win_midi_close(slot);
#elif defined(__APPLE__)
    return abi_audio_mac_midi_close(slot);
#elif defined(__linux__)
    return abi_audio_linux_midi_close(slot);
#else
    return abi_audio_fail(ABI_AUDIO_ERR_MIDI_NO_DEVICE, "no_midi", "midi not supported on this platform");
#endif
}

/* ── Diagnostics ──────────────────────────────────────────────────────── */

int64_t abi_audio_last_status(void) {
    return g_last_status;
}

const char* abi_audio_last_error_kind(void) {
    return g_last_error_kind;
}

const char* abi_audio_last_error_message(void) {
    return g_last_error_message;
}
