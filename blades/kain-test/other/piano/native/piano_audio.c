#define _POSIX_C_SOURCE 200809L

#include "piano_audio.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define MA_IMPLEMENTATION
#include "miniaudio.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PIANO_NOTE_MIN 21
#define PIANO_NOTE_MAX 108
#define PIANO_MAX_LOOP_EVENTS 4096
#define PIANO_NOTE_SAMPLE_SECONDS 0.72

typedef struct PianoNoteEvent {
    int midi_note;
    unsigned int offset_ms;
} PianoNoteEvent;

typedef struct PianoAudioState {
    ma_engine engine;
    int engine_ready;
    int sample_rate;
    char cache_dir[PATH_MAX];
    char status[1024];
    char last_error[512];
    PianoNoteEvent recorded_events[PIANO_MAX_LOOP_EVENTS];
    int recorded_event_count;
    int recording;
    unsigned long long recording_start_ms;
    int loop_playing;
    int loop_duration_ms;
    int playback_thread_running;
    int playback_stop_requested;
    pthread_t playback_thread;
} PianoAudioState;

static PianoAudioState G_STATE;
static pthread_mutex_t G_MUTEX = PTHREAD_MUTEX_INITIALIZER;

static unsigned long long piano_now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000ULL + (unsigned long long)(tv.tv_usec / 1000ULL);
}

static void piano_clear_error_locked(void) {
    G_STATE.last_error[0] = '\0';
}

static void piano_set_error_locked(const char* message) {
    if (message == NULL || message[0] == '\0') {
        G_STATE.last_error[0] = '\0';
        return;
    }
    snprintf(G_STATE.last_error, sizeof(G_STATE.last_error), "%s", message);
}

static void piano_update_status_locked(const char* reason) {
    const char* error_text = G_STATE.last_error[0] ? G_STATE.last_error : "none";
    snprintf(
        G_STATE.status,
        sizeof(G_STATE.status),
        "audio=%s|reason=%s|error=%s|sample_rate=%d|recording=%s|loop_playing=%s|notes=%d|loop_ms=%d|cache=%s",
        G_STATE.engine_ready ? "ready" : "offline",
        reason ? reason : "idle",
        error_text,
        G_STATE.sample_rate,
        G_STATE.recording ? "yes" : "no",
        G_STATE.loop_playing ? "yes" : "no",
        G_STATE.recorded_event_count,
        G_STATE.loop_duration_ms,
        G_STATE.cache_dir[0] ? G_STATE.cache_dir : "(unset)"
    );
}

static int piano_mkdir_recursive(const char* path) {
    char buffer[PATH_MAX];
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (strlen(path) >= sizeof(buffer)) {
        return 0;
    }

    snprintf(buffer, sizeof(buffer), "%s", path);
    for (i = 1; buffer[i] != '\0'; ++i) {
        if (buffer[i] == '/') {
            buffer[i] = '\0';
            if (mkdir(buffer, 0775) != 0 && errno != EEXIST) {
                return 0;
            }
            buffer[i] = '/';
        }
    }

    if (mkdir(buffer, 0775) != 0 && errno != EEXIST) {
        return 0;
    }

    return 1;
}

static int piano_note_path(int midi_note, char* out_path, size_t out_size) {
    if (midi_note < PIANO_NOTE_MIN || midi_note > PIANO_NOTE_MAX || out_path == NULL || out_size == 0) {
        return 0;
    }

    if (G_STATE.cache_dir[0] == '\0') {
        return 0;
    }

    if ((size_t)snprintf(out_path, out_size, "%s/note_%03d.wav", G_STATE.cache_dir, midi_note) >= out_size) {
        return 0;
    }

    return 1;
}

static double piano_frequency_for_midi(int midi_note) {
    return 440.0 * pow(2.0, ((double)midi_note - 69.0) / 12.0);
}

static double piano_envelope(double t) {
    double attack = t < 0.012 ? (t / 0.012) : 1.0;
    double decay = exp(-t * 3.8);
    return attack * decay;
}

