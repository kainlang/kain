//! Network Synchronization Intermediate Representation
//!
//! This module defines the IR structures for network synchronization patterns
//! and provides conversion from AST to IR with proper type mapping.
//!
//! The NetworkSync system supports multiple replication modes:
//! - Simple: Basic replication without interpolation
//! - Interpolated: Client-side interpolation with state buffers
//! - Extrapolated: Client-side prediction with movement extrapolation
//! - Compressed: Bandwidth optimization with threshold-based compression

use crate::ue5::context::Ue5Context;
use crate::ue5::types::TypeMapper;
use kain_core::ast::{Attribute, BinaryOp, Field, Struct};

/// Network synchronization intermediate representation
/// Represents a component with replicated properties and network configuration
#[derive(Debug, Clone)]
pub struct NetworkSyncIR {
    /// Name of the component (without U prefix)
    pub component_name: String,

    /// List of replicated properties with their modes
    pub replicated_properties: Vec<ReplicatedPropertyIR>,

    /// Network configuration settings
    pub config: NetworkConfigIR,
}

/// A single replicated property with its replication mode
#[derive(Debug, Clone)]
pub struct ReplicatedPropertyIR {
    /// Property name (KAIN identifier)
    pub name: String,

    /// C++ type string (e.g., "FVector", "float", "int64")
    pub cpp_type: String,

    /// Replication mode with parameters
    pub mode: ReplicationModeIR,

    /// Optional compression settings
    pub compression: Option<CompressionSettingsIR>,
}

/// Replication mode determines how the property is synchronized
#[derive(Debug, Clone, PartialEq)]
pub enum ReplicationModeIR {
    /// Simple replication - just replicate the value as-is
    Simple,

    /// Interpolated replication - client interpolates between states
    Interpolated {
        /// How far back in time to interpolate (seconds)
        back_time: f32,
        /// Size of the state buffer (number of states to keep)
        buffer_size: usize,
    },

    /// Extrapolated replication - client predicts future values
    Extrapolated {
        /// Maximum extrapolation distance (units)
        limit: f32,
    },

    /// Compressed replication - reduce bandwidth with lossy compression
    Compressed {
        /// Threshold for sending updates (minimum change)
        threshold: f32,
        /// Use half-precision floats (16-bit instead of 32-bit)
        use_half_float: bool,
    },
}

/// Compression settings for bandwidth optimization
#[derive(Debug, Clone)]
pub struct CompressionSettingsIR {
    /// Minimum change threshold to trigger replication
    pub threshold: f32,

    /// Use quantization (reduce precision)
    pub quantize: bool,

    /// Quantization bits (if quantize is true)
    pub quantize_bits: u8,
}

/// Network configuration for the component
#[derive(Debug, Clone)]
pub struct NetworkConfigIR {
    /// Snap threshold for teleportation detection (units)
    pub snap_threshold: f32,

    /// Network update rate (updates per second)
    pub send_rate: f32,

    /// Enable owner time synchronization
    pub owner_time_sync: bool,

    /// Enable bandwidth optimization
    pub optimize_bandwidth: bool,
}

impl Default for NetworkConfigIR {
    fn default() -> Self {
        Self {
            snap_threshold: 500.0,
            send_rate: 30.0,
            owner_time_sync: true,
            optimize_bandwidth: true,
        }
    }
}

/// Convert a component struct with @replicated attributes to NetworkSyncIR
///
/// # Arguments
/// * `component` - The component struct from AST
/// * `ctx` - UE5 compilation context for type mapping
///
/// # Returns
/// * `Ok(NetworkSyncIR)` - Successfully converted IR
/// * `Err(String)` - Conversion error with description
pub fn convert_to_network_sync_ir(
    component: &Struct,
    ctx: &Ue5Context,
) -> Result<NetworkSyncIR, String> {
    // Create type mapper with context knowledge
    let mut type_mapper = TypeMapper::with_knowledge(ctx.knowledge.clone());

    // Register all known types from context
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

    // Extract replicated properties
    let mut replicated_properties = Vec::new();

    for field in &component.fields {
        // Check if field has @replicated attribute
        if let Some(repl_attr) = find_attribute(&field.attributes, "replicated") {
            let property = convert_replicated_property(field, &repl_attr, &type_mapper)?;
            replicated_properties.push(property);
        }
    }

    // Extract network configuration from component attributes
    let config = extract_network_config(&component.attributes)?;

    Ok(NetworkSyncIR {
        component_name: component.name.clone(),
        replicated_properties,
        config,
    })
}

