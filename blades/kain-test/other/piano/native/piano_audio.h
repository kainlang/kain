#ifndef PIANO_AUDIO_H
#define PIANO_AUDIO_H

#if defined(_WIN32)
#define PIANO_AUDIO_EXPORT __declspec(dllexport)
#else
#define PIANO_AUDIO_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

PIANO_AUDIO_EXPORT int piano_audio_init(const char* cache_dir, int sample_rate);
PIANO_AUDIO_EXPORT void piano_audio_shutdown(void);
PIANO_AUDIO_EXPORT int piano_audio_note_on(int midi_note);
PIANO_AUDIO_EXPORT int piano_audio_start_recording(void);
PIANO_AUDIO_EXPORT int piano_audio_stop_recording(void);
PIANO_AUDIO_EXPORT int piano_audio_start_loop_playback(void);
PIANO_AUDIO_EXPORT int piano_audio_stop_loop_playback(void);
PIANO_AUDIO_EXPORT int piano_audio_clear_loop(void);
PIANO_AUDIO_EXPORT int piano_audio_recorded_event_count(void);
PIANO_AUDIO_EXPORT int piano_audio_loop_duration_ms(void);
PIANO_AUDIO_EXPORT const char* piano_audio_status(void);

#ifdef __cplusplus
}
#endif

#endif
