use ash::{vk, Entry};
use gpu::generate_spirv;
use kain_core::comptime;
use kain_core::diagnostics::SpanMapper;
use kain_core::stdlib;
use kain_core::types;
use kain_core::types::TypedItem;
use kain_core::{CompileTarget, Lexer, Parser, TypedProgram};
use std::ffi::CString;
use std::slice;

fn typed_program_for_spirv(source: &str) -> TypedProgram {
    let stdlib_src = stdlib::load_stdlib_for_target(CompileTarget::Spirv);
    let full_source = format!("{}\n{}", stdlib_src, source);
    let span_mapper = SpanMapper::new(&full_source);
    let tokens = Lexer::new(&full_source)
        .tokenize()
        .expect("tokenize failed");
    let mut ast = Parser::new(&tokens, &span_mapper, "<spirv-exec>")
        .parse()
        .expect("parse failed");
    comptime::eval_program(&mut ast).expect("comptime failed");
    types::check(&ast, &span_mapper, "<spirv-exec>").expect("typecheck failed")
}

fn compile_spirv_with_entry(source: &str) -> (Vec<u8>, String) {
    let typed = typed_program_for_spirv(source);
    let entry_name = typed
        .items
        .iter()
        .find_map(|item| match item {
            TypedItem::Shader(shader) => Some(shader.ast.name.clone()),
            _ => None,
        })
        .expect("expected a shader item for SPIR-V execution test");
    let bytes = generate_spirv(&typed).expect("spirv generation failed");
    (bytes, entry_name)
}

fn bytes_to_words(bytes: &[u8]) -> Vec<u32> {
    assert_eq!(bytes.len() % 4, 0, "SPIR-V bytecode must be 4-byte aligned");
    bytes
        .chunks_exact(4)
        .map(|chunk| u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]))
        .collect()
}

fn encode_f32s(values: &[f32]) -> Vec<u8> {
    values
        .iter()
        .flat_map(|value| value.to_le_bytes())
        .collect()
}

fn encode_u32s(values: &[u32]) -> Vec<u8> {
    values
        .iter()
        .flat_map(|value| value.to_le_bytes())
        .collect()
}

