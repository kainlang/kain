// ============================================================================
// Attribute Set IR — Intermediate Representation for GAS Attribute Sets
// ============================================================================
// Converts AST attribute sets into a structured IR for codegen.
// Handles attribute metadata, replication, lifecycle hooks, and delegates.
// ============================================================================

use kain_core::ast::{Struct, Field, Function, Type};
use kain_core::error::{KainError, KainResult};
use kain_core::span::Span;

/// Intermediate representation of an attribute set
#[derive(Debug, Clone)]
pub struct AttributeSetIR {
    pub name: String,
    pub attributes: Vec<AttributeIR>,
    pub lifecycle_hooks: LifecycleHooksIR,
    pub delegates: Vec<DelegateIR>,
}

/// Intermediate representation of a single attribute
#[derive(Debug, Clone)]
pub struct AttributeIR {
    pub name: String,
    pub ty: Type,
    pub default_value: Option<String>,
    pub replicated: bool,
    pub rep_notify: bool,
    pub hide_from_modifiers: bool,
    pub is_meta: bool,
    pub clamp_min: Option<f32>,
    pub clamp_max: Option<f32>,
    pub category: String,
}

/// Lifecycle hooks for attribute sets
#[derive(Debug, Clone, Default)]
pub struct LifecycleHooksIR {
    pub pre_gameplay_effect_execute: Option<FunctionIR>,
    pub post_gameplay_effect_execute: Option<FunctionIR>,
    pub pre_attribute_change: Option<FunctionIR>,
    pub post_attribute_change: Option<FunctionIR>,
}

/// Simplified function IR for lifecycle hooks
#[derive(Debug, Clone)]
pub struct FunctionIR {
    pub name: String,
    pub body: String,
}

/// Delegate IR for attribute events
#[derive(Debug, Clone)]
pub struct DelegateIR {
    pub name: String,
    pub delegate_type: String,
}

impl AttributeSetIR {
    /// Convert AST struct to AttributeSetIR
    pub fn from_ast(struct_def: &Struct) -> KainResult<Self> {
        // Verify @attribute_set attribute
        if !struct_def.attributes.iter().any(|a| a.name == "attribute_set") {
            return Err(KainError::codegen(
                format!("Struct '{}' must have @attribute_set attribute", struct_def.name),
                struct_def.span,
            ));
        }

        let mut attributes = Vec::new();
        let mut delegates = Vec::new();
        let mut lifecycle_hooks = LifecycleHooksIR::default();

        // Parse fields (attributes and delegates)
        for field in &struct_def.fields {
            if field.attributes.iter().any(|a| a.name == "delegate") {
                delegates.push(DelegateIR::from_field(field)?);
            } else if field.attributes.iter().any(|a| a.name == "attribute") {
                attributes.push(AttributeIR::from_field(field)?);
            } else {
                // Default to attribute if no explicit marker
                attributes.push(AttributeIR::from_field(field)?);
            }
        }

        // Parse lifecycle hooks from methods
        for method in &struct_def.methods {
            match method.name.as_str() {
                "pre_gameplay_effect_execute" => {
                    lifecycle_hooks.pre_gameplay_effect_execute = Some(FunctionIR::from_ast(method)?);
                }
                "post_gameplay_effect_execute" => {
                    lifecycle_hooks.post_gameplay_effect_execute = Some(FunctionIR::from_ast(method)?);
                }
                "pre_attribute_change" => {
                    lifecycle_hooks.pre_attribute_change = Some(FunctionIR::from_ast(method)?);
                }
                "post_attribute_change" => {
                    lifecycle_hooks.post_attribute_change = Some(FunctionIR::from_ast(method)?);
                }
                _ => {
                    return Err(KainError::codegen(
                        format!("Unknown lifecycle hook: {}", method.name),
                        method.span,
                    ));
                }
            }
        }

        Ok(AttributeSetIR {
            name: struct_def.name.clone(),
            attributes,
            lifecycle_hooks,
            delegates,
        })
    }
}

