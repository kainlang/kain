use gpu::generate_wgsl;
use kain_core::comptime;
use kain_core::diagnostics::SpanMapper;
use kain_core::types;
use kain_core::{Lexer, Parser, TypedProgram};

fn typed_program_for_wgsl(source: &str) -> TypedProgram {
    let full_source = source.to_string();
    let span_mapper = SpanMapper::new(&full_source);
    let tokens = Lexer::new(&full_source)
        .tokenize()
        .expect("tokenize failed");
    let mut ast = Parser::new(&tokens, &span_mapper, "<wgsl-codegen>")
        .parse()
        .expect("parse failed");
    comptime::eval_program(&mut ast).expect("comptime failed");
    types::check(&ast, &span_mapper, "<wgsl-codegen>").expect("typecheck failed")
}

fn validate_wgsl(source: &str) {
    let module = naga::front::wgsl::parse_str(source).expect(source);
    let mut validator = naga::valid::Validator::new(
        naga::valid::ValidationFlags::all(),
        naga::valid::Capabilities::all(),
    );
    validator.validate(&module).expect(source);
}

#[test]
fn wgsl_codegen_emits_valid_compute_storage_buffer_kernel() {
    let src = r#"
shader compute wgsl_copy(id: UVec3) -> Void:
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

    let typed = typed_program_for_wgsl(src);
    let wgsl = generate_wgsl(&typed).expect("wgsl generation failed");

    assert!(wgsl.contains("@compute @workgroup_size(8, 8, 1)"));
    assert!(wgsl.contains("@binding(0) var<storage, read_write> src: array<f32>;"));
    assert!(wgsl.contains("@binding(1) var<storage, read_write> dst: array<f32>;"));
    assert!(wgsl.contains("dst[idx] = (value + 1.0);"));
    validate_wgsl(&wgsl);
}

#[test]
fn wgsl_codegen_emits_valid_fragment_shader() {
    let src = r#"
shader fragment color_field(uv: Vec2) -> Vec4:
    let wave = sin(uv.x)
    return vec4(wave, uv.y, 1.0, 1.0)
"#;

    let typed = typed_program_for_wgsl(src);
    let wgsl = generate_wgsl(&typed).expect("wgsl generation failed");

    assert!(wgsl.contains("@fragment"));
    assert!(wgsl.contains("fn color_field("));
    assert!(wgsl.contains("return vec4<f32>(wave, uv.y, 1.0, 1.0);"));
    validate_wgsl(&wgsl);
}
