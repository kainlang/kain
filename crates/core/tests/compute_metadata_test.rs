use kain_core::diagnostics::SpanMapper;
use kain_core::{Lexer, Parser};

#[test]
fn parses_explicit_compute_metadata_from_comptime_plan() {
    let source = r#"
shader compute TensorBlend() -> Void:
    comptime:
        let compute = (
            [16, 8, 1],
            [
                ("src", "f32", ["dispatch.x"], "input", "kain.shared.buffer"),
                ("dst", "f32", ["dispatch.x"], "output", "kain.shared.buffer"),
            ],
            [
                ("TensorBlend", "blend", ["src"], ["dst"], false),
            ],
        )

    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1

    return
"#;

    let tokens = Lexer::new(source).tokenize().expect("tokens");
    let span_mapper = SpanMapper::new(source);
    let ast = Parser::new(&tokens, &span_mapper, "<test>")
        .parse()
        .expect("parse");

    let shader = match ast.items.first() {
        Some(kain_core::Item::Shader(shader)) => shader,
        other => panic!("expected shader item, got {:?}", other),
    };

    let metadata = shader
        .explicit_compute_metadata()
        .expect("compute metadata should parse")
        .expect("compute metadata should be present");

    assert_eq!(metadata.workgroup_size, None);
    assert_eq!(metadata.dispatch_size, [16, 8, 1]);
    assert_eq!(metadata.tensor_plans.len(), 2);
    assert_eq!(metadata.tensor_plans[0].key, "src");
    assert_eq!(
        metadata.tensor_plans[0].shape,
        vec!["dispatch.x".to_string()]
    );
    assert_eq!(metadata.stream_plans, None);
    assert_eq!(metadata.neural_node_plans.len(), 1);
    assert_eq!(metadata.neural_node_plans[0].op, "blend");
}

#[test]
fn parses_extended_compute_metadata_with_workgroup_and_stream_plans() {
    let source = r#"
shader compute TensorBlend() -> Void:
    comptime:
        let compute = (
            [8, 4, 1],
            [16, 8, 1],
            [
                ("src", "f32", ["dispatch.x"], "input", "kain.shared.buffer"),
                ("dst", "f32", ["dispatch.x"], "output", "kain.shared.buffer"),
            ],
            [
                ("src", "ingress", "per-dispatch", "kain.shared.buffer"),
                ("dst", "egress", "per-dispatch", "kain.shared.buffer"),
            ],
            [
                ("TensorBlend", "blend", ["src"], ["dst"], false),
            ],
        )

    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1

    return
"#;

    let tokens = Lexer::new(source).tokenize().expect("tokens");
    let span_mapper = SpanMapper::new(source);
    let ast = Parser::new(&tokens, &span_mapper, "<test>")
        .parse()
        .expect("parse");

    let shader = match ast.items.first() {
        Some(kain_core::Item::Shader(shader)) => shader,
        other => panic!("expected shader item, got {:?}", other),
    };

    let metadata = shader
        .explicit_compute_metadata()
        .expect("compute metadata should parse")
        .expect("compute metadata should be present");

    assert_eq!(metadata.workgroup_size, Some([8, 4, 1]));
    assert_eq!(metadata.dispatch_size, [16, 8, 1]);
    let stream_plans = metadata
        .stream_plans
        .expect("stream plans should be present in extended form");
    assert_eq!(stream_plans.len(), 2);
    assert_eq!(stream_plans[0].direction, "ingress");
    assert_eq!(stream_plans[0].cadence, "per-dispatch");
}

#[test]
fn rejects_malformed_compute_dispatch_size() {
    let source = r#"
shader compute BadDispatch() -> Void:
    comptime:
        let compute = (
            [16, 8],
            [],
            [],
        )

    return
"#;

    let tokens = Lexer::new(source).tokenize().expect("tokens");
    let span_mapper = SpanMapper::new(source);
    let result = Parser::new(&tokens, &span_mapper, "<test>").parse();

    let error = result.expect_err("parse should fail");
    let message = error.to_string();
    assert!(message.contains("dispatch"));
    assert!(message.contains("3"));
}
