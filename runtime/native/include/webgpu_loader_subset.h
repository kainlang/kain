#ifndef KAIN_WEBGPU_LOADER_SUBSET_H
#define KAIN_WEBGPU_LOADER_SUBSET_H

// ============================================================================
//  webgpu_loader_subset.h — Pure-declaration header for WebGPU.
// ============================================================================
//  WebGPU uses wgpu-native C API. This header defines WGPU handle types
//  and the few PFN prototypes needed to bootstrap. All types are uintptr_t.
//
//  Never includes <webgpu/webgpu.h> or <wgpu.h>. The actual struct types
//  are only needed in the separately-linked ABI library (webgpu-abi/).
// ============================================================================

#include <stdint.h>

// ── WGPU handle types (all uintptr_t) ────────────────────────────

typedef uintptr_t WGPUInstance;
typedef uintptr_t WGPUAdapter;
typedef uintptr_t WGPUDevice;
typedef uintptr_t WGPUQueue;
typedef uintptr_t WGPUSurface;
typedef uintptr_t WGPUSwapChain;
typedef uintptr_t WGPUTexture;
typedef uintptr_t WGPUTextureView;
typedef uintptr_t WGPUCommandEncoder;
typedef uintptr_t WGPUCommandBuffer;
typedef uintptr_t WGPURenderPassEncoder;
typedef uintptr_t WGPUBindGroup;
typedef uintptr_t WGPUBindGroupLayout;
typedef uintptr_t WGPUPipelineLayout;
typedef uintptr_t WGPURenderPipeline;
typedef uintptr_t WGPUShaderModule;
typedef uintptr_t WGPUBuffer;
typedef uintptr_t WGPUFence;

// ── wgpu-native PFN prototypes ───────────────────────────────────

WGPUInstance   wgpuCreateInstance(const void* pDescriptor);
WGPUSurface    wgpuInstanceCreateSurface(WGPUInstance instance,
                                          const void* pDescriptor);
void           wgpuAdapterRequestDevice(WGPUAdapter adapter,
                                         const void* pDescriptor,
                                         void (*callback)(uint32_t status,
                                                          WGPUDevice device,
                                                          const char* message,
                                                          void* userdata),
                                         void* userdata);
WGPUSwapChain  wgpuDeviceCreateSwapChain(WGPUDevice device,
                                          WGPUSurface surface,
                                          const void* pDescriptor);
WGPUTextureView wgpuSwapChainGetCurrentTextureView(WGPUSwapChain swapChain);
void            wgpuSwapChainPresent(WGPUSwapChain swapChain);

#endif /* KAIN_WEBGPU_LOADER_SUBSET_H */
