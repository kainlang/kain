use kain_core::diagnostics::SpanMapper;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::stdlib;
use kain_core::types;
use kain_core::{CompileTarget, TypedProgram};
use kain_sys_codegen::{
    collect_gpu_artifacts,
    collect_gpu_artifacts_json,
    generate_rust_gpu_host_wrappers,
    RustGpuBindingKind,
    RustGpuShaderStage,
};

fn typed_shader_program(source: &str) -> TypedProgram {
    let stdlib_source = stdlib::load_stdlib_for_target(CompileTarget::Spirv);
    let full_source = format!("{}\n{}", stdlib_source, source);
    let tokens = Lexer::new(&full_source)
        .tokenize()
        .expect("lexer should succeed");
    let span_mapper = SpanMapper::new(&full_source);
    let mut parser = Parser::new(&tokens, &span_mapper, "<gpu-test>");
    let ast = parser.parse().expect("parser should succeed");
    types::check(&ast, &span_mapper, "<gpu-test>").expect("type-check should succeed")
}

fn sample_shader_source() -> &'static str {
    r#"
shader compute sample_gpu_kernel(id: UVec3) -> Vec4:
    uniform positions: StorageBuffer<Vec4> @0
    uniform center: Vec4 @1
    uniform brush_alpha: Sampler2D @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform CFG_HIGH_QUALITY: UInt @101

    let idx = id.x
    let pos = positions[idx]
    return vec4(pos.x + center.x, pos.y, pos.z, 1.0)
"#
}

#[test]
fn collects_gpu_shader_artifacts() {
    let typed = typed_shader_program(sample_shader_source());
    let artifacts = collect_gpu_artifacts(&typed);

    assert_eq!(artifacts.shaders.len(), 1);
    let shader = &artifacts.shaders[0];
    assert_eq!(shader.name, "sample_gpu_kernel");
    assert_eq!(shader.stage, RustGpuShaderStage::Compute);
    assert_eq!(shader.entry_point, "sample_gpu_kernel");
    assert_eq!(shader.output_type, "Vec4");

    assert_eq!(shader.inputs.len(), 1);
    assert_eq!(shader.inputs[0].name, "id");
    assert_eq!(shader.inputs[0].ty, "UVec3");

    assert_eq!(shader.bindings.len(), 5);
    assert_eq!(shader.bindings[0].name, "positions");
    assert_eq!(shader.bindings[0].binding, 0);
    assert_eq!(shader.bindings[0].descriptor_set, 0);
    assert_eq!(shader.bindings[0].kind, RustGpuBindingKind::StorageBuffer);

    assert_eq!(shader.bindings[1].kind, RustGpuBindingKind::Uniform);
    assert_eq!(shader.bindings[2].kind, RustGpuBindingKind::Sampler2D);
    assert_eq!(shader.bindings[3].kind, RustGpuBindingKind::LocalSize);
    assert_eq!(shader.bindings[4].kind, RustGpuBindingKind::SpecializationConstant);
}

#[test]
fn serializes_gpu_artifacts_to_reflection_json() {
    let typed = typed_shader_program(sample_shader_source());
    let reflection_json = collect_gpu_artifacts_json(&typed).expect("json serialization should succeed");

    assert!(reflection_json.contains("sample_gpu_kernel"));
    assert!(reflection_json.contains("storage_buffer"));
    assert!(reflection_json.contains("specialization_constant"));
    assert!(reflection_json.contains("descriptor_set"));
}

#[test]
fn generates_rust_gpu_host_wrappers_with_layout_and_dispatch_helpers() {
    let typed = typed_shader_program(sample_shader_source());
    let rust_host = generate_rust_gpu_host_wrappers(&typed).expect("gpu host generation should succeed");

    assert!(rust_host.contains("pub mod kain_gpu_generated"));
    assert!(rust_host.contains("pub mod sample_gpu_kernel"));
    assert!(rust_host.contains("pub struct Params"));
    assert!(rust_host.contains("StorageBufferParam"));
    assert!(rust_host.contains("Sampler2DParam"));
    assert!(rust_host.contains("LocalSizeParam"));
    assert!(rust_host.contains("SpecializationConstantParam"));
    assert!(rust_host.contains("pub fn descriptor_layout() -> Vec<BindingLayoutEntry>"));
    assert!(rust_host.contains("pub fn dispatch<'a>(params: &'a Params, x: u32, y: u32, z: u32) -> DispatchCall<'a, Params>"));
}
