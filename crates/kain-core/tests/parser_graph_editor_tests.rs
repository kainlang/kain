use kain_core::parser::Parser;
use kain_core::lexer::Lexer;
use kain_core::ast::*;

// Helper function to check if a Type is a simple named type
fn is_named_type(ty: &Type, expected_name: &str) -> bool {
    match ty {
        Type::Named { name, generics, .. } => {
            name == expected_name && generics.is_empty()
        }
        _ => false,
    }
}

#[test]
fn test_simple_graph_editor() {
    let source = r#"
@graph_editor
graph CombatGraph:
    @node_type
    @category("Combat/Input")
    node InputNode:
        outputs:
            Execute: Exec
            Damage: Float = 10.0
"#;
    
    let lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    // Verify it's a GraphEditor
    match &program.items[0] {
        Item::GraphEditor(graph) => {
            assert_eq!(graph.name, "CombatGraph");
            assert_eq!(graph.node_types.len(), 1);
            
            let node = &graph.node_types[0];
            assert_eq!(node.name, "InputNode");
            assert_eq!(node.category, Some("Combat/Input".to_string()));
            assert_eq!(node.outputs.len(), 2);
            
            // Check Execute pin
            assert_eq!(node.outputs[0].name, "Execute");
            assert!(is_named_type(&node.outputs[0].ty, "Exec"));
            assert!(node.outputs[0].default.is_none());
            
            // Check Damage pin with default
            assert_eq!(node.outputs[1].name, "Damage");
            assert!(is_named_type(&node.outputs[1].ty, "Float"));
            assert!(node.outputs[1].default.is_some());
        }
        _ => panic!("Expected GraphEditor item"),
    }
}

#[test]
fn test_graph_with_multiple_nodes() {
    let source = r#"
@graph_editor
graph TestGraph:
    @node_type
    node InputNode:
        outputs:
            Execute: Exec
    
    @node_type
    node OutputNode:
        inputs:
            Execute: Exec
"#;
    
    let lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    match &program.items[0] {
        Item::GraphEditor(graph) => {
            assert_eq!(graph.name, "TestGraph");
            assert_eq!(graph.node_types.len(), 2);
            
            // First node
            assert_eq!(graph.node_types[0].name, "InputNode");
            assert_eq!(graph.node_types[0].outputs.len(), 1);
            
            // Second node
            assert_eq!(graph.node_types[1].name, "OutputNode");
            assert_eq!(graph.node_types[1].inputs.len(), 1);
        }
        _ => panic!("Expected GraphEditor item"),
    }
}

#[test]
fn test_graph_with_properties() {
    let source = r#"
@graph_editor
graph PropertyGraph:
    @node_type
    node ConfigNode:
        properties:
            Speed: Float = 1.0
            Name: String = "Default"
        outputs:
            Value: Float
"#;
    
    let lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    match &program.items[0] {
        Item::GraphEditor(graph) => {
            assert_eq!(graph.node_types.len(), 1);
            
            let node = &graph.node_types[0];
            assert_eq!(node.properties.len(), 2);
            
            // Check Speed property
            assert_eq!(node.properties[0].name, "Speed");
            assert!(is_named_type(&node.properties[0].ty, "Float"));
            assert!(node.properties[0].default.is_some());
            
            // Check Name property
            assert_eq!(node.properties[1].name, "Name");
            assert!(is_named_type(&node.properties[1].ty, "String"));
            assert!(node.properties[1].default.is_some());
        }
        _ => panic!("Expected GraphEditor item"),
    }
}

#[test]
fn test_graph_with_array_pins() {
    let source = r#"
@graph_editor
graph ArrayGraph:
    @node_type
    node ArrayNode:
        inputs:
            Items: Array<Int>
        outputs:
            Result: Array<Float>
"#;
    
    let lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    match &program.items[0] {
        Item::GraphEditor(graph) => {
            let node = &graph.node_types[0];
            
            // Check input array
            assert_eq!(node.inputs[0].name, "Items");
            assert!(node.inputs[0].is_array);
            
            // Check output array
            assert_eq!(node.outputs[0].name, "Result");
            assert!(node.outputs[0].is_array);
        }
        _ => panic!("Expected GraphEditor item"),
    }
}

#[test]
fn test_graph_with_schema() {
    let source = r#"
@graph_editor
graph SchemaGraph:
    @node_type
    node TestNode:
        outputs:
            Value: Float
    
    @schema
    schema:
        no_cycles: true
        max_depth: 10
"#;
    
    let lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    match &program.items[0] {
        Item::GraphEditor(graph) => {
            assert!(graph.schema.is_some());
            
            let schema = graph.schema.as_ref().unwrap();
            assert_eq!(schema.rules.len(), 2);
            
            assert_eq!(schema.rules[0].name, "no_cycles");
            assert_eq!(schema.rules[1].name, "max_depth");
        }
        _ => panic!("Expected GraphEditor item"),
    }
}

#[test]
fn test_complex_graph_editor() {
    let source = r#"
@graph_editor
graph ComplexGraph:
    @node_type
    @category("Input")
    node StartNode:
        outputs:
            Execute: Exec
            Value: Float = 0.0
    
    @node_type
    @category("Logic")
    node ProcessNode:
        inputs:
            Execute: Exec
            Input: Float
        properties:
            Multiplier: Float = 2.0
            Enabled: Bool = true
        outputs:
            Execute: Exec
            Result: Float
    
    @node_type
    @category("Output")
    node EndNode:
        inputs:
            Execute: Exec
            FinalValue: Float
"#;
    
    let lexer = Lexer::new(source);
    let tokens = lexer.tokenize().unwrap();
    let mut parser = Parser::new(&tokens);
    let program = parser.parse().unwrap();
    
    assert_eq!(program.items.len(), 1);
    
    match &program.items[0] {
        Item::GraphEditor(graph) => {
            assert_eq!(graph.name, "ComplexGraph");
            assert_eq!(graph.node_types.len(), 3);
            
            // Verify categories
            assert_eq!(graph.node_types[0].category, Some("Input".to_string()));
            assert_eq!(graph.node_types[1].category, Some("Logic".to_string()));
            assert_eq!(graph.node_types[2].category, Some("Output".to_string()));
            
            // Verify ProcessNode has both inputs, properties, and outputs
            let process_node = &graph.node_types[1];
            assert_eq!(process_node.inputs.len(), 2);
            assert_eq!(process_node.properties.len(), 2);
            assert_eq!(process_node.outputs.len(), 2);
        }
        _ => panic!("Expected GraphEditor item"),
    }
}