impl AttributeIR {
    /// Convert AST field to AttributeIR
    fn from_field(field: &Field) -> KainResult<Self> {
        let mut replicated = false;
        let mut rep_notify = false;
        let mut hide_from_modifiers = false;
        let mut is_meta = false;
        let clamp_min = None;
        let clamp_max = None;
        let mut category = String::new();

        // Parse @attribute(...) parameters
        for attr in &field.attributes {
            if attr.name == "attribute" {
                // Parse args as key-value pairs
                // For now, we'll use a simple approach - args should be in format: name: value
                for arg in &attr.args {
                    // Extract parameter name and value from expression
                    // This is a simplified parser - in production, you'd want proper expression parsing
                    let arg_str = format!("{:?}", arg);
                    if arg_str.contains("replicated") && arg_str.contains("true") {
                        replicated = true;
                    }
                    if arg_str.contains("rep_notify") && arg_str.contains("true") {
                        rep_notify = true;
                    }
                    if arg_str.contains("hide_from_modifiers") && arg_str.contains("true") {
                        hide_from_modifiers = true;
                    }
                    if arg_str.contains("meta") && arg_str.contains("true") {
                        is_meta = true;
                    }
                }
            }
        }

        // Default category to attribute set name (will be set by caller)
        if category.is_empty() {
            category = "Attributes".to_string();
        }

        // Validate: meta attributes cannot be replicated
        if is_meta && replicated {
            return Err(KainError::codegen(
                format!("Meta attribute '{}' cannot be replicated", field.name),
                field.span,
            ));
        }

        // Validate: rep_notify requires replicated
        if rep_notify && !replicated {
            return Err(KainError::codegen(
                format!("Attribute '{}' has rep_notify but is not replicated", field.name),
                field.span,
            ));
        }

        // Extract default value from field.default
        let default_value = field.default.as_ref().map(|expr| format!("{:?}", expr));

        Ok(AttributeIR {
            name: field.name.clone(),
            ty: field.ty.clone(),
            default_value,
            replicated,
            rep_notify,
            hide_from_modifiers,
            is_meta,
            clamp_min,
            clamp_max,
            category,
        })
    }
}

impl DelegateIR {
    fn from_field(field: &Field) -> KainResult<Self> {
        // Extract delegate type from field type
        let delegate_type = match &field.ty {
            Type::Named { name, .. } => name.clone(),
            _ => {
                return Err(KainError::codegen(
                    format!("Delegate '{}' must have a named type", field.name),
                    field.span,
                ));
            }
        };

        Ok(DelegateIR {
            name: field.name.clone(),
            delegate_type,
        })
    }
}

impl FunctionIR {
    fn from_ast(func: &Function) -> KainResult<Self> {
        // For now, store the function as-is
        // Full codegen will happen in attribute_set_codegen.rs
        Ok(FunctionIR {
            name: func.name.clone(),
            body: format!("{:?}", func.body), // Placeholder
        })
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

#[allow(dead_code)]
fn parse_bool_param(value: &str) -> KainResult<bool> {
    match value {
        "true" => Ok(true),
        "false" => Ok(false),
        _ => Err(KainError::codegen(
            format!("Invalid boolean value: {}", value),
            Span::default(),
        )),
    }
}

#[allow(dead_code)]
fn parse_float_param(value: &str) -> KainResult<f32> {
    value.parse::<f32>().map_err(|_| {
        KainError::codegen(
            format!("Invalid float value: {}", value),
            Span::default(),
        )
    })
}

#[allow(dead_code)]
fn parse_string_param(value: &str) -> KainResult<String> {
    // Remove quotes if present
    let trimmed = value.trim_matches('"').trim_matches('\'');
    Ok(trimmed.to_string())
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_bool_param() {
        assert_eq!(parse_bool_param("true").unwrap(), true);
        assert_eq!(parse_bool_param("false").unwrap(), false);
        assert!(parse_bool_param("invalid").is_err());
    }

    #[test]
    fn test_parse_float_param() {
        assert_eq!(parse_float_param("1.5").unwrap(), 1.5);
        assert_eq!(parse_float_param("100.0").unwrap(), 100.0);
        assert!(parse_float_param("invalid").is_err());
    }

    #[test]
    fn test_parse_string_param() {
        assert_eq!(parse_string_param("\"test\"").unwrap(), "test");
        assert_eq!(parse_string_param("'test'").unwrap(), "test");
        assert_eq!(parse_string_param("test").unwrap(), "test");
    }
}
