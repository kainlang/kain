use kain_core::diagnostics::SpanMapper;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::types;
use kain_core::types::TypedProgram;
use kain_sys_codegen::generate_c;

fn typed_program_from_source(source: &str) -> TypedProgram {
    let tokens = Lexer::new(source).tokenize().expect("lexer should succeed");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "<c-codegen-test>")
        .parse()
        .expect("parser should succeed");
    types::check(&program, &mapper, "<c-codegen-test>").expect("typecheck should succeed")
}

#[test]
fn c_backend_generates_compiler_owned_intent_callables_and_world_state() {
    let source = r#"
world Physics:
    state player_health: Int = 100
    surface native_ui => App

world UI:
    state health_display: Int = 100
    surface web => App

component App():
    render <panel />

entangle Physics.player_health <-> UI.health_display with single_writer

patch set_health(physics: Physics, value: Int) -> Int:
    physics.player_health = value
    return physics.player_health

law health_valid(value: Int) -> Bool:
    return value >= 0

converge choose_value(value: Int) -> Int:
    spec reference:
        return value + 1
    fast interpret_lane when target("interpret"):
        return value + 1

fn stage_bias(value: Int) -> Int:
    return value + 2

orchestrate pipeline(value: Int) -> Int:
    let staged: Int = kain choose_value(value)
    let echoed: Int = rust stage_bias(staged)
    return echoed

fn main() -> Int:
    let physics = Physics
    let updated = set_health(physics, 7)
    if health_valid(updated):
        return pipeline(updated)
    return 0
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("typedef struct Physics Physics;"));
    assert!(c.contains("struct Physics {"));
    assert!(c.contains("static Physics __kain_world_Physics = {.player_health = 100};"));
    assert!(c.contains("KainCEntangleBinding"));
    assert!(c.contains("\"Physics.player_health\""));
    assert!(c.contains("\"UI.health_display\""));
    assert!(c.contains("\"single_writer\""));
    assert!(c.contains("static void __kain_register_entanglements(void)"));
    assert!(c.contains("__kain_register_entanglements();"));
    assert!(c.contains(
        "(void)kain_native_entangle_register(binding->authority, binding->mirror, binding->policy, binding->type_name);"
    ));
    assert!(c.contains("int64_t set_health(Physics* physics, int64_t value);"));
    assert!(c.contains("bool health_valid(int64_t value);"));
    assert!(c.contains("int64_t choose_value(int64_t value);"));
    assert!(c.contains("int64_t pipeline(int64_t value);"));
    assert!(c.contains("int main(void);"));
    assert!(c.contains("physics->player_health = value;"));
    assert!(c.contains("/* orchestrate stage kain */ choose_value(value)"));
    assert!(c.contains("/* orchestrate stage rust */ stage_bias(staged)"));
}

#[test]
fn c_backend_keeps_extern_stdlib_symbols_as_declarations() {
    let source = r#"
@extern
fn kain_native_entangle_registered_count() -> Int

@extern
fn c_bridge_shared_buffer_info(target: Any) -> Any

fn native_entangle_count() -> Int:
    return kain_native_entangle_registered_count()

fn main() -> Int:
    return native_entangle_count()
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("typedef void* Any;"));
    assert!(c.contains("int64_t kain_native_entangle_registered_count(void);"));
    assert!(c.contains("Any c_bridge_shared_buffer_info(Any target);"));
    assert!(!c.contains("int64_t kain_native_entangle_registered_count(void) {"));
    assert!(!c.contains("Any c_bridge_shared_buffer_info(Any target) {"));
    assert!(c.contains("int64_t native_entangle_count(void) {"));
    assert!(c.contains("return kain_native_entangle_registered_count();"));
}

#[test]
fn c_backend_keeps_native_input_symbols_as_declarations() {
    let source = r#"
@extern
fn kain_native_input_session_create(name: String) -> Int

@extern
fn kain_native_input_begin_frame(session_id: Int, delta_ms: Float) -> Int

@extern
fn kain_native_input_action_pressed(session_id: Int, action: String) -> Int

fn main() -> Int:
    let session = kain_native_input_session_create("input")
    let _frame = kain_native_input_begin_frame(session, 16.0)
    return kain_native_input_action_pressed(session, "confirm")
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("int64_t kain_native_input_session_create(const char * name);"));
    assert!(
        c.contains("int64_t kain_native_input_begin_frame(int64_t session_id, double delta_ms);")
    );
    assert!(c.contains(
        "int64_t kain_native_input_action_pressed(int64_t session_id, const char * action);"
    ));
    assert!(!c.contains("int64_t kain_native_input_session_create(const char * name) {"));
    assert!(c.contains("return kain_native_input_action_pressed(session, \"confirm\");"));
}

#[test]
fn c_backend_keeps_native_process_symbols_as_declarations() {
    let source = r#"
@extern
fn kain_native_process_spec_create(executable: String) -> Int

@extern
fn kain_native_process_spec_set_stdout_mode(spec_id: Int, mode: String) -> Int

@extern
fn kain_native_process_spawn(spec_id: Int) -> Int

@extern
fn kain_native_process_wait(process_id: Int, timeout_ms: Int) -> Int

@extern
fn kain_native_process_stdout_capture_text(process_id: Int) -> String

fn main() -> Int:
    let spec = kain_native_process_spec_create("cmd.exe")
    let _stdout = kain_native_process_spec_set_stdout_mode(spec, "pipe")
    let child = kain_native_process_spawn(spec)
    let _wait = kain_native_process_wait(child, 5000)
    let _capture = kain_native_process_stdout_capture_text(child)
    return child
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("int64_t kain_native_process_spec_create(const char * executable);"));
    assert!(c.contains(
        "int64_t kain_native_process_spec_set_stdout_mode(int64_t spec_id, const char * mode);"
    ));
    assert!(c.contains("int64_t kain_native_process_spawn(int64_t spec_id);"));
    assert!(c.contains("int64_t kain_native_process_wait(int64_t process_id, int64_t timeout_ms);"));
    assert!(c.contains("const char * kain_native_process_stdout_capture_text(int64_t process_id);"));
    assert!(!c.contains("int64_t kain_native_process_spawn(int64_t spec_id) {"));
    assert!(c.contains("int64_t child = kain_native_process_spawn(spec);"));
}

#[test]
fn c_backend_lowers_string_equality_to_strcmp() {
    let source = r#"
fn main() -> Int:
    let value = "hello"
    if value == "hello":
        return 0
    if value != "world":
        return 1
    return 2
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("#include <string.h>"));
    assert!(c.contains("(strcmp(value, \"hello\") == 0)"));
    assert!(c.contains("(strcmp(value, \"world\") != 0)"));
}

#[test]
fn c_backend_lowers_actor_spawn_and_send_to_native_runtime_facade() {
    let source = r#"
@extern
fn kain_native_actor_spawn(actor_name: String, init_payload: String) -> Int

@extern
fn kain_native_actor_send(actor_id: Int, message_name: String, data_payload: String) -> Int

actor Probe:
    state total: Int = 0

    on Add(value: Int):
        self.total = self.total + value

fn main() -> Int:
    let probe = spawn Probe(total = 0)
    send probe.Add(value = 3)
    return 0
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains(
        "int64_t kain_native_actor_spawn(const char * actor_name, const char * init_payload);"
    ));
    assert!(c.contains("int main(void);"));
    assert!(c.contains("int64_t probe = kain_native_actor_spawn(\"Probe\", \"total=0\");"));
    assert!(c.contains("kain_native_actor_send(probe, \"Add\", \"value=3\");"));
}
