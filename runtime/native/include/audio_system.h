/*
 * KAIN Native Audio System ABI
 *
 * The C ABI surface for std::audio device streaming and MIDI input. Kain
 * owns the semantics (worlds, patches, orchestrate graphs, pulse transport).
 * The C runtime owns the platform door-knocking: WASAPI/CoreAudio/ALSA for
 * audio streaming and WinMM/CoreMIDI/ALSA raw MIDI for input.
 *
 * Design decisions (non-negotiable, from research/audio/RUNTIME_C.md):
 *  - f32-only at the ABI boundary. No S16/S24/S32 in the hot path.
 *  - Stereo-first, channels parameterized (default 2, supports 1..128).
 *  - 48 kHz default sample rate.
 *  - No exclusive mode in Phase 1 — WASAPI shared-mode only.
 *  - No ASIO in Phase 1 — capability bit defined, implementation Phase 2.
 *  - Event-driven, not polling (SetEventHandle on Windows, render callback
 *    on macOS, poll() on Linux).
 *  - No device hot-plug in Phase 1 — snapshot enumeration only.
 *
 * Threading: Audio and MIDI callbacks fire on platform-internal threads.
 * The Kain compiler enforces real-time safety via `pulse budget(...)`; the
 * C ABI does not lock or allocate inside callbacks.
 *
 * Error model: 0 on success, negative on error, positive on counts/lengths.
 * Status enum (KainNativeAudioStatus) carries the same negative values for
 * the audio-specific failures.
 */

#ifndef ABI_AUDIO_SYSTEM_H
#define ABI_AUDIO_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ───────────────────────────────────────────────────────────── */

#define ABI_AUDIO_MAX_DEVICES         64
#define ABI_AUDIO_MAX_DEVICE_NAME     256
#define ABI_AUDIO_MAX_API_NAME        64
#define ABI_AUDIO_MAX_CHANNELS        128
#define ABI_AUDIO_MAX_MIDI_DEVICES    32
#define ABI_AUDIO_MAX_MIDI_NAME       128
#define ABI_AUDIO_MAX_STREAMS         16
#define ABI_AUDIO_MAX_MIDI_INPUTS     16

#define ABI_AUDIO_DEFAULT_SAMPLE_RATE 48000
#define ABI_AUDIO_DEFAULT_BUFFER_SIZE 256
#define ABI_AUDIO_DEFAULT_CHANNELS    2

/* ── Status Codes ────────────────────────────────────────────────────────── */

typedef enum KainNativeAudioStatus {
    ABI_AUDIO_OK                  =  0,
    ABI_AUDIO_ERR_NO_DEVICE       = -1,
    ABI_AUDIO_ERR_DEVICE_BUSY     = -2,
    ABI_AUDIO_ERR_UNSUPPORTED_FMT = -3,
    ABI_AUDIO_ERR_STREAM_ACTIVE   = -4,
    ABI_AUDIO_ERR_INVALID_HANDLE  = -5,
    ABI_AUDIO_ERR_BUFFER_OVERFLOW = -6,
    ABI_AUDIO_ERR_BUFFER_UNDERRUN = -7,
    ABI_AUDIO_ERR_MIDI_NO_DEVICE  = -8,
    ABI_AUDIO_ERR_MIDI_OVERFLOW   = -9,
    ABI_AUDIO_ERR_INVALID_ARG     = -10,
    ABI_AUDIO_ERR_OUT_OF_MEMORY   = -11,
    ABI_AUDIO_ERR_PLATFORM        = -12
} KainNativeAudioStatus;

/* ── Sample Format (internal — always f32 at the ABI boundary) ─────────── */

typedef enum KainNativeAudioSampleFormat {
    ABI_AUDIO_F32 = 0   /* 32-bit float (THE internal format) */
} KainNativeAudioSampleFormat;

/* ── Device Info ─────────────────────────────────────────────────────────── */

