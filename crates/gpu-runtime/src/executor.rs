use crate::bindings::{
    BarrierMetadata, ComputeCase, GpuBindingAccess, GpuCudaLaunchOptions, GpuDescriptorKind,
    GpuDispatchBinding, GpuDispatchRequest, GpuDispatchResult, GpuQueuePolicy,
};
use ash::{vk, Entry};
use kain_core::{gpu_storage_element_stride_bytes, shader_artifact_bundle_from_json};
use kain_interop::{
    shared_buffer_gpu_binding_view, GpuBindingAccess as InteropAccess,
    GpuDescriptorKind as InteropDescriptorKind, KainSharedBuffer, SharedBufferMetadata,
};
use serde::Deserialize;
use std::collections::HashMap;
use std::convert::TryFrom;
use std::ffi::{c_char, c_void, CStr, CString};
use std::fs;
use std::path::{Path, PathBuf};
use std::slice;
use std::sync::Mutex;
use thiserror::Error;

const DEFAULT_WORKGROUP_SIZE_X: u32 = 8;

#[derive(Debug, Clone, Default)]
pub struct GpuComputeExecutorConfig {
    pub validation_mode: u32,
}

#[repr(C)]
pub struct GpuRuntimeDispatchRequest {
    pub shader_bundle_path: *const c_char,
    pub compute_residency_path: *const c_char,
    pub compute_key: *const c_char,
    pub dispatch_size: [u32; 3],
    pub barrier_json: *const c_char,
}

#[repr(C)]
pub struct GpuRuntimeDispatchResult {
    pub status_code: i32,
    pub dispatch_invocations: u64,
    pub tensor_binding_count: u32,
    pub stream_binding_count: u32,
    pub neural_node_count: u32,
    pub output_binding_count: u32,
    pub total_output_bytes: u64,
    pub barrier_count: u32,
    pub async_queue_used: u32,
    pub message: [c_char; 256],
}

#[derive(Debug, Error)]
pub enum ComputeExecutorError {
    #[error("failed to load Vulkan entry: {0}")]
    LoadEntry(#[source] ash::LoadingError),
    #[error("NVIDIA CUDA driver is unavailable: {message}")]
    CudaDriverUnavailable { message: String },
    #[error("NVIDIA CUDA driver symbol {symbol} was not found")]
    CudaDriverSymbolMissing { symbol: &'static str },
    #[error("CUDA Driver API call {call} failed with code {code}: {message}")]
    CudaDriverCallFailed {
        call: &'static str,
        code: i32,
        message: String,
    },
    #[error("failed to create Vulkan instance: {0:?}")]
    CreateInstance(vk::Result),
    #[error("failed to enumerate physical devices: {0:?}")]
    EnumeratePhysicalDevices(vk::Result),
    #[error("no compute-capable physical device was found")]
    NoComputeDevice,
    #[error("failed to create Vulkan device: {0:?}")]
    CreateDevice(vk::Result),
    #[error("failed to create command pool: {0:?}")]
    CreateCommandPool(vk::Result),
    #[error("failed to create shader module: {0:?}")]
    CreateShaderModule(vk::Result),
    #[error("failed to create descriptor set layout: {0:?}")]
    CreateDescriptorSetLayout(vk::Result),
    #[error("failed to create pipeline layout: {0:?}")]
    CreatePipelineLayout(vk::Result),
    #[error("failed to create compute pipeline: {0:?}")]
    CreateComputePipeline(vk::Result),
    #[error("failed to create descriptor pool: {0:?}")]
    CreateDescriptorPool(vk::Result),
    #[error("failed to allocate descriptor sets: {0:?}")]
    AllocateDescriptorSets(vk::Result),
    #[error("failed to allocate command buffers: {0:?}")]
    AllocateCommandBuffers(vk::Result),
    #[error("failed to begin command buffer: {0:?}")]
    BeginCommandBuffer(vk::Result),
    #[error("failed to end command buffer: {0:?}")]
    EndCommandBuffer(vk::Result),
    #[error("failed to submit queue: {0:?}")]
    QueueSubmit(vk::Result),
    #[error("failed to wait for queue idle: {0:?}")]
    QueueWaitIdle(vk::Result),
    #[error("failed to create buffer: {0:?}")]
    CreateBuffer(vk::Result),
    #[error("failed to allocate device memory: {0:?}")]
    AllocateMemory(vk::Result),
    #[error("failed to bind buffer memory: {0:?}")]
    BindBufferMemory(vk::Result),
    #[error("failed to map device memory: {0:?}")]
    MapMemory(vk::Result),
    #[error("no suitable HOST_VISIBLE|HOST_COHERENT memory type was found")]
    NoSuitableMemoryType,
    #[error("binding {binding} had an empty payload")]
    EmptyBindingPayload { binding: usize },
    #[error("output binding {binding} is out of range for {len} bound buffer(s)")]
    OutputBindingOutOfRange { binding: usize, len: usize },
    #[error("SPIR-V bytecode length {len} is not 4-byte aligned")]
    SpirvMisaligned { len: usize },
    #[error("shader entry name contains an interior NUL byte")]
    InvalidShaderEntryName,
    #[error("buffer or allocation size {size} does not fit in usize")]
    DeviceSizeTooLarge { size: u64 },
    #[error("unable to read file {path}: {message}")]
    ReadFile { path: String, message: String },
    #[error("unable to write file {path}: {message}")]
    WriteFile { path: String, message: String },
    #[error("unable to parse shader bundle {path}: {message}")]
    ParseShaderBundle { path: String, message: String },
    #[error("unable to parse compute residency {path}: {message}")]
    ParseComputeResidency { path: String, message: String },
    #[error("compute key {compute_key} was not found in residency manifest {path}")]
    MissingComputeKey { compute_key: String, path: String },
    #[error("SPIR-V module {module_name} was not found in shader bundle {path}")]
    MissingSpirvModule { module_name: String, path: String },
    #[error("PTX module {module_name} was not found in shader bundle {path}")]
    MissingPtxModule { module_name: String, path: String },
    #[error("compute key {compute_key} PTX sidecar {field} expected {expected} but artifact reported {actual}")]
    PtxSidecarMismatch {
        compute_key: String,
        field: &'static str,
        expected: String,
        actual: String,
    },
    #[error("compute key {compute_key} PTX sidecar binding slots {expected:?} do not match resolved binding slots {actual:?}")]
    PtxSidecarBindingSlotsMismatch {
        compute_key: String,
        expected: Vec<u32>,
        actual: Vec<u32>,
    },
    #[error("invalid hex payload for SPIR-V module {module_name}: {message}")]
    InvalidSpirvHex {
        module_name: String,
        message: String,
    },
    #[error("PTX source contains an interior NUL byte")]
    PtxContainsNul,
    #[error("PTX kernel entry name contains an interior NUL byte")]
    InvalidPtxEntryName,
    #[error(
        "PTX runtime currently supports buffer-backed bindings only; binding {binding} used {kind}"
    )]
    UnsupportedPtxBinding { binding: u32, kind: String },
    #[error("unsupported descriptor kind {value}")]
    UnsupportedDescriptorKind { value: String },
    #[error("invalid PTX uniform binding {binding}: {message}")]
    InvalidPtxUniformBinding { binding: String, message: String },
    #[error("unsupported access mode {value}")]
    UnsupportedAccessMode { value: String },
    #[error("unsupported compute residency target {value}")]
    UnsupportedComputeResidencyTarget { value: String },
    #[error("unsupported shared buffer contract {value} for binding {binding}")]
    UnsupportedSharedBufferContract { binding: String, value: String },
    #[error("shared buffer metadata is invalid for binding {binding}: {message}")]
    InvalidSharedBufferBinding { binding: String, message: String },
    #[error(
        "expected an output payload for binding slot {binding}, but no GPU result was returned"
    )]
    MissingOutputBinding { binding: u32 },
    #[error("output binding slot {binding} produced {actual} bytes but {expected} were expected for {path}")]
    OutputBindingLengthMismatch {
        binding: u32,
        expected: usize,
        actual: usize,
        path: String,
    },
    #[error("PTX source did not declare a .target sm_* architecture")]
    MissingPtxTargetArch,
    #[error(
        "PTX target directive {directive} did not contain a supported sm_* architecture token"
    )]
    InvalidPtxTargetArch { directive: String },
    #[error("PTX target {required} requires newer NVIDIA capability than {device_name} ({device}) provides")]
    UnsupportedPtxTargetForDevice {
        required: String,
        device_name: String,
        device: String,
    },
    #[error("invalid CUDA device ordinal override {value}; expected a non-negative integer")]
    InvalidCudaDeviceOrdinal { value: String },
    #[error("compute residency binding {binding} reuses slot @{slot}; PTX runtime requires unique binding slots")]
    DuplicateBindingSlot { binding: String, slot: u32 },
    #[error("compute residency binding {binding} declared {expected} bytes but sidecar {path} contained {actual}")]
    BindingPayloadLengthMismatch {
        binding: String,
        expected: usize,
        actual: usize,
        path: String,
    },
    #[error("compute residency binding {binding} used unsupported residency role {value}")]
    UnsupportedResidencyRole { binding: String, value: String },
    #[error("compute residency binding {binding} used residency role {role} but access mode {access} does not match")]
    InvalidResidencyRoleForAccess {
        binding: String,
        role: String,
        access: String,
    },
    #[error("unsupported CUDA stream policy {value}")]
    UnsupportedCudaStreamPolicy { value: String },
    #[error("unsupported CUDA graph policy {value}")]
    UnsupportedCudaGraphPolicy { value: String },
}

