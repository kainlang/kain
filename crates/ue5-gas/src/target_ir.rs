// ============================================================================
// Target Actor IR — Intermediate Representation for GAS Target Actors
// ============================================================================

use kain_core::ast::{TargetActorDef, TraceType, TargetFilter};
use kain_core::error::{KainError, KainResult};

#[derive(Debug, Clone)]
pub struct TargetActorIR {
    pub name: String,
    pub trace_type: TraceTypeIR,
    pub max_range: Option<f64>,
    pub trace_channel: Option<String>,
    pub filter: Option<TargetFilterIR>,
    pub reticle_class: Option<String>,
    pub custom_methods: Vec<MethodIR>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum TraceTypeIR {
    Line,
    Sphere,
    Cone,
    Box,
    Cylinder,
}

#[derive(Debug, Clone)]
pub struct TargetFilterIR {
    pub self_filter: Option<String>,
    pub required_actor_class: Option<String>,
    pub require_tags: Vec<String>,
    pub ignore_tags: Vec<String>,
    pub custom_filter_body: Option<String>,
}

#[derive(Debug, Clone)]
pub struct MethodIR {
    pub name: String,
    pub body: String,
}

impl TargetActorIR {
    pub fn from_ast(target: &TargetActorDef) -> KainResult<Self> {
        // Verify @target_actor attribute
        if !target.attributes.iter().any(|a| a.name == "target_actor") {
            return Err(KainError::codegen(
                format!("Struct '{}' must have @target_actor attribute", target.name),
                target.span,
            ));
        }
        
        // Convert trace type
        let trace_type = match target.trace_type {
            TraceType::Line => TraceTypeIR::Line,
            TraceType::Sphere => TraceTypeIR::Sphere,
            TraceType::Cone => TraceTypeIR::Cone,
            TraceType::Box => TraceTypeIR::Box,
            TraceType::Cylinder => TraceTypeIR::Cylinder,
        };
        
        // Convert filter
        let filter = target.filter.as_ref().map(|f| TargetFilterIR {
            self_filter: f.self_filter.clone(),
            required_actor_class: f.required_actor_class.clone(),
            require_tags: f.require_tags.clone(),
            ignore_tags: f.ignore_tags.clone(),
            custom_filter_body: f.custom_filter_method.as_ref().map(|_| {
                "// TODO: Implement custom filter codegen".to_string()
            }),
        });
        
        // Convert custom methods
        let custom_methods = target.custom_methods.iter()
            .map(|m| MethodIR {
                name: m.name.clone(),
                body: "// TODO: Implement method codegen".to_string(),
            })
            .collect();
        
        Ok(TargetActorIR {
            name: target.name.clone(),
            trace_type,
            max_range: target.max_range,
            trace_channel: target.trace_channel.clone(),
            filter,
            reticle_class: target.reticle_class.clone(),
            custom_methods,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_trace_type_variants() {
        let _types = vec![
            TraceTypeIR::Line,
            TraceTypeIR::Sphere,
            TraceTypeIR::Cone,
            TraceTypeIR::Box,
            TraceTypeIR::Cylinder,
        ];
    }
}
