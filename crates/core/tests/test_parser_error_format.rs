use kain_core::*;

#[test]
fn test_parser_error_contains_file_line_col() {
    let source = "fn test():\n    let x = \n    print(x)";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "test.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // The error message should contain file:line:col format
            assert!(
                error_str.contains("test.kn:"),
                "Error message should contain 'test.kn:' but got: {}",
                error_str
            );

            // Should contain line and column numbers (format is file:line:col:)
            assert!(
                error_str.contains(":1:") || error_str.contains(":2:") || error_str.contains(":3:"),
                "Error message should contain line:col but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_parser_error_format_with_different_files() {
    let source =
        "struct MyStruct:\n    field: Int\n\nfn test():\n    let x = MyStruct { field: 5 }";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "my_module.kn");

    match parser.parse() {
        Ok(_) => {
            // This might succeed or fail depending on struct literal support
            println!("Parse succeeded");
        }
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // The error message should contain the correct filename
            assert!(
                error_str.contains("my_module.kn:"),
                "Error message should contain 'my_module.kn:' but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_missing_colon_before_newline_has_fixit_and_header_label() {
    let source = "fn demo()\n    let x = 1";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "test.kn");

    let err = parser
        .parse()
        .expect_err("missing function-header colon should fail");
    let rendered = diagnostics::Diagnostics::new(source, "test.kn").format_error(&err);

    assert!(
        rendered.contains("Missing ':' before line break"),
        "got: {rendered}"
    );
    assert!(rendered.contains("fix-it"));
    assert!(rendered.contains("insert ':'"));
    assert!(rendered.contains("this header or declaration ended without ':'"));
    assert!(
        rendered.contains("test.kn:1:"),
        "diagnostic should anchor the header line, got: {rendered}"
    );
}

#[test]
fn test_synthetic_import_scan_error_does_not_inline_fake_location() {
    let source = "fn demo()\n    let x = 1";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "<frontend-import-scan>");

    let err = parser
        .parse()
        .expect_err("missing function-header colon should fail");
    let raw = err.to_string();
    assert!(
        raw.contains("Missing ':' before line break"),
        "got raw diagnostic: {raw}"
    );
    assert!(
        !raw.contains("<frontend-import-scan>:"),
        "synthetic scanner origin must not masquerade as a source location: {raw}"
    );

    let rendered = diagnostics::Diagnostics::new(source, "real_main.kn").format_error(&err);
    assert!(rendered.contains("real_main.kn:1:"));
    assert!(rendered.contains("fix-it"));
}

#[test]
fn test_rich_parse_diagnostic_has_machine_readable_json() {
    let source = "fn demo()\n    let x = 1";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "json.kn");

    let err = parser
        .parse()
        .expect_err("missing function-header colon should fail");
    let json = err
        .diagnostic_json()
        .expect("rich parser diagnostics should expose JSON");
    assert_eq!(json["diagnostics"][0]["code"], "KAIN-PARSE-0005");
    assert_eq!(
        json["diagnostics"][0]["title"],
        "Missing Delimiter Before Newline"
    );
    assert_eq!(json["diagnostics"][0]["fixits"][0]["replacement"], ":");
    assert_eq!(json["diagnostics"][0]["primary_range"]["start"]["line"], 1);
}