static double piano_voice_sample(double frequency_hz, double t) {
    double envelope = piano_envelope(t);
    double fundamental = sin(2.0 * M_PI * frequency_hz * t);
    double second = 0.32 * sin(2.0 * M_PI * frequency_hz * 2.0 * t + 0.2);
    double third = 0.14 * sin(2.0 * M_PI * frequency_hz * 3.01 * t + 0.5);
    double shimmer = 0.06 * sin(2.0 * M_PI * frequency_hz * 4.1 * t + 1.2);
    return (fundamental + second + third + shimmer) * envelope * 0.55;
}

static int piano_generate_note_wave(const char* path, int midi_note) {
    ma_encoder encoder;
    ma_encoder_config config;
    ma_result result;
    float* samples = NULL;
    int frame_count;
    int frame_index;
    double frequency_hz;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    frame_count = (int)((double)G_STATE.sample_rate * PIANO_NOTE_SAMPLE_SECONDS);
    if (frame_count <= 0) {
        return 0;
    }

    samples = (float*)malloc((size_t)frame_count * sizeof(float));
    if (samples == NULL) {
        return 0;
    }

    frequency_hz = piano_frequency_for_midi(midi_note);
    for (frame_index = 0; frame_index < frame_count; ++frame_index) {
        double t = (double)frame_index / (double)G_STATE.sample_rate;
        samples[frame_index] = (float)piano_voice_sample(frequency_hz, t);
    }

    config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 1, (ma_uint32)G_STATE.sample_rate);
    result = ma_encoder_init_file(path, &config, &encoder);
    if (result != MA_SUCCESS) {
        free(samples);
        return 0;
    }

    result = ma_encoder_write_pcm_frames(&encoder, samples, (ma_uint64)frame_count, NULL);
    ma_encoder_uninit(&encoder);
    free(samples);

    return result == MA_SUCCESS;
}

static int piano_ensure_note_sample_locked(int midi_note, char* out_path, size_t out_size) {
    if (!piano_note_path(midi_note, out_path, out_size)) {
        piano_set_error_locked("invalid piano note");
        piano_update_status_locked("note-path-failed");
        return 0;
    }

    if (access(out_path, F_OK) == 0) {
        return 1;
    }

    if (!piano_mkdir_recursive(G_STATE.cache_dir)) {
        piano_set_error_locked("failed to create piano cache");
        piano_update_status_locked("cache-create-failed");
        return 0;
    }

    if (!piano_generate_note_wave(out_path, midi_note)) {
        piano_set_error_locked("failed to generate piano sample");
        piano_update_status_locked("sample-generate-failed");
        return 0;
    }

    return 1;
}

static int piano_play_note_locked(int midi_note) {
    char path[PATH_MAX];
    ma_result result;

    if (!G_STATE.engine_ready) {
        piano_set_error_locked("audio engine is not ready");
        piano_update_status_locked("play-failed");
        return 0;
    }

    if (!piano_ensure_note_sample_locked(midi_note, path, sizeof(path))) {
        return 0;
    }

    result = ma_engine_play_sound(&G_STATE.engine, path, NULL);
    if (result != MA_SUCCESS) {
        piano_set_error_locked("failed to play piano sample");
        piano_update_status_locked("play-failed");
        return 0;
    }

    return 1;
}

static void piano_record_event_locked(int midi_note, unsigned int offset_ms) {
    if (!G_STATE.recording) {
        return;
    }

    if (G_STATE.recorded_event_count >= PIANO_MAX_LOOP_EVENTS) {
        piano_set_error_locked("loop buffer full");
        return;
    }

    G_STATE.recorded_events[G_STATE.recorded_event_count].midi_note = midi_note;
    G_STATE.recorded_events[G_STATE.recorded_event_count].offset_ms = offset_ms;
    G_STATE.recorded_event_count += 1;
}

