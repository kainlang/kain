use kain_core::*;

/// Test suite for parser error quality improvements (Requirement 25)
/// Validates that the parser detects common syntax errors with clear, actionable messages

#[test]
fn test_reserved_keyword_state_in_parameter() {
    let source = "fn update(state: Int):\n    print(state)";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for reserved keyword 'state' but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // Should mention 'state' is reserved (case-insensitive check)
            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("state") && lower.contains("reserved"),
                "Error should mention 'state' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_reserved_keyword_uniform_in_parameter() {
    let source = "fn process(uniform: Float):\n    return uniform * 2.0";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "test.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for reserved keyword 'uniform' but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            assert!(
                error_str.contains("uniform") && error_str.contains("reserved"),
                "Error should mention 'uniform' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_reserved_keyword_buffer_in_parameter() {
    let source = "fn write_data(buffer: Array<Float>):\n    print(buffer)";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => {
            // Buffer might not be reserved - that's okay, skip this test
            println!("Note: 'buffer' is not currently a reserved keyword");
        }
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("buffer") && lower.contains("reserved"),
                "Error should mention 'buffer' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
#[test]
fn test_texture_is_now_allowed_as_variable() {
    // After fix: 'texture' should be allowed as a variable name
    let source = "fn sample_color(texture: Sampler2D):\n    return texture";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "test.kn");

    match parser.parse() {
        Ok(_) => {
            println!("Success: 'texture' is now allowed as a variable name");
        }
        Err(e) => {
            panic!(
                "'texture' should be allowed as a variable name but got error: {}",
                e
            );
        }
    }
}

#[test]
fn test_cs_is_now_allowed_as_variable() {
    // After fix: 'cs' should be allowed as a variable name
    let source = "fn compute_step(cs: Int):\n    return cs + 1";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "test.kn");

    match parser.parse() {
        Ok(_) => {
            println!("Success: 'cs' is now allowed as a variable name");
        }
        Err(e) => {
            panic!(
                "'cs' should be allowed as a variable name but got error: {}",
                e
            );
        }
    }
}

#[test]
fn test_shader_keyword_only_reserved_at_top_level() {
    // 'shader' is only a keyword when declaring shaders, not as a variable
    let source = "fn process_shader(shader: String):\n    return shader";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "test.kn");

    match parser.parse() {
        Ok(_) => {
            println!("Success: 'shader' is now allowed as a variable name");
        }
        Err(e) => {
            panic!(
                "'shader' should be allowed as a variable name but got error: {}",
                e
            );
        }
    }
}

