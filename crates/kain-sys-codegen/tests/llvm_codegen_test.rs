use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

use kain_core::ast::{
    BinaryOp, Block, Component, Expr, Field, Function, Impl, JSXAttrValue, JSXAttribute, JSXNode,
    MatchArm, Param, Pattern, Stmt, Struct, Type, Visibility,
};
use kain_core::diagnostics::SpanMapper;
use kain_core::effects::EffectSet;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::types;
use kain_core::types::{
    IntSize, ResolvedType, TypedComponent, TypedFunction, TypedImpl, TypedItem, TypedProgram,
    TypedStruct,
};
use kain_core::Span;
use kain_sys_codegen::generate_llvm;

fn span() -> Span {
    (0..0).into()
}

fn string_type() -> Type {
    Type::Named {
        name: "String".to_string(),
        generics: vec![],
        span: span(),
    }
}

fn int_type() -> Type {
    Type::Named {
        name: "Int".to_string(),
        generics: vec![],
        span: span(),
    }
}

fn float_type() -> Type {
    Type::Named {
        name: "Float".to_string(),
        generics: vec![],
        span: span(),
    }
}

fn typed_program_from_source(source: &str) -> TypedProgram {
    let tokens = Lexer::new(source).tokenize().expect("lexer should succeed");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "<llvm-test>")
        .parse()
        .expect("parser should succeed");
    types::check(&program, &mapper, "<llvm-test>").expect("typecheck should succeed")
}

fn llvm_function_ir<'a>(llvm: &'a str, signature: &str) -> &'a str {
    let start = llvm
        .find(signature)
        .expect("function signature should exist");
    let rest = &llvm[start..];
    let end = rest.find("\n}").expect("function body should close");
    &rest[..end]
}

fn verify_llvm_ir_with_repo_llvm_as(llvm: &str, test_name: &str) {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let llvm_as = manifest_dir
        .join("..")
        .join("..")
        .join("toolchain")
        .join("llvm")
        .join("bin")
        .join(if cfg!(windows) {
            "llvm-as.exe"
        } else {
            "llvm-as"
        });

    if !llvm_as.exists() {
        return;
    }

    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("system time should be valid")
        .as_nanos();
    let temp_dir = std::env::temp_dir().join(format!(
        "kain-llvm-verify-{}-{}-{}",
        test_name,
        std::process::id(),
        nonce
    ));
    fs::create_dir_all(&temp_dir).expect("temp dir should be created");

    let ll_path = temp_dir.join("module.ll");
    let bc_path = temp_dir.join("module.bc");
    fs::write(&ll_path, llvm).expect("llvm ir should be written");

    let output = Command::new(&llvm_as)
        .arg(&ll_path)
        .arg("-o")
        .arg(&bc_path)
        .output()
        .expect("llvm-as should launch");

    let _ = fs::remove_file(&ll_path);
    let _ = fs::remove_file(&bc_path);
    let _ = fs::remove_dir(&temp_dir);

    assert!(
        output.status.success(),
        "llvm-as rejected generated ir for {}:\n{}",
        test_name,
        String::from_utf8_lossy(&output.stderr)
    );
}

#[test]
fn llvm_hoists_loop_local_allocas_to_function_entry() {
    let source = r#"
fn main() -> Int:
    let frame = 0
    while frame < 4:
        let next_frame = frame + 1
        let doubled = next_frame * 2
        frame = doubled - next_frame
    return frame
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    let main_start = llvm
        .find("define i64 @main()")
        .expect("main function should be emitted");
    let main_rest = &llvm[main_start..];
    let main_end = main_rest.find("\n}").expect("main function should close");
    let main_ir = &main_rest[..main_end];
    let first_non_entry_label = main_ir
        .find("\nL")
        .expect("loop lowering should emit non-entry labels");
    let entry_region = &main_ir[..first_non_entry_label];
    let loop_region = &main_ir[first_non_entry_label..];

    assert!(entry_region.contains("%next_frame.addr_"));
    assert!(entry_region.contains("%doubled.addr_"));
    assert!(
        !loop_region.contains(" = alloca "),
        "loop body must not allocate fresh stack slots per iteration:\n{}",
        loop_region
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "loop-alloca-hoisting");
}

#[test]
fn llvm_resolves_top_level_const_values() {
    let source = r#"
const ANSWER_BASE: Int = 40
const ANSWER: Int = ANSWER_BASE + 2
const CONST_ENABLED: Bool = true
const CONST_NAME: String = "kain-llvm"

fn main() -> Int:
    let value = ANSWER
    if CONST_ENABLED && CONST_NAME == "kain-llvm":
        return value
    return 0
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("@__kain_const_ANSWER_BASE = internal constant i64 40"));
    assert!(llvm.contains("@__kain_const_CONST_ENABLED = internal constant i1 1"));
    assert!(llvm.contains("@__kain_const_ANSWER = internal global i64 zeroinitializer"));
    assert!(llvm.contains("@__kain_const_CONST_NAME = internal global i8* zeroinitializer"));
    assert!(llvm.contains("define void @__kain_init_const_ANSWER()"));
    assert!(llvm.contains("define void @__kain_init_const_CONST_NAME()"));
    assert!(llvm.contains("call void @__kain_init_const_ANSWER()"));
    assert!(llvm.contains("call void @__kain_init_const_CONST_NAME()"));
    assert!(llvm.contains("load i64, i64* @__kain_const_ANSWER_BASE"));
    assert!(llvm.contains("load i64, i64* @__kain_const_ANSWER"));
    assert!(llvm.contains("load i8*, i8** @__kain_const_CONST_NAME"));

    verify_llvm_ir_with_repo_llvm_as(&llvm, "top-level-const-values");
}

#[test]
fn llvm_registers_nested_module_const_values() {
    let source = r#"
mod math:
    const HALF_PI: Float = 1.5707963267948966

    fn half_turn() -> Float:
        return HALF_PI

fn main() -> Int:
    return 0
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("@__kain_const_HALF_PI = internal constant double 1.570796"));
    assert!(llvm.contains("load double, double* @__kain_const_HALF_PI"));

    verify_llvm_ir_with_repo_llvm_as(&llvm, "nested-module-const-values");
}

#[test]
fn llvm_lowers_named_vec_fields_and_tuple_alias_access() {
    let source = r#"
struct Bounds:
    min: Vec3
    max: Vec3

fn x_component(v: Vec3) -> Int:
    if v.x < v.z:
        return 1
    return 0

fn bounds_mix(bounds: Bounds) -> Int:
    if bounds.min.x < bounds.max.z:
        return 1
    return 0

fn main() -> Int:
    let bounds = Bounds { min: vec3(1.0, 2.0, 3.0), max: vec3(4.0, 5.0, 6.0) }
    return x_component(bounds.min) + bounds_mix(bounds)
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("%__kain_tuple_double_double_double = type { double, double, double }"));
    assert!(llvm.contains(
        "%Bounds = type { %__kain_tuple_double_double_double, %__kain_tuple_double_double_double }"
    ));
    assert!(llvm.contains("@x_component(%__kain_tuple_double_double_double %arg0)"));
    assert!(llvm.contains("@bounds_mix(%Bounds %arg0)"));
    assert!(llvm.contains("getelementptr inbounds %__kain_tuple_double_double_double"));

    verify_llvm_ir_with_repo_llvm_as(&llvm, "vec-field-tuple-alias-access");
}

#[test]
fn llvm_uses_explicit_native_stdlib_wrapper_string_signatures() {
    let source = r#"
@extern
fn abi_fs_read_text(path: String) -> String

pub fn fs_read_text(path: String) -> String:
    return abi_fs_read_text(path)

fn main() -> Int:
    let value = fs_read_text("note.txt")
    if value == "hello":
        return 1
    return 0
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(llvm.contains("declare i8* @abi_fs_read_text(i8* %arg0)"));
    assert!(llvm.contains("define i8* @fs_read_text(i8* %arg0)"));
    assert!(!llvm.contains("declare i8* @fs_read_text(i8*"));
    assert!(llvm.contains("call i1 @deep_eq(i8*"));
}

#[test]
fn llvm_lowers_map_get_string_literals_as_borrowed_static_byte_views() {
    let source = r#"
fn lookup_value(metrics: Int) -> Int:
    return map_get(metrics, "alpha")

fn main() -> Int:
    let metrics = map_new()
    map_set(metrics, "alpha", 7)
    return lookup_value(metrics)
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");
    let lookup_ir = llvm_function_ir(&llvm, "define internal i64 @lookup_value(i64 %arg0)");

    assert!(lookup_ir.contains("call i64 @map_get_prehashed"));
    assert!(
        !lookup_ir.contains("call i8* @string_new"),
        "map_get literal path should use a borrowed static byte pointer instead of heap string allocation:\n{}",
        lookup_ir
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "map-get-borrowed-static-string");
}

#[test]
fn llvm_lowers_map_set_string_literals_as_static_prehashed_inserts() {
    let source = r#"
fn main() -> Int:
    let metrics = map_new()
    map_set(metrics, "alpha", 11)
    return map_get(metrics, "alpha")
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");
    let main_ir = llvm_function_ir(&llvm, "define i64 @main()");

    assert!(
        main_ir.contains("call void @map_set_static_prehashed"),
        "map_set literal path should call the static prehashed insert helper:\n{}",
        main_ir
    );
    assert!(
        !main_ir.contains("call i8* @string_new"),
        "map_set literal path should not allocate heap strings for static keys:\n{}",
        main_ir
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "map-set-static-prehashed-string");
}

#[test]
fn llvm_lowers_native_input_action_primitives() {
    let source = r#"
@extern
fn abi_input_session_create(name: String) -> Int

@extern
fn abi_input_bind_action(session_id: Int, source_kind: String, event_kind: String, code: String, action: String) -> Int

@extern
fn abi_input_push_agent_intent(session_id: Int, source_id: String, action: String, command_text: String, confidence: Float) -> Int

@extern
fn abi_input_begin_frame(session_id: Int, delta_ms: Float) -> Int

@extern
fn abi_input_action_pressed(session_id: Int, action: String) -> Int

fn main() -> Int:
    let session = abi_input_session_create("agent-input")
    let _binding = abi_input_bind_action(session, "human.keyboard", "key_down", "Enter", "confirm")
    let _event = abi_input_push_agent_intent(session, "codex", "confirm", "activate", 0.95)
    let _frame = abi_input_begin_frame(session, 16.0)
    return abi_input_action_pressed(session, "confirm")
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(llvm.contains("declare i64 @abi_input_session_create(i8* %arg0)"));
    assert!(llvm.contains(
        "declare i64 @abi_input_bind_action(i64 %arg0, i8* %arg1, i8* %arg2, i8* %arg3, i8* %arg4)"
    ));
    assert!(llvm.contains("declare i64 @abi_input_push_agent_intent(i64 %arg0, i8* %arg1, i8* %arg2, i8* %arg3, double %arg4)"));
    assert!(llvm.contains("call i64 @abi_input_action_pressed"));
    assert!(llvm.contains("agent-input"));
    assert!(llvm.contains("codex"));
}

