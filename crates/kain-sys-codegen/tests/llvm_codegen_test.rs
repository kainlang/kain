use std::collections::HashMap;

use kain_core::ast::{
    BinaryOp, Block, Component, Expr, Field, Function, Impl, JSXAttrValue, JSXAttribute, JSXNode,
    MatchArm, Param, Pattern, Stmt, Struct, Type, Visibility,
};
use kain_core::diagnostics::SpanMapper;
use kain_core::effects::EffectSet;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::types;
use kain_core::types::{
    IntSize, ResolvedType, TypedActor, TypedComponent, TypedFunction, TypedImpl, TypedItem,
    TypedProgram, TypedStruct,
};
use kain_core::Span;
use kain_sys_codegen::generate_llvm;

fn span() -> Span {
    (0..0).into()
}

fn string_type() -> Type {
    Type::Named {
        name: "String".to_string(),
        generics: vec![],
        span: span(),
    }
}

fn int_type() -> Type {
    Type::Named {
        name: "Int".to_string(),
        generics: vec![],
        span: span(),
    }
}

fn float_type() -> Type {
    Type::Named {
        name: "Float".to_string(),
        generics: vec![],
        span: span(),
    }
}

fn typed_program_from_source(source: &str) -> TypedProgram {
    let tokens = Lexer::new(source).tokenize().expect("lexer should succeed");
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "<llvm-test>")
        .parse()
        .expect("parser should succeed");
    types::check(&program, &mapper, "<llvm-test>").expect("typecheck should succeed")
}

