// audio_device_bridge.h — Flat C API over miniaudio for reson8
//
// Wraps https://github.com/mackron/miniaudio (public domain, single-header)
// into a Kain-includable flat C surface. Kain never sees miniaudio types.
//
// Platforms covered: WASAPI (Windows), ASIO (Windows pro audio),
// CoreAudio (macOS), ALSA/PulseAudio (Linux).

#ifndef KAIN_AUDIO_DEVICE_BRIDGE_H
#define KAIN_AUDIO_DEVICE_BRIDGE_H

#if defined(_WIN32)
#define KAIN_AD_EXPORT __declspec(dllexport)
#else
#define KAIN_AD_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ── Device types ──
#define AD_DEVICE_WASAPI  0
#define AD_DEVICE_ASIO    1
#define AD_DEVICE_COREAUDIO 2
#define AD_DEVICE_ALSA    3
#define AD_DEVICE_PULSE   4

// ── Format constants ──
#define AD_FORMAT_F32  0   // 32-bit float (Kain native format)
#define AD_FORMAT_S16  1   // 16-bit signed integer
#define AD_FORMAT_S24  2   // 24-bit signed integer (packed)
#define AD_FORMAT_S32  3   // 32-bit signed integer

// ── State ──
#define AD_STATE_STOPPED  0
#define AD_STATE_STARTING 1
#define AD_STATE_RUNNING  2
#define AD_STATE_STOPPING 3

// ── Lifecycle ──
KAIN_AD_EXPORT int audio_device_init(
    int device_type,
    int sample_rate,
    int channels,
    int buffer_size_frames,
    int format
);
KAIN_AD_EXPORT int audio_device_start(void);
KAIN_AD_EXPORT int audio_device_stop(void);
KAIN_AD_EXPORT int audio_device_close(void);

// ── Buffer exchange (called from audio callback OR externally for polling) ──
// Returns number of frames in the input buffer, or 0 if none.
KAIN_AD_EXPORT int audio_device_input_frame_count(void);
// Copy input frames into Kain-provided buffer. Returns frames copied.
KAIN_AD_EXPORT int audio_device_read_input(float* dst, int max_frames);
// Write output frames from Kain. Returns frames written.
KAIN_AD_EXPORT int audio_device_write_output(const float* src, int frames);
// Swap internal ring buffers (called after processing completes).
KAIN_AD_EXPORT void audio_device_swap_buffers(void);

// ── State query ──
KAIN_AD_EXPORT int audio_device_state(void);
KAIN_AD_EXPORT int audio_device_sample_rate(void);
KAIN_AD_EXPORT int audio_device_channels(void);
KAIN_AD_EXPORT int audio_device_buffer_size_frames(void);

// ── Error ──
KAIN_AD_EXPORT const char* audio_device_last_error(void);
KAIN_AD_EXPORT int audio_device_last_status(void);

// ── Device enumeration ──
KAIN_AD_EXPORT int audio_device_count(int device_type);
KAIN_AD_EXPORT const char* audio_device_name(int device_type, int index);
KAIN_AD_EXPORT int audio_device_is_default(int device_type, int index);

// ── CPU usage (from miniaudio internal timer) ──
KAIN_AD_EXPORT float audio_device_cpu_load(void);

#ifdef __cplusplus
}
#endif

#endif // KAIN_AUDIO_DEVICE_BRIDGE_H
