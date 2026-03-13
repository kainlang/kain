use kain_core::{ast::*, diagnostics::SpanMapper, lexer::Lexer, parser::Parser};

#[test]
fn test_parse_graph_runtime_basic() {
    let source = r#"
@graph_runtime
struct CombatGraph:
    @node_data
    struct ActionNode:
        @property
        action_class: Class
        
        @input_pin
        execute: Exec
        
        @output_pin
        success: Exec
"#;

    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().expect("Failed to tokenize");
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().expect("Failed to parse");

    assert_eq!(program.items.len(), 1);

    match &program.items[0] {
        Item::GraphRuntime(graph_def) => {
            assert_eq!(graph_def.name, "CombatGraph");
            assert_eq!(graph_def.node_types.len(), 1);

            let node = &graph_def.node_types[0];
            assert_eq!(node.name, "ActionNode");
            assert_eq!(node.properties.len(), 1);
            assert_eq!(node.input_pins.len(), 1);
            assert_eq!(node.output_pins.len(), 1);
        }
        _ => panic!("Expected GraphRuntime item"),
    }
}

#[test]
fn test_parse_graph_runtime_with_instance() {
    let source = r#"
@graph_runtime
struct TestGraph:
    @instance
    struct Instance:
        current_node: NodeData
        
        fn trigger() -> Bool:
            return true
"#;

    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().expect("Failed to tokenize");
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().expect("Failed to parse");

    assert_eq!(program.items.len(), 1);

    match &program.items[0] {
        Item::GraphRuntime(graph_def) => {
            assert_eq!(graph_def.name, "TestGraph");
            assert!(graph_def.instance.is_some());

            let instance = graph_def.instance.as_ref().unwrap();
            assert_eq!(instance.state.len(), 1);
            assert_eq!(instance.methods.len(), 1);
        }
        _ => panic!("Expected GraphRuntime item"),
    }
}

#[test]
fn test_parse_node_data_with_pins() {
    let source = r#"
@graph_runtime
struct TestGraph:
    @node_data
    struct TestNode:
        @input_pin
        in_exec: Exec
        
        @input_pin
        value: Int
        
        @output_pin
        out_exec: Exec
        
        @output_pin
        result: Float
"#;

    let mut lexer = Lexer::new(source);
    let tokens = lexer.tokenize().expect("Failed to tokenize");
    let span_mapper = SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "<test>");
    let program = parser.parse().expect("Failed to parse");

    match &program.items[0] {
        Item::GraphRuntime(graph_def) => {
            let node = &graph_def.node_types[0];
            assert_eq!(node.input_pins.len(), 2);
            assert_eq!(node.output_pins.len(), 2);

            assert_eq!(node.input_pins[0].name, "in_exec");
            assert_eq!(node.input_pins[1].name, "value");
            assert_eq!(node.output_pins[0].name, "out_exec");
            assert_eq!(node.output_pins[1].name, "result");
        }
        _ => panic!("Expected GraphRuntime item"),
    }
}