#[test]
fn llvm_generates_component_and_jsx_calls() {
    let hud_panel = TypedItem::Component(TypedComponent {
        ast: Component {
            name: "HudPanel".to_string(),
            props: vec![Param {
                name: "title".to_string(),
                ty: string_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            state: vec![],
            methods: vec![],
            effects: vec![],
            body: JSXNode::Element {
                tag: "div".to_string(),
                attributes: vec![JSXAttribute {
                    name: "class".to_string(),
                    value: JSXAttrValue::String("hud".to_string()),
                    span: span(),
                }],
                children: vec![
                    JSXNode::Text("Title: ".to_string(), span()),
                    JSXNode::Expression(Box::new(Expr::Ident("title".to_string(), span()))),
                    JSXNode::Text(" ".to_string(), span()),
                    JSXNode::Expression(Box::new(Expr::Ident("children".to_string(), span()))),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        prop_types: HashMap::from([("title".to_string(), ResolvedType::String)]),
    });

    let app_shell = TypedItem::Component(TypedComponent {
        ast: Component {
            name: "AppShell".to_string(),
            props: vec![],
            state: vec![],
            methods: vec![],
            effects: vec![],
            body: JSXNode::ComponentCall {
                name: "HudPanel".to_string(),
                props: vec![JSXAttribute {
                    name: "title".to_string(),
                    value: JSXAttrValue::String("Status".to_string()),
                    span: span(),
                }],
                children: vec![JSXNode::Text("Inner body".to_string(), span())],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        prop_types: HashMap::new(),
    });

    let program = TypedProgram {
        items: vec![hud_panel, app_shell],
    };

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define i8* @HudPanel(i8* %arg0, i8* %arg1)"));
    assert!(llvm.contains("define i8* @AppShell(i8* %arg0)"));
    assert!(llvm.contains("call i8* @HudPanel(i8*"));
    assert!(llvm.contains("<div"));
    assert!(llvm.contains("hud"));
    assert!(llvm.contains("Inner body"));
}

#[test]
fn llvm_generates_struct_array_and_fstring_paths() {
    let view_model = TypedItem::Struct(TypedStruct {
        ast: Struct {
            name: "ViewModel".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "value".to_string(),
                ty: int_type(),
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: span(),
            }],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: span(),
        },
        field_types: HashMap::from([("value".to_string(), ResolvedType::Int(IntSize::I64))]),
    });

    let make_view = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "make_view".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "n".to_string(),
                ty: int_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(string_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "model".to_string(),
                            mutable: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Struct {
                            name: "ViewModel".to_string(),
                            fields: vec![(
                                "value".to_string(),
                                Expr::Ident("n".to_string(), span()),
                            )],
                            span: span(),
                        }),
                        span: span(),
                    },
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "items".to_string(),
                            mutable: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Array(
                            vec![
                                Expr::Int(1, span()),
                                Expr::Int(2, span()),
                                Expr::Int(3, span()),
                            ],
                            span(),
                        )),
                        span: span(),
                    },
                    Stmt::Expr(Expr::Assign {
                        target: Box::new(Expr::Field {
                            object: Box::new(Expr::Ident("model".to_string(), span())),
                            field: "value".to_string(),
                            span: span(),
                        }),
                        value: Box::new(Expr::Binary {
                            left: Box::new(Expr::Field {
                                object: Box::new(Expr::Ident("model".to_string(), span())),
                                field: "value".to_string(),
                                span: span(),
                            }),
                            op: BinaryOp::Add,
                            right: Box::new(Expr::Index {
                                object: Box::new(Expr::Ident("items".to_string(), span())),
                                index: Box::new(Expr::Int(0, span())),
                                span: span(),
                            }),
                            span: span(),
                        }),
                        span: span(),
                    }),
                    Stmt::Return(
                        Some(Expr::FString(
                            vec![
                                Expr::String("value=".to_string(), span()),
                                Expr::Field {
                                    object: Box::new(Expr::Ident("model".to_string(), span())),
                                    field: "value".to_string(),
                                    span: span(),
                                },
                            ],
                            span(),
                        )),
                        span(),
                    ),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Int(IntSize::I64)],
            ret: Box::new(ResolvedType::String),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let program = TypedProgram {
        items: vec![view_model, make_view],
    };

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define i8* @make_view(i64 %arg0)"));
    assert!(llvm.contains("call i8* @KAIN_alloc(i64"));
    assert!(llvm.contains("call i8* @array_new(i64 4)"));
    assert!(llvm.contains("call i64 @array_get(i8*"));
    assert!(llvm.contains("call i8* @to_string(i64"));
    assert!(llvm.contains("call i8* @str_concat(i8*"));
}

#[test]
fn llvm_generates_impl_methods_and_method_calls() {
    let view_model = TypedItem::Struct(TypedStruct {
        ast: Struct {
            name: "ViewModel".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "value".to_string(),
                ty: int_type(),
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: span(),
            }],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: span(),
        },
        field_types: HashMap::from([("value".to_string(), ResolvedType::Int(IntSize::I64))]),
    });

    let typed_impl = TypedItem::Impl(TypedImpl {
        ast: Impl {
            generics: vec![],
            trait_name: None,
            target_type: Type::Named {
                name: "ViewModel".to_string(),
                generics: vec![],
                span: span(),
            },
            methods: vec![Function {
                name: "increment".to_string(),
                generics: vec![],
                params: vec![Param {
                    name: "delta".to_string(),
                    ty: int_type(),
                    mutable: false,
                    default: None,
                    span: span(),
                }],
                return_type: Some(int_type()),
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::Return(
                        Some(Expr::Binary {
                            left: Box::new(Expr::Field {
                                object: Box::new(Expr::Ident("self".to_string(), span())),
                                field: "value".to_string(),
                                span: span(),
                            }),
                            op: BinaryOp::Add,
                            right: Box::new(Expr::Ident("delta".to_string(), span())),
                            span: span(),
                        }),
                        span(),
                    )],
                    span: span(),
                },
                visibility: Visibility::Public,
                attributes: vec![],
                span: span(),
            }],
            span: span(),
        },
    });

    let call_method = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "call_increment".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "model".to_string(),
                ty: Type::Named {
                    name: "ViewModel".to_string(),
                    generics: vec![],
                    span: span(),
                },
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::MethodCall {
                        receiver: Box::new(Expr::Ident("model".to_string(), span())),
                        method: "increment".to_string(),
                        args: vec![kain_core::ast::CallArg {
                            name: None,
                            value: Expr::Int(5, span()),
                            span: span(),
                        }],
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Struct(
                "ViewModel".to_string(),
                HashMap::new(),
            )],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let program = TypedProgram {
        items: vec![view_model, typed_impl, call_method],
    };

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define i64 @ViewModel_increment(%ViewModel* %arg0, i64 %arg1)"));
    assert!(llvm.contains("call i64 @ViewModel_increment(%ViewModel*"));
}

