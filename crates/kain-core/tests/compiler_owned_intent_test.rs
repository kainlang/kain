use kain_core::runtime::{interpret_with_env, run_tests, Env, Value};
use kain_core::{
    build_ui_output_from_source, diagnostics, emit_realtime_app_bundle,
    emit_runtime_contract_bundle, error, lexer, parser, types, CompileTarget, EffectSet, IntSize,
    ResolvedType, TypedItem,
};

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    types::check(&ast, &span_mapper, "<test>")
}

fn parse_and_typecheck_with_extra_globals<I>(
    source: &str,
    extra_globals: I,
) -> Result<types::TypedProgram, error::KainError>
where
    I: IntoIterator<Item = (String, ResolvedType)>,
{
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    types::check_with_extra_globals(&ast, &span_mapper, "<test>", extra_globals)
}

fn unary_int_function_type() -> ResolvedType {
    ResolvedType::Function {
        params: vec![ResolvedType::Int(IntSize::I64)],
        ret: Box::new(ResolvedType::Int(IntSize::I64)),
        effects: EffectSet::new(),
    }
}

fn compiler_owned_intent_stage_globals() -> Vec<(String, ResolvedType)> {
    vec![
        ("native_plus_two".to_string(), unary_int_function_type()),
        ("py_plus_two".to_string(), unary_int_function_type()),
        ("js_plus_two".to_string(), unary_int_function_type()),
    ]
}

fn compiler_owned_intent_source() -> &'static str {
    r#"law positive(value: Int) -> Bool:
    return value >= 0

world Studio:
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

converge choose_value(value: Int) -> Int:
    spec reference:
        return value + 1
    fast default_lane:
        return value + 1
    verify random(8)

orchestrate pipeline(value: Int) -> Int:
    let a: Int = kain choose_value(value)
    let b: Int = rust native_plus_two(a)
    let c: Int = python py_plus_two(b)
    let d: Int = node js_plus_two(c)
    return d

fn main() -> Int:
    let studio = Studio
    let current = set_counter(studio, 41)
    if positive(current):
        return pipeline(current)
    return 0
"#
}

fn best_effort_patch_source() -> &'static str {
    r#"world Studio:
    state counter: Int = 0
    surface native_ui => App

component App():
    render <panel />

fn next_value(value: Int) -> Int:
    return value + 1

patch bump(studio: Studio) -> Int:
    studio.counter = next_value(studio.counter)
    return studio.counter
"#
}

fn extract_single_int_arg(name: &str, args: &[Value]) -> Result<i64, error::KainError> {
    match args {
        [Value::Int(value)] => Ok(*value),
        other => Err(error::KainError::runtime(format!(
            "{name} expected a single Int argument, found {other:?}"
        ))),
    }
}

fn extract_single_int_from_array_arg(name: &str, args: &[Value]) -> Result<i64, error::KainError> {
    match args {
        [Value::String(_), Value::Array(values)] => {
            let values = values
                .read()
                .map_err(|_| error::KainError::runtime(format!("{name} args lock poisoned")))?;
            extract_single_int_arg(name, &values)
        }
        [Value::String(_), Value::String(_), Value::Array(values)] => {
            let values = values
                .read()
                .map_err(|_| error::KainError::runtime(format!("{name} args lock poisoned")))?;
            extract_single_int_arg(name, &values)
        }
        other => Err(error::KainError::runtime(format!(
            "{name} expected a function name plus Int array args, found {other:?}"
        ))),
    }
}

fn native_plus_two(_env: &mut Env, args: Vec<Value>) -> Result<Value, error::KainError> {
    Ok(Value::Int(extract_single_int_arg("native_plus_two", &args)? + 2))
}

fn fake_py_call(_env: &mut Env, args: Vec<Value>) -> Result<Value, error::KainError> {
    Ok(Value::Int(extract_single_int_from_array_arg("py_call", &args)? + 2))
}

fn fake_js_call(_env: &mut Env, args: Vec<Value>) -> Result<Value, error::KainError> {
    Ok(Value::Int(extract_single_int_from_array_arg("js_call", &args)? + 2))
}

fn register_pipeline_stage_bridges(env: &mut Env) {
    env.register_native_fn("native_plus_two", native_plus_two);
    env.register_native_fn("py_call", fake_py_call);
    env.register_native_fn("js_call", fake_js_call);
}

