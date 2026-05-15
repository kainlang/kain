#ifndef KAIN_RUNTIME_WIN32_H
#define KAIN_RUNTIME_WIN32_H

#include "kain_runtime_base.h"

typedef struct KainWin32AppHost KainWin32AppHost;
typedef struct KainWin32MouseCapture KainWin32MouseCapture;

typedef struct {
    const char* id;
    const char* label;
    const char* scene_aliases;
    float clear_color[4];
    float fog_color[4];
    float ambient_light[4];
    float diffuse_light[4];
    float light_position[4];
    float accent_a[3];
    float accent_b[3];
    int default_width;
    int default_height;
    double move_speed;
    double sprint_multiplier;
    double jump_velocity;
    double gravity;
    double mouse_sensitivity;
    double eye_height;
    double fog_density;
    int particle_count;
} KainViewportProfile;

typedef struct {
    double x;
    double y;
    double z;
} KainVec3;

const KainViewportProfile* kain_find_viewport_profile(const char* id);
int kain_env_flag(const char* name, int fallback);
int kain_env_int(const char* name, int fallback);
double kain_env_double(const char* name, double fallback);
char* kain_env_dup(const char* name);
void kain_env_free(char* value);
int kain_env_set_string(const char* name, const char* value);
int kain_env_set_int(const char* name, long long value);
int kain_env_set_double(const char* name, double value);
int kain_env_set_flag(const char* name, int value);
int kain_win32_get_executable_path(char* out_path, size_t out_cap);
int kain_win32_get_executable_sidecar_path(const char* suffix, char* out_path, size_t out_cap);
KainVec3 kain_vec3_make(double x, double y, double z);
KainVec3 kain_vec3_add(KainVec3 a, KainVec3 b);
KainVec3 kain_vec3_sub(KainVec3 a, KainVec3 b);
KainVec3 kain_vec3_scale(KainVec3 v, double scale);
double kain_vec3_dot(KainVec3 a, KainVec3 b);
KainVec3 kain_vec3_cross(KainVec3 a, KainVec3 b);
KainVec3 kain_vec3_normalize(KainVec3 v);

#ifdef _WIN32
typedef int (*KainWin32AppInitFn)(KainWin32AppHost* host, void* user_data);
typedef void (*KainWin32AppFrameFn)(KainWin32AppHost* host, void* user_data, double frame_delta);
typedef void (*KainWin32AppShutdownFn)(KainWin32AppHost* host, void* user_data);
typedef LRESULT (*KainWin32AppMessageFn)(
    KainWin32AppHost* host,
    void* user_data,
    HWND hwnd,
    UINT msg,
    WPARAM w_param,
    LPARAM l_param,
    int* handled
);

struct KainWin32MouseCapture {
    HWND hwnd;
    int pointer_locked;
    int drag_capture_count;
    int cursor_hidden;
};

typedef struct {
    const char* class_name;
    const char* window_title;
    UINT class_style;
    DWORD window_style;
    DWORD window_ex_style;
    int default_width;
    int default_height;
    int show_command;
    int sleep_millis;
    double min_frame_delta;
    double max_frame_delta;
    KainWin32AppInitFn on_init;
    KainWin32AppFrameFn on_frame;
    KainWin32AppShutdownFn on_shutdown;
    KainWin32AppMessageFn on_message;
} KainWin32AppConfig;

struct KainWin32AppHost {
    HWND hwnd;
    HINSTANCE instance;
    int width;
    int height;
    int running;
    LARGE_INTEGER perf_freq;
    LARGE_INTEGER prev_counter;
    double frame_delta;
    double frame_fps;
    double fps_accumulator;
    int fps_frames;
    const KainWin32AppConfig* config;
    void* user_data;
};

int kain_win32_app_run(KainWin32AppHost* host, const KainWin32AppConfig* config, void* user_data);
void kain_win32_app_request_close(KainWin32AppHost* host);
void kain_win32_mouse_capture_bind(KainWin32MouseCapture* capture, HWND hwnd);
void kain_win32_mouse_capture_set_pointer_lock(KainWin32MouseCapture* capture, int enabled);
void kain_win32_mouse_capture_begin_drag(KainWin32MouseCapture* capture, HWND hwnd);
void kain_win32_mouse_capture_end_drag(KainWin32MouseCapture* capture);
void kain_win32_mouse_capture_release_all(KainWin32MouseCapture* capture);
int kain_win32_mouse_capture_sample_relative(KainWin32MouseCapture* capture, int* delta_x, int* delta_y);
void kain_win32_frame_timer_begin(LARGE_INTEGER* perf_freq, LARGE_INTEGER* prev_counter, double* fps_accumulator, int* fps_frames, double* frame_fps);
double kain_win32_frame_timer_step(LARGE_INTEGER* perf_freq, LARGE_INTEGER* prev_counter, double* fps_accumulator, int* fps_frames, double* frame_fps, double min_dt, double max_dt);
#endif

#endif
