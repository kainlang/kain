//! Integration tests for GraphData generator

use ue5_graphs::graph_ir::*;
use ue5_graphs::runtime_codegen::*;

fn create_combat_graph() -> GraphEditor {
    let mut graph = GraphEditor::new("Combat");
    
    // Add AttackNode
    graph.add_node_type(NodeType {
        name: "AttackNode".to_string(),
        category: "Combat/Actions".to_string(),
        inputs: vec![
            PinDefinition {
                name: "Execute".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: Some("Execute this attack".to_string()),
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
                name: "OnComplete".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: Some("Fired when attack completes".to_string()),
            },
            PinDefinition {
                name: "ActualDamage".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: None,
                tooltip: Some("Actual damage dealt".to_string()),
            },
        ],
        properties: Vec::new(),
        color: Some([1.0, 0.0, 0.0, 1.0]),
        icon: Some("Icons.Attack".to_string()),
        tooltip: Some("Execute an attack action".to_string()),
        execution_logic: None,
    });
    
    // Add DefendNode
    graph.add_node_type(NodeType {
        name: "DefendNode".to_string(),
        category: "Combat/Actions".to_string(),
        inputs: vec![
            PinDefinition {
                name: "Execute".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: Some("Execute defense".to_string()),
            },
        ],
        outputs: vec![
            PinDefinition {
                name: "OnComplete".to_string(),
                pin_type: PinType::Exec,
                is_array: false,
                default_value: None,
                tooltip: Some("Fired when defense completes".to_string()),
            },
        ],
        properties: Vec::new(),
        color: Some([0.0, 0.0, 1.0, 1.0]),
        icon: Some("Icons.Defend".to_string()),
        tooltip: Some("Execute a defense action".to_string()),
        execution_logic: None,
    });
    
    graph
}

#[test]
fn test_graph_data_header_generation() {
    let graph = create_combat_graph();
    let result = generate_graph_data_header(&graph, "CombatPlugin");
    
    assert!(result.is_ok(), "Header generation should succeed");
    let header = result.unwrap();
    
    // Verify header structure
    assert!(header.contains("#pragma once"), "Should have header guard");
    assert!(header.contains("#include \"CoreMinimal.h\""), "Should include CoreMinimal");
    assert!(header.contains("CombatGraphData.generated.h"), "Should include generated header");
    
    // Verify forward declarations
    assert!(header.contains("class UCombatGraphInstance"), "Should forward declare GraphInstance");
    assert!(header.contains("class UCombatGraphNodeData"), "Should forward declare NodeData");
    
    // Verify PinData class
    assert!(header.contains("UCLASS()"), "Should have UCLASS macro");
    assert!(header.contains("class COMBATPLUGIN_API UCombatPinData"), "Should have correct class name and API macro");
    assert!(header.contains("GENERATED_BODY()"), "Should have GENERATED_BODY macro");
    assert!(header.contains("FName PinName"), "Should have PinName field");
    assert!(header.contains("FGuid PinId"), "Should have PinId field");
    assert!(header.contains("TArray<UCombatPinData*> ConnectToPins"), "Should have ConnectToPins array");
    assert!(header.contains("UCombatGraphNodeData* Parent"), "Should have Parent pointer");
    
    // Verify NodeData class
    assert!(header.contains("class COMBATPLUGIN_API UCombatGraphNodeData"), "Should have NodeData class");
    assert!(header.contains("GetNextOutputNodeByPinIndex"), "Should have GetNextOutputNodeByPinIndex method");
    assert!(header.contains("ExecuteNode"), "Should have ExecuteNode method");
    assert!(header.contains("TArray<UCombatPinData*> InputPins"), "Should have InputPins array");
    assert!(header.contains("TArray<UCombatPinData*> OutputPins"), "Should have OutputPins array");
    assert!(header.contains("FIntPoint NodePosition"), "Should have NodePosition field");
    assert!(header.contains("FGuid NodeGuid"), "Should have NodeGuid field");
    
    // Verify GraphData class
    assert!(header.contains("class COMBATPLUGIN_API UCombatGraphData"), "Should have GraphData class");
    assert!(header.contains("FindPinById"), "Should have FindPinById method");
    assert!(header.contains("TArray<UCombatGraphNodeData*> Nodes"), "Should have Nodes array");
}

