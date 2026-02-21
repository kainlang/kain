//! AST to IR Converter
//!
//! Converts KAIN AST (GraphEditorDef) to Graph IR (GraphEditor)

use crate::graph_ir::*;
use crate::{GraphError, Result};
use kain_core::ast::{
    Attribute, Expr, GraphEditorDef, GraphSchemaDef, NodeTypeDef, PinDef, Type,
};
use std::collections::HashMap;

/// Converts KAIN AST graph editor definitions to GraphEditor IR
pub struct GraphEditorConverter {
    /// Track extracted properties for validation
    properties: GraphProperties,
}

impl GraphEditorConverter {
    pub fn new() -> Self {
        Self {
            properties: GraphProperties::default(),
        }
    }

    /// Convert a GraphEditorDef AST node to GraphEditor IR
    pub fn convert(&mut self, def: &GraphEditorDef) -> Result<GraphEditor> {
        let mut graph = GraphEditor::new(def.name.clone());

        // Extract properties from attributes
        graph.properties = self.extract_properties(&def.attributes)?;

        // Convert node types
        for node_type_def in &def.node_types {
            let node_type = self.convert_node_type(node_type_def)?;
            graph.add_node_type(node_type);
        }

        // Convert schema if present
        if let Some(schema_def) = &def.schema {
            graph.schema = self.convert_schema(schema_def)?;
        } else {
            // Use default schema
            graph.schema = GraphSchema::default();
        }

        // Validate the graph
        self.validate_graph(&graph)?;

        Ok(graph)
    }

    /// Extract graph properties from attributes
    fn extract_properties(&self, attributes: &[Attribute]) -> Result<GraphProperties> {
        let mut props = GraphProperties::default();

        for attr in attributes {
            match attr.name.as_str() {
                "allow_multiple_inputs" => {
                    props.allow_multiple_input_connections = self.extract_bool_arg(&attr.args, 0)?;
                }
                "allow_multiple_outputs" => {
                    props.allow_multiple_output_connections =
                        self.extract_bool_arg(&attr.args, 0)?;
                }
                "allow_cycles" => {
                    props.allow_cycles = self.extract_bool_arg(&attr.args, 0)?;
                }
                "grid_snap" => {
                    props.grid_snap_size = self.extract_int_arg(&attr.args, 0)?;
                }
                _ => {
                    // Ignore unknown attributes (they might be for other purposes)
                }
            }
        }

        Ok(props)
    }

    /// Convert a node type definition
    fn convert_node_type(&self, def: &NodeTypeDef) -> Result<NodeType> {
        let mut node_type = NodeType {
            name: def.name.clone(),
            category: def.category.clone().unwrap_or_else(|| "Default".to_string()),
            inputs: Vec::new(),
            outputs: Vec::new(),
            color: None,
            icon: None,
            tooltip: None,
            execution_logic: None,
        };

        // Convert input pins
        for pin_def in &def.inputs {
            let pin = self.convert_pin(pin_def)?;
            node_type.inputs.push(pin);
        }

        // Convert output pins
        for pin_def in &def.outputs {
            let pin = self.convert_pin(pin_def)?;
            node_type.outputs.push(pin);
        }

        // Extract node properties from attributes
        for attr in &def.attributes {
            match attr.name.as_str() {
                "category" => {
                    node_type.category = self.extract_string_arg(&attr.args, 0)?;
                }
                "color" => {
                    node_type.color = Some(self.extract_color_arg(&attr.args)?);
                }
                "icon" => {
                    node_type.icon = Some(self.extract_string_arg(&attr.args, 0)?);
                }
                "tooltip" => {
                    node_type.tooltip = Some(self.extract_string_arg(&attr.args, 0)?);
                }
                _ => {}
            }
        }

        Ok(node_type)
    }

    /// Convert a pin definition
    fn convert_pin(&self, def: &PinDef) -> Result<PinDefinition> {
        let pin_type = self.convert_type(&def.ty)?;

        let mut pin = PinDefinition {
            name: def.name.clone(),
            pin_type,
            is_array: def.is_array,
            default_value: None,
            tooltip: None,
        };

        // Extract default value if present
        if let Some(default_expr) = &def.default {
            pin.default_value = Some(self.expr_to_string(default_expr)?);
        }

        // Extract tooltip from attributes
        for attr in &def.attributes {
            if attr.name == "tooltip" {
                pin.tooltip = Some(self.extract_string_arg(&attr.args, 0)?);
            }
        }

        Ok(pin)
    }