#[test]
fn llvm_lowers_native_process_and_pty_primitives() {
    let source = r#"
@extern
fn abi_process_spec_create(executable: String) -> Int

@extern
fn abi_process_spec_add_arg(spec_id: Int, argument: String) -> Int

@extern
fn abi_process_spec_set_stdout_mode(spec_id: Int, mode: String) -> Int

@extern
fn abi_process_spawn(spec_id: Int) -> Int

@extern
fn abi_process_wait(process_id: Int, timeout_ms: Int) -> Int

@extern
fn abi_process_stdout_capture_text(process_id: Int) -> String

@extern
fn abi_process_spawn_pty(spec_id: Int, columns: Int, rows: Int) -> Int

@extern
fn abi_process_pty_write_text(process_id: Int, text: String) -> Int

@extern
fn abi_process_pty_capture_text(process_id: Int) -> String

fn main() -> Int:
    let echo = abi_process_spec_create("cmd.exe")
    let _arg_a = abi_process_spec_add_arg(echo, "/d")
    let _arg_b = abi_process_spec_add_arg(echo, "/c")
    let _arg_c = abi_process_spec_add_arg(echo, "echo process-proof")
    let _stdout = abi_process_spec_set_stdout_mode(echo, "pipe")
    let child = abi_process_spawn(echo)
    let _wait = abi_process_wait(child, 5000)
    let captured = abi_process_stdout_capture_text(child)

    let shell = abi_process_spec_create("cmd.exe")
    let pty = abi_process_spawn_pty(shell, 120, 40)
    let _write = abi_process_pty_write_text(pty, "echo pty-proof\r\nexit\r\n")
    let interactive = abi_process_pty_capture_text(pty)

    if captured != "" and interactive != "":
        return child + pty
    return 0
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(llvm.contains("declare i64 @abi_process_spec_create(i8* %arg0)"));
    assert!(llvm.contains("declare i64 @abi_process_spec_add_arg(i64 %arg0, i8* %arg1)"));
    assert!(llvm.contains("declare i64 @abi_process_spawn(i64 %arg0)"));
    assert!(llvm.contains("declare i64 @abi_process_spawn_pty(i64 %arg0, i64 %arg1, i64 %arg2)"));
    assert!(llvm.contains("declare i8* @abi_process_stdout_capture_text(i64 %arg0)"));
    assert!(llvm.contains("call i64 @abi_process_pty_write_text"));
    assert!(llvm.contains("echo process-proof"));
    assert!(llvm.contains("echo pty-proof"));
}

#[test]
fn llvm_lowers_native_net_tcp_http_and_actor_route_primitives() {
    let source = r#"
@extern
fn abi_net_platform_name() -> String

@extern
fn abi_net_capability_state(capability_key: String) -> Int

@extern
fn abi_tcp_connect(host: String, port: Int, timeout_ms: Int) -> Int

@extern
fn abi_tcp_write_text(connection_id: Int, payload: String) -> Int

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

@extern
fn abi_http_server_create(host: String, port: Int) -> Int

@extern
fn abi_http_server_route_actor(server_id: Int, method: String, path: String, actor_id: Int, message_kind: String) -> Int

@extern
fn abi_http_server_pump(server_id: Int, timeout_ms: Int) -> Int

@extern
fn abi_http_server_pending_request_count(server_id: Int) -> Int

@extern
fn abi_http_respond_text(incoming_request_id: Int, status_code: Int, payload: String) -> Int

fn main() -> Int:
    let _platform = abi_net_platform_name()
    let _capability = abi_net_capability_state("http2.client")
    let tcp = abi_tcp_connect("127.0.0.1", 8080, 100)
    let _write = abi_tcp_write_text(tcp, "ping")
    let server = abi_http_server_create("127.0.0.1", 0)
    let _route = abi_http_server_route_actor(server, "GET", "/actor", 7, "HttpRequest")
    let incoming = abi_http_server_pump(server, 1)
    let _pending = abi_http_server_pending_request_count(server)
    let _respond = abi_http_respond_text(incoming, 200, "ok")
    let request = abi_http_request_create("GET", "http://127.0.0.1/")
    let _protocol = abi_http_request_set_protocol(request, "http/2")
    let response = abi_http_client_send(request)
    let _response_protocol = abi_http_response_protocol(response)
    let body = abi_http_response_body_text(response)
    if body != "":
        return response
    return tcp
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(llvm.contains("declare i8* @abi_net_platform_name()"));
    assert!(llvm.contains("declare i64 @abi_net_capability_state(i8* %arg0)"));
    assert!(llvm.contains("declare i64 @abi_tcp_connect(i8* %arg0, i64 %arg1, i64 %arg2)"));
    assert!(llvm.contains("declare i64 @abi_http_request_create(i8* %arg0, i8* %arg1)"));
    assert!(llvm.contains("declare i64 @abi_http_request_set_protocol(i64 %arg0, i8* %arg1)"));
    assert!(llvm.contains("declare i64 @abi_http_client_send(i64 %arg0)"));
    assert!(llvm.contains("declare i8* @abi_http_response_protocol(i64 %arg0)"));
    assert!(llvm.contains("declare i64 @abi_http_server_route_actor(i64 %arg0, i8* %arg1, i8* %arg2, i64 %arg3, i8* %arg4)"));
    assert!(llvm.contains("declare i64 @abi_http_server_pending_request_count(i64 %arg0)"));
    assert!(llvm.contains("call i64 @abi_http_respond_text"));
    assert!(llvm.contains("HttpRequest"));
    assert!(llvm.contains("http2.client"));
}

#[test]
fn llvm_lowers_single_file_native_ui_primitives_without_component_catalog() {
    let source = r#"
@extern
fn abi_ui_session_create(app_name: String, width: Int, height: Int) -> Int

@extern
fn abi_ui_node_create(session_id: Int, kind: String) -> Int

@extern
fn abi_ui_node_set_rect(session_id: Int, node_id: Int, x: Float, y: Float, width: Float, height: Float) -> Int

@extern
fn abi_ui_draw_rect(session_id: Int, node_id: Int, x: Float, y: Float, width: Float, height: Float, style_key: String) -> Int

@extern
fn abi_ui_present(session_id: Int) -> Int

pub fn native_ui_session_create(app_name: String, width: Int, height: Int) -> Int:
    return abi_ui_session_create(app_name, width, height)

pub fn native_ui_node_create(session_id: Int, kind: String) -> Int:
    return abi_ui_node_create(session_id, kind)

pub fn native_ui_node_set_rect(session_id: Int, node_id: Int, x: Float, y: Float, width: Float, height: Float) -> Int:
    return abi_ui_node_set_rect(session_id, node_id, x, y, width, height)

pub fn native_ui_draw_rect(session_id: Int, node_id: Int, x: Float, y: Float, width: Float, height: Float, style_key: String) -> Int:
    return abi_ui_draw_rect(session_id, node_id, x, y, width, height, style_key)

pub fn native_ui_present(session_id: Int) -> Int:
    return abi_ui_present(session_id)

fn main() -> Int:
    let session = native_ui_session_create("single-file-authoring", 960, 540)
    let root = native_ui_node_create(session, "app.workspace.root")
    let command = native_ui_node_create(session, "user.authored.command-strip")
    let _root_rect = native_ui_node_set_rect(session, root, 0.0, 0.0, 960.0, 540.0)
    let _command_rect = native_ui_node_set_rect(session, command, 16.0, 16.0, 240.0, 44.0)
    let _draw = native_ui_draw_rect(session, command, 16.0, 16.0, 240.0, 44.0, "accent-fill")
    return native_ui_present(session)
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(llvm.contains("declare i64 @abi_ui_session_create(i8* %arg0, i64 %arg1, i64 %arg2)"));
    assert!(llvm.contains("declare i64 @abi_ui_node_create(i64 %arg0, i8* %arg1)"));
    assert!(llvm.contains("declare i64 @abi_ui_node_set_rect(i64 %arg0, i64 %arg1, double %arg2, double %arg3, double %arg4, double %arg5)"));
    assert!(llvm.contains("call i64 @abi_ui_draw_rect"));
    assert!(llvm.contains("user.authored.command-strip"));
    assert!(!llvm.contains("button"));
    assert!(!llvm.contains("panel"));
}

#[test]
fn llvm_coerces_numeric_call_arguments_to_declared_param_types() {
    let source = r#"
fn takes_float(x: Float) -> Float:
    return x

fn main() -> Float:
    return takes_float(7)
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(llvm.contains("define double @takes_float(double %arg0)"));
    assert!(llvm.contains("sitofp i64 7 to double"));
    assert!(!llvm.contains("@Float("));
}

#[test]
fn llvm_lowers_float_constructor_calls_as_numeric_casts() {
    let source = r#"
fn main() -> Float:
    return Float(7)
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(llvm.contains("sitofp i64 7 to double"));
    assert!(!llvm.contains("@Float("));
}

