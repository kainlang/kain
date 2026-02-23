use kain_core::{lexer::Lexer, parser::Parser, ast::*, diagnostics::SpanMapper};

#[test]
fn test_parse_replicated_attribute_basic() {
    let source = r#"
@component
struct NetworkedTransform:
    @replicated(mode: Interpolated, back_time: 0.1)
    position: Vec3
"#;
    
    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let span_mapper = SpanMapper::new(source);let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    // Check that it's a struct with @component attribute
    if let Item::Struct(s) = &program.items[0] {
        assert_eq!(s.name, "NetworkedTransform");
        assert!(s.attributes.iter().any(|a| a.name == "component"));
        assert_eq!(s.fields.len(), 1);
        
        // Check the field has @replicated attribute
        let field = &s.fields[0];
        assert_eq!(field.name, "position");
        assert_eq!(field.attributes.len(), 1);
        
        let attr = &field.attributes[0];
        assert_eq!(attr.name, "replicated");
        assert_eq!(attr.args.len(), 2); // mode: Interpolated, back_time: 0.1
    } else {
        panic!("Expected Struct item with @component attribute");
    }
}

#[test]
fn test_parse_replicated_modes() {
    let source = r#"
@component
struct NetworkedTransform:
    @replicated(mode: Interpolated, back_time: 0.1)
    position: Vec3
    
    @replicated(mode: Extrapolated, limit: 1.0)
    predicted_position: Vec3
    
    @replicated(mode: Compressed, threshold: 0.01)
    velocity: Vec3
    
    @replicated(mode: Simple)
    health: Float
"#;
    
    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let span_mapper = SpanMapper::new(source);let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    if let Item::Struct(s) = &program.items[0] {
        assert_eq!(s.fields.len(), 4);
        
        // Check each field has @replicated attribute
        for field in &s.fields {
            assert!(field.attributes.iter().any(|a| a.name == "replicated"));
        }
    } else {
        panic!("Expected Struct item with @component attribute");
    }
}

#[test]
fn test_parse_network_config() {
    let source = r#"
@component
struct NetworkedTransform:
    @replicated(mode: Interpolated)
    position: Vec3
    
    @config
    snap_threshold: Float = 500.0
    
    @config
    send_rate: Float = 10.0
"#;
    
    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let span_mapper = SpanMapper::new(source);let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    if let Item::Struct(s) = &program.items[0] {
        assert_eq!(s.fields.len(), 3);
        
        // Check config fields
        let config_fields: Vec<_> = s.fields.iter()
            .filter(|f| f.attributes.iter().any(|a| a.name == "config"))
            .collect();
        assert_eq!(config_fields.len(), 2);
    } else {
        panic!("Expected Struct item with @component attribute");
    }
}