    /// Convert KAIN type to PinType
    fn convert_type(&self, ty: &Type) -> Result<PinType> {
        match ty {
            Type::Named { name, .. } => match name.as_str() {
                "Exec" => Ok(PinType::Exec),
                "Bool" => Ok(PinType::Bool),
                "Int" => Ok(PinType::Int),
                "Float" => Ok(PinType::Float),
                "String" => Ok(PinType::String),
                "Wildcard" => Ok(PinType::Wildcard),
                _ => {
                    // Check if it's an object, struct, or enum type
                    // For now, treat unknown types as Object
                    Ok(PinType::Object(name.clone()))
                }
            },
            Type::Array(inner, _, _) => {
                // Arrays are handled by is_array flag, so just convert the element type
                self.convert_type(inner)
            }
            _ => Err(GraphError::ASTConversion(format!(
                "Unsupported pin type: {:?}",
                ty
            ))),
        }
    }

    /// Convert schema definition
    fn convert_schema(&self, def: &GraphSchemaDef) -> Result<GraphSchema> {
        let mut schema = GraphSchema::default();

        // Convert schema rules to validation rules
        for rule in &def.rules {
            let validation_rule = ValidationRule {
                name: rule.name.clone(),
                description: format!("Validation rule: {}", rule.name),
                implementation: self.expr_to_string(&rule.condition)?,
            };
            schema.validation_rules.push(validation_rule);
        }

        Ok(schema)
    }

    /// Validate the graph IR
    fn validate_graph(&self, graph: &GraphEditor) -> Result<()> {
        // Check for duplicate node type names
        let mut seen_names = HashMap::new();
        for node_type in &graph.node_types {
            if let Some(_) = seen_names.insert(&node_type.name, ()) {
                return Err(GraphError::IRValidation(format!(
                    "Duplicate node type name: {}",
                    node_type.name
                )));
            }
        }

        // Validate each node type
        for node_type in &graph.node_types {
            self.validate_node_type(node_type)?;
        }

        Ok(())
    }

    /// Validate a node type
    fn validate_node_type(&self, node_type: &NodeType) -> Result<()> {
        // Check for duplicate pin names within inputs
        let mut seen_input_names = HashMap::new();
        for pin in &node_type.inputs {
            if seen_input_names.insert(&pin.name, ()).is_some() {
                return Err(GraphError::IRValidation(format!(
                    "Duplicate input pin name '{}' in node type '{}'",
                    pin.name, node_type.name
                )));
            }
        }

        // Check for duplicate pin names within outputs
        let mut seen_output_names = HashMap::new();
        for pin in &node_type.outputs {
            if seen_output_names.insert(&pin.name, ()).is_some() {
                return Err(GraphError::IRValidation(format!(
                    "Duplicate output pin name '{}' in node type '{}'",
                    pin.name, node_type.name
                )));
            }
        }

        Ok(())
    }

    // === Helper methods for extracting attribute arguments ===

    fn extract_bool_arg(&self, args: &[Expr], index: usize) -> Result<bool> {
        if index >= args.len() {
            return Err(GraphError::ASTConversion(format!(
                "Missing argument at index {}",
                index
            )));
        }

        match &args[index] {
            Expr::Bool(val, _) => Ok(*val),
            _ => Err(GraphError::ASTConversion(
                "Expected boolean argument".to_string(),
            )),
        }
    }

    fn extract_int_arg(&self, args: &[Expr], index: usize) -> Result<i32> {
        if index >= args.len() {
            return Err(GraphError::ASTConversion(format!(
                "Missing argument at index {}",
                index
            )));
        }

        match &args[index] {
            Expr::Int(val, _) => Ok(*val as i32),
            _ => Err(GraphError::ASTConversion(
                "Expected integer argument".to_string(),
            )),
        }
    }

    fn extract_string_arg(&self, args: &[Expr], index: usize) -> Result<String> {
        if index >= args.len() {
            return Err(GraphError::ASTConversion(format!(
                "Missing argument at index {}",
                index
            )));
        }

        match &args[index] {
            Expr::String(val, _) => Ok(val.clone()),
            _ => Err(GraphError::ASTConversion(
                "Expected string argument".to_string(),
            )),
        }
    }