/// Mirrors the same struct in nvidia_ptx.rs — tracks where to flush each output binding after
/// GPU dispatch completes on the Vulkan path.
#[derive(Debug, Clone)]
struct OutputBindingTarget {
    slot: u32,
    payload_path: PathBuf,
    byte_length: usize,
}

/// Write every GPU output binding back to its residency sidecar file, exactly as the CUDA PTX
/// path does via `persist_output_bindings_to_sidecars` in nvidia_ptx.rs.  Without this, the
/// Kain simulation loop reads stale (all-zero) data from disk every frame — producing a black
/// screen even though the GPU computed valid results.
fn persist_spirv_output_bindings(
    output_targets: &[OutputBindingTarget],
    output_bindings: &[(u32, Vec<u8>)],
) -> Result<(), ComputeExecutorError> {
    for target in output_targets {
        let bytes = output_bindings
            .iter()
            .find(|(slot, _)| *slot == target.slot)
            .map(|(_, b)| b)
            .ok_or(ComputeExecutorError::MissingOutputBinding { binding: target.slot })?;
        if bytes.len() != target.byte_length {
            return Err(ComputeExecutorError::OutputBindingLengthMismatch {
                binding: target.slot,
                expected: target.byte_length,
                actual: bytes.len(),
                path: target.payload_path.display().to_string(),
            });
        }
        fs::write(&target.payload_path, bytes).map_err(|err| ComputeExecutorError::WriteFile {
            path: target.payload_path.display().to_string(),
            message: err.to_string(),
        })?;
    }
    Ok(())
}

pub type GpuComputeExecutor = VulkanComputeExecutor;

pub struct VulkanComputeExecutor {
    _entry: Entry,
    instance: ash::Instance,
    physical_device: vk::PhysicalDevice,
    device: ash::Device,
    queue: vk::Queue,
    command_pool: vk::CommandPool,
    async_compute_queue: Option<vk::Queue>,
    async_command_pool: Option<vk::CommandPool>,
    pipeline_cache: Mutex<HashMap<String, CachedPipeline>>,
    _config: GpuComputeExecutorConfig,
}

struct CachedPipeline {
    pipeline: vk::Pipeline,
    dispatch_size: [u32; 3],
}

struct GpuBuffer<'a> {
    device: &'a ash::Device,
    buffer: vk::Buffer,
    memory: vk::DeviceMemory,
    size: vk::DeviceSize,
}

impl Drop for GpuBuffer<'_> {
    fn drop(&mut self) {
        unsafe {
            self.device.destroy_buffer(self.buffer, None);
            self.device.free_memory(self.memory, None);
        }
    }
}

struct ExecutionResources<'a> {
    device: &'a ash::Device,
    command_pool: vk::CommandPool,
    shader_module: Option<vk::ShaderModule>,
    descriptor_set_layout: Option<vk::DescriptorSetLayout>,
    pipeline_layout: Option<vk::PipelineLayout>,
    pipeline: Option<vk::Pipeline>,
    descriptor_pool: Option<vk::DescriptorPool>,
    command_buffer: Option<vk::CommandBuffer>,
}

