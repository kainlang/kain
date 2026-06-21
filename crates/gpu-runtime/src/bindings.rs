use ash::vk;
use serde::Deserialize;

/// Barrier metadata deserialized from the JSON passed via the ABI
/// (`abi_gpu_dispatch_ext`'s barrier_json parameter). When present,
/// the executor emits precise pipeline barriers instead of a full
/// pipeline drain.
#[derive(Clone, Debug, Deserialize)]
pub struct BarrierMetadata {
    pub barriers: Vec<BarrierDescription>,
}

impl BarrierMetadata {
    pub fn from_json(json: &str) -> Option<Self> {
        serde_json::from_str(json).ok()
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct BarrierDescription {
    pub from_stage: String,
    pub to_stage: String,
    pub src_stage_mask: u32,
    pub dst_stage_mask: u32,
    pub src_access_mask: u32,
    pub dst_access_mask: u32,
}

/// Queue selection policy for dispatching GPU compute work.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum GpuQueuePolicy {
    /// Use the default compute queue (always available).
    #[default]
    Default,
    /// Prefer an async compute queue when the GPU supports one.
    PreferAsyncCompute,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GpuDescriptorKind {
    StorageBuffer,
    UniformBuffer,
}

impl GpuDescriptorKind {
    pub fn as_str(self) -> &'static str {
        match self {
            GpuDescriptorKind::StorageBuffer => "storage_buffer",
            GpuDescriptorKind::UniformBuffer => "uniform_buffer",
        }
    }

    pub(crate) fn descriptor_type(self) -> vk::DescriptorType {
        match self {
            GpuDescriptorKind::StorageBuffer => vk::DescriptorType::STORAGE_BUFFER,
            GpuDescriptorKind::UniformBuffer => vk::DescriptorType::UNIFORM_BUFFER,
        }
    }

    pub(crate) fn buffer_usage(self) -> vk::BufferUsageFlags {
        match self {
            GpuDescriptorKind::StorageBuffer => vk::BufferUsageFlags::STORAGE_BUFFER,
            GpuDescriptorKind::UniformBuffer => vk::BufferUsageFlags::UNIFORM_BUFFER,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GpuBindingAccess {
    Read,
    Write,
    ReadWrite,
}

impl GpuBindingAccess {
    pub fn as_str(self) -> &'static str {
        match self {
            GpuBindingAccess::Read => "read",
            GpuBindingAccess::Write => "write",
            GpuBindingAccess::ReadWrite => "read_write",
        }
    }

    pub fn is_output(self) -> bool {
        matches!(self, GpuBindingAccess::Write | GpuBindingAccess::ReadWrite)
    }
}

#[derive(Clone, Debug)]
pub struct ComputeBinding {
    kind: GpuDescriptorKind,
    bytes: Vec<u8>,
}

impl ComputeBinding {
    pub fn storage_f32(values: &[f32]) -> Self {
        Self::new(
            GpuDescriptorKind::StorageBuffer,
            values
                .iter()
                .flat_map(|value| value.to_le_bytes())
                .collect(),
        )
    }

    pub fn storage_u32(values: &[u32]) -> Self {
        Self::new(
            GpuDescriptorKind::StorageBuffer,
            values
                .iter()
                .flat_map(|value| value.to_le_bytes())
                .collect(),
        )
    }

    pub fn storage_i32(values: &[i32]) -> Self {
        Self::new(
            GpuDescriptorKind::StorageBuffer,
            values
                .iter()
                .flat_map(|value| value.to_le_bytes())
                .collect(),
        )
    }

    pub fn uniform_f32(values: &[f32]) -> Self {
        Self::new(
            GpuDescriptorKind::UniformBuffer,
            values
                .iter()
                .flat_map(|value| value.to_le_bytes())
                .collect(),
        )
    }

    pub fn uniform_u32(values: &[u32]) -> Self {
        Self::new(
            GpuDescriptorKind::UniformBuffer,
            values
                .iter()
                .flat_map(|value| value.to_le_bytes())
                .collect(),
        )
    }

    pub fn storage_bytes(bytes: Vec<u8>) -> Self {
        Self::new(GpuDescriptorKind::StorageBuffer, bytes)
    }

    pub fn uniform_bytes(bytes: Vec<u8>) -> Self {
        Self::new(GpuDescriptorKind::UniformBuffer, bytes)
    }

    pub(crate) fn kind(&self) -> GpuDescriptorKind {
        self.kind
    }

    pub(crate) fn to_bytes(&self) -> Vec<u8> {
        self.bytes.clone()
    }

    fn new(kind: GpuDescriptorKind, bytes: Vec<u8>) -> Self {
        Self { kind, bytes }
    }
}

#[derive(Clone, Debug)]
pub struct GpuDispatchBinding {
    pub key: String,
    pub binding_slot: u32,
    pub descriptor_kind: GpuDescriptorKind,
    pub access: GpuBindingAccess,
    pub bytes: Vec<u8>,
    pub element_type: String,
    pub shape: Vec<i64>,
    pub strides: Vec<i64>,
}

#[derive(Clone, Debug)]
pub struct GpuDispatchRequest {
    pub module_name: String,
    pub entry_point: String,
    pub workgroup_size: [u32; 3],
    pub dispatch_size: [u32; 3],
    pub cuda_launch: GpuCudaLaunchOptions,
    pub bindings: Vec<GpuDispatchBinding>,
    pub tensor_binding_count: usize,
    pub stream_binding_count: usize,
    pub neural_node_count: usize,
    /// Barrier metadata for precise pipeline barriers.
    /// None = use full pipeline drain fallback.
    pub barrier_metadata: Option<BarrierMetadata>,
    /// Queue selection policy for async compute routing.
    pub queue_policy: GpuQueuePolicy,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GpuCudaLaunchOptions {
    pub dynamic_shared_memory_bytes: u32,
    pub stream_policy: GpuCudaStreamPolicy,
    pub graph_policy: GpuCudaGraphPolicy,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum GpuCudaStreamPolicy {
    #[default]
    Default,
    NonBlocking,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum GpuCudaGraphPolicy {
    #[default]
    Disabled,
    CaptureOnce,
}

#[derive(Clone, Debug, Default)]
pub struct GpuDispatchResult {
    pub dispatch_invocations: u64,
    pub output_bindings: Vec<(u32, Vec<u8>)>,
    pub tensor_binding_count: usize,
    pub stream_binding_count: usize,
    pub neural_node_count: usize,
}

#[derive(Clone, Debug)]
pub enum ExpectedOutput {
    F32 { values: Vec<f32>, epsilon: f32 },
    U32(Vec<u32>),
    I32(Vec<i32>),
}

impl ExpectedOutput {
    pub fn byte_len(&self) -> usize {
        match self {
            ExpectedOutput::F32 { values, .. } => values.len() * std::mem::size_of::<f32>(),
            ExpectedOutput::U32(values) => values.len() * std::mem::size_of::<u32>(),
            ExpectedOutput::I32(values) => values.len() * std::mem::size_of::<i32>(),
        }
    }

    pub fn assert_matches(&self, bytes: &[u8]) {
        match self {
            ExpectedOutput::F32 { values, epsilon } => {
                let actual = decode_f32s(bytes);
                approx_eq_slice(&actual, values, *epsilon);
            }
            ExpectedOutput::U32(values) => {
                let actual = decode_u32s(bytes);
                assert_eq!(actual, *values, "u32 output mismatch");
            }
            ExpectedOutput::I32(values) => {
                let actual = decode_i32s(bytes);
                assert_eq!(actual, *values, "i32 output mismatch");
            }
        }
    }
}

#[derive(Clone, Debug)]
pub struct ComputeCase {
    pub name: &'static str,
    pub invocation_count: u32,
    pub bindings: Vec<ComputeBinding>,
    pub output_binding: usize,
    pub expected_output: ExpectedOutput,
}

fn decode_f32s(bytes: &[u8]) -> Vec<f32> {
    assert_eq!(
        bytes.len() % 4,
        0,
        "f32 byte slice length must be divisible by 4"
    );
    bytes
        .chunks_exact(4)
        .map(|chunk| f32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]))
        .collect()
}

fn decode_u32s(bytes: &[u8]) -> Vec<u32> {
    assert_eq!(
        bytes.len() % 4,
        0,
        "u32 byte slice length must be divisible by 4"
    );
    bytes
        .chunks_exact(4)
        .map(|chunk| u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]))
        .collect()
}

fn decode_i32s(bytes: &[u8]) -> Vec<i32> {
    assert_eq!(
        bytes.len() % 4,
        0,
        "i32 byte slice length must be divisible by 4"
    );
    bytes
        .chunks_exact(4)
        .map(|chunk| i32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]))
        .collect()
}

fn approx_eq_slice(actual: &[f32], expected: &[f32], epsilon: f32) {
    assert_eq!(actual.len(), expected.len(), "slice length mismatch");
    for (index, (a, e)) in actual.iter().zip(expected.iter()).enumerate() {
        let delta = (a - e).abs();
        assert!(
            delta <= epsilon,
            "value mismatch at index {index}: actual={a}, expected={e}, delta={delta}, epsilon={epsilon}"
        );
    }
}