    fn extract_color_arg(&self, args: &[Expr]) -> Result<[f32; 4]> {
        if args.len() < 4 {
            return Err(GraphError::ASTConversion(
                "Color requires 4 arguments (RGBA)".to_string(),
            ));
        }

        let r = self.extract_float_from_expr(&args[0])?;
        let g = self.extract_float_from_expr(&args[1])?;
        let b = self.extract_float_from_expr(&args[2])?;
        let a = self.extract_float_from_expr(&args[3])?;

        Ok([r, g, b, a])
    }

    fn extract_float_from_expr(&self, expr: &Expr) -> Result<f32> {
        match expr {
            Expr::Float(val, _) => Ok(*val as f32),
            Expr::Int(val, _) => Ok(*val as f32),
            _ => Err(GraphError::ASTConversion(
                "Expected numeric value for color component".to_string(),
            )),
        }
    }

    /// Convert an expression to a string representation
    fn expr_to_string(&self, expr: &Expr) -> Result<String> {
        match expr {
            Expr::Int(val, _) => Ok(val.to_string()),
            Expr::Float(val, _) => Ok(val.to_string()),
            Expr::String(val, _) => Ok(val.clone()),
            Expr::Bool(val, _) => Ok(val.to_string()),
            Expr::Ident(name, _) => Ok(name.clone()),
            _ => Ok(format!("{:?}", expr)), // Fallback for complex expressions
        }
    }
}

impl Default for GraphEditorConverter {
    fn default() -> Self {
        Self::new()
    }
}

/// Convenience function for converting AST to IR
pub fn convert_graph_editor(ast: &GraphEditorDef) -> Result<GraphEditor> {
    let mut converter = GraphEditorConverter::new();
    converter.convert(ast)
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{GraphEditorDef, NodeTypeDef, PinDef, Type};
    use kain_core::span::Span;

    fn dummy_span() -> Span {
        Span::new(0, 0)
    }

    #[test]
    fn test_convert_simple_graph() {
        let ast = GraphEditorDef {
            name: "TestGraph".to_string(),
            attributes: vec![],
            node_types: vec![],
            schema: None,
            span: dummy_span(),
        };

        let result = convert_graph_editor(&ast);
        assert!(result.is_ok());

        let graph = result.unwrap();
        assert_eq!(graph.name, "TestGraph");
        assert_eq!(graph.node_types.len(), 0);
    }

    #[test]
    fn test_convert_node_type() {
        let node_def = NodeTypeDef {
            name: "TestNode".to_string(),
            category: Some("Test".to_string()),
            inputs: vec![PinDef {
                name: "Input1".to_string(),
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
            outputs: vec![PinDef {
                name: "Output1".to_string(),
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
            name: "TestGraph".to_string(),
            attributes: vec![],
            node_types: vec![node_def],
            schema: None,
            span: dummy_span(),
        };

        let result = convert_graph_editor(&ast);
        assert!(result.is_ok());

        let graph = result.unwrap();
        assert_eq!(graph.node_types.len(), 1);
        assert_eq!(graph.node_types[0].name, "TestNode");
        assert_eq!(graph.node_types[0].inputs.len(), 1);
        assert_eq!(graph.node_types[0].outputs.len(), 1);
    }

    #[test]
    fn test_convert_pin_types() {
        let converter = GraphEditorConverter::new();

        let float_type = Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: dummy_span(),
        };
        assert_eq!(
            converter.convert_type(&float_type).unwrap(),
            PinType::Float
        );

        let exec_type = Type::Named {
            name: "Exec".to_string(),
            generics: vec![],
            span: dummy_span(),
        };
        assert_eq!(converter.convert_type(&exec_type).unwrap(), PinType::Exec);

        let bool_type = Type::Named {
            name: "Bool".to_string(),
            generics: vec![],
            span: dummy_span(),
        };
        assert_eq!(converter.convert_type(&bool_type).unwrap(), PinType::Bool);
    }

    #[test]
    fn test_validation_duplicate_node_names() {
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
            name: "TestGraph".to_string(),
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
    fn test_validation_duplicate_pin_names() {
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
            name: "TestNode".to_string(),
            category: None,
            inputs: vec![pin_def.clone(), pin_def],
            outputs: vec![],
            properties: vec![],
            attributes: vec![],
            span: dummy_span(),
        };

        let ast = GraphEditorDef {
            name: "TestGraph".to_string(),
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
}