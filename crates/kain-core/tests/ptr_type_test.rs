use kain_core::ast::Type;
use kain_core::diagnostics::SpanMapper;
use kain_core::error::KainError;
use kain_core::low_level_memory_metadata::{marker_attr, usize_attr, C_BITFIELD_ATTR, C_UNION_ATTR};
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
fn parser_normalizes_sizeof_type_call() {
    let source = "fn size() -> Int:\n    return sizeof_type(\"ptr<Int>\")\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "sizeof_type.kn")
        .parse()
        .expect("parse");

    let function = match &program.items[0] {
        kain_core::ast::Item::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::SizeOfType { target, .. }), _) =
        &function.body.stmts[0]
    else {
        panic!("expected sizeof_type return");
    };
    assert!(matches!(target, Type::Ptr { inner, .. } if matches!(inner.as_ref(), Type::Named { name, .. } if name == "Int")));
}

#[test]
fn parser_normalizes_alignof_alloca_and_uninit_calls() {
    let source = "fn storage() -> Int:\n    let mut buf: [Int; 2] = alloca(\"[Int; 2]\")\n    let mut x: Int = uninit(\"Int\")\n    return alignof_type(\"Int\")\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "storage_intrinsics.kn")
        .parse()
        .expect("parse");

    let function = match &program.items[0] {
        kain_core::ast::Item::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::Alloca { ty, .. }), .. } =
        &function.body.stmts[0]
    else {
        panic!("expected alloca initializer");
    };
    assert!(matches!(ty, Type::Array(_, 2, _)));

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::Uninit { ty, .. }), .. } =
        &function.body.stmts[1]
    else {
        panic!("expected uninit initializer");
    };
    assert!(matches!(ty, Type::Named { name, .. } if name == "Int"));

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::AlignOfType { target, .. }), _) =
        &function.body.stmts[2]
    else {
        panic!("expected alignof_type return");
    };
    assert!(matches!(target, Type::Named { name, .. } if name == "Int"));
}

#[test]
fn parser_normalizes_alloc_realloc_and_aggregate_init_calls() {
    let source = "struct Pair:\n    left: Int\n    right: Int\n\nfn heap(n: Int, p: ptr<Int>) -> ptr<Pair>:\n    let mut q: ptr<Pair> = alloc(sizeof_type(\"Pair\"), \"Pair\")\n    let mut r: ptr<Int> = realloc_mem(p, (n * sizeof_type(\"Int\")), \"Int\")\n    let mut s: Pair = aggregate_init(\"Pair\", true, left = 1)\n    return q\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "heap_intrinsics.kn")
        .parse()
        .expect("parse");

    let function = match &program.items[1] {
        kain_core::ast::Item::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::Alloc { ty, zeroed, .. }), .. } =
        &function.body.stmts[0]
    else {
        panic!("expected alloc initializer");
    };
    assert!(!zeroed);
    assert!(matches!(ty, Some(Type::Named { name, .. }) if name == "Pair"));

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::Realloc { ty, .. }), .. } =
        &function.body.stmts[1]
    else {
        panic!("expected realloc initializer");
    };
    assert!(matches!(ty, Some(Type::Named { name, .. }) if name == "Int"));

    let kain_core::ast::Stmt::Let {
        value: Some(kain_core::ast::Expr::AggregateInit { zero_fill_rest, .. }),
        ..
    } = &function.body.stmts[2]
    else {
        panic!("expected aggregate init initializer");
    };
    assert!(*zero_fill_rest);
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

#[test]
fn ts_memory_lowering_resolves_sizeof_type_from_layouts() {
    let source = "struct Pair:\n    left: Int\n    right: Int\n\nfn size() -> Int:\n    return sizeof_type(\"Pair\")\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "sizeof_layout.kn")
        .parse()
        .expect("parse");
    let typed = kain_core::types::check(&program, &mapper, "sizeof_layout.kn").expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, CompileTarget::Ts).expect("lower");

    let function = match &lowered.items[1] {
        kain_core::types::TypedItem::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::Int(size, _)), _) =
        &function.ast.body.stmts[0]
    else {
        panic!("expected lowered integer sizeof");
    };
    assert_eq!(*size, 16);
}

#[test]
fn ts_memory_lowering_resolves_alignof_and_storage_nodes() {
    let source = "fn storage() -> Int:\n    let mut buf: [Int; 2] = alloca(\"[Int; 2]\")\n    let mut x: Int = uninit(\"Int\")\n    return alignof_type(\"Int\")\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "storage_lowering.kn")
        .parse()
        .expect("parse");
    let typed = kain_core::types::check(&program, &mapper, "storage_lowering.kn").expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, CompileTarget::Ts).expect("lower");

    let function = match &lowered.items[0] {
        kain_core::types::TypedItem::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::Array(items, _)), .. } =
        &function.ast.body.stmts[0]
    else {
        panic!("expected lowered alloca array");
    };
    assert_eq!(items.len(), 2);

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::None(_)), .. } =
        &function.ast.body.stmts[1]
    else {
        panic!("expected lowered uninit scalar as none");
    };

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::Int(align, _)), _) =
        &function.ast.body.stmts[2]
    else {
        panic!("expected lowered align integer");
    };
    assert_eq!(*align, 8);
}

#[test]
fn ts_memory_lowering_rewrites_heap_nodes_and_zero_fills_struct_aggregates() {
    let source = "struct Pair:\n    left: Int\n    right: Int\n\nfn heap(n: Int, p: ptr<Int>) -> Pair:\n    let mut q: ptr<Pair> = alloc(sizeof_type(\"Pair\"), \"Pair\")\n    let mut r: ptr<Int> = realloc_mem(p, (n * sizeof_type(\"Int\")), \"Int\")\n    let mut s: Pair = aggregate_init(\"Pair\", true, left = 1)\n    return s\n";
    let tokens = Lexer::new(source).tokenize().expect("lex");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "heap_lowering.kn")
        .parse()
        .expect("parse");
    let typed = kain_core::types::check(&program, &mapper, "heap_lowering.kn").expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, CompileTarget::Ts).expect("lower");

    let function = match &lowered.items[1] {
        kain_core::types::TypedItem::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::Call { callee, .. }), .. } =
        &function.ast.body.stmts[0]
    else {
        panic!("expected lowered alloc helper");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_alloc"));

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::Call { callee, .. }), .. } =
        &function.ast.body.stmts[1]
    else {
        panic!("expected lowered realloc helper");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_realloc"));

    let kain_core::ast::Stmt::Let { value: Some(kain_core::ast::Expr::Struct { fields, .. }), .. } =
        &function.ast.body.stmts[2]
    else {
        panic!("expected lowered aggregate init struct");
    };
    assert!(matches!(&fields[0].1, kain_core::ast::Expr::Int(1, _)));
    assert!(matches!(&fields[1].1, kain_core::ast::Expr::Int(0, _)));
}

#[test]
fn ts_memory_lowering_uses_union_layout_metadata() {
    let span = kain_core::span::Span::default();
    let int_ty = Type::Named {
        name: "Int".to_string(),
        generics: Vec::new(),
        span,
    };
    let float_ty = Type::Named {
        name: "Float".to_string(),
        generics: Vec::new(),
        span,
    };
    let union_ty = Type::Named {
        name: "Number".to_string(),
        generics: Vec::new(),
        span,
    };

    let program = kain_core::ast::Program {
        items: vec![
            kain_core::ast::Item::Struct(kain_core::ast::Struct {
                name: "Number".to_string(),
                generics: Vec::new(),
                fields: vec![
                    kain_core::ast::Field {
                        name: "as_int".to_string(),
                        ty: int_ty.clone(),
                        attributes: Vec::new(),
                        visibility: kain_core::ast::Visibility::Public,
                        default: None,
                        weak: false,
                        span,
                    },
                    kain_core::ast::Field {
                        name: "as_float".to_string(),
                        ty: float_ty.clone(),
                        attributes: Vec::new(),
                        visibility: kain_core::ast::Visibility::Public,
                        default: None,
                        weak: false,
                        span,
                    },
                ],
                methods: Vec::new(),
                attributes: vec![marker_attr(C_UNION_ATTR, span)],
                visibility: kain_core::ast::Visibility::Public,
                span,
            }),
            kain_core::ast::Item::Function(kain_core::ast::Function {
                name: "make_number".to_string(),
                generics: Vec::new(),
                params: Vec::new(),
                return_type: Some(union_ty.clone()),
                effects: Vec::new(),
                body: kain_core::ast::Block {
                    stmts: vec![
                        kain_core::ast::Stmt::Let {
                            pattern: kain_core::ast::Pattern::Binding {
                                name: "n".to_string(),
                                mutable: true,
                                span,
                            },
                            ty: Some(union_ty.clone()),
                            value: Some(kain_core::ast::Expr::AggregateInit {
                                ty: union_ty.clone(),
                                fields: vec![
                                    ("as_int".to_string(), kain_core::ast::Expr::Int(7, span)),
                                    (
                                        "as_float".to_string(),
                                        kain_core::ast::Expr::Float(3.0, span),
                                    ),
                                ],
                                zero_fill_rest: true,
                                span,
                            }),
                            span,
                        },
                        kain_core::ast::Stmt::Return(
                            Some(kain_core::ast::Expr::SizeOfType {
                                target: union_ty,
                                span,
                            }),
                            span,
                        ),
                    ],
                    span,
                },
                visibility: kain_core::ast::Visibility::Public,
                attributes: Vec::new(),
                span,
            }),
        ],
        span,
    };

    let mapper = SpanMapper::new("");
    let typed = kain_core::types::check(&program, &mapper, "union_lowering.kn").expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, CompileTarget::Ts).expect("lower");

    let function = match &lowered.items[1] {
        kain_core::types::TypedItem::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Let {
        value: Some(kain_core::ast::Expr::Call { callee, args, .. }),
        ..
    } = &function.ast.body.stmts[0]
    else {
        panic!("expected lowered union aggregate");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_union_wrap"));
    let kain_core::ast::Expr::Struct { fields, .. } = &args[0].value else {
        panic!("expected wrapped struct value");
    };
    assert_eq!(fields.len(), 2);
    assert_eq!(args[1].value, kain_core::ast::Expr::String("as_float".to_string(), span));

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::Int(size, _)), _) =
        &function.ast.body.stmts[1]
    else {
        panic!("expected lowered union sizeof");
    };
    assert_eq!(*size, 8);
}