#[test]
fn parse_and_typecheck_compiler_owned_intent_forms() {
    let typed = parse_and_typecheck_with_extra_globals(
        compiler_owned_intent_source(),
        compiler_owned_intent_stage_globals(),
    )
    .expect("typecheck");

    assert!(typed.items.iter().any(|item| matches!(item, TypedItem::Law(_))));
    assert!(typed.items.iter().any(|item| matches!(item, TypedItem::World(_))));
    assert!(typed.items.iter().any(|item| matches!(item, TypedItem::Patch(_))));
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
fn typecheck_rejects_world_without_any_surface() {
    let source = r#"world Broken:
    state counter: Int = 0

component App():
    render <panel />
"#;

    let error = parse_and_typecheck(source).expect_err("worlds without surfaces should fail");
    assert!(error.to_string().contains("must declare at least one surface"));
}

#[test]
fn typecheck_rejects_world_state_name_leakage() {
    let source = r#"world Studio:
    state counter: Int = 0
    surface native_ui => App

component App():
    render <panel />

fn main() -> Int:
    return counter
"#;

    let error = parse_and_typecheck(source).expect_err("bare world state names should not typecheck");
    assert!(error.to_string().contains("counter"));
}

#[test]
fn runtime_contract_emits_compiler_owned_intent_sections() {
    let typed = parse_and_typecheck_with_extra_globals(
        compiler_owned_intent_source(),
        compiler_owned_intent_stage_globals(),
    )
    .expect("typecheck");
    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);

    assert_eq!(bundle.patches.len(), 1);
    assert_eq!(bundle.laws.len(), 1);
    assert_eq!(bundle.converges.len(), 1);
    assert_eq!(bundle.worlds.len(), 1);
    assert_eq!(bundle.orchestrations.len(), 1);
    assert_eq!(bundle.worlds[0].surfaces.len(), 4);
    assert_eq!(bundle.patches[0].undo_mode, "reversible");
    assert_eq!(bundle.laws[0].name, "positive");
    assert_eq!(bundle.laws[0].return_type, "Bool");
    assert_eq!(bundle.converges[0].verify_random_count, Some(8));
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
        .any(|capability| capability.key == "law.invariants"));
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
    let typed =
        parse_and_typecheck_with_extra_globals(source, compiler_owned_intent_stage_globals())
            .expect("typecheck");
    let ui = build_ui_output_from_source(source, "App").expect("ui");
    let bundle = emit_realtime_app_bundle(&typed, Some(&ui), CompileTarget::Rust);

    assert_eq!(bundle.patches.len(), 1);
    assert_eq!(bundle.laws.len(), 1);
    assert_eq!(bundle.converges.len(), 1);
    assert_eq!(bundle.worlds.len(), 1);
    assert_eq!(bundle.orchestrations.len(), 1);
    assert_eq!(bundle.worlds[0].surfaces.len(), 4);
    assert_eq!(bundle.laws[0].name, "positive");
    assert_eq!(bundle.converges[0].verify_random_count, Some(8));
    assert_eq!(
        bundle
            .active_world
            .as_ref()
            .map(|world| world.name.as_str()),
        Some("Studio")
    );
    assert!(bundle.tool_caps.iter().any(|entry| entry == "patch.transactions"));
    assert!(bundle.tool_caps.iter().any(|entry| entry == "law.invariants"));
    assert!(bundle.tool_caps.iter().any(|entry| entry == "converge.dispatch"));
    assert!(bundle.tool_caps.iter().any(|entry| entry == "world.native-ui"));
    assert!(bundle.tool_caps.iter().any(|entry| entry == "world.viewport3d"));
    assert!(bundle.tool_caps.iter().any(|entry| entry == "world.web"));
    assert!(bundle.tool_caps.iter().any(|entry| entry == "world.ue5"));
    assert!(bundle
        .requirements
        .iter()
        .any(|entry| entry == "orchestrate.pipeline"));
}

#[test]
fn runtime_executes_patch_converge_law_and_orchestrate_and_records_patch_transactions() {
    let typed = parse_and_typecheck_with_extra_globals(
        compiler_owned_intent_source(),
        compiler_owned_intent_stage_globals(),
    )
    .expect("typecheck");
    let mut env = Env::new();
    register_pipeline_stage_bridges(&mut env);
    let result = interpret_with_env(&mut env, &typed).expect("interpret");

    match result {
        Value::Int(value) => assert_eq!(value, 48),
        other => panic!("expected Int result, found {other:?}"),
    }

    assert_eq!(env.patch_records().len(), 1);
    assert_eq!(env.patch_records()[0].name, "set_counter");
    assert_eq!(env.patch_records()[0].undo_mode, "reversible");
    assert_eq!(env.patch_records()[0].mutation_paths, vec!["studio.counter"]);
    assert_eq!(env.patch_records()[0].changes.len(), 1);
    assert_eq!(
        env.patch_records()[0].collaboration_event,
        "patch.set_counter.applied"
    );
    assert_eq!(env.patch_collaboration_events().len(), 1);
    assert_eq!(env.patch_collaboration_events()[0].patch_name, "set_counter");
    assert_eq!(
        env.patch_collaboration_events()[0].mutation_paths,
        vec!["studio.counter"]
    );
}

#[test]
fn laws_are_callable_directly_at_runtime() {
    let typed = parse_and_typecheck(
        r#"law non_negative(value: Int) -> Bool:
    return value >= 0

fn main() -> Bool:
    return non_negative(3)
"#,
    )
    .expect("typecheck");
    let mut env = Env::new();
    let result = interpret_with_env(&mut env, &typed).expect("interpret law");

    match result {
        Value::Bool(value) => assert!(value),
        other => panic!("expected Bool(true), got {other:?}"),
    }
}

