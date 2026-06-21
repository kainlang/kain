#ifndef KAIN_WEBGPU_LOADER_SUBSET_H
#define KAIN_WEBGPU_LOADER_SUBSET_H

// ============================================================================
//  webgpu_loader_subset.h — Pure-declaration header for WebGPU.
// ============================================================================
//  WebGPU uses wgpu-native C API. This header defines WGPU handle types
//  and the PFN prototypes needed to bootstrap. All handles are uintptr_t.
//
//  Never includes <webgpu/webgpu.h> or <wgpu.h>. The actual struct types
//  are only needed in the separately-linked ABI library (webgpu-abi/).
//
//  This is the full subset used by the Kain WebGPU ABI library. It is
//  intentionally flat (no struct definitions, no enums) — wgpu-native is
//  consumed exclusively through opaque handles and void* descriptors.
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
typedef uintptr_t WGPUComputePassEncoder;
typedef uintptr_t WGPUBindGroup;
typedef uintptr_t WGPUBindGroupLayout;
typedef uintptr_t WGPUPipelineLayout;
typedef uintptr_t WGPURenderPipeline;
typedef uintptr_t WGPUComputePipeline;
typedef uintptr_t WGPUShaderModule;
typedef uintptr_t WGPUBuffer;
typedef uintptr_t WGPUFence;
typedef uintptr_t WGPURenderBundleEncoder;
typedef uintptr_t WGPUQuerySet;
typedef uintptr_t WGPUSampler;

// ── WGPU flag types (uint64_t bitmasks) ──────────────────────────

typedef uint64_t WGPUBufferUsageFlags;
typedef uint64_t WGPUTextureUsageFlags;
typedef uint64_t WGPUColorWriteMaskFlags;
typedef uint64_t WGPUShaderStageFlags;

// ── WGPU enum types (uint32_t, opaque to us) ─────────────────────

typedef uint32_t WGPUBackendType;
typedef uint32_t WGPUPresentMode;
typedef uint32_t WGPUTextureFormat;
typedef uint32_t WGPULoadOp;
typedef uint32_t WGPUStoreOp;
typedef uint32_t WGPUIndexFormat;
typedef uint32_t WGPUVertexFormat;
typedef uint32_t WGPUPrimitiveTopology;
typedef uint32_t WGPUCompareFunction;
typedef uint32_t WGPUBlendFactor;
typedef uint32_t WGPUBlendOperation;
typedef uint32_t WGPUStencilOperation;
typedef uint32_t WGPUAddressMode;
typedef uint32_t WGPUFilterMode;
typedef uint32_t WGPUPowerPreference;

// ── wgpu-native PFN prototypes ───────────────────────────────────

// Instance
WGPUInstance  wgpuCreateInstance(const void* pDescriptor);
void          wgpuInstanceRelease(WGPUInstance instance);
WGPUSurface   wgpuInstanceCreateSurface(WGPUInstance instance,
                                        const void* pDescriptor);
void          wgpuInstanceProcessEvents(WGPUInstance instance);

// Adapter
void          wgpuInstanceRequestAdapter(WGPUInstance instance,
                                          const void* pOptions,
                                          void (*callback)(uint32_t status,
                                                           WGPUAdapter adapter,
                                                           const char* message,
                                                           void* userdata),
                                          void* userdata);
void          wgpuAdapterRelease(WGPUAdapter adapter);

// Device
void          wgpuAdapterRequestDevice(WGPUAdapter adapter,
                                         const void* pDescriptor,
                                         void (*callback)(uint32_t status,
                                                          WGPUDevice device,
                                                          const char* message,
                                                          void* userdata),
                                         void* userdata);
void          wgpuDeviceRelease(WGPUDevice device);
WGPUQueue     wgpuDeviceGetQueue(WGPUDevice device);
void          wgpuDeviceSetUncapturedErrorCallback(WGPUDevice device,
                                                    void (*callback)(uint32_t type,
                                                                     const char* message,
                                                                     void* userdata),
                                                    void* userdata);
void          wgpuDeviceSetDeviceLostCallback(WGPUDevice device,
                                               void (*callback)(uint32_t type,
                                                                const char* message,
                                                                void* userdata),
                                               void* userdata);

// Swapchain
WGPUSwapChain    wgpuDeviceCreateSwapChain(WGPUDevice device,
                                           WGPUSurface surface,
                                           const void* pDescriptor);
void             wgpuSwapChainRelease(WGPUSwapChain swapChain);
WGPUTextureView  wgpuSwapChainGetCurrentTextureView(WGPUSwapChain swapChain);
void             wgpuSwapChainPresent(WGPUSwapChain swapChain);

// Command encoding
WGPUCommandEncoder    wgpuDeviceCreateCommandEncoder(WGPUDevice device,
                                                     const void* pDescriptor);
void                  wgpuCommandEncoderRelease(WGPUCommandEncoder encoder);
WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder encoder,
                                                        const void* pDescriptor);
void                  wgpuRenderPassEncoderEnd(WGPURenderPassEncoder encoder);
void                  wgpuRenderPassEncoderRelease(WGPURenderPassEncoder encoder);
WGPUCommandBuffer     wgpuCommandEncoderFinish(WGPUCommandEncoder encoder,
                                               const void* pDescriptor);
void                  wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer);

// Clear
void wgpuRenderPassEncoderClearColor(WGPURenderPassEncoder encoder,
                                      const void* pColor);

// Submit
void wgpuQueueSubmit(WGPUQueue queue,
                     uint32_t commandCount,
                     const WGPUCommandBuffer* pCommands);
void wgpuQueueRelease(WGPUQueue queue);

// Shader
WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device,
                                               const void* pDescriptor);
void             wgpuShaderModuleRelease(WGPUShaderModule shaderModule);

// Pipeline
WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device,
                                                   const void* pDescriptor);
void               wgpuRenderPipelineRelease(WGPURenderPipeline pipeline);

// Buffer
WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device,
                                   const void* pDescriptor);
void       wgpuBufferRelease(WGPUBuffer buffer);
void       wgpuBufferDestroy(WGPUBuffer buffer);

// Bind group
WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device,
                                                     const void* pDescriptor);
WGPUBindGroup       wgpuDeviceCreateBindGroup(WGPUDevice device,
                                               const void* pDescriptor);
WGPUPipelineLayout  wgpuDeviceCreatePipelineLayout(WGPUDevice device,
                                                    const void* pDescriptor);

#endif /* KAIN_WEBGPU_LOADER_SUBSET_H */
