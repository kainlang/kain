use gpu::codegen_ptx::{generate_with_options, PtxCodegenOptions};
use gpu::ptx_module::PtxArch;
use gpu::{generate_ptx, generate_spirv};
use kain_core::comptime;
use kain_core::diagnostics::SpanMapper;
use kain_core::types;
use kain_core::{CompileTarget, Lexer, Parser, TypedProgram};

fn typed_program_for_target(source: &str, _target: CompileTarget) -> TypedProgram {
    let full_source = source.to_string();
    let span_mapper = SpanMapper::new(&full_source);
    let tokens = Lexer::new(&full_source)
        .tokenize()
        .expect("tokenize failed");
    let mut ast = Parser::new(&tokens, &span_mapper, "<ptx-codegen>")
        .parse()
        .expect("parse failed");
    comptime::eval_program(&mut ast).expect("comptime failed");
    types::check(&ast, &span_mapper, "<ptx-codegen>").expect("typecheck failed")
}

#[test]
fn ptx_emits_raw_compute_kernel_for_storage_buffer_copy() {
    let src = r#"
shader compute copy_kernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform count: UInt @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = dispatch_thread_id.x
    if idx >= count:
        return

    dst[idx] = src[idx]
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let ptx = generate_ptx(&typed).expect("ptx generation failed");

    assert!(ptx.contains(".version 7.8"));
    assert!(ptx.contains(".target sm_50"));
    assert!(ptx.contains(".visible .entry copy_kernel"));
    assert!(ptx.contains(".param .u64 _kain_param_src"));
    assert!(ptx.contains(".param .u64 _kain_param_dst"));
    assert!(ptx.contains(".param .u32 _kain_param_count"));
    assert!(ptx.contains("mad.lo.u32"));
    assert!(ptx.contains("ld.global.f32"));
    assert!(ptx.contains("st.global.f32"));
}