static void* piano_playback_thread_main(void* user_data) {
    PianoNoteEvent events[PIANO_MAX_LOOP_EVENTS];
    int event_count;
    int loop_duration_ms;
    unsigned long long cycle_start_ms;
    int i;

    (void)user_data;

    pthread_mutex_lock(&G_MUTEX);
    event_count = G_STATE.recorded_event_count;
    loop_duration_ms = G_STATE.loop_duration_ms;
    if (event_count > PIANO_MAX_LOOP_EVENTS) {
        event_count = PIANO_MAX_LOOP_EVENTS;
    }
    memcpy(events, G_STATE.recorded_events, (size_t)event_count * sizeof(PianoNoteEvent));
    pthread_mutex_unlock(&G_MUTEX);

    if (event_count <= 0 || loop_duration_ms <= 0) {
        pthread_mutex_lock(&G_MUTEX);
        G_STATE.loop_playing = 0;
        G_STATE.playback_thread_running = 0;
        G_STATE.playback_stop_requested = 0;
        piano_set_error_locked("no loop data available");
        piano_update_status_locked("loop-stopped");
        pthread_mutex_unlock(&G_MUTEX);
        return NULL;
    }

    while (1) {
        cycle_start_ms = piano_now_ms();

        for (i = 0; i < event_count; ++i) {
            unsigned long long target_ms = cycle_start_ms + (unsigned long long)events[i].offset_ms;

            while (1) {
                unsigned long long now_ms;
                unsigned long long remaining_ms;

                pthread_mutex_lock(&G_MUTEX);
                if (G_STATE.playback_stop_requested) {
                    pthread_mutex_unlock(&G_MUTEX);
                    goto playback_exit;
                }
                pthread_mutex_unlock(&G_MUTEX);

                now_ms = piano_now_ms();
                if (now_ms >= target_ms) {
                    break;
                }

                remaining_ms = target_ms - now_ms;
                if (remaining_ms > 4) {
                    remaining_ms = 4;
                }
                ma_sleep((ma_uint32)remaining_ms);
            }

            pthread_mutex_lock(&G_MUTEX);
            if (!G_STATE.playback_stop_requested) {
                piano_play_note_locked(events[i].midi_note);
            }
            pthread_mutex_unlock(&G_MUTEX);
        }

        while (1) {
            unsigned long long now_ms;
            unsigned long long elapsed_ms;
            unsigned long long remaining_ms;

            pthread_mutex_lock(&G_MUTEX);
            if (G_STATE.playback_stop_requested) {
                pthread_mutex_unlock(&G_MUTEX);
                goto playback_exit;
            }
            pthread_mutex_unlock(&G_MUTEX);

            now_ms = piano_now_ms();
            elapsed_ms = now_ms - cycle_start_ms;
            if (elapsed_ms >= (unsigned long long)loop_duration_ms) {
                break;
            }

            remaining_ms = (unsigned long long)loop_duration_ms - elapsed_ms;
            if (remaining_ms > 4) {
                remaining_ms = 4;
            }
            ma_sleep((ma_uint32)remaining_ms);
        }
    }

playback_exit:
    pthread_mutex_lock(&G_MUTEX);
    G_STATE.loop_playing = 0;
    G_STATE.playback_thread_running = 0;
    G_STATE.playback_stop_requested = 0;
    piano_clear_error_locked();
    piano_update_status_locked("loop-stopped");
    pthread_mutex_unlock(&G_MUTEX);
    return NULL;
}

static void piano_reset_runtime_state_locked(void) {
    G_STATE.recorded_event_count = 0;
    G_STATE.recording = 0;
    G_STATE.recording_start_ms = 0;
    G_STATE.loop_playing = 0;
    G_STATE.loop_duration_ms = 0;
    G_STATE.playback_thread_running = 0;
    G_STATE.playback_stop_requested = 0;
    piano_clear_error_locked();
}