#[test]
fn llvm_consumes_lowered_memory_helpers_into_pointer_ir() {
    let typed = typed_program_from_source(
        "fn mutate() -> Int:\n    let mut x: Int = 1\n    let p: ptr<Int> = addr_of(x, \"Int\")\n    mem_store(p, 7, \"Int\")\n    return x\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("ptrtoint i64* %x.addr_"));
    assert!(llvm.contains("inttoptr i64 "));
    assert!(llvm.contains("store i64 7, i64* %"));
    assert!(llvm.contains("load i64, i64* %"));
    assert!(!llvm.contains("@__kain_mem_store"));
    assert!(!llvm.contains("@__kain_mem_load"));
}

#[test]
fn llvm_consumes_lowered_alloc_and_realloc_helpers() {
    let typed = typed_program_from_source(
        "fn heap(n: Int, p: ptr<Int>) -> Int:\n    let mut q: ptr<Int> = alloc((n * sizeof_type(\"Int\")), \"Int\")\n    let mut r: ptr<Int> = realloc_mem(p, (n * sizeof_type(\"Int\")), \"Int\")\n    return 0\n",
    );

    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("call i8* @KAIN_alloc(i64"));
    assert!(llvm.contains("ptrtoint i8*"));
    assert!(llvm.contains("phi i64"));
    assert!(!llvm.contains("@__kain_alloc"));
    assert!(!llvm.contains("@__kain_realloc"));
}

#[test]
fn llvm_generates_actor_spawn_and_send_message_paths() {
    let printer = TypedItem::Actor(TypedActor {
        ast: kain_core::ast::Actor {
            name: "Printer".to_string(),
            state: vec![kain_core::ast::StateDecl {
                name: "count".to_string(),
                ty: int_type(),
                initial: Expr::Int(0, span()),
                weak: false,
                attributes: vec![],
                span: span(),
            }],
            handlers: vec![kain_core::ast::MessageHandler {
                message_type: "Print".to_string(),
                params: vec![Param {
                    name: "value".to_string(),
                    ty: int_type(),
                    mutable: false,
                    default: None,
                    span: span(),
                }],
                body: Block {
                    stmts: vec![Stmt::Return(None, span())],
                    span: span(),
                },
                span: span(),
            }],
            methods: vec![],
            attributes: vec![],
            span: span(),
        },
        state_types: HashMap::from([("count".to_string(), ResolvedType::Int(IntSize::I64))]),
    });

    let drive = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "drive".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "actor".to_string(),
                            mutable: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Spawn {
                            actor: "Printer".to_string(),
                            init: vec![("count".to_string(), Expr::Int(1, span()))],
                            span: span(),
                        }),
                        span: span(),
                    },
                    Stmt::Expr(Expr::SendMsg {
                        target: Box::new(Expr::Ident("actor".to_string(), span())),
                        message: "Print".to_string(),
                        data: vec![("value".to_string(), Expr::Int(7, span()))],
                        span: span(),
                    }),
                    Stmt::Return(Some(Expr::Int(0, span())), span()),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let program = TypedProgram {
        items: vec![printer, drive],
    };

    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define void @Printer_run(i8* %arg)"));
    assert!(llvm.contains(
        "call void @KAIN_spawn(i8* bitcast (void (i8*)* @default_actor_run to i8*), i8*"
    ));
    assert!(llvm.contains("call void @mq_push(i8* "));
    assert!(llvm.contains("%Printer_Print = type { i64 }"));
}

