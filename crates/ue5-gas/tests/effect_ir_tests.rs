// ============================================================================
// Gameplay Effect IR Tests
// ============================================================================
// Comprehensive tests for gameplay effect IR conversion and validation
// ============================================================================

use kain_core::ast::{GameplayEffectDef, GameplayEffectModifier, Attribute};
use kain_core::span::Span;
use ue5_gas::effect_ir::{GameplayEffectIR, DurationPolicy, ModifierOp, StackingType};

/// Helper to create a test effect with minimal fields
fn create_test_effect(name: &str) -> GameplayEffectDef {
    GameplayEffectDef {
        name: name.to_string(),
        duration_policy: None,
        duration_magnitude: None,
        period: None,
        execute_on_application: false,
        modifiers: Vec::new(),
        stacking_type: None,
        stacking_limit: None,
        owned_tags: Vec::new(),
        granted_tags: Vec::new(),
        application_required_tags: Vec::new(),
        application_ignored_tags: Vec::new(),
        ongoing_required_tags: Vec::new(),
        ongoing_ignored_tags: Vec::new(),
        removal_required_tags: Vec::new(),
        removal_ignored_tags: Vec::new(),
        attributes: vec![Attribute {
            name: "gameplay_effect".to_string(),
            args: Vec::new(),
            span: Span::default(),
        }],
        span: Span::default(),
    }
}

// ============================================================================
// Duration Policy Tests
// ============================================================================

#[test]
fn test_duration_policy_instant() {
    let mut effect = create_test_effect("InstantEffect");
    effect.duration_policy = Some("Instant".to_string());
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.duration_policy, DurationPolicy::Instant);
    assert_eq!(ir.duration_magnitude, None);
}

#[test]
fn test_duration_policy_infinite() {
    let mut effect = create_test_effect("InfiniteEffect");
    effect.duration_policy = Some("Infinite".to_string());
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.duration_policy, DurationPolicy::Infinite);
    assert_eq!(ir.duration_magnitude, None);
}

#[test]
fn test_duration_policy_has_duration() {
    let mut effect = create_test_effect("DurationEffect");
    effect.duration_policy = Some("HasDuration".to_string());
    effect.duration_magnitude = Some(5.0);
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.duration_policy, DurationPolicy::HasDuration);
    assert_eq!(ir.duration_magnitude, Some(5.0));
}

#[test]
fn test_duration_policy_default() {
    let effect = create_test_effect("DefaultEffect");
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.duration_policy, DurationPolicy::Instant);
}

#[test]
fn test_duration_policy_has_duration_without_magnitude() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.duration_policy = Some("HasDuration".to_string());
    // Missing duration_magnitude
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("no duration magnitude"));
}

#[test]
fn test_invalid_duration_policy() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.duration_policy = Some("InvalidPolicy".to_string());
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("Invalid duration policy"));
}

// ============================================================================
// Period Execution Tests
// ============================================================================

#[test]
fn test_period_execution() {
    let mut effect = create_test_effect("PeriodicEffect");
    effect.duration_policy = Some("HasDuration".to_string());
    effect.duration_magnitude = Some(10.0);
    effect.period = Some(1.0);
    effect.execute_on_application = true;
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.period, Some(1.0));
    assert!(ir.execute_on_application);
}

#[test]
fn test_period_without_execute_on_application() {
    let mut effect = create_test_effect("PeriodicEffect");
    effect.duration_policy = Some("HasDuration".to_string());
    effect.duration_magnitude = Some(10.0);
    effect.period = Some(2.0);
    effect.execute_on_application = false;
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.period, Some(2.0));
    assert!(!ir.execute_on_application);
}

// ============================================================================
// Modifier Operation Tests
// ============================================================================

#[test]
fn test_modifier_operation_add() {
    let mut effect = create_test_effect("AddEffect");
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "Health".to_string(),
        operation: "Add".to_string(),
        magnitude: 50.0,
        span: Span::default(),
    });
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.modifiers.len(), 1);
    assert_eq!(ir.modifiers[0].attribute, "Health");
    assert_eq!(ir.modifiers[0].operation, ModifierOp::Add);
    assert_eq!(ir.modifiers[0].magnitude, 50.0);
}

#[test]
fn test_modifier_operation_multiply() {
    let mut effect = create_test_effect("MultiplyEffect");
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "AttackPower".to_string(),
        operation: "Multiply".to_string(),
        magnitude: 1.5,
        span: Span::default(),
    });
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.modifiers[0].operation, ModifierOp::Multiply);
    assert_eq!(ir.modifiers[0].magnitude, 1.5);
}

#[test]
fn test_modifier_operation_divide() {
    let mut effect = create_test_effect("DivideEffect");
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "MovementSpeed".to_string(),
        operation: "Divide".to_string(),
        magnitude: 2.0,
        span: Span::default(),
    });
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.modifiers[0].operation, ModifierOp::Divide);
    assert_eq!(ir.modifiers[0].magnitude, 2.0);
}

