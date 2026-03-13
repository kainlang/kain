//! Integration tests for GraphInstance generator

use ue5_graphs::{generate_graph_instance, GraphEditor};

fn create_combat_graph() -> GraphEditor {
    let mut graph = GraphEditor::new("CombatGraph");

    // Add some basic properties for testing
    graph.properties.allow_cycles = false;
    graph.properties.allow_multiple_input_connections = false;
    graph.properties.allow_multiple_output_connections = true;

    graph
}

#[test]
fn test_generate_instance_output() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(
        result.is_ok(),
        "Should generate instance output successfully"
    );

    let output = result.unwrap();

    // Verify filenames
    assert_eq!(output.instance_header.0, "CombatGraphInstance.h");
    assert_eq!(output.instance_source.0, "CombatGraphInstance.cpp");
    assert_eq!(output.node_data_header.0, "CombatGraphNodeData.h");
    assert_eq!(output.node_data_source.0, "CombatGraphNodeData.cpp");

    // Verify content is not empty
    assert!(!output.instance_header.1.is_empty());
    assert!(!output.instance_source.1.is_empty());
    assert!(!output.node_data_header.1.is_empty());
    assert!(!output.node_data_source.1.is_empty());
}

#[test]
fn test_instance_header_structure() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let header = &output.instance_header.1;

    // Check class declaration (note: API macro is between "class" and class name)
    assert!(
        header.contains("UCombatGraphInstance"),
        "Should declare instance class"
    );
    assert!(
        header.contains(": public UObject"),
        "Should inherit from UObject"
    );
    assert!(
        header.contains("UCLASS(Abstract)"),
        "Should be abstract UCLASS"
    );

    // Check essential methods
    assert!(
        header.contains("ResetInstance"),
        "Should have ResetInstance method"
    );
    assert!(
        header.contains("IsValidInstance"),
        "Should have IsValidInstance method"
    );
    assert!(
        header.contains("GetCurrentNode"),
        "Should have GetCurrentNode method"
    );
    assert!(
        header.contains("GetGraphAsset"),
        "Should have GetGraphAsset method"
    );
    assert!(
        header.contains("TryProceedGraph"),
        "Should have TryProceedGraph method"
    );
    assert!(
        header.contains("SetCurrentNode"),
        "Should have SetCurrentNode method"
    );

    // Check Blueprint integration
    assert!(
        header.contains("UFUNCTION(BlueprintCallable"),
        "Should have Blueprint-callable methods"
    );
    assert!(
        header.contains("UFUNCTION(BlueprintPure"),
        "Should have Blueprint-pure methods"
    );
    assert!(
        header.contains("UFUNCTION(BlueprintNativeEvent"),
        "Should have Blueprint native events"
    );

    // Check state tracking
    assert!(
        header.contains("TWeakObjectPtr<const UCombatGraphNodeData>"),
        "Should use weak pointer for current node"
    );
    assert!(
        header.contains("bInstanceActive"),
        "Should have active flag"
    );
    assert!(header.contains("bNeedProceed"), "Should have proceed flag");
}

#[test]
fn test_instance_source_implementation() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let source = &output.instance_source.1;

    // Check includes
    assert!(
        source.contains("#include \"CombatGraphInstance.h\""),
        "Should include instance header"
    );
    assert!(
        source.contains("#include \"CombatGraphNodeData.h\""),
        "Should include node data header"
    );
    assert!(
        source.contains("#include \"CombatGraphAsset.h\""),
        "Should include asset header"
    );

    // Check method implementations
    assert!(
        source.contains("bool UCombatGraphInstance::ResetInstance"),
        "Should implement ResetInstance"
    );
    assert!(
        source.contains("bool UCombatGraphInstance::IsValidInstance"),
        "Should implement IsValidInstance"
    );
    assert!(
        source.contains("const UCombatGraphNodeData* UCombatGraphInstance::GetCurrentNode"),
        "Should implement GetCurrentNode"
    );
    assert!(
        source.contains("bool UCombatGraphInstance::TryProceedGraph"),
        "Should implement TryProceedGraph"
    );
    assert!(
        source.contains("bool UCombatGraphInstance::SetCurrentNode"),
        "Should implement SetCurrentNode"
    );
    assert!(
        source.contains("void UCombatGraphInstance::BeginDestroy"),
        "Should implement BeginDestroy"
    );

    // Check delegate broadcasting
    assert!(
        source.contains("OnInstanceReset.Broadcast"),
        "Should broadcast reset event"
    );
}

