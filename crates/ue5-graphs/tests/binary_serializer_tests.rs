//! Binary Serializer Tests
//!
//! Comprehensive tests for the graph editor binary serializer

use ue5_graphs::{binary_serializer::serialize, GraphEditor, NodeType, PinDefinition, PinType};

#[test]
fn test_simple_graph_binary_format() {
    let mut graph = GraphEditor::new("SimpleGraph");

    graph.add_node_type(NodeType {
        name: "TestNode".to_string(),
        category: "Test".to_string(),
        inputs: vec![],
        outputs: vec![],
        properties: vec![],
        color: None,
        icon: None,
        tooltip: None,
        execution_logic: None,
    });

    let bytes = serialize(&graph).expect("serialization should succeed");

    // Verify UE5 magic number
    assert_eq!(
        &bytes[0..4],
        &[0xC1, 0x83, 0x2A, 0x9E],
        "Invalid UE5 magic number"
    );

    // Verify file is not empty
    assert!(bytes.len() > 100, "File should be larger than header");
}

#[test]
fn test_combat_graph_serialization() {
    let mut graph = GraphEditor::new("CombatGraph");

    // Input node
    graph.add_node_type(NodeType {
        name: "InputNode".to_string(),
        category: "Combat/Input".to_string(),
        inputs: vec![],
        outputs: vec![
            PinDefinition {
                name: "Execute".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: Some("Execution flow".to_string()),
            },
            PinDefinition {
                name: "Damage".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: Some("10.0".to_string()),
                tooltip: Some("Damage amount".to_string()),
            },
        ],
        properties: vec![],
        color: Some([0.0, 1.0, 0.0, 1.0]),
        icon: Some("Icons.Input".to_string()),
        tooltip: Some("Combat input node".to_string()),
        execution_logic: None,
    });

    // Execution node
    graph.add_node_type(NodeType {
        name: "ExecutionNode".to_string(),
        category: "Combat/Execution".to_string(),
        inputs: vec![
            PinDefinition {
                name: "Execute".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: None,
            },
            PinDefinition {
                name: "Damage".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: Some("10.0".to_string()),
                tooltip: Some("Damage to apply".to_string()),
            },
            PinDefinition {
                name: "Target".to_string(),
                pin_type: PinType::Object("Actor".to_string()),
                is_array: false,
                default_value: None,
                tooltip: Some("Target actor".to_string()),
            },
        ],
        outputs: vec![
            PinDefinition {
                name: "Execute".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: None,
            },
            PinDefinition {
                name: "Success".to_string(),
                pin_type: PinType::Bool,
                is_array: false,
                default_value: Some("false".to_string()),
                tooltip: Some("Whether the action succeeded".to_string()),
            },
        ],
        properties: vec![],
        color: Some([1.0, 1.0, 0.0, 1.0]),
        icon: Some("Icons.Execute".to_string()),
        tooltip: Some("Execute combat action".to_string()),
        execution_logic: Some("ApplyDamage(Target, Damage)".to_string()),
    });

    // Portal node
    graph.add_node_type(NodeType {
        name: "PortalNode".to_string(),
        category: "Combat/Flow".to_string(),
        inputs: vec![PinDefinition {
            name: "Execute".to_string(),
            pin_type: PinType::Exec,
            is_array: false,
            default_value: None,
            tooltip: None,
        }],
        outputs: vec![PinDefinition {
            name: "Execute".to_string(),
            pin_type: PinType::Exec,
            is_array: false,
            default_value: None,
            tooltip: None,
        }],
        properties: vec![],
        color: Some([0.5, 0.5, 1.0, 1.0]),
        icon: Some("Icons.Portal".to_string()),
        tooltip: Some("Portal to another part of the graph".to_string()),
        execution_logic: None,
    });

    let bytes = serialize(&graph).expect("serialization should succeed");

    // Verify magic number
    assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);

    // Verify file size is reasonable (should have 3 node types + graph + schema)
    assert!(bytes.len() > 500, "File should contain multiple exports");
}

#[test]
fn test_all_pin_types_serialization() {
    let mut graph = GraphEditor::new("AllPinTypesGraph");

    graph.add_node_type(NodeType {
        name: "AllPinsNode".to_string(),
        category: "Test/Comprehensive".to_string(),
        inputs: vec![
            PinDefinition {
                name: "ExecIn".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: Some("Execution input".to_string()),
            },
            PinDefinition {
                name: "BoolIn".to_string(),
                pin_type: PinType::Bool,
                is_array: false,
                default_value: Some("true".to_string()),
                tooltip: Some("Boolean input".to_string()),
            },
            PinDefinition {
                name: "IntIn".to_string(),
                pin_type: PinType::Int,
                is_array: false,
                default_value: Some("42".to_string()),
                tooltip: Some("Integer input".to_string()),
            },
            PinDefinition {
                name: "FloatIn".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: Some("3.14".to_string()),
                tooltip: Some("Float input".to_string()),
            },
            PinDefinition {
                name: "StringIn".to_string(),
                pin_type: PinType::String,
                is_array: false,
                default_value: Some("Hello".to_string()),
                tooltip: Some("String input".to_string()),
            },
            PinDefinition {
                name: "ObjectIn".to_string(),
                pin_type: PinType::Object("Actor".to_string()),
                is_array: false,
                default_value: None,
                tooltip: Some("Object reference input".to_string()),
            },
            PinDefinition {
                name: "StructIn".to_string(),
                pin_type: PinType::Struct("Vector".to_string()),
                is_array: false,
                default_value: None,
                tooltip: Some("Struct input".to_string()),
            },
            PinDefinition {
                name: "EnumIn".to_string(),
                pin_type: PinType::Enum("EDirection".to_string()),
                is_array: false,
                default_value: Some("North".to_string()),
                tooltip: Some("Enum input".to_string()),
            },
            PinDefinition {
                name: "WildcardIn".to_string(),
                pin_type: PinType::Wildcard,
                is_array: false,
                default_value: None,
                tooltip: Some("Wildcard input (any type)".to_string()),
            },
            PinDefinition {
                name: "ArrayIn".to_string(),
                pin_type: PinType::Int,
                is_array: true,
                default_value: None,
                tooltip: Some("Array of integers".to_string()),
            },
        ],
        outputs: vec![
            PinDefinition {
                name: "ExecOut".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: Some("Execution output".to_string()),
            },
            PinDefinition {
                name: "Result".to_string(),
                pin_type: PinType::Bool,
                is_array: false,
                default_value: Some("false".to_string()),
                tooltip: Some("Operation result".to_string()),
            },
        ],
        properties: vec![],
        color: Some([0.5, 0.5, 0.5, 1.0]),
        icon: Some("Icons.AllTypes".to_string()),
        tooltip: Some("Node demonstrating all pin types".to_string()),
        execution_logic: Some("ProcessAllTypes()".to_string()),
    });

    let bytes = serialize(&graph).expect("serialization should succeed");

    // Verify magic number
    assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);

    // Verify file is substantial
    assert!(
        bytes.len() > 300,
        "File should contain comprehensive node definition"
    );
}

