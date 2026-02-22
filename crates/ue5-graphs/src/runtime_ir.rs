//! Runtime Graph Intermediate Representation (IR)
//!
//! This module defines the IR types that represent a runtime graph system
//! after conversion from KAIN AST but before code generation.
//!
//! Runtime graphs are different from editor graphs:
//! - Editor graphs: UEdGraph, UEdGraphNode, UEdGraphSchema (visual editing in UE5 Editor)
//! - Runtime graphs: UObject-based node instances that execute at runtime
//!
//! Runtime graphs generate:
//! - Node data classes (UMyNodeData : public UObject)
//! - Graph instance classes (UMyGraphInstance : public UObject)
//! - Pin definitions with type safety
//! - Execution logic for node processing

use serde::{Deserialize, Serialize};

/// Complete runtime graph definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeGraph {
    /// Graph name (e.g., "CombatGraph")
    pub name: String,
    
    /// Node type definitions
    pub node_types: Vec<RuntimeNodeData>,
    
    /// Graph instance definition
    pub instance_def: RuntimeInstance,
    
    /// Graph-level properties
    pub properties: RuntimeGraphProperties,
}

/// Runtime node data definition
/// Generates: UMyNodeData : public UObject
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeNodeData {
    /// Node type name (e.g., "DamageNode")
    pub name: String,
    
    /// Node category for organization (e.g., "Combat/Damage")
    pub category: String,
    
    /// Node properties (UPROPERTY fields)
    pub properties: Vec<RuntimeProperty>,
    
    /// Input pins
    pub input_pins: Vec<RuntimePin>,
    
    /// Output pins
    pub output_pins: Vec<RuntimePin>,
    
    /// Execution logic (C++ code or KAIN expression)
    pub execute_logic: Option<ExecuteLogic>,
    
    /// Node color (RGBA) for visualization
    pub color: Option<[f32; 4]>,
    
    /// Node icon path
    pub icon: Option<String>,
    
    /// Node tooltip
    pub tooltip: Option<String>,
}

/// Runtime graph instance definition
/// Generates: UMyGraphInstance : public UObject
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeInstance {
    /// Instance class name (e.g., "CombatGraphInstance")
    pub name: String,
    
    /// State fields (UPROPERTY fields)
    pub state_fields: Vec<RuntimeProperty>,
    
    /// Instance methods
    pub methods: Vec<RuntimeMethod>,
    
    /// Whether this instance is replicated
    pub is_replicated: bool,
    
    /// Whether this instance is savegame
    pub is_savegame: bool,
}

/// Runtime pin definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimePin {
    /// Pin name (e.g., "Execute", "Damage", "Target")
    pub name: String,
    
    /// Pin type
    pub pin_type: RuntimePinType,
    
    /// Is this an array pin?
    pub is_array: bool,
    
    /// Default value (optional)
    pub default_value: Option<String>,
    
    /// Pin tooltip
    pub tooltip: Option<String>,
    
    /// Pin direction (Input or Output)
    pub direction: PinDirection,
}

/// Pin direction
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum PinDirection {
    Input,
    Output,
}

/// Runtime pin type enumeration
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum RuntimePinType {
    /// Execution flow pin
    Exec,
    
    /// Boolean value
    Bool,
    
    /// Integer value (int32)
    Int,
    
    /// 64-bit integer
    Int64,
    
    /// Float value
    Float,
    
    /// String value
    String,
    
    /// Name value (FName)
    Name,
    
    /// Text value (FText)
    Text,
    
    /// Vector (FVector)
    Vector,
    
    /// Rotator (FRotator)
    Rotator,
    
    /// Transform (FTransform)
    Transform,
    
    /// Color (FLinearColor)
    Color,
    
    /// UObject reference (class name)
    Object(String),
    
    /// Struct value (struct name)
    Struct(String),
    
    /// Enum value (enum name)
    Enum(String),
    
    /// Wildcard (any type)
    Wildcard,
}

/// Runtime property definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeProperty {
    /// Property name
    pub name: String,
    
    /// Property type
    pub property_type: RuntimePinType,
    
    /// Is this an array property?
    pub is_array: bool,
    
    /// Default value (optional)
    pub default_value: Option<String>,
    
    /// UPROPERTY specifiers
    pub specifiers: Vec<PropertySpecifier>,
    
    /// Property tooltip
    pub tooltip: Option<String>,
}

/// UPROPERTY specifiers
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum PropertySpecifier {
    /// EditAnywhere
    EditAnywhere,
    
    /// EditDefaultsOnly
    EditDefaultsOnly,
    
    /// VisibleAnywhere
    VisibleAnywhere,
    
    /// BlueprintReadOnly
    BlueprintReadOnly,
    
    /// BlueprintReadWrite
    BlueprintReadWrite,
    
    /// Replicated
    Replicated,
    
    /// SaveGame
    SaveGame,
    
    /// Transient
    Transient,
    
    /// Category with name
    Category(String),
}

