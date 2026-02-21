//! Graph Editor Intermediate Representation (IR)
//!
//! This module defines the IR types that represent a graph editor
//! after conversion from KAIN AST but before code generation.

use serde::{Deserialize, Serialize};

/// Complete graph editor definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GraphEditor {
    /// Graph editor name (e.g., "CombatGraph")
    pub name: String,
    
    /// Node type definitions
    pub node_types: Vec<NodeType>,
    
    /// Schema rules for connections
    pub schema: GraphSchema,
    
    /// Graph-level properties
    pub properties: GraphProperties,
}

/// Node type definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeType {
    /// Node type name (e.g., "InputNode")
    pub name: String,
    
    /// Category for context menu (e.g., "Combat/Input")
    pub category: String,
    
    /// Input pins
    pub inputs: Vec<PinDefinition>,
    
    /// Output pins
    pub outputs: Vec<PinDefinition>,
    
    /// Node color (RGBA)
    pub color: Option<[f32; 4]>,
    
    /// Node icon path
    pub icon: Option<String>,
    
    /// Node tooltip
    pub tooltip: Option<String>,
    
    /// Execution logic (optional)
    pub execution_logic: Option<String>,
}

/// Pin definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PinDefinition {
    /// Pin name (e.g., "Execute", "Damage")
    pub name: String,
    
    /// Pin type
    pub pin_type: PinType,
    
    /// Is this an array pin?
    pub is_array: bool,
    
    /// Default value (optional)
    pub default_value: Option<String>,
    
    /// Pin tooltip
    pub tooltip: Option<String>,
}

/// Pin type enumeration
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum PinType {
    /// Execution flow pin
    Exec,
    
    /// Boolean value
    Bool,
    
    /// Integer value
    Int,
    
    /// Float value
    Float,
    
    /// String value
    String,
    
    /// UObject reference (class name)
    Object(String),
    
    /// Struct value (struct name)
    Struct(String),
    
    /// Enum value (enum name)
    Enum(String),
    
    /// Wildcard (any type)
    Wildcard,
}

/// Graph schema definition
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GraphSchema {
    /// Allowed pin connections
    pub allowed_connections: Vec<ConnectionRule>,
    
    /// Context menu actions
    pub context_actions: Vec<ContextAction>,
    
    /// Custom validation rules
    pub validation_rules: Vec<ValidationRule>,
}

/// Connection rule
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConnectionRule {
    /// Source pin type
    pub from: PinType,
    
    /// Target pin type
    pub to: PinType,
    
    /// Is this connection allowed?
    pub allowed: bool,
    
    /// Error message if not allowed
    pub error_message: Option<String>,
}

/// Context menu action
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContextAction {
    /// Action label
    pub label: String,
    
    /// Action category
    pub category: String,
    
    /// Action tooltip
    pub tooltip: Option<String>,
    
    /// Action implementation
    pub implementation: String,
}

/// Validation rule
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ValidationRule {
    /// Rule name
    pub name: String,
    
    /// Rule description
    pub description: String,
    
    /// Rule implementation
    pub implementation: String,
}

/// Graph-level properties
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GraphProperties {
    /// Allow multiple connections to input pins?
    pub allow_multiple_input_connections: bool,
    
    /// Allow multiple connections from output pins?
    pub allow_multiple_output_connections: bool,
    
    /// Allow cycles in the graph?
    pub allow_cycles: bool,
    
    /// Grid snap size
    pub grid_snap_size: i32,
}

impl Default for GraphProperties {
    fn default() -> Self {
        Self {
            allow_multiple_input_connections: false,
            allow_multiple_output_connections: true,
            allow_cycles: false,
            grid_snap_size: 16,
        }
    }
}

impl GraphEditor {
    /// Create a new graph editor
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            node_types: Vec::new(),
            schema: GraphSchema::default(),
            properties: GraphProperties::default(),
        }
    }
    
    /// Add a node type
    pub fn add_node_type(&mut self, node_type: NodeType) {
        self.node_types.push(node_type);
    }
    
    /// Find a node type by name
    pub fn find_node_type(&self, name: &str) -> Option<&NodeType> {
        self.node_types.iter().find(|nt| nt.name == name)
    }
}

impl Default for GraphSchema {
    fn default() -> Self {
        Self {
            allowed_connections: vec![
                // Default: Exec can connect to Exec
                ConnectionRule {
                    from: PinType::Exec,
                    to: PinType::Exec,
                    allowed: true,
                    error_message: None,
                },
                // Default: Same types can connect
                ConnectionRule {
                    from: PinType::Bool,
                    to: PinType::Bool,
                    allowed: true,
                    error_message: None,
                },
                ConnectionRule {
                    from: PinType::Int,
                    to: PinType::Int,
                    allowed: true,
                    error_message: None,
                },
                ConnectionRule {
                    from: PinType::Float,
                    to: PinType::Float,
                    allowed: true,
                    error_message: None,
                },
                ConnectionRule {
                    from: PinType::String,
                    to: PinType::String,
                    allowed: true,
                    error_message: None,
                },
            ],
            context_actions: Vec::new(),
            validation_rules: Vec::new(),
        }
    }
}