#[test]
fn test_reset_reason_enum() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let header = &output.instance_header.1;

    // Check enum declaration
    assert!(
        header.contains("enum class ECombatGraphResetReason"),
        "Should declare reset reason enum"
    );
    assert!(
        header.contains("UENUM(BlueprintType)"),
        "Should be Blueprint-exposed enum"
    );

    // Check enum values
    assert!(header.contains("RETRY"), "Should have RETRY value");
    assert!(header.contains("RESET"), "Should have RESET value");
    assert!(header.contains("END_GRAPH"), "Should have END_GRAPH value");
    assert!(header.contains("COUNT"), "Should have COUNT value");
}

#[test]
fn test_delegate_declaration() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let header = &output.instance_header.1;

    // Check delegate declaration
    assert!(
        header.contains("DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam"),
        "Should declare multicast delegate"
    );
    assert!(
        header.contains("FCombatGraphInstanceResetDelegate"),
        "Should have reset delegate type"
    );
    assert!(
        header.contains("ECombatGraphResetReason"),
        "Should use reset reason enum"
    );
    assert!(
        header.contains("OnInstanceReset"),
        "Should have OnInstanceReset property"
    );
}

#[test]
fn test_node_data_header_structure() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let header = &output.node_data_header.1;

    // Check class declaration (note: API macro is between "class" and class name)
    assert!(
        header.contains("UCombatGraphNodeData"),
        "Should declare node data class"
    );
    assert!(
        header.contains(": public UObject"),
        "Should inherit from UObject"
    );
    assert!(
        header.contains("UCLASS(Abstract)"),
        "Should be abstract UCLASS"
    );

    // Check methods
    assert!(
        header.contains("GetNodeName"),
        "Should have GetNodeName method"
    );
    assert!(
        header.contains("GetNodeDisplayName"),
        "Should have GetNodeDisplayName method"
    );

    // Check properties
    assert!(
        header.contains("FName NodeName"),
        "Should have NodeName property"
    );
    assert!(
        header.contains("FText DisplayName"),
        "Should have DisplayName property"
    );
}

#[test]
fn test_node_data_source_implementation() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let source = &output.node_data_source.1;

    // Check includes
    assert!(
        source.contains("#include \"CombatGraphNodeData.h\""),
        "Should include node data header"
    );

    // Check method implementations
    assert!(
        source.contains("FName UCombatGraphNodeData::GetNodeName"),
        "Should implement GetNodeName"
    );
    assert!(
        source.contains("FText UCombatGraphNodeData::GetNodeDisplayName"),
        "Should implement GetNodeDisplayName"
    );

    // Check fallback logic
    assert!(
        source.contains("DisplayName.IsEmpty()"),
        "Should check if DisplayName is empty"
    );
    assert!(
        source.contains("FText::FromName(NodeName)"),
        "Should fallback to NodeName"
    );
}

#[test]
fn test_api_macro_usage() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();

    // Check instance header
    let instance_header = &output.instance_header.1;
    assert!(
        instance_header.contains("COMBATPLUGIN_API"),
        "Instance should use API macro"
    );

    // Check node data header
    let node_data_header = &output.node_data_header.1;
    assert!(
        node_data_header.contains("COMBATPLUGIN_API"),
        "NodeData should use API macro"
    );
}

#[test]
fn test_uproperty_specifiers() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let header = &output.instance_header.1;

    // Check property specifiers
    assert!(
        header.contains("UPROPERTY(BlueprintAssignable)"),
        "Should have BlueprintAssignable delegate"
    );
    assert!(
        header.contains("UPROPERTY(Transient, BlueprintReadOnly"),
        "Should have transient state flags"
    );
    assert!(
        header.contains("UPROPERTY()"),
        "Should have private properties"
    );
}

#[test]
fn test_proceed_blocked_native_event() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();

    // Check header
    let header = &output.instance_header.1;
    assert!(
        header.contains("IsProceedBlocked"),
        "Should declare IsProceedBlocked"
    );
    assert!(
        header.contains("BlueprintNativeEvent"),
        "Should be native event"
    );

    // Check source
    let source = &output.instance_source.1;
    assert!(
        source.contains("IsProceedBlocked_Implementation"),
        "Should implement native event"
    );
    assert!(
        source.contains("return false"),
        "Should default to not blocked"
    );
}

#[test]
fn test_construct_instance_method() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();

    // Check header
    let header = &output.instance_header.1;
    assert!(
        header.contains("ConstructInstance"),
        "Should declare ConstructInstance"
    );

    // Check source
    let source = &output.instance_source.1;
    assert!(
        source.contains("bool UCombatGraphInstance::ConstructInstance"),
        "Should implement ConstructInstance"
    );
    assert!(
        source.contains("GraphAsset = InGraphAsset"),
        "Should store graph asset"
    );
    assert!(
        source.contains("bInstanceActive = false"),
        "Should initialize active flag"
    );
}

