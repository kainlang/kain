//! Tests for Runtime Graph IR Converter

use ue5_graphs::{convert_graph_runtime_to_ir, RuntimePinType, PinDirection};
use kain_core::ast::*;
use kain_core::span::Span;

/// Helper to create a dummy span
fn span() -> Span {
    Span::new(0, 0)
}

#[test]
fn test_convert_simple_graph_runtime() {
    // Create a simple GraphRuntimeDef AST
    let ast = GraphRuntimeDef {
        name: "TestGraph".to_string(),
        attributes: vec![],
        graph_data: None,
        node_types: vec![],
        instance: None,
        pin_config: None,
        span: span(),
    };
    
    let result = convert_graph_runtime_to_ir(&ast);
    assert!(result.is_ok());
    
    let graph = result.unwrap();
    assert_eq!(graph.name, "TestGraph");
    assert_eq!(graph.instance_def.name, "TestGraphInstance");
    assert_eq!(graph.node_types.len(), 0);
}

#[test]
fn test_convert_graph_with_node_data() {
    // Create a GraphRuntimeDef with a node type
    let node_data = NodeDataDef {
        name: "ActionNode".to_string(),
        base_class: None,
        input_pins: vec![
            PinDef {
                name: "Execute".to_string(),
                ty: Type::Named {
                    name: "Exec".to_string(),
                    generics: vec![],
                    span: span(),
                },
                is_array: false,
                default: None,
                attributes: vec![],
                span: span(),
            },
        ],
        output_pins: vec![
            PinDef {
                name: "Success".to_string(),
                ty: Type::Named {
                    name: "Exec".to_string(),
                    generics: vec![],
                    span: span(),
                },
                is_array: false,
                default: None,
                attributes: vec![],
                span: span(),
            },
        ],
        properties: vec![],
        methods: vec![],
        execute_logic: None,
        attributes: vec![
            Attribute {
                name: "category".to_string(),
                args: vec![Expr::String("Combat/Actions".to_string(), span())],
                span: span(),
            },
        ],
        span: span(),
    };
    
    let ast = GraphRuntimeDef {
        name: "CombatGraph".to_string(),
        attributes: vec![],
        graph_data: None,
        node_types: vec![node_data],
        instance: None,
        pin_config: None,
        span: span(),
    };
    
    let result = convert_graph_runtime_to_ir(&ast);
    assert!(result.is_ok());
    
    let graph = result.unwrap();
    assert_eq!(graph.name, "CombatGraph");
    assert_eq!(graph.node_types.len(), 1);
    
    let node = &graph.node_types[0];
    assert_eq!(node.name, "ActionNode");
    assert_eq!(node.category, "Combat/Actions");
    assert_eq!(node.input_pins.len(), 1);
    assert_eq!(node.output_pins.len(), 1);
    
    assert_eq!(node.input_pins[0].name, "Execute");
    assert_eq!(node.input_pins[0].pin_type, RuntimePinType::Exec);
    assert_eq!(node.input_pins[0].direction, PinDirection::Input);
    
    assert_eq!(node.output_pins[0].name, "Success");
    assert_eq!(node.output_pins[0].pin_type, RuntimePinType::Exec);
    assert_eq!(node.output_pins[0].direction, PinDirection::Output);
}

#[test]
fn test_convert_graph_with_instance() {
    // Create instance definition
    let instance = GraphInstanceDef {
        state: vec![
            Field {
                name: "CurrentNode".to_string(),
                ty: Type::Named {
                    name: "NodeData".to_string(),
                    generics: vec![],
                    span: span(),
                },
                attributes: vec![
                    Attribute {
                        name: "replicated".to_string(),
                        args: vec![],
                        span: span(),
                    },
                ],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: span(),
            },
        ],
        methods: vec![],
        delegates: vec![],
        attributes: vec![
            Attribute {
                name: "replicated".to_string(),
                args: vec![],
                span: span(),
            },
        ],
        span: span(),
    };
    
    let ast = GraphRuntimeDef {
        name: "TestGraph".to_string(),
        attributes: vec![],
        graph_data: None,
        node_types: vec![],
        instance: Some(instance),
        pin_config: None,
        span: span(),
    };
    
    let result = convert_graph_runtime_to_ir(&ast);
    assert!(result.is_ok());
    
    let graph = result.unwrap();
    assert_eq!(graph.instance_def.name, "TestGraphInstance");
    assert_eq!(graph.instance_def.state_fields.len(), 1);
    assert!(graph.instance_def.is_replicated);
    
    let field = &graph.instance_def.state_fields[0];
    assert_eq!(field.name, "CurrentNode");
    assert_eq!(field.property_type, RuntimePinType::Object("UNodeData".to_string()));
}

