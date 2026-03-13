//! Tests for Runtime Graph IR

use ue5_graphs::runtime_ir::*;

#[test]
fn test_runtime_graph_creation() {
    let graph = RuntimeGraph::new("TestGraph");

    assert_eq!(graph.name, "TestGraph");
    assert_eq!(graph.node_types.len(), 0);
    assert_eq!(graph.instance_def.name, "TestGraphInstance");
}

#[test]
fn test_runtime_node_data_creation() {
    let node = RuntimeNodeData::new("DamageNode", "Combat/Damage");

    assert_eq!(node.name, "DamageNode");
    assert_eq!(node.category, "Combat/Damage");
    assert_eq!(node.input_pins.len(), 0);
    assert_eq!(node.output_pins.len(), 0);
    assert_eq!(node.properties.len(), 0);
}

#[test]
fn test_runtime_pin_creation() {
    let input_pin = RuntimePin::input("Damage", RuntimePinType::Float);
    assert_eq!(input_pin.name, "Damage");
    assert_eq!(input_pin.pin_type, RuntimePinType::Float);
    assert_eq!(input_pin.direction, PinDirection::Input);
    assert!(!input_pin.is_array);

    let output_pin = RuntimePin::output("Result", RuntimePinType::Bool);
    assert_eq!(output_pin.name, "Result");
    assert_eq!(output_pin.pin_type, RuntimePinType::Bool);
    assert_eq!(output_pin.direction, PinDirection::Output);
}

#[test]
fn test_runtime_pin_array() {
    let pin = RuntimePin::input("Targets", RuntimePinType::Object("AActor".to_string())).as_array();

    assert!(pin.is_array);
    assert_eq!(pin.name, "Targets");
}

#[test]
fn test_runtime_pin_with_default() {
    let pin = RuntimePin::input("Multiplier", RuntimePinType::Float).with_default("1.0");

    assert_eq!(pin.default_value, Some("1.0".to_string()));
}

#[test]
fn test_runtime_pin_with_tooltip() {
    let pin =
        RuntimePin::input("Health", RuntimePinType::Float).with_tooltip("Current health value");

    assert_eq!(pin.tooltip, Some("Current health value".to_string()));
}

#[test]
fn test_runtime_instance_creation() {
    let instance = RuntimeInstance::new("CombatGraphInstance");

    assert_eq!(instance.name, "CombatGraphInstance");
    assert_eq!(instance.state_fields.len(), 0);
    assert_eq!(instance.methods.len(), 0);
    assert!(!instance.is_replicated);
    assert!(!instance.is_savegame);
}

#[test]
fn test_add_node_to_graph() {
    let mut graph = RuntimeGraph::new("TestGraph");
    let node = RuntimeNodeData::new("TestNode", "Test");

    graph.add_node_type(node);

    assert_eq!(graph.node_types.len(), 1);
    assert_eq!(graph.node_types[0].name, "TestNode");
}

#[test]
fn test_find_node_type() {
    let mut graph = RuntimeGraph::new("TestGraph");
    let node1 = RuntimeNodeData::new("Node1", "Test");
    let node2 = RuntimeNodeData::new("Node2", "Test");

    graph.add_node_type(node1);
    graph.add_node_type(node2);

    let found = graph.find_node_type("Node1");
    assert!(found.is_some());
    assert_eq!(found.unwrap().name, "Node1");

    let not_found = graph.find_node_type("Node3");
    assert!(not_found.is_none());
}

#[test]
fn test_runtime_property_creation() {
    let property = RuntimeProperty {
        name: "Damage".to_string(),
        property_type: RuntimePinType::Float,
        is_array: false,
        default_value: Some("10.0".to_string()),
        specifiers: vec![
            PropertySpecifier::EditAnywhere,
            PropertySpecifier::Category("Combat".to_string()),
        ],
        tooltip: Some("Base damage value".to_string()),
    };

    assert_eq!(property.name, "Damage");
    assert_eq!(property.property_type, RuntimePinType::Float);
    assert_eq!(property.specifiers.len(), 2);
}

#[test]
fn test_runtime_method_creation() {
    let method = RuntimeMethod {
        name: "CalculateDamage".to_string(),
        params: vec![
            RuntimeParam {
                name: "BaseDamage".to_string(),
                param_type: RuntimePinType::Float,
                is_array: false,
            },
            RuntimeParam {
                name: "Multiplier".to_string(),
                param_type: RuntimePinType::Float,
                is_array: false,
            },
        ],
        return_type: Some(RuntimePinType::Float),
        body: "return BaseDamage * Multiplier;".to_string(),
        specifiers: vec![
            FunctionSpecifier::BlueprintCallable,
            FunctionSpecifier::Category("Combat".to_string()),
        ],
    };

    assert_eq!(method.name, "CalculateDamage");
    assert_eq!(method.params.len(), 2);
    assert!(method.return_type.is_some());
}

