use kain_core::*;

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    comptime::eval_program(&mut ast)?;
    types::check(&ast, &span_mapper, "<test>")
}

#[test]
fn parser_accepts_bitwise_and_shift_operators() {
    let source = r#"fn main() -> Int:
    return 6 & 3 | 8 >> 1 ^ 1 << 2"#;

    let typed = parse_and_typecheck(source).expect("bitwise operators should parse and typecheck");
    assert!(!typed.items.is_empty());
}

#[test]
fn runtime_evaluates_bitwise_expression() {
    let source = r#"fn main() -> Int:
    return 6 & 3 | 8 >> 1 ^ 1 << 2"#;

    let typed = parse_and_typecheck(source).expect("source should typecheck");
    let result = runtime::interpret(&typed).expect("runtime should evaluate expression");

    match result {
        runtime::Value::Int(value) => assert_eq!(value, 2),
        runtime::Value::Return(inner) => match *inner {
            runtime::Value::Int(value) => assert_eq!(value, 2),
            other => panic!("expected return(int), got {:?}", other),
        },
        other => panic!("expected int runtime value, got {:?}", other),
    }
}
