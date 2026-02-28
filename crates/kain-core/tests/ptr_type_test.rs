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