#[test]
fn llvm_generates_float_arithmetic_and_comparisons() {
    let blend = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "blend".to_string(),
            generics: vec![],
            params: vec![
                Param {
                    name: "a".to_string(),
                    ty: float_type(),
                    mutable: false,
                    default: None,
                    span: span(),
                },
                Param {
                    name: "b".to_string(),
                    ty: float_type(),
                    mutable: false,
                    default: None,
                    span: span(),
                },
            ],
            return_type: Some(float_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::If {
                        condition: Box::new(Expr::Binary {
                            left: Box::new(Expr::Binary {
                                left: Box::new(Expr::Ident("a".to_string(), span())),
                                op: BinaryOp::Div,
                                right: Box::new(Expr::Ident("b".to_string(), span())),
                                span: span(),
                            }),
                            op: BinaryOp::Gt,
                            right: Box::new(Expr::Float(1.5, span())),
                            span: span(),
                        }),
                        then_branch: Block {
                            stmts: vec![Stmt::Expr(Expr::Binary {
                                left: Box::new(Expr::Ident("a".to_string(), span())),
                                op: BinaryOp::Pow,
                                right: Box::new(Expr::Float(2.0, span())),
                                span: span(),
                            })],
                            span: span(),
                        },
                        else_branch: Some(Box::new(kain_core::ast::ElseBranch::Else(Block {
                            stmts: vec![Stmt::Expr(Expr::Binary {
                                left: Box::new(Expr::Ident("a".to_string(), span())),
                                op: BinaryOp::Mod,
                                right: Box::new(Expr::Ident("b".to_string(), span())),
                                span: span(),
                            })],
                            span: span(),
                        }))),
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![
                ResolvedType::Float(kain_core::types::FloatSize::F64),
                ResolvedType::Float(kain_core::types::FloatSize::F64),
            ],
            ret: Box::new(ResolvedType::Float(kain_core::types::FloatSize::F64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let program = TypedProgram { items: vec![blend] };
    let llvm = String::from_utf8(generate_llvm(&program).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("define double @blend(double %arg0, double %arg1)"));
    assert!(llvm.contains("fdiv double"));
    assert!(llvm.contains("fcmp ogt double"));
    assert!(llvm.contains("call double @pow(double"));
    assert!(llvm.contains("frem double"));
}

#[test]
fn llvm_generates_integer_mod_and_bitwise_ops() {
    let source = r#"
fn bit_ops(a: Int, b: Int) -> Int:
    let c = (a & b) | (a ^ b)
    return (c % 7) << 1
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains(" and i64 "));
    assert!(llvm.contains(" xor i64 "));
    assert!(llvm.contains(" or i64 "));
    assert!(llvm.contains(" srem i64 "));
    assert!(llvm.contains(" shl i64 "));
}

#[test]
fn llvm_generates_match_patterns_for_ranges_or_and_literals() {
    let classify_int = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "classify_int".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "value".to_string(),
                ty: int_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::Match {
                        scrutinee: Box::new(Expr::Ident("value".to_string(), span())),
                        arms: vec![
                            MatchArm {
                                pattern: Pattern::Range {
                                    start: Some(Box::new(Expr::Int(1, span()))),
                                    end: Some(Box::new(Expr::Int(3, span()))),
                                    inclusive: true,
                                    span: span(),
                                },
                                guard: None,
                                body: Expr::Int(10, span()),
                                span: span(),
                            },
                            MatchArm {
                                pattern: Pattern::Or(
                                    vec![
                                        Pattern::Literal(Expr::Int(4, span())),
                                        Pattern::Literal(Expr::Int(5, span())),
                                    ],
                                    span(),
                                ),
                                guard: None,
                                body: Expr::Int(20, span()),
                                span: span(),
                            },
                            MatchArm {
                                pattern: Pattern::Wildcard(span()),
                                guard: None,
                                body: Expr::Int(30, span()),
                                span: span(),
                            },
                        ],
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Int(IntSize::I64)],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let classify_flag = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "classify_flag".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "flag".to_string(),
                ty: Type::Named {
                    name: "Bool".to_string(),
                    generics: vec![],
                    span: span(),
                },
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::Match {
                        scrutinee: Box::new(Expr::Ident("flag".to_string(), span())),
                        arms: vec![
                            MatchArm {
                                pattern: Pattern::Literal(Expr::Bool(true, span())),
                                guard: None,
                                body: Expr::Int(1, span()),
                                span: span(),
                            },
                            MatchArm {
                                pattern: Pattern::Literal(Expr::Bool(false, span())),
                                guard: None,
                                body: Expr::Int(0, span()),
                                span: span(),
                            },
                        ],
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Bool],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let classify_name = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "classify_name".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "name".to_string(),
                ty: string_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::Match {
                        scrutinee: Box::new(Expr::Ident("name".to_string(), span())),
                        arms: vec![
                            MatchArm {
                                pattern: Pattern::Literal(Expr::String("hero".to_string(), span())),
                                guard: None,
                                body: Expr::Int(7, span()),
                                span: span(),
                            },
                            MatchArm {
                                pattern: Pattern::Wildcard(span()),
                                guard: None,
                                body: Expr::Int(9, span()),
                                span: span(),
                            },
                        ],
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::String],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![classify_int, classify_flag, classify_name],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("icmp sge i64"));
    assert!(llvm.contains("icmp sle i64"));
    assert!(llvm.contains(" or i1 "));
    assert!(llvm.contains("icmp eq i1"));
    assert!(llvm.contains("call i1 @deep_eq(i8*"));
}

#[test]
fn llvm_generates_tuple_values_and_tuple_destructuring() {
    let source = r#"
fn step() -> (Int, Int, Int):
    return (1, 2, 3)

fn unpack_sum() -> Int:
    let (a, b, c) = step()
    return a + b + c
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("%__kain_tuple_i64_i64_i64 = type { i64, i64, i64 }"));
    assert!(llvm.contains("define %__kain_tuple_i64_i64_i64* @step()"));
    assert!(llvm.contains("call i8* @KAIN_alloc(i64"));
    assert!(llvm.contains("getelementptr inbounds %__kain_tuple_i64_i64_i64"));
    assert!(llvm.contains("define i64 @unpack_sum()"));
}

#[test]
fn llvm_generates_indexed_array_assignment_and_readback() {
    let source = r#"
fn mutate_items() -> Int:
    let items = [1, 2, 3]
    items[1] = 42
    return items[1]
"#;

    let typed = typed_program_from_source(source);
    let llvm = String::from_utf8(generate_llvm(&typed).expect("llvm generation should succeed"))
        .expect("llvm output should be utf8");

    assert!(llvm.contains("call void @array_set(i8*"));
    assert!(llvm.contains("call i64 @array_get(i8*"));
    assert!(llvm.contains("define i64 @mutate_items()"));
}

#[test]
fn llvm_generates_struct_destructuring_patterns() {
    let point = TypedItem::Struct(TypedStruct {
        ast: Struct {
            name: "Point".to_string(),
            generics: vec![],
            fields: vec![
                Field {
                    name: "x".to_string(),
                    ty: int_type(),
                    attributes: vec![],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: span(),
                },
                Field {
                    name: "y".to_string(),
                    ty: int_type(),
                    attributes: vec![],
                    visibility: Visibility::Public,
                    default: None,
                    weak: false,
                    span: span(),
                },
            ],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: span(),
        },
        field_types: HashMap::from([
            ("x".to_string(), ResolvedType::Int(IntSize::I64)),
            ("y".to_string(), ResolvedType::Int(IntSize::I64)),
        ]),
    });

    let sum_point = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "sum_point".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "p".to_string(),
                            mutable: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Struct {
                            name: "Point".to_string(),
                            fields: vec![
                                ("x".to_string(), Expr::Int(4, span())),
                                ("y".to_string(), Expr::Int(5, span())),
                            ],
                            span: span(),
                        }),
                        span: span(),
                    },
                    Stmt::Let {
                        pattern: Pattern::Struct {
                            name: "Point".to_string(),
                            fields: vec![
                                (
                                    "x".to_string(),
                                    Pattern::Binding {
                                        name: "x".to_string(),
                                        mutable: false,
                                        span: span(),
                                    },
                                ),
                                (
                                    "y".to_string(),
                                    Pattern::Binding {
                                        name: "y".to_string(),
                                        mutable: false,
                                        span: span(),
                                    },
                                ),
                            ],
                            rest: false,
                            span: span(),
                        },
                        ty: None,
                        value: Some(Expr::Ident("p".to_string(), span())),
                        span: span(),
                    },
                    Stmt::Return(
                        Some(Expr::Binary {
                            left: Box::new(Expr::Ident("x".to_string(), span())),
                            op: BinaryOp::Add,
                            right: Box::new(Expr::Ident("y".to_string(), span())),
                            span: span(),
                        }),
                        span(),
                    ),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![point, sum_point],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("%Point = type { i64, i64 }"));
    assert!(llvm.contains("getelementptr inbounds %Point, %Point*"));
    assert!(llvm.contains("define i64 @sum_point()"));
}

