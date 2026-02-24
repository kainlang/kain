// ============================================================================
// Gameplay Effect IR — Intermediate Representation for GAS Effects
// ============================================================================
// Converts AST gameplay effects into a structured IR for codegen.
// Handles duration policies, modifiers, stacking, and tag requirements.
// ============================================================================

use kain_core::ast::GameplayEffectDef;
use kain_core::error::{KainError, KainResult};
use kain_core::span::Span;

/// Intermediate representation of a gameplay effect
#[derive(Debug, Clone)]
pub struct GameplayEffectIR {
    pub name: String,
    pub duration_policy: DurationPolicy,
    pub duration_magnitude: Option<f32>,
    pub period: Option<f32>,
    pub execute_on_application: bool,
    pub modifiers: Vec<ModifierIR>,
    pub stacking: Option<StackingIR>,
    pub owned_tags: Vec<String>,
    pub granted_tags: Vec<String>,
    pub application_tag_requirements: TagRequirementsIR,
    pub ongoing_tag_requirements: TagRequirementsIR,
    pub removal_tag_requirements: TagRequirementsIR,
}

/// Duration policy for gameplay effects
#[derive(Debug, Clone, PartialEq)]
pub enum DurationPolicy {
    Instant,
    Infinite,
    HasDuration,
}

impl Default for DurationPolicy {
    fn default() -> Self {
        DurationPolicy::Instant
    }
}

/// Modifier intermediate representation
#[derive(Debug, Clone)]
pub struct ModifierIR {
    pub attribute: String,
    pub operation: ModifierOp,
    pub magnitude: f32,
}

/// Modifier operation types
#[derive(Debug, Clone, PartialEq)]
pub enum ModifierOp {
    Add,
    Multiply,
    Divide,
    Override,
}

/// Stacking configuration
#[derive(Debug, Clone)]
pub struct StackingIR {
    pub stacking_type: StackingType,
    pub limit: i32,
}

/// Stacking types
#[derive(Debug, Clone, PartialEq)]
pub enum StackingType {
    None,
    AggregateBySource,
    AggregateByTarget,
}

/// Tag requirements for effect application/ongoing/removal
#[derive(Debug, Clone, Default)]
pub struct TagRequirementsIR {
    pub require: Vec<String>,
    pub ignore: Vec<String>,
}

impl GameplayEffectIR {
    /// Convert AST GameplayEffectDef to IR
    pub fn from_ast(effect: &GameplayEffectDef) -> KainResult<Self> {
        // Verify @gameplay_effect attribute
        if !effect.attributes.iter().any(|a| a.name == "gameplay_effect") {
            return Err(KainError::codegen(
                format!("Struct '{}' must have @gameplay_effect attribute", effect.name),
                effect.span,
            ));
        }

        // Parse duration policy with defaults
        let duration_policy = Self::parse_duration_policy(effect)?;

        // Validate duration magnitude for HasDuration policy
        if duration_policy == DurationPolicy::HasDuration && effect.duration_magnitude.is_none() {
            return Err(KainError::codegen(
                format!("Effect '{}' has HasDuration policy but no duration magnitude specified", effect.name),
                effect.span,
            ));
        }

        // Parse modifiers
        let modifiers = Self::parse_modifiers(effect)?;

        // Parse stacking
        let stacking = Self::parse_stacking(effect)?;

        // Validate tag syntax
        Self::validate_tags(&effect.owned_tags, effect.span)?;
        Self::validate_tags(&effect.granted_tags, effect.span)?;
        Self::validate_tags(&effect.application_required_tags, effect.span)?;
        Self::validate_tags(&effect.application_ignored_tags, effect.span)?;
        Self::validate_tags(&effect.ongoing_required_tags, effect.span)?;
        Self::validate_tags(&effect.ongoing_ignored_tags, effect.span)?;
        Self::validate_tags(&effect.removal_required_tags, effect.span)?;
        Self::validate_tags(&effect.removal_ignored_tags, effect.span)?;

        Ok(GameplayEffectIR {
            name: effect.name.clone(),
            duration_policy,
            duration_magnitude: effect.duration_magnitude,
            period: effect.period,
            execute_on_application: effect.execute_on_application,
            modifiers,
            stacking,
            owned_tags: effect.owned_tags.clone(),
            granted_tags: effect.granted_tags.clone(),
            application_tag_requirements: TagRequirementsIR {
                require: effect.application_required_tags.clone(),
                ignore: effect.application_ignored_tags.clone(),
            },
            ongoing_tag_requirements: TagRequirementsIR {
                require: effect.ongoing_required_tags.clone(),
                ignore: effect.ongoing_ignored_tags.clone(),
            },
            removal_tag_requirements: TagRequirementsIR {
                require: effect.removal_required_tags.clone(),
                ignore: effect.removal_ignored_tags.clone(),
            },
        })
    }

