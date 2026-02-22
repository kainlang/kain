//! Blueprint Integration Intermediate Representation
//!
//! This module defines the IR structures for Blueprint integration patterns
//! and provides conversion from AST to IR with proper type mapping.
//!
//! The Blueprint system supports:
//! - @blueprint_event for BlueprintNativeEvent functions with _Implementation methods
//! - K2Node generation for custom Blueprint nodes
//! - Async Blueprint nodes (UK2Node_AsyncAction) for latent actions
//! - Blueprint-callable functions (already implemented via @blueprint attribute)

use kain_core::ast::{Function, Field, Block, Attribute};
use crate::ue5::context::Ue5Context;
use crate::ue5::types::TypeMapper;

/// Blueprint event intermediate representation
/// Represents a function that can be overridden in Blueprint with native implementation
#[derive(Debug, Clone)]
pub struct BlueprintEventIR {
    /// Name of the event (without prefix)
    pub event_name: String,
    
    /// Parameters for the event
    pub params: Vec<BlueprintParamIR>,
    
    /// Return type (if any)
    pub return_type: Option<String>,
    
    /// Category for Blueprint organization
    pub category: String,
    
    /// Default implementation body (C++ code)
    pub implementation_body: Option<String>,
}

/// A single parameter for Blueprint event
#[derive(Debug, Clone)]
pub struct BlueprintParamIR {
    /// Parameter name (KAIN identifier)
    pub name: String,
    
    /// C++ type string (e.g., "int32", "FVector", "AActor*")
    pub cpp_type: String,
    
    /// Whether this is a reference parameter
    pub is_ref: bool,
    
    /// Whether this is a const parameter
    pub is_const: bool,
}

/// K2Node intermediate representation
/// Represents a custom Blueprint node with input/output pins
#[derive(Debug, Clone)]
pub struct K2NodeIR {
    /// Name of the K2Node class (without UK2Node_ prefix)
    pub node_name: String,
    
    /// Input pins for the node
    pub input_pins: Vec<K2PinIR>,
    
    /// Output pins for the node
    pub output_pins: Vec<K2PinIR>,
    
    /// Node title displayed in Blueprint editor
    pub node_title: String,
    
    /// Node category for organization
    pub category: String,
    
    /// Node expansion logic (C++ code for ExpandNode)
    pub expand_logic: Option<String>,
}

/// A single pin for K2Node
#[derive(Debug, Clone)]
pub struct K2PinIR {
    /// Pin name
    pub name: String,
    
    /// Pin type (Exec, Bool, Int, Float, String, Object, etc.)
    pub pin_type: K2PinType,
    
    /// Whether this is an array pin
    pub is_array: bool,
    
    /// Default value (if any)
    pub default_value: Option<String>,
}

/// K2Node pin types
#[derive(Debug, Clone, PartialEq)]
pub enum K2PinType {
    Exec,
    Bool,
    Int,
    Int64,
    Float,
    String,
    Name,
    Text,
    Vector,
    Rotator,
    Transform,
    Object(String),  // Object type with class name
    Struct(String),  // Struct type with struct name
    Wildcard,
}

/// Async Blueprint node intermediate representation
/// Represents a latent Blueprint action that executes asynchronously
#[derive(Debug, Clone)]
pub struct AsyncBlueprintIR {
    /// Name of the async action (without U prefix)
    pub action_name: String,
    
    /// Input parameters for the action
    pub input_params: Vec<BlueprintParamIR>,
    
    /// Output pins (delegates for completion/failure/etc.)
    pub output_pins: Vec<AsyncOutputPinIR>,
    
    /// Activate method body (C++ code)
    pub activate_body: Option<String>,
    
    /// Category for Blueprint organization
    pub category: String,
}

/// Output pin for async Blueprint node (delegate)
#[derive(Debug, Clone)]
pub struct AsyncOutputPinIR {
    /// Pin name (e.g., "OnCompleted", "OnFailed")
    pub name: String,
    
    /// Parameters passed to the delegate
    pub params: Vec<BlueprintParamIR>,
}

impl Default for BlueprintEventIR {
    fn default() -> Self {
        Self {
            event_name: String::new(),
            params: Vec::new(),
            return_type: None,
            category: "Events".to_string(),
            implementation_body: None,
        }
    }
}

impl Default for K2NodeIR {
    fn default() -> Self {
        Self {
            node_name: String::new(),
            input_pins: Vec::new(),
            output_pins: Vec::new(),
            node_title: String::new(),
            category: "Custom".to_string(),
            expand_logic: None,
        }
    }
}

impl Default for AsyncBlueprintIR {
    fn default() -> Self {
        Self {
            action_name: String::new(),
            input_params: Vec::new(),
            output_pins: Vec::new(),
            activate_body: None,
            category: "Async".to_string(),
        }
    }
}

/// Convert a function with @blueprint_event attribute to BlueprintEventIR
///
/// # Arguments
/// * `func` - The function definition from AST
/// * `ctx` - UE5 compilation context for type mapping
///
/// # Returns
/// * `Ok(BlueprintEventIR)` - Successfully converted IR
/// * `Err(String)` - Conversion error with description
pub fn convert_to_blueprint_event_ir(
    func: &Function,
    ctx: &Ue5Context,
) -> Result<BlueprintEventIR, String> {
    // Create type mapper with context knowledge
    let mut type_mapper = TypeMapper::with_knowledge(ctx.knowledge.clone());
    
    // Register all known types from context
    register_context_types(&mut type_mapper, ctx);
    
    // Convert parameters
    let params = func.params.iter()
        .map(|param| convert_param(param, &type_mapper))
        .collect::<Result<Vec<_>, _>>()?;
    
    // Convert return type
    let return_type = func.return_type.as_ref()
        .map(|ty| type_mapper.map_type_string(ty));
    
    // Extract category from attributes
    let category = extract_category(&func.attributes)
        .unwrap_or_else(|| "Events".to_string());
    
    // Convert body to C++ code
    let implementation_body = Some(convert_block_to_cpp(&func.body, ctx));
    
    Ok(BlueprintEventIR {
        event_name: func.name.clone(),
        params,
        return_type,
        category,
        implementation_body,
    })
}