#[test]
fn llvm_generates_raw_address_indexing_reads_and_writes() {
    let mutate_ptr = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "mutate_ptr".to_string(),
            generics: vec![],
            params: vec![Param {
                name: "ptr".to_string(),
                ty: int_type(),
                mutable: false,
                default: None,
                span: span(),
            }],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Expr(Expr::Assign {
                        target: Box::new(Expr::Index {
                            object: Box::new(Expr::Ident("ptr".to_string(), span())),
                            index: Box::new(Expr::Int(1, span())),
                            span: span(),
                        }),
                        value: Box::new(Expr::Int(99, span())),
                        span: span(),
                    }),
                    Stmt::Return(
                        Some(Expr::Index {
                            object: Box::new(Expr::Ident("ptr".to_string(), span())),
                            index: Box::new(Expr::Int(1, span())),
                            span: span(),
                        }),
                        span(),
                    ),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![ResolvedType::Int(IntSize::I64)],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![mutate_ptr],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("define i64 @mutate_ptr(i64 %arg0)"));
    assert!(llvm.contains("inttoptr i64"));
    assert!(llvm.contains("getelementptr inbounds i64, i64*"));
    assert!(llvm.contains("store i64 99, i64*"));
    assert!(llvm.contains("load i64, i64*"));
}