#[test]
fn llvm_lowers_native_ui_host_services_without_component_catalog() {
    let source = r#"
@extern
fn abi_ui_session_create(app_name: String, width: Int, height: Int) -> Int

@extern
fn abi_ui_node_create(session_id: Int, kind: String) -> Int

@extern
fn abi_ui_node_set_rect(session_id: Int, node_id: Int, x: Float, y: Float, width: Float, height: Float) -> Int

@extern
fn abi_ui_node_set_stable_key(session_id: Int, node_id: Int, stable_key: String) -> Int

@extern
fn abi_ui_node_find_by_stable_key(session_id: Int, stable_key: String) -> Int

@extern
fn abi_ui_node_set_state_i64(session_id: Int, node_id: Int, key: String, value: Int) -> Int

@extern
fn abi_ui_node_state_i64(session_id: Int, node_id: Int, key: String, fallback: Int) -> Int

@extern
fn abi_ui_node_set_state_string(session_id: Int, node_id: Int, key: String, value: String) -> Int

@extern
fn abi_ui_node_state_string(session_id: Int, node_id: Int, key: String, fallback: String) -> String

@extern
fn abi_ui_state_count(session_id: Int) -> Int

@extern
fn abi_ui_host_attach(session_id: Int, backend_id: String) -> Int

@extern
fn abi_ui_host_present(session_id: Int) -> Int

@extern
fn abi_ui_hot_reload_begin(session_id: Int, revision_key: String) -> Int

@extern
fn abi_ui_font_create(session_id: Int, key: String, family: String, size: Float) -> Int

@extern
fn abi_ui_texture_create(session_id: Int, key: String, width: Int, height: Int, format: String, byte_length: Int) -> Int

@extern
fn abi_ui_text_measure_width(session_id: Int, font_resource_id: Int, text: String) -> Float

@extern
fn abi_ui_draw_resource(session_id: Int, node_id: Int, resource_id: Int, x: Float, y: Float, width: Float, height: Float, style_key: String) -> Int

@extern
fn abi_ui_resource_set_bytes_hex(session_id: Int, resource_id: Int, bytes_hex: String) -> Int

@extern
fn abi_ui_clipboard_set_text(session_id: Int, text: String) -> Int

@extern
fn abi_ui_draw_text(session_id: Int, node_id: Int, font_resource_id: Int, x: Float, y: Float, text: String, style_key: String) -> Int

fn main() -> Int:
    let session = abi_ui_session_create("single-file-host-authoring", 960, 540)
    let _backend = abi_ui_host_attach(session, "software")
    let _reload = abi_ui_hot_reload_begin(session, "rev-a")
    let root = abi_ui_node_create(session, "app.root")
    let command = abi_ui_node_create(session, "author.command")
    let _root_key = abi_ui_node_set_stable_key(session, root, "root")
    let _command_key = abi_ui_node_set_stable_key(session, command, "command.launch")
    let _command_rect = abi_ui_node_set_rect(session, command, 16.0, 16.0, 180.0, 36.0)
    let font = abi_ui_font_create(session, "font.body", "Inter", 14.0)
    let width = abi_ui_text_measure_width(session, font, "Launch")
    let texture = abi_ui_texture_create(session, "texture.icon", 32, 32, "rgba8", 4096)
    let _upload = abi_ui_resource_set_bytes_hex(session, texture, "FF8F3FFF7DC9FFFF1F242EFFEEF2F8FF")
    let _shape = abi_ui_node_set_state_string(session, command, "shape.kind", "tetra.surface")
    let _resource_state = abi_ui_node_set_state_i64(session, command, "resource.id", texture)
    let _draw = abi_ui_draw_resource(session, command, texture, 164.0, 18.0, 32.0, 32.0, "icon")
    let _text = abi_ui_draw_text(session, command, font, 28.0, 38.0, "Launch", "label")
    let _clipboard = abi_ui_clipboard_set_text(session, "Launch")
    if abi_ui_node_find_by_stable_key(session, "command.launch") == command and width > 10.0 and abi_ui_node_state_string(session, command, "shape.kind", "") == "tetra.surface" and abi_ui_node_state_i64(session, command, "resource.id", 0) == texture and abi_ui_state_count(session) == 2:
        return abi_ui_host_present(session)
    return 0
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(llvm.contains("declare i64 @abi_ui_host_attach(i64 %arg0, i8* %arg1)"));
    assert!(llvm.contains("declare i64 @abi_ui_font_create"));
    assert!(llvm.contains("declare double @abi_ui_text_measure_width"));
    assert!(llvm.contains("declare i64 @abi_ui_resource_set_bytes_hex"));
    assert!(llvm.contains("declare i64 @abi_ui_draw_text"));
    assert!(llvm.contains("declare i64 @abi_ui_node_set_state_string"));
    assert!(llvm.contains("declare i64 @abi_ui_node_set_state_i64"));
    assert!(llvm.contains("declare i8* @abi_ui_node_state_string"));
    assert!(llvm.contains("declare i64 @abi_ui_state_count"));
    assert!(llvm.contains("call i64 @abi_ui_draw_resource"));
    assert!(llvm.contains("call i64 @abi_ui_draw_text"));
    assert!(llvm.contains("call i64 @abi_ui_node_set_state_string"));
    assert!(llvm.contains("call i64 @abi_ui_node_state_i64"));
    assert!(llvm.contains("call i64 @abi_ui_host_present"));
    assert!(llvm.contains("tetra.surface"));
    assert!(llvm.contains("command.launch"));
    assert!(!llvm.contains("button"));
    assert!(!llvm.contains("panel"));
}

#[test]
fn llvm_lowers_native_graphics_engine_primitives_without_scene_catalog() {
    let source = r#"
@extern
fn abi_graphics_session_create(app_name: String, width: Int, height: Int) -> Int

@extern
fn abi_graphics_backend_select(session_id: Int, backend_id: String) -> Int

@extern
fn abi_graphics_shader_spirv_from_hex(session_id: Int, key: String, stage: String, entry_point: String, bytes_hex: String) -> Int

@extern
fn abi_graphics_buffer_create_from_hex(session_id: Int, kind: String, label: String, bytes_hex: String, element_stride: Int) -> Int

@extern
fn abi_graphics_mesh_create(session_id: Int, label: String, vertex_buffer_id: Int, index_buffer_id: Int, vertex_count: Int, index_count: Int) -> Int

@extern
fn abi_graphics_pipeline_create(session_id: Int, label: String, vertex_shader_id: Int, fragment_shader_id: Int, backend_id: String) -> Int

@extern
fn abi_graphics_begin_frame(session_id: Int, delta_ms: Float) -> Int

@extern
fn abi_graphics_draw_mesh(session_id: Int, pipeline_id: Int, mesh_id: Int, instance_count: Int) -> Int

@extern
fn abi_graphics_present(session_id: Int) -> Int

pub fn native_graphics_session_create(app_name: String, width: Int, height: Int) -> Int:
    return abi_graphics_session_create(app_name, width, height)

pub fn native_graphics_backend_select(session_id: Int, backend_id: String) -> Int:
    return abi_graphics_backend_select(session_id, backend_id)

pub fn native_graphics_shader_spirv_from_hex(session_id: Int, key: String, stage: String, entry_point: String, bytes_hex: String) -> Int:
    return abi_graphics_shader_spirv_from_hex(session_id, key, stage, entry_point, bytes_hex)

pub fn native_graphics_buffer_create_from_hex(session_id: Int, kind: String, label: String, bytes_hex: String, element_stride: Int) -> Int:
    return abi_graphics_buffer_create_from_hex(session_id, kind, label, bytes_hex, element_stride)

pub fn native_graphics_mesh_create(session_id: Int, label: String, vertex_buffer_id: Int, index_buffer_id: Int, vertex_count: Int, index_count: Int) -> Int:
    return abi_graphics_mesh_create(session_id, label, vertex_buffer_id, index_buffer_id, vertex_count, index_count)

pub fn native_graphics_pipeline_create(session_id: Int, label: String, vertex_shader_id: Int, fragment_shader_id: Int, backend_id: String) -> Int:
    return abi_graphics_pipeline_create(session_id, label, vertex_shader_id, fragment_shader_id, backend_id)

pub fn native_graphics_begin_frame(session_id: Int, delta_ms: Float) -> Int:
    return abi_graphics_begin_frame(session_id, delta_ms)

pub fn native_graphics_draw_mesh(session_id: Int, pipeline_id: Int, mesh_id: Int, instance_count: Int) -> Int:
    return abi_graphics_draw_mesh(session_id, pipeline_id, mesh_id, instance_count)

pub fn native_graphics_present(session_id: Int) -> Int:
    return abi_graphics_present(session_id)

fn main() -> Int:
    let session = native_graphics_session_create("kain-authored-engine", 1280, 720)
    let _backend = native_graphics_backend_select(session, "vulkan")
    let vertex_shader = native_graphics_shader_spirv_from_hex(session, "engine.vertex", "vertex", "main", "03022307")
    let fragment_shader = native_graphics_shader_spirv_from_hex(session, "engine.fragment", "fragment", "main", "03022307")
    let vertex_buffer = native_graphics_buffer_create_from_hex(session, "vertex", "author.mesh.vertices", "000000000100000002000000", 12)
    let index_buffer = native_graphics_buffer_create_from_hex(session, "index", "author.mesh.indices", "000000000100000002000000", 4)
    let mesh = native_graphics_mesh_create(session, "author.mesh.triangle", vertex_buffer, index_buffer, 3, 3)
    let pipeline = native_graphics_pipeline_create(session, "author.pipeline", vertex_shader, fragment_shader, "vulkan")
    let _frame = native_graphics_begin_frame(session, 8.33)
    let _draw = native_graphics_draw_mesh(session, pipeline, mesh, 1)
    return native_graphics_present(session)
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm should be utf8");

    assert!(
        llvm.contains("declare i64 @abi_graphics_session_create(i8* %arg0, i64 %arg1, i64 %arg2)")
    );
    assert!(llvm.contains("declare i64 @abi_graphics_shader_spirv_from_hex"));
    assert!(llvm.contains("declare i64 @abi_graphics_buffer_create_from_hex"));
    assert!(llvm.contains("call i64 @abi_graphics_draw_mesh"));
    assert!(llvm.contains("author.mesh.triangle"));
    assert!(llvm.contains("vulkan"));
    assert!(!llvm.contains("geometry_fixture"));
}

