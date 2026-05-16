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
        "(void)abi_entangle_register(binding->authority, binding->mirror, binding->policy, binding->type_name);"
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
fn abi_entangle_registered_count() -> Int

@extern
fn c_bridge_shared_buffer_info(target: Any) -> Any

fn native_entangle_count() -> Int:
    return abi_entangle_registered_count()

fn main() -> Int:
    return native_entangle_count()
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("typedef void* Any;"));
    assert!(c.contains("int64_t abi_entangle_registered_count(void);"));
    assert!(c.contains("Any c_bridge_shared_buffer_info(Any target);"));
    assert!(!c.contains("int64_t abi_entangle_registered_count(void) {"));
    assert!(!c.contains("Any c_bridge_shared_buffer_info(Any target) {"));
    assert!(c.contains("int64_t native_entangle_count(void) {"));
    assert!(c.contains("return abi_entangle_registered_count();"));
}

#[test]
fn c_backend_keeps_native_input_symbols_as_declarations() {
    let source = r#"
@extern
fn abi_input_session_create(name: String) -> Int

@extern
fn abi_input_begin_frame(session_id: Int, delta_ms: Float) -> Int

@extern
fn abi_input_action_pressed(session_id: Int, action: String) -> Int

fn main() -> Int:
    let session = abi_input_session_create("input")
    let _frame = abi_input_begin_frame(session, 16.0)
    return abi_input_action_pressed(session, "confirm")
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("int64_t abi_input_session_create(const char * name);"));
    assert!(c.contains("int64_t abi_input_begin_frame(int64_t session_id, double delta_ms);"));
    assert!(
        c.contains("int64_t abi_input_action_pressed(int64_t session_id, const char * action);")
    );
    assert!(!c.contains("int64_t abi_input_session_create(const char * name) {"));
    assert!(c.contains("return abi_input_action_pressed(session, \"confirm\");"));
}

#[test]
fn c_backend_keeps_native_process_symbols_as_declarations() {
    let source = r#"
@extern
fn abi_process_spec_create(executable: String) -> Int

@extern
fn abi_process_spec_set_stdout_mode(spec_id: Int, mode: String) -> Int

@extern
fn abi_process_spawn(spec_id: Int) -> Int

@extern
fn abi_process_wait(process_id: Int, timeout_ms: Int) -> Int

@extern
fn abi_process_stdout_capture_text(process_id: Int) -> String

fn main() -> Int:
    let spec = abi_process_spec_create("cmd.exe")
    let _stdout = abi_process_spec_set_stdout_mode(spec, "pipe")
    let child = abi_process_spawn(spec)
    let _wait = abi_process_wait(child, 5000)
    let _capture = abi_process_stdout_capture_text(child)
    return child
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("int64_t abi_process_spec_create(const char * executable);"));
    assert!(
        c.contains("int64_t abi_process_spec_set_stdout_mode(int64_t spec_id, const char * mode);")
    );
    assert!(c.contains("int64_t abi_process_spawn(int64_t spec_id);"));
    assert!(c.contains("int64_t abi_process_wait(int64_t process_id, int64_t timeout_ms);"));
    assert!(c.contains("const char * abi_process_stdout_capture_text(int64_t process_id);"));
    assert!(!c.contains("int64_t abi_process_spawn(int64_t spec_id) {"));
    assert!(c.contains("int64_t child = abi_process_spawn(spec);"));
}

#[test]
fn c_backend_keeps_native_net_symbols_as_declarations() {
    let source = r#"
@extern
fn abi_net_capability_state(capability_key: String) -> Int

@extern
fn abi_http_server_create(host: String, port: Int) -> Int

@extern
fn abi_http_server_route_actor(server_id: Int, method: String, path: String, actor_id: Int, message_kind: String) -> Int

@extern
fn abi_http_request_create(method: String, url: String) -> Int

@extern
fn abi_http_request_set_protocol(request_id: Int, protocol_name: String) -> Int

@extern
fn abi_http_client_send(request_id: Int) -> Int

@extern
fn abi_http_response_protocol(response_id: Int) -> String

@extern
fn abi_http_response_body_text(response_id: Int) -> String

fn main() -> Int:
    let _capability = abi_net_capability_state("http2.client")
    let server = abi_http_server_create("127.0.0.1", 0)
    let _route = abi_http_server_route_actor(server, "GET", "/actor", 9, "HttpRequest")
    let request = abi_http_request_create("GET", "http://127.0.0.1/")
    let _protocol = abi_http_request_set_protocol(request, "http/2")
    let response = abi_http_client_send(request)
    let _response_protocol = abi_http_response_protocol(response)
    let _body = abi_http_response_body_text(response)
    return response
"#;

    let program = typed_program_from_source(source);
    let c = generate_c(&program).expect("C generation should succeed");

    assert!(c.contains("int64_t abi_net_capability_state(const char * capability_key);"));
    assert!(c.contains("int64_t abi_http_server_create(const char * host, int64_t port);"));
    assert!(c.contains("int64_t abi_http_server_route_actor(int64_t server_id, const char * method, const char * path, int64_t actor_id, const char * message_kind);"));
    assert!(c.contains("int64_t abi_http_request_create(const char * method, const char * url);"));
    assert!(c.contains(
        "int64_t abi_http_request_set_protocol(int64_t request_id, const char * protocol_name);"
    ));
    assert!(c.contains("int64_t abi_http_client_send(int64_t request_id);"));
    assert!(c.contains("const char * abi_http_response_protocol(int64_t response_id);"));
    assert!(c.contains("const char * abi_http_response_body_text(int64_t response_id);"));
    assert!(!c.contains("int64_t abi_http_client_send(int64_t request_id) {"));
    assert!(c.contains("int64_t response = abi_http_client_send(request);"));
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
fn abi_actor_spawn(actor_name: String, init_payload: String) -> Int

@extern
fn abi_actor_send(actor_id: Int, message_name: String, data_payload: String) -> Int

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

    assert!(
        c.contains("int64_t abi_actor_spawn(const char * actor_name, const char * init_payload);")
    );
    assert!(c.contains("int main(void);"));
    assert!(c.contains("int64_t probe = abi_actor_spawn(\"Probe\", \"total=0\");"));
    assert!(c.contains("abi_actor_send(probe, \"Add\", \"value=3\");"));
}
