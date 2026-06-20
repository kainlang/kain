// audio_device_bridge.c — miniaudio wrapper for reson8
//
// Implements a flat C device I/O surface using miniaudio's unified API.
// All platform detection (WASAPI/ASIO/CoreAudio/ALSA) is handled by
// miniaudio internally. This bridge just owns the ma_device and ring buffers.

#define MINIAUDIO_IMPLEMENTATION
#include "../../3rdparty/miniaudio/miniaudio.h"

#include "audio_device_bridge.h"
#include <string.h>
#include <stdlib.h>

// ── Internal state ──
static ma_device         g_device;
static ma_device_config  g_config;
static int               g_state = AD_STATE_STOPPED;
static int               g_channels = 2;
static int               g_sample_rate = 48000;
static int               g_buffer_frames = 256;
static int               g_format = AD_FORMAT_F32;

// Double-buffered ring for input (miniaudio callback → Kain read)
static float* g_input_ring[2]  = {NULL, NULL};
static int    g_input_write     = 0;  // which ring the callback is writing into
static int    g_input_frames[2] = {0, 0};

// Double-buffered ring for output (Kain write → miniaudio callback)
static float* g_output_ring[2]  = {NULL, NULL};
static int    g_output_read     = 0;  // which ring the callback is reading from
static int    g_output_frames[2] = {0, 0};

static char g_last_error[256] = "";
static int  g_last_status = 0;

// ── miniaudio data callback ──
// This runs in the audio thread — no allocations, no locks.
static void audio_callback(ma_device* device, void* output, const void* input,
                           ma_uint32 frame_count)
{
    (void)device;

    // Write incoming input into the input ring
    int write_idx = g_input_write;
    if (input && g_input_ring[write_idx]) {
        int copy_frames = (frame_count > (unsigned)g_buffer_frames) ?
                          (unsigned)g_buffer_frames : (int)frame_count;
        memcpy(g_input_ring[write_idx], input,
               copy_frames * (unsigned)g_channels * sizeof(float));
        g_input_frames[write_idx] = copy_frames;
        g_input_write = 1 - write_idx;
    }

    // Read outgoing output from the output ring
    int read_idx = g_output_read;
    if (output) {
        if (g_output_ring[read_idx] && g_output_frames[read_idx] > 0) {
            int copy_frames = (frame_count > (unsigned)g_output_frames[read_idx]) ?
                              (unsigned)g_output_frames[read_idx] : (int)frame_count;
            memcpy(output, g_output_ring[read_idx],
                   copy_frames * (unsigned)g_channels * sizeof(float));
            // Zero-pad remaining if needed
            if (copy_frames < (int)frame_count) {
                memset((float*)output + copy_frames * g_channels, 0,
                       ((unsigned)frame_count - (unsigned)copy_frames) * (unsigned)g_channels * sizeof(float));
            }
            g_output_frames[read_idx] = 0;
            g_output_read = 1 - read_idx;
        } else {
            // Silence
            memset(output, 0, frame_count * (unsigned)g_channels * sizeof(float));
        }
    }
}

// ── Lifecycle ──

int audio_device_init(int device_type, int sample_rate, int channels,
                      int buffer_size_frames, int format)
{
    if (g_state != AD_STATE_STOPPED) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "Device already initialized (state=%d)", g_state);
        g_last_status = -1;
        return -1;
    }

    g_sample_rate = sample_rate;
    g_channels = channels;
    g_buffer_frames = buffer_size_frames;
    g_format = format;

    // Allocate ring buffers
    size_t ring_bytes = (size_t)(buffer_size_frames * channels) * sizeof(float);
    g_input_ring[0]  = (float*)malloc(ring_bytes);
    g_input_ring[1]  = (float*)malloc(ring_bytes);
    g_output_ring[0] = (float*)malloc(ring_bytes);
    g_output_ring[1] = (float*)malloc(ring_bytes);

    if (!g_input_ring[0] || !g_input_ring[1] || !g_output_ring[0] || !g_output_ring[1]) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "Failed to allocate ring buffers (%zu bytes each)", ring_bytes);
        g_last_status = -2;
        audio_device_close();
        return -2;
    }

    memset(g_input_ring[0],  0, ring_bytes);
    memset(g_input_ring[1],  0, ring_bytes);
    memset(g_output_ring[0], 0, ring_bytes);
    memset(g_output_ring[1], 0, ring_bytes);
    g_input_frames[0]  = 0;
    g_input_frames[1]  = 0;
    g_output_frames[0] = 0;
    g_output_frames[1] = 0;
    g_input_write      = 0;
    g_output_read      = 0;

    // Configure miniaudio device
    ma_format ma_fmt = ma_format_f32;
    switch (format) {
        case AD_FORMAT_S16: ma_fmt = ma_format_s16; break;
        case AD_FORMAT_S24: ma_fmt = ma_format_s24; break;
        case AD_FORMAT_S32: ma_fmt = ma_format_s32; break;
        default:            ma_fmt = ma_format_f32; break;
    }

    g_config = ma_device_config_init(ma_device_type_duplex);
    g_config.playback.pDeviceID  = NULL;
    g_config.capture.pDeviceID   = NULL;
    g_config.playback.format     = ma_fmt;
    g_config.capture.format      = ma_fmt;
    g_config.playback.channels   = (ma_uint32)channels;
    g_config.capture.channels    = (ma_uint32)channels;
    g_config.sampleRate          = (ma_uint32)sample_rate;
    g_config.periodSizeInFrames  = (ma_uint32)buffer_size_frames;
    g_config.dataCallback        = audio_callback;
    g_config.pUserData           = NULL;

    // If ASIO requested, set backend
    if (device_type == AD_DEVICE_ASIO) {
        ma_backend backends[1] = { ma_backend_asio };
        g_config.pPlayback = (ma_device_id*)NULL; // use default ASIO device
        // Try ASIO backend
        ma_context context;
        if (ma_context_init(backends, 1, NULL, &context) == MA_SUCCESS) {
            ma_device_info* info = NULL;
            if (ma_context_get_device_info(&context, ma_device_type_playback,
                                           NULL, &info) == MA_SUCCESS) {
                g_config.playback.pDeviceID = &info->id;
            }
            ma_context_uninit(&context);
        }
    }

    g_state = AD_STATE_STOPPED; // configured but not started
    g_last_status = 0;
    g_last_error[0] = '\0';
    return 0;
}

