#ifndef KAIN_BLADE_PONG_WINDOW_BRIDGE_H
#define KAIN_BLADE_PONG_WINDOW_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

int pong_window_probe(void);
int pong_window_open_state(
    const char* title,
    int width,
    int height,
    int board_width,
    int board_height,
    int frame_budget
);
int pong_window_present_state(
    int frame_clock,
    int left_paddle_y,
    int right_paddle_y,
    int ball_x,
    int ball_y,
    int ball_dx,
    int ball_dy,
    int left_score,
    int right_score,
    int logical_swarm_count,
    int render_swarm_sample_count,
    int collisions_total,
    int chaos_mode,
    int swarm_energy,
    int entangle_registered,
    int entangle_propagations,
    int paddle_width,
    int paddle_height,
    int ball_size,
    int show_scanlines
);
int pong_window_should_close(void);
int pong_window_shutdown(void);
int pong_window_frames_presented(void);
int pong_window_write_report(const char* path);

#ifdef __cplusplus
}
#endif

#endif