#[test]
fn test_graph_data_source_generation() {
    let graph = create_combat_graph();
    let result = generate_graph_data_source(&graph, "CombatPlugin");
    
    assert!(result.is_ok(), "Source generation should succeed");
    let source = result.unwrap();
    
    // Verify includes
    assert!(source.contains("#include \"GraphData/CombatGraphData.h\""), "Should include header");
    
    // Verify FindPinById implementation
    assert!(source.contains("UCombatPinData* UCombatGraphData::FindPinById(FGuid PinId)"), 
            "Should have FindPinById implementation");
    assert!(source.contains("for (UCombatGraphNodeData* Node : Nodes)"), 
            "Should iterate over nodes");
    assert!(source.contains("for (UCombatPinData* Pin : Node->InputPins)"), 
            "Should check input pins");
    assert!(source.contains("for (UCombatPinData* Pin : Node->OutputPins)"), 
            "Should check output pins");
    assert!(source.contains("if (Pin->PinId == PinId)"), 
            "Should compare pin IDs");
    assert!(source.contains("return Pin;"), 
            "Should return found pin");
    assert!(source.contains("return nullptr;"), 
            "Should return nullptr if not found");
    
    // Verify GetNextOutputNodeByPinIndex implementation
    assert!(source.contains("UCombatGraphNodeData* UCombatGraphNodeData::GetNextOutputNodeByPinIndex(int OutputPinIndex)"), 
            "Should have GetNextOutputNodeByPinIndex implementation");
    assert!(source.contains("if (!OutputPins.IsValidIndex(OutputPinIndex))"), 
            "Should validate index");
    assert!(source.contains("if (!IsValid(OutputPin))"), 
            "Should validate pin");
    assert!(source.contains("if (OutputPin->ConnectToPins.Num() == 0)"), 
            "Should check for connections");
    assert!(source.contains("return OutputPin->ConnectToPins[0]->Parent;"), 
            "Should return connected node");
    
    // Verify ExecuteNode implementation
    assert!(source.contains("const UCombatGraphNodeData* UCombatGraphNodeData::ExecuteNode(UCombatGraphInstance* GraphInstance)"), 
            "Should have ExecuteNode implementation");
    assert!(source.contains("return GetNextOutputNodeByPinIndex(0);"), 
            "Should call GetNextOutputNodeByPinIndex");
}

#[test]
fn test_template_methods_in_header() {
    let graph = create_combat_graph();
    let result = generate_graph_data_header(&graph, "CombatPlugin");
    
    assert!(result.is_ok());
    let header = result.unwrap();
    
    // Verify GetNodeData template
    assert!(header.contains("template <typename T>"), "Should have template declaration");
    assert!(header.contains("T* GetNodeData()"), "Should have GetNodeData method");
    assert!(header.contains("static_assert(TIsDerivedFrom<T, UCombatGraphNodeData>::IsDerived"), 
            "Should have static_assert for type checking");
    assert!(header.contains("if (!IsValid(Nodes[i]))"), "Should validate nodes");
    assert!(header.contains("if (Nodes[i]->IsA<T>())"), "Should check node type");
    assert!(header.contains("return Cast<T>(Nodes[i]);"), "Should cast and return");
    
    // Verify GetListNodeData template
    assert!(header.contains("TArray<T*> GetListNodeData()"), "Should have GetListNodeData method");
    assert!(header.contains("TArray<T*> Result;"), "Should declare result array");
    assert!(header.contains("Result.Add(Cast<T>(Nodes[i]));"), "Should add to result");
    assert!(header.contains("return Result;"), "Should return result array");
}

#[test]
fn test_api_macro_formatting() {
    let graph = create_combat_graph();
    
    // Test with different plugin names
    let test_cases = vec![
        ("MyPlugin", "MYPLUGIN_API"),
        ("CombatSystem", "COMBATSYSTEM_API"),
        ("test_plugin", "TEST_PLUGIN_API"),
    ];
    
    for (plugin_name, expected_macro) in test_cases {
        let result = generate_graph_data_header(&graph, plugin_name);
        assert!(result.is_ok());
        let header = result.unwrap();
        assert!(header.contains(expected_macro), 
                "Should contain {} for plugin {}", expected_macro, plugin_name);
    }
}

