#ifndef KAIN_SMOKETEST_VISUALIZER_BRIDGE_H
#define KAIN_SMOKETEST_VISUALIZER_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

int smoketest_visualizer_native_probe(void);
int smoketest_visualizer_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    const char* input_path
);
int smoketest_visualizer_native_frames_presented(void);
int smoketest_visualizer_native_cells_drawn(void);
int smoketest_visualizer_native_write_report(const char* path);

#ifdef __cplusplus
}
#endif

#endif