#[test]
fn llvm_generates_world_patch_converge_and_orchestrate_paths() {
    let source = r#"
world Studio:
    state counter: Int = 1
    surface native_ui => App

world Mirror:
    state counter: Int = 1
    surface web => App

component App():
    render <text>{"studio"}</text>

entangle Studio.counter <-> Mirror.counter with single_writer

patch set_counter(studio: Studio, value: Int) -> Int:
    studio.counter = value
    return studio.counter

law revision_is_valid(value: Int) -> Bool:
    return value >= 0

converge choose_value(value: Int) -> Int:
    spec reference:
        return value + 1
    fast interpret_lane when target("interpret"):
        return value + 1
    fast llvm_lane when target("llvm"):
        return value + 1
    fast avx2_lane when capability("cpu.x86.avx2"):
        return value + 1
    fast native_lane when capability("native.actor"):
        return value + 1

fn stage_bias(value: Int) -> Int:
    return value + 2

orchestrate pipeline(value: Int) -> Int:
    let staged: Int = kain choose_value(value)
    let echoed: Int = rust stage_bias(staged)
    return echoed

fn main() -> Int:
    let studio = Studio
    let updated = set_counter(studio, 7)
    if revision_is_valid(updated):
        return pipeline(updated)
    return 0
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("%Studio = type { i64 }"));
    assert!(llvm.contains("%Mirror = type { i64 }"));
    assert!(llvm.contains("@__kain_world_Studio = internal global %Studio zeroinitializer"));
    assert!(llvm.contains("@__kain_world_Mirror = internal global %Mirror zeroinitializer"));
    assert!(llvm.contains("@__kain_world_init_flag_Studio = internal global i1 0"));
    assert!(llvm.contains("define void @__kain_init_world_Studio()"));
    assert!(llvm.contains("define i64 @set_counter(%Studio* %arg0, i64 %arg1)"));
    assert!(llvm.contains("define i1 @revision_is_valid(i64 %arg0)"));
    assert!(llvm.contains("define i64 @choose_value(i64 %arg0)"));
    assert!(llvm.contains("define i64 @pipeline(i64 %arg0)"));
    assert!(llvm.contains("declare i64 @abi_entangle_register(i8*, i8*, i8*, i8*)"));
    assert!(llvm.contains("define void @__kain_register_entanglements()"));
    assert!(llvm.contains("call i64 @abi_entangle_register"));
    assert!(llvm.contains("call void @__kain_register_entanglements()"));
    assert!(llvm.contains("call void @__kain_init_world_Studio()"));
    assert!(llvm.contains("call i1 @revision_is_valid(i64"));
    assert!(llvm.contains("call i64 @choose_value(i64"));
    assert!(llvm.contains("call i64 @stage_bias(i64"));
    assert!(llvm.contains("call i64 @pipeline(i64"));
    assert!(llvm.contains("call i64 @abi_patch_begin"));
    assert!(llvm.contains("call i64 @abi_patch_record_i64"));
    assert!(llvm.contains("call i64 @abi_patch_commit"));
    assert!(llvm.contains("call i64 @abi_entangle_record_i64"));
    assert!(llvm.contains("define i64 @choose_value__spec"));
    assert!(llvm.contains("define i64 @choose_value__fast_interpret_lane"));
    assert!(llvm.contains("define i64 @choose_value__fast_llvm_lane"));
    assert!(llvm.contains("define i64 @choose_value__fast_avx2_lane"));
    assert!(llvm.contains("define i64 @choose_value__fast_native_lane"));
    assert!(llvm.contains("call i64 @abi_cpu_capability_mask_for_key"));
    assert!(llvm.contains("call i64 @abi_cpu_feature_mask()"));
    assert!(llvm.contains("call i64 @abi_converge_select_lane_for_key"));
    assert!(llvm.contains("switch i64"));
    assert!(llvm.contains("call i64 @abi_orchestrate_stage_begin"));
    assert!(llvm.contains("call i64 @abi_orchestrate_stage_end_i64"));
    assert!(llvm.contains("Studio.counter"));
    assert!(llvm.contains("Mirror.counter"));
    assert!(llvm.contains("single_writer"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "multi-lane-converge-selector");
}

#[test]
fn llvm_lowers_option_result_future_to_native_tagged_runtime() {
    let source = r#"
fn maybe(flag: Bool) -> Option<Int>:
    if flag:
        return Some(7)
    return None

fn parse(flag: Bool) -> Result<Int, String>:
    if flag:
        return Result::Ok(9)
    return Result::Err("bad")

fn ready() -> impl Future<Int>:
    return async 11

fn use_result() -> Result<Int, String>:
    let parsed: Int = parse(true)?
    return Result::Ok(parsed)

fn main() -> Int:
    let fallback: Int = maybe(false).unwrap_or(3)
    let parsed: Int = use_result().unwrap()
    let awaited: Int = await ready()
    if maybe(true).is_some() and parse(false).is_err():
        return fallback + parsed + awaited
    return 1
"#;

    let program = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define internal i8* @maybe(i1 %arg0)"));
    assert!(llvm.contains("define internal i8* @parse(i1 %arg0)"));
    assert!(llvm.contains("call i8* @KAIN_alloc(i64 24)"));
    assert!(llvm.contains("inttoptr i64"));
    assert!(llvm.contains("ptrtoint i8*"));
    assert!(!llvm.contains("call i8* @abi_option_none()"));
    assert!(!llvm.contains("call i8* @abi_option_some"));
    assert!(!llvm.contains("call i8* @abi_result_ok"));
    assert!(!llvm.contains("call i8* @abi_result_err"));
    assert!(!llvm.contains("call i64 @abi_tagged_is_success"));
    assert!(!llvm.contains("call i64 @abi_tagged_payload_copy"));
    assert!(!llvm.contains("call i64 @abi_option_is_some"));
    assert!(!llvm.contains("call i64 @abi_result_is_err"));
    assert!(llvm.contains("call i8* @abi_future_ready_from_value"));

    let maybe_start = llvm
        .find("define internal i8* @maybe(i1 %arg0)")
        .expect("maybe function should be emitted");
    let maybe_rest = &llvm[maybe_start..];
    let maybe_end = maybe_rest
        .find("\n}\n")
        .expect("maybe function should terminate");
    let maybe_ir = &maybe_rest[..maybe_end];
    assert!(
        !maybe_ir.contains("call void @rc_retain(i8*"),
        "fresh tagged return should not retain in maybe():\n{}",
        maybe_ir
    );
    assert!(
        !maybe_ir.contains("call void @rc_retain(i8* null)"),
        "None should not retain the null sentinel in maybe():\n{}",
        maybe_ir
    );

    let main_start = llvm
        .find("define i64 @main()")
        .expect("main function should be emitted");
    let main_rest = &llvm[main_start..];
    let main_end = main_rest
        .find("\n}\n")
        .expect("main function should terminate");
    let main_ir = &main_rest[..main_end];
    assert!(
        !main_ir.contains("call i8* @ready()"),
        "await on immediate ready future should inline the payload instead of calling ready():\n{}",
        main_ir
    );
    assert!(
        !main_ir.contains("call i64 @abi_future_await_payload_copy"),
        "await on immediate ready future should not hit the runtime await ABI in main():\n{}",
        main_ir
    );
    assert!(
        main_ir.contains("ptrtoint i8*"),
        "tagged cleanup should guard heap-only RC operations in main():\n{}",
        main_ir
    );
}

#[test]
fn llvm_generates_component_and_jsx_calls() {
    let hud_panel = TypedItem::Component(TypedComponent {
        ast: Component {
            name: "HudPanel".to_string(),
            props: vec![Param {
                name: "title".to_string(),
                ty: string_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            state: vec![],
            methods: vec![],
            effects: vec![],
            body: JSXNode::Element {
                tag: "div".to_string(),
                attributes: vec![JSXAttribute {
                    name: "class".to_string(),
                    value: JSXAttrValue::String("hud".to_string()),
                    span: span(),
                }],
                children: vec![
                    JSXNode::Text("Title: ".to_string(), span()),
                    JSXNode::Expression(Box::new(Expr::Ident("title".to_string(), span()))),
                    JSXNode::Text(" ".to_string(), span()),
                    JSXNode::Expression(Box::new(Expr::Ident("children".to_string(), span()))),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        prop_types: HashMap::from([("title".to_string(), ResolvedType::String)]),
    });

    let app_shell = TypedItem::Component(TypedComponent {
        ast: Component {
            name: "AppShell".to_string(),
            props: vec![],
            state: vec![],
            methods: vec![],
            effects: vec![],
            body: JSXNode::ComponentCall {
                name: "HudPanel".to_string(),
                props: vec![JSXAttribute {
                    name: "title".to_string(),
                    value: JSXAttrValue::String("Status".to_string()),
                    span: span(),
                }],
                children: vec![JSXNode::Text("Inner body".to_string(), span())],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        prop_types: HashMap::new(),
    });

    let program = TypedProgram {
        items: vec![hud_panel, app_shell],
    };

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define i8* @HudPanel(i8* %arg0, i8* %arg1)"));
    assert!(llvm.contains("define i8* @AppShell(i8* %arg0)"));
    assert!(llvm.contains("call i8* @HudPanel(i8*"));
    assert!(llvm.contains("<div"));
    assert!(llvm.contains("hud"));
    assert!(llvm.contains("Inner body"));
}

#[test]
fn llvm_generates_struct_array_and_fstring_paths() {
    let view_model = TypedItem::Struct(TypedStruct {
        ast: Struct {
            name: "ViewModel".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "value".to_string(),
                ty: int_type(),
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: span(),
            }],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: span(),
        },
        field_types: HashMap::from([("value".to_string(), ResolvedType::Int(IntSize::I64))]),
    });

    let make_view = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "make_view".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "n".to_string(),
                ty: int_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(string_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "model".to_string(),
                            mutable: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Struct {
                            name: "ViewModel".to_string(),
                            fields: vec![(
                                "value".to_string(),
                                Expr::Ident("n".to_string(), span()),
                            )],
                            rest: None,
                            span: span(),
                        }),
                        span: span(),
                    },
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "items".to_string(),
                            mutable: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Array(
                            vec![
                                Expr::Int(1, span()),
                                Expr::Int(2, span()),
                                Expr::Int(3, span()),
                            ],
                            span(),
                        )),
                        span: span(),
                    },
                    Stmt::Expr(Expr::Assign {
                        target: Box::new(Expr::Field {
                            object: Box::new(Expr::Ident("model".to_string(), span())),
                            field: "value".to_string(),
                            span: span(),
                        }),
                        value: Box::new(Expr::Binary {
                            left: Box::new(Expr::Field {
                                object: Box::new(Expr::Ident("model".to_string(), span())),
                                field: "value".to_string(),
                                span: span(),
                            }),
                            op: BinaryOp::Add,
                            right: Box::new(Expr::Index {
                                object: Box::new(Expr::Ident("items".to_string(), span())),
                                index: Box::new(Expr::Int(0, span())),
                                span: span(),
                            }),
                            span: span(),
                        }),
                        span: span(),
                    }),
                    Stmt::Return(
                        Some(Expr::FString(
                            vec![
                                Expr::String("value=".to_string(), span()),
                                Expr::Field {
                                    object: Box::new(Expr::Ident("model".to_string(), span())),
                                    field: "value".to_string(),
                                    span: span(),
                                },
                            ],
                            span(),
                        )),
                        span(),
                    ),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Int(IntSize::I64)],
            ret: Box::new(ResolvedType::String),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let program = TypedProgram {
        items: vec![view_model, make_view],
    };

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define i8* @make_view(i64 %arg0)"));
    assert!(llvm.contains("call i8* @KAIN_alloc(i64"));
    assert!(llvm.contains("call i8* @array_new(i64 4)"));
    assert!(llvm.contains("call i64 @array_get(i8*"));
    assert!(llvm.contains("call i8* @to_string(i64"));
    assert!(llvm.contains("call i8* @str_concat(i8*"));
}

#[test]
fn llvm_generates_impl_methods_and_method_calls() {
    let view_model = TypedItem::Struct(TypedStruct {
        ast: Struct {
            name: "ViewModel".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "value".to_string(),
                ty: int_type(),
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: span(),
            }],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: span(),
        },
        field_types: HashMap::from([("value".to_string(), ResolvedType::Int(IntSize::I64))]),
    });

    let typed_impl = TypedItem::Impl(TypedImpl {
        ast: Impl {
            generics: vec![],
            trait_name: None,
            trait_generics: vec![],
            target_type: Type::Named {
                name: "ViewModel".to_string(),
                generics: vec![],
                span: span(),
            },
            methods: vec![Function {
                name: "increment".to_string(),
                generics: vec![],
                params: vec![Param {
                    name: "delta".to_string(),
                    ty: int_type(),
                    mutable: false,
                    default: None,
                    span: span(),
                }],
                return_type: Some(int_type()),
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::Return(
                        Some(Expr::Binary {
                            left: Box::new(Expr::Field {
                                object: Box::new(Expr::Ident("self".to_string(), span())),
                                field: "value".to_string(),
                                span: span(),
                            }),
                            op: BinaryOp::Add,
                            right: Box::new(Expr::Ident("delta".to_string(), span())),
                            span: span(),
                        }),
                        span(),
                    )],
                    span: span(),
                },
                visibility: Visibility::Public,
                attributes: vec![],
                span: span(),
            }],
            span: span(),
        },
    });

    let call_method = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "call_increment".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "model".to_string(),
                ty: Type::Named {
                    name: "ViewModel".to_string(),
                    generics: vec![],
                    span: span(),
                },
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::MethodCall {
                        receiver: Box::new(Expr::Ident("model".to_string(), span())),
                        method: "increment".to_string(),
                        args: vec![kain_core::ast::CallArg {
                            name: None,
                            value: Expr::Int(5, span()),
                            span: span(),
                        }],
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Struct(
                "ViewModel".to_string(),
                HashMap::new(),
            )],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let program = TypedProgram {
        items: vec![view_model, typed_impl, call_method],
    };

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define i64 @ViewModel_increment(%ViewModel* %arg0, i64 %arg1)"));
    assert!(llvm.contains("call i64 @ViewModel_increment(%ViewModel*"));
}