#[test]
fn test_empty_graph_serialization() {
    let graph = GraphEditor::new("EmptyGraph");

    let bytes = serialize(&graph).expect("serialization should succeed");

    // Verify magic number
    assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);

    // Even empty graph should have basic structure
    assert!(
        bytes.len() > 50,
        "Empty graph should still have header and basic exports"
    );
}

#[test]
fn test_large_graph_serialization() {
    let mut graph = GraphEditor::new("LargeGraph");

    // Add 10 different node types
    for i in 0..10 {
        graph.add_node_type(NodeType {
            name: format!("Node{}", i),
            category: format!("Category{}", i % 3),
            inputs: vec![
                PinDefinition {
                    name: "Execute".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: None,
                },
                PinDefinition {
                    name: format!("Input{}", i),
                    pin_type: PinType::Float,
                    is_array: false,
                    default_value: Some(format!("{}.0", i)),
                    tooltip: Some(format!("Input for node {}", i)),
                },
            ],
            outputs: vec![
                PinDefinition {
                    name: "Execute".to_string(),
                    pin_type: PinType::Exec,
                    is_array: false,
                    default_value: None,
                    tooltip: None,
                },
                PinDefinition {
                    name: format!("Output{}", i),
                    pin_type: PinType::Float,
                    is_array: false,
                    default_value: None,
                    tooltip: Some(format!("Output from node {}", i)),
                },
            ],
            properties: vec![],
            color: Some([i as f32 / 10.0, 0.5, 0.5, 1.0]),
            icon: Some(format!("Icons.Node{}", i)),
            tooltip: Some(format!("Node number {}", i)),
            execution_logic: Some(format!("Process{}()", i)),
        });
    }

    let bytes = serialize(&graph).expect("serialization should succeed");

    // Verify magic number
    assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);

    // Large graph should have substantial size
    assert!(
        bytes.len() > 1000,
        "Large graph should have significant size"
    );
}

#[test]
fn test_node_with_complex_tooltips() {
    let mut graph = GraphEditor::new("TooltipGraph");

    graph.add_node_type(NodeType {
        name: "DocumentedNode".to_string(),
        category: "Documentation".to_string(),
        inputs: vec![
            PinDefinition {
                name: "Input1".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: Some("1.0".to_string()),
                tooltip: Some("This is a very detailed tooltip that explains exactly what this input does and how it should be used in the context of the graph editor.".to_string()),
            },
        ],
        outputs: vec![
            PinDefinition {
                name: "Output1".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: None,
                tooltip: Some("This output provides the result of the computation performed by this node. It can be connected to other nodes that accept float inputs.".to_string()),
            },
        ],
        properties: vec![],
        color: Some([0.2, 0.8, 0.2, 1.0]),
        icon: Some("Icons.Documentation".to_string()),
        tooltip: Some("This is a comprehensive node tooltip that provides detailed information about the node's purpose, behavior, and usage patterns. It demonstrates that the serializer can handle long text fields without issues.".to_string()),
        execution_logic: Some("PerformComplexCalculation()".to_string()),
    });

    let bytes = serialize(&graph).expect("serialization should succeed");

    // Verify magic number
    assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]);

    // Should handle long strings
    assert!(
        bytes.len() > 200,
        "File should contain long tooltip strings"
    );
}

#[test]
fn test_serialization_deterministic() {
    let mut graph = GraphEditor::new("DeterministicGraph");

    graph.add_node_type(NodeType {
        name: "TestNode".to_string(),
        category: "Test".to_string(),
        inputs: vec![],
        outputs: vec![],
        properties: vec![],
        color: Some([1.0, 0.0, 0.0, 1.0]),
        icon: None,
        tooltip: Some("Test".to_string()),
        execution_logic: None,
    });

    // Serialize twice
    let bytes1 = serialize(&graph).expect("first serialization should succeed");
    let bytes2 = serialize(&graph).expect("second serialization should succeed");

    // Results should be identical (deterministic)
    assert_eq!(bytes1, bytes2, "Serialization should be deterministic");
}
