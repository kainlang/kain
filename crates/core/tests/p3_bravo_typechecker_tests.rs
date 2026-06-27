// P3-BRAVO: Test typechecker enrichment for component pulse/resonate/state/callbacks
// Note: These tests require P3-ALPHA parser changes (which are already in the source).

use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::diagnostics::SpanMapper;
use kain_core::types;
use kain_core::{TypedItem, ResolvedType};

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, kain_core::error::KainError> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = SpanMapper::new(source);
    let ast = Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    types::check(&ast, &span_mapper, "<test>")
}

#[test]
fn p3_bravo_component_with_f64_state_typechecks() {
    let source = r#"component Slider(label: String, min: Float, max: Float):
    state value: Float = 0.0
    state dragging: Bool = false

    render <div class="slider">
        <Text value={label} />
        <div class="thumb" x={value} />
    </div>
"#;
    let typed = parse_and_typecheck(source).expect("component with f64 state should typecheck");
    // Verify TypedComponent has state_types populated
    for item in &typed.items {
        if let TypedItem::Component(comp) = item {
            assert!(comp.state_types.contains_key("value"));
            assert!(comp.state_types.contains_key("dragging"));
            // Verify Float → f64 mapping
            match comp.state_types.get("value") {
                Some(ResolvedType::Float(_)) => {},
                other => panic!("Expected Float type for 'value', got {:?}", other),
            }
            match comp.state_types.get("dragging") {
                Some(ResolvedType::Bool) => {},
                other => panic!("Expected Bool type for 'dragging', got {:?}", other),
            }
            return;
        }
    }
    panic!("Expected a TypedComponent in output");
}

#[test]
fn p3_bravo_component_with_pulse_typechecks() {
    let source = r#"component Ticker(title: String):
    state count: Int = 0

    pulse tick every 16ms:
        count = count + 1

    render <div>
        <Text value={title} />
        <Text value={count} />
    </div>
"#;
    let typed = parse_and_typecheck(source).expect("component with pulse should typecheck");
    for item in &typed.items {
        if let TypedItem::Component(comp) = item {
            assert!(!comp.pulse_types.is_empty(), "pulse_types should not be empty");
            assert_eq!(comp.pulse_types.len(), 1);
            assert_eq!(comp.pulse_types[0].ast.name, "tick");
            return;
        }
    }
    panic!("Expected a TypedComponent in output");
}

#[test]
fn p3_bravo_component_with_resonate_typechecks() {
    let source = r#"world AppState:
    state slider_value: Float = 0.0
    state slider_fill: Float = 0.0

component Slider(label: String):
    state local_fill: Float = 0.0

    resonate AppState.slider_value dampen 0ms:
        local_fill = AppState.slider_value

    render <div class="slider">
        <Text value={label} />
        <div class="fill" width={local_fill} />
    </div>
"#;
    let typed = parse_and_typecheck(source).expect("component with resonate should typecheck");
    for item in &typed.items {
        if let TypedItem::Component(comp) = item {
            assert!(!comp.resonate_types.is_empty(), "resonate_types should not be empty");
            assert_eq!(comp.resonate_types.len(), 1);
            return;
        }
    }
    panic!("Expected a TypedComponent in output");
}

#[test]
fn p3_bravo_component_with_callback_typechecks() {
    let source = r#"fn handle_click():
    ()

component Button(label: String):
    state hovered: Bool = false

    render <button on_click={handle_click}>
        <Text value={label} />
    </button>
"#;
    let typed = parse_and_typecheck(source).expect("component with on_click callback should typecheck");
    // Verify no errors
    for item in &typed.items {
        if let TypedItem::Component(_) = item {
            return; // success
        }
    }
}

#[test]
fn p3_bravo_component_with_non_function_callback_rejected() {
    let source = r#"component BadButton(label: String):
    render <button on_click={"not_a_function"}>
        <Text value={label} />
    </button>
"#;
    let err = parse_and_typecheck(source).expect_err("non-function callback should be rejected");
    let msg = err.to_string();
    assert!(msg.contains("callback") || msg.contains("function") || msg.contains("Callback"),
        "Error should mention callback/function issue, got: {}", msg);
}

#[test]
fn p3_bravo_component_without_pulse_still_typechecks() {
    // Backward compat: component without pulse/resonate must still work
    let source = r#"component Simple(label: String):
    state count: Int = 0

    render <div>
        <Text value={label} />
        <Text value={count} />
    </div>
"#;
    let typed = parse_and_typecheck(source).expect("simple component without pulse should typecheck");
    for item in &typed.items {
        if let TypedItem::Component(comp) = item {
            assert!(comp.pulse_types.is_empty());
            assert!(comp.resonate_types.is_empty());
            assert!(comp.state_types.contains_key("count"));
            return;
        }
    }
    panic!("Expected a TypedComponent in output");
}

#[test]
fn p3_bravo_component_self_access_in_pulse() {
    // Verify self. access works inside pulse body
    let source = r#"component Counter(label: String):
    state value: Int = 0
    state max: Int = 100

    pulse tick every 100ms:
        self.value = self.value + 1

    render <div>
        <Text value={label} />
        <Text value={self.value} />
    </div>
"#;
    let typed = parse_and_typecheck(source).expect("component with self access in pulse should typecheck");
    for item in &typed.items {
        if let TypedItem::Component(comp) = item {
            assert!(!comp.pulse_types.is_empty());
            assert!(comp.state_types.contains_key("value"));
            return;
        }
    }
    panic!("Expected a TypedComponent in output");
}

#[test]
fn p3_bravo_unknown_event_kind_passes_as_expr() {
    // on_invalid is NOT in EVENT_CALLBACK_ATTRS, so it's treated as a regular
    // expression attribute, not a callback. This is correct parser behavior —
    // only known event names are routed to the Callback variant.
    let source = r#"fn handle():
    ()

component Bad(label: String):
    render <button on_invalid={handle}>
        <Text value={label} />
    </button>
"#;
    // This should PASS because on_invalid is not recognized as an event,
    // so it typechecks as a regular expression prop.
    let _ = parse_and_typecheck(source)
        .expect("on_invalid treated as regular expr (not a callback) should typecheck");
}