#[test]
fn test_modifier_operation_override() {
    let mut effect = create_test_effect("OverrideEffect");
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "MovementSpeed".to_string(),
        operation: "Override".to_string(),
        magnitude: 0.0,
        span: Span::default(),
    });
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.modifiers[0].operation, ModifierOp::Override);
    assert_eq!(ir.modifiers[0].magnitude, 0.0);
}

#[test]
fn test_modifier_negative_magnitude() {
    let mut effect = create_test_effect("DamageEffect");
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "Health".to_string(),
        operation: "Add".to_string(),
        magnitude: -10.0,
        span: Span::default(),
    });
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.modifiers[0].magnitude, -10.0);
}

#[test]
fn test_multiple_modifiers() {
    let mut effect = create_test_effect("ComplexEffect");
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "Health".to_string(),
        operation: "Add".to_string(),
        magnitude: -10.0,
        span: Span::default(),
    });
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "MovementSpeed".to_string(),
        operation: "Multiply".to_string(),
        magnitude: 0.5,
        span: Span::default(),
    });
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.modifiers.len(), 2);
    assert_eq!(ir.modifiers[0].attribute, "Health");
    assert_eq!(ir.modifiers[1].attribute, "MovementSpeed");
}

#[test]
fn test_invalid_modifier_operation() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "Health".to_string(),
        operation: "InvalidOp".to_string(),
        magnitude: 10.0,
        span: Span::default(),
    });
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("Invalid modifier operation"));
}

// ============================================================================
// Stacking Tests
// ============================================================================

#[test]
fn test_stacking_aggregate_by_source() {
    let mut effect = create_test_effect("StackingEffect");
    effect.stacking_type = Some("AggregateBySource".to_string());
    effect.stacking_limit = Some(5);
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert!(ir.stacking.is_some());
    let stacking = ir.stacking.unwrap();
    assert_eq!(stacking.stacking_type, StackingType::AggregateBySource);
    assert_eq!(stacking.limit, 5);
}

#[test]
fn test_stacking_aggregate_by_target() {
    let mut effect = create_test_effect("StackingEffect");
    effect.stacking_type = Some("AggregateByTarget".to_string());
    effect.stacking_limit = Some(3);
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert!(ir.stacking.is_some());
    let stacking = ir.stacking.unwrap();
    assert_eq!(stacking.stacking_type, StackingType::AggregateByTarget);
    assert_eq!(stacking.limit, 3);
}

#[test]
fn test_stacking_none() {
    let mut effect = create_test_effect("NoStackingEffect");
    effect.stacking_type = Some("None".to_string());
    effect.stacking_limit = Some(1);
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert!(ir.stacking.is_some());
    let stacking = ir.stacking.unwrap();
    assert_eq!(stacking.stacking_type, StackingType::None);
}

#[test]
fn test_stacking_default_limit() {
    let mut effect = create_test_effect("StackingEffect");
    effect.stacking_type = Some("AggregateBySource".to_string());
    // No stacking_limit specified
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert!(ir.stacking.is_some());
    let stacking = ir.stacking.unwrap();
    assert_eq!(stacking.limit, 1);
}

#[test]
fn test_stacking_invalid_limit() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.stacking_type = Some("AggregateBySource".to_string());
    effect.stacking_limit = Some(0);
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("Stacking limit must be at least 1"));
}

#[test]
fn test_invalid_stacking_type() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.stacking_type = Some("InvalidType".to_string());
    effect.stacking_limit = Some(5);
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("Invalid stacking type"));
}

// ============================================================================
// Tag Requirements Tests
// ============================================================================

#[test]
fn test_tag_requirements() {
    let mut effect = create_test_effect("TaggedEffect");
    effect.owned_tags = vec!["Effect.Burn".to_string()];
    effect.granted_tags = vec!["Status.Burning".to_string()];
    effect.application_required_tags = vec!["Status.Alive".to_string()];
    effect.application_ignored_tags = vec!["Status.Immune.Fire".to_string()];
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.owned_tags, vec!["Effect.Burn"]);
    assert_eq!(ir.granted_tags, vec!["Status.Burning"]);
    assert_eq!(ir.application_tag_requirements.require, vec!["Status.Alive"]);
    assert_eq!(ir.application_tag_requirements.ignore, vec!["Status.Immune.Fire"]);
}

#[test]
fn test_ongoing_tag_requirements() {
    let mut effect = create_test_effect("OngoingEffect");
    effect.ongoing_required_tags = vec!["Status.InCombat".to_string()];
    effect.ongoing_ignored_tags = vec!["Status.Dead".to_string()];
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.ongoing_tag_requirements.require, vec!["Status.InCombat"]);
    assert_eq!(ir.ongoing_tag_requirements.ignore, vec!["Status.Dead"]);
}

