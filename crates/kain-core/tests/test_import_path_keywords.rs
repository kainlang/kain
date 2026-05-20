use kain_core::{diagnostics, lexer, parser, Item};

#[test]
fn use_import_allows_reserved_namespace_segments() {
    let source = "use std::graphics::shared\nfn main() -> Int:\n    return 0\n";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "import_keywords.kn");

    let program = parser
        .parse()
        .expect("reserved path segments should parse inside import namespaces");

    let import = program
        .items
        .iter()
        .find_map(|item| match item {
            Item::Use(import) => Some(import),
            _ => None,
        })
        .expect("expected use item");

    assert_eq!(
        import.path,
        vec![
            "std".to_string(),
            "graphics".to_string(),
            "shared".to_string()
        ]
    );
}