typedef struct KainNativeAudioDeviceInfo {
    int64_t device_id;
    char    name[ABI_AUDIO_MAX_DEVICE_NAME];
    char    api_name[ABI_AUDIO_MAX_API_NAME]; /* "wasapi", "asio", "coreaudio", "alsa" */
    int32_t max_output_channels;
    int32_t max_input_channels;
    int32_t default_sample_rate;
    int32_t is_default;
} KainNativeAudioDeviceInfo;

/* ── Stream Handle (opaque) ─────────────────────────────────────────────── */

typedef struct KainNativeAudioStream KainNativeAudioStream;

/* ── Audio Callback Type ───────────────────────────────────────────────────
 *
 * Called from the platform audio thread. MUST be real-time safe (no
 * allocation, locking, or blocking). The Kain compiler enforces this via
 * `pulse budget(alloc=0, lock=0, io=0)`.
 *
 *   input_channels:   interleaved f32 input samples (NULL if input disabled
 *                     — Phase 1 is output only)
 *   output_channels:  interleaved f32 output samples (caller fills)
 *   frames:           number of frames to process
 *   channels:         channel count (1 = mono, 2 = stereo, ...)
 *   user_data:        opaque pointer passed at stream creation
 */
typedef void (*KainNativeAudioCallback)(
    const float* input_channels,
    float*       output_channels,
    int32_t      frames,
    int32_t      channels,
    void*        user_data
);

/* ── Device Enumeration ──────────────────────────────────────────────────── */

int64_t abi_audio_device_count(void);

int64_t abi_audio_enumerate_devices(
    KainNativeAudioDeviceInfo* devices,
    int64_t                    max_devices
);

int64_t abi_audio_default_output_device(KainNativeAudioDeviceInfo* out_device);

/* ── Stream Lifecycle ────────────────────────────────────────────────────── */

int64_t abi_audio_stream_open(
    int64_t                  device_id,
    int32_t                  sample_rate,
    int32_t                  buffer_size_frames,
    int32_t                  output_channels,
    int32_t                  input_channels,
    KainNativeAudioCallback  callback,
    void*                    user_data,
    KainNativeAudioStream**  out_stream
);

int64_t abi_audio_stream_start(KainNativeAudioStream* stream);

int64_t abi_audio_stream_stop(KainNativeAudioStream* stream);

int64_t abi_audio_stream_close(KainNativeAudioStream* stream);

/* ── Stream Info ─────────────────────────────────────────────────────────── */

int64_t abi_audio_stream_is_running(KainNativeAudioStream* stream, int32_t* out_running);

int64_t abi_audio_stream_sample_rate(KainNativeAudioStream* stream, int32_t* out_rate);

int64_t abi_audio_stream_buffer_size(KainNativeAudioStream* stream, int32_t* out_size);

int64_t abi_audio_stream_channels(KainNativeAudioStream* stream, int32_t* out_channels);

int64_t abi_audio_stream_cpu_load(KainNativeAudioStream* stream, double* out_load);

/* ── MIDI Input ──────────────────────────────────────────────────────────── */

typedef struct KainNativeMidiEvent {
    int64_t timestamp_ms;
    uint8_t status;     /* High nibble = message type, low nibble = channel */
    uint8_t data1;      /* Note number / CC number                       */
    uint8_t data2;      /* Velocity / CC value                           */
} KainNativeMidiEvent;

typedef void (*KainNativeMidiCallback)(
    const KainNativeMidiEvent* event,
    void*                      user_data
);

int64_t abi_audio_midi_device_count(void);

int64_t abi_audio_midi_device_name(int64_t device_id, char* out_name, int64_t out_name_capacity);

int64_t abi_audio_midi_open_input(
    int64_t                device_id,
    KainNativeMidiCallback callback,
    void*                  user_data,
    int64_t*               out_handle
);

int64_t abi_audio_midi_close_input(int64_t handle);

/* ── Diagnostics ───────────────────────────────────────────────────────────
 *
 * Match the input_system pattern: every function updates last status,
 * and these accessors return the most recent failure details.
 */

int64_t         abi_audio_last_status(void);
const char*     abi_audio_last_error_kind(void);
const char*     abi_audio_last_error_message(void);

#ifdef __cplusplus
}
#endif

#endif /* ABI_AUDIO_SYSTEM_H */