int audio_device_start(void)
{
    if (g_state != AD_STATE_STOPPED) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "Cannot start: state=%d (expected STOPPED)", g_state);
        g_last_status = -3;
        return -3;
    }

    ma_result result = ma_device_init(NULL, &g_config, &g_device);
    if (result != MA_SUCCESS) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "ma_device_init failed: %d", (int)result);
        g_last_status = (int)result;
        return (int)result;
    }

    result = ma_device_start(&g_device);
    if (result != MA_SUCCESS) {
        snprintf(g_last_error, sizeof(g_last_error),
                 "ma_device_start failed: %d", (int)result);
        ma_device_uninit(&g_device);
        g_last_status = (int)result;
        return (int)result;
    }

    g_state = AD_STATE_RUNNING;
    g_last_status = 0;
    return 0;
}

int audio_device_stop(void)
{
    if (g_state != AD_STATE_RUNNING) {
        return 0; // not an error
    }
    g_state = AD_STATE_STOPPING;
    ma_device_stop(&g_device);
    ma_device_uninit(&g_device);
    g_state = AD_STATE_STOPPED;
    g_last_status = 0;
    return 0;
}

int audio_device_close(void)
{
    audio_device_stop();

    free(g_input_ring[0]);  g_input_ring[0]  = NULL;
    free(g_input_ring[1]);  g_input_ring[1]  = NULL;
    free(g_output_ring[0]); g_output_ring[0] = NULL;
    free(g_output_ring[1]); g_output_ring[1] = NULL;
    g_input_frames[0]  = 0;
    g_input_frames[1]  = 0;
    g_output_frames[0] = 0;
    g_output_frames[1] = 0;

    g_last_status = 0;
    return 0;
}

// ── Buffer exchange ──

int audio_device_input_frame_count(void)
{
    // Return frames in the most recently completed input ring
    int read_idx = 1 - g_input_write;
    return g_input_frames[read_idx];
}

int audio_device_read_input(float* dst, int max_frames)
{
    int read_idx = 1 - g_input_write;
    int frames = g_input_frames[read_idx];
    if (frames <= 0) return 0;
    if (frames > max_frames) frames = max_frames;

    memcpy(dst, g_input_ring[read_idx],
           (size_t)(frames * g_channels) * sizeof(float));
    g_input_frames[read_idx] = 0;
    return frames;
}

int audio_device_write_output(const float* src, int frames)
{
    int write_idx = 1 - g_output_read;
    if (frames > g_buffer_frames) frames = g_buffer_frames;

    // Copy into the output ring that the callback will consume next
    memcpy(g_output_ring[write_idx], src,
           (size_t)(frames * g_channels) * sizeof(float));
    g_output_frames[write_idx] = frames;
    return frames;
}

void audio_device_swap_buffers(void)
{
    // Advance output read index so the callback picks up the new data
    // (The callback flips g_output_read after consuming)
    // Actually, we use a simpler approach: write to 1-read_idx, callback reads from read_idx
    // We just need to make sure the write goes to the correct ring.
    // The callback flips read_idx after consuming, so we write to 1-read_idx.
    // No explicit swap needed — the double buffering handles it.
}

// ── State query ──

int audio_device_state(void)           { return g_state; }
int audio_device_sample_rate(void)     { return g_sample_rate; }
int audio_device_channels(void)        { return g_channels; }
int audio_device_buffer_size_frames(void) { return g_buffer_frames; }

// ── Error ──

const char* audio_device_last_error(void) { return g_last_error; }
int audio_device_last_status(void)        { return g_last_status; }

// ── Device enumeration ──

int audio_device_count(int device_type)
{
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        return 0;
    }
    ma_device_type ma_type = ma_device_type_playback;
    ma_uint32 count = 0;
    ma_context_get_devices(&context, NULL, NULL, NULL, &count);
    ma_context_uninit(&context);
    return (int)count;
}

const char* audio_device_name(int device_type, int index)
{
    static char name_buf[256] = "";
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        return "(no context)";
    }
    ma_device_info* infos = NULL;
    ma_uint32 count = 0;
    ma_context_get_devices(&context, NULL, NULL, &infos, &count);
    if ((unsigned)index < count) {
        snprintf(name_buf, sizeof(name_buf), "%s", infos[index].name);
    } else {
        snprintf(name_buf, sizeof(name_buf), "(invalid index %d)", index);
    }
    ma_context_uninit(&context);
    return name_buf;
}

int audio_device_is_default(int device_type, int index)
{
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        return 0;
    }
    ma_device_info* infos = NULL;
    ma_uint32 count = 0;
    ma_context_get_devices(&context, NULL, NULL, &infos, &count);
    int result = 0;
    if ((unsigned)index < count) {
        result = (infos[index].isDefault) ? 1 : 0;
    }
    ma_context_uninit(&context);
    return result;
}

float audio_device_cpu_load(void)
{
    // miniaudio doesn't expose CPU load directly.
    // Can be extended with OS-specific calls.
    return 0.0f;
}
