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
    state reaction: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

component App():
    render <panel title="Studio" />

patch set_counter(studio: Studio, to: Int) -> Int:
    studio.counter = to
    return studio.counter

resonate Studio.counter dampen 16ms:
    Studio.reaction = Studio.counter + resonate_new_i64

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

fn entangle_source() -> &'static str {
    r#"world Physics:
    state player_health: Int = 100
    surface native_ui => App

world UI:
    state health_display: Int = 100
    surface web => App

component App():
    render <panel />

entangle Physics.player_health <-> UI.health_display with single_writer

fn main() -> Int:
    Physics.player_health -= 10
    return UI.health_display
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
    Ok(Value::Int(
        extract_single_int_arg("native_plus_two", &args)? + 2,
    ))
}

fn fake_py_call(_env: &mut Env, args: Vec<Value>) -> Result<Value, error::KainError> {
    Ok(Value::Int(
        extract_single_int_from_array_arg("py_call", &args)? + 2,
    ))
}

fn fake_js_call(_env: &mut Env, args: Vec<Value>) -> Result<Value, error::KainError> {
    Ok(Value::Int(
        extract_single_int_from_array_arg("js_call", &args)? + 2,
    ))
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

    assert!(typed
        .items
        .iter()
        .any(|item| matches!(item, TypedItem::Law(_))));
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
fn parse_and_typecheck_entangle_intent_form() {
    let typed = parse_and_typecheck(entangle_source()).expect("entangle source should typecheck");

    assert!(typed
        .items
        .iter()
        .any(|item| matches!(item, TypedItem::Entangle(_))));
}

#[test]
fn typecheck_rejects_entangle_type_mismatch() {
    let source = r#"world Physics:
    state player_health: Int = 100
    surface native_ui => App

world UI:
    state health_display: String = "100"
    surface web => App

component App():
    render <panel />

entangle Physics.player_health <-> UI.health_display with single_writer
"#;

    let error = parse_and_typecheck(source).expect_err("mismatched endpoints should fail");
    assert!(error.to_string().contains("entangle endpoint"), "{error}");
}

#[test]
fn typecheck_rejects_world_without_any_surface() {
    let source = r#"world Broken:
    state counter: Int = 0

component App():
    render <panel />
"#;

    let error = parse_and_typecheck(source).expect_err("worlds without surfaces should fail");
    assert!(error
        .to_string()
        .contains("must declare at least one surface"));
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

    let error =
        parse_and_typecheck(source).expect_err("bare world state names should not typecheck");
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
    assert_eq!(bundle.resonances.len(), 1);
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
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "state.resonate"));
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "reactivity.shadow-patch"));
    assert_eq!(bundle.resonances[0].target, "Studio.counter");
    assert_eq!(bundle.resonances[0].dampen_ns, 16_000_000);
}

#[test]
fn runtime_contract_emits_entangle_contracts() {
    let typed = parse_and_typecheck(entangle_source()).expect("typecheck");
    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);

    assert_eq!(bundle.entanglements.len(), 1);
    assert_eq!(bundle.entanglements[0].authority, "Physics.player_health");
    assert_eq!(bundle.entanglements[0].mirror, "UI.health_display");
    assert_eq!(bundle.entanglements[0].policy, "single_writer");
    assert_eq!(bundle.entanglements[0].type_name, "Int");
    assert!(bundle
        .required_capabilities
        .iter()
        .any(|capability| capability.key == "state.entangle"));
}

#[test]
fn resonate_rejects_direct_self_feedback() {
    let source = r#"world Reactor:
    state signal: Int = 0
    surface native_ui => App

component App():
    render <panel />

resonate Reactor.signal dampen 1ms:
    Reactor.signal = Reactor.signal + 1
"#;

    let error =
        parse_and_typecheck(source).expect_err("direct resonance feedback should not typecheck");
    assert!(error.to_string().contains("directly mutates its own target"));
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
    assert_eq!(bundle.resonances.len(), 1);
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
    assert!(bundle
        .tool_caps
        .iter()
        .any(|entry| entry == "patch.transactions"));
    assert!(bundle
        .tool_caps
        .iter()
        .any(|entry| entry == "law.invariants"));
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
    assert!(bundle
        .tool_caps
        .iter()
        .any(|entry| entry == "state.resonate"));
    assert!(bundle
        .requirements
        .iter()
        .any(|entry| entry == "reactivity.shadow-patch"));
    assert_eq!(
        bundle.resonances[0].handler_symbol,
        "__kain_resonate_resonate__Studio__counter"
    );
}

