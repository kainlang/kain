use gpu::generate_spirv;
use kain_core::comptime;
use kain_core::diagnostics::SpanMapper;
use kain_core::stdlib;
use kain_core::types;
use kain_core::types::TypedItem;
use kain_core::{CompileTarget, Lexer, Parser, TypedProgram};
use kain_gpu_runtime::{ComputeBinding, ComputeCase, ExpectedOutput, VulkanComputeExecutor};

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

struct SpirvExecKernel {
    source: &'static str,
    case: ComputeCase,
}

fn assert_exec_case(executor: &VulkanComputeExecutor, case: &SpirvExecKernel) {
    eprintln!("[spirv-exec] running case {}", case.case.name);
    let (spirv, entry_name) = compile_spirv_with_entry(case.source);
    let output = executor
        .run_compute_case(&spirv, &entry_name, &case.case)
        .unwrap_or_else(|err| panic!("execution failed for {}: {err}", case.case.name));
    case.case.expected_output.assert_matches(&output);
}

fn execution_cases() -> Vec<SpirvExecKernel> {
    vec![
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "add_buffers",
                invocation_count: 4,
                bindings: vec![
                    ComputeBinding::storage_f32(&[1.0, 2.0, 3.0, 4.0]),
                    ComputeBinding::storage_f32(&[10.0, 20.0, 30.0, 40.0]),
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0]),
                ],
                output_binding: 2,
                expected_output: ExpectedOutput::F32 {
                    values: vec![11.0, 22.0, 33.0, 44.0],
                    epsilon: 0.0001,
                },
            },
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "scalar_ctors_and_casts",
                invocation_count: 4,
                bindings: vec![
                    ComputeBinding::storage_f32(&[1.25, 0.0, 3.75, 4.1]),
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0]),
                ],
                output_binding: 1,
                expected_output: ExpectedOutput::F32 {
                    values: vec![2.0, 0.0, 4.0, 5.0],
                    epsilon: 0.0001,
                },
            },
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "builtin_dispatch_group_shapes",
                invocation_count: 16,
                bindings: vec![
                    ComputeBinding::storage_f32(&[
                        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                        0.0,
                    ]),
                    ComputeBinding::storage_f32(&[
                        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                        0.0,
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
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "mixed_storage_and_uniform_scalars",
                invocation_count: 4,
                bindings: vec![
                    ComputeBinding::storage_f32(&[0.25, 0.5, 0.75, 1.0]),
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0]),
                    ComputeBinding::uniform_f32(&[0.25]),
                    ComputeBinding::uniform_f32(&[0.5]),
                ],
                output_binding: 1,
                expected_output: ExpectedOutput::F32 {
                    values: vec![0.0, 0.25, 1.0, 1.25],
                    epsilon: 0.0001,
                },
            },
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "storage_vec4_and_scalar_mix",
                invocation_count: 2,
                bindings: vec![
                    ComputeBinding::storage_f32(&[1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 2.0]),
                    ComputeBinding::storage_f32(&[0.0, 1.0, 0.0, 3.0, 0.0, 0.0, 1.0, 4.0]),
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
                    ComputeBinding::storage_f32(&[0.25, 0.5]),
                    ComputeBinding::uniform_f32(&[2.0]),
                ],
                output_binding: 2,
                expected_output: ExpectedOutput::F32 {
                    values: vec![1.5, 0.5, 0.0, 1.5, 0.0, 1.0, 1.0, 3.0],
                    epsilon: 0.0001,
                },
            },
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "nested_conditionals_and_uint_output",
                invocation_count: 4,
                bindings: vec![
                    ComputeBinding::storage_i32(&[-1, 0, 1, 2]),
                    ComputeBinding::storage_u32(&[0, 0, 0, 0]),
                ],
                output_binding: 1,
                expected_output: ExpectedOutput::U32(vec![0, 10, 11, 102]),
            },
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "loop_range_start_end_and_while",
                invocation_count: 4,
                bindings: vec![
                    ComputeBinding::storage_f32(&[1.0, 2.0, 3.0, 4.0]),
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0]),
                ],
                output_binding: 1,
                expected_output: ExpectedOutput::F32 {
                    values: vec![10.0, 13.0, 16.0, 19.0],
                    epsilon: 0.0001,
                },
            },
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "precision_vector_math",
                invocation_count: 2,
                bindings: vec![
                    ComputeBinding::storage_f32(&[1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0]),
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
                    ComputeBinding::uniform_f32(&[0.000001]),
                ],
                output_binding: 1,
                expected_output: ExpectedOutput::F32 {
                    values: vec![0.0, 1.0, 1.0, 0.5, -1.0, 0.0, 1.0, 0.5],
                    epsilon: 0.0002,
                },
            },
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "reduced_sculpt_runtime",
                invocation_count: 2,
                bindings: vec![
                    ComputeBinding::storage_f32(&[1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0]),
                    ComputeBinding::storage_f32(&[0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0]),
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
                    ComputeBinding::storage_f32(&[0.0, 1.0]),
                    ComputeBinding::uniform_f32(&[0.5]),
                    ComputeBinding::uniform_f32(&[0.000001]),
                ],
                output_binding: 2,
                expected_output: ExpectedOutput::F32 {
                    values: vec![1.5, 0.0, 0.0, 1.0, 0.0000005, 1.5, 0.0, 1.0],
                    epsilon: 0.0002,
                },
            },
        },
        SpirvExecKernel {
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
            case: ComputeCase {
                name: "reduced_supermotion_runtime",
                invocation_count: 2,
                bindings: vec![
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0]),
                    ComputeBinding::storage_i32(&[-1, 0]),
                    ComputeBinding::storage_f32(&[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]),
                    ComputeBinding::uniform_u32(&[2]),
                    ComputeBinding::uniform_f32(&[0.000001]),
                ],
                output_binding: 2,
                expected_output: ExpectedOutput::F32 {
                    values: vec![0.0, 0.0, 0.0, 1.0, 1.0, 0.0, -0.000001, 1.0],
                    epsilon: 0.001,
                },
            },
        },
    ]
}

#[test]
fn spirv_execute_known_kernels() {
    let Ok(executor) = VulkanComputeExecutor::try_new() else {
        eprintln!("[spirv-exec] Vulkan runtime unavailable; skipping execution tests");
        return;
    };

    for case in execution_cases() {
        assert_exec_case(&executor, &case);
    }
}