    /// Parse duration policy from AST
    fn parse_duration_policy(effect: &GameplayEffectDef) -> KainResult<DurationPolicy> {
        if let Some(policy_str) = &effect.duration_policy {
            match policy_str.as_str() {
                "Instant" => Ok(DurationPolicy::Instant),
                "Infinite" => Ok(DurationPolicy::Infinite),
                "HasDuration" => Ok(DurationPolicy::HasDuration),
                _ => Err(KainError::codegen(
                    format!("Invalid duration policy: {}. Valid values: Instant, Infinite, HasDuration", policy_str),
                    effect.span,
                )),
            }
        } else {
            Ok(DurationPolicy::default())
        }
    }

    /// Parse modifiers from AST
    fn parse_modifiers(effect: &GameplayEffectDef) -> KainResult<Vec<ModifierIR>> {
        let mut modifiers = Vec::new();

        for modifier in &effect.modifiers {
            let operation = match modifier.operation.as_str() {
                "Add" => ModifierOp::Add,
                "Multiply" => ModifierOp::Multiply,
                "Divide" => ModifierOp::Divide,
                "Override" => ModifierOp::Override,
                _ => {
                    return Err(KainError::codegen(
                        format!("Invalid modifier operation: {}. Valid values: Add, Multiply, Divide, Override", modifier.operation),
                        modifier.span,
                    ));
                }
            };

            modifiers.push(ModifierIR {
                attribute: modifier.attribute.clone(),
                operation,
                magnitude: modifier.magnitude,
            });
        }

        Ok(modifiers)
    }

    /// Parse stacking configuration from AST
    fn parse_stacking(effect: &GameplayEffectDef) -> KainResult<Option<StackingIR>> {
        if let Some(stacking_type_str) = &effect.stacking_type {
            let stacking_type = match stacking_type_str.as_str() {
                "None" => StackingType::None,
                "AggregateBySource" => StackingType::AggregateBySource,
                "AggregateByTarget" => StackingType::AggregateByTarget,
                _ => {
                    return Err(KainError::codegen(
                        format!("Invalid stacking type: {}. Valid values: None, AggregateBySource, AggregateByTarget", stacking_type_str),
                        effect.span,
                    ));
                }
            };

            let limit = effect.stacking_limit.unwrap_or(1);

            if limit < 1 {
                return Err(KainError::codegen(
                    format!("Stacking limit must be at least 1, got {}", limit),
                    effect.span,
                ));
            }

            Ok(Some(StackingIR {
                stacking_type,
                limit,
            }))
        } else {
            Ok(None)
        }
    }

    /// Validate tag syntax (must be dot-separated identifiers)
    fn validate_tags(tags: &[String], span: Span) -> KainResult<()> {
        for tag in tags {
            if tag.is_empty() {
                return Err(KainError::codegen(
                    "Tag cannot be empty".to_string(),
                    span,
                ));
            }

            // Tags must be dot-separated identifiers
            for part in tag.split('.') {
                if part.is_empty() {
                    return Err(KainError::codegen(
                        format!("Invalid tag '{}': empty component", tag),
                        span,
                    ));
                }

                // Check if part is a valid identifier
                if !part.chars().next().unwrap().is_alphabetic() {
                    return Err(KainError::codegen(
                        format!("Invalid tag '{}': component '{}' must start with a letter", tag, part),
                        span,
                    ));
                }

                for ch in part.chars() {
                    if !ch.is_alphanumeric() && ch != '_' {
                        return Err(KainError::codegen(
                            format!("Invalid tag '{}': component '{}' contains invalid character '{}'", tag, part, ch),
                            span,
                        ));
                    }
                }
            }
        }

        Ok(())
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_duration_policy_default() {
        assert_eq!(DurationPolicy::default(), DurationPolicy::Instant);
    }

    #[test]
    fn test_modifier_op_variants() {
        // Just ensure all variants exist
        let _ops = vec![
            ModifierOp::Add,
            ModifierOp::Multiply,
            ModifierOp::Divide,
            ModifierOp::Override,
        ];
    }

    #[test]
    fn test_stacking_type_variants() {
        // Just ensure all variants exist
        let _types = vec![
            StackingType::None,
            StackingType::AggregateBySource,
            StackingType::AggregateByTarget,
        ];
    }

    #[test]
    fn test_validate_tags_valid() {
        let tags = vec![
            "Effect.Burn".to_string(),
            "Status.Burning".to_string(),
            "Damage.Fire".to_string(),
        ];
        assert!(GameplayEffectIR::validate_tags(&tags, Span::default()).is_ok());
    }

    #[test]
    fn test_validate_tags_empty() {
        let tags = vec!["".to_string()];
        assert!(GameplayEffectIR::validate_tags(&tags, Span::default()).is_err());
    }

    #[test]
    fn test_validate_tags_empty_component() {
        let tags = vec!["Effect..Burn".to_string()];
        assert!(GameplayEffectIR::validate_tags(&tags, Span::default()).is_err());
    }

    #[test]
    fn test_validate_tags_invalid_start() {
        let tags = vec!["1Effect.Burn".to_string()];
        assert!(GameplayEffectIR::validate_tags(&tags, Span::default()).is_err());
    }

    #[test]
    fn test_validate_tags_invalid_char() {
        let tags = vec!["Effect.Burn!".to_string()];
        assert!(GameplayEffectIR::validate_tags(&tags, Span::default()).is_err());
    }
}
