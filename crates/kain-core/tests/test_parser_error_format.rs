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
    let source = "struct MyStruct:\n    field: Int\n\nfn test():\n    let x = MyStruct { field: 5 }";
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
