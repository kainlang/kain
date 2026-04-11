use kain_core::runtime::{interpret_with_env, run_tests, Env, Value};
use kain_core::{
    build_ui_output_from_source, diagnostics, emit_realtime_app_bundle,
    emit_runtime_contract_bundle, error, lexer, parser, types, CompileTarget, TypedItem,
};

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    types::check(&ast, &span_mapper, "<test>")
}

fn compiler_owned_intent_source() -> &'static str {
    r#"world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

component App():
    render <panel title="Studio" />

patch set_counter(studio: Studio, to: Int) -> Int:
    studio.counter = to
    return studio.counter

fn plus_two(value: Int) -> Int:
    return value + 2

converge choose_value(value: Int) -> Int:
    spec reference:
        return value + 1
    fast interpret_lane when target("interpret"):
        return value + 1
    fast dispatch_lane when capability("converge.dispatch"):
        return value + 1
    verify random(8)

orchestrate pipeline(value: Int) -> Int:
    let a: Int = kain choose_value(value)
    let b: Int = rust plus_two(a)
    let c: Int = python plus_two(b)
    let d: Int = node plus_two(c)
    return d

fn main() -> Int:
    let studio = Studio
    let current = set_counter(studio, 41)
    return pipeline(current)
"#
}

fn best_effort_patch_source() -> &'static str {
    r#"world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

component App():
    render <panel />

fn next_value(value: Int) -> Int:
    return value + 1

patch bump(studio: Studio) -> Int:
    studio.counter = next_value(studio.counter)
    return studio.counter
"#
}

#[test]
fn parse_and_typecheck_compiler_owned_intent_forms() {
    let typed = parse_and_typecheck(compiler_owned_intent_source()).expect("typecheck");

    assert!(typed
        .items
        .iter()
        .any(|item| matches!(item, TypedItem::World(_))));
    assert!(typed
        .items
        .iter()
        .any(|item| matches!(item, TypedItem::Patch(_))));
    assert!(typed
        .items
        .iter()
        .any(|item| matches!(item, TypedItem::Converge(_))));
    assert!(typed
        .items
        .iter()
        .any(|item| matches!(item, TypedItem::Orchestrate(_))));
}

#[test]
fn typecheck_rejects_world_missing_required_surface() {
    let source = r#"world Broken:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "BrokenPreview"
    surface web => App

component App():
    render <panel />
"#;

    let error = parse_and_typecheck(source).expect_err("missing ue5 surface should fail");
    assert!(error.to_string().contains("missing required 'ue5' surface"));
}

#[test]
fn runtime_contract_emits_compiler_owned_intent_sections() {
    let typed = parse_and_typecheck(compiler_owned_intent_source()).expect("typecheck");
    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);

    assert_eq!(bundle.patches.len(), 1);
    assert_eq!(bundle.converges.len(), 1);
    assert_eq!(bundle.worlds.len(), 1);
    assert_eq!(bundle.orchestrations.len(), 1);
    assert_eq!(bundle.worlds[0].surfaces.len(), 4);
    assert_eq!(bundle.patches[0].undo_mode, "reversible");
    assert_eq!(
        bundle
            .active_world
            .as_ref()
            .map(|world| world.name.as_str()),
        Some("Studio")
    );
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "patch.transactions"));
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "converge.dispatch"));
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "world.native-ui"));
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "world.viewport3d"));
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "world.web"));
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "world.ue5"));
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "orchestrate.pipeline"));
}

#[test]
fn runtime_contract_marks_calling_patches_as_best_effort() {
    let typed = parse_and_typecheck(best_effort_patch_source()).expect("typecheck");
    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);

    assert_eq!(bundle.patches.len(), 1);
    assert_eq!(bundle.patches[0].undo_mode, "best_effort");
}

#[test]
fn realtime_bundle_emits_compiler_owned_intent_sections() {
    let source = compiler_owned_intent_source();
    let typed = parse_and_typecheck(source).expect("typecheck");
    let ui = build_ui_output_from_source(source, "App").expect("ui");
    let bundle = emit_realtime_app_bundle(&typed, Some(&ui), CompileTarget::Rust);

    assert_eq!(bundle.patches.len(), 1);
    assert_eq!(bundle.converges.len(), 1);
    assert_eq!(bundle.worlds.len(), 1);
    assert_eq!(bundle.orchestrations.len(), 1);
    assert_eq!(bundle.worlds[0].surfaces.len(), 4);
    assert_eq!(
        bundle
            .active_world
            .as_ref()
            .map(|world| world.name.as_str()),
        Some("Studio")
    );
    assert!(bundle
        .tool_caps
        .iter()
        .any(|entry| entry == "patch.transactions"));
    assert!(bundle
        .tool_caps
        .iter()
        .any(|entry| entry == "converge.dispatch"));
    assert!(bundle
        .tool_caps
        .iter()
        .any(|entry| entry == "world.native-ui"));
    assert!(bundle
        .tool_caps
        .iter()
        .any(|entry| entry == "world.viewport3d"));
    assert!(bundle.tool_caps.iter().any(|entry| entry == "world.web"));
    assert!(bundle.tool_caps.iter().any(|entry| entry == "world.ue5"));
    assert!(bundle
        .requirements
        .iter()
        .any(|entry| entry == "orchestrate.pipeline"));
}

#[test]
fn runtime_executes_patch_converge_and_orchestrate_and_records_patch_transactions() {
    let typed = parse_and_typecheck(compiler_owned_intent_source()).expect("typecheck");
    let mut env = Env::new();
    let result = interpret_with_env(&mut env, &typed).expect("interpret");

    match result {
        Value::Int(value) => assert_eq!(value, 48),
        other => panic!("expected Int result, found {:?}", other),
    }

    assert_eq!(env.patch_records().len(), 1);
    assert_eq!(env.patch_records()[0].name, "set_counter");
    assert_eq!(env.patch_records()[0].undo_mode, "reversible");
    assert_eq!(
        env.patch_records()[0].mutation_paths,
        vec!["studio.counter"]
    );
    assert_eq!(env.patch_records()[0].changes.len(), 1);
    assert_eq!(
        env.patch_records()[0].collaboration_event,
        "patch.set_counter.applied"
    );
    assert_eq!(env.patch_collaboration_events().len(), 1);
    assert_eq!(
        env.patch_collaboration_events()[0].patch_name,
        "set_counter"
    );
    assert_eq!(
        env.patch_collaboration_events()[0].mutation_paths,
        vec!["studio.counter"]
    );
}

#[test]
fn run_tests_reports_converge_verification_mismatch() {
    let source = r#"converge diverge(value: Int) -> Int:
    spec reference:
        return value + 1
    fast dispatch_lane when capability("converge.dispatch"):
        return value + 2
    verify random(4)

test mismatch:
    let value: Int = diverge(1)
"#;

    let typed = parse_and_typecheck(source).expect("typecheck");
    let error = run_tests(&typed).expect_err("test lane should reject diverging converge lanes");
    assert!(error
        .to_string()
        .contains("Converge verification failed for diverge"));
}