/// Runtime method definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeMethod {
    /// Method name
    pub name: String,
    
    /// Method parameters
    pub params: Vec<RuntimeParam>,
    
    /// Return type (None for void)
    pub return_type: Option<RuntimePinType>,
    
    /// Method body (C++ code or KAIN expression)
    pub body: String,
    
    /// UFUNCTION specifiers
    pub specifiers: Vec<FunctionSpecifier>,
}

/// Runtime method parameter
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeParam {
    /// Parameter name
    pub name: String,
    
    /// Parameter type
    pub param_type: RuntimePinType,
    
    /// Is this an array parameter?
    pub is_array: bool,
}

/// UFUNCTION specifiers
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum FunctionSpecifier {
    /// BlueprintCallable
    BlueprintCallable,
    
    /// BlueprintPure
    BlueprintPure,
    
    /// BlueprintNativeEvent
    BlueprintNativeEvent,
    
    /// Category with name
    Category(String),
}

/// Execution logic for a node
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ExecuteLogic {
    /// Inline C++ code
    CppCode(String),
    
    /// KAIN expression (to be converted to C++)
    KainExpr(String),
    
    /// Blueprint-callable function name
    BlueprintFunction(String),
}

/// Runtime graph properties
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RuntimeGraphProperties {
    /// Allow parallel execution of nodes?
    pub allow_parallel_execution: bool,
    
    /// Maximum execution depth (prevent infinite loops)
    pub max_execution_depth: i32,
    
    /// Enable debug logging?
    pub enable_debug_logging: bool,
    
    /// Graph execution mode
    pub execution_mode: ExecutionMode,
}

/// Graph execution mode
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum ExecutionMode {
    /// Sequential execution (one node at a time)
    Sequential,
    
    /// Parallel execution (multiple nodes simultaneously)
    Parallel,
    
    /// Event-driven execution (nodes execute on events)
    EventDriven,
}

impl Default for RuntimeGraphProperties {
    fn default() -> Self {
        Self {
            allow_parallel_execution: false,
            max_execution_depth: 100,
            enable_debug_logging: false,
            execution_mode: ExecutionMode::Sequential,
        }
    }
}

impl RuntimeGraph {
    /// Create a new runtime graph
    pub fn new(name: impl Into<String>) -> Self {
        let name_str = name.into();
        let instance_name = format!("{}Instance", name_str);
        Self {
            name: name_str,
            node_types: Vec::new(),
            instance_def: RuntimeInstance::new(instance_name),
            properties: RuntimeGraphProperties::default(),
        }
    }
    
    /// Add a node type
    pub fn add_node_type(&mut self, node_type: RuntimeNodeData) {
        self.node_types.push(node_type);
    }
    
    /// Find a node type by name
    pub fn find_node_type(&self, name: &str) -> Option<&RuntimeNodeData> {
        self.node_types.iter().find(|nt| nt.name == name)
    }
}

impl RuntimeInstance {
    /// Create a new runtime instance
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            state_fields: Vec::new(),
            methods: Vec::new(),
            is_replicated: false,
            is_savegame: false,
        }
    }
    
    /// Add a state field
    pub fn add_state_field(&mut self, field: RuntimeProperty) {
        self.state_fields.push(field);
    }
    
    /// Add a method
    pub fn add_method(&mut self, method: RuntimeMethod) {
        self.methods.push(method);
    }
}

impl RuntimeNodeData {
    /// Create a new runtime node data
    pub fn new(name: impl Into<String>, category: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            category: category.into(),
            properties: Vec::new(),
            input_pins: Vec::new(),
            output_pins: Vec::new(),
            execute_logic: None,
            color: None,
            icon: None,
            tooltip: None,
        }
    }
    
    /// Add an input pin
    pub fn add_input_pin(&mut self, pin: RuntimePin) {
        self.input_pins.push(pin);
    }
    
    /// Add an output pin
    pub fn add_output_pin(&mut self, pin: RuntimePin) {
        self.output_pins.push(pin);
    }
    
    /// Add a property
    pub fn add_property(&mut self, property: RuntimeProperty) {
        self.properties.push(property);
    }
}

impl RuntimePin {
    /// Create a new input pin
    pub fn input(name: impl Into<String>, pin_type: RuntimePinType) -> Self {
        Self {
            name: name.into(),
            pin_type,
            is_array: false,
            default_value: None,
            tooltip: None,
            direction: PinDirection::Input,
        }
    }
    
    /// Create a new output pin
    pub fn output(name: impl Into<String>, pin_type: RuntimePinType) -> Self {
        Self {
            name: name.into(),
            pin_type,
            is_array: false,
            default_value: None,
            tooltip: None,
            direction: PinDirection::Output,
        }
    }
    
    /// Make this pin an array
    pub fn as_array(mut self) -> Self {
        self.is_array = true;
        self
    }
    
    /// Set default value
    pub fn with_default(mut self, default: impl Into<String>) -> Self {
        self.default_value = Some(default.into());
        self
    }
    
    /// Set tooltip
    pub fn with_tooltip(mut self, tooltip: impl Into<String>) -> Self {
        self.tooltip = Some(tooltip.into());
        self
    }
}
