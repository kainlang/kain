use kain_core::{diagnostics, error, lexer, parser, types};

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let ast = parser::Parser::new(&tokens, &span_mapper, "<test>").parse()?;
    types::check(&ast, &span_mapper, "<test>")
}

fn render_error(source: &str, filename: &str, err: &error::KainError) -> String {
    diagnostics::Diagnostics::new(source, filename).format_error(err)
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

#[test]
fn typecheck_unknown_identifier_includes_semantic_typo_guidance() {
    let source = r#"fn mix_scalar(value: Int) -> Int:
    return value

fn main() -> Int:
    return mix_scalr(1)
"#;

    let err = parse_and_typecheck(source).expect_err("typecheck should reject typoed symbol");
    let rendered = render_error(source, "typo.kn", &err);

    assert!(
        rendered.contains("Closest known symbol is 'mix_scalar'"),
        "expected semantic explanation, got: {rendered}"
    );
    assert!(
        rendered.contains("replace with 'mix_scalar'"),
        "expected typo fix-it, got: {rendered}"
    );
    assert!(
        rendered.contains("fix-it"),
        "expected fix-it block, got: {rendered}"
    );

    let json = err
        .diagnostic_json()
        .expect("rich type diagnostics should expose JSON");
    assert_eq!(json["diagnostics"][0]["semantic"]["failure_mode"], "typo");
    assert_eq!(
        json["diagnostics"][0]["fixits"][0]["replacement"],
        "mix_scalar"
    );
}

#[test]
fn typecheck_world_missing_surface_includes_semantic_surface_guidance() {
    let source = r#"world Demo:
    state hp: Int = 3

fn main() -> Int:
    return 0
"#;

    let err =
        parse_and_typecheck(source).expect_err("typecheck should reject world without surface");
    let rendered = render_error(source, "world.kn", &err);

    assert!(
        rendered.contains("world 'Demo' must declare at least one surface"),
        "expected world error headline, got: {rendered}"
    );
    assert!(
        rendered.contains("This world declaration is missing a surface clause."),
        "expected semantic explanation, got: {rendered}"
    );
    assert!(
        rendered.contains("Add a 'surface native_ui => ...' or 'surface web => ...' clause"),
        "expected semantic repair help, got: {rendered}"
    );

    let json = err
        .diagnostic_json()
        .expect("rich world diagnostics should expose JSON");
    assert_eq!(
        json["diagnostics"][0]["semantic"]["failure_mode"],
        "missing_surface"
    );
}

#[test]
fn typecheck_accumulates_multiple_same_file_errors() {
    let source = r#"let first: Int = "hello"
let second = missing_top + 1
let third: Bool = 123
"#;

    let err = parse_and_typecheck(source).expect_err("typecheck should accumulate script errors");
    let json = err
        .diagnostic_json()
        .expect("multi-error typecheck should expose JSON diagnostics");
    let diagnostics = json["diagnostics"].as_array().expect("diagnostics array");

    assert!(
        diagnostics.len() >= 3,
        "expected at least 3 diagnostics, got {}: {json}",
        diagnostics.len()
    );
    assert!(diagnostics.iter().any(|diag| diag["message"]
        .as_str()
        .is_some_and(|message| message.contains("let binding expected Int, found String"))));
    assert!(diagnostics.iter().any(|diag| diag["message"]
        .as_str()
        .is_some_and(|message| message.contains("Unknown identifier 'missing_top'"))));
    assert!(diagnostics.iter().any(|diag| diag["message"]
        .as_str()
        .is_some_and(|message| message.contains("let binding expected Bool, found Int"))));
}