/// Convert a single field with @replicated attribute to ReplicatedPropertyIR
fn convert_replicated_property(
    field: &Field,
    repl_attr: &Attribute,
    type_mapper: &TypeMapper,
) -> Result<ReplicatedPropertyIR, String> {
    // Map KAIN type to C++ type
    let cpp_type = type_mapper.map_type_string(&field.ty);

    // Parse replication mode from attribute arguments
    let mode = parse_replication_mode(repl_attr)?;

    // Parse optional compression settings
    let compression = parse_compression_settings(repl_attr)?;

    Ok(ReplicatedPropertyIR {
        name: field.name.clone(),
        cpp_type,
        mode,
        compression,
    })
}

/// Parse replication mode from @replicated attribute
///
/// Supported formats:
/// - @replicated - defaults to Simple
/// - @replicated(mode: Simple)
/// - @replicated(mode: Interpolated, back_time: 0.1, buffer_size: 32)
/// - @replicated(mode: Extrapolated, limit: 100.0)
/// - @replicated(mode: Compressed, threshold: 0.01, use_half_float: true)
fn parse_replication_mode(attr: &Attribute) -> Result<ReplicationModeIR, String> {
    // Find 'mode' argument
    let mode_arg = attr.args.iter().find(|arg| {
        // Check if this is a named argument with name "mode"
        if let kain_core::ast::Expr::Binary { op, left, .. } = arg {
            if *op == BinaryOp::Assign {
                if let kain_core::ast::Expr::Ident(name, _) = &**left {
                    return name == "mode";
                }
            }
        }
        false
    });

    // If no mode specified, default to Simple
    let mode_name = if let Some(arg) = mode_arg {
        extract_mode_name(arg)?
    } else {
        "Simple".to_string()
    };

    // Parse mode-specific parameters
    match mode_name.as_str() {
        "Simple" => Ok(ReplicationModeIR::Simple),

        "Interpolated" => {
            let back_time = extract_float_param(attr, "back_time").unwrap_or(0.1);
            let buffer_size = extract_int_param(attr, "buffer_size").unwrap_or(32);
            Ok(ReplicationModeIR::Interpolated {
                back_time,
                buffer_size: buffer_size as usize,
            })
        }

        "Extrapolated" => {
            let limit = extract_float_param(attr, "limit").unwrap_or(100.0);
            Ok(ReplicationModeIR::Extrapolated { limit })
        }

        "Compressed" => {
            let threshold = extract_float_param(attr, "threshold").unwrap_or(0.01);
            let use_half_float = extract_bool_param(attr, "use_half_float").unwrap_or(false);
            Ok(ReplicationModeIR::Compressed {
                threshold,
                use_half_float,
            })
        }

        _ => Err(format!("Unknown replication mode: {}", mode_name)),
    }
}

/// Parse optional compression settings from attribute
fn parse_compression_settings(attr: &Attribute) -> Result<Option<CompressionSettingsIR>, String> {
    // Check if compression parameters are present
    let has_compression = attr.args.iter().any(|arg| {
        if let kain_core::ast::Expr::Binary { op, left, .. } = arg {
            if *op == BinaryOp::Assign {
                if let kain_core::ast::Expr::Ident(name, _) = &**left {
                    return matches!(name.as_str(), "threshold" | "quantize" | "quantize_bits");
                }
            }
        }
        false
    });

    if !has_compression {
        return Ok(None);
    }

    let threshold = extract_float_param(attr, "threshold").unwrap_or(0.01);
    let quantize = extract_bool_param(attr, "quantize").unwrap_or(false);
    let quantize_bits = extract_int_param(attr, "quantize_bits").unwrap_or(8) as u8;

    Ok(Some(CompressionSettingsIR {
        threshold,
        quantize,
        quantize_bits,
    }))
}

