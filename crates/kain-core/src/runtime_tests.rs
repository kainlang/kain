use crate::comptime;
use crate::diagnostics::SpanMapper;
use crate::lexer::Lexer;
use crate::parser::Parser;
use crate::runtime::{interpret, Value};
use crate::stdlib::StdLib;
use crate::types;

fn interpret_test_source(source: &str) -> Value {
    let tokens = Lexer::new(source).tokenize().expect("tokenize test source");
    let span_mapper = SpanMapper::new(source);
    let mut ast = Parser::new(&tokens, &span_mapper, "<runtime-test>")
        .parse()
        .expect("parse test source");
    comptime::eval_program(&mut ast).expect("run comptime");
    let typed_program = types::check(&ast, &span_mapper, "<runtime-test>").expect("typecheck");
    interpret(&typed_program).expect("interpret")
}

#[test]
fn elif_chain_executes_matching_arm() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let opcode = "+"
    if opcode == ">":
        return 1
    elif opcode == "+":
        return 2
    else:
        return 3
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 2),
        other => panic!("expected Int(2), got {:?}", other),
    }
}

#[test]
fn string_len_method_matches_typechecker() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let text = "brainfuck"
    return text.len()
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 9),
        other => panic!("expected Int(9), got {:?}", other),
    }
}

#[test]
fn char_at_out_of_range_returns_empty_string() {
    let value = interpret_test_source(
        r#"
fn main() -> String:
    return char_at("abc", 99)
"#,
    );

    match value {
        Value::String(result) => assert_eq!(result, ""),
        other => panic!("expected empty string, got {:?}", other),
    }
}

#[test]
fn stdlib_registry_exposes_ord_and_chr() {
    let stdlib = StdLib::new();

    assert!(stdlib.functions.contains_key("ord"));
    assert!(stdlib.functions.contains_key("chr"));
    assert!(stdlib.functions.contains_key("ask"));
    assert!(stdlib.functions.contains_key("ask_timeout"));
    assert!(stdlib.functions.contains_key("command_run"));
    assert!(stdlib.functions.contains_key("json_parse"));
    assert!(stdlib.functions.contains_key("json_object_new"));
    assert!(stdlib.functions.contains_key("read_line"));
    assert!(stdlib.functions.contains_key("stdout_write"));
    assert!(stdlib.functions.contains_key("stdin_read_exact"));
    assert!(stdlib.functions.contains_key("to_int"));
}

#[test]
fn gen_server_stdlib_round_trip() {
    let stdlib = include_str!("../../../stdlib/gen_server.kn");
    let value = interpret_test_source(&format!(
        r#"
{stdlib}

fn main() -> Int:
    let server = gen_server_start_link(
        0,
        |request, state| gen_server_call_result(state + request, state + request),
        |request, state| state + request,
        |message, state| state + message
    )
    let first = gen_server_call(server, 5)
    gen_server_cast(server, 10)
    gen_server_info(server, 2)
    let second = gen_server_call(server, 3)
    return first + second
"#
    ));

    match value {
        Value::Int(result) => assert_eq!(result, 25),
        other => panic!("expected Int(25), got {:?}", other),
    }
}

#[test]
fn command_run_builtin_captures_stdout_and_status() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let result = command_run("bash", ["-lc", "printf hello"], "")
    if result.success && result.status == 0 && result.stdout == "hello":
        return 1
    return 0
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 1),
        other => panic!("expected Int(1), got {:?}", other),
    }
}

#[test]
fn json_helpers_extract_nested_fields() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let payload = json_parse("{\"method\":\"tools/call\",\"params\":{\"count\":7,\"enabled\":true}}")
    let params = json_get(payload, "params")
    if json_get_string(payload, "method") == "tools/call" && json_get_int(params, "count") == 7 && json_get_bool(params, "enabled"):
        return 1
    return 0
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 1),
        other => panic!("expected Int(1), got {:?}", other),
    }
}

#[test]
fn ask_timeout_builtin_round_trips_actor_reply() {
    let value = interpret_test_source(
        r#"
actor Echo:
    on Call(reply_to: P, request: Int):
        send reply_to.Reply(value = request + 1)

fn main() -> Int:
    let echo = spawn Echo()
    return ask_timeout(echo, "Call", 9, 1000)
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 10),
        other => panic!("expected Int(10), got {:?}", other),
    }
}
