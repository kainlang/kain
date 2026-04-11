use kain_core::{diagnostics, error, lexer, parser, types};

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    types::check(&ast, &span_mapper, "<test>")
}

#[test]
fn typecheck_rejects_return_type_mismatch() {
    let source = r#"fn bad() -> Int:
    return true
"#;

    let err = parse_and_typecheck(source).expect_err("typecheck should reject mismatched return");
    let message = err.to_string();
    assert!(message.contains("return value expected Int, found Bool"));
}

#[test]
fn typecheck_rejects_mismatched_match_arm_types() {
    let source = r#"fn classify(flag: Bool) -> Int:
    match flag:
        true => 1
        false => "nope"
"#;

    let err =
        parse_and_typecheck(source).expect_err("typecheck should reject mismatched match arms");
    let message = err.to_string();
    assert!(message.contains("match arms do not agree on a type"));
}

#[test]
fn typecheck_rejects_duplicate_boolean_match_arms() {
    let source = r#"fn classify(flag: Bool) -> Int:
    match flag:
        true => 1
        true => 2
    return 0
"#;

    let err =
        parse_and_typecheck(source).expect_err("typecheck should reject duplicate boolean arms");
    let message = err.to_string();
    assert!(message.contains("Duplicate boolean match arm"));
}

#[test]
fn typecheck_accepts_async_block_and_await() {
    let source = r#"fn spawn_value() -> impl Future<Int>:
    return async 42

fn use_value() -> Int:
    let fut = async 42
    return await fut
"#;

    parse_and_typecheck(source).expect("async blocks and await should typecheck");
}
