use kain_core::ast::Type;
use kain_core::diagnostics::SpanMapper;
use kain_core::error::KainError;
use kain_core::{validate_typed_program_memory_support, CompileTarget, Lexer, Parser};

#[test]
fn parser_recognizes_ptr_type_syntax() {
    let source = "fn take_ptr(p: ptr<Int>) -> Int:\n    return 0\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "ptr_test.kn")
        .parse()
        .expect("parse");

    let function = match &program.items[0] {
        kain_core::ast::Item::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    match &function.params[0].ty {
        Type::Ptr { mutable, inner, .. } => {
            assert!(!mutable);
            assert!(matches!(inner.as_ref(), Type::Named { name, .. } if name == "Int"));
        }
        other => panic!("expected ptr type, got {other:?}"),
    }
}

#[test]
fn ts_backend_validation_rejects_raw_ptr_types() {
    let source = "fn take_ptr(p: ptr<Int>) -> Int:\n    return 0\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "ptr_test.kn")
        .parse()
        .expect("parse");
    let typed = kain_core::types::check(&program, &mapper, "ptr_test.kn").expect("typecheck");

    let err = validate_typed_program_memory_support(&typed, CompileTarget::Ts)
        .expect_err("ts backend should reject raw ptr types");

    match err {
        KainError::Enhanced { .. } => {
            let rendered = err.to_string();
            assert!(rendered.contains("KAIN-MEM-0002"));
            assert!(rendered.contains("raw pointer"));
        }
        other => panic!("expected enhanced diagnostic, got {other:?}"),
    }
}

#[test]
fn parser_normalizes_ptr_offset_call() {
    let source = "fn advance(p: ptr<Int>, i: Int) -> ptr<Int>:\n    return ptr_offset(p, i)\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "ptr_offset_test.kn")
        .parse()
        .expect("parse");

    let function = match &program.items[0] {
        kain_core::ast::Item::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::PtrOffset { pointer, offset, .. }), _) =
        &function.body.stmts[0]
    else {
        panic!("expected ptr_offset expression in return");
    };

    assert!(matches!(pointer.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "p"));
    assert!(matches!(offset.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "i"));
}

#[test]
fn parser_normalizes_mem_load_and_store_calls() {
    let source = "fn poke(p: ptr<Int>, v: Int) -> Int:\n    mem_store(p, v)\n    return mem_load(p)\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "mem_ops_test.kn")
        .parse()
        .expect("parse");

    let function = match &program.items[0] {
        kain_core::ast::Item::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Expr(kain_core::ast::Expr::MemStore { pointer, value, .. }) =
        &function.body.stmts[0]
    else {
        panic!("expected mem_store expression");
    };
    assert!(matches!(pointer.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "p"));
    assert!(matches!(value.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "v"));

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::MemLoad { pointer, .. }), _) =
        &function.body.stmts[1]
    else {
        panic!("expected mem_load return expression");
    };
    assert!(matches!(pointer.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "p"));
}

#[test]
fn ts_backend_validation_rejects_raw_memory_ops() {
    let source = "fn poke(p: Int, v: Int) -> Int:\n    mem_store(p, v)\n    return mem_load(p)\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "mem_ops_test.kn")
        .parse()
        .expect("parse");
    let typed = kain_core::types::check(&program, &mapper, "mem_ops_test.kn").expect("typecheck");

    let err = validate_typed_program_memory_support(&typed, CompileTarget::Ts)
        .expect_err("ts backend should reject raw memory ops");
    let rendered = err.to_string();
    assert!(rendered.contains("KAIN-MEM-0002"));
    assert!(rendered.contains("raw memory operation"));
}
