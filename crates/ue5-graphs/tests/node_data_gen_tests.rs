//! NodeData Generator Tests
//!
//! Comprehensive tests for runtime NodeData class generation

use ue5_graphs::{
    GraphEditor, NodeType, PinDefinition, PinType,
    runtime_codegen::{generate_node_data, NodeDataGenerator},
};

/// Create a comprehensive test graph with multiple node types
fn create_comprehensive_graph() -> GraphEditor {
    let mut graph = GraphEditor::new("TestGraph");
    
    // Root node - entry point (no inputs)
    let root_node = NodeType {
        name: "Root".to_string(),
        category: "Core".to_string(),
        inputs: vec![],
        outputs: vec![
            PinDefinition {
                name: "Execute".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: Some("Start execution".to_string()),
            },
        ],
        properties: Vec::new(),
        color: Some([1.0, 0.0, 0.0, 1.0]),
        icon: None,
        tooltip: Some("Root entry node".to_string()),
        execution_logic: None,
    };
    
    // Condition node - branching logic
    let condition_node = NodeType {
        name: "Condition".to_string(),
        category: "Logic".to_string(),
        inputs: vec![
            PinDefinition {
                name: "Execute".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: None,
            },
            PinDefinition {
                name: "Condition".to_string(),
                pin_type: PinType::Bool,
                is_array: false,
                default_value: Some("true".to_string()),
                tooltip: Some("Condition to evaluate".to_string()),
            },
        ],
        outputs: vec![
            PinDefinition {
                name: "True".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: None,
            },
            PinDefinition {
                name: "False".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: None,
            },
        ],
        properties: Vec::new(),
        color: Some([0.0, 1.0, 0.0, 1.0]),
        icon: None,
        tooltip: Some("Branch based on condition".to_string()),
        execution_logic: Some("if (Condition) { return True; } else { return False; }".to_string()),
    };
    
    // Action node - performs work
    let action_node = NodeType {
        name: "Action".to_string(),
        category: "Actions".to_string(),
        inputs: vec![
            PinDefinition {
                name: "Execute".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: None,
            },
            PinDefinition {
                name: "Target".to_string(),
                pin_type: PinType::Object("AActor".to_string()),
                is_array: false,
                default_value: None,
                tooltip: Some("Target actor".to_string()),
            },
            PinDefinition {
                name: "Damage".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: Some("10.0".to_string()),
                tooltip: Some("Damage amount".to_string()),
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
        ],
        properties: Vec::new(),
        color: Some([0.0, 0.0, 1.0, 1.0]),
        icon: None,
        tooltip: Some("Perform action".to_string()),
        execution_logic: Some("ApplyDamage(Target, Damage)".to_string()),
    };
    
    graph.add_node_type(root_node);
    graph.add_node_type(condition_node);
    graph.add_node_type(action_node);
    graph
}

#[test]
fn test_generate_node_data_creates_all_files() {
    let graph = create_comprehensive_graph();
    let result = generate_node_data(&graph, "TestRuntime");
    
    assert!(result.is_ok(), "Should generate NodeData successfully");
    let output = result.unwrap();
    
    // Check base files
    assert_eq!(output.base_header.0, "TestGraphGraphNodeData.h");
    assert_eq!(output.base_source.0, "TestGraphGraphNodeData.cpp");
    assert_eq!(output.pin_data_header.0, "TestGraphPinData.h");
    assert_eq!(output.pin_data_source.0, "TestGraphPinData.cpp");
    
    // Check node data files (3 node types)
    assert_eq!(output.node_data_headers.len(), 3);
    assert_eq!(output.node_data_sources.len(), 3);
    
    // Check filenames
    assert!(output.node_data_headers.iter().any(|(name, _)| name == "RootNodeData.h"));
    assert!(output.node_data_headers.iter().any(|(name, _)| name == "ConditionNodeData.h"));
    assert!(output.node_data_headers.iter().any(|(name, _)| name == "ActionNodeData.h"));
}

#[test]
fn test_base_node_data_header_structure() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    let header = generator.generate_base_node_data_header().unwrap();
    
    // Check header guard
    assert!(header.contains("#pragma once"));
    
    // Check includes
    assert!(header.contains("#include \"CoreMinimal.h\""));
    assert!(header.contains("#include \"UObject/NoExportTypes.h\""));
    assert!(header.contains("#include \"TestGraphGraphNodeData.generated.h\""));
    
    // Check class declaration
    assert!(header.contains("class TESTRUNTIME_API UTestGraphGraphNodeData"));
    assert!(header.contains(": public UObject"));
    assert!(header.contains("GENERATED_BODY()"));
    
    // Check properties
    assert!(header.contains("TArray<UTestGraphPinData*> InputPins"));
    assert!(header.contains("TArray<UTestGraphPinData*> OutputPins"));
    assert!(header.contains("FIntPoint NodePosition"));
    assert!(header.contains("FGuid NodeGuid"));
    
    // Check methods
    assert!(header.contains("GetNextOutputNodeByPinIndex(int OutputPinIndex) const"));
    assert!(header.contains("virtual const UTestGraphGraphNodeData* ExecuteNode"));
    assert!(header.contains("UTestGraphGraphInstance* Instance"));
}

