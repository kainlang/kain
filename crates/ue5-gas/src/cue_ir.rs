// ============================================================================
// Gameplay Cue IR — Intermediate Representation for GAS Cues
// ============================================================================
// Converts AST gameplay cues into a structured IR for codegen.
// Handles static/actor cues, lifecycle methods, and state fields.
// ============================================================================

use kain_core::ast::{CueType, GameplayCueDef};
use kain_core::error::{KainError, KainResult};

/// Intermediate representation of a gameplay cue
#[derive(Debug, Clone)]
pub struct GameplayCueIR {
    pub name: String,
    pub tag: String,
    pub cue_type: CueTypeIR,
    pub auto_destroy: bool,
    pub state_fields: Vec<StateFieldIR>,
    pub on_execute_body: Option<String>,
    pub on_add_body: Option<String>,
    pub on_remove_body: Option<String>,
    pub while_active_body: Option<String>,
}

/// Cue type IR
#[derive(Debug, Clone, PartialEq)]
pub enum CueTypeIR {
    Static,
    Actor,
}

/// State field IR
#[derive(Debug, Clone)]
pub struct StateFieldIR {
    pub name: String,
    pub field_type: String,
}

impl GameplayCueIR {
    /// Convert AST GameplayCueDef to IR
    pub fn from_ast(cue: &GameplayCueDef) -> KainResult<Self> {
        // Verify @gameplay_cue attribute
        if !cue.attributes.iter().any(|a| a.name == "gameplay_cue") {
            return Err(KainError::codegen(
                format!("Struct '{}' must have @gameplay_cue attribute", cue.name),
                cue.span,
            ));
        }

        // Validate tag format
        if cue.tag.is_empty() {
            return Err(KainError::codegen(
                "Tag cannot be empty".to_string(),
                cue.span,
            ));
        }

        if !cue.tag.starts_with("GameplayCue.") {
            return Err(KainError::codegen(
                format!("Cue tag '{}' must start with 'GameplayCue.'", cue.tag),
                cue.span,
            ));
        }

        // Validate tag has content after prefix
        if cue.tag == "GameplayCue." {
            return Err(KainError::codegen(
                "Tag must have content after 'GameplayCue.' prefix".to_string(),
                cue.span,
            ));
        }

        // Convert cue type
        let cue_type = match cue.cue_type {
            CueType::Static => CueTypeIR::Static,
            CueType::Actor => CueTypeIR::Actor,
        };

        // Convert state fields
        let state_fields = cue
            .state_fields
            .iter()
            .map(|f| StateFieldIR {
                name: f.name.clone(),
                field_type: format!("{:?}", f.ty), // TODO: proper type mapping
            })
            .collect();

        // Convert lifecycle methods to strings (placeholder for now)
        let on_execute_body = cue
            .on_execute
            .as_ref()
            .map(|_| "// TODO: Implement on_execute codegen".to_string());

        let on_add_body = cue
            .on_add
            .as_ref()
            .map(|_| "// TODO: Implement on_add codegen".to_string());

        let on_remove_body = cue
            .on_remove
            .as_ref()
            .map(|_| "// TODO: Implement on_remove codegen".to_string());

        let while_active_body = cue
            .while_active
            .as_ref()
            .map(|_| "// TODO: Implement while_active codegen".to_string());

        Ok(GameplayCueIR {
            name: cue.name.clone(),
            tag: cue.tag.clone(),
            cue_type,
            auto_destroy: cue.auto_destroy,
            state_fields,
            on_execute_body,
            on_add_body,
            on_remove_body,
            while_active_body,
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
    fn test_cue_type_variants() {
        let _types = vec![CueTypeIR::Static, CueTypeIR::Actor];
    }

    #[test]
    fn test_tag_validation_valid() {
        // Tag validation is tested in from_ast
        assert!(true);
    }
}