#[test]
fn test_convert_pin_types() {
    let test_cases = vec![
        ("Bool", RuntimePinType::Bool),
        ("Int", RuntimePinType::Int),
        ("Float", RuntimePinType::Float),
        ("String", RuntimePinType::String),
        ("Vec3", RuntimePinType::Vector),
        ("Exec", RuntimePinType::Exec),
    ];
    
    for (type_name, expected_pin_type) in test_cases {
        let pin_def = PinDef {
            name: "TestPin".to_string(),
            ty: Type::Named {
                name: type_name.to_string(),
                generics: vec![],
                span: span(),
            },
            is_array: false,
            default: None,
            attributes: vec![],
            span: span(),
        };
        
        let node_data = NodeDataDef {
            name: "TestNode".to_string(),
            base_class: None,
            input_pins: vec![pin_def],
            output_pins: vec![],
            properties: vec![],
            methods: vec![],
            execute_logic: None,
            attributes: vec![],
            span: span(),
        };
        
        let ast = GraphRuntimeDef {
            name: "TestGraph".to_string(),
            attributes: vec![],
            graph_data: None,
            node_types: vec![node_data],
            instance: None,
            pin_config: None,
            span: span(),
        };
        
        let result = convert_graph_runtime_to_ir(&ast);
        assert!(result.is_ok(), "Failed to convert type: {}", type_name);
        
        let graph = result.unwrap();
        assert_eq!(graph.node_types[0].input_pins[0].pin_type, expected_pin_type);
    }
}

#[test]
fn test_convert_array_pins() {
    let pin_def = PinDef {
        name: "Targets".to_string(),
        ty: Type::Array(
            Box::new(Type::Named {
                name: "AActor".to_string(),
                generics: vec![],
                span: span(),
            }),
            10,
            span(),
        ),
        is_array: true,
        default: None,
        attributes: vec![],
        span: span(),
    };
    
    let node_data = NodeDataDef {
        name: "TestNode".to_string(),
        base_class: None,
        input_pins: vec![pin_def],
        output_pins: vec![],
        properties: vec![],
        methods: vec![],
        execute_logic: None,
        attributes: vec![],
        span: span(),
    };
    
    let ast = GraphRuntimeDef {
        name: "TestGraph".to_string(),
        attributes: vec![],
        graph_data: None,
        node_types: vec![node_data],
        instance: None,
        pin_config: None,
        span: span(),
    };
    
    let result = convert_graph_runtime_to_ir(&ast);
    assert!(result.is_ok());
    
    let graph = result.unwrap();
    let pin = &graph.node_types[0].input_pins[0];
    assert_eq!(pin.name, "Targets");
    assert_eq!(pin.pin_type, RuntimePinType::Object("AActor".to_string()));
    assert!(pin.is_array);
}

