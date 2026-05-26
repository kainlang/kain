use kain_core::diagnostics::SpanMapper;
use kain_core::{emit_runtime_contract_bundle, types, CompileTarget, Lexer, Parser};

#[test]
fn emits_reflection_payload_for_rust_target() {
    let source = r#"
struct Point:
    x: Float
    y: Float

component App():
    render <panel title="Test" />

actor Counter:
    fn handle_increment():
        pass
"#;

    let tokens = Lexer::new(source).tokenize().expect("tokens");
    let span_mapper = SpanMapper::new(source);
    let ast = Parser::new(&tokens, &span_mapper, "<test>")
        .parse()
        .expect("parse");
    let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");

    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);

    // Check reflection payload is emitted
    assert!(bundle.reflection_payload.is_some());
    let payload = bundle.reflection_payload.as_ref().unwrap();

    // Check schema version
    assert_eq!(payload.schema_version, 1);

    // Check types
    assert_eq!(payload.types.len(), 1);
    assert_eq!(payload.types[0].name, "Point");
    assert_eq!(payload.types[0].kind, "struct");
    assert_eq!(payload.types[0].fields.len(), 2);
    assert_eq!(payload.types[0].fields[0].name, "x");
    assert_eq!(payload.types[0].fields[0].type_name, "Float");
    assert_eq!(payload.types[0].fields[1].name, "y");
    assert_eq!(payload.types[0].fields[1].type_name, "Float");

    // Check items
    assert!(payload.items.len() >= 3);
    let struct_item = payload.items.iter().find(|i| i.name == "Point").unwrap();
    assert_eq!(struct_item.kind, "struct");

    let component_item = payload.items.iter().find(|i| i.name == "App").unwrap();
    assert_eq!(component_item.kind, "component");

    let actor_item = payload.items.iter().find(|i| i.name == "Counter").unwrap();
    assert_eq!(actor_item.kind, "actor");

    // Check components
    assert_eq!(payload.components.len(), 1);
    assert_eq!(payload.components[0].name, "App");

    // Check actors
    assert_eq!(payload.actors.len(), 1);
    assert_eq!(payload.actors[0].name, "Counter");
}

#[test]
fn emits_reflection_payload_for_llvm_target() {
    let source = r#"
struct Vec2:
    x: Float
    y: Float

fn add(a: Vec2, b: Vec2) -> Vec2:
    Vec2 { x: a.x + b.x, y: a.y + b.y }
"#;

    let tokens = Lexer::new(source).tokenize().expect("tokens");
    let span_mapper = SpanMapper::new(source);
    let ast = Parser::new(&tokens, &span_mapper, "<test>")
        .parse()
        .expect("parse");
    let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");

    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Llvm);

    // Check reflection payload is emitted for LLVM target
    assert!(bundle.reflection_payload.is_some());
    let payload = bundle.reflection_payload.as_ref().unwrap();

    // Check types
    assert_eq!(payload.types.len(), 1);
    assert_eq!(payload.types[0].name, "Vec2");

    // Check items
    let struct_item = payload.items.iter().find(|i| i.name == "Vec2").unwrap();
    assert_eq!(struct_item.kind, "struct");

    let func_item = payload.items.iter().find(|i| i.name == "add").unwrap();
    assert_eq!(func_item.kind, "function");
}

#[test]
fn does_not_emit_reflection_payload_for_js_target() {
    let source = r#"
struct Point:
    x: Float
    y: Float
"#;

    let tokens = Lexer::new(source).tokenize().expect("tokens");
    let span_mapper = SpanMapper::new(source);
    let ast = Parser::new(&tokens, &span_mapper, "<test>")
        .parse()
        .expect("parse");
    let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");

    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Js);

    // Check reflection payload is NOT emitted for JS target
    assert!(bundle.reflection_payload.is_none());
    assert!(!bundle.reflection.emitted);
}

#[test]
fn reflection_payload_serializes_to_json() {
    let source = r#"
struct Color:
    r: Float
    g: Float
    b: Float

component Button():
    render <button />
"#;

    let tokens = Lexer::new(source).tokenize().expect("tokens");
    let span_mapper = SpanMapper::new(source);
    let ast = Parser::new(&tokens, &span_mapper, "<test>")
        .parse()
        .expect("parse");
    let typed = types::check(&ast, &span_mapper, "<test>").expect("typecheck");

    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);

    // Serialize to JSON
    let json = serde_json::to_string_pretty(&bundle).expect("serialize");

    // Check JSON contains reflection payload
    assert!(json.contains("reflection_payload"));
    assert!(json.contains("\"name\": \"Color\""));
    assert!(json.contains("\"name\": \"Button\""));
    assert!(json.contains("\"types\""));
    assert!(json.contains("\"items\""));
    assert!(json.contains("\"components\""));
}
