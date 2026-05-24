#ifndef KAIN_BLADE_CUDA_VISUAL_BRIDGE_H
#define KAIN_BLADE_CUDA_VISUAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

int cuda_visual_native_probe(void);
int cuda_visual_verify_and_render(
    const char* gpu_payload_path,
    const char* report_path,
    const char* gpu_bmp_path,
    const char* cpu_bmp_path,
    const char* diff_bmp_path,
    int width,
    int height,
    int seed,
    int tone
);

#ifdef __cplusplus
}
#endif

#endif
