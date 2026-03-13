/// KAIN Source Markers
///
/// Embeds original KAIN source as comments in generated C++ to enable round-trip compilation.
/// This allows extracting KAIN from C++ and validating that KAIN → C++ → KAIN is lossless.
use kain_core::ast::*;

/// Marker style for embedding KAIN source
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MarkerStyle {
    /// Block markers: // KAIN_BEGIN: ... // KAIN_END: ...
    Block,
    /// Inline markers: // KAIN: ...
    Inline,
    /// No markers (default for now)
    None,
}

/// Configuration for KAIN marker generation
#[derive(Debug, Clone)]
pub struct MarkerConfig {
    pub style: MarkerStyle,
    pub include_attributes: bool,
    pub include_types: bool,
    pub include_expressions: bool,
}

impl Default for MarkerConfig {
    fn default() -> Self {
        Self {
            style: MarkerStyle::None, // Disabled by default, enable with --embed-kain flag
            include_attributes: true,
            include_types: true,
            include_expressions: false, // Too verbose
        }
    }
}

impl MarkerConfig {
    /// Enable all markers
    pub fn enabled() -> Self {
        Self {
            style: MarkerStyle::Block,
            include_attributes: true,
            include_types: true,
            include_expressions: false,
        }
    }
}

/// Generate KAIN source marker for an actor
pub fn actor_marker(actor: &Actor, config: &MarkerConfig) -> String {
    if config.style == MarkerStyle::None {
        return String::new();
    }

    // TODO: Implement based on current AST structure
    // For now, just return a simple marker
    format!("// KAIN: actor {}:", actor.name)
}

/// Generate KAIN source marker for actor method end
pub fn actor_end_marker(actor: &Actor, config: &MarkerConfig) -> String {
    if config.style == MarkerStyle::Block {
        format!("// KAIN_END: actor {}", actor.name)
    } else {
        String::new()
    }
}

/// Generate KAIN source marker for a message handler (RPC)
pub fn message_handler_marker(handler: &MessageHandler, config: &MarkerConfig) -> String {
    if config.style == MarkerStyle::None {
        return String::new();
    }
    format!("// KAIN: on {}(...)", handler.message_type)
}

/// Generate KAIN source marker for a struct
pub fn struct_marker(struct_def: &Struct, config: &MarkerConfig) -> String {
    if config.style == MarkerStyle::None {
        return String::new();
    }
    format!("// KAIN: struct {}:", struct_def.name)
}

/// Generate KAIN source marker for struct end
pub fn struct_end_marker(struct_def: &Struct, config: &MarkerConfig) -> String {
    if config.style == MarkerStyle::Block {
        format!("// KAIN_END: struct {}", struct_def.name)
    } else {
        String::new()
    }
}

/// Generate KAIN source marker for an enum
pub fn enum_marker(enum_def: &Enum, config: &MarkerConfig) -> String {
    if config.style == MarkerStyle::None {
        return String::new();
    }
    format!("// KAIN: enum {}:", enum_def.name)
}

/// Generate KAIN source marker for a blueprint function
pub fn blueprint_function_marker(func: &Function, config: &MarkerConfig) -> String {
    if config.style == MarkerStyle::None {
        return String::new();
    }
    format!("// KAIN: @blueprint fn {}(...)", func.name)
}

// Helper functions - simplified versions

fn format_type(ty: &Type) -> String {
    // Simplified - just return the type name
    "Type".to_string()
}

fn format_expr(expr: &Expr) -> String {
    // Simplified - just return placeholder
    "...".to_string()
}

fn format_attr_args(args: &[CallArg]) -> String {
    // Simplified
    "...".to_string()
}