#[test]
fn selectorless_fast_converge_lane_is_selected_in_declaration_order() {
    let typed = parse_and_typecheck(
        r#"converge choose_value(value: Int) -> Int:
    spec reference:
        return value + 1
    fast target_only when target("llvm"):
        return value + 200
    fast default_lane:
        return value + 2
    fast dispatch_lane when capability("converge.dispatch"):
        return value + 3

fn main() -> Int:
    return choose_value(1)
"#,
    )
    .expect("typecheck");
    let mut env = Env::new();
    let result = interpret_with_env(&mut env, &typed).expect("interpret converge");

    match result {
        Value::Int(value) => assert_eq!(value, 3),
        other => panic!("expected Int(3), got {other:?}"),
    }
}

#[test]
fn typecheck_rejects_verify_random_for_unsupported_types() {
    let source = r#"struct Payload:
    value: Int

converge choose_value(payload: Payload) -> Payload:
    spec reference:
        return payload
    fast default_lane:
        return payload
    verify random(1)
"#;

    let error = parse_and_typecheck(source).expect_err("verify random should reject structs");
    assert!(error
        .to_string()
        .contains("verify random(n) does not support parameter 'payload'"), "{error}");
}

#[test]
fn run_tests_reports_converge_verification_mismatch() {
    let source = r#"converge diverge(value: Int) -> Int:
    spec reference:
        return value + 1
    fast dispatch_lane:
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

#[test]
fn run_tests_reports_converge_synthesized_sample_mismatch() {
    let source = r#"converge drift(value: Int) -> Int:
    spec reference:
        return value + 1
    fast default_lane:
        if value == -1001:
            return value + 1
        return value + 2
    verify random(4)

test mismatch:
    let value: Int = drift(-1001)
"#;

    let typed = parse_and_typecheck(source).expect("typecheck");
    let error = run_tests(&typed).expect_err("synthesized samples should catch divergence");
    assert!(error.to_string().contains("during sample"));
}

#[test]
fn orchestrate_rejects_branch_local_stage_calls() {
    let source = r#"converge choose_value(value: Int) -> Int:
    spec reference:
        return value + 1
    fast default_lane:
        return value + 1

orchestrate pipeline(value: Int) -> Int:
    if value > 0:
        let chosen: Int = kain choose_value(value)
        return chosen
    return value
"#;

    let error =
        parse_and_typecheck(source).expect_err("branch-local orchestrate stage calls should fail");
    assert!(error.to_string().contains("top-level"), "{error}");
}

#[test]
fn orchestrate_rejects_nested_stage_calls() {
    let source = r#"fn wrap(value: Int) -> Int:
    return value

converge choose_value(value: Int) -> Int:
    spec reference:
        return value + 1
    fast default_lane:
        return value + 1

orchestrate pipeline(value: Int) -> Int:
    let chosen: Int = wrap(kain choose_value(value))
    return chosen
"#;

    let error = parse_and_typecheck(source).expect_err("nested stage calls should fail");
    assert!(error.to_string().contains("top-level"), "{error}");
}

#[test]
fn runtime_enforces_rust_stage_labels() {
    let typed = parse_and_typecheck(
        r#"fn plus_two(value: Int) -> Int:
    return value + 2

orchestrate pipeline(value: Int) -> Int:
    let next: Int = rust plus_two(value)
    return next

fn main() -> Int:
    return pipeline(1)
"#,
    )
    .expect("typecheck");
    let mut env = Env::new();
    let error = interpret_with_env(&mut env, &typed).expect_err("rust stage should require native fn");
    assert!(error.to_string().contains("must resolve to a native function"));
}

#[test]
fn runtime_enforces_python_stage_labels() {
    let typed = parse_and_typecheck_with_extra_globals(
        r#"orchestrate pipeline(value: Int) -> Int:
    let next: Int = python py_plus_two(value)
    return next

fn main() -> Int:
    return pipeline(1)
"#,
        vec![("py_plus_two".to_string(), unary_int_function_type())],
    )
    .expect("typecheck");
    let mut env = Env::new();
    let error = interpret_with_env(&mut env, &typed).expect_err("python stage should require bridge");
    assert!(error
        .to_string()
        .contains("python bridge is not registered"));
}

#[test]
fn runtime_enforces_node_stage_labels() {
    let typed = parse_and_typecheck_with_extra_globals(
        r#"orchestrate pipeline(value: Int) -> Int:
    let next: Int = node js_plus_two(value)
    return next

fn main() -> Int:
    return pipeline(1)
"#,
        vec![("js_plus_two".to_string(), unary_int_function_type())],
    )
    .expect("typecheck");
    let mut env = Env::new();
    let error = interpret_with_env(&mut env, &typed).expect_err("node stage should require bridge");
    assert!(error
        .to_string()
        .contains("node bridge is not registered"));
}
