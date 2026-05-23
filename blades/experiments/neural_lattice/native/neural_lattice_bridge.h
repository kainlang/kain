#ifndef KAIN_BLADE_NEURAL_LATTICE_BRIDGE_H
#define KAIN_BLADE_NEURAL_LATTICE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

int neural_lattice_native_probe(void);
int neural_lattice_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    int signal,
    int mirror_signal,
    int epoch,
    int lock_state,
    int hot_synapses,
    int actor_echo,
    int ui_hash,
    int graphics_score
);
int neural_lattice_native_frames_presented(void);
int neural_lattice_native_cells_drawn(void);
int neural_lattice_native_write_report(const char* path);

#ifdef __cplusplus
}
#endif

#endif
