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
}
