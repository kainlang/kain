#include "audio_tone_lab.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vendor/miniaudio.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static char G_SIGNATURE[256];

static int audiofx_decode_summary(
    const char* path,
    int* out_channels,
    int* out_frame_count,
    double* out_peak
) {
    float* pcm = NULL;
    ma_decoder_config config;
    ma_result result;
    ma_uint32 channels = 0;
    ma_uint32 sample_rate = 0;
    ma_uint64 total_frames = 0;
    double peak = 0.0;
    ma_uint64 sample_index;

    if (!path || !path[0]) {
        return 0;
    }

    config = ma_decoder_config_init_default();
    config.format = ma_format_f32;
    result = ma_decode_file(path, &config, &total_frames, (void**)&pcm);
    if (result != MA_SUCCESS || !pcm || total_frames == 0) {
        if (pcm) {
            ma_free(pcm, NULL);
        }
        return 0;
    }

    channels = config.channels;
    sample_rate = config.sampleRate;
    if (channels == 0 || sample_rate == 0) {
        ma_free(pcm, NULL);
        return 0;
    }

    for (sample_index = 0; sample_index < total_frames * (ma_uint64)channels; ++sample_index) {
        double value = fabs((double)pcm[sample_index]);
        if (value > peak) {
            peak = value;
        }
    }
    ma_free(pcm, NULL);

    if (out_channels) {
        *out_channels = (int)channels;
    }
    if (out_frame_count) {
        *out_frame_count = (int)total_frames;
    }
    if (out_peak) {
        *out_peak = peak;
    }

    return 1;
}

AUDIO_TONE_EXPORT int audiofx_write_sine_wave(
    const char* path,
    int sample_rate,
    int channels,
    int duration_ms,
    double frequency_hz,
    double amplitude
) {
    ma_encoder_config config;
    ma_encoder encoder;
    ma_result result;
    ma_int16* pcm;
    int frame_count;
    int frame_index;
    int channel_index;

    if (!path || !path[0] || sample_rate <= 0 || channels <= 0 || duration_ms <= 0) {
        return 0;
    }

    frame_count = (sample_rate * duration_ms) / 1000;
    if (frame_count <= 0) {
        return 0;
    }

    pcm = (ma_int16*)malloc((size_t)frame_count * (size_t)channels * sizeof(ma_int16));
    if (!pcm) {
        return 0;
    }

    for (frame_index = 0; frame_index < frame_count; ++frame_index) {
        double t = (double)frame_index / (double)sample_rate;
        double value = sin(2.0 * M_PI * frequency_hz * t) * amplitude;
        ma_int16 sample = (ma_int16)(value * 32767.0);
        for (channel_index = 0; channel_index < channels; ++channel_index) {
            pcm[frame_index * channels + channel_index] = sample;
        }
    }

    config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, (ma_uint32)channels, (ma_uint32)sample_rate);
    result = ma_encoder_init_file(path, &config, &encoder);
    if (result != MA_SUCCESS) {
        free(pcm);
        return 0;
    }

    result = ma_encoder_write_pcm_frames(&encoder, pcm, (ma_uint64)frame_count, NULL);
    ma_encoder_uninit(&encoder);
    free(pcm);
    if (result != MA_SUCCESS) {
        return 0;
    }
    return frame_count;
}

AUDIO_TONE_EXPORT int audiofx_wav_frame_count(const char* path) {
    int frame_count = 0;
    if (!audiofx_decode_summary(path, NULL, &frame_count, NULL)) {
        return 0;
    }
    return frame_count;
}

AUDIO_TONE_EXPORT int audiofx_wav_channels(const char* path) {
    int channels = 0;
    if (!audiofx_decode_summary(path, &channels, NULL, NULL)) {
        return 0;
    }
    return channels;
}

AUDIO_TONE_EXPORT double audiofx_wav_peak(const char* path) {
    double peak = 0.0;
    if (!audiofx_decode_summary(path, NULL, NULL, &peak)) {
        return 0.0;
    }
    return peak;
}

AUDIO_TONE_EXPORT const char* audiofx_wav_signature(const char* path) {
    int channels = 0;
    int frame_count = 0;
    double peak = 0.0;
    if (!audiofx_decode_summary(path, &channels, &frame_count, &peak)) {
        return "";
    }
    snprintf(
        G_SIGNATURE,
        sizeof(G_SIGNATURE),
        "wav|channels=%d|frames=%d|peak=%.6f",
        channels,
        frame_count,
        peak
    );
    return G_SIGNATURE;
}