#[test]
fn test_removal_tag_requirements() {
    let mut effect = create_test_effect("RemovalEffect");
    effect.removal_required_tags = vec!["Cleanse.Fire".to_string()];
    effect.removal_ignored_tags = vec!["Status.Immune.Cleanse".to_string()];
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.removal_tag_requirements.require, vec!["Cleanse.Fire"]);
    assert_eq!(ir.removal_tag_requirements.ignore, vec!["Status.Immune.Cleanse"]);
}

#[test]
fn test_multiple_tags() {
    let mut effect = create_test_effect("MultiTagEffect");
    effect.owned_tags = vec![
        "Effect.Damage".to_string(),
        "Effect.DOT".to_string(),
        "Effect.Fire".to_string(),
    ];
    effect.granted_tags = vec![
        "Status.Burning".to_string(),
        "Status.Debuff".to_string(),
    ];
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.owned_tags.len(), 3);
    assert_eq!(ir.granted_tags.len(), 2);
}

// ============================================================================
// Tag Validation Tests
// ============================================================================

#[test]
fn test_valid_tag_syntax() {
    let mut effect = create_test_effect("ValidEffect");
    effect.owned_tags = vec![
        "Effect.Burn".to_string(),
        "Status.CC.Stunned".to_string(),
        "Damage.Physical.Slash".to_string(),
    ];
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_ok());
}

#[test]
fn test_invalid_tag_empty() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.owned_tags = vec!["".to_string()];
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("Tag cannot be empty"));
}

#[test]
fn test_invalid_tag_empty_component() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.owned_tags = vec!["Effect..Burn".to_string()];
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("empty component"));
}

#[test]
fn test_invalid_tag_starts_with_number() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.owned_tags = vec!["1Effect.Burn".to_string()];
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("must start with a letter"));
}

#[test]
fn test_invalid_tag_special_char() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.owned_tags = vec!["Effect.Burn!".to_string()];
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("invalid character"));
}

// ============================================================================
// Complete Effect Tests
// ============================================================================

#[test]
fn test_complete_effect() {
    let mut effect = create_test_effect("BurnEffect");
    effect.duration_policy = Some("HasDuration".to_string());
    effect.duration_magnitude = Some(5.0);
    effect.period = Some(1.0);
    effect.execute_on_application = true;
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "Health".to_string(),
        operation: "Add".to_string(),
        magnitude: -10.0,
        span: Span::default(),
    });
    effect.stacking_type = Some("AggregateBySource".to_string());
    effect.stacking_limit = Some(5);
    effect.owned_tags = vec!["Effect.Burn".to_string()];
    effect.granted_tags = vec!["Status.Burning".to_string()];
    effect.application_required_tags = vec!["Status.Alive".to_string()];
    effect.application_ignored_tags = vec!["Status.Immune.Fire".to_string()];
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.name, "BurnEffect");
    assert_eq!(ir.duration_policy, DurationPolicy::HasDuration);
    assert_eq!(ir.duration_magnitude, Some(5.0));
    assert_eq!(ir.period, Some(1.0));
    assert!(ir.execute_on_application);
    assert_eq!(ir.modifiers.len(), 1);
    assert!(ir.stacking.is_some());
    assert_eq!(ir.owned_tags.len(), 1);
    assert_eq!(ir.granted_tags.len(), 1);
}

#[test]
fn test_instant_damage_effect() {
    let mut effect = create_test_effect("InstantDamage");
    effect.duration_policy = Some("Instant".to_string());
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "Health".to_string(),
        operation: "Add".to_string(),
        magnitude: -50.0,
        span: Span::default(),
    });
    effect.owned_tags = vec!["Effect.Damage.Instant".to_string()];
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.duration_policy, DurationPolicy::Instant);
    assert_eq!(ir.modifiers[0].magnitude, -50.0);
}

#[test]
fn test_infinite_passive_effect() {
    let mut effect = create_test_effect("PassiveRegen");
    effect.duration_policy = Some("Infinite".to_string());
    effect.period = Some(1.0);
    effect.modifiers.push(GameplayEffectModifier {
        attribute: "Health".to_string(),
        operation: "Add".to_string(),
        magnitude: 2.0,
        span: Span::default(),
    });
    effect.owned_tags = vec!["Effect.Heal.HOT".to_string()];
    
    let ir = GameplayEffectIR::from_ast(&effect).unwrap();
    
    assert_eq!(ir.duration_policy, DurationPolicy::Infinite);
    assert_eq!(ir.period, Some(1.0));
}

#[test]
fn test_missing_gameplay_effect_attribute() {
    let mut effect = create_test_effect("InvalidEffect");
    effect.attributes.clear(); // Remove @gameplay_effect attribute
    
    let result = GameplayEffectIR::from_ast(&effect);
    
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("must have @gameplay_effect attribute"));
}