#[test]
fn test_convert_pin_with_default_value() {
    let pin_def = PinDef {
        name: "Damage".to_string(),
        ty: Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: span(),
        },
        is_array: false,
        default: Some(Expr::Float(10.0, span())),
        attributes: vec![],
        span: span(),
    };
    
    let node_data = NodeDataDef {
        name: "TestNode".to_string(),
        base_class: None,
        input_pins: vec![pin_def],
        output_pins: vec![],
        properties: vec![],
        methods: vec![],
        execute_logic: None,
        attributes: vec![],
        span: span(),
    };
    
    let ast = GraphRuntimeDef {
        name: "TestGraph".to_string(),
        attributes: vec![],
        graph_data: None,
        node_types: vec![node_data],
        instance: None,
        pin_config: None,
        span: span(),
    };
    
    let result = convert_graph_runtime_to_ir(&ast);
    assert!(result.is_ok());
    
    let graph = result.unwrap();
    let pin = &graph.node_types[0].input_pins[0];
    assert_eq!(pin.default_value, Some("10".to_string()));
}

#[test]
fn test_convert_node_with_properties() {
    let property = Field {
        name: "ActionClass".to_string(),
        ty: Type::Named {
            name: "Class".to_string(),
            generics: vec![],
            span: span(),
        },
        attributes: vec![
            Attribute {
                name: "editanywhere".to_string(),
                args: vec![],
                span: span(),
            },
        ],
        visibility: Visibility::Public,
        default: None,
        weak: false,
        span: span(),
    };
    
    let node_data = NodeDataDef {
        name: "ActionNode".to_string(),
        base_class: None,
        input_pins: vec![],
        output_pins: vec![],
        properties: vec![property],
        methods: vec![],
        execute_logic: None,
        attributes: vec![],
        span: span(),
    };
    
    let ast = GraphRuntimeDef {
        name: "TestGraph".to_string(),
        attributes: vec![],
        graph_data: None,
        node_types: vec![node_data],
        instance: None,
        pin_config: None,
        span: span(),
    };
    
    let result = convert_graph_runtime_to_ir(&ast);
    assert!(result.is_ok());
    
    let graph = result.unwrap();
    assert_eq!(graph.node_types[0].properties.len(), 1);
    
    let prop = &graph.node_types[0].properties[0];
    assert_eq!(prop.name, "ActionClass");
    assert_eq!(prop.property_type, RuntimePinType::Object("UClass".to_string()));
}

#[test]
fn test_convert_graph_properties() {
    let ast = GraphRuntimeDef {
        name: "TestGraph".to_string(),
        attributes: vec![
            Attribute {
                name: "parallel_execution".to_string(),
                args: vec![],
                span: span(),
            },
            Attribute {
                name: "debug_logging".to_string(),
                args: vec![],
                span: span(),
            },
        ],
        graph_data: None,
        node_types: vec![],
        instance: None,
        pin_config: None,
        span: span(),
    };
    
    let result = convert_graph_runtime_to_ir(&ast);
    assert!(result.is_ok());
    
    let graph = result.unwrap();
    assert!(graph.properties.allow_parallel_execution);
    assert!(graph.properties.enable_debug_logging);
}

#[test]
fn test_convert_instance_with_methods() {
    let method = Function {
        name: "TriggerAction".to_string(),
        generics: vec![],
        params: vec![],
        return_type: Some(Type::Named {
            name: "Bool".to_string(),
            generics: vec![],
            span: span(),
        }),
        effects: vec![],
        body: Block {
            stmts: vec![],
            span: span(),
        },
        visibility: Visibility::Public,
        attributes: vec![
            Attribute {
                name: "blueprint_callable".to_string(),
                args: vec![],
                span: span(),
            },
        ],
        span: span(),
    };
    
    let instance = GraphInstanceDef {
        state: vec![],
        methods: vec![method],
        delegates: vec![],
        attributes: vec![],
        span: span(),
    };
    
    let ast = GraphRuntimeDef {
        name: "TestGraph".to_string(),
        attributes: vec![],
        graph_data: None,
        node_types: vec![],
        instance: Some(instance),
        pin_config: None,
        span: span(),
    };
    
    let result = convert_graph_runtime_to_ir(&ast);
    assert!(result.is_ok());
    
    let graph = result.unwrap();
    assert_eq!(graph.instance_def.methods.len(), 1);
    
    let method = &graph.instance_def.methods[0];
    assert_eq!(method.name, "TriggerAction");
    assert_eq!(method.return_type, Some(RuntimePinType::Bool));
}