fn encode_i32s(values: &[i32]) -> Vec<u8> {
    values
        .iter()
        .flat_map(|value| value.to_le_bytes())
        .collect()
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

#[derive(Clone, Copy)]
enum BindingKind {
    Storage,
    Uniform,
}

impl BindingKind {
    fn descriptor_type(self) -> vk::DescriptorType {
        match self {
            BindingKind::Storage => vk::DescriptorType::STORAGE_BUFFER,
            BindingKind::Uniform => vk::DescriptorType::UNIFORM_BUFFER,
        }
    }

    fn buffer_usage(self) -> vk::BufferUsageFlags {
        match self {
            BindingKind::Storage => vk::BufferUsageFlags::STORAGE_BUFFER,
            BindingKind::Uniform => vk::BufferUsageFlags::UNIFORM_BUFFER,
        }
    }
}

#[derive(Clone)]
enum BufferPayload {
    F32(Vec<f32>),
    U32(Vec<u32>),
    I32(Vec<i32>),
}

impl BufferPayload {
    fn to_bytes(&self) -> Vec<u8> {
        match self {
            BufferPayload::F32(values) => encode_f32s(values),
            BufferPayload::U32(values) => encode_u32s(values),
            BufferPayload::I32(values) => encode_i32s(values),
        }
    }
}

#[derive(Clone)]
struct ExecBinding {
    kind: BindingKind,
    payload: BufferPayload,
}

impl ExecBinding {
    fn storage_f32(values: &[f32]) -> Self {
        Self {
            kind: BindingKind::Storage,
            payload: BufferPayload::F32(values.to_vec()),
        }
    }

    fn storage_u32(values: &[u32]) -> Self {
        Self {
            kind: BindingKind::Storage,
            payload: BufferPayload::U32(values.to_vec()),
        }
    }

    fn storage_i32(values: &[i32]) -> Self {
        Self {
            kind: BindingKind::Storage,
            payload: BufferPayload::I32(values.to_vec()),
        }
    }

    fn uniform_f32(values: &[f32]) -> Self {
        Self {
            kind: BindingKind::Uniform,
            payload: BufferPayload::F32(values.to_vec()),
        }
    }

    fn uniform_u32(values: &[u32]) -> Self {
        Self {
            kind: BindingKind::Uniform,
            payload: BufferPayload::U32(values.to_vec()),
        }
    }
}

enum ExpectedOutput {
    F32 { values: Vec<f32>, epsilon: f32 },
    U32(Vec<u32>),
    I32(Vec<i32>),
}

impl ExpectedOutput {
    fn byte_len(&self) -> usize {
        match self {
            ExpectedOutput::F32 { values, .. } => values.len() * std::mem::size_of::<f32>(),
            ExpectedOutput::U32(values) => values.len() * std::mem::size_of::<u32>(),
            ExpectedOutput::I32(values) => values.len() * std::mem::size_of::<i32>(),
        }
    }

    fn assert_matches(&self, bytes: &[u8]) {
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

struct ExecCase {
    name: &'static str,
    source: &'static str,
    invocation_count: u32,
    bindings: Vec<ExecBinding>,
    output_binding: usize,
    expected_output: ExpectedOutput,
}

struct VulkanContext {
    _entry: Entry,
    instance: ash::Instance,
    physical_device: vk::PhysicalDevice,
    device: ash::Device,
    queue: vk::Queue,
    command_pool: vk::CommandPool,
}

struct TestBuffer {
    buffer: vk::Buffer,
    memory: vk::DeviceMemory,
    size: vk::DeviceSize,
    kind: BindingKind,
}

impl VulkanContext {
    fn try_new() -> Option<Self> {
        unsafe {
            let entry = Entry::load().ok()?;
            let app_name = CString::new("kain-spirv-exec-test").ok()?;
            let engine_name = CString::new("kain").ok()?;
            let app_info = vk::ApplicationInfo::builder()
                .application_name(&app_name)
                .application_version(0)
                .engine_name(&engine_name)
                .engine_version(0)
                .api_version(vk::make_api_version(0, 1, 1, 0));
            let instance_info = vk::InstanceCreateInfo::builder().application_info(&app_info);
            let instance = entry.create_instance(&instance_info, None).ok()?;

            let physical_devices = instance.enumerate_physical_devices().ok()?;
            let mut selected = None;
            for physical_device in physical_devices {
                let queue_families =
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
            let (physical_device, queue_family_index) = selected?;

            let priorities = [1.0f32];
            let queue_info = [vk::DeviceQueueCreateInfo::builder()
                .queue_family_index(queue_family_index)
                .queue_priorities(&priorities)
                .build()];
            let device_info = vk::DeviceCreateInfo::builder().queue_create_infos(&queue_info);
            let device = instance
                .create_device(physical_device, &device_info, None)
                .ok()?;
            let queue = device.get_device_queue(queue_family_index, 0);
            let pool_info = vk::CommandPoolCreateInfo::builder()
                .queue_family_index(queue_family_index)
                .flags(vk::CommandPoolCreateFlags::RESET_COMMAND_BUFFER);
            let command_pool = device.create_command_pool(&pool_info, None).ok()?;

            Some(Self {
                _entry: entry,
                instance,
                physical_device,
                device,
                queue,
                command_pool,
            })
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

    fn create_buffer(&self, binding: &ExecBinding) -> Result<TestBuffer, String> {
        unsafe {
            let bytes = binding.payload.to_bytes();
            let size = bytes.len() as vk::DeviceSize;
            let buffer_info = vk::BufferCreateInfo::builder()
                .size(size)
                .usage(binding.kind.buffer_usage())
                .sharing_mode(vk::SharingMode::EXCLUSIVE);
            let buffer = self
                .device
                .create_buffer(&buffer_info, None)
                .map_err(|e| format!("create_buffer failed: {e:?}"))?;
            let requirements = self.device.get_buffer_memory_requirements(buffer);
            let memory_type_index = self
                .find_memory_type_index(
                    requirements.memory_type_bits,
                    vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
                )
                .ok_or_else(|| {
                    "no suitable HOST_VISIBLE|HOST_COHERENT memory type found".to_string()
                })?;
            let alloc_info = vk::MemoryAllocateInfo::builder()
                .allocation_size(requirements.size)
                .memory_type_index(memory_type_index);
            let memory = self
                .device
                .allocate_memory(&alloc_info, None)
                .map_err(|e| format!("allocate_memory failed: {e:?}"))?;
            self.device
                .bind_buffer_memory(buffer, memory, 0)
                .map_err(|e| format!("bind_buffer_memory failed: {e:?}"))?;

            let mapped =
                self.device
                    .map_memory(memory, 0, size, vk::MemoryMapFlags::empty())
                    .map_err(|e| format!("map_memory failed: {e:?}"))? as *mut u8;
            mapped.copy_from_nonoverlapping(bytes.as_ptr(), bytes.len());
            self.device.unmap_memory(memory);

            Ok(TestBuffer {
                buffer,
                memory,
                size,
                kind: binding.kind,
            })
        }
    }

    fn read_buffer_bytes(&self, buffer: &TestBuffer, byte_len: usize) -> Result<Vec<u8>, String> {
        unsafe {
            let read_len = byte_len.min(buffer.size as usize);
            let mapped =
                self.device
                    .map_memory(buffer.memory, 0, buffer.size, vk::MemoryMapFlags::empty())
                    .map_err(|e| format!("map_memory failed: {e:?}"))? as *const u8;
            let data = slice::from_raw_parts(mapped, read_len).to_vec();
            self.device.unmap_memory(buffer.memory);
            Ok(data)
        }
    }

    fn destroy_buffer(&self, buffer: &TestBuffer) {
        unsafe {
            self.device.destroy_buffer(buffer.buffer, None);
            self.device.free_memory(buffer.memory, None);
        }
    }

    fn run_compute_case(&self, case: &ExecCase) -> Result<Vec<u8>, String> {
        unsafe {
            let (spirv, entry_name_string) = compile_spirv_with_entry(case.source);
            let shader_words = bytes_to_words(&spirv);
            let module_info = vk::ShaderModuleCreateInfo::builder().code(&shader_words);
            let shader_module = self
                .device
                .create_shader_module(&module_info, None)
                .map_err(|e| format!("create_shader_module failed: {e:?}"))?;

            let buffers: Vec<TestBuffer> = case
                .bindings
                .iter()
                .map(|binding| self.create_buffer(binding))
                .collect::<Result<_, _>>()?;

            let layout_bindings: Vec<_> = buffers
                .iter()
                .enumerate()
                .map(|(binding, buffer)| {
                    vk::DescriptorSetLayoutBinding::builder()
                        .binding(binding as u32)
                        .descriptor_type(buffer.kind.descriptor_type())
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
                .map_err(|e| format!("create_descriptor_set_layout failed: {e:?}"))?;
            let set_layouts = [descriptor_set_layout];
            let pipeline_layout_info =
                vk::PipelineLayoutCreateInfo::builder().set_layouts(&set_layouts);
            let pipeline_layout = self
                .device
                .create_pipeline_layout(&pipeline_layout_info, None)
                .map_err(|e| format!("create_pipeline_layout failed: {e:?}"))?;

            let entry_name = CString::new(entry_name_string)
                .map_err(|_| "invalid shader entry name".to_string())?;
            let stage = vk::PipelineShaderStageCreateInfo::builder()
                .stage(vk::ShaderStageFlags::COMPUTE)
                .module(shader_module)
                .name(&entry_name);
            let pipeline_info = vk::ComputePipelineCreateInfo::builder()
                .stage(*stage)
                .layout(pipeline_layout);
            let compute_pipelines = self
                .device
                .create_compute_pipelines(vk::PipelineCache::null(), &[*pipeline_info], None)
                .map_err(|(_, e)| format!("create_compute_pipelines failed: {e:?}"))?;
            let pipeline = compute_pipelines[0];

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
                .map_err(|e| format!("create_descriptor_pool failed: {e:?}"))?;
            let alloc_info = vk::DescriptorSetAllocateInfo::builder()
                .descriptor_pool(descriptor_pool)
                .set_layouts(&set_layouts);
            let descriptor_set = self
                .device
                .allocate_descriptor_sets(&alloc_info)
                .map_err(|e| format!("allocate_descriptor_sets failed: {e:?}"))?[0];

            let buffer_infos: Vec<_> = buffers
                .iter()
                .map(|buffer| {
                    vk::DescriptorBufferInfo::builder()
                        .buffer(buffer.buffer)
                        .offset(0)
                        .range(buffer.size)
                        .build()
                })
                .collect();
            let writes: Vec<_> = buffer_infos
                .iter()
                .enumerate()
                .map(|(binding, info)| {
                    vk::WriteDescriptorSet::builder()
                        .dst_set(descriptor_set)
                        .dst_binding(binding as u32)
                        .descriptor_type(buffers[binding].kind.descriptor_type())
                        .buffer_info(slice::from_ref(info))
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
                .map_err(|e| format!("allocate_command_buffers failed: {e:?}"))?[0];
            let begin_info = vk::CommandBufferBeginInfo::builder()
                .flags(vk::CommandBufferUsageFlags::ONE_TIME_SUBMIT);
            self.device
                .begin_command_buffer(command_buffer, &begin_info)
                .map_err(|e| format!("begin_command_buffer failed: {e:?}"))?;
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
            let group_count_x = ((case.invocation_count.max(1) - 1) / 8) + 1;
            self.device
                .cmd_dispatch(command_buffer, group_count_x, 1, 1);

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
            self.device
                .end_command_buffer(command_buffer)
                .map_err(|e| format!("end_command_buffer failed: {e:?}"))?;

            let submit_info =
                vk::SubmitInfo::builder().command_buffers(slice::from_ref(&command_buffer));
            self.device
                .queue_submit(self.queue, &[*submit_info], vk::Fence::null())
                .map_err(|e| format!("queue_submit failed: {e:?}"))?;
            self.device
                .queue_wait_idle(self.queue)
                .map_err(|e| format!("queue_wait_idle failed: {e:?}"))?;

            let output = self.read_buffer_bytes(
                &buffers[case.output_binding],
                case.expected_output.byte_len(),
            )?;

            self.device
                .free_command_buffers(self.command_pool, &[command_buffer]);
            self.device.destroy_descriptor_pool(descriptor_pool, None);
            self.device.destroy_pipeline(pipeline, None);
            self.device.destroy_pipeline_layout(pipeline_layout, None);
            self.device
                .destroy_descriptor_set_layout(descriptor_set_layout, None);
            self.device.destroy_shader_module(shader_module, None);
            for buffer in &buffers {
                self.destroy_buffer(buffer);
            }

            Ok(output)
        }
    }
}

impl Drop for VulkanContext {
    fn drop(&mut self) {
        unsafe {
            let _ = self.device.device_wait_idle();
            self.device.destroy_command_pool(self.command_pool, None);
            self.device.destroy_device(None);
            self.instance.destroy_instance(None);
        }
    }
}

fn assert_exec_case(ctx: &VulkanContext, case: &ExecCase) {
    eprintln!("[spirv-exec] running case {}", case.name);
    let output = ctx
        .run_compute_case(case)
        .unwrap_or_else(|err| panic!("execution failed for {}: {err}", case.name));
    case.expected_output.assert_matches(&output);
}

fn execution_cases() -> Vec<ExecCase> {
    vec![
        ExecCase {
            name: "add_buffers",
            source: r#"
shader compute add_buffers() -> Void:
    uniform a: StorageBuffer<Float> @0
    uniform b: StorageBuffer<Float> @1
    uniform out_values: StorageBuffer<Float> @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= 4 as UInt:
        return

    out_values[idx] = a[idx] + b[idx]
    return
"#,
            invocation_count: 4,
            bindings: vec![
                ExecBinding::storage_f32(&[1.0, 2.0, 3.0, 4.0]),
                ExecBinding::storage_f32(&[10.0, 20.0, 30.0, 40.0]),
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0]),
            ],
            output_binding: 2,
            expected_output: ExpectedOutput::F32 {
                values: vec![11.0, 22.0, 33.0, 44.0],
                epsilon: 0.0001,
            },
        },
        ExecCase {
            name: "scalar_ctors_and_casts",
            source: r#"
shader compute scalar_cast_runtime() -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform out_values: StorageBuffer<Float> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= 4 as UInt:
        return

    let a = src[idx]
    let ai = Int(a)
    let au = UInt(ai)
    let af = Float(au)
    let mask = Bool(af)
    let mf = Float(mask)
    out_values[idx] = af + mf
    return
"#,
            invocation_count: 4,
            bindings: vec![
                ExecBinding::storage_f32(&[1.25, 0.0, 3.75, 4.1]),
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0]),
            ],
            output_binding: 1,
            expected_output: ExpectedOutput::F32 {
                values: vec![2.0, 0.0, 4.0, 5.0],
                epsilon: 0.0001,
            },
        },
        ExecCase {
            name: "builtin_dispatch_group_shapes",
            source: r#"
shader compute builtin_group_runtime() -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform out_values: StorageBuffer<Float> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= 16 as UInt:
        return
    if dispatch_thread_id.y > UInt(0):
        return

    let tile = group_id.x
    let lane = group_index
    out_values[idx] = src[idx] + tile as Float * 100.0 + lane as Float
    return
"#,
            invocation_count: 16,
            bindings: vec![
                ExecBinding::storage_f32(&[
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                ]),
                ExecBinding::storage_f32(&[
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                ]),
            ],
            output_binding: 1,
            expected_output: ExpectedOutput::F32 {
                values: vec![
                    0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 100.0, 101.0, 102.0, 103.0, 104.0,
                    105.0, 106.0, 107.0,
                ],
                epsilon: 0.0001,
            },
        },
        ExecCase {
            name: "mixed_storage_and_uniform_scalars",
            source: r#"
shader compute uniform_branch_runtime() -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform out_values: StorageBuffer<Float> @1
    uniform bias: Float @2
    uniform threshold: Float @3
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= 4 as UInt:
        return

    let x = src[idx]
    let y = if x > threshold:
        x + bias
    else:
        x - bias
    out_values[idx] = y
    return
"#,
            invocation_count: 4,
            bindings: vec![
                ExecBinding::storage_f32(&[0.25, 0.5, 0.75, 1.0]),
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0]),
                ExecBinding::uniform_f32(&[0.25]),
                ExecBinding::uniform_f32(&[0.5]),
            ],
            output_binding: 1,
            expected_output: ExpectedOutput::F32 {
                values: vec![0.0, 0.25, 1.0, 1.25],
                epsilon: 0.0001,
            },
        },
        ExecCase {
            name: "storage_vec4_and_scalar_mix",
            source: r#"
shader compute vec4_mix_runtime() -> Void:
    uniform positions_a: StorageBuffer<Vec4> @0
    uniform positions_b: StorageBuffer<Vec4> @1
    uniform out_positions: StorageBuffer<Vec4> @2
    uniform alpha_per_joint: StorageBuffer<Float> @3
    uniform scale: Float @4
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= 2 as UInt:
        return

    let a = positions_a[idx]
    let b = positions_b[idx]
    let t = alpha_per_joint[idx]
    let p = mix(a.xyz, b.xyz, t) * scale
    let w = mix(a.w, b.w, t)
    out_positions[idx] = vec4(p.x, p.y, p.z, w)
    return
"#,
            invocation_count: 2,
            bindings: vec![
                ExecBinding::storage_f32(&[1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 2.0]),
                ExecBinding::storage_f32(&[0.0, 1.0, 0.0, 3.0, 0.0, 0.0, 1.0, 4.0]),
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
                ExecBinding::storage_f32(&[0.25, 0.5]),
                ExecBinding::uniform_f32(&[2.0]),
            ],
            output_binding: 2,
            expected_output: ExpectedOutput::F32 {
                values: vec![1.5, 0.5, 0.0, 1.5, 0.0, 1.0, 1.0, 3.0],
                epsilon: 0.0001,
            },
        },
        ExecCase {
            name: "nested_conditionals_and_uint_output",
            source: r#"
shader compute nested_uint_runtime() -> Void:
    uniform parents: StorageBuffer<Int> @0
    uniform out_flags: StorageBuffer<UInt> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= 4 as UInt:
        return

    let parent = parents[idx]
    if parent < 0:
        out_flags[idx] = UInt(0)
        return

    let p = parent as UInt
    if p > UInt(1):
        out_flags[idx] = p + UInt(100)
    else:
        out_flags[idx] = p + UInt(10)
    return
"#,
            invocation_count: 4,
            bindings: vec![
                ExecBinding::storage_i32(&[-1, 0, 1, 2]),
                ExecBinding::storage_u32(&[0, 0, 0, 0]),
            ],
            output_binding: 1,
            expected_output: ExpectedOutput::U32(vec![0, 10, 11, 102]),
        },
        ExecCase {
            name: "loop_range_start_end_and_while",
            source: r#"
shader compute loop_shapes_runtime() -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform out_values: StorageBuffer<Float> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= 4 as UInt:
        return

    let base = src[idx]
    let mut accum = 0.0
    for k in range(1, 4):
        accum = accum + base + k as Float

    let mut spin = 0.0
    while spin < 2.0:
        accum = accum + spin
        spin = spin + 1.0

    out_values[idx] = accum
    return
"#,
            invocation_count: 4,
            bindings: vec![
                ExecBinding::storage_f32(&[1.0, 2.0, 3.0, 4.0]),
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0]),
            ],
            output_binding: 1,
            expected_output: ExpectedOutput::F32 {
                values: vec![10.0, 13.0, 16.0, 19.0],
                epsilon: 0.0001,
            },
        },
        ExecCase {
            name: "precision_vector_math",
            source: r#"
shader compute precision_math_runtime() -> Void:
    uniform src_dir: StorageBuffer<Vec4> @0
    uniform out_values: StorageBuffer<Vec4> @1
    uniform eps: Float @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= 2 as UInt:
        return

    let v = src_dir[idx].xyz
    let tangent = normalize(v + vec3(eps, 0.0, 0.0))
    let right = cross(vec3(0.0, 0.0, 1.0), tangent)
    let align = dot(tangent, tangent)
    let soft = smoothstep(0.0, 1.0, 0.5)
    out_values[idx] = vec4(right.x, right.y, align, soft)
    return
"#,
            invocation_count: 2,
            bindings: vec![
                ExecBinding::storage_f32(&[1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0]),
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
                ExecBinding::uniform_f32(&[0.000001]),
            ],
            output_binding: 1,
            expected_output: ExpectedOutput::F32 {
                values: vec![0.0, 1.0, 1.0, 0.5, -1.0, 0.0, 1.0, 0.5],
                epsilon: 0.0002,
            },
        },
        ExecCase {
            name: "reduced_sculpt_runtime",
            source: r#"
shader compute sculpt_runtime_reduced() -> Void:
    uniform positions_a: StorageBuffer<Vec4> @0
    uniform positions_b: StorageBuffer<Vec4> @1
    uniform out_positions: StorageBuffer<Vec4> @2
    uniform alpha_per_joint: StorageBuffer<Float> @3
    uniform gain: Float @4
    uniform eps: Float @5
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let joint = dispatch_thread_id.x
    if joint >= 2 as UInt:
        return

    let a = positions_a[joint].xyz
    let b = positions_b[joint].xyz
    let t = alpha_per_joint[joint]
    let blended = mix(a, b, t)
    let normal = normalize(blended + vec3(eps, 0.0, 0.0))
    let displaced = blended + normal * gain
    out_positions[joint] = vec4(displaced.x, displaced.y, displaced.z, 1.0)
    return
"#,
            invocation_count: 2,
            bindings: vec![
                ExecBinding::storage_f32(&[1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0]),
                ExecBinding::storage_f32(&[0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0]),
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
                ExecBinding::storage_f32(&[0.0, 1.0]),
                ExecBinding::uniform_f32(&[0.5]),
                ExecBinding::uniform_f32(&[0.000001]),
            ],
            output_binding: 2,
            expected_output: ExpectedOutput::F32 {
                values: vec![1.5, 0.0, 0.0, 1.0, 0.0000005, 1.5, 0.0, 1.0],
                epsilon: 0.0002,
            },
        },
        ExecCase {
            name: "reduced_supermotion_runtime",
            source: r#"
shader compute supermotion_runtime_reduced() -> Void:
    uniform in_joints: StorageBuffer<Float> @0
    uniform parents: StorageBuffer<Int> @1
    uniform out_rotations: StorageBuffer<Float> @2
    uniform joint_count: UInt @3
    uniform eps: Float @4
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let joint = dispatch_thread_id.x
    if joint >= joint_count:
        return

    let base = joint * 4
    let parent = parents[joint]
    if parent < 0:
        out_rotations[base + 0] = 0.0
        out_rotations[base + 1] = 0.0
        out_rotations[base + 2] = 0.0
        out_rotations[base + 3] = 1.0
        return

    let pidx = parent as UInt
    let pbase = pidx * 4
    let dx = in_joints[base + 0] - in_joints[pbase + 0]
    let dy = in_joints[base + 1] - in_joints[pbase + 1]
    let dz = in_joints[base + 2] - in_joints[pbase + 2]
    let forward = normalize(vec3(dx, dy, dz) + vec3(eps, 0.0, 0.0))
    let align = abs(dot(forward, vec3(0.0, 1.0, 0.0)))
    let up_ref = if align > 0.95: vec3(1.0, 0.0, 0.0) else: vec3(0.0, 1.0, 0.0)
    let right = normalize(cross(up_ref, forward) + vec3(eps, 0.0, 0.0))

    out_rotations[base + 0] = right.x
    out_rotations[base + 1] = right.y
    out_rotations[base + 2] = right.z
    out_rotations[base + 3] = 1.0
    return
"#,
            invocation_count: 2,
            bindings: vec![
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0]),
                ExecBinding::storage_i32(&[-1, 0]),
                ExecBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
                ExecBinding::uniform_u32(&[2]),
                ExecBinding::uniform_f32(&[0.000001]),
            ],
            output_binding: 2,
            expected_output: ExpectedOutput::F32 {
                values: vec![0.0, 0.0, 0.0, 1.0, 1.0, 0.0, -0.000001, 1.0],
                epsilon: 0.001,
            },
        },
    ]
}

#[test]
fn spirv_execute_known_kernels() {
    let Some(ctx) = VulkanContext::try_new() else {
        eprintln!("[spirv-exec] Vulkan runtime unavailable; skipping execution tests");
        return;
    };

    for case in execution_cases() {
        assert_exec_case(&ctx, &case);
    }
}
