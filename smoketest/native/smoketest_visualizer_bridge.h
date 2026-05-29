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

/*
 * Natural include aliases key off the header stem, so we also expose a
 * bridge-prefixed veneer that maps cleanly to `include "...bridge.h" as viz`
 * and gives Kain the expected `viz_*` surface.
 */
int smoketest_visualizer_bridge_probe(void);
int smoketest_visualizer_bridge_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    const char* input_path
);
int smoketest_visualizer_bridge_frames_presented(void);
int smoketest_visualizer_bridge_cells_drawn(void);
int smoketest_visualizer_bridge_write_report(const char* path);

#ifdef __cplusplus
}
#endif

#endif