#[test]
fn ts_memory_lowering_rewrites_bitfield_field_access_and_store() {
    let span = kain_core::span::Span::default();
    let int_ty = Type::Named {
        name: "Int".to_string(),
        generics: Vec::new(),
        span,
    };
    let flags_ty = Type::Named {
        name: "Flags".to_string(),
        generics: Vec::new(),
        span,
    };

    let program = kain_core::ast::Program {
        items: vec![
            kain_core::ast::Item::Struct(kain_core::ast::Struct {
                name: "Flags".to_string(),
                generics: Vec::new(),
                fields: vec![
                    kain_core::ast::Field {
                        name: "ready".to_string(),
                        ty: int_ty.clone(),
                        attributes: vec![usize_attr(C_BITFIELD_ATTR, 1, span)],
                        visibility: kain_core::ast::Visibility::Public,
                        default: None,
                        weak: false,
                        span,
                    },
                    kain_core::ast::Field {
                        name: "mode".to_string(),
                        ty: int_ty.clone(),
                        attributes: vec![usize_attr(C_BITFIELD_ATTR, 3, span)],
                        visibility: kain_core::ast::Visibility::Public,
                        default: None,
                        weak: false,
                        span,
                    },
                ],
                methods: Vec::new(),
                attributes: Vec::new(),
                visibility: kain_core::ast::Visibility::Public,
                span,
            }),
            kain_core::ast::Item::Function(kain_core::ast::Function {
                name: "update".to_string(),
                generics: Vec::new(),
                params: vec![kain_core::ast::Param {
                    name: "f".to_string(),
                    ty: flags_ty.clone(),
                    mutable: true,
                    default: None,
                    span,
                }],
                return_type: Some(int_ty.clone()),
                effects: Vec::new(),
                body: kain_core::ast::Block {
                    stmts: vec![
                        kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Assign {
                            target: Box::new(kain_core::ast::Expr::Field {
                                object: Box::new(kain_core::ast::Expr::Ident("f".to_string(), span)),
                                field: "mode".to_string(),
                                span,
                            }),
                            value: Box::new(kain_core::ast::Expr::Int(5, span)),
                            span,
                        }),
                        kain_core::ast::Stmt::Return(
                            Some(kain_core::ast::Expr::Field {
                                object: Box::new(kain_core::ast::Expr::Ident("f".to_string(), span)),
                                field: "ready".to_string(),
                                span,
                            }),
                            span,
                        ),
                    ],
                    span,
                },
                visibility: kain_core::ast::Visibility::Public,
                attributes: Vec::new(),
                span,
            }),
        ],
        span,
    };

    let mapper = SpanMapper::new("");
    let typed = kain_core::types::check(&program, &mapper, "bitfield_lowering.kn").expect("typecheck");
    let lowered = lower_typed_program_memory_for_target(&typed, CompileTarget::Ts).expect("lower");

    let function = match &lowered.items[1] {
        kain_core::types::TypedItem::Function(function) => function,
        other => panic!("expected function, got {other:?}"),
    };

    let kain_core::ast::Stmt::Expr(kain_core::ast::Expr::Call { callee, .. }) =
        &function.ast.body.stmts[0]
    else {
        panic!("expected bitfield set helper");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_bitfield_set"));

    let kain_core::ast::Stmt::Return(Some(kain_core::ast::Expr::Call { callee, .. }), _) =
        &function.ast.body.stmts[1]
    else {
        panic!("expected bitfield get helper");
    };
    assert!(matches!(callee.as_ref(), kain_core::ast::Expr::Ident(name, _) if name == "__kain_bitfield_get"));
}