/// Convert a parameter to BlueprintParamIR
fn convert_param(
    param: &kain_core::ast::Param,
    type_mapper: &TypeMapper,
) -> Result<BlueprintParamIR, String> {
    // Map KAIN type to C++ type
    let cpp_type = type_mapper.map_type_string(&param.ty);
    
    // Check if this is a reference type
    let is_ref = cpp_type.contains("&");
    let is_const = cpp_type.starts_with("const ");
    
    Ok(BlueprintParamIR {
        name: param.name.clone(),
        cpp_type,
        is_ref,
        is_const,
    })
}

/// Extract category from attributes
fn extract_category(attributes: &[Attribute]) -> Option<String> {
    for attr in attributes {
        if attr.name == "category" {
            if let Some(first_arg) = attr.args.first() {
                // Extract string value from expression
                if let kain_core::ast::Expr::String(s, _) = first_arg {
                    return Some(s.clone());
                }
            }
        }
    }
    None
}

/// Register all known types from context into type mapper
fn register_context_types(type_mapper: &mut TypeMapper, ctx: &Ue5Context) {
    for enum_name in &ctx.enum_names {
        type_mapper.register_enum(enum_name.clone());
    }
    for struct_name in &ctx.struct_names {
        type_mapper.register_struct(struct_name.clone());
    }
    for component_name in &ctx.component_names {
        type_mapper.register_component(component_name.clone());
    }
    for actor_name in &ctx.actor_names {
        type_mapper.register_actor(actor_name.clone());
    }
    for delegate_name in &ctx.delegate_names {
        type_mapper.register_delegate(delegate_name.clone());
    }
}

/// Convert a KAIN block to C++ code
/// 
/// This is a placeholder implementation that will be replaced with proper
/// expression codegen when the full codegen pipeline is integrated.
fn convert_block_to_cpp(block: &Block, _ctx: &Ue5Context) -> String {
    // For now, return a placeholder comment
    // TODO: Integrate with expression codegen from ue5 crate
    format!("/* Block with {} statements */", block.stmts.len())
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Function, Param, Block, Type, Visibility, Attribute};
    use kain_core::span::Span;
    
    fn dummy_span() -> Span {
        Span::new(0, 0)
    }
    
    fn make_simple_param(name: &str, ty: Type) -> Param {
        Param {
            name: name.to_string(),
            ty,
            mutable: false,
            default: None,
            span: dummy_span(),
        }
    }
    
    #[test]
    fn test_convert_simple_blueprint_event() {
        let ctx = Ue5Context::new("TestPlugin", None);
        
        let func = Function {
            name: "OnCustomEvent".to_string(),
            generics: vec![],
            params: vec![
                make_simple_param(
                    "value",
                    Type::Named {
                        name: "Int".to_string(),
                        generics: vec![],
                        span: dummy_span(),
                    }
                ),
            ],
            return_type: None,
            effects: vec![],
            body: Block {
                stmts: vec![],
                span: dummy_span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: dummy_span(),
        };
        
        let ir = convert_to_blueprint_event_ir(&func, &ctx).unwrap();
        
        assert_eq!(ir.event_name, "OnCustomEvent");
        assert_eq!(ir.params.len(), 1);
        assert_eq!(ir.params[0].name, "value");
        assert_eq!(ir.params[0].cpp_type, "int64");
        assert_eq!(ir.return_type, None);
        assert_eq!(ir.category, "Events");
    }
    
    #[test]
    fn test_convert_blueprint_event_with_return() {
        let ctx = Ue5Context::new("TestPlugin", None);
        
        let func = Function {
            name: "CalculateDamage".to_string(),
            generics: vec![],
            params: vec![
                make_simple_param(
                    "base_damage",
                    Type::Named {
                        name: "Float".to_string(),
                        generics: vec![],
                        span: dummy_span(),
                    }
                ),
            ],
            return_type: Some(Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: dummy_span(),
            }),
            effects: vec![],
            body: Block {
                stmts: vec![],
                span: dummy_span(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: dummy_span(),
        };
        
        let ir = convert_to_blueprint_event_ir(&func, &ctx).unwrap();
        
        assert_eq!(ir.event_name, "CalculateDamage");
        assert_eq!(ir.return_type, Some("float".to_string()));
    }
    
    #[test]
    fn test_extract_category_from_attributes() {
        let attributes = vec![
            Attribute {
                name: "category".to_string(),
                args: vec![
                    kain_core::ast::Expr::String("Combat".to_string(), dummy_span()),
                ],
                span: dummy_span(),
            },
        ];
        
        let category = extract_category(&attributes);
        assert_eq!(category, Some("Combat".to_string()));
    }
    
    #[test]
    fn test_blueprint_param_ir() {
        let type_mapper = TypeMapper::new();
        
        let param = make_simple_param(
            "target",
            Type::Named {
                name: "Actor".to_string(),
                generics: vec![],
                span: dummy_span(),
            }
        );
        
        let param_ir = convert_param(&param, &type_mapper).unwrap();
        
        assert_eq!(param_ir.name, "target");
        assert!(param_ir.cpp_type.contains("AActor"));
    }
}
