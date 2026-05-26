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

#[test]
fn parser_accepts_compound_assignment_operators() {
    let source = r#"fn main() -> Int:
    let mut x = 10
    x += 2
    x -= 1
    x *= 3
    x /= 3
    x %= 4
    x <<= 2
    x >>= 1
    x &= 6
    x |= 1
    x ^= 3
    return x"#;

    let typed = parse_and_typecheck(source)
        .expect("compound assignment operators should parse and typecheck");
    assert!(!typed.items.is_empty());
}

#[test]
fn runtime_evaluates_compound_assignment_and_bitnot() {
    let source = r#"fn main() -> Int:
    let mut x = 2
    x += 3
    x *= 4
    x /= 5
    x -= 1
    x <<= 2
    x >>= 1
    x %= 4
    x |= 8
    x &= 14
    x ^= 3
    return ~x"#;

    let typed = parse_and_typecheck(source).expect("source should typecheck");
    let result = runtime::interpret(&typed).expect("runtime should evaluate expression");

    match result {
        runtime::Value::Int(value) => assert_eq!(value, -10),
        runtime::Value::Return(inner) => match *inner {
            runtime::Value::Int(value) => assert_eq!(value, -10),
            other => panic!("expected return(int), got {:?}", other),
        },
        other => panic!("expected int runtime value, got {:?}", other),
    }
}

#[test]
fn parser_accepts_inc_dec_ternary_and_raw_strings() {
    let source = r#"fn main() -> Int:
    let mut x = 1
    x++
    ++x
    x--
    --x
    let path = r"C:\temp\kain\file.txt"
    return (x == 1) ? 7 : 9"#;

    let typed = parse_and_typecheck(source)
        .expect("inc/dec, ternary, and raw strings should parse and typecheck");
    assert!(!typed.items.is_empty());
}

#[test]
fn runtime_evaluates_inc_dec_and_ternary() {
    let source = r#"fn main() -> Int:
    let mut x = 1
    x++
    ++x
    x--
    --x
    return (x == 1) ? 42 : 0"#;

    let typed = parse_and_typecheck(source).expect("source should typecheck");
    let result = runtime::interpret(&typed).expect("runtime should evaluate expression");

    match result {
        runtime::Value::Int(value) => assert_eq!(value, 42),
        runtime::Value::Return(inner) => match *inner {
            runtime::Value::Int(value) => assert_eq!(value, 42),
            other => panic!("expected return(int), got {:?}", other),
        },
        other => panic!("expected int runtime value, got {:?}", other),
    }
}

#[test]
fn runtime_preserves_prefix_postfix_expression_semantics() {
    let source = r#"fn main() -> Int:
    let mut x = 1
    let a = x++
    let b = ++x
    return ((a * 10) + b)"#;

    let typed = parse_and_typecheck(source).expect("source should typecheck");
    let result = runtime::interpret(&typed).expect("runtime should evaluate expression");

    match result {
        runtime::Value::Int(value) => assert_eq!(value, 13),
        runtime::Value::Return(inner) => match *inner {
            runtime::Value::Int(value) => assert_eq!(value, 13),
            other => panic!("expected return(int), got {:?}", other),
        },
        other => panic!("expected int runtime value, got {:?}", other),
    }
}

#[test]
fn runtime_evaluates_null_coalesce() {
    let source = r#"fn main() -> Int:
    let a = (none ?? 7)
    let b = (3 ?? 9)
    return (a + b)"#;

    let typed = parse_and_typecheck(source).expect("source should typecheck");
    let result = runtime::interpret(&typed).expect("runtime should evaluate expression");

    match result {
        runtime::Value::Int(value) => assert_eq!(value, 10),
        runtime::Value::Return(inner) => match *inner {
            runtime::Value::Int(value) => assert_eq!(value, 10),
            other => panic!("expected return(int), got {:?}", other),
        },
        other => panic!("expected int runtime value, got {:?}", other),
    }
}

#[test]
fn parser_accepts_safe_navigation() {
    let source = r#"fn main() -> Int:
    let value = (none?.field) ?? 1
    return value"#;

    let typed = parse_and_typecheck(source).expect("safe navigation should parse and typecheck");
    assert!(!typed.items.is_empty());
}

#[test]
fn parser_accepts_ref_and_deref_prefix_operators() {
    let source = r#"fn main() -> Int:
    let mut x = 41
    let p = &x
    return (*p + 1)"#;

    let typed =
        parse_and_typecheck(source).expect("ref/deref operators should parse and typecheck");
    assert!(!typed.items.is_empty());
}

#[test]
fn typecheck_flattens_inline_module_items() {
    let source = r#"mod imported:
    struct Point:
        x: Int

    fn make_point(x: Int) -> Int:
        return x

fn main() -> Int:
    return make_point(7)"#;

    let typed = parse_and_typecheck(source).expect("inline module items should typecheck");
    assert_eq!(typed.items.len(), 3);
}
