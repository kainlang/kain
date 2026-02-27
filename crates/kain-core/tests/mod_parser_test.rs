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
            let inline = module.inline.as_ref().expect("module should have inline items");
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
