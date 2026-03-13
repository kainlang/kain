//! Built-in Node Types
//!
//! Common node types that can be reused across graph editors

use crate::{NodeType, PinDefinition, PinType};

/// Create a basic execution node
pub fn create_exec_node(name: impl Into<String>) -> NodeType {
    NodeType {
        name: name.into(),
        category: "Flow Control".to_string(),
        inputs: vec![PinDefinition {
            name: "Execute".to_string(),
            pin_type: PinType::Exec,
            is_array: false,
            default_value: None,
            tooltip: None,
        }],
        outputs: vec![PinDefinition {
            name: "Then".to_string(),
            pin_type: PinType::Exec,
            is_array: false,
            default_value: None,
            tooltip: None,
        }],
        properties: Vec::new(),
        color: Some([1.0, 1.0, 1.0, 1.0]),
        icon: None,
        tooltip: None,
        execution_logic: None,
    }
}

/// Create a basic math node
pub fn create_math_node(name: impl Into<String>, operation: &str) -> NodeType {
    NodeType {
        name: name.into(),
        category: "Math".to_string(),
        inputs: vec![
            PinDefinition {
                name: "A".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: Some("0.0".to_string()),
                tooltip: None,
            },
            PinDefinition {
                name: "B".to_string(),
                pin_type: PinType::Float,
                is_array: false,
                default_value: Some("0.0".to_string()),
                tooltip: None,
            },
        ],
        outputs: vec![PinDefinition {
            name: "Result".to_string(),
            pin_type: PinType::Float,
            is_array: false,
            default_value: None,
            tooltip: None,
        }],
        properties: Vec::new(),
        color: Some([0.0, 1.0, 0.0, 1.0]),
        icon: None,
        tooltip: Some(format!("Performs {} operation", operation)),
        execution_logic: Some(format!("Result = A {} B", operation)),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_create_exec_node() {
        let node = create_exec_node("TestNode");
        assert_eq!(node.name, "TestNode");
        assert_eq!(node.inputs.len(), 1);
        assert_eq!(node.outputs.len(), 1);
    }

    #[test]
    fn test_create_math_node() {
        let node = create_math_node("Add", "+");
        assert_eq!(node.name, "Add");
        assert_eq!(node.inputs.len(), 2);
        assert_eq!(node.outputs.len(), 1);
    }
}
