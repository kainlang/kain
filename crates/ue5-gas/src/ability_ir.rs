// ============================================================================
// Gameplay Ability IR — Intermediate Representation for GAS Abilities
// ============================================================================
// Converts AST gameplay abilities into a structured IR for codegen.
// Handles ability policies, tags, cost/cooldown, and lifecycle hooks.
// ============================================================================

use kain_core::ast::{Function, GameplayAbilityDef};
use kain_core::error::{KainError, KainResult};
use kain_core::span::Span;

/// Intermediate representation of a gameplay ability
#[derive(Debug, Clone)]
pub struct GameplayAbilityIR {
    pub name: String,
    pub instancing_policy: InstancingPolicy,
    pub replication_policy: ReplicationPolicy,
    pub net_execution_policy: NetExecutionPolicy,
    pub ability_tags: Vec<String>,
    pub activation_required_tags: Vec<String>,
    pub activation_blocked_tags: Vec<String>,
    pub activation_owned_tags: Vec<String>,
    pub cancel_abilities_with_tag: Vec<String>,
    pub block_abilities_with_tag: Vec<String>,
    pub cost_effect: Option<String>,
    pub cooldown_effect: Option<String>,
    pub lifecycle_hooks: AbilityLifecycleHooksIR,
}

/// Instancing policy for ability instances
#[derive(Debug, Clone, PartialEq)]
pub enum InstancingPolicy {
    InstancedPerExecution,
    InstancedPerActor,
    NonInstanced,
}

impl Default for InstancingPolicy {
    fn default() -> Self {
        InstancingPolicy::InstancedPerExecution
    }
}

/// Replication policy for ability state
#[derive(Debug, Clone, PartialEq)]
pub enum ReplicationPolicy {
    ReplicateNo,
    ReplicateYes,
}

impl Default for ReplicationPolicy {
    fn default() -> Self {
        ReplicationPolicy::ReplicateYes
    }
}

/// Network execution policy for ability activation
#[derive(Debug, Clone, PartialEq)]
pub enum NetExecutionPolicy {
    LocalPredicted,
    LocalOnly,
    ServerInitiated,
    ServerOnly,
}

impl Default for NetExecutionPolicy {
    fn default() -> Self {
        NetExecutionPolicy::LocalPredicted
    }
}

/// Lifecycle hooks for gameplay abilities
#[derive(Debug, Clone, Default)]
pub struct AbilityLifecycleHooksIR {
    pub can_activate_ability: Option<FunctionIR>,
    pub activate_ability: Option<FunctionIR>,
    pub end_ability: Option<FunctionIR>,
    pub cancel_ability: Option<FunctionIR>,
    pub commit_ability: Option<FunctionIR>,
    pub input_pressed: Option<FunctionIR>,
    pub input_released: Option<FunctionIR>,
}

/// Simplified function IR for lifecycle hooks
#[derive(Debug, Clone)]
pub struct FunctionIR {
    pub name: String,
    pub body: String,
}

impl GameplayAbilityIR {
    /// Convert AST GameplayAbilityDef to IR
    pub fn from_ast(ability: &GameplayAbilityDef) -> KainResult<Self> {
        // Verify @ability attribute
        if !ability.attributes.iter().any(|a| a.name == "ability") {
            return Err(KainError::codegen(
                format!("Struct '{}' must have @ability attribute", ability.name),
                ability.span,
            ));
        }

        // Parse policies with defaults
        let instancing_policy = Self::parse_instancing_policy(ability)?;
        let replication_policy = Self::parse_replication_policy(ability)?;
        let net_execution_policy = Self::parse_net_execution_policy(ability)?;

        // Get tag arrays directly from AST
        let ability_tags = ability.ability_tags.clone();
        let activation_required_tags = ability.activation_required_tags.clone();
        let activation_blocked_tags = ability.activation_blocked_tags.clone();
        let activation_owned_tags = ability.activation_owned_tags.clone();
        let cancel_abilities_with_tag = ability.cancel_abilities_with_tag.clone();
        let block_abilities_with_tag = ability.block_abilities_with_tag.clone();

        // Validate tag syntax
        Self::validate_tags(&ability_tags, ability.span)?;
        Self::validate_tags(&activation_required_tags, ability.span)?;
        Self::validate_tags(&activation_blocked_tags, ability.span)?;
        Self::validate_tags(&activation_owned_tags, ability.span)?;
        Self::validate_tags(&cancel_abilities_with_tag, ability.span)?;
        Self::validate_tags(&block_abilities_with_tag, ability.span)?;

        // Get cost and cooldown effects directly from AST
        let cost_effect = ability.cost_effect.clone();
        let cooldown_effect = ability.cooldown_effect.clone();

        // Parse lifecycle hooks from methods
        let lifecycle_hooks = Self::parse_lifecycle_hooks(&ability.methods)?;

        Ok(GameplayAbilityIR {
            name: ability.name.clone(),
            instancing_policy,
            replication_policy,
            net_execution_policy,
            ability_tags,
            activation_required_tags,
            activation_blocked_tags,
            activation_owned_tags,
            cancel_abilities_with_tag,
            block_abilities_with_tag,
            cost_effect,
            cooldown_effect,
            lifecycle_hooks,
        })
    }