#[test]
fn test_base_node_data_source_implementation() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    let source = generator.generate_base_node_data_source().unwrap();
    
    // Check includes
    assert!(source.contains("#include \"TestGraphGraphNodeData.h\""));
    assert!(source.contains("#include \"TestGraphPinData.h\""));
    
    // Check GetNextOutputNodeByPinIndex implementation
    assert!(source.contains("UTestGraphGraphNodeData* UTestGraphGraphNodeData::GetNextOutputNodeByPinIndex"));
    assert!(source.contains("if (!OutputPins.IsValidIndex(OutputPinIndex))"));
    assert!(source.contains("return nullptr"));
    assert!(source.contains("OutputPin->ConnectToPins"));
    assert!(source.contains("ConnectedPin->Parent"));
    
    // Check ExecuteNode implementation
    assert!(source.contains("const UTestGraphGraphNodeData* UTestGraphGraphNodeData::ExecuteNode"));
    assert!(source.contains("return GetNextOutputNodeByPinIndex(0)"));
}

#[test]
fn test_pin_data_header_structure() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    let header = generator.generate_pin_data_header().unwrap();
    
    // Check class declaration
    assert!(header.contains("class TESTRUNTIME_API UTestGraphPinData"));
    assert!(header.contains(": public UObject"));
    
    // Check properties
    assert!(header.contains("FName PinName"));
    assert!(header.contains("FGuid PinId"));
    assert!(header.contains("TArray<UTestGraphPinData*> ConnectToPins"));
    assert!(header.contains("UTestGraphGraphNodeData* Parent = nullptr"));
    
    // Check UPROPERTY macros
    let uproperty_count = header.matches("UPROPERTY()").count();
    assert_eq!(uproperty_count, 4, "Should have 4 UPROPERTY declarations");
}

#[test]
fn test_root_node_data_subclass() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    let node = &graph.node_types[0]; // Root node
    let header = generator.generate_node_data_subclass_header(node).unwrap();
    let source = generator.generate_node_data_subclass_source(node).unwrap();
    
    // Header checks
    assert!(header.contains("class TESTRUNTIME_API URootNodeData"));
    assert!(header.contains(": public UTestGraphGraphNodeData"));
    assert!(header.contains("virtual const UTestGraphGraphNodeData* ExecuteNode"));
    
    // Root node has no properties (only exec pins)
    assert!(!header.contains("UPROPERTY(EditAnywhere"));
    
    // Source checks
    assert!(source.contains("#include \"RootNodeData.h\""));
    assert!(source.contains("const UTestGraphGraphNodeData* URootNodeData::ExecuteNode"));
    assert!(source.contains("return GetNextOutputNodeByPinIndex(0)"));
}

#[test]
fn test_condition_node_data_subclass() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    let node = &graph.node_types[1]; // Condition node
    let header = generator.generate_node_data_subclass_header(node).unwrap();
    let source = generator.generate_node_data_subclass_source(node).unwrap();
    
    // Header checks
    assert!(header.contains("class TESTRUNTIME_API UConditionNodeData"));
    assert!(header.contains(": public UTestGraphGraphNodeData"));
    
    // Should have Condition property (bool pin)
    assert!(header.contains("UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = \"Node Data\")"));
    assert!(header.contains("bool Condition"));
    
    // Source checks
    assert!(source.contains("UConditionNodeData::ExecuteNode"));
    assert!(source.contains("if (Condition) { return True; } else { return False; }"));
}

#[test]
fn test_action_node_data_subclass() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    let node = &graph.node_types[2]; // Action node
    let header = generator.generate_node_data_subclass_header(node).unwrap();
    let source = generator.generate_node_data_subclass_source(node).unwrap();
    
    // Header checks
    assert!(header.contains("class TESTRUNTIME_API UActionNodeData"));
    
    // Should have Target and Damage properties
    assert!(header.contains("AActor* Target"));
    assert!(header.contains("float Damage"));
    
    // Source checks
    assert!(source.contains("UActionNodeData::ExecuteNode"));
    assert!(source.contains("ApplyDamage(Target, Damage)"));
}

#[test]
fn test_pin_type_conversions() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    // Test all pin type conversions
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Exec), "void");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Bool), "bool");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Int), "int32");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Float), "float");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::String), "FString");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Object("AActor".to_string())), "AActor*");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Object("UStaticMeshComponent".to_string())), "UStaticMeshComponent*");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Struct("Vector".to_string())), "FVector");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Enum("Direction".to_string())), "EDirection");
    assert_eq!(generator.pin_type_to_cpp_type(&PinType::Wildcard), "UObject*");
}