/// Extract network configuration from component attributes
fn extract_network_config(attributes: &[Attribute]) -> Result<NetworkConfigIR, String> {
    let mut config = NetworkConfigIR::default();

    // Look for @network_config attribute
    if let Some(attr) = find_attribute(attributes, "network_config") {
        if let Some(snap) = extract_float_param(attr, "snap_threshold") {
            config.snap_threshold = snap;
        }
        if let Some(rate) = extract_float_param(attr, "send_rate") {
            config.send_rate = rate;
        }
        if let Some(sync) = extract_bool_param(attr, "owner_time_sync") {
            config.owner_time_sync = sync;
        }
        if let Some(opt) = extract_bool_param(attr, "optimize_bandwidth") {
            config.optimize_bandwidth = opt;
        }
    }

    Ok(config)
}

// ============================================================================
// Helper Functions
// ============================================================================

/// Find an attribute by name
fn find_attribute<'a>(attributes: &'a [Attribute], name: &str) -> Option<&'a Attribute> {
    attributes.iter().find(|attr| attr.name == name)
}

/// Extract mode name from binary expression (mode: ModeName)
fn extract_mode_name(expr: &kain_core::ast::Expr) -> Result<String, String> {
    use kain_core::ast::Expr;

    if let Expr::Binary { op, right, .. } = expr {
        if *op == BinaryOp::Assign {
            if let Expr::Ident(name, _) = &**right {
                return Ok(name.clone());
            }
        }
    }

    Err("Invalid mode specification".to_string())
}

/// Extract float parameter from attribute arguments
fn extract_float_param(attr: &Attribute, param_name: &str) -> Option<f32> {
    use kain_core::ast::Expr;

    for arg in &attr.args {
        if let Expr::Binary {
            op, left, right, ..
        } = arg
        {
            if *op == BinaryOp::Assign {
                if let Expr::Ident(name, _) = &**left {
                    if name == param_name {
                        if let Expr::Float(val, _) = &**right {
                            return Some(*val as f32);
                        }
                        if let Expr::Int(val, _) = &**right {
                            return Some(*val as f32);
                        }
                    }
                }
            }
        }
    }

    None
}

/// Extract integer parameter from attribute arguments
fn extract_int_param(attr: &Attribute, param_name: &str) -> Option<i64> {
    use kain_core::ast::Expr;

    for arg in &attr.args {
        if let Expr::Binary {
            op, left, right, ..
        } = arg
        {
            if *op == BinaryOp::Assign {
                if let Expr::Ident(name, _) = &**left {
                    if name == param_name {
                        if let Expr::Int(val, _) = &**right {
                            return Some(*val);
                        }
                    }
                }
            }
        }
    }

    None
}