#[test]
fn test_documentation_comments() {
    let graph = create_combat_graph();
    let result = generate_graph_data_header(&graph, "CombatPlugin");
    
    assert!(result.is_ok());
    let header = result.unwrap();
    
    // Verify documentation comments exist
    assert!(header.contains("/**"), "Should have documentation comments");
    assert!(header.contains("Pin connection data"), "Should document PinData");
    assert!(header.contains("Base class for"), "Should document NodeData");
    assert!(header.contains("Container for"), "Should document GraphData");
    assert!(header.contains("@return"), "Should document return values");
}

#[test]
fn test_uproperty_macros() {
    let graph = create_combat_graph();
    let result = generate_graph_data_header(&graph, "CombatPlugin");
    
    assert!(result.is_ok());
    let header = result.unwrap();
    
    // Count UPROPERTY macros (should have one for each field)
    let uproperty_count = header.matches("UPROPERTY()").count();
    
    // PinData: 4 fields (PinName, PinId, ConnectToPins, Parent)
    // NodeData: 4 fields (InputPins, OutputPins, NodePosition, NodeGuid)
    // GraphData: 1 field (Nodes)
    // Total: 9 UPROPERTY macros
    assert_eq!(uproperty_count, 9, "Should have 9 UPROPERTY macros");
}

#[test]
fn test_null_safety_in_source() {
    let graph = create_combat_graph();
    let result = generate_graph_data_source(&graph, "CombatPlugin");
    
    assert!(result.is_ok());
    let source = result.unwrap();
    
    // Verify null safety checks
    assert!(source.contains("IsValidIndex"), "Should check array bounds");
    assert!(source.contains("IsValid"), "Should check pointer validity");
    assert!(source.contains("return nullptr;"), "Should return nullptr on failure");
}

#[test]
fn test_different_graph_names() {
    let test_cases = vec!["Combat", "Dialogue", "Quest", "AI"];
    
    for graph_name in test_cases {
        let graph = GraphEditor::new(graph_name);
        
        let header_result = generate_graph_data_header(&graph, "TestPlugin");
        assert!(header_result.is_ok(), "Header generation should succeed for {}", graph_name);
        let header = header_result.unwrap();
        
        // Verify correct naming
        assert!(header.contains(&format!("U{}PinData", graph_name)), 
                "Should have correct PinData name");
        assert!(header.contains(&format!("U{}GraphNodeData", graph_name)), 
                "Should have correct NodeData name");
        assert!(header.contains(&format!("U{}GraphData", graph_name)), 
                "Should have correct GraphData name");
        
        let source_result = generate_graph_data_source(&graph, "TestPlugin");
        assert!(source_result.is_ok(), "Source generation should succeed for {}", graph_name);
        let source = source_result.unwrap();
        
        // Verify correct includes
        assert!(source.contains(&format!("GraphData/{}GraphData.h", graph_name)), 
                "Should have correct include path");
    }
}

#[test]
fn test_generated_code_compiles_conceptually() {
    // This test verifies the structure is correct for UE5 compilation
    let graph = create_combat_graph();
    let header = generate_graph_data_header(&graph, "CombatPlugin").unwrap();
    let source = generate_graph_data_source(&graph, "CombatPlugin").unwrap();
    
    // Verify all required UE5 macros are present
    assert!(header.contains("UCLASS()"), "Missing UCLASS macro");
    assert!(header.contains("GENERATED_BODY()"), "Missing GENERATED_BODY macro");
    assert!(header.contains("UPROPERTY()"), "Missing UPROPERTY macro");
    
    // Verify proper include structure
    assert!(header.contains("#pragma once"), "Missing pragma once");
    assert!(header.contains("#include \"CoreMinimal.h\""), "Missing CoreMinimal include");
    assert!(header.contains(".generated.h"), "Missing generated header include");
    
    // Verify source includes header
    assert!(source.contains(&format!("#include \"GraphData/{}GraphData.h\"", graph.name)), 
            "Source should include header");
    
    // Verify no syntax errors (basic checks)
    assert_eq!(header.matches('{').count(), header.matches('}').count(), 
               "Braces should be balanced in header");
    assert_eq!(source.matches('{').count(), source.matches('}').count(), 
               "Braces should be balanced in source");
}