    /// Parse instancing policy from AST
    fn parse_instancing_policy(ability: &GameplayAbilityDef) -> KainResult<InstancingPolicy> {
        if let Some(policy_str) = &ability.instancing_policy {
            match policy_str.as_str() {
                "InstancedPerExecution" => Ok(InstancingPolicy::InstancedPerExecution),
                "InstancedPerActor" => Ok(InstancingPolicy::InstancedPerActor),
                "NonInstanced" => Ok(InstancingPolicy::NonInstanced),
                _ => Err(KainError::codegen(
                    format!("Invalid instancing policy: {}. Valid values: InstancedPerExecution, InstancedPerActor, NonInstanced", policy_str),
                    ability.span,
                )),
            }
        } else {
            Ok(InstancingPolicy::default())
        }
    }

    /// Parse replication policy from AST
    fn parse_replication_policy(ability: &GameplayAbilityDef) -> KainResult<ReplicationPolicy> {
        if let Some(policy_str) = &ability.replication_policy {
            match policy_str.as_str() {
                "ReplicateNo" => Ok(ReplicationPolicy::ReplicateNo),
                "ReplicateYes" => Ok(ReplicationPolicy::ReplicateYes),
                _ => Err(KainError::codegen(
                    format!(
                        "Invalid replication policy: {}. Valid values: ReplicateNo, ReplicateYes",
                        policy_str
                    ),
                    ability.span,
                )),
            }
        } else {
            Ok(ReplicationPolicy::default())
        }
    }

    /// Parse net execution policy from AST
    fn parse_net_execution_policy(ability: &GameplayAbilityDef) -> KainResult<NetExecutionPolicy> {
        if let Some(policy_str) = &ability.net_execution_policy {
            match policy_str.as_str() {
                "LocalPredicted" => Ok(NetExecutionPolicy::LocalPredicted),
                "LocalOnly" => Ok(NetExecutionPolicy::LocalOnly),
                "ServerInitiated" => Ok(NetExecutionPolicy::ServerInitiated),
                "ServerOnly" => Ok(NetExecutionPolicy::ServerOnly),
                _ => Err(KainError::codegen(
                    format!("Invalid net execution policy: {}. Valid values: LocalPredicted, LocalOnly, ServerInitiated, ServerOnly", policy_str),
                    ability.span,
                )),
            }
        } else {
            Ok(NetExecutionPolicy::default())
        }
    }

    /// Validate tag syntax (must be dot-separated identifiers)
    fn validate_tags(tags: &[String], span: Span) -> KainResult<()> {
        for tag in tags {
            if tag.is_empty() {
                return Err(KainError::codegen("Tag cannot be empty".to_string(), span));
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
                        format!(
                            "Invalid tag '{}': component '{}' must start with a letter",
                            tag, part
                        ),
                        span,
                    ));
                }

                for ch in part.chars() {
                    if !ch.is_alphanumeric() && ch != '_' {
                        return Err(KainError::codegen(
                            format!(
                                "Invalid tag '{}': component '{}' contains invalid character '{}'",
                                tag, part, ch
                            ),
                            span,
                        ));
                    }
                }
            }
        }

        Ok(())
    }

    /// Parse lifecycle hooks from methods
    fn parse_lifecycle_hooks(methods: &[Function]) -> KainResult<AbilityLifecycleHooksIR> {
        let mut hooks = AbilityLifecycleHooksIR::default();

        for method in methods {
            let func_ir = FunctionIR {
                name: method.name.clone(),
                body: format!("{:?}", method.body), // Placeholder for now
            };

            match method.name.as_str() {
                "can_activate_ability" => hooks.can_activate_ability = Some(func_ir),
                "activate_ability" => hooks.activate_ability = Some(func_ir),
                "end_ability" => hooks.end_ability = Some(func_ir),
                "cancel_ability" => hooks.cancel_ability = Some(func_ir),
                "commit_ability" => hooks.commit_ability = Some(func_ir),
                "input_pressed" => hooks.input_pressed = Some(func_ir),
                "input_released" => hooks.input_released = Some(func_ir),
                _ => {
                    // Unknown method - could be a helper function, ignore for now
                }
            }
        }

        Ok(hooks)
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_instancing_policy_default() {
        assert_eq!(
            InstancingPolicy::default(),
            InstancingPolicy::InstancedPerExecution
        );
    }

    #[test]
    fn test_replication_policy_default() {
        assert_eq!(
            ReplicationPolicy::default(),
            ReplicationPolicy::ReplicateYes
        );
    }

    #[test]
    fn test_net_execution_policy_default() {
        assert_eq!(
            NetExecutionPolicy::default(),
            NetExecutionPolicy::LocalPredicted
        );
    }

    #[test]
    fn test_validate_tags_valid() {
        let tags = vec![
            "Ability.Jump".to_string(),
            "Status.Grounded".to_string(),
            "Effect.Buff.Speed".to_string(),
        ];
        assert!(GameplayAbilityIR::validate_tags(&tags, Span::default()).is_ok());
    }

    #[test]
    fn test_validate_tags_empty() {
        let tags = vec!["".to_string()];
        assert!(GameplayAbilityIR::validate_tags(&tags, Span::default()).is_err());
    }

    #[test]
    fn test_validate_tags_empty_component() {
        let tags = vec!["Ability..Jump".to_string()];
        assert!(GameplayAbilityIR::validate_tags(&tags, Span::default()).is_err());
    }

    #[test]
    fn test_validate_tags_invalid_start() {
        let tags = vec!["1Ability.Jump".to_string()];
        assert!(GameplayAbilityIR::validate_tags(&tags, Span::default()).is_err());
    }

    #[test]
    fn test_validate_tags_invalid_char() {
        let tags = vec!["Ability.Jump!".to_string()];
        assert!(GameplayAbilityIR::validate_tags(&tags, Span::default()).is_err());
    }
}