/// Extract boolean parameter from attribute arguments
fn extract_bool_param(attr: &Attribute, param_name: &str) -> Option<bool> {
    use kain_core::ast::Expr;

    for arg in &attr.args {
        if let Expr::Binary {
            op, left, right, ..
        } = arg
        {
            if *op == BinaryOp::Assign {
                if let Expr::Ident(name, _) = &**left {
                    if name == param_name {
                        if let Expr::Bool(val, _) = &**right {
                            return Some(*val);
                        }
                        // Also accept "true"/"false" as identifiers
                        if let Expr::Ident(val, _) = &**right {
                            match val.as_str() {
                                "true" => return Some(true),
                                "false" => return Some(false),
                                _ => {}
                            }
                        }
                    }
                }
            }
        }
    }

    None
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Attribute, Expr, Field, Struct, Type, Visibility};
    use kain_core::span::Span;

    fn dummy_span() -> Span {
        Span::new(0, 0)
    }

    fn make_simple_replicated_field(name: &str, ty: Type) -> Field {
        Field {
            name: name.to_string(),
            ty,
            attributes: vec![Attribute {
                name: "replicated".to_string(),
                args: vec![],
                span: dummy_span(),
            }],
            visibility: Visibility::Public,
            default: None,
            weak: false,
            span: dummy_span(),
        }
    }

    fn make_interpolated_field(name: &str, ty: Type, back_time: f64) -> Field {
        Field {
            name: name.to_string(),
            ty,
            attributes: vec![Attribute {
                name: "replicated".to_string(),
                args: vec![
                    Expr::Binary {
                        op: BinaryOp::Assign,
                        left: Box::new(Expr::Ident("mode".to_string(), dummy_span())),
                        right: Box::new(Expr::Ident("Interpolated".to_string(), dummy_span())),
                        span: dummy_span(),
                    },
                    Expr::Binary {
                        op: BinaryOp::Assign,
                        left: Box::new(Expr::Ident("back_time".to_string(), dummy_span())),
                        right: Box::new(Expr::Float(back_time, dummy_span())),
                        span: dummy_span(),
                    },
                ],
                span: dummy_span(),
            }],
            visibility: Visibility::Public,
            default: None,
            weak: false,
            span: dummy_span(),
        }
    }

    #[test]
    fn test_simple_replication_mode() {
        let attr = Attribute {
            name: "replicated".to_string(),
            args: vec![],
            span: dummy_span(),
        };

        let mode = parse_replication_mode(&attr).unwrap();
        assert_eq!(mode, ReplicationModeIR::Simple);
    }

    #[test]
    fn test_interpolated_replication_mode() {
        let attr = Attribute {
            name: "replicated".to_string(),
            args: vec![
                Expr::Binary {
                    op: BinaryOp::Assign,
                    left: Box::new(Expr::Ident("mode".to_string(), dummy_span())),
                    right: Box::new(Expr::Ident("Interpolated".to_string(), dummy_span())),
                    span: dummy_span(),
                },
                Expr::Binary {
                    op: BinaryOp::Assign,
                    left: Box::new(Expr::Ident("back_time".to_string(), dummy_span())),
                    right: Box::new(Expr::Float(0.15, dummy_span())),
                    span: dummy_span(),
                },
                Expr::Binary {
                    op: BinaryOp::Assign,
                    left: Box::new(Expr::Ident("buffer_size".to_string(), dummy_span())),
                    right: Box::new(Expr::Int(64, dummy_span())),
                    span: dummy_span(),
                },
            ],
            span: dummy_span(),
        };

        let mode = parse_replication_mode(&attr).unwrap();
        match mode {
            ReplicationModeIR::Interpolated {
                back_time,
                buffer_size,
            } => {
                assert!((back_time - 0.15).abs() < 0.001);
                assert_eq!(buffer_size, 64);
            }
            _ => panic!("Expected Interpolated mode"),
        }
    }

    #[test]
    fn test_convert_simple_component() {
        let ctx = Ue5Context::new("TestPlugin", None);

        let component = Struct {
            name: "NetworkedTransform".to_string(),
            generics: vec![],
            fields: vec![make_simple_replicated_field(
                "position",
                Type::Named {
                    name: "Vec3".to_string(),
                    generics: vec![],
                    span: dummy_span(),
                },
            )],
            methods: vec![],
            attributes: vec![Attribute {
                name: "component".to_string(),
                args: vec![],
                span: dummy_span(),
            }],
            visibility: Visibility::Public,
            span: dummy_span(),
        };

        let ir = convert_to_network_sync_ir(&component, &ctx).unwrap();

        assert_eq!(ir.component_name, "NetworkedTransform");
        assert_eq!(ir.replicated_properties.len(), 1);
        assert_eq!(ir.replicated_properties[0].name, "position");
        assert_eq!(ir.replicated_properties[0].cpp_type, "FVector");
        assert_eq!(ir.replicated_properties[0].mode, ReplicationModeIR::Simple);
    }
}