#[test]
fn test_begin_destroy_cleanup() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let source = &output.instance_source.1;

    // Check cleanup
    assert!(
        source.contains("void UCombatGraphInstance::BeginDestroy"),
        "Should implement BeginDestroy"
    );
    assert!(
        source.contains("Super::BeginDestroy()"),
        "Should call parent BeginDestroy"
    );
    assert!(
        source.contains("CurrentNode = nullptr"),
        "Should clear current node"
    );
    assert!(
        source.contains("GraphAsset = nullptr"),
        "Should clear graph asset"
    );
}

#[test]
fn test_set_instance_active() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let source = &output.instance_source.1;

    // Check method
    assert!(
        source.contains("void UCombatGraphInstance::SetInstanceActive"),
        "Should implement SetInstanceActive"
    );
    assert!(
        source.contains("bInstanceActive = bActive"),
        "Should set active flag"
    );
    assert!(
        source.contains("if (!bActive)"),
        "Should check for deactivation"
    );
    assert!(
        source.contains("CurrentNode = nullptr"),
        "Should reset on deactivation"
    );
}

#[test]
fn test_multiple_graphs_no_collision() {
    let graph1 = GraphEditor::new("CombatGraph");
    let graph2 = GraphEditor::new("DialogueGraph");

    let result1 = generate_graph_instance(&graph1, "CombatPlugin");
    let result2 = generate_graph_instance(&graph2, "DialoguePlugin");

    assert!(result1.is_ok());
    assert!(result2.is_ok());

    let output1 = result1.unwrap();
    let output2 = result2.unwrap();

    // Check that names don't collide
    assert!(output1.instance_header.1.contains("UCombatGraphInstance"));
    assert!(output2.instance_header.1.contains("UDialogueGraphInstance"));

    assert!(output1
        .instance_header
        .1
        .contains("ECombatGraphResetReason"));
    assert!(output2
        .instance_header
        .1
        .contains("EDialogueGraphResetReason"));
}

#[test]
fn test_category_naming() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let header = &output.instance_header.1;

    // Check category names
    assert!(
        header.contains("Category = \"CombatGraph|Instance\""),
        "Should use graph name in category"
    );

    let node_data_header = &output.node_data_header.1;
    assert!(
        node_data_header.contains("Category = \"CombatGraph|NodeData\""),
        "Should use graph name in NodeData category"
    );
}

#[test]
fn test_forward_declarations() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let header = &output.instance_header.1;

    // Check forward declarations
    assert!(
        header.contains("class UCombatGraphAsset;"),
        "Should forward declare asset class"
    );
    assert!(
        header.contains("class UCombatGraphNodeData;"),
        "Should forward declare node data class"
    );
}

#[test]
fn test_generated_body_macro() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();

    // Check instance header
    let instance_header = &output.instance_header.1;
    assert!(
        instance_header.contains("GENERATED_BODY()"),
        "Instance should have GENERATED_BODY"
    );

    // Check node data header
    let node_data_header = &output.node_data_header.1;
    assert!(
        node_data_header.contains("GENERATED_BODY()"),
        "NodeData should have GENERATED_BODY"
    );
}

#[test]
fn test_try_proceed_graph_logic() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let source = &output.instance_source.1;

    // Check TryProceedGraph implementation
    assert!(
        source.contains("bool UCombatGraphInstance::TryProceedGraph"),
        "Should implement TryProceedGraph"
    );
    assert!(
        source.contains("if (!IsValidInstance())"),
        "Should validate instance"
    );
    assert!(
        source.contains("if (IsProceedBlocked())"),
        "Should check if blocked"
    );
    assert!(
        source.contains("bNeedProceed = false"),
        "Should clear proceed flag"
    );
}

#[test]
fn test_set_current_node_validation() {
    let graph = create_combat_graph();
    let result = generate_graph_instance(&graph, "CombatPlugin");

    assert!(result.is_ok());
    let output = result.unwrap();
    let source = &output.instance_source.1;

    // Check SetCurrentNode implementation
    assert!(
        source.contains("bool UCombatGraphInstance::SetCurrentNode"),
        "Should implement SetCurrentNode"
    );
    assert!(
        source.contains("if (!IsValidInstance())"),
        "Should validate instance"
    );
    assert!(
        source.contains("CurrentNode = ToNode"),
        "Should set current node"
    );
    assert!(source.contains("return true"), "Should return success");
}