#[test]
fn test_property_name_sanitization() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    // Test various name formats
    assert_eq!(generator.sanitize_property_name("simple"), "Simple");
    assert_eq!(generator.sanitize_property_name("my_property"), "MyProperty");
    assert_eq!(generator.sanitize_property_name("some-value"), "SomeValue");
    assert_eq!(generator.sanitize_property_name("test 123"), "Test123");
    assert_eq!(generator.sanitize_property_name("UPPERCASE"), "UPPERCASE");
    assert_eq!(generator.sanitize_property_name("camelCase"), "CamelCase");
    assert_eq!(generator.sanitize_property_name("123invalid"), "_123invalid");
    assert_eq!(generator.sanitize_property_name("with.dots"), "WithDots");
    assert_eq!(generator.sanitize_property_name("special!@#chars"), "SpecialChars");
}

#[test]
fn test_node_properties_detection() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    // Root node has no properties (only exec pins)
    assert!(!generator.has_node_properties(&graph.node_types[0]));
    
    // Condition node has properties (bool pin)
    assert!(generator.has_node_properties(&graph.node_types[1]));
    
    // Action node has properties (object and float pins)
    assert!(generator.has_node_properties(&graph.node_types[2]));
}

#[test]
fn test_generated_code_compiles_syntax() {
    let graph = create_comprehensive_graph();
    let output = generate_node_data(&graph, "TestRuntime").unwrap();
    
    // Basic syntax checks for generated code
    
    // Check balanced braces in headers
    for (_, content) in &output.node_data_headers {
        let open_braces = content.matches('{').count();
        let close_braces = content.matches('}').count();
        assert_eq!(open_braces, close_braces, "Braces should be balanced in header");
    }
    
    // Check balanced braces in sources
    for (_, content) in &output.node_data_sources {
        let open_braces = content.matches('{').count();
        let close_braces = content.matches('}').count();
        assert_eq!(open_braces, close_braces, "Braces should be balanced in source");
    }
    
    // Check all headers have pragma once
    assert!(output.base_header.1.contains("#pragma once"));
    assert!(output.pin_data_header.1.contains("#pragma once"));
    for (_, content) in &output.node_data_headers {
        assert!(content.contains("#pragma once"));
    }
    
    // Check all classes have GENERATED_BODY
    assert!(output.base_header.1.contains("GENERATED_BODY()"));
    assert!(output.pin_data_header.1.contains("GENERATED_BODY()"));
    for (_, content) in &output.node_data_headers {
        assert!(content.contains("GENERATED_BODY()"));
    }
}

#[test]
fn test_inheritance_hierarchy() {
    let graph = create_comprehensive_graph();
    let output = generate_node_data(&graph, "TestRuntime").unwrap();
    
    // Base class inherits from UObject
    assert!(output.base_header.1.contains(": public UObject"));
    
    // All subclasses inherit from base
    for (_, content) in &output.node_data_headers {
        assert!(content.contains(": public UTestGraphGraphNodeData"));
    }
}

#[test]
fn test_forward_declarations() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    let header = generator.generate_base_node_data_header().unwrap();
    
    // Check forward declarations
    assert!(header.contains("class UTestGraphGraphInstance;"));
    assert!(header.contains("class UTestGraphPinData;"));
}

#[test]
fn test_api_macro_usage() {
    let graph = create_comprehensive_graph();
    let output = generate_node_data(&graph, "TestRuntime").unwrap();
    
    // All classes should have API macro
    assert!(output.base_header.1.contains("TESTRUNTIME_API"));
    assert!(output.pin_data_header.1.contains("TESTRUNTIME_API"));
    
    for (_, content) in &output.node_data_headers {
        assert!(content.contains("TESTRUNTIME_API"));
    }
}

#[test]
fn test_execution_logic_preservation() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    // Condition node has execution logic
    let condition_node = &graph.node_types[1];
    let source = generator.generate_node_data_subclass_source(condition_node).unwrap();
    
    assert!(source.contains("if (Condition) { return True; } else { return False; }"));
    
    // Action node has execution logic
    let action_node = &graph.node_types[2];
    let source = generator.generate_node_data_subclass_source(action_node).unwrap();
    
    assert!(source.contains("ApplyDamage(Target, Damage)"));
}

#[test]
fn test_multiple_properties_generation() {
    let graph = create_comprehensive_graph();
    let generator = NodeDataGenerator::new(&graph, "TestRuntime");
    
    // Action node has multiple properties
    let action_node = &graph.node_types[2];
    let header = generator.generate_node_data_subclass_header(action_node).unwrap();
    
    // Should have both Target and Damage properties
    let property_count = header.matches("UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = \"Node Data\")").count();
    assert_eq!(property_count, 2, "Action node should have 2 properties");
}

#[test]
fn test_empty_graph() {
    let graph = GraphEditor::new("Empty");
    let result = generate_node_data(&graph, "EmptyRuntime");
    
    assert!(result.is_ok());
    let output = result.unwrap();
    
    // Should still generate base classes
    assert!(!output.base_header.1.is_empty());
    assert!(!output.base_source.1.is_empty());
    assert!(!output.pin_data_header.1.is_empty());
    assert!(!output.pin_data_source.1.is_empty());
    
    // But no node data subclasses
    assert_eq!(output.node_data_headers.len(), 0);
    assert_eq!(output.node_data_sources.len(), 0);
}

