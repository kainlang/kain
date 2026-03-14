use kain_core::*;

fn parse_program(source: &str) -> Result<ast::Program, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    parser::Parser::new(&tokens, &span_mapper, "<test>").parse()
}

#[test]
fn parser_accepts_inline_mod_items() {
    let source = r#"pub mod math:
    fn add(a: Int, b: Int) -> Int:
        return a + b
"#;

    let program = parse_program(source).expect("inline mod should parse");
    assert_eq!(program.items.len(), 1);

    match &program.items[0] {
        ast::Item::Mod(module) => {
            assert_eq!(module.name, "math");
            assert!(matches!(module.visibility, ast::Visibility::Public));
            let inline = module
                .inline
                .as_ref()
                .expect("module should have inline items");
            assert_eq!(inline.len(), 1);
            assert!(matches!(inline[0], ast::Item::Function(_)));
        }
        other => panic!("expected module item, got {:?}", other),
    }
}

#[test]
fn parser_accepts_declaration_mod_items() {
    let source = r#"mod os

fn main() -> Int:
    return 1
"#;

    let program = parse_program(source).expect("mod declaration should parse");
    assert_eq!(program.items.len(), 2);

    match &program.items[0] {
        ast::Item::Mod(module) => {
            assert_eq!(module.name, "os");
            assert!(module.inline.is_none());
        }
        other => panic!("expected module item, got {:?}", other),
    }
    assert!(matches!(program.items[1], ast::Item::Function(_)));
}

#[test]
fn parser_accepts_qualified_variant_patterns_in_match_arms() {
    let source = r#"fn binary_op_to_string(op: kain_core::ast::BinaryOp) -> String:
    match op:
        kain_core__ast__BinaryOp::Gt => ">"
        kain_core__ast__BinaryOp::Le => "<="
"#;

    let program = parse_program(source).expect("qualified match arms should parse");
    let function = match &program.items[0] {
        ast::Item::Function(function) => function,
        other => panic!("expected function, got {:?}", other),
    };

    let ast::Stmt::Expr(ast::Expr::Match { arms, .. }) = &function.body.stmts[0] else {
        panic!("expected match expression in function body");
    };

    assert_eq!(arms.len(), 2);
    assert!(matches!(
        &arms[0].pattern,
        ast::Pattern::Variant {
            enum_name: Some(enum_name),
            variant,
            fields: ast::VariantPatternFields::Unit,
            ..
        } if enum_name == "kain_core__ast__BinaryOp" && variant == "Gt"
    ));
    assert!(matches!(
        &arms[1].pattern,
        ast::Pattern::Variant {
            enum_name: Some(enum_name),
            variant,
            fields: ast::VariantPatternFields::Unit,
            ..
        } if enum_name == "kain_core__ast__BinaryOp" && variant == "Le"
    ));
}

#[test]
fn parser_accepts_qualified_enum_variant_expressions() {
    let source = r#"fn build_root(root_component: &String) -> Any:
    return crate::ast::JSXNode::ComponentCall { name: root_component.clone(), props: [], children: [], span: none }
"#;

    let program = parse_program(source).expect("qualified enum variant expressions should parse");
    let function = match &program.items[0] {
        ast::Item::Function(function) => function,
        other => panic!("expected function, got {:?}", other),
    };

    let ast::Stmt::Return(Some(ast::Expr::EnumVariant {
        enum_name,
        variant,
        fields: ast::EnumVariantFields::Struct(fields),
        ..
    }), _) = &function.body.stmts[0]
    else {
        panic!("expected qualified enum variant return");
    };

    assert_eq!(enum_name, "crate::ast::JSXNode");
    assert_eq!(variant, "ComponentCall");
    assert_eq!(fields.len(), 4);
}

#[test]
fn parser_accepts_qualified_function_calls() {
    let source = r#"fn call_it() -> Int:
    return foo::bar(1)
"#;

    let program = parse_program(source).expect("qualified function calls should parse");
    let function = match &program.items[0] {
        ast::Item::Function(function) => function,
        other => panic!("expected function, got {:?}", other),
    };

    let ast::Stmt::Return(Some(ast::Expr::Call { callee, args, .. }), _) = &function.body.stmts[0]
    else {
        panic!("expected call return");
    };

    assert!(matches!(
        callee.as_ref(),
        ast::Expr::Ident(name, _) if name == "foo::bar"
    ));
    assert_eq!(args.len(), 1);
}