#[test]
fn realtime_bundle_emits_entangle_bindings() {
    let source = entangle_source();
    let typed = parse_and_typecheck(source).expect("typecheck");
    let ui = build_ui_output_from_source(source, "App").expect("ui");
    let bundle = emit_realtime_app_bundle(&typed, Some(&ui), CompileTarget::Rust);

    assert_eq!(bundle.entanglements.len(), 1);
    assert_eq!(bundle.entanglements[0].authority, "Physics.player_health");
    assert_eq!(bundle.entanglements[0].mirror, "UI.health_display");
    assert_eq!(bundle.entanglements[0].policy, "single_writer");
    assert!(bundle
        .tool_caps
        .iter()
        .any(|entry| entry == "state.entangle"));
    assert!(bundle
        .requirements
        .iter()
        .any(|entry| entry == "state.entangle"));
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
fn runtime_entangle_propagates_authority_writes_to_mirror() {
    let typed = parse_and_typecheck(entangle_source()).expect("typecheck");
    let mut env = Env::new();
    let result = interpret_with_env(&mut env, &typed).expect("interpret entangle");

    match result {
        Value::Int(value) => assert_eq!(value, 90),
        other => panic!("expected Int(90), got {other:?}"),
    }
}

#[test]
fn runtime_entangle_rejects_direct_mirror_writes() {
    let source = r#"world Physics:
    state player_health: Int = 100
    surface native_ui => App

world UI:
    state health_display: Int = 100
    surface web => App

component App():
    render <panel />

entangle Physics.player_health <-> UI.health_display with single_writer

fn main() -> Int:
    UI.health_display = 1
    return UI.health_display
"#;
    let typed = parse_and_typecheck(source).expect("typecheck");
    let mut env = Env::new();
    let error = interpret_with_env(&mut env, &typed).expect_err("mirror write should fail");

    assert!(
        error
            .to_string()
            .contains("cannot write entangle mirror 'UI.health_display'"),
        "{error}"
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
    assert!(
        error
            .to_string()
            .contains("verify random(n) does not support parameter 'payload'"),
        "{error}"
    );
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
fn orchestrate_graph_metadata_emits_runtime_and_realtime_contracts() {
    let source = r#"component GraphPanel():
    render <panel title="Graph" />

world GraphAuthority:
    state value: Int = 0
    surface native_ui => GraphPanel

fn portable_guard(value: Int) -> Int:
    return value

axiom graph_truth:
    when target("llvm")
    when capability("gpu.compute")
    guarantee "graph orchestration may stage cpu and gpu residency"
    fallback portable_guard

law non_negative(value: Int) -> Bool:
    return value >= 0

patch record(authority: GraphAuthority, value: Int) -> Int:
    authority.value = value
    return authority.value

fn plus_one(value: Int) -> Int:
    return value + 1

orchestrate pipeline(authority: GraphAuthority, value: Int) -> Int:
    stage seed: cpu plus_one(value) residency host policy static
    stage device: gpu plus_one(seed) after seed residency device transfer host_to_device guarded by graph_truth fallback degrade seed policy telemetry_prefer_gpu
    stage legal: law non_negative(device) after device residency host transfer device_to_host policy static
    stage committed: patch record(authority, device) after legal requires legal residency host policy telemetry_balance_latency
    return committed
"#;

    let typed = parse_and_typecheck(source).expect("graph orchestrate should typecheck");
    let runtime_bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Llvm);
    let orchestration = &runtime_bundle.orchestrations[0];
    assert!(orchestration.graph_mode);
    assert!(orchestration.adaptive_policy);
    assert_eq!(orchestration.stages[1].binding_name, "device");
    assert_eq!(orchestration.stages[1].dependencies, vec!["seed"]);
    assert_eq!(orchestration.stages[1].residency.as_deref(), Some("device"));
    assert_eq!(
        orchestration.stages[1].transfer.as_deref(),
        Some("host_to_device")
    );
    assert_eq!(
        orchestration.stages[1].guard.as_deref(),
        Some("graph_truth")
    );
    assert_eq!(
        orchestration.stages[1].fallback.as_deref(),
        Some("degrade seed")
    );
    assert_eq!(
        orchestration.stages[1].policy.as_deref(),
        Some("telemetry_prefer_gpu")
    );
    assert_eq!(orchestration.stages[3].requires.as_deref(), Some("legal"));
    assert!(orchestration.stages[3].adaptive_policy);

    let ui = build_ui_output_from_source(source, "GraphPanel").expect("ui");
    let realtime_bundle = emit_realtime_app_bundle(&typed, Some(&ui), CompileTarget::Llvm);
    let realtime = &realtime_bundle.orchestrations[0];
    assert!(realtime.graph_mode);
    assert!(realtime.adaptive_policy);
    assert_eq!(
        realtime.stages[2].transfer.as_deref(),
        Some("device_to_host")
    );
}

#[test]
fn orchestrate_graph_rejects_non_axiom_guard() {
    let source = r#"fn plus_one(value: Int) -> Int:
    return value + 1

orchestrate pipeline(value: Int) -> Int:
    stage seed: cpu plus_one(value) guarded by plus_one
    return seed
"#;

    let error = parse_and_typecheck(source).expect_err("function guard should fail");
    assert!(
        error.to_string().contains("must reference an axiom"),
        "{error}"
    );
}

#[test]
fn orchestrate_graph_rejects_dependency_cycles() {
    let source = r#"fn plus_one(value: Int) -> Int:
    return value + 1

orchestrate pipeline(value: Int) -> Int:
    stage left: cpu plus_one(value) after right
    stage right: cpu plus_one(left) after left
    return right
"#;

    let error = parse_and_typecheck(source).expect_err("cycle should fail");
    assert!(error.to_string().contains("dependency cycle"), "{error}");
}

#[test]
fn orchestrate_graph_rejects_impossible_transfer_residency_pairs() {
    let source = r#"fn plus_one(value: Int) -> Int:
    return value + 1

orchestrate pipeline(value: Int) -> Int:
    stage seed: gpu plus_one(value) residency host transfer host_to_device
    return seed
"#;

    let error = parse_and_typecheck(source).expect_err("impossible transfer should fail");
    assert!(
        error.to_string().contains("incompatible with residency"),
        "{error}"
    );
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
    let error =
        interpret_with_env(&mut env, &typed).expect_err("rust stage should require native fn");
    assert!(error
        .to_string()
        .contains("must resolve to a native function"));
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
    let error =
        interpret_with_env(&mut env, &typed).expect_err("python stage should require bridge");
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
    assert!(error.to_string().contains("node bridge is not registered"));
}
