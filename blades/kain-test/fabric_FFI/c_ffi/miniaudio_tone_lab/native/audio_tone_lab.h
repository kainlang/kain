#if defined(_WIN32)
#define AUDIO_TONE_EXPORT __declspec(dllexport)
#else
#define AUDIO_TONE_EXPORT
#endif

AUDIO_TONE_EXPORT int audiofx_write_sine_wave(
    const char* path,
    int sample_rate,
    int channels,
    int duration_ms,
    double frequency_hz,
    double amplitude
);
AUDIO_TONE_EXPORT int audiofx_wav_frame_count(const char* path);
AUDIO_TONE_EXPORT int audiofx_wav_channels(const char* path);
AUDIO_TONE_EXPORT double audiofx_wav_peak(const char* path);
AUDIO_TONE_EXPORT const char* audiofx_wav_signature(const char* path);