int piano_audio_init(const char* cache_dir, int sample_rate) {
    ma_engine_config config;
    ma_result result;

    if (G_STATE.engine_ready) {
        piano_audio_shutdown();
    }

    pthread_mutex_lock(&G_MUTEX);
    memset(&G_STATE, 0, sizeof(G_STATE));
    G_STATE.sample_rate = sample_rate > 0 ? sample_rate : 48000;
    snprintf(
        G_STATE.cache_dir,
        sizeof(G_STATE.cache_dir),
        "%s",
        (cache_dir != NULL && cache_dir[0] != '\0') ? cache_dir : "native/piano_cache"
    );

    if (!piano_mkdir_recursive(G_STATE.cache_dir)) {
        piano_set_error_locked("failed to create piano cache directory");
        piano_update_status_locked("init-failed");
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    config = ma_engine_config_init();
    config.sampleRate = (ma_uint32)G_STATE.sample_rate;
    result = ma_engine_init(&config, &G_STATE.engine);
    if (result != MA_SUCCESS) {
        G_STATE.engine_ready = 0;
        piano_set_error_locked("failed to initialize audio engine");
        piano_update_status_locked("init-failed");
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    G_STATE.engine_ready = 1;
    piano_reset_runtime_state_locked();
    piano_update_status_locked("boot");
    pthread_mutex_unlock(&G_MUTEX);
    return 1;
}

void piano_audio_shutdown(void) {
    if (!G_STATE.engine_ready) {
        return;
    }

    piano_audio_stop_loop_playback();

    pthread_mutex_lock(&G_MUTEX);
    if (G_STATE.engine_ready) {
        ma_engine_uninit(&G_STATE.engine);
        G_STATE.engine_ready = 0;
    }
    piano_reset_runtime_state_locked();
    piano_update_status_locked("shutdown");
    pthread_mutex_unlock(&G_MUTEX);
}

int piano_audio_note_on(int midi_note) {
    unsigned long long now_ms;
    unsigned int event_offset_ms;

    pthread_mutex_lock(&G_MUTEX);
    if (!G_STATE.engine_ready) {
        piano_set_error_locked("audio engine is not ready");
        piano_update_status_locked("note-failed");
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    now_ms = piano_now_ms();
    if (!piano_play_note_locked(midi_note)) {
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    if (G_STATE.recording) {
        if (now_ms >= G_STATE.recording_start_ms) {
            unsigned long long elapsed_ms = now_ms - G_STATE.recording_start_ms;
            event_offset_ms = elapsed_ms > UINT_MAX ? UINT_MAX : (unsigned int)elapsed_ms;
        } else {
            event_offset_ms = 0;
        }
        piano_record_event_locked(midi_note, event_offset_ms);
    }

    piano_clear_error_locked();
    piano_update_status_locked("note");
    pthread_mutex_unlock(&G_MUTEX);
    return 1;
}

int piano_audio_start_recording(void) {
    piano_audio_stop_loop_playback();

    pthread_mutex_lock(&G_MUTEX);
    if (!G_STATE.engine_ready) {
        piano_set_error_locked("audio engine is not ready");
        piano_update_status_locked("record-failed");
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    piano_reset_runtime_state_locked();
    G_STATE.recording = 1;
    G_STATE.recording_start_ms = piano_now_ms();
    piano_update_status_locked("recording");
    pthread_mutex_unlock(&G_MUTEX);
    return 1;
}

int piano_audio_stop_recording(void) {
    pthread_mutex_lock(&G_MUTEX);
    if (!G_STATE.engine_ready) {
        piano_set_error_locked("audio engine is not ready");
        piano_update_status_locked("record-stop-failed");
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    G_STATE.recording = 0;
    if (G_STATE.recording_start_ms != 0) {
        unsigned long long elapsed_ms = piano_now_ms() - G_STATE.recording_start_ms;
        G_STATE.loop_duration_ms = elapsed_ms > (unsigned long long)INT_MAX ? INT_MAX : (int)elapsed_ms;
    }
    piano_clear_error_locked();
    piano_update_status_locked("record-stopped");
    pthread_mutex_unlock(&G_MUTEX);
    return G_STATE.recorded_event_count;
}

int piano_audio_start_loop_playback(void) {
    int thread_created = 0;

    pthread_mutex_lock(&G_MUTEX);
    if (!G_STATE.engine_ready) {
        piano_set_error_locked("audio engine is not ready");
        piano_update_status_locked("loop-start-failed");
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    if (G_STATE.playback_thread_running) {
        piano_clear_error_locked();
        piano_update_status_locked("loop-playing");
        pthread_mutex_unlock(&G_MUTEX);
        return 1;
    }

    if (G_STATE.recorded_event_count <= 0 || G_STATE.loop_duration_ms <= 0) {
        piano_set_error_locked("no loop recorded yet");
        piano_update_status_locked("loop-start-failed");
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    G_STATE.playback_stop_requested = 0;
    G_STATE.loop_playing = 1;
    G_STATE.playback_thread_running = 1;
    piano_clear_error_locked();
    piano_update_status_locked("loop-playing");
    if (pthread_create(&G_STATE.playback_thread, NULL, piano_playback_thread_main, NULL) == 0) {
        thread_created = 1;
    } else {
        G_STATE.playback_thread_running = 0;
        G_STATE.loop_playing = 0;
        piano_set_error_locked("failed to start loop thread");
        piano_update_status_locked("loop-start-failed");
    }
    pthread_mutex_unlock(&G_MUTEX);

    if (!thread_created) {
        return 0;
    }

    return 1;
}

int piano_audio_stop_loop_playback(void) {
    int should_join = 0;

    pthread_mutex_lock(&G_MUTEX);
    if (!G_STATE.engine_ready) {
        G_STATE.loop_playing = 0;
        G_STATE.playback_thread_running = 0;
        G_STATE.playback_stop_requested = 0;
        piano_clear_error_locked();
        piano_update_status_locked("loop-stopped");
        pthread_mutex_unlock(&G_MUTEX);
        return 1;
    }

    if (G_STATE.playback_thread_running) {
        G_STATE.playback_stop_requested = 1;
        should_join = 1;
    }
    pthread_mutex_unlock(&G_MUTEX);

    if (should_join) {
        pthread_join(G_STATE.playback_thread, NULL);
    }

    pthread_mutex_lock(&G_MUTEX);
    G_STATE.loop_playing = 0;
    G_STATE.playback_thread_running = 0;
    G_STATE.playback_stop_requested = 0;
    piano_clear_error_locked();
    piano_update_status_locked("loop-stopped");
    pthread_mutex_unlock(&G_MUTEX);
    return 1;
}

int piano_audio_clear_loop(void) {
    piano_audio_stop_loop_playback();

    pthread_mutex_lock(&G_MUTEX);
    if (!G_STATE.engine_ready) {
        piano_set_error_locked("audio engine is not ready");
        piano_update_status_locked("clear-failed");
        pthread_mutex_unlock(&G_MUTEX);
        return 0;
    }

    G_STATE.recorded_event_count = 0;
    G_STATE.recording = 0;
    G_STATE.recording_start_ms = 0;
    G_STATE.loop_duration_ms = 0;
    G_STATE.loop_playing = 0;
    piano_clear_error_locked();
    piano_update_status_locked("cleared");
    pthread_mutex_unlock(&G_MUTEX);
    return 1;
}

int piano_audio_recorded_event_count(void) {
    int result;

    pthread_mutex_lock(&G_MUTEX);
    result = G_STATE.recorded_event_count;
    pthread_mutex_unlock(&G_MUTEX);
    return result;
}

int piano_audio_loop_duration_ms(void) {
    int result;

    pthread_mutex_lock(&G_MUTEX);
    result = G_STATE.loop_duration_ms;
    pthread_mutex_unlock(&G_MUTEX);
    return result;
}

const char* piano_audio_status(void) {
    const char* result;

    pthread_mutex_lock(&G_MUTEX);
    if (G_STATE.status[0] == '\0') {
        piano_update_status_locked("idle");
    }
    result = G_STATE.status;
    pthread_mutex_unlock(&G_MUTEX);
    return result;
}
