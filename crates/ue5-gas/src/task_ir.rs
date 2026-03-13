// ============================================================================
// Ability Task IR — Intermediate Representation for GAS Tasks
// ============================================================================
// Converts AST ability tasks into a structured IR for codegen.
// Handles delegates, state fields, and lifecycle methods.
// ============================================================================

use kain_core::ast::{AbilityTaskDef, TaskDelegateDef};
use kain_core::error::{KainError, KainResult};

/// Intermediate representation of an ability task
#[derive(Debug, Clone)]
pub struct AbilityTaskIR {
    pub name: String,
    pub delegates: Vec<DelegateIR>,
    pub state_fields: Vec<StateFieldIR>,
    pub activate_body: Option<String>,
    pub on_destroy_body: Option<String>,
    pub custom_methods: Vec<MethodIR>,
}

/// Delegate IR
#[derive(Debug, Clone)]
pub struct DelegateIR {
    pub name: String,
    pub delegate_type: DelegateTypeIR,
}

/// Delegate type IR
#[derive(Debug, Clone, PartialEq)]
pub enum DelegateTypeIR {
    AttributeChange,
    TaskCancelled,
    TargetDataReady,
    GameplayEvent,
    Custom(String),
}

/// State field IR
#[derive(Debug, Clone)]
pub struct StateFieldIR {
    pub name: String,
    pub field_type: String,
}

/// Method IR
#[derive(Debug, Clone)]
pub struct MethodIR {
    pub name: String,
    pub body: String,
}

impl AbilityTaskIR {
    /// Convert AST AbilityTaskDef to IR
    pub fn from_ast(task: &AbilityTaskDef) -> KainResult<Self> {
        // Verify @ability_task attribute
        if !task.attributes.iter().any(|a| a.name == "ability_task") {
            return Err(KainError::codegen(
                format!("Struct '{}' must have @ability_task attribute", task.name),
                task.span,
            ));
        }

        // Convert delegates
        let delegates = task
            .delegates
            .iter()
            .map(|d| {
                let delegate_type = match d.delegate_type.as_str() {
                    "AttributeChangeDelegate" => DelegateTypeIR::AttributeChange,
                    "TaskCancelledDelegate" => DelegateTypeIR::TaskCancelled,
                    "TargetDataDelegate" => DelegateTypeIR::TargetDataReady,
                    "GameplayEventDelegate" => DelegateTypeIR::GameplayEvent,
                    other => DelegateTypeIR::Custom(other.to_string()),
                };

                DelegateIR {
                    name: d.name.clone(),
                    delegate_type,
                }
            })
            .collect();

        // Convert state fields
        let state_fields = task
            .state_fields
            .iter()
            .map(|f| StateFieldIR {
                name: f.name.clone(),
                field_type: format!("{:?}", f.ty), // TODO: proper type mapping
            })
            .collect();

        // Convert methods to strings (placeholder for now)
        let activate_body = task
            .activate_method
            .as_ref()
            .map(|_| "// TODO: Implement activate codegen".to_string());

        let on_destroy_body = task
            .on_destroy_method
            .as_ref()
            .map(|_| "// TODO: Implement on_destroy codegen".to_string());

        let custom_methods = task
            .custom_methods
            .iter()
            .map(|m| MethodIR {
                name: m.name.clone(),
                body: "// TODO: Implement method codegen".to_string(),
            })
            .collect();

        Ok(AbilityTaskIR {
            name: task.name.clone(),
            delegates,
            state_fields,
            activate_body,
            on_destroy_body,
            custom_methods,
        })
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_delegate_type_variants() {
        let _types = vec![
            DelegateTypeIR::AttributeChange,
            DelegateTypeIR::TaskCancelled,
            DelegateTypeIR::TargetDataReady,
            DelegateTypeIR::GameplayEvent,
            DelegateTypeIR::Custom("Test".to_string()),
        ];
    }
}