impl<'a> ExecutionResources<'a> {
    fn new(device: &'a ash::Device, command_pool: vk::CommandPool) -> Self {
        Self {
            device,
            command_pool,
            shader_module: None,
            descriptor_set_layout: None,
            pipeline_layout: None,
            pipeline: None,
            descriptor_pool: None,
            command_buffer: None,
        }
    }
}

impl Drop for ExecutionResources<'_> {
    fn drop(&mut self) {
        unsafe {
            if let Some(command_buffer) = self.command_buffer.take() {
                self.device
                    .free_command_buffers(self.command_pool, &[command_buffer]);
            }
            if let Some(descriptor_pool) = self.descriptor_pool.take() {
                self.device.destroy_descriptor_pool(descriptor_pool, None);
            }
            if let Some(pipeline) = self.pipeline.take() {
                self.device.destroy_pipeline(pipeline, None);
            }
            if let Some(pipeline_layout) = self.pipeline_layout.take() {
                self.device.destroy_pipeline_layout(pipeline_layout, None);
            }
            if let Some(descriptor_set_layout) = self.descriptor_set_layout.take() {
                self.device
                    .destroy_descriptor_set_layout(descriptor_set_layout, None);
            }
            if let Some(shader_module) = self.shader_module.take() {
                self.device.destroy_shader_module(shader_module, None);
            }
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
struct ComputeResidencyBundle {
    target: String,
    compute_shaders: Vec<ComputeResidencyEntry>,
}

#[derive(Debug, Clone, Deserialize)]
struct ComputeResidencyEntry {
    key: String,
    module_name: String,
    entry_point: String,
    workgroup_size: Option<[u32; 3]>,
    dispatch_size: Option<[u32; 3]>,
    tensor_binding_count: usize,
    stream_binding_count: usize,
    neural_node_count: usize,
    bindings: Vec<ComputeResidencyBinding>,
}

#[derive(Debug, Clone, Deserialize)]
struct ComputeResidencyBinding {
    key: String,
    contract: String,
    descriptor_kind: String,
    element_type: String,
    shape: Vec<i64>,
    strides: Vec<i64>,
    access_mode: String,
    slot: u32,
    payload_file: String,
}

impl VulkanComputeExecutor {
    pub fn try_new() -> Result<Self, ComputeExecutorError> {
        Self::try_new_with_config(GpuComputeExecutorConfig::default())
    }

    pub fn try_new_with_config(
        config: GpuComputeExecutorConfig,
    ) -> Result<Self, ComputeExecutorError> {
        unsafe {
            let entry = Entry::load().map_err(ComputeExecutorError::LoadEntry)?;
            let app_name = CString::new("kain-gpu-runtime").expect("static string");
            let engine_name = CString::new("kain").expect("static string");
            let app_info = vk::ApplicationInfo::builder()
                .application_name(&app_name)
                .application_version(0)
                .engine_name(&engine_name)
                .engine_version(0)
                .api_version(vk::make_api_version(0, 1, 1, 0));
            let instance_info = vk::InstanceCreateInfo::builder().application_info(&app_info);
            let instance = entry
                .create_instance(&instance_info, None)
                .map_err(ComputeExecutorError::CreateInstance)?;

            let physical_devices = instance
                .enumerate_physical_devices()
                .map_err(ComputeExecutorError::EnumeratePhysicalDevices)?;
            let mut selected = None;
            let mut queue_families = Vec::new();
            for physical_device in physical_devices {
                queue_families =
                    instance.get_physical_device_queue_family_properties(physical_device);
                if let Some((index, _)) = queue_families
                    .iter()
                    .enumerate()
                    .find(|(_, props)| props.queue_flags.contains(vk::QueueFlags::COMPUTE))
                {
                    selected = Some((physical_device, index as u32));
                    break;
                }
            }
            let (physical_device, queue_family_index) =
                selected.ok_or(ComputeExecutorError::NoComputeDevice)?;
            let queue_families =
                instance.get_physical_device_queue_family_properties(physical_device);

            // Detect a second COMPUTE queue family for async compute.
            // Prefer a queue family that is COMPUTE-only (no GRAPHICS bit)
            // for dedicated async compute hardware.
            let mut async_queue_family_index: Option<u32> = None;
            for (idx, props) in queue_families.iter().enumerate() {
                if idx as u32 != queue_family_index
                    && props.queue_flags.contains(vk::QueueFlags::COMPUTE)
                    && !props.queue_flags.contains(vk::QueueFlags::GRAPHICS)
                {
                    async_queue_family_index = Some(idx as u32);
                    break;
                }
            }
            // Fallback: any other COMPUTE queue (may share family with graphics)
            if async_queue_family_index.is_none() {
                for (idx, props) in queue_families.iter().enumerate() {
                    if idx as u32 != queue_family_index
                        && props.queue_flags.contains(vk::QueueFlags::COMPUTE)
                    {
                        async_queue_family_index = Some(idx as u32);
                        break;
                    }
                }
            }

            let priorities = [1.0f32];
            let mut queue_infos = vec![
                vk::DeviceQueueCreateInfo::builder()
                    .queue_family_index(queue_family_index)
                    .queue_priorities(&priorities)
                    .build()
            ];
            if let Some(async_idx) = async_queue_family_index {
                let async_priorities = [1.0f32];
                queue_infos.push(
                    vk::DeviceQueueCreateInfo::builder()
                        .queue_family_index(async_idx)
                        .queue_priorities(&async_priorities)
                        .build()
                );
            }
            let device_info = vk::DeviceCreateInfo::builder().queue_create_infos(&queue_infos);
            let device = instance
                .create_device(physical_device, &device_info, None)
                .map_err(ComputeExecutorError::CreateDevice)?;
            let queue = device.get_device_queue(queue_family_index, 0);
            let pool_info = vk::CommandPoolCreateInfo::builder()
                .queue_family_index(queue_family_index)
                .flags(vk::CommandPoolCreateFlags::RESET_COMMAND_BUFFER);
            let command_pool = device
                .create_command_pool(&pool_info, None)
                .map_err(ComputeExecutorError::CreateCommandPool)?;

            let (async_compute_queue, async_command_pool) =
                if let Some(async_idx) = async_queue_family_index {
                    let async_queue = device.get_device_queue(async_idx, 0);
                    let async_pool_info = vk::CommandPoolCreateInfo::builder()
                        .queue_family_index(async_idx)
                        .flags(vk::CommandPoolCreateFlags::RESET_COMMAND_BUFFER);
                    let async_pool = device
                        .create_command_pool(&async_pool_info, None)
                        .map_err(ComputeExecutorError::CreateCommandPool)?;
                    (Some(async_queue), Some(async_pool))
                } else {
                    (None, None)
                };

            Ok(Self {
                _entry: entry,
                instance,
                physical_device,
                device,
                queue,
                command_pool,
                async_compute_queue,
                async_command_pool,
                pipeline_cache: Mutex::new(HashMap::new()),
                _config: config,
            })
        }
    }

    pub fn dispatch_from_sidecars(
        &self,
        shader_bundle_path: &Path,
        compute_residency_path: &Path,
        compute_key: &str,
    ) -> Result<GpuDispatchResult, ComputeExecutorError> {
        let (spirv, request, output_targets) = dispatch_request_from_sidecars(
            shader_bundle_path,
            compute_residency_path,
            compute_key,
            None,
        )?;
        let dispatch = self.run_dispatch_request(&spirv, &request)?;
        persist_spirv_output_bindings(&output_targets, &dispatch.output_bindings)?;
        Ok(dispatch)
    }

    pub fn run_dispatch_request(
        &self,
        spirv: &[u8],
        request: &GpuDispatchRequest,
    ) -> Result<GpuDispatchResult, ComputeExecutorError> {
        unsafe {
            let shader_words = bytes_to_words(spirv)?;
            let module_info = vk::ShaderModuleCreateInfo::builder().code(&shader_words);
            let shader_module = self
                .device
                .create_shader_module(&module_info, None)
                .map_err(ComputeExecutorError::CreateShaderModule)?;

            let buffers: Vec<GpuBuffer<'_>> = request
                .bindings
                .iter()
                .enumerate()
                .map(|(index, binding)| self.create_buffer(index, binding))
                .collect::<Result<_, _>>()?;

            let mut resources = ExecutionResources::new(&self.device, self.command_pool);
            resources.shader_module = Some(shader_module);

            let mut sorted_bindings = request.bindings.iter().collect::<Vec<_>>();
            sorted_bindings.sort_by_key(|binding| binding.binding_slot);

            let layout_bindings: Vec<_> = sorted_bindings
                .iter()
                .map(|binding| {
                    vk::DescriptorSetLayoutBinding::builder()
                        .binding(binding.binding_slot)
                        .descriptor_type(binding.descriptor_kind.descriptor_type())
                        .descriptor_count(1)
                        .stage_flags(vk::ShaderStageFlags::COMPUTE)
                        .build()
                })
                .collect();
            let set_layout_info =
                vk::DescriptorSetLayoutCreateInfo::builder().bindings(&layout_bindings);
            let descriptor_set_layout = self
                .device
                .create_descriptor_set_layout(&set_layout_info, None)
                .map_err(ComputeExecutorError::CreateDescriptorSetLayout)?;
            resources.descriptor_set_layout = Some(descriptor_set_layout);

            let set_layouts = [descriptor_set_layout];
            let pipeline_layout_info =
                vk::PipelineLayoutCreateInfo::builder().set_layouts(&set_layouts);
            let pipeline_layout = self
                .device
                .create_pipeline_layout(&pipeline_layout_info, None)
                .map_err(ComputeExecutorError::CreatePipelineLayout)?;
            resources.pipeline_layout = Some(pipeline_layout);

            let entry_name = CString::new(request.entry_point.as_str())
                .map_err(|_| ComputeExecutorError::InvalidShaderEntryName)?;
            let stage = vk::PipelineShaderStageCreateInfo::builder()
                .stage(vk::ShaderStageFlags::COMPUTE)
                .module(shader_module)
                .name(&entry_name);
            let pipeline_info = vk::ComputePipelineCreateInfo::builder()
                .stage(*stage)
                .layout(pipeline_layout);
            let cache_key = format!(
                "{}:{}:{}x{}x{}",
                request.module_name,
                request.entry_point,
                request.workgroup_size[0],
                request.workgroup_size[1],
                request.workgroup_size[2]
            );
            let pipeline = {
                let mut cache = self.pipeline_cache.lock().unwrap();
                if let Some(cached) = cache.get(&cache_key) {
                    if cached.dispatch_size == request.dispatch_size {
                        cached.pipeline
                    } else {
                        // Dispatch size changed; recompile.
                        cache.remove(&cache_key);
                        let compute_pipelines = self
                            .device
                            .create_compute_pipelines(
                                vk::PipelineCache::null(),
                                &[*pipeline_info],
                                None,
                            )
                            .map_err(|(_, err)| {
                                ComputeExecutorError::CreateComputePipeline(err)
                            })?;
                        let pipeline = compute_pipelines[0];
                        cache.insert(
                            cache_key.clone(),
                            CachedPipeline {
                                pipeline,
                                dispatch_size: request.dispatch_size,
                            },
                        );
                        pipeline
                    }
                } else {
                    let compute_pipelines = self
                        .device
                        .create_compute_pipelines(
                            vk::PipelineCache::null(),
                            &[*pipeline_info],
                            None,
                        )
                        .map_err(|(_, err)| {
                            ComputeExecutorError::CreateComputePipeline(err)
                        })?;
                    let pipeline = compute_pipelines[0];
                    cache.insert(
                        cache_key.clone(),
                        CachedPipeline {
                            pipeline,
                            dispatch_size: request.dispatch_size,
                        },
                    );
                    pipeline
                }
            };
            resources.pipeline = Some(pipeline);

            let pool_sizes: Vec<_> = layout_bindings
                .iter()
                .map(|binding| vk::DescriptorPoolSize {
                    ty: binding.descriptor_type,
                    descriptor_count: 1,
                })
                .collect();
            let descriptor_pool_info = vk::DescriptorPoolCreateInfo::builder()
                .pool_sizes(&pool_sizes)
                .max_sets(1);
            let descriptor_pool = self
                .device
                .create_descriptor_pool(&descriptor_pool_info, None)
                .map_err(ComputeExecutorError::CreateDescriptorPool)?;
            resources.descriptor_pool = Some(descriptor_pool);

            let alloc_info = vk::DescriptorSetAllocateInfo::builder()
                .descriptor_pool(descriptor_pool)
                .set_layouts(&set_layouts);
            let descriptor_set = self
                .device
                .allocate_descriptor_sets(&alloc_info)
                .map_err(ComputeExecutorError::AllocateDescriptorSets)?[0];

            let mut buffer_refs = request
                .bindings
                .iter()
                .zip(buffers.iter())
                .collect::<Vec<_>>();
            buffer_refs.sort_by_key(|(binding, _)| binding.binding_slot);
            let buffer_infos: Vec<_> = buffer_refs
                .iter()
                .map(|(_, buffer)| {
                    vk::DescriptorBufferInfo::builder()
                        .buffer(buffer.buffer)
                        .offset(0)
                        .range(buffer.size)
                        .build()
                })
                .collect();
            let writes: Vec<_> = buffer_refs
                .iter()
                .enumerate()
                .map(|(index, (binding, _))| {
                    vk::WriteDescriptorSet::builder()
                        .dst_set(descriptor_set)
                        .dst_binding(binding.binding_slot)
                        .descriptor_type(binding.descriptor_kind.descriptor_type())
                        .buffer_info(slice::from_ref(&buffer_infos[index]))
                        .build()
                })
                .collect();
            self.device.update_descriptor_sets(&writes, &[]);

            let command_buffer_info = vk::CommandBufferAllocateInfo::builder()
                .command_pool(self.command_pool)
                .level(vk::CommandBufferLevel::PRIMARY)
                .command_buffer_count(1);
            let command_buffer = self
                .device
                .allocate_command_buffers(&command_buffer_info)
                .map_err(ComputeExecutorError::AllocateCommandBuffers)?[0];
            resources.command_buffer = Some(command_buffer);

            let begin_info = vk::CommandBufferBeginInfo::builder()
                .flags(vk::CommandBufferUsageFlags::ONE_TIME_SUBMIT);
            self.device
                .begin_command_buffer(command_buffer, &begin_info)
                .map_err(ComputeExecutorError::BeginCommandBuffer)?;
            self.device
                .cmd_bind_pipeline(command_buffer, vk::PipelineBindPoint::COMPUTE, pipeline);
            self.device.cmd_bind_descriptor_sets(
                command_buffer,
                vk::PipelineBindPoint::COMPUTE,
                pipeline_layout,
                0,
                &[descriptor_set],
                &[],
            );
            self.device.cmd_dispatch(
                command_buffer,
                dispatch_group_count(request.dispatch_size[0], request.workgroup_size[0]),
                dispatch_group_count(request.dispatch_size[1], request.workgroup_size[1]),
                dispatch_group_count(request.dispatch_size[2], request.workgroup_size[2]),
            );

            // Emit pipeline barrier(s). When barrier_metadata is present from
            // orchestrate inference, use precise per-resource barriers.
            // Otherwise, fall back to a full pipeline drain for safety.
            if let Some(ref barrier_meta) = request.barrier_metadata {
                for barrier in &barrier_meta.barriers {
                    let src_stage =
                        vk::PipelineStageFlags::from_raw(barrier.src_stage_mask);
                    let dst_stage =
                        vk::PipelineStageFlags::from_raw(barrier.dst_stage_mask);
                    let src_access =
                        vk::AccessFlags::from_raw(barrier.src_access_mask);
                    let dst_access =
                        vk::AccessFlags::from_raw(barrier.dst_access_mask);

                    // Skip zero-mask barriers (defensive).
                    if src_stage.is_empty() || dst_stage.is_empty() {
                        continue;
                    }

                    let vk_barrier = vk::MemoryBarrier::builder()
                        .src_access_mask(src_access)
                        .dst_access_mask(dst_access)
                        .build();

                    self.device.cmd_pipeline_barrier(
                        command_buffer,
                        src_stage,
                        dst_stage,
                        vk::DependencyFlags::empty(),
                        &[vk_barrier],
                        &[],
                        &[],
                    );
                }
            } else {
                // Fallback: full pipeline drain (existing behavior).
                let barrier = vk::MemoryBarrier::builder()
                    .src_access_mask(vk::AccessFlags::SHADER_WRITE)
                    .dst_access_mask(vk::AccessFlags::HOST_READ)
                    .build();
                self.device.cmd_pipeline_barrier(
                    command_buffer,
                    vk::PipelineStageFlags::COMPUTE_SHADER,
                    vk::PipelineStageFlags::HOST,
                    vk::DependencyFlags::empty(),
                    &[barrier],
                    &[],
                    &[],
                );
            }
            self.device
                .end_command_buffer(command_buffer)
                .map_err(ComputeExecutorError::EndCommandBuffer)?;

            let submit_info =
                vk::SubmitInfo::builder().command_buffers(slice::from_ref(&command_buffer));
            self.device
                .queue_submit(self.queue, &[*submit_info], vk::Fence::null())
                .map_err(ComputeExecutorError::QueueSubmit)?;
            self.device
                .queue_wait_idle(self.queue)
                .map_err(ComputeExecutorError::QueueWaitIdle)?;

            let mut output_bindings = Vec::new();
            for (binding, buffer) in request.bindings.iter().zip(buffers.iter()) {
                if binding.access.is_output() {
                    let output = self.read_buffer_bytes(buffer, binding.bytes.len())?;
                    output_bindings.push((binding.binding_slot, output));
                }
            }

            let barrier_count = request
                .barrier_metadata
                .as_ref()
                .map(|meta| meta.barriers.len())
                .unwrap_or(1);
            let async_queue_used = if request.queue_policy == GpuQueuePolicy::PreferAsyncCompute
                && self.async_compute_queue.is_some()
            {
                1
            } else {
                0
            };

            Ok(GpuDispatchResult {
                dispatch_invocations: request
                    .dispatch_size
                    .iter()
                    .map(|value| *value as u64)
                    .product(),
                output_bindings,
                tensor_binding_count: request.tensor_binding_count,
                stream_binding_count: request.stream_binding_count,
                neural_node_count: request.neural_node_count,
                barrier_count,
                async_queue_used,
            })
        }
    }

    pub fn run_compute_case(
        &self,
        spirv: &[u8],
        entry_name: &str,
        case: &ComputeCase,
    ) -> Result<Vec<u8>, ComputeExecutorError> {
        let request = GpuDispatchRequest {
            module_name: entry_name.to_string(),
            entry_point: entry_name.to_string(),
            workgroup_size: [DEFAULT_WORKGROUP_SIZE_X, 1, 1],
            dispatch_size: [case.invocation_count.max(1), 1, 1],
            cuda_launch: GpuCudaLaunchOptions::default(),
            bindings: case
                .bindings
                .iter()
                .enumerate()
                .map(|(index, binding)| GpuDispatchBinding {
                    key: format!("binding_{index}"),
                    binding_slot: index as u32,
                    descriptor_kind: binding.kind(),
                    access: if index == case.output_binding {
                        GpuBindingAccess::Write
                    } else {
                        GpuBindingAccess::Read
                    },
                    bytes: binding.to_bytes(),
                    element_type: "f32".to_string(),
                    shape: vec![case.invocation_count.max(1) as i64],
                    strides: vec![1],
                })
                .collect(),
            tensor_binding_count: 0,
            stream_binding_count: 0,
            neural_node_count: 0,
            barrier_metadata: None,
            queue_policy: GpuQueuePolicy::Default,
        };
        let result = self.run_dispatch_request(spirv, &request)?;
        result
            .output_bindings
            .into_iter()
            .find(|(slot, _)| *slot == case.output_binding as u32)
            .map(|(_, bytes)| bytes)
            .ok_or(ComputeExecutorError::OutputBindingOutOfRange {
                binding: case.output_binding,
                len: case.bindings.len(),
            })
    }

    fn create_buffer<'a>(
        &'a self,
        binding_index: usize,
        binding: &GpuDispatchBinding,
    ) -> Result<GpuBuffer<'a>, ComputeExecutorError> {
        unsafe {
            if binding.bytes.is_empty() {
                return Err(ComputeExecutorError::EmptyBindingPayload {
                    binding: binding_index,
                });
            }

            let size = binding.bytes.len() as vk::DeviceSize;
            let buffer_info = vk::BufferCreateInfo::builder()
                .size(size)
                .usage(binding.descriptor_kind.buffer_usage())
                .sharing_mode(vk::SharingMode::EXCLUSIVE);
            let buffer = self
                .device
                .create_buffer(&buffer_info, None)
                .map_err(ComputeExecutorError::CreateBuffer)?;
            let requirements = self.device.get_buffer_memory_requirements(buffer);
            let memory_type_index = self
                .find_memory_type_index(
                    requirements.memory_type_bits,
                    vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
                )
                .ok_or(ComputeExecutorError::NoSuitableMemoryType)?;
            let alloc_info = vk::MemoryAllocateInfo::builder()
                .allocation_size(requirements.size)
                .memory_type_index(memory_type_index);
            let memory = self
                .device
                .allocate_memory(&alloc_info, None)
                .map_err(ComputeExecutorError::AllocateMemory)?;
            self.device
                .bind_buffer_memory(buffer, memory, 0)
                .map_err(ComputeExecutorError::BindBufferMemory)?;

            let mapped = self
                .device
                .map_memory(memory, 0, requirements.size, vk::MemoryMapFlags::empty())
                .map_err(ComputeExecutorError::MapMemory)? as *mut u8;
            let mapped_len = device_size_to_usize(requirements.size).map_err(|_| {
                ComputeExecutorError::DeviceSizeTooLarge {
                    size: requirements.size,
                }
            })?;
            std::ptr::write_bytes(mapped, 0, mapped_len);
            mapped.copy_from_nonoverlapping(binding.bytes.as_ptr(), binding.bytes.len());
            self.device.unmap_memory(memory);

            Ok(GpuBuffer {
                device: &self.device,
                buffer,
                memory,
                size: requirements.size,
            })
        }
    }

    fn read_buffer_bytes(
        &self,
        buffer: &GpuBuffer<'_>,
        byte_len: usize,
    ) -> Result<Vec<u8>, ComputeExecutorError> {
        let buffer_len = device_size_to_usize(buffer.size)
            .map_err(|_| ComputeExecutorError::DeviceSizeTooLarge { size: buffer.size })?;
        if buffer_len < byte_len {
            return Err(ComputeExecutorError::OutputBindingOutOfRange {
                binding: 0,
                len: buffer_len,
            });
        }

        unsafe {
            let mapped = self
                .device
                .map_memory(buffer.memory, 0, buffer.size, vk::MemoryMapFlags::empty())
                .map_err(ComputeExecutorError::MapMemory)? as *const u8;
            let data = slice::from_raw_parts(mapped, byte_len).to_vec();
            self.device.unmap_memory(buffer.memory);
            Ok(data)
        }
    }

    fn find_memory_type_index(
        &self,
        type_bits: u32,
        required: vk::MemoryPropertyFlags,
    ) -> Option<u32> {
        let props = unsafe {
            self.instance
                .get_physical_device_memory_properties(self.physical_device)
        };
        for index in 0..props.memory_type_count {
            let bit = 1u32 << index;
            let memory_type = props.memory_types[index as usize];
            if (type_bits & bit) != 0 && memory_type.property_flags.contains(required) {
                return Some(index);
            }
        }
        None
    }
}

