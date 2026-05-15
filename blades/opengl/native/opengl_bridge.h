#ifndef KAIN_BLADE_OPENGL_BRIDGE_H
#define KAIN_BLADE_OPENGL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

int opengl_native_probe(void);
int opengl_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    int clear_red,
    int clear_green,
    int clear_blue,
    int accent_red,
    int accent_green,
    int accent_blue
);
int opengl_native_frames_presented(void);
int opengl_native_triangles_drawn(void);
int opengl_native_write_report(const char* path);

#ifdef __cplusplus
}
#endif

#endif