#[test]
fn llvm_consumes_lowered_memory_helpers_into_pointer_ir() {
    let typed = typed_program_from_source(
        "fn mutate() -> Int:\n    let mut x: Int = 1\n    let p: ptr<Int> = addr_of(x, \"Int\")\n    mem_store(p, 7, \"Int\")\n    return x\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("%x.addr_"));
    assert!(!llvm.contains("call void @__kain_mem_store(i8* "));
    assert!(!llvm.contains("call void @__kain_mem_load(i8* "));
    assert!(llvm.contains("inttoptr i64"));
    assert!(llvm.contains("store i64 7, i64*"));
    assert!(llvm.contains("align 1"));
}

#[test]
fn llvm_sizes_runtime_memory_helpers_for_bool_values() {
    let typed = typed_program_from_source(
        "fn flip(p: ptr<Bool>) -> Bool:\n    mem_store(p, true, \"Bool\")\n    return mem_load(p, \"Bool\")\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(!llvm.contains("call void @__kain_mem_store(i8* "));
    assert!(!llvm.contains("call void @__kain_mem_load(i8* "));
    assert!(llvm.contains("store i1 1, i1*"));
    assert!(llvm.contains("load i1, i1*"));
    assert!(llvm.contains("align 1"));
}

#[test]
fn llvm_consumes_lowered_alloc_and_realloc_helpers() {
    let typed = typed_program_from_source(
        "fn heap(n: Int, p: ptr<Int>) -> Int:\n    let mut q: ptr<Int> = alloc_zeroed((n * sizeof_type(\"Int\")), \"Int\")\n    let mut r: ptr<Int> = realloc_mem(p, (n * sizeof_type(\"Int\")), \"Int\", true)\n    return 0\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("call i8* @__kain_alloc(i64"));
    assert!(llvm.contains(", i64 8, i32 1)"));
    assert!(llvm.contains("call i8* @__kain_realloc(i8*"));
    assert!(llvm.contains(", i64 8, i32 1)"));
    assert!(llvm.contains("inttoptr i64"));
    assert!(llvm.contains("ptrtoint i8*"));
}

#[test]
fn llvm_lowers_ownership_keywords_to_runtime_guards() {
    let typed = typed_program_from_source(
        "fn own(p: ptr<Int>) -> Int:\n    let read = observe p:\n        mem_load(p, \"Int\")\n    collapse p:\n        mem_store(p, read + 1, \"Int\")\n        0\n    decay p\n    return read\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("declare i32 @__kain_ownership_register_imported(i8*, i64)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_ensure_imported(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_begin_observe(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_end_observe(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_begin_collapse(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_end_collapse(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_decay(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_begin_observe_helper(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_end_observe_helper(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_begin_collapse_helper(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_end_collapse_helper(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_decay_helper(i8*)"));
    assert!(llvm.contains("call i32 @__kain_ownership_ensure_imported(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_begin_observe(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_end_observe(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_begin_collapse(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_end_collapse(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_decay(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_observe_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_collapse_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_decay_helper(i8*"));
    assert!(llvm.contains("call void @abort()"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "ownership-keywords");
}

#[test]
fn llvm_lowers_machine_stones_to_native_runtime_abi() {
    let typed = typed_program_from_source(
        r#"
axiom native_atomic_mask_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("atomic.bitmask")
    guarantee "atomic mask lane exists"
    fallback portable_mask_update

component MachinePanel():
    render <panel title="Machine" />

world NativeWorld:
    state beat: Int = 0
    surface native_ui => MachinePanel

world GpuWorld:
    state beat: Int = 0
    surface viewport3d => "gpu"

shatter struct AgentParticle:
    x: Float
    y: Float
    alive: Bool

fn portable_mask_update(value: Int, mask: Int) -> Int:
    return value | mask

pulse agent_sinus every 16ms jitter 1ms:
    let particle = AgentParticle { x: 1.0, y: 2.0, alive: true }
    let gpu_particle = teleport particle from NativeWorld to GpuWorld via gpu_upload
    let _alive = gpu_particle.alive
    let _tick = pulse_tick + pulse_dt_ms + pulse_missed

fn main() -> Int:
    let particles = [
        AgentParticle { x: 1.0, y: 2.0, alive: true },
        AgentParticle { x: 3.0, y: 5.0, alive: false }
    ]
    let hot_x = particles[1].x
    var live_count = 0
    for i in range(0, 2):
        if particles[i].alive:
            live_count = live_count + 1
    if hot_x != 3.0:
        return 1
    if live_count != 1:
        return 2
    return portable_mask_update(1, 2) - 3
"#,
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("declare i64 @kain_machine_axiom_accept(i8*, i8*, i64)"));
    assert!(llvm.contains("declare void @kain_machine_pulse_snapshot"));
    assert!(llvm.contains("declare i64 @kain_machine_pulse_start"));
    assert!(llvm.contains("declare i64 @kain_machine_pulse_total_fire_count"));
    assert!(llvm.contains("declare i8* @kain_machine_teleport_ptr"));
    assert!(llvm.contains("declare i8* @kain_machine_shatter_alloc"));
    assert!(llvm.contains("declare i8* @kain_machine_shatter_lane_base"));
    assert!(llvm.contains("define i64 @__kain_axiom_accept_native_atomic_mask_truth()"));
    assert!(llvm.contains("define void @__kain_pulse_body_agent_sinus"));
    assert!(llvm.contains("define void @__kain_pulse_fire_agent_sinus()"));
    assert!(llvm.contains("call i64 @kain_machine_pulse_start(i64"));
    assert!(llvm.contains("void ()* @__kain_pulse_fire_agent_sinus"));
    assert!(llvm.contains("call i8* @kain_machine_shatter_alloc(i64 3, i64 2)"));
    assert!(llvm.contains("call i8* @kain_machine_shatter_lane_base"));
    assert!(llvm.contains("shl i64"));
    assert!(!llvm.contains("call i8* @kain_machine_shatter_lane_ptr"));
    assert!(llvm.contains("call i8* @kain_machine_teleport_ptr"));
    assert!(!llvm.contains("call i8* @array_new(i64 4)"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "machine-stones-native-runtime");
}

#[test]
fn llvm_cleans_shattered_array_locals_on_each_return_path() {
    let typed = typed_program_from_source(
        r#"
shatter struct PairShard:
    x: Int
    y: Int
    alive: Bool

fn main() -> Int:
    let pairs = [
        PairShard { x: 1, y: 2, alive: true },
        PairShard { x: 3, y: 4, alive: false }
    ]
    if pairs[0].x == 1:
        return 0
    if pairs[1].y == 4:
        return 1
    return 2
"#,
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    let shatter_free_count = llvm.matches("call void @kain_machine_shatter_free").count();
    assert!(
        shatter_free_count >= 3,
        "each return path should free the shatter handle, saw {shatter_free_count}\n{llvm}"
    );
    assert!(
        !llvm.contains("call void @rc_release(i8*"),
        "shatter handles are not RC allocations and must not be released with rc_release\n{llvm}"
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "machine-stones-shatter-return-cleanup");
}

#[test]
fn llvm_routes_helper_owned_ownership_keywords_to_helper_fast_path() {
    let typed = typed_program_from_source(
        "fn own_local_pair(count: Int) -> Int:\n    let mut cell: ptr<Int> = alloc_zeroed(count, \"Int\")\n    collapse cell:\n        mem_store(cell, 7, \"Int\")\n        0\n    let read = observe cell:\n        mem_load(cell, \"Int\")\n    decay cell\n    return read\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("declare i32 @__kain_ownership_begin_observe_helper(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_end_observe_helper(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_begin_collapse_helper(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_end_collapse_helper(i8*)"));
    assert!(llvm.contains("declare i32 @__kain_ownership_decay_helper(i8*)"));
    assert!(llvm.contains("call i32 @__kain_ownership_begin_observe_helper(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_end_observe_helper(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_begin_collapse_helper(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_end_collapse_helper(i8*"));
    assert!(llvm.contains("call i32 @__kain_ownership_decay_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_ensure_imported(i8*"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "ownership-helper-fast-path");
}

#[test]
fn llvm_erases_ephemeral_single_cell_ownership_to_local_storage() {
    let typed = typed_program_from_source(
        "fn own_local() -> Int:\n    let mut cell: ptr<Int> = alloc_zeroed(1, \"Int\")\n    collapse cell:\n        mem_store(cell, 7, \"Int\")\n        0\n    let read = observe cell:\n        mem_load(cell, \"Int\")\n    decay cell\n    return read\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(!llvm.contains("call i8* @__kain_alloc(i64"));
    assert!(!llvm.contains("call i32 @__kain_ownership_ensure_imported(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_observe(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_end_observe(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_collapse(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_end_collapse(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_observe_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_end_observe_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_collapse_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_end_collapse_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_decay(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_decay_helper(i8*"));
    assert!(!llvm.contains("store [8 x i8] zeroinitializer"));
    assert!(llvm.contains("alloca i64"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "ephemeral-ownership-erasure");
}

#[test]
fn llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage() {
    let typed = typed_program_from_source(
        "fn own_loop_local() -> Int:\n    let cell_count: Int = 1\n    var acc: Int = 0\n    var i: Int = 0\n    while i < 4:\n        let mut cell: ptr<Int> = alloc_zeroed(cell_count, \"Int\")\n        collapse cell:\n            mem_store(cell, i + 7, \"Int\")\n            0\n        let read = observe cell:\n            mem_load(cell, \"Int\")\n        decay cell\n        acc = acc + read\n        i = i + 1\n    return acc\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(!llvm.contains("call i8* @__kain_alloc(i64"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_observe_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_collapse_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_decay_helper(i8*"));
    assert!(!llvm.contains("store [8 x i8] zeroinitializer"));
    assert!(llvm.contains("alloca i64"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "loop-ephemeral-ownership-erasure");
}

#[test]
fn llvm_erases_bounded_ephemeral_ptr_offset_buffer_to_local_storage() {
    let typed = typed_program_from_source(
        "fn own_small_buffer() -> Int:\n    let mut cell: ptr<Int> = alloc_zeroed(4, \"Int\")\n    collapse cell:\n        mem_store(ptr_offset(cell, 3, \"Int\"), 11, \"Int\")\n        0\n    let read = observe cell:\n        mem_load(ptr_offset(cell, 3, \"Int\"), \"Int\")\n    decay cell\n    return read\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(!llvm.contains("call i8* @__kain_alloc(i64"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_observe_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_begin_collapse_helper(i8*"));
    assert!(!llvm.contains("call i32 @__kain_ownership_decay_helper(i8*"));
    assert!(!llvm.contains("call i8* @__kain_ptr_offset"));
    assert!(llvm.contains("alloca [32 x i8]"));
    assert!(llvm.contains("getelementptr i8"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "bounded-ephemeral-ptr-offset-buffer-erasure");
}

#[test]
fn llvm_uses_typed_gep_and_natural_alignment_for_helper_owned_ptr_offset_accesses() {
    let typed = typed_program_from_source(
        "fn helper_buffer_probe() -> Int:\n    let cells: Int = 9000\n    let mut buffer: ptr<Int> = alloc_zeroed(cells, \"Int\")\n    collapse buffer:\n        mem_store(ptr_offset(buffer, 3, \"Int\"), 11, \"Int\")\n        0\n    let read = observe buffer:\n        mem_load(ptr_offset(buffer, 3, \"Int\"), \"Int\")\n    decay buffer\n    return read\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("getelementptr i64, i64*"));
    assert!(llvm.contains("store i64 11, i64*"));
    assert!(llvm.contains("align 8"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "helper-owned-typed-gep-alignment");
}

#[test]
fn llvm_lowers_safe_fixed_array_literal_to_stack_gep() {
    let typed = typed_program_from_source(
        "fn scan() -> Int:\n    let values = [1, 2, 3]\n    var acc: Int = 0\n    var index: Int = 0\n    while index < len(values):\n        acc = acc + values[index]\n        index = index + 1\n    return acc\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(!llvm.contains("call i8* @array_new"));
    assert!(!llvm.contains("call void @array_push"));
    assert!(!llvm.contains("call i64 @len"));
    assert!(!llvm.contains("call i64 @array_get"));
    assert!(llvm.contains("alloca [3 x i64]"));
    assert!(llvm.contains("getelementptr inbounds [3 x i64]"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "fixed-array-literal-stack-gep");
}

#[test]
fn llvm_keeps_ephemeral_zero_init_when_first_use_is_read() {
    let typed = typed_program_from_source(
        "fn own_zero_init() -> Int:\n    let mut cell: ptr<Int> = alloc_zeroed(1, \"Int\")\n    collapse cell:\n        let current = mem_load(cell, \"Int\")\n        mem_store(cell, current + 7, \"Int\")\n        0\n    let read = observe cell:\n        mem_load(cell, \"Int\")\n    decay cell\n    return read\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("store i64 0, i64*"));
    assert!(llvm.contains("align 8"));
    assert!(!llvm.contains("store [8 x i8] zeroinitializer"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "ephemeral-zero-init-retained");
}

#[test]
fn llvm_generates_actor_spawn_and_send_message_paths() {
    let program = typed_program_from_source(
        r#"
actor Printer:
    state count: Int = 0

    on Print(value: Int):
        return

fn drive() -> Int:
    let printer = spawn Printer(count = 1)
    send printer.Print(value = 7)
    return 0
"#,
    );

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("%KainActorMessage = type { i64, i8*, i64, i64 }"));
    assert!(llvm.contains(
        "%KainActorSpawnConfig = type { i32 (i64, i8*, i8*)*, i8*, i64, i32, i32, i64, i32, [128 x i8], i32, i32 (i64, i8*, i8*, i32)*, i32, i32, i32 }"
    ));
    assert!(llvm.contains(
        "define i32 @Printer_turn(i64 %actor_id, i8* %mailbox, i8* %user_data, i32 %budget)"
    ));
    assert!(llvm.contains("call void @kain_actor_spawn_config_init(%KainActorSpawnConfig*"));
    assert!(llvm.contains("store i32 1, i32*"));
    assert!(llvm.contains("store i32 2, i32*"));
    assert!(llvm.contains("store i32 (i64, i8*, i8*, i32)* @Printer_turn"));
    assert!(llvm.contains("call i64 @kain_actor_spawn(%KainActorSpawnConfig*"));
    assert!(llvm.contains("call i32 @kain_actor_try_receive(i8* %mailbox, %KainActorMessage*"));
    assert!(llvm.contains("call i32 @kain_actor_send(i64 "));
    assert!(llvm.contains("call void @free(i8* "));
    assert!(llvm.contains("%Printer_Print = type { i64 }"));
    assert!(!llvm.contains("KAIN_spawn"));
    assert!(!llvm.contains("mq_push"));
}

#[test]
fn llvm_generates_actor_ask_reply_roundtrip_paths() {
    let program = typed_program_from_source(
        r#"
actor Echo:
    on Call(reply_to: P, request: Int):
        send reply_to.Reply(value = request + 1)

fn drive() -> Int:
    let echo = spawn Echo()
    return ask_timeout(echo, "Call", 9, 1000)
"#,
    );

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("%KainActorRef = type { i64, i32, i32, i32 }"));
    assert!(llvm.contains("%KainReplyPort = type { %KainActorRef }"));
    assert!(llvm.contains("call i8* @kain_actor_reply_port_new()"));
    assert!(llvm.contains("call void @kain_actor_reply_port_actor_ref(i8*"));
    assert!(llvm
        .contains("declare i32 @kain_actor_ask_send_ref(%KainActorRef*, %KainActorMessage*, i8*)"));
    assert!(llvm.contains("call i32 @kain_actor_ask_send_ref(%KainActorRef* "));
    assert!(llvm.contains("declare i32 @kain_actor_reply_port_send_ref(%KainActorRef*, i8*, i64)"));
    assert!(llvm.contains("call i32 @kain_actor_reply_port_send_ref(%KainActorRef* "));
    assert!(llvm.contains("call i64 @kain_actor_reply_port_wait_i64(i8*"));
    assert!(llvm.contains("call void @kain_actor_reply_port_destroy(i8*"));
    assert!(llvm.contains("extractvalue %KainReplyPort"));
    assert!(llvm.contains("%Echo_Call = type { %KainReplyPort, i64 }"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "actor-ask-reply-roundtrip");
}

#[test]
fn llvm_generates_typed_actor_ask_reply_wait_for_bool_payloads() {
    let program = typed_program_from_source(
        r#"
actor Gate:
    on Probe(reply_to: P, request: Int):
        send reply_to.Reply(value = request == 7)

fn drive() -> Bool:
    let gate = spawn Gate()
    let allowed: Bool = ask_timeout(gate, "Probe", 7, 1000)
    return allowed
"#,
    );

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("declare i32 @kain_actor_reply_port_wait(i8*, i64, i8*, i64, i64*)"));
    assert!(llvm.contains("call i32 @kain_actor_reply_port_wait(i8*"));
    assert!(llvm.contains("store i1"));
    assert!(llvm.contains("%Gate_Probe = type { %KainReplyPort, i64 }"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "actor-ask-reply-bool-roundtrip");
}

#[test]
fn llvm_generates_float_arithmetic_and_comparisons() {
    let blend = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "blend".to_string(),
            generics: vec![],
            params: vec![
                Param {
                    name: "a".to_string(),
                    ty: float_type(),
                    mutable: false,
                    default: None,
                    span: span(),
                },
                Param {
                    name: "b".to_string(),
                    ty: float_type(),
                    mutable: false,
                    default: None,
                    span: span(),
                },
            ],
            return_type: Some(float_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::If {
                        condition: Box::new(Expr::Binary {
                            left: Box::new(Expr::Binary {
                                left: Box::new(Expr::Ident("a".to_string(), span())),
                                op: BinaryOp::Div,
                                right: Box::new(Expr::Ident("b".to_string(), span())),
                                span: span(),
                            }),
                            op: BinaryOp::Gt,
                            right: Box::new(Expr::Float(1.5, span())),
                            span: span(),
                        }),
                        then_branch: Block {
                            stmts: vec![Stmt::Expr(Expr::Binary {
                                left: Box::new(Expr::Ident("a".to_string(), span())),
                                op: BinaryOp::Pow,
                                right: Box::new(Expr::Float(2.0, span())),
                                span: span(),
                            })],
                            span: span(),
                        },
                        else_branch: Some(Box::new(kain_core::ast::ElseBranch::Else(Block {
                            stmts: vec![Stmt::Expr(Expr::Binary {
                                left: Box::new(Expr::Ident("a".to_string(), span())),
                                op: BinaryOp::Mod,
                                right: Box::new(Expr::Ident("b".to_string(), span())),
                                span: span(),
                            })],
                            span: span(),
                        }))),
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![
                ResolvedType::Float(kain_core::types::FloatSize::F64),
                ResolvedType::Float(kain_core::types::FloatSize::F64),
            ],
            ret: Box::new(ResolvedType::Float(kain_core::types::FloatSize::F64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let program = TypedProgram { items: vec![blend] };
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define double @blend(double %arg0, double %arg1)"));
    assert!(llvm.contains("fdiv double"));
    assert!(llvm.contains("fcmp ogt double"));
    assert!(llvm.contains("call double @pow(double"));
    assert!(llvm.contains("frem double"));
}

#[test]
fn llvm_lowers_numeric_builtins_with_float_operands_without_int_abs_calls() {
    let source = r#"
fn probe() -> Float:
    let a = abs(-1.5)
    let b = min(10.0, 20.0)
    let c = max(-2.0, 0.5)
    let d = clamp(2.5, 0.0, 1.0)
    return a + b + c + d
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("@probe("));
    assert!(!llvm.contains("call i64 @abs(double"));
    assert!(!llvm.contains("call i64 @min(double"));
    assert!(!llvm.contains("call i64 @max(double"));
    assert!(!llvm.contains("call i64 @clamp(double"));
    assert!(llvm.contains("fcmp oge double"));
    assert!(llvm.contains("select i1"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "float-numeric-builtins");
}

#[test]
fn llvm_generates_integer_mod_and_bitwise_ops() {
    let source = r#"
fn bit_ops(a: Int, b: Int) -> Int:
    let c = (a & b) | (a ^ b)
    return (c % 7) << 1
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains(" and i64 "));
    assert!(llvm.contains(" xor i64 "));
    assert!(llvm.contains(" or i64 "));
    assert!(llvm.contains(" srem i64 "));
    assert!(llvm.contains(" shl i64 "));
}

#[test]
fn llvm_generates_match_patterns_for_ranges_or_and_literals() {
    let classify_int = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "classify_int".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "value".to_string(),
                ty: int_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::Match {
                        scrutinee: Box::new(Expr::Ident("value".to_string(), span())),
                        arms: vec![
                            MatchArm {
                                pattern: Pattern::Range {
                                    start: Some(Box::new(Expr::Int(1, span()))),
                                    end: Some(Box::new(Expr::Int(3, span()))),
                                    inclusive: true,
                                    span: span(),
                                },
                                guard: None,
                                body: Expr::Int(10, span()),
                                span: span(),
                            },
                            MatchArm {
                                pattern: Pattern::Or(
                                    vec![
                                        Pattern::Literal(Expr::Int(4, span())),
                                        Pattern::Literal(Expr::Int(5, span())),
                                    ],
                                    span(),
                                ),
                                guard: None,
                                body: Expr::Int(20, span()),
                                span: span(),
                            },
                            MatchArm {
                                pattern: Pattern::Wildcard(span()),
                                guard: None,
                                body: Expr::Int(30, span()),
                                span: span(),
                            },
                        ],
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Int(IntSize::I64)],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let classify_flag = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "classify_flag".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "flag".to_string(),
                ty: Type::Named {
                    name: "Bool".to_string(),
                    generics: vec![],
                    span: span(),
                },
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::Match {
                        scrutinee: Box::new(Expr::Ident("flag".to_string(), span())),
                        arms: vec![
                            MatchArm {
                                pattern: Pattern::Literal(Expr::Bool(true, span())),
                                guard: None,
                                body: Expr::Int(1, span()),
                                span: span(),
                            },
                            MatchArm {
                                pattern: Pattern::Literal(Expr::Bool(false, span())),
                                guard: None,
                                body: Expr::Int(0, span()),
                                span: span(),
                            },
                        ],
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Bool],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let classify_name = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "classify_name".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "name".to_string(),
                ty: string_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::Match {
                        scrutinee: Box::new(Expr::Ident("name".to_string(), span())),
                        arms: vec![
                            MatchArm {
                                pattern: Pattern::Literal(Expr::String("hero".to_string(), span())),
                                guard: None,
                                body: Expr::Int(7, span()),
                                span: span(),
                            },
                            MatchArm {
                                pattern: Pattern::Wildcard(span()),
                                guard: None,
                                body: Expr::Int(9, span()),
                                span: span(),
                            },
                        ],
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::String],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![classify_int, classify_flag, classify_name],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("icmp sge i64"));
    assert!(llvm.contains("icmp sle i64"));
    assert!(llvm.contains(" or i1 "));
    assert!(llvm.contains("icmp eq i1"));
    assert!(llvm.contains("call i1 @deep_eq(i8*"));
}

#[test]
fn llvm_generates_tuple_values_and_tuple_destructuring() {
    let source = r#"
fn step() -> (Int, Int, Int):
    return (1, 2, 3)

fn unpack_sum() -> Int:
    let (a, b, c) = step()
    return a + b + c
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("%__kain_tuple_i64_i64_i64 = type { i64, i64, i64 }"));
    assert!(llvm.contains("define %__kain_tuple_i64_i64_i64* @step()"));
    assert!(llvm.contains("call i8* @KAIN_alloc(i64"));
    assert!(llvm.contains("getelementptr inbounds %__kain_tuple_i64_i64_i64"));
    assert!(llvm.contains("define i64 @unpack_sum()"));
}

#[test]
fn llvm_generates_indexed_array_assignment_and_readback() {
    let source = r#"
fn mutate_items() -> Int:
    let items = [1, 2, 3]
    items[1] = 42
    return items[1]
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("call void @array_set(i8*"));
    assert!(llvm.contains("call i64 @array_get(i8*"));
    assert!(llvm.contains("define i64 @mutate_items()"));
}

#[test]
fn llvm_generates_struct_destructuring_patterns() {
    let point = TypedItem::Struct(TypedStruct {
        ast: Struct {
            name: "Point".to_string(),
            generics: vec![],
            fields: vec![
                Field {
                    name: "x".to_string(),
                    ty: int_type(),
                    attributes: vec![],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: span(),
                },
                Field {
                    name: "y".to_string(),
                    ty: int_type(),
                    attributes: vec![],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: span(),
                },
            ],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: span(),
        },
        field_types: HashMap::from([
            ("x".to_string(), ResolvedType::Int(IntSize::I64)),
            ("y".to_string(), ResolvedType::Int(IntSize::I64)),
        ]),
    });

    let sum_point = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "sum_point".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "p".to_string(),
                            mutable: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Struct {
                            name: "Point".to_string(),
                            fields: vec![
                                ("x".to_string(), Expr::Int(4, span())),
                                ("y".to_string(), Expr::Int(5, span())),
                            ],
                            rest: None,
                            span: span(),
                        }),
                        span: span(),
                    },
                    Stmt::Let {
                        pattern: Pattern::Struct {
                            name: "Point".to_string(),
                            fields: vec![
                                (
                                    "x".to_string(),
                                    Pattern::Binding {
                                        name: "x".to_string(),
                                        mutable: false,
                                        span: span(),
                                    },
                                ),
                                (
                                    "y".to_string(),
                                    Pattern::Binding {
                                        name: "y".to_string(),
                                        mutable: false,
                                        span: span(),
                                    },
                                ),
                            ],
                            rest: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Ident("p".to_string(), span())),
                        span: span(),
                    },
                    Stmt::Return(
                        Some(Expr::Binary {
                            left: Box::new(Expr::Ident("x".to_string(), span())),
                            op: BinaryOp::Add,
                            right: Box::new(Expr::Ident("y".to_string(), span())),
                            span: span(),
                        }),
                        span(),
                    ),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![point, sum_point],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("%Point = type { i64, i64 }"));
    assert!(llvm.contains("getelementptr inbounds %Point, %Point*"));
    assert!(llvm.contains("define i64 @sum_point()"));
}

#[test]
fn llvm_generates_raw_address_indexing_reads_and_writes() {
    let mutate_ptr = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "mutate_ptr".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "ptr".to_string(),
                ty: int_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Expr(Expr::Assign {
                        target: Box::new(Expr::Index {
                            object: Box::new(Expr::Ident("ptr".to_string(), span())),
                            index: Box::new(Expr::Int(1, span())),
                            span: span(),
                        }),
                        value: Box::new(Expr::Int(99, span())),
                        span: span(),
                    }),
                    Stmt::Return(
                        Some(Expr::Index {
                            object: Box::new(Expr::Ident("ptr".to_string(), span())),
                            index: Box::new(Expr::Int(1, span())),
                            span: span(),
                        }),
                        span(),
                    ),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Int(IntSize::I64)],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![mutate_ptr],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("define i64 @mutate_ptr(i64 %arg0)"));
    assert!(llvm.contains("inttoptr i64"));
    assert!(llvm.contains("getelementptr inbounds i64, i64*"));
    assert!(llvm.contains("store i64 99, i64*"));
    assert!(llvm.contains("load i64, i64*"));
}

#[test]
fn llvm_lowers_tuple_aggregate_init_without_dummy_fallback() {
    let build_pair = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "build_pair".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(Type::Tuple(vec![int_type(), int_type()], span())),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::AggregateInit {
                        ty: Type::Tuple(vec![int_type(), int_type()], span()),
                        fields: vec![
                            ("0".to_string(), Expr::Int(10, span())),
                            ("1".to_string(), Expr::Int(20, span())),
                        ],
                        zero_fill_rest: false,
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Tuple(vec![
                ResolvedType::Int(IntSize::I64),
                ResolvedType::Int(IntSize::I64),
            ])),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![build_pair],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("%__kain_tuple_i64_i64 = type { i64, i64 }"));
    assert!(llvm.contains("define %__kain_tuple_i64_i64* @build_pair()"));
    assert!(llvm.contains("call i8* @KAIN_alloc(i64"));
    assert!(!llvm.contains("ret i64 0"));
}

#[test]
fn llvm_rejects_unsupported_expressions_instead_of_silent_dummy_values() {
    let bad_fn = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "bad_fn".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::Lambda {
                        params: vec![],
                        return_type: Some(int_type()),
                        body: Box::new(Expr::Int(1, span())),
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let err = generate_llvm(&TypedProgram {
        items: vec![bad_fn],
    })
    .expect_err("llvm generation should fail for unsupported expressions");

    let message = err.to_string();
    assert!(message.contains("Unsupported LLVM expression"));
    assert!(message.contains("Lambda"));
}

#[test]
fn llvm_lowers_println_to_stdout_write() {
    let typed =
        typed_program_from_source("fn main() -> Int:\n    println(\"llvm\")\n    return 0\n");

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("call void @stdout_write(i8*"));
    assert!(llvm.contains("call i8* @str_concat(i8*"));
}

#[test]
fn llvm_lowers_enum_match_parameters_as_native_enum_pointers() {
    let source = r#"
enum AssetKind:
    Script
    Material

fn classify(kind: AssetKind) -> Int:
    match kind:
        AssetKind::Script => 1
        AssetKind::Material => 2
        _ => 0
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("%AssetKind = type { i64, i8* }"));
    assert!(llvm.contains("define i64 @classify(%AssetKind* %arg0)"));
    assert!(llvm.contains("getelementptr inbounds %AssetKind, %AssetKind*"));
    let classify_ir = llvm_function_ir(&llvm, "define i64 @classify(%AssetKind* %arg0)");
    assert!(classify_ir.contains("phi i64"));
    assert!(classify_ir.contains("ret i64 %"));
    assert!(!classify_ir.trim_end().ends_with("unreachable\n}"));
}

#[test]
fn llvm_lowers_format_and_vec_macros() {
    let source = r#"
fn main() -> Int:
    let values = vec!(1, 2, 3)
    let text = format!("llvm:", len(values))
    stdout_write(text)
    return 0
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("call i8* @array_new(i64 4)"));
    assert!(llvm.contains("call void @array_push(i8*"));
    assert!(llvm.contains("call i8* @str_concat(i8*"));
}

#[test]
fn llvm_flattens_long_string_concat_chains_into_fixed_arity_runtime_calls() {
    let source = r#"
fn bool_text(flag: Bool) -> String:
    if flag:
        return "true"
    return "false"

fn render_payload(id: Int, name: String, enabled: Bool, count: Int) -> String:
    return "{\"id\":" + str(id) + ",\"name\":\"" + name + "\",\"enabled\":" + bool_text(enabled) + ",\"count\":" + str(count) + "}"

fn main() -> Int:
    let rendered = render_payload(17, "orbital", true, 42)
    if rendered == "{\"id\":17,\"name\":\"orbital\",\"enabled\":true,\"count\":42}":
        return 0
    return 1
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");
    let render_ir = llvm_function_ir(
        &llvm,
        "define internal i8* @render_payload(i64 %arg0, i8* %arg1, i1 %arg2, i64 %arg3)",
    );

    assert!(render_ir.contains("call i8* @str_concat9("));
    assert!(
        !render_ir.contains("call i8* @str_concat(i8*"),
        "long render chain should collapse into the fixed-arity helper instead of nested binary concat:\n{}",
        render_ir
    );
    assert!(
        !render_ir.contains("call i64 @strlen(i8* %arg"),
        "string param lengths should be computed lazily instead of at function entry when the body never reads them:\n{}",
        render_ir
    );
    assert!(
        render_ir.contains("call void @rc_release(i8* %r4)")
            && render_ir.contains("call void @rc_release(i8* %r13)")
            && render_ir.contains("call void @rc_release(i8* %r18)"),
        "fixed-arity concat should release owned string temporaries after copying them:\n{}",
        render_ir
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "long-string-concat-fixed-arity");
}

#[test]
fn llvm_hoists_repeated_string_literals_out_of_loop_bodies() {
    let source = r#"
fn build_payload(line_count: Int) -> String:
    let mut text = ""
    let mut index = 0
    while index < line_count:
        text = text + "line-" + str(index % 97) + "-orbital-flux\n"
        index = index + 1
    return text

fn main() -> Int:
    let payload = build_payload(8)
    if len(payload) > 0:
        return 0
    return 1
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");
    let build_ir = llvm_function_ir(&llvm, "define internal i8* @build_payload(i64 %arg0)");
    let loop_start = build_ir
        .find("\n  br label %L")
        .expect("build_payload should branch into a loop");
    let entry_region = &build_ir[..loop_start];
    let loop_region = &build_ir[loop_start..];

    assert!(
        entry_region.matches("call i8* @string_new").count() >= 3,
        "entry preamble should allocate each distinct literal once before the loop:\n{}",
        build_ir
    );
    assert!(
        !loop_region.contains("call i8* @string_new"),
        "loop body should reuse pooled literals instead of allocating them each iteration:\n{}",
        build_ir
    );
    assert!(
        loop_region.contains("load i8*, i8** %__kain_pooled_literal_"),
        "loop body should load from pooled literal slots:\n{}",
        build_ir
    );
    assert!(
        loop_region.contains("call void @rc_release(i8* %r19)"),
        "loop concat should release the owned numeric string temporary after copying it:\n{}",
        build_ir
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "loop-string-literal-pooling");
}

#[test]
fn llvm_lowers_byte_at_on_known_strings_without_runtime_helper_calls() {
    let source = r#"
fn parse_positive_int(text: String, start: Int) -> Int:
    let text_len = len(text)
    let mut index = start
    let mut value = 0
    while index < text_len:
        let digit = byte_at(text, index) - 48
        if digit < 0 or digit > 9:
            return value
        value = value * 10 + digit
        index = index + 1
    return value

fn main() -> Int:
    return parse_positive_int("42", 0)
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");
    let parse_ir = llvm_function_ir(
        &llvm,
        "define internal i64 @parse_positive_int(i8* %arg0, i64 %arg1)",
    );

    assert!(
        !parse_ir.contains("call i64 @byte_at("),
        "byte_at on known strings should lower inline instead of calling the runtime helper:\n{}",
        parse_ir
    );
    assert!(
        parse_ir.contains("call i64 @len(i8*"),
        "string length fallback should use the native len helper rather than rescanning with strlen:\n{}",
        parse_ir
    );
    assert!(
        parse_ir.contains("load i8, i8*"),
        "inline byte_at lowering should load bytes directly from the string buffer:\n{}",
        parse_ir
    );
    assert!(
        !parse_ir.contains("call i64 @strlen("),
        "known-string byte_at lowering should not fall back to strlen:\n{}",
        parse_ir
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "inline-byte-at-known-string");
}

#[test]
fn llvm_hoists_loop_carried_string_param_lengths_out_of_loop_bodies() {
    let source = r#"
fn sum_bytes(text: String) -> Int:
    let mut index = 0
    let mut acc = 0
    while index < len(text):
        acc = acc + byte_at(text, index)
        index = index + 1
    return acc

fn main() -> Int:
    return sum_bytes("kain")
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");
    let sum_ir = llvm_function_ir(&llvm, "define internal i64 @sum_bytes(i8* %arg0)");
    let loop_start = sum_ir
        .find("\n  br label %L")
        .expect("sum_bytes should branch into a loop");
    let entry_region = &sum_ir[..loop_start];
    let loop_region = &sum_ir[loop_start..];

    assert!(
        entry_region.contains("call i64 @len(i8*"),
        "loop-carried string param lengths should be primed before the loop:\n{}",
        sum_ir
    );
    assert!(
        !loop_region.contains("call i64 @len(i8*"),
        "loop body should reuse the entry-cached string length instead of rescanning each iteration:\n{}",
        sum_ir
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "loop-carried-string-param-len-hoist");
}

#[test]
fn llvm_lowers_find_substring_from_on_known_strings_with_precomputed_lengths() {
    let source = r#"
fn locate_field(text: String, key: String) -> Int:
    return find_substring_from(text, key, 0)

fn main() -> Int:
    let payload = "{\"id\":17,\"name\":\"orbital\"}"
    let key = "\"name\":\""
    return locate_field(payload, key)
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");
    let locate_ir = llvm_function_ir(
        &llvm,
        "define internal i64 @locate_field(i8* %arg0, i8* %arg1)",
    );

    assert!(
        locate_ir.contains("call i64 @find_substring_from_known_lengths("),
        "known-string search should lower to the length-aware fast helper:\n{}",
        locate_ir
    );
    assert!(
        !locate_ir.contains("call i64 @find_substring_from("),
        "known-string search should bypass the generic helper that reloads lengths:\n{}",
        locate_ir
    );
    verify_llvm_ir_with_repo_llvm_as(&llvm, "find-substring-known-lengths-fast-path");
}

#[test]
fn llvm_match_ir_verifies_with_guarded_string_results() {
    let source = r#"
enum AssetKind:
    Script
    Material

fn describe(kind: AssetKind, bias: Int) -> String:
    match kind:
        AssetKind::Script if bias > 0 => format!("script:", bias)
        AssetKind::Material => "material"
        _ => "unknown"

fn main() -> Int:
    println(describe(AssetKind::Script, 2))
    return 0
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define i8* @describe(%AssetKind* %arg0, i64 %arg1)"));
    assert!(llvm.contains("phi i8*"));
    verify_llvm_ir_with_repo_llvm_as(&llvm, "guarded-string-match");
}

#[test]
fn llvm_lowers_typed_none_to_null_for_struct_pointer_flows() {
    let node = TypedItem::Struct(TypedStruct {
        ast: Struct {
            name: "Node".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "value".to_string(),
                ty: int_type(),
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: span(),
            }],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: span(),
        },
        field_types: HashMap::from([("value".to_string(), ResolvedType::Int(IntSize::I64))]),
    });

    let bind_none = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "bind_none".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "node".to_string(),
                            mutable: true,
                            span: span(),
                        },
                        ty: Some(Type::Named {
                            name: "Node".to_string(),
                            generics: vec![],
                            span: span(),
                        }),
                        value: Some(Expr::None(span())),
                        span: span(),
                    },
                    Stmt::Return(Some(Expr::Int(0, span())), span()),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let return_none = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "return_none".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(Type::Named {
                name: "Node".to_string(),
                generics: vec![],
                span: span(),
            }),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(Some(Expr::None(span())), span())],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Struct("Node".to_string(), HashMap::new())),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![node, bind_none, return_none],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("%Node = type { i64 }"));
    assert!(llvm.contains("%node.addr_0 = alloca %Node*"));
    assert!(llvm.contains("store %Node* null, %Node** %node.addr_0"));
    assert!(llvm.contains("define %Node* @return_none()"));
    assert!(llvm.contains("ret %Node* null"));
}