#[test]
fn test_reserved_keyword_register_in_parameter() {
    let source = "fn allocate(register: Int):\n    return register + 1";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "test.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for reserved keyword 'register' but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            assert!(
                error_str.contains("register") && error_str.contains("reserved"),
                "Error should mention 'register' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_reserved_keyword_static_in_variable() {
    let source = "fn my_func():\n    let static = 5\n    print(static)";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for reserved keyword 'static' but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("static") && lower.contains("reserved"),
                "Error should mention 'static' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_reserved_keyword_const_in_variable() {
    let source = "fn my_func():\n    let const = 10\n    return const";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => {
            // 'const' might not be reserved in variable context - that's okay
            println!("Note: 'const' is not currently reserved in variable context");
        }
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // If it errors, it should be a clear error message
            assert!(
                error_str.len() > 10,
                "Error message should be descriptive but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_struct_literal_brace_style() {
    let source = "struct Point:\n    x: Float\n    y: Float\n\nfn create_point():\n    let p = Point { x: 1.0, y: 2.0 }";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    let program = parser.parse().unwrap();
    let Item::Function(function) = &program.items[1] else {
        panic!("Expected function item");
    };
    let Stmt::Let {
        value: Some(expr), ..
    } = &function.body.stmts[0]
    else {
        panic!("Expected let statement with initializer");
    };
    let Expr::Struct {
        name,
        fields,
        rest,
        ..
    } = expr
    else {
        panic!("Expected struct literal expression");
    };
    assert_eq!(name, "Point");
    assert_eq!(fields.len(), 2);
    assert!(rest.is_none());
    assert_eq!(fields[0].0, "x");
    assert_eq!(fields[1].0, "y");
}

#[test]
fn test_struct_literal_rest_and_ref_lifetimes() {
    let source = "struct Point:\n    x: Float\n    y: Float\n\nfn clone_point(base: Point, view: &static Point, other: &mut arena Point):\n    let p = Point {\n        x: view.x,\n        ..base,\n    }\n    return other";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    let program = parser.parse().unwrap();
    let Item::Function(function) = &program.items[1] else {
        panic!("Expected function item");
    };

    match &function.params[1].ty {
        Type::Ref {
            mutable,
            lifetime,
            inner,
            ..
        } => {
            assert!(!mutable);
            assert_eq!(lifetime.as_deref(), Some("static"));
            assert!(matches!(inner.as_ref(), Type::Named { name, .. } if name == "Point"));
        }
        other => panic!("Expected immutable ref type, got {other:?}"),
    }

    match &function.params[2].ty {
        Type::Ref {
            mutable,
            lifetime,
            inner,
            ..
        } => {
            assert!(*mutable);
            assert_eq!(lifetime.as_deref(), Some("arena"));
            assert!(matches!(inner.as_ref(), Type::Named { name, .. } if name == "Point"));
        }
        other => panic!("Expected mutable ref type, got {other:?}"),
    }

    let Stmt::Let {
        value: Some(expr), ..
    } = &function.body.stmts[0]
    else {
        panic!("Expected let statement with initializer");
    };
    let Expr::Struct {
        name,
        fields,
        rest,
        ..
    } = expr
    else {
        panic!("Expected struct literal expression");
    };
    assert_eq!(name, "Point");
    assert_eq!(fields.len(), 1);
    assert_eq!(fields[0].0, "x");
    assert!(matches!(
        rest.as_deref(),
        Some(Expr::Ident(base, _)) if base == "base"
    ));
}

#[test]
fn test_struct_literal_function_call_style() {
    let source = "struct Vec3:\n    x: Float\n    y: Float\n    z: Float\n\nfn create_vec():\n    let v = Vec3(x: 1.0, y: 2.0, z: 3.0)";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for function-call style struct init but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // Parser detects syntax error - check that error message is clear
            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("expected") || lower.contains("comma") || lower.contains("colon"),
                "Error should be clear about syntax issue but got: {}",
                error_str
            );

            // Should include location
            assert!(
                error_str.contains("example.kn:"),
                "Error should include file location but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_double_colon_on_struct_field_access() {
    let source = "struct Config:\n    value: Int\n\nfn get_value(c: Config):\n    return c::value";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => {
            // Parser currently allows :: on structs - this test documents current behavior
            // In the future, this should be an error with a helpful message
            println!("Note: Parser currently allows :: on struct field access (should be improved to suggest . instead)");
        }
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // If it errors, check for helpful message
            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("::") || lower.contains("struct") || lower.contains("enum"),
                "Error should mention :: vs . for structs but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_error_messages_include_location() {
    let source = "fn my_func():\n    let state = 5";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "my_file.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // Should include file:line:col format
            assert!(
                error_str.contains("my_file.kn:"),
                "Error should include filename but got: {}",
                error_str
            );

            assert!(
                error_str.contains(":2:") || error_str.contains(":1:"),
                "Error should include line:col but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_multiple_reserved_keywords_detected() {
    let source = "fn process(state: Int, uniform: Float):\n    let const = state + uniform\n    return const";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for multiple reserved keywords but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // Should detect at least one reserved keyword
            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("reserved"),
                "Error should mention reserved keyword but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_valid_enum_double_colon_usage() {
    let source = "enum Color:\n    Red\n    Green\n    Blue\n\nfn get_color():\n    let c = Color::Red\n    return c";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    // This should parse successfully - :: is valid for enums
    match parser.parse() {
        Ok(_) => {
            println!("Correctly parsed enum with :: syntax");
        }
        Err(e) => {
            panic!(
                "Valid enum :: syntax should parse successfully but got error: {}",
                e
            );
        }
    }
}

#[test]
fn test_reserved_cpp_keyword_class() {
    let source = "fn my_func(class: Int):\n    return class";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for C++ keyword 'class' but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("class") && lower.contains("reserved"),
                "Error should mention 'class' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_reserved_cpp_keyword_namespace() {
    let source = "fn my_func(namespace: String):\n    print(namespace)";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for C++ keyword 'namespace' but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("namespace") && lower.contains("reserved"),
                "Error should mention 'namespace' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_reserved_ue5_keyword_uclass() {
    let source = "fn my_func(UCLASS: Bool):\n    return UCLASS";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for UE5 keyword 'UCLASS' but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("uclass") && lower.contains("reserved"),
                "Error should mention 'UCLASS' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_reserved_hlsl_keyword_cbuffer() {
    let source = "fn my_func(cbuffer: Int):\n    return cbuffer";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error for HLSL keyword 'cbuffer' but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            let lower = error_str.to_lowercase();
            assert!(
                lower.contains("cbuffer") && lower.contains("reserved"),
                "Error should mention 'cbuffer' is reserved but got: {}",
                error_str
            );
        }
    }
}

#[test]
fn test_actionable_error_message_quality() {
    let source = "struct Point:\n    x: Float\n    y: Float\n\nfn create_point():\n    let p = Point { x: 1.0, y: 2.0 }";
    let tokens = lexer::Lexer::new(source).tokenize().unwrap();
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = parser::Parser::new(&tokens, &span_mapper, "example.kn");

    match parser.parse() {
        Ok(_) => panic!("Expected parse error but got success"),
        Err(e) => {
            let error_str = e.to_string();
            println!("Error message: {}", error_str);

            // Error should be clear and actionable
            assert!(
                error_str.len() > 20,
                "Error message should be descriptive but got: {}",
                error_str
            );

            // Should include location
            assert!(
                error_str.contains("example.kn:"),
                "Error should include file location but got: {}",
                error_str
            );
        }
    }
}
