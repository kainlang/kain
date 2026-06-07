#ifndef KAINTANA_DESKTOP_BRIDGE_H
#define KAINTANA_DESKTOP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

int kaintana_native_desktop_probe(void);
int kaintana_native_desktop_scene_active(void);
int kaintana_native_desktop_reset(void);
int kaintana_native_desktop_begin_scene(
    const char* title,
    int width,
    int height,
    int clear_red,
    int clear_green,
    int clear_blue
);
int kaintana_native_desktop_push_rect(
    int x,
    int y,
    int width,
    int height,
    int red,
    int green,
    int blue,
    int alpha
);
int kaintana_native_desktop_push_text(
    const char* text,
    int x,
    int y,
    int red,
    int green,
    int blue,
    int alpha
);
int kaintana_native_desktop_run_window(int frame_budget);
int kaintana_native_desktop_command_count(void);
int kaintana_native_desktop_frames_presented(void);
int kaintana_native_desktop_write_report(const char* path);
int kaintana_native_desktop_get_system_dpi(void);
int kaintana_native_desktop_write_bmp(const char* path);

#ifdef __cplusplus
}
#endif

#endif
