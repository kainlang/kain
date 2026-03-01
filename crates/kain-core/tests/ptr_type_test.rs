use kain_core::ast::Type;
use kain_core::diagnostics::SpanMapper;
use kain_core::error::KainError;
use kain_core::{
    lower_typed_program_memory_for_target, validate_typed_program_memory_support, CompileTarget,
    Lexer, Parser,
};

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

#[test]
fn parser_preserves_typed_memory_intrinsics() {
    let source = "fn poke(p: ptr<Int>, i: Int, v: Int) -> Int:\n    mem_store(ptr_offset(addr_of(v, \"Int\"), i, \"Int\"), v, \"Int\")\n    return mem_load(ptr_offset(p, i, \"Int\"), \"Int\")\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "typed_mem.kn")
        .parse()
        .expect("parse");

    let function = match &program.items[0] {
        kain_core::ast::Item::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Expr(kain_core::ast::Expr::MemStore { pointer, store_ty, .. }) =
        &function.body.stmts[0]
    else {
        panic!("expected typed mem_store");
    };
    assert!(matches!(store_ty, Some(Type::Named { name, .. }) if name == "Int"));
    let kain_core::ast::Expr::PtrOffset { pointer: base, element_ty, .. } = pointer.as_ref() else {
        panic!("expected ptr_offset in mem_store");
    };
    assert!(matches!(element_ty, Some(Type::Named { name, .. }) if name == "Int"));
    assert!(matches!(base.as_ref(), kain_core::ast::Expr::AddrOf { .. }));

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::MemLoad { pointer, load_ty, .. }), _) =
        &function.body.stmts[1]
    else {
        panic!("expected typed mem_load");
    };
    assert!(matches!(load_ty, Some(Type::Named { name, .. }) if name == "Int"));
    assert!(matches!(pointer.as_ref(), kain_core::ast::Expr::PtrOffset { .. }));
}

#[test]
fn ts_memory_lowering_rewrites_raw_ops_into_helper_calls() {
    let source = "fn poke(p: ptr<Int>, i: Int, v: Int) -> Int:\n    mem_store(ptr_offset(p, i, \"Int\"), v, \"Int\")\n    return mem_load(ptr_offset(p, i, \"Int\"), \"Int\")\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "typed_mem.kn")
        .parse()
        .expect("parse");
    let typed = kain_core::types::check(&program, &mapper, "typed_mem.kn").expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, CompileTarget::Ts).expect("lower");

    let function = match &lowered.items[0] {
        kain_core::types::TypedItem::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    assert!(matches!(function.ast.params[0].ty, Type::Named { ref name, .. } if name == "Int"));
    let kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Call { callee, .. }) =
        &function.ast.body.stmts[0]
    else {
        panic!("expected lowered mem_store helper call");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_mem_store"));

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::Cast { value, target, .. }), _) =
        &function.ast.body.stmts[1]
    else {
        panic!("expected lowered mem_load cast");
    };
    assert!(matches!(target, Type::Named { name, .. } if name == "Int"));
    let kain_core::ast::Expr::Call { callee, .. } = value.as_ref() else {
        panic!("expected helper call inside cast");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_mem_load"));
}

#[test]
fn ts_memory_lowering_binds_address_taken_locals() {
    let source = "fn mutate() -> Int:\n    let mut x: Int = 1\n    let p: ptr<Int> = addr_of(x, \"Int\")\n    mem_store(p, 7, \"Int\")\n    return x\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "local_addr.kn")
        .parse()
        .expect("parse");
    let typed = kain_core::types::check(&program, &mapper, "local_addr.kn").expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, CompileTarget::Ts).expect("lower");

    let function = match &lowered.items[0] {
        kain_core::types::TypedItem::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Let {
        pattern: kain_core::ast::Pattern::Binding { name, .. },
        ..
    } = &function.ast.body.stmts[1]
    else {
        panic!("expected pointer binding stmt");
    };
    assert_eq!(name, "__kain_ptr_x");

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::Cast { value, .. }), _) =
        &function.ast.body.stmts[4]
    else {
        panic!("expected return of lowered mem_load");
    };
    let kain_core::ast::Expr::Call { callee, args, .. } = value.as_ref() else {
        panic!("expected helper call");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_mem_load"));
    assert!(matches!(&args[0].value, kain_core::ast::Expr::Ident(name, _) if name == "__kain_ptr_x"));
}

#[test]
fn ts_memory_lowering_uses_layout_offsets_for_field_addresses() {
    let source = "struct Pair:\n    left: Int\n    right: Int\n\nfn field_ptr(pair: Pair) -> ptr<Int>:\n    return addr_of(pair.right, \"Int\")\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "field_addr.kn")
        .parse()
        .expect("parse");
    let typed = kain_core::types::check(&program, &mapper, "field_addr.kn").expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, CompileTarget::Ts).expect("lower");

    let function = match &lowered.items[1] {
        kain_core::types::TypedItem::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::Call { callee, args, .. }), _) =
        &function.ast.body.stmts[1]
    else {
        panic!("expected lowered field pointer helper");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_field_ptr"));
    assert!(matches!(&args[1].value, kain_core::ast::Expr::String(field, _) if field == "right"));
    assert!(matches!(&args[2].value, kain_core::ast::Expr::Int(offset, _) if *offset == 8));
}