impl Drop for VulkanComputeExecutor {
    fn drop(&mut self) {
        unsafe {
            let _ = self.device.device_wait_idle();
            // Destroy cached pipelines
            if let Ok(mut cache) = self.pipeline_cache.lock() {
                for (_, cached) in cache.drain() {
                    self.device.destroy_pipeline(cached.pipeline, None);
                }
            }
            if let Some(async_pool) = self.async_command_pool {
                self.device.destroy_command_pool(async_pool, None);
            }
            self.device.destroy_command_pool(self.command_pool, None);
            self.device.destroy_device(None);
            self.instance.destroy_instance(None);
        }
    }
}

#[no_mangle]
pub extern "C" fn kain_gpu_runtime_create(_config: *const GpuComputeExecutorConfig) -> *mut c_void {
    match VulkanComputeExecutor::try_new() {
        Ok(executor) => Box::into_raw(Box::new(executor)) as *mut c_void,
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn kain_gpu_runtime_destroy(handle: *mut c_void) {
    if handle.is_null() {
        return;
    }
    unsafe {
        drop(Box::from_raw(handle as *mut VulkanComputeExecutor));
    }
}

#[no_mangle]
pub extern "C" fn kain_gpu_runtime_dispatch_primary_compute(
    handle: *mut c_void,
    request: *const GpuRuntimeDispatchRequest,
    out_result: *mut GpuRuntimeDispatchResult,
) -> i32 {
    let Some(result) = (unsafe { out_result.as_mut() }) else {
        return -1;
    };
    *result = empty_dispatch_result();

    if handle.is_null() || request.is_null() {
        write_result_message(result, "runtime handle or request was null");
        return -1;
    }

    let executor = unsafe { &*(handle as *mut VulkanComputeExecutor) };
    let request = unsafe { &*request };

    let shader_bundle_path = match c_string_arg(request.shader_bundle_path) {
        Ok(value) => PathBuf::from(value),
        Err(err) => {
            write_result_message(result, &err.to_string());
            return -1;
        }
    };
    let compute_residency_path = match c_string_arg(request.compute_residency_path) {
        Ok(value) => PathBuf::from(value),
        Err(err) => {
            write_result_message(result, &err.to_string());
            return -1;
        }
    };
    let compute_key = match c_string_arg(request.compute_key) {
        Ok(value) => value,
        Err(err) => {
            write_result_message(result, &err.to_string());
            return -1;
        }
    };

    let dispatch_override = ffi_dispatch_size_override(request.dispatch_size);

    // Parse barrier JSON from the C FFI request struct (may be NULL).
    let barrier_metadata = c_string_arg(request.barrier_json)
        .ok()
        .and_then(|json| BarrierMetadata::from_json(&json));

    match dispatch_request_from_sidecars(
        &shader_bundle_path,
        &compute_residency_path,
        &compute_key,
        dispatch_override,
    )
    .and_then(|(spirv, mut dispatch_request, output_targets)| {
        // Inject barrier metadata parsed from the C FFI request.
        dispatch_request.barrier_metadata = barrier_metadata;
        executor
            .run_dispatch_request(&spirv, &dispatch_request)
            .and_then(|dispatch| {
                // Flush output bindings back to their residency sidecar files so that the
                // Kain simulation loop sees the GPU-computed results on the next read.
                persist_spirv_output_bindings(&output_targets, &dispatch.output_bindings)?;
                Ok(dispatch)
            })
    })
    {
        Ok(dispatch) => {
            populate_dispatch_result(result, &dispatch, "dispatch ok");
            0
        }
        Err(err) => {
            result.status_code = -1;
            write_result_message(result, &err.to_string());
            -1
        }
    }
}

fn dispatch_request_from_sidecars(
    shader_bundle_path: &Path,
    compute_residency_path: &Path,
    compute_key: &str,
    dispatch_override: Option<[u32; 3]>,
) -> Result<(Vec<u8>, GpuDispatchRequest, Vec<OutputBindingTarget>), ComputeExecutorError> {
    let shader_bundle_json =
        fs::read_to_string(shader_bundle_path).map_err(|err| ComputeExecutorError::ReadFile {
            path: shader_bundle_path.display().to_string(),
            message: err.to_string(),
        })?;
    let shader_bundle = shader_artifact_bundle_from_json(&shader_bundle_json).map_err(|err| {
        ComputeExecutorError::ParseShaderBundle {
            path: shader_bundle_path.display().to_string(),
            message: err.to_string(),
        }
    })?;

    let residency_json = fs::read_to_string(compute_residency_path).map_err(|err| {
        ComputeExecutorError::ReadFile {
            path: compute_residency_path.display().to_string(),
            message: err.to_string(),
        }
    })?;
    let residency: ComputeResidencyBundle =
        serde_json::from_str(&residency_json).map_err(|err| {
            ComputeExecutorError::ParseComputeResidency {
                path: compute_residency_path.display().to_string(),
                message: err.to_string(),
            }
        })?;
    let entry = residency
        .compute_shaders
        .iter()
        .find(|entry| entry.key == compute_key)
        .ok_or_else(|| ComputeExecutorError::MissingComputeKey {
            compute_key: compute_key.to_string(),
            path: compute_residency_path.display().to_string(),
        })?;
    let module = shader_bundle
        .spirv_modules
        .iter()
        .find(|module| module.module_name == entry.module_name)
        .ok_or_else(|| ComputeExecutorError::MissingSpirvModule {
            module_name: entry.module_name.clone(),
            path: shader_bundle_path.display().to_string(),
        })?;
    let spirv =
        decode_hex(&module.bytes_hex).map_err(|message| ComputeExecutorError::InvalidSpirvHex {
            module_name: entry.module_name.clone(),
            message,
        })?;

    let residency_root = compute_residency_path
        .parent()
        .unwrap_or_else(|| Path::new("."));
    let mut bindings = Vec::with_capacity(entry.bindings.len());
    let mut output_targets: Vec<OutputBindingTarget> = Vec::new();
    for binding in &entry.bindings {
        let payload_path = residency_root.join(&binding.payload_file);
        let bytes = fs::read(&payload_path).map_err(|err| ComputeExecutorError::ReadFile {
            path: payload_path.display().to_string(),
            message: err.to_string(),
        })?;
        if binding.contract != "kain.shared.buffer" {
            return Err(ComputeExecutorError::UnsupportedSharedBufferContract {
                binding: binding.key.clone(),
                value: binding.contract.clone(),
            });
        }
        let metadata = SharedBufferMetadata {
            element_type: binding.element_type.clone(),
            element_size: gpu_storage_element_stride_bytes(&binding.element_type).unwrap_or(4)
                as i64,
            shape: binding.shape.clone(),
            strides: binding.strides.clone(),
            format: Some(binding.element_type.clone()),
            mime_type: Some("application/octet-stream".to_string()),
            source_runtime: "compute-residency".to_string(),
            source_backend: Some(residency.target.clone()),
            ownership: "owned".to_string(),
            labels: vec![binding.key.clone()],
        };
        let shared = KainSharedBuffer::owned(metadata, bytes.clone());
        let descriptor_kind = parse_descriptor_kind(&binding.descriptor_kind)?;
        let access = parse_access_mode(&binding.access_mode)?;
        let _view = shared_buffer_gpu_binding_view(
            &shared,
            match descriptor_kind {
                GpuDescriptorKind::StorageBuffer => InteropDescriptorKind::StorageBuffer,
                GpuDescriptorKind::UniformBuffer => InteropDescriptorKind::UniformBuffer,
            },
            match access {
                GpuBindingAccess::Read => InteropAccess::Read,
                GpuBindingAccess::Write => InteropAccess::Write,
                GpuBindingAccess::ReadWrite => InteropAccess::ReadWrite,
            },
        )
        .map_err(|err| ComputeExecutorError::InvalidSharedBufferBinding {
            binding: binding.key.clone(),
            message: err.to_string(),
        })?;

        if access.is_output() {
            output_targets.push(OutputBindingTarget {
                slot: binding.slot,
                payload_path: payload_path.clone(),
                byte_length: bytes.len(),
            });
        }
        bindings.push(GpuDispatchBinding {
            key: binding.key.clone(),
            binding_slot: binding.slot,
            descriptor_kind,
            access,
            bytes,
            element_type: binding.element_type.clone(),
            shape: binding.shape.clone(),
            strides: binding.strides.clone(),
        });
    }

    Ok((
        spirv,
        GpuDispatchRequest {
            module_name: entry.module_name.clone(),
            entry_point: entry.entry_point.clone(),
            workgroup_size: entry
                .workgroup_size
                .unwrap_or([DEFAULT_WORKGROUP_SIZE_X, 1, 1]),
            dispatch_size: dispatch_override
                .or(entry.dispatch_size)
                .unwrap_or([1, 1, 1]),
            cuda_launch: GpuCudaLaunchOptions::default(),
            bindings,
            tensor_binding_count: entry.tensor_binding_count,
            stream_binding_count: entry.stream_binding_count,
            neural_node_count: entry.neural_node_count,
            barrier_metadata: None,
            queue_policy: GpuQueuePolicy::Default,
        },
        output_targets,
    ))
}

pub fn ffi_dispatch_size_override(dispatch_size: [u32; 3]) -> Option<[u32; 3]> {
    if dispatch_size.iter().all(|value| *value > 0) {
        Some(dispatch_size)
    } else {
        None
    }
}

fn parse_descriptor_kind(value: &str) -> Result<GpuDescriptorKind, ComputeExecutorError> {
    match value {
        "storage_buffer" => Ok(GpuDescriptorKind::StorageBuffer),
        "uniform_buffer" => Ok(GpuDescriptorKind::UniformBuffer),
        other => Err(ComputeExecutorError::UnsupportedDescriptorKind {
            value: other.to_string(),
        }),
    }
}

fn parse_access_mode(value: &str) -> Result<GpuBindingAccess, ComputeExecutorError> {
    match value {
        "read" => Ok(GpuBindingAccess::Read),
        "write" => Ok(GpuBindingAccess::Write),
        "read_write" => Ok(GpuBindingAccess::ReadWrite),
        other => Err(ComputeExecutorError::UnsupportedAccessMode {
            value: other.to_string(),
        }),
    }
}

pub(crate) fn c_string_arg(raw: *const c_char) -> Result<String, ComputeExecutorError> {
    if raw.is_null() {
        return Err(ComputeExecutorError::ReadFile {
            path: "<null>".to_string(),
            message: "null c string".to_string(),
        });
    }
    unsafe { CStr::from_ptr(raw) }
        .to_str()
        .map(|value| value.to_string())
        .map_err(|err| ComputeExecutorError::ReadFile {
            path: "<c-string>".to_string(),
            message: err.to_string(),
        })
}

pub(crate) fn empty_dispatch_result() -> GpuRuntimeDispatchResult {
    GpuRuntimeDispatchResult {
        status_code: -1,
        dispatch_invocations: 0,
        tensor_binding_count: 0,
        stream_binding_count: 0,
        neural_node_count: 0,
        output_binding_count: 0,
        total_output_bytes: 0,
        barrier_count: 0,
        async_queue_used: 0,
        message: [0; 256],
    }
}

pub(crate) fn populate_dispatch_result(
    result: &mut GpuRuntimeDispatchResult,
    dispatch: &GpuDispatchResult,
    message: &str,
) {
    result.status_code = 0;
    result.dispatch_invocations = dispatch.dispatch_invocations;
    result.tensor_binding_count = dispatch.tensor_binding_count as u32;
    result.stream_binding_count = dispatch.stream_binding_count as u32;
    result.neural_node_count = dispatch.neural_node_count as u32;
    result.output_binding_count = dispatch.output_bindings.len() as u32;
    result.total_output_bytes = dispatch
        .output_bindings
        .iter()
        .map(|(_, bytes)| bytes.len() as u64)
        .sum();
    result.barrier_count = dispatch.barrier_count as u32;
    result.async_queue_used = dispatch.async_queue_used as u32;
    write_result_message(result, message);
}

pub(crate) fn write_result_message(result: &mut GpuRuntimeDispatchResult, message: &str) {
    let bytes = message.as_bytes();
    let len = bytes.len().min(result.message.len().saturating_sub(1));
    for item in &mut result.message {
        *item = 0;
    }
    for (index, byte) in bytes.iter().take(len).enumerate() {
        result.message[index] = *byte as c_char;
    }
}

fn dispatch_group_count(dispatch: u32, workgroup: u32) -> u32 {
    let safe_dispatch = dispatch.max(1);
    let safe_workgroup = workgroup.max(1);
    ((safe_dispatch - 1) / safe_workgroup) + 1
}

fn decode_hex(input: &str) -> Result<Vec<u8>, String> {
    if input.len() % 2 != 0 {
        return Err("hex length must be even".to_string());
    }
    let mut bytes = Vec::with_capacity(input.len() / 2);
    for chunk in input.as_bytes().chunks_exact(2) {
        let value = std::str::from_utf8(chunk)
            .map_err(|err| err.to_string())
            .and_then(|pair| u8::from_str_radix(pair, 16).map_err(|err| err.to_string()))?;
        bytes.push(value);
    }
    Ok(bytes)
}

fn bytes_to_words(bytes: &[u8]) -> Result<Vec<u32>, ComputeExecutorError> {
    if bytes.len() % 4 != 0 {
        return Err(ComputeExecutorError::SpirvMisaligned { len: bytes.len() });
    }
    Ok(bytes
        .chunks_exact(4)
        .map(|chunk| u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]))
        .collect())
}

fn device_size_to_usize(size: vk::DeviceSize) -> Result<usize, ()> {
    usize::try_from(size).map_err(|_| ())
}