#[test]
fn llvm_lowers_tuple_aggregate_init_without_dummy_fallback() {
    let build_pair = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "build_pair".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(Type::Tuple(vec![int_type(), int_type()], span())),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::AggregateInit {
                        ty: Type::Tuple(vec![int_type(), int_type()], span()),
                        fields: vec![
                            ("0".to_string(), Expr::Int(10, span())),
                            ("1".to_string(), Expr::Int(20, span())),
                        ],
                        zero_fill_rest: false,
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Tuple(vec![
                ResolvedType::Int(IntSize::I64),
                ResolvedType::Int(IntSize::I64),
            ])),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![build_pair],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("%__kain_tuple_i64_i64 = type { i64, i64 }"));
    assert!(llvm.contains("define %__kain_tuple_i64_i64* @build_pair()"));
    assert!(llvm.contains("call i8* @KAIN_alloc(i64"));
    assert!(!llvm.contains("ret i64 0"));
}

#[test]
fn llvm_rejects_unsupported_expressions_instead_of_silent_dummy_values() {
    let bad_fn = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "bad_fn".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(
                    Some(Expr::Lambda {
                        params: vec![],
                        return_type: Some(int_type()),
                        body: Box::new(Expr::Int(1, span())),
                        span: span(),
                    }),
                    span(),
                )],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let err = generate_llvm(&TypedProgram {
        items: vec![bad_fn],
    })
    .expect_err("llvm generation should fail for unsupported expressions");

    let message = err.to_string();
    assert!(message.contains("Unsupported LLVM expression"));
    assert!(message.contains("Lambda"));
}

#[test]
fn llvm_lowers_typed_none_to_null_for_struct_pointer_flows() {
    let node = TypedItem::Struct(TypedStruct {
        ast: Struct {
            name: "Node".to_string(),
            generics: vec![],
            fields: vec![Field {
                name: "value".to_string(),
                ty: int_type(),
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: span(),
            }],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: span(),
        },
        field_types: HashMap::from([("value".to_string(), ResolvedType::Int(IntSize::I64))]),
    });

    let bind_none = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "bind_none".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(int_type()),
            effects: vec![],
            body: Block {
                stmts: vec![
                    Stmt::Let {
                        pattern: Pattern::Binding {
                            name: "node".to_string(),
                            mutable: true,
                            span: span(),
                        },
                        ty: Some(Type::Named {
                            name: "Node".to_string(),
                            generics: vec![],
                            span: span(),
                        }),
                        value: Some(Expr::None(span())),
                        span: span(),
                    },
                    Stmt::Return(Some(Expr::Int(0, span())), span()),
                ],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Int(IntSize::I64)),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let return_none = TypedItem::Function(TypedFunction {
        ast: Function {
            name: "return_none".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(Type::Named {
                name: "Node".to_string(),
                generics: vec![],
                span: span(),
            }),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Return(Some(Expr::None(span())), span())],
                span: span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: span(),
        },
        resolved_type: ResolvedType::Function {
            params: vec![],
            ret: Box::new(ResolvedType::Struct("Node".to_string(), HashMap::new())),
            effects: EffectSet::default(),
        },
        effects: EffectSet::default(),
    });

    let llvm = String::from_utf8(
        generate_llvm(&TypedProgram {
            items: vec![node, bind_none, return_none],
        })
        .expect("llvm generation should succeed"),
    )
    .expect("llvm output should be utf8");

    assert!(llvm.contains("%Node = type { i64 }"));
    assert!(llvm.contains("%node.addr_0 = alloca %Node*"));
    assert!(llvm.contains("store %Node* null, %Node** %node.addr_0"));
    assert!(llvm.contains("define %Node* @return_none()"));
    assert!(llvm.contains("ret %Node* null"));
}
