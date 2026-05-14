use kain_core::ast::{Expr, Item, Stmt};
use kain_core::diagnostics::SpanMapper;
use kain_core::{validate_typed_program_memory_support, CompileTarget, Lexer, Parser};

fn parse_program(source: &str) -> kain_core::Program {
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    Parser::new(&tokens, &mapper, "ownership_keywords.kn")
        .parse()
        .expect("parse")
}

fn parse_and_typecheck(source: &str) -> Result<kain_core::TypedProgram, kain_core::KainError> {
    let tokens = Lexer::new(source).tokenize()?;
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "ownership_keywords.kn").parse()?;
    kain_core::types::check(&program, &mapper, "ownership_keywords.kn")
}

#[test]
fn parser_recognizes_observe_collapse_and_decay_expressions() {
    let source = r#"fn own(p: ptr<Int>) -> Int:
    let read = observe p:
        mem_load(p, "Int")
    collapse p:
        mem_store(p, read + 1, "Int")
        0
    decay p
    return read
"#;

    let program = parse_program(source);
    let function = match &program.items[0] {
        Item::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let Stmt::Let {
        value: Some(Expr::Observe { target, body, .. }),
        ..
    } = &function.body.stmts[0]
    else {
        panic!("expected observe let initializer");
    };
    assert!(matches!(target.as_ref(), Expr::Ident(name, _) if name == "p"));
    assert!(matches!(body.as_ref(), Expr::Block(_, _)));

    let Stmt::Expr(Expr::Collapse { target, body, .. }) = &function.body.stmts[1] else {
        panic!("expected collapse expression statement");
    };
    assert!(matches!(target.as_ref(), Expr::Ident(name, _) if name == "p"));
    assert!(matches!(body.as_ref(), Expr::Block(_, _)));

    let Stmt::Expr(Expr::Decay { target, .. }) = &function.body.stmts[2] else {
        panic!("expected decay expression statement");
    };
    assert!(matches!(target.as_ref(), Expr::Ident(name, _) if name == "p"));
}

#[test]
fn ownership_keywords_typecheck_for_pointer_regions() {
    let source = r#"fn own(p: ptr<Int>) -> Int:
    let read = observe p:
        mem_load(p, "Int")
    collapse p:
        mem_store(p, read + 1, "Int")
        0
    decay p
    return read
"#;

    parse_and_typecheck(source).expect("ownership scopes should typecheck for ptr regions");
}

#[test]
fn typecheck_rejects_non_pointer_ownership_targets() {
    let source = r#"fn bad(value: Int) -> Int:
    let read = observe value:
        0
    return read
"#;

    let err = parse_and_typecheck(source).expect_err("observe should reject scalar targets");
    assert!(err
        .to_string()
        .contains("observe expects a pointer-like ownership region"));
}

#[test]
fn typecheck_rejects_early_exit_inside_scoped_ownership() {
    let source = r#"fn bad(p: ptr<Int>) -> Int:
    observe p:
        return 1
    return 0
"#;

    let err = parse_and_typecheck(source).expect_err("observe should reject unbalanced exits");
    assert!(err
        .to_string()
        .contains("observe scopes do not support return, break, or continue"));
}

#[test]
fn non_native_backends_reject_ownership_expressions_before_codegen() {
    let source = r#"fn own(p: ptr<Int>) -> Int:
    let read = observe p:
        mem_load(p, "Int")
    return read
"#;

    let typed = parse_and_typecheck(source).expect("native ownership source should typecheck");
    let err = validate_typed_program_memory_support(&typed, CompileTarget::Ts)
        .expect_err("ts should reject ownership memory semantics");
    let rendered = err.to_string();
    assert!(rendered.contains("KAIN-MEM-0002"));
    assert!(
        rendered.contains("raw pointer") || rendered.contains("ownership observe expression"),
        "unexpected diagnostic: {rendered}"
    );
}
