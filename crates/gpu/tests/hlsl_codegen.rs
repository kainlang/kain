use gpu::generate_hlsl;
use kain_core::comptime;
use kain_core::diagnostics::SpanMapper;
use kain_core::types;
use kain_core::{Lexer, Parser, TypedProgram};

fn typed_program_for_hlsl(source: &str) -> TypedProgram {
    let full_source = source.to_string();
    let span_mapper = SpanMapper::new(&full_source);
    let tokens = Lexer::new(&full_source)
        .tokenize()
        .expect("tokenize failed");
    let mut ast = Parser::new(&tokens, &span_mapper, "<hlsl-codegen>")
        .parse()
        .expect("parse failed");
    comptime::eval_program(&mut ast).expect("comptime failed");
    types::check(&ast, &span_mapper, "<hlsl-codegen>").expect("typecheck failed")
}

#[test]
fn hlsl_codegen_accepts_statement_style_if_control_flow() {
    let src = r#"
shader compute hlsl_if_control(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform count: UInt @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let idx = id.x
    if idx >= count:
        return
    if src[idx] > 0.0:
        dst[idx] = src[idx]
    else:
        dst[idx] = 0.0
    return
"#;

    let typed = typed_program_for_hlsl(src);
    let hlsl = generate_hlsl(&typed).expect("hlsl generation failed");

    assert!(hlsl.contains("if ((idx >= count))"));
    assert!(hlsl.contains("else"));
    assert!(hlsl.contains("dst[idx] = src[idx];"));
    assert!(hlsl.contains("dst[idx] = 0.000000;"));
}