#[test]
fn test_execution_logic_types() {
    let cpp_logic = ExecuteLogic::CppCode("UE_LOG(LogTemp, Log, TEXT(\"Test\"));".to_string());
    let kain_logic = ExecuteLogic::KainExpr("println(\"Test\")".to_string());
    let bp_logic = ExecuteLogic::BlueprintFunction("ExecuteCustomLogic".to_string());

    match cpp_logic {
        ExecuteLogic::CppCode(_) => assert!(true),
        _ => panic!("Expected CppCode"),
    }

    match kain_logic {
        ExecuteLogic::KainExpr(_) => assert!(true),
        _ => panic!("Expected KainExpr"),
    }

    match bp_logic {
        ExecuteLogic::BlueprintFunction(_) => assert!(true),
        _ => panic!("Expected BlueprintFunction"),
    }
}

#[test]
fn test_graph_properties_default() {
    let props = RuntimeGraphProperties::default();

    assert!(!props.allow_parallel_execution);
    assert_eq!(props.max_execution_depth, 100);
    assert!(!props.enable_debug_logging);
    assert_eq!(props.execution_mode, ExecutionMode::Sequential);
}

#[test]
fn test_execution_modes() {
    let sequential = ExecutionMode::Sequential;
    let parallel = ExecutionMode::Parallel;
    let event_driven = ExecutionMode::EventDriven;

    assert_eq!(sequential, ExecutionMode::Sequential);
    assert_eq!(parallel, ExecutionMode::Parallel);
    assert_eq!(event_driven, ExecutionMode::EventDriven);
}

#[test]
fn test_pin_type_variants() {
    let exec = RuntimePinType::Exec;
    let bool_type = RuntimePinType::Bool;
    let int_type = RuntimePinType::Int;
    let float_type = RuntimePinType::Float;
    let string_type = RuntimePinType::String;
    let vector_type = RuntimePinType::Vector;
    let object_type = RuntimePinType::Object("AActor".to_string());
    let struct_type = RuntimePinType::Struct("FVector".to_string());
    let enum_type = RuntimePinType::Enum("EItemRarity".to_string());

    assert_eq!(exec, RuntimePinType::Exec);
    assert_eq!(bool_type, RuntimePinType::Bool);
    assert_eq!(int_type, RuntimePinType::Int);
    assert_eq!(float_type, RuntimePinType::Float);
    assert_eq!(string_type, RuntimePinType::String);
    assert_eq!(vector_type, RuntimePinType::Vector);

    match object_type {
        RuntimePinType::Object(name) => assert_eq!(name, "AActor"),
        _ => panic!("Expected Object type"),
    }
}

#[test]
fn test_complex_node_setup() {
    let mut node = RuntimeNodeData::new("CombatNode", "Combat/Actions");

    // Add input pins
    node.add_input_pin(RuntimePin::input("Execute", RuntimePinType::Exec));
    node.add_input_pin(RuntimePin::input(
        "Target",
        RuntimePinType::Object("AActor".to_string()),
    ));
    node.add_input_pin(RuntimePin::input("Damage", RuntimePinType::Float).with_default("10.0"));

    // Add output pins
    node.add_output_pin(RuntimePin::output("OnComplete", RuntimePinType::Exec));
    node.add_output_pin(RuntimePin::output("Success", RuntimePinType::Bool));

    // Add properties
    node.add_property(RuntimeProperty {
        name: "CriticalHitChance".to_string(),
        property_type: RuntimePinType::Float,
        is_array: false,
        default_value: Some("0.1".to_string()),
        specifiers: vec![PropertySpecifier::EditAnywhere],
        tooltip: Some("Chance for critical hit".to_string()),
    });

    assert_eq!(node.input_pins.len(), 3);
    assert_eq!(node.output_pins.len(), 2);
    assert_eq!(node.properties.len(), 1);
}

#[test]
fn test_instance_with_state_and_methods() {
    let mut instance = RuntimeInstance::new("CombatGraphInstance");

    // Add state fields
    instance.add_state_field(RuntimeProperty {
        name: "CurrentHealth".to_string(),
        property_type: RuntimePinType::Float,
        is_array: false,
        default_value: Some("100.0".to_string()),
        specifiers: vec![PropertySpecifier::Replicated, PropertySpecifier::SaveGame],
        tooltip: None,
    });

    // Add methods
    instance.add_method(RuntimeMethod {
        name: "TakeDamage".to_string(),
        params: vec![RuntimeParam {
            name: "Amount".to_string(),
            param_type: RuntimePinType::Float,
            is_array: false,
        }],
        return_type: None,
        body: "CurrentHealth -= Amount;".to_string(),
        specifiers: vec![FunctionSpecifier::BlueprintCallable],
    });

    instance.is_replicated = true;
    instance.is_savegame = true;

    assert_eq!(instance.state_fields.len(), 1);
    assert_eq!(instance.methods.len(), 1);
    assert!(instance.is_replicated);
    assert!(instance.is_savegame);
}
