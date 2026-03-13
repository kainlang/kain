//! Comprehensive tests for AST to IR conversion

use kain_core::ast::{
    Attribute, Expr, GraphEditorDef, GraphSchemaDef, NodeTypeDef, PinDef, PropertyDef, SchemaRule,
    Type,
};
use kain_core::span::Span;
use ue5_graphs::ast_converter::convert_graph_editor;
use ue5_graphs::graph_ir::PinType;

fn dummy_span() -> Span {
    Span::new(0, 0)
}

#[test]
fn test_empty_graph_editor() {
    let ast = GraphEditorDef {
        name: "EmptyGraph".to_string(),
        attributes: vec![],
        node_types: vec![],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_ok());

    let graph = result.unwrap();
    assert_eq!(graph.name, "EmptyGraph");
    assert_eq!(graph.node_types.len(), 0);
}

#[test]
fn test_graph_with_single_node_type() {
    let node_def = NodeTypeDef {
        name: "SimpleNode".to_string(),
        category: Some("Test".to_string()),
        inputs: vec![],
        outputs: vec![],
        properties: vec![],
        attributes: vec![],
        span: dummy_span(),
    };

    let ast = GraphEditorDef {
        name: "SimpleGraph".to_string(),
        attributes: vec![],
        node_types: vec![node_def],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_ok());

    let graph = result.unwrap();
    assert_eq!(graph.node_types.len(), 1);
    assert_eq!(graph.node_types[0].name, "SimpleNode");
    assert_eq!(graph.node_types[0].category, "Test");
}

#[test]
fn test_node_with_multiple_pins() {
    let node_def = NodeTypeDef {
        name: "MathNode".to_string(),
        category: Some("Math".to_string()),
        inputs: vec![
            PinDef {
                name: "A".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: dummy_span(),
                },
                is_array: false,
                default: Some(Expr::Float(0.0, dummy_span())),
                attributes: vec![],
                span: dummy_span(),
            },
            PinDef {
                name: "B".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: dummy_span(),
                },
                is_array: false,
                default: Some(Expr::Float(1.0, dummy_span())),
                attributes: vec![],
                span: dummy_span(),
            },
        ],
        outputs: vec![PinDef {
            name: "Result".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: dummy_span(),
            },
            is_array: false,
            default: None,
            attributes: vec![],
            span: dummy_span(),
        }],
        properties: vec![],
        attributes: vec![],
        span: dummy_span(),
    };

    let ast = GraphEditorDef {
        name: "MathGraph".to_string(),
        attributes: vec![],
        node_types: vec![node_def],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_ok());

    let graph = result.unwrap();
    assert_eq!(graph.node_types[0].inputs.len(), 2);
    assert_eq!(graph.node_types[0].outputs.len(), 1);
    assert_eq!(graph.node_types[0].inputs[0].name, "A");
    assert_eq!(graph.node_types[0].inputs[1].name, "B");
    assert_eq!(graph.node_types[0].outputs[0].name, "Result");
}

#[test]
fn test_all_pin_types() {
    let pin_types = vec![
        ("Exec", PinType::Exec),
        ("Bool", PinType::Bool),
        ("Int", PinType::Int),
        ("Float", PinType::Float),
        ("String", PinType::String),
        ("Wildcard", PinType::Wildcard),
    ];

    for (type_name, expected_pin_type) in pin_types {
        let node_def = NodeTypeDef {
            name: format!("{}Node", type_name),
            category: None,
            inputs: vec![PinDef {
                name: "Input".to_string(),
                ty: Type::Named {
                    name: type_name.to_string(),
                    generics: vec![],
                    span: dummy_span(),
                },
                is_array: false,
                default: None,
                attributes: vec![],
                span: dummy_span(),
            }],
            outputs: vec![],
            properties: vec![],
            attributes: vec![],
            span: dummy_span(),
        };

        let ast = GraphEditorDef {
            name: "TypeTestGraph".to_string(),
            attributes: vec![],
            node_types: vec![node_def],
            schema: None,
            span: dummy_span(),
        };

        let result = convert_graph_editor(&ast);
        assert!(result.is_ok());

        let graph = result.unwrap();
        assert_eq!(graph.node_types[0].inputs[0].pin_type, expected_pin_type);
    }
}

#[test]
fn test_array_pins() {
    let node_def = NodeTypeDef {
        name: "ArrayNode".to_string(),
        category: None,
        inputs: vec![PinDef {
            name: "Items".to_string(),
            ty: Type::Array(
                Box::new(Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: dummy_span(),
                }),
                0,
                dummy_span(),
            ),
            is_array: true,
            default: None,
            attributes: vec![],
            span: dummy_span(),
        }],
        outputs: vec![],
        properties: vec![],
        attributes: vec![],
        span: dummy_span(),
    };

    let ast = GraphEditorDef {
        name: "ArrayGraph".to_string(),
        attributes: vec![],
        node_types: vec![node_def],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_ok());

    let graph = result.unwrap();
    assert!(graph.node_types[0].inputs[0].is_array);
    assert_eq!(graph.node_types[0].inputs[0].pin_type, PinType::Float);
}

