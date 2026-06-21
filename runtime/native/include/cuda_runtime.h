#ifndef KAIN_CUDA_RUNTIME_H
#define KAIN_CUDA_RUNTIME_H

#include <stdint.h>

#include "graphics_bundle.h"

#define KAIN_CUDA_SHADER_BUNDLE_ENV "KAIN_CUDA_SHADER_BUNDLE"
#define KAIN_CUDA_COMPUTE_RESIDENCY_ENV "KAIN_CUDA_COMPUTE_RESIDENCY"
#define KAIN_CUDA_SHADER_BUNDLE_FILE_NAME "kain_shader_bundle.json"
#define KAIN_CUDA_COMPUTE_RESIDENCY_FILE_NAME "kain_compute_residency.json"
#define KAIN_GPU_RUNTIME_LINUX_SO "libkain_gpu_runtime.so"

#ifdef __cplusplus
extern "C" {
#endif

int abi_cuda_driver_available(void);
int abi_cuda_runtime_library_available(void);
int abi_cuda_runtime_ready(void);
const char* abi_cuda_runtime_library_path(void);
const char* abi_cuda_shader_bundle_path(void);
const char* abi_cuda_compute_residency_path(void);
int64_t abi_cuda_dispatch_primary_compute(const char* compute_key);
int64_t abi_cuda_dispatch(
    const char* shader_bundle_path,
    const char* compute_residency_path,
    const char* compute_key
);
int64_t abi_gpu_dispatch(
    const char* compute_key,
    int64_t dispatch_x,
    int64_t dispatch_y,
    int64_t dispatch_z
);
int64_t abi_cuda_last_status(void);
const char* abi_cuda_last_error_kind(void);
const char* abi_cuda_last_error_message(void);
int64_t abi_cuda_last_dispatch_invocations(void);
int64_t abi_cuda_last_tensor_binding_count(void);
int64_t abi_cuda_last_stream_binding_count(void);
int64_t abi_cuda_last_neural_node_count(void);
int64_t abi_cuda_last_output_binding_count(void);
int64_t abi_cuda_last_total_output_bytes(void);
void kain_cuda_pipeline_cache_free_all(void);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_CUDA_RUNTIME_H */