#[test]
fn same_compute_shader_can_emit_spirv_and_ptx() {
    let src = r#"
shader compute dual_backend(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = id.x
    let value = src[idx]
    dst[idx] = value + 1.0
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let spirv = generate_spirv(&typed).expect("spirv generation failed");
    let ptx = generate_ptx(&typed).expect("ptx generation failed");

    assert!(spirv.len() > 16);
    assert_eq!(&spirv[0..4], [0x03, 0x02, 0x23, 0x07]);
    assert!(ptx.contains(".visible .entry dual_backend"));
    assert!(ptx.contains("add.f32"));
}

#[test]
fn ptx_rejects_non_compute_shader_stage() {
    let src = r#"
shader fragment pixel(position: Vec4) -> Vec4:
    return position
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let err = generate_ptx(&typed).expect_err("fragment shader should not lower to PTX");
    assert!(err.to_string().contains("only supports compute shaders"));
}

#[test]
fn ptx_supports_min_max_in_compute_kernels() {
    let src = r#"
shader compute min_max_kernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1
    uniform count: UInt @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let safe_count = max(count, UInt(1))
    let idx = min(id.x, safe_count - UInt(1))
    dst[idx] = max(src[idx], UInt(7))
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let ptx = generate_ptx(&typed).expect("ptx generation with min/max should succeed");

    assert!(ptx.contains(".visible .entry min_max_kernel"));
    assert!(ptx.contains("setp.lt.u32"));
    assert!(ptx.contains("selp.u32"));
}

#[test]
fn ptx_can_target_turing_explicitly() {
    let src = r#"
shader compute turing_kernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = id.x
    dst[idx] = src[idx]
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let ptx = generate_with_options(&typed, PtxCodegenOptions::with_target_arch(PtxArch::Sm75))
        .expect("ptx generation for sm_75 should succeed");

    assert!(ptx.contains(".target sm_75"));
    assert!(ptx.contains(".visible .entry turing_kernel"));
}

#[test]
fn ptx_updates_loop_carried_locals_in_place() {
    let src = r#"
shader compute loop_update_kernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform count_data: StorageBuffer<UInt> @2

    let count = count_data[0]
    let idx = id.x
    if idx >= UInt(1):
        return

    var total: Float = 0.0
    var i: UInt = UInt(0)
    while i < count:
        total = total + src[i]
        i = i + UInt(1)

    dst[idx] = total
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let ptx = generate_ptx(&typed).expect("ptx generation with loop-carried locals should succeed");

    assert!(ptx.contains("add.f32 %f3, %f1, %f2;"));
    assert!(ptx.contains("mov.f32 %f1, %f3;"));
    assert!(ptx.contains("add.u32 %r14, %r11, %r13;"));
    assert!(ptx.contains("mov.u32 %r11, %r14;"));
}

#[test]
fn ptx_lowers_cuda_warp_intrinsics_and_tensor_arch_floor() {
    let src = r#"
use std::cuda

shader compute cuda_intrinsic_kernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1
    uniform count: UInt @2

    let idx = id.x
    if idx >= count:
        return

    cuda_require_tensor_cores()
    let lane = cuda_lane_id()
    let mask = cuda_active_mask()
    let ballot = cuda_ballot(lane < UInt(16))
    let folded = cuda_warp_reduce_sum_u32(cuda_shfl_xor_u32(src[idx], UInt(1)) + (ballot & mask))
    cuda_warp_sync(mask)
    cuda_block_sync()
    dst[idx] = folded
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let ptx = generate_ptx(&typed).expect("cuda warp intrinsic lowering should succeed");

    assert!(ptx.contains(".target sm_75"));
    assert!(ptx.contains("mov.u32"));
    assert!(ptx.contains("%laneid"));
    assert!(ptx.contains("activemask.b32"));
    assert!(ptx.contains("vote.sync.ballot.b32"));
    assert!(ptx.contains("shfl.sync.bfly.b32"));
    assert!(ptx.contains("bar.warp.sync"));
    assert!(ptx.contains("bar.sync 0"));
    assert!(ptx.contains("require tensor cores"));
}

#[test]
fn ptx_rejects_tensor_core_intrinsic_below_sm75() {
    let src = r#"
use std::cuda

shader compute cuda_tensor_floor(id: UVec3) -> Void:
    cuda_require_tensor_cores()
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let err = generate_with_options(&typed, PtxCodegenOptions::with_target_arch(PtxArch::Sm50))
        .expect_err("tensor core requirements should reject sm_50");

    assert!(err.to_string().contains("too old"));
    assert!(err.to_string().contains("sm_75"));
}

#[test]
fn ptx_packs_narrow_storage_buffer_numeric_lanes() {
    let src = r#"
shader compute packed_numeric_kernel(id: UVec3) -> Void:
    uniform src8: StorageBuffer<u8> @0
    uniform src16: StorageBuffer<i16> @1
    uniform dst: StorageBuffer<UInt> @2
    uniform count: UInt @3

    let idx = id.x
    if idx >= count:
        return

    let a = src8[idx]
    let b = src16[idx]
    dst[idx] = a + UInt(b)
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let ptx = generate_ptx(&typed).expect("packed numeric storage should lower to PTX");

    assert!(ptx.contains("ld.global.u8"));
    assert!(ptx.contains("ld.global.s16"));
    assert!(ptx.contains("st.global.u32"));
}

#[test]
fn ptx_async_shared_group_intrinsics_raise_sm80_floor() {
    let src = r#"
use std::cuda

shader compute cuda_async_group_kernel(id: UVec3) -> Void:
    cuda_cp_async_commit_group()
    cuda_cp_async_wait_group_0()
    return
"#;

    let typed = typed_program_for_target(src, CompileTarget::Cuda);
    let ptx = generate_ptx(&typed).expect("cp.async group intrinsics should lower");

    assert!(ptx.contains(".target sm_80"));
    assert!(ptx.contains("cp.async.commit_group"));
    assert!(ptx.contains("cp.async.wait_group 0"));
}