#[test]
fn test_node_attributes() {
    let node_def = NodeTypeDef {
        name: "StyledNode".to_string(),
        category: None,
        inputs: vec![],
        outputs: vec![],
        properties: vec![],
        attributes: vec![
            Attribute {
                name: "category".to_string(),
                args: vec![Expr::String("Custom/Category".to_string(), dummy_span())],
                span: dummy_span(),
            },
            Attribute {
                name: "tooltip".to_string(),
                args: vec![Expr::String("This is a tooltip".to_string(), dummy_span())],
                span: dummy_span(),
            },
            Attribute {
                name: "icon".to_string(),
                args: vec![Expr::String("Icons.Star".to_string(), dummy_span())],
                span: dummy_span(),
            },
            Attribute {
                name: "color".to_string(),
                args: vec![
                    Expr::Float(1.0, dummy_span()),
                    Expr::Float(0.5, dummy_span()),
                    Expr::Float(0.0, dummy_span()),
                    Expr::Float(1.0, dummy_span()),
                ],
                span: dummy_span(),
            },
        ],
        span: dummy_span(),
    };

    let ast = GraphEditorDef {
        name: "StyledGraph".to_string(),
        attributes: vec![],
        node_types: vec![node_def],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_ok());

    let graph = result.unwrap();
    let node = &graph.node_types[0];
    assert_eq!(node.category, "Custom/Category");
    assert_eq!(node.tooltip, Some("This is a tooltip".to_string()));
    assert_eq!(node.icon, Some("Icons.Star".to_string()));
    assert_eq!(node.color, Some([1.0, 0.5, 0.0, 1.0]));
}

#[test]
fn test_graph_properties() {
    let ast = GraphEditorDef {
        name: "ConfiguredGraph".to_string(),
        attributes: vec![
            Attribute {
                name: "allow_multiple_inputs".to_string(),
                args: vec![Expr::Bool(true, dummy_span())],
                span: dummy_span(),
            },
            Attribute {
                name: "allow_cycles".to_string(),
                args: vec![Expr::Bool(true, dummy_span())],
                span: dummy_span(),
            },
            Attribute {
                name: "grid_snap".to_string(),
                args: vec![Expr::Int(32, dummy_span())],
                span: dummy_span(),
            },
        ],
        node_types: vec![],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_ok());

    let graph = result.unwrap();
    assert!(graph.properties.allow_multiple_input_connections);
    assert!(graph.properties.allow_cycles);
    assert_eq!(graph.properties.grid_snap_size, 32);
}

#[test]
fn test_duplicate_node_names_error() {
    let node_def = NodeTypeDef {
        name: "DuplicateNode".to_string(),
        category: None,
        inputs: vec![],
        outputs: vec![],
        properties: vec![],
        attributes: vec![],
        span: dummy_span(),
    };

    let ast = GraphEditorDef {
        name: "ErrorGraph".to_string(),
        attributes: vec![],
        node_types: vec![node_def.clone(), node_def],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_err());
    assert!(result
        .unwrap_err()
        .to_string()
        .contains("Duplicate node type name"));
}

#[test]
fn test_duplicate_input_pin_names_error() {
    let pin_def = PinDef {
        name: "DuplicatePin".to_string(),
        ty: Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: dummy_span(),
        },
        is_array: false,
        default: None,
        attributes: vec![],
        span: dummy_span(),
    };

    let node_def = NodeTypeDef {
        name: "ErrorNode".to_string(),
        category: None,
        inputs: vec![pin_def.clone(), pin_def],
        outputs: vec![],
        properties: vec![],
        attributes: vec![],
        span: dummy_span(),
    };

    let ast = GraphEditorDef {
        name: "ErrorGraph".to_string(),
        attributes: vec![],
        node_types: vec![node_def],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_err());
    assert!(result
        .unwrap_err()
        .to_string()
        .contains("Duplicate input pin name"));
}

#[test]
fn test_complex_graph_editor() {
    // Create a complex graph with multiple node types
    let input_node = NodeTypeDef {
        name: "InputNode".to_string(),
        category: Some("Input".to_string()),
        inputs: vec![],
        outputs: vec![PinDef {
            name: "Value".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: dummy_span(),
            },
            is_array: false,
            default: None,
            attributes: vec![],
            span: dummy_span(),
        }],
        properties: vec![],
        attributes: vec![],
        span: dummy_span(),
    };

    let math_node = NodeTypeDef {
        name: "AddNode".to_string(),
        category: Some("Math".to_string()),
        inputs: vec![
            PinDef {
                name: "A".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: dummy_span(),
                },
                is_array: false,
                default: None,
                attributes: vec![],
                span: dummy_span(),
            },
            PinDef {
                name: "B".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: dummy_span(),
                },
                is_array: false,
                default: None,
                attributes: vec![],
                span: dummy_span(),
            },
        ],
        outputs: vec![PinDef {
            name: "Result".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: dummy_span(),
            },
            is_array: false,
            default: None,
            attributes: vec![],
            span: dummy_span(),
        }],
        properties: vec![],
        attributes: vec![],
        span: dummy_span(),
    };

    let output_node = NodeTypeDef {
        name: "OutputNode".to_string(),
        category: Some("Output".to_string()),
        inputs: vec![PinDef {
            name: "Value".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: dummy_span(),
            },
            is_array: false,
            default: None,
            attributes: vec![],
            span: dummy_span(),
        }],
        outputs: vec![],
        properties: vec![],
        attributes: vec![],
        span: dummy_span(),
    };

    let ast = GraphEditorDef {
        name: "ComplexGraph".to_string(),
        attributes: vec![],
        node_types: vec![input_node, math_node, output_node],
        schema: None,
        span: dummy_span(),
    };

    let result = convert_graph_editor(&ast);
    assert!(result.is_ok());

    let graph = result.unwrap();
    assert_eq!(graph.node_types.len(), 3);
    assert_eq!(graph.node_types[0].name, "InputNode");
    assert_eq!(graph.node_types[1].name, "AddNode");
    assert_eq!(graph.node_types[2].name, "OutputNode");
}
