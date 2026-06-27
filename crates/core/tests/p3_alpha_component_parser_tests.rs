// P3-ALPHA: Verify component AST and parser changes
// Tests: pulse/resonate in components, dimensions, on_* callback attrs

use kain_core::ast::*;
use kain_core::parser::Parser;
use kain_core::lexer::Lexer;
use kain_core::diagnostics::SpanMapper;

fn parse(source: &str) -> kain_core::KainResult<kain_core::ast::Program> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = SpanMapper::new(source);
    Parser::new(&tokens, &span_mapper, "<test>").parse()
}

#[test]
fn p3_component_with_pulse_parses() {
    let program = parse(
        "component PulseTest(count: Int):\n    state tick_count: Int = 0\n    pulse tick every 16ms:\n        tick_count = tick_count + 1\n    render <div><Text value={tick_count} /></div>\n"
    ).expect("should parse component with pulse");

    let Item::Component(component) = &program.items[0] else { panic!("expected component"); };

    assert_eq!(component.pulses.len(), 1);
    let pulse = &component.pulses[0];
    assert_eq!(pulse.name, "tick");
    assert_eq!(pulse.interval.value, 16);
    assert_eq!(pulse.interval.unit, "ms");
    assert!(pulse.jitter.is_none());
    assert!(pulse.budget.is_none());
    // Verify body exists
    assert!(!pulse.body.stmts.is_empty());
}

#[test]
fn p3_component_with_resonate_parses() {
    let program = parse(
        "world SensorState:\n    state value: Float = 0.0\n\ncomponent ResonateTest(title: String):\n    state last_value: Float = 0.0\n    resonate SensorState.value dampen 0ms:\n        last_value = SensorState.value\n    render <div><Text value={last_value} /></div>\n"
    ).expect("should parse component with resonate");

    // Find the component (second item after world)
    let component_item = program.items.iter().find(|i| matches!(i, Item::Component(_))).expect("expected component item");
    let Item::Component(component) = component_item else { panic!("expected component"); };

    assert_eq!(component.resonates.len(), 1);
    let resonate = &component.resonates[0];
    assert_eq!(resonate.target.segments, vec!["SensorState", "value"]);
    assert!(resonate.dampen.is_some());
    let dampen = resonate.dampen.as_ref().unwrap();
    assert_eq!(dampen.value, 0);
    assert_eq!(dampen.unit, "ms");
    assert!(!resonate.body.stmts.is_empty());
}

#[test]
fn p3_component_with_dimensions_parses() {
    let program = parse(
        "component SizedPanel(title: String) width=1024, height=768:\n    render <div><Text value={title} /></div>\n"
    ).expect("should parse component with dimensions");

    let Item::Component(component) = &program.items[0] else { panic!("expected component"); };

    let dims = component.dimensions.as_ref().expect("should have dimensions");
    let Expr::Int(w, _) = dims.width.as_ref().expect("should have width") else { panic!("expected int width"); };
    assert_eq!(*w, 1024);
    let Expr::Int(h, _) = dims.height.as_ref().expect("should have height") else { panic!("expected int height"); };
    assert_eq!(*h, 768);
}

#[test]
fn p3_component_with_width_only_parses() {
    let program = parse(
        "component WidthOnly() width=800:\n    render <div />\n"
    ).expect("should parse component with width only");

    let Item::Component(component) = &program.items[0] else { panic!("expected component"); };
    let dims = component.dimensions.as_ref().expect("should have dimensions");
    assert!(dims.width.is_some());
    assert!(dims.height.is_none());
}

#[test]
fn p3_component_without_dimensions_parses() {
    let program = parse(
        "component NoDims():\n    render <div />\n"
    ).expect("should parse component without dimensions");

    let Item::Component(component) = &program.items[0] else { panic!("expected component"); };
    assert!(component.dimensions.is_none());
    assert!(component.pulses.is_empty());
    assert!(component.resonates.is_empty());
}

#[test]
fn p3_component_with_callback_parses() {
    let program = parse(
        "fn handle_click():\n    ()\n\ncomponent Button(label: String):\n    render <button on_click={handle_click}>\n        <Text value={label} />\n    </button>\n"
    ).expect("should parse component with callback");

    let Item::Component(component) = &program.items[1] else { panic!("expected component as second item"); };

    let JSXNode::Element { attributes, .. } = &component.body else { panic!("expected element"); };

    let attr = attributes.iter().find(|a| a.name == "on_click").expect("should have on_click attr");
    match &attr.value {
        JSXAttrValue::Callback(event_kind, _expr) => {
            assert_eq!(event_kind, "click");
        }
        other => panic!("expected Callback variant, got {:?}", other),
    }
}

#[test]
fn p3_component_with_callback_string_fallback() {
    let program = parse(
        "component Button(label: String):\n    render <button on_click=\"handle_click\">\n        <Text value={label} />\n    </button>\n"
    ).expect("should parse on_click string as callback");

    let Item::Component(component) = &program.items[0] else { panic!("expected component"); };
    let JSXNode::Element { attributes, .. } = &component.body else { panic!("expected element"); };

    let attr = attributes.iter().find(|a| a.name == "on_click").expect("should have on_click attr");
    match &attr.value {
        JSXAttrValue::Callback(event_kind, _expr) => {
            assert_eq!(event_kind, "click");
        }
        other => panic!("expected Callback variant, got {:?}", other),
    }
}

#[test]
fn p3_regular_string_attr_still_works() {
    // Backward-compat: regular string attrs still produce String variant
    let program = parse(
        "component Label(text: String):\n    render <span class=\"my-class\">{text}</span>\n"
    ).expect("should parse regular string attrs");

    let Item::Component(component) = &program.items[0] else { panic!("expected component"); };
    let JSXNode::Element { attributes, .. } = &component.body else { panic!("expected element"); };

    let attr = attributes.iter().find(|a| a.name == "class").expect("should have class attr");
    assert!(matches!(attr.value, JSXAttrValue::String(_)));
}

#[test]
fn p3_pulse_with_jitter_parses_in_component() {
    let program = parse(
        "component JitterPulse():\n    pulse tick every 16ms jitter 2ms:\n        ()\n    render <div />\n"
    ).expect("should parse pulse with jitter in component");

    let Item::Component(component) = &program.items[0] else { panic!("expected component"); };
    assert_eq!(component.pulses.len(), 1);
    let pulse = &component.pulses[0];
    assert!(pulse.jitter.is_some());
    let jitter = pulse.jitter.as_ref().unwrap();
    assert_eq!(jitter.value, 2);
    assert_eq!(jitter.unit, "ms");
}
