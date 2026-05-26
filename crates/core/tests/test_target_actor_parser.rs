use kain_core::{ast::*, diagnostics::SpanMapper, lexer::Lexer, parser::Parser};

#[test]
fn test_parse_target_actor_minimal() {
    let source = r#"
@target_actor
struct LineTraceTarget:
    trace_type: "Line"
    max_range: 1000.0
    trace_channel: "Visibility"
"#;

    let tokens = Lexer::new(source)
        .tokenize()
        .expect("Failed to tokenize target actor source");
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "<test_target_actor_minimal>");
    let program = parser.parse().expect("Failed to parse target actor source");

    assert_eq!(program.items.len(), 1);
    match &program.items[0] {
        Item::TargetActor(def) => {
            assert_eq!(def.name, "LineTraceTarget");
            assert_eq!(def.trace_type, TraceType::Line);
            assert_eq!(def.max_range, Some(1000.0));
            assert_eq!(def.trace_channel.as_deref(), Some("Visibility"));
        }
        other => panic!("Expected Item::TargetActor, got: {:?}", other),
    }
}

#[test]
fn test_parse_target_actor_with_filter_arrays() {
    let source = r#"
@target_actor
struct SphereTraceTarget:
    trace_type: "Sphere"
    max_range: 2000.0
    trace_channel: "Visibility"

    filter:
        self_filter: "Exclude"
        required_actor_class: "ACharacter"
        require_tags: ["Status.Alive"]
        ignore_tags: ["Status.Dead", "Status.Invulnerable"]

    reticle_class: "BP_LineTraceReticle"
"#;

    let tokens = Lexer::new(source)
        .tokenize()
        .expect("Failed to tokenize filtered target actor source");
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(
        &tokens,
        &span_mapper,
        "<test_target_actor_with_filter_arrays>",
    );
    let program = parser
        .parse()
        .expect("Failed to parse filtered target actor source");

    assert_eq!(program.items.len(), 1);
    match &program.items[0] {
        Item::TargetActor(def) => {
            assert_eq!(def.name, "SphereTraceTarget");
            assert_eq!(def.trace_type, TraceType::Sphere);
            assert_eq!(def.max_range, Some(2000.0));
            assert_eq!(def.trace_channel.as_deref(), Some("Visibility"));
            assert_eq!(def.reticle_class.as_deref(), Some("BP_LineTraceReticle"));

            let filter = def.filter.as_ref().expect("Expected filter block");
            assert_eq!(filter.self_filter.as_deref(), Some("Exclude"));
            assert_eq!(filter.required_actor_class.as_deref(), Some("ACharacter"));
            assert_eq!(filter.require_tags, vec!["Status.Alive"]);
            assert_eq!(
                filter.ignore_tags,
                vec!["Status.Dead", "Status.Invulnerable"]
            );
        }
        other => panic!("Expected Item::TargetActor, got: {:?}", other),
    }
}

#[test]
fn test_parse_target_actor_fixture_file() {
    let fixture_path = std::path::Path::new("m:/Code/Factory/Example_GAS/test_targets.kn");
    let source = std::fs::read_to_string(fixture_path)
        .unwrap_or_else(|e| panic!("Failed to read fixture '{}': {e}", fixture_path.display()));

    let tokens = Lexer::new(&source)
        .tokenize()
        .expect("Failed to tokenize target actor fixture source");
    let span_mapper = SpanMapper::new(&source);
    let mut parser = Parser::new(&tokens, &span_mapper, "<test_target_actor_fixture_file>");
    let program = parser
        .parse()
        .expect("Failed to parse target actor fixture source");

    let target_actor_count = program
        .items
        .iter()
        .filter(|item| matches!(item, Item::TargetActor(_)))
        .count();

    assert_eq!(
        target_actor_count, 8,
        "Expected all target_actor structs in fixture to parse"
    );
}
