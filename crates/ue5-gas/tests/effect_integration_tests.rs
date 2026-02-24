// ============================================================================
// Gameplay Effect Integration Tests — Test codegen output
// ============================================================================

use ue5_gas::{
    GameplayEffectIR, DurationPolicy, ModifierIR, ModifierOp, StackingIR, StackingType,
    TagRequirementsIR, generate_effect,
};

// ============================================================================
// Helper Functions
// ============================================================================

fn create_simple_burn_effect() -> GameplayEffectIR {
    let mut ir = GameplayEffectIR::new("BurnEffect".to_string());
    ir.duration_policy = DurationPolicy::HasDuration;
    ir.duration_magnitude = Some(5.0);
    ir.period = Some(1.0);
    ir.execute_on_application = true;
    ir.modifiers.push(ModifierIR {
        attribute: "health".to_string(),
        attribute_set: "HealthSet".to_string(),
        operation: ModifierOp::Add,
        magnitude: -10.0,
    });
    ir.stacking = Some(StackingIR {
        stacking_type: StackingType::AggregateBySource,
        limit: 5,
    });
    ir.owned_tags.push("Effect.Burn".to_string());
    ir.granted_tags.push("Status.Burning".to_string());
    ir.application_tag_requirements.require.push("Weakness.Fire".to_string());
    ir.application_tag_requirements.ignore.push("Immunity.Fire".to_string());
    ir
}

// ============================================================================
// Duration Tests
// ============================================================================

#[test]
fn test_instant_duration() {
    let mut ir = GameplayEffectIR::new("InstantEffect".to_string());
    ir.duration_policy = DurationPolicy::Instant;
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::Instant"));
    assert!(!output.source.contains("DurationMagnitude"));
}

#[test]
fn test_infinite_duration() {
    let mut ir = GameplayEffectIR::new("InfiniteEffect".to_string());
    ir.duration_policy = DurationPolicy::Infinite;
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::Infinite"));
}

#[test]
fn test_has_duration_with_magnitude() {
    let mut ir = GameplayEffectIR::new("TimedEffect".to_string());
    ir.duration_policy = DurationPolicy::HasDuration;
    ir.duration_magnitude = Some(10.0);
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::HasDuration"));
    assert!(output.source.contains("DurationMagnitude = FScalableFloat(10.0f)"));
}

// ============================================================================
// Period Tests
// ============================================================================

#[test]
fn test_periodic_execution() {
    let mut ir = GameplayEffectIR::new("PeriodicEffect".to_string());
    ir.period = Some(2.0);
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("Period = FScalableFloat(2.0f)"));
}

#[test]
fn test_execute_on_application() {
    let mut ir = GameplayEffectIR::new("ExecuteOnAppEffect".to_string());
    ir.period = Some(1.0);
    ir.execute_on_application = true;
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("Period = FScalableFloat(1.0f)"));
    assert!(output.source.contains("bExecutePeriodicEffectOnApplication = true"));
}

#[test]
fn test_no_execute_on_application() {
    let mut ir = GameplayEffectIR::new("NoExecuteEffect".to_string());
    ir.period = Some(1.0);
    ir.execute_on_application = false;
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("Period = FScalableFloat(1.0f)"));
    assert!(!output.source.contains("bExecutePeriodicEffectOnApplication"));
}

// ============================================================================
// Modifier Tests
// ============================================================================

#[test]
fn test_additive_modifier() {
    let mut ir = GameplayEffectIR::new("AdditiveEffect".to_string());
    ir.modifiers.push(ModifierIR {
        attribute: "health".to_string(),
        attribute_set: "HealthSet".to_string(),
        operation: ModifierOp::Add,
        magnitude: -10.0,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("FGameplayModifierInfo Modifier"));
    assert!(output.source.contains("Modifier.Attribute = UHealthSet::GetHealthAttribute()"));
    assert!(output.source.contains("Modifier.ModifierOp = EGameplayModOp::Additive"));
    assert!(output.source.contains("Modifier.ModifierMagnitude = FScalableFloat(-10.0f)"));
    assert!(output.source.contains("Modifiers.Add(Modifier)"));
}

#[test]
fn test_multiplicative_modifier() {
    let mut ir = GameplayEffectIR::new("MultiplicativeEffect".to_string());
    ir.modifiers.push(ModifierIR {
        attribute: "damage".to_string(),
        attribute_set: "CombatSet".to_string(),
        operation: ModifierOp::Multiply,
        magnitude: 1.5,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("Modifier.Attribute = UCombatSet::GetDamageAttribute()"));
    assert!(output.source.contains("Modifier.ModifierOp = EGameplayModOp::Multiplicative"));
    assert!(output.source.contains("Modifier.ModifierMagnitude = FScalableFloat(1.5f)"));
}

#[test]
fn test_division_modifier() {
    let mut ir = GameplayEffectIR::new("DivisionEffect".to_string());
    ir.modifiers.push(ModifierIR {
        attribute: "speed".to_string(),
        attribute_set: "MovementSet".to_string(),
        operation: ModifierOp::Divide,
        magnitude: 2.0,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("Modifier.Attribute = UMovementSet::GetSpeedAttribute()"));
    assert!(output.source.contains("Modifier.ModifierOp = EGameplayModOp::Division"));
    assert!(output.source.contains("Modifier.ModifierMagnitude = FScalableFloat(2.0f)"));
}

#[test]
fn test_override_modifier() {
    let mut ir = GameplayEffectIR::new("OverrideEffect".to_string());
    ir.modifiers.push(ModifierIR {
        attribute: "max_health".to_string(),
        attribute_set: "HealthSet".to_string(),
        operation: ModifierOp::Override,
        magnitude: 200.0,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("Modifier.Attribute = UHealthSet::GetMaxHealthAttribute()"));
    assert!(output.source.contains("Modifier.ModifierOp = EGameplayModOp::Override"));
    assert!(output.source.contains("Modifier.ModifierMagnitude = FScalableFloat(200.0f)"));
}

#[test]
fn test_multiple_modifiers() {
    let mut ir = GameplayEffectIR::new("MultiModifierEffect".to_string());
    ir.modifiers.push(ModifierIR {
        attribute: "health".to_string(),
        attribute_set: "HealthSet".to_string(),
        operation: ModifierOp::Add,
        magnitude: -10.0,
    });
    ir.modifiers.push(ModifierIR {
        attribute: "stamina".to_string(),
        attribute_set: "StaminaSet".to_string(),
        operation: ModifierOp::Add,
        magnitude: -5.0,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("UHealthSet::GetHealthAttribute()"));
    assert!(output.source.contains("UStaminaSet::GetStaminaAttribute()"));
    
    // Count modifier blocks
    let modifier_count = output.source.matches("FGameplayModifierInfo Modifier").count();
    assert_eq!(modifier_count, 2);
}

// ============================================================================
// Stacking Tests
// ============================================================================

#[test]
fn test_no_stacking() {
    let mut ir = GameplayEffectIR::new("NoStackEffect".to_string());
    ir.stacking = Some(StackingIR {
        stacking_type: StackingType::None,
        limit: 1,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("StackingType = EGameplayEffectStackingType::None"));
    assert!(output.source.contains("StackLimitCount = 1"));
}

#[test]
fn test_aggregate_by_source() {
    let mut ir = GameplayEffectIR::new("SourceStackEffect".to_string());
    ir.stacking = Some(StackingIR {
        stacking_type: StackingType::AggregateBySource,
        limit: 5,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("StackingType = EGameplayEffectStackingType::AggregateBySource"));
    assert!(output.source.contains("StackLimitCount = 5"));
}

#[test]
fn test_aggregate_by_target() {
    let mut ir = GameplayEffectIR::new("TargetStackEffect".to_string());
    ir.stacking = Some(StackingIR {
        stacking_type: StackingType::AggregateByTarget,
        limit: 3,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("StackingType = EGameplayEffectStackingType::AggregateByTarget"));
    assert!(output.source.contains("StackLimitCount = 3"));
}

#[test]
fn test_stacking_limit() {
    let mut ir = GameplayEffectIR::new("LimitedStackEffect".to_string());
    ir.stacking = Some(StackingIR {
        stacking_type: StackingType::AggregateBySource,
        limit: 10,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("StackLimitCount = 10"));
}

// ============================================================================
// Tag Tests
// ============================================================================

#[test]
fn test_owned_tags() {
    let mut ir = GameplayEffectIR::new("OwnedTagEffect".to_string());
    ir.owned_tags.push("Effect.Burn".to_string());
    ir.owned_tags.push("Effect.Damage".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("InheritableOwnedTagsContainer.AddTag"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Effect.Burn\"))"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Effect.Damage\"))"));
}

#[test]
fn test_granted_tags() {
    let mut ir = GameplayEffectIR::new("GrantedTagEffect".to_string());
    ir.granted_tags.push("Status.Burning".to_string());
    ir.granted_tags.push("Status.Damaged".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("InheritableGameplayEffectTags.Added.AddTag"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Status.Burning\"))"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Status.Damaged\"))"));
}

#[test]
fn test_application_requirements() {
    let mut ir = GameplayEffectIR::new("RequirementEffect".to_string());
    ir.application_tag_requirements.require.push("Weakness.Fire".to_string());
    ir.application_tag_requirements.ignore.push("Immunity.Fire".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("ApplicationTagRequirements.RequireTags.AddTag"));
    assert!(output.source.contains("ApplicationTagRequirements.IgnoreTags.AddTag"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Weakness.Fire\"))"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Immunity.Fire\"))"));
}

#[test]
fn test_ongoing_requirements() {
    let mut ir = GameplayEffectIR::new("OngoingEffect".to_string());
    ir.ongoing_tag_requirements.require.push("Status.Alive".to_string());
    ir.ongoing_tag_requirements.ignore.push("Status.Dead".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("OngoingTagRequirements.RequireTags.AddTag"));
    assert!(output.source.contains("OngoingTagRequirements.IgnoreTags.AddTag"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Status.Alive\"))"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Status.Dead\"))"));
}

#[test]
fn test_removal_requirements() {
    let mut ir = GameplayEffectIR::new("RemovalEffect".to_string());
    ir.removal_tag_requirements.require.push("Action.Cleanse".to_string());
    ir.removal_tag_requirements.ignore.push("Status.Permanent".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("RemovalTagRequirements.RequireTags.AddTag"));
    assert!(output.source.contains("RemovalTagRequirements.IgnoreTags.AddTag"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Action.Cleanse\"))"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Status.Permanent\"))"));
}

// ============================================================================
// Complex Tests
// ============================================================================

#[test]
fn test_complete_effect_with_all_features() {
    let ir = create_simple_burn_effect();
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    // Verify all features are present
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::HasDuration"));
    assert!(output.source.contains("DurationMagnitude = FScalableFloat(5.0f)"));
    assert!(output.source.contains("Period = FScalableFloat(1.0f)"));
    assert!(output.source.contains("bExecutePeriodicEffectOnApplication = true"));
    assert!(output.source.contains("UHealthSet::GetHealthAttribute()"));
    assert!(output.source.contains("EGameplayModOp::Additive"));
    assert!(output.source.contains("FScalableFloat(-10.0f)"));
    assert!(output.source.contains("StackingType = EGameplayEffectStackingType::AggregateBySource"));
    assert!(output.source.contains("StackLimitCount = 5"));
    assert!(output.source.contains("InheritableOwnedTagsContainer.AddTag"));
    assert!(output.source.contains("Effect.Burn"));
    assert!(output.source.contains("InheritableGameplayEffectTags.Added.AddTag"));
    assert!(output.source.contains("Status.Burning"));
    assert!(output.source.contains("ApplicationTagRequirements.RequireTags.AddTag"));
    assert!(output.source.contains("Weakness.Fire"));
    assert!(output.source.contains("ApplicationTagRequirements.IgnoreTags.AddTag"));
    assert!(output.source.contains("Immunity.Fire"));
}

#[test]
fn test_compression_ratio() {
    let ir = create_simple_burn_effect();
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    let total_lines = output.header.lines().count() + output.source.lines().count();
    
    // Complete effect should generate at least 70 lines
    assert!(total_lines > 70, "Generated {} lines, expected > 70", total_lines);
    
    println!("Compression ratio: 1 effect → {} C++ lines (1:{})", total_lines, total_lines);
}

#[test]
fn test_includes() {
    let ir = GameplayEffectIR::new("TestEffect".to_string());
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    // Header includes
    assert!(output.header.contains("#include \"CoreMinimal.h\""));
    assert!(output.header.contains("#include \"GameplayEffect.h\""));
    assert!(output.header.contains("#include \"TestEffect.generated.h\""));
    
    // Source includes
    assert!(output.source.contains("#include \"TestEffect.h\""));
    assert!(output.source.contains("#include \"GameplayTags.h\""));
}

#[test]
fn test_class_declaration() {
    let ir = GameplayEffectIR::new("MyEffect".to_string());
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("class UMyEffect : public UGameplayEffect"));
    assert!(output.header.contains("GENERATED_BODY()"));
    assert!(output.header.contains("UCLASS(MinimalAPI, BlueprintType)"));
}

#[test]
fn test_constructor_initialization() {
    let ir = GameplayEffectIR::new("InitEffect".to_string());
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("UInitEffect::UInitEffect()"));
}

#[test]
fn test_full_output_structure() {
    let ir = GameplayEffectIR::new("StructureEffect".to_string());
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    // Verify header structure
    assert!(output.header.starts_with("#pragma once"));
    assert!(output.header.contains("class UStructureEffect : public UGameplayEffect"));
    assert!(output.header.contains("public:"));
    assert!(output.header.ends_with("};\n"));
    
    // Verify source structure
    assert!(output.source.contains("#include \"StructureEffect.h\""));
    assert!(output.source.contains("UStructureEffect::UStructureEffect()"));
}

#[test]
fn test_minimal_effect() {
    let ir = GameplayEffectIR::new("MinimalEffect".to_string());
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    // Should still generate valid C++
    assert!(output.header.contains("class UMinimalEffect"));
    assert!(output.source.contains("UMinimalEffect::UMinimalEffect()"));
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::Instant"));
}

#[test]
fn test_snake_case_to_pascal_case() {
    let mut ir = GameplayEffectIR::new("TestEffect".to_string());
    ir.modifiers.push(ModifierIR {
        attribute: "max_health".to_string(),
        attribute_set: "HealthSet".to_string(),
        operation: ModifierOp::Add,
        magnitude: 50.0,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    // Should convert max_health to MaxHealth
    assert!(output.source.contains("GetMaxHealthAttribute()"));
}

#[test]
fn test_multiple_application_requirements() {
    let mut ir = GameplayEffectIR::new("MultiReqEffect".to_string());
    ir.application_tag_requirements.require.push("Status.Alive".to_string());
    ir.application_tag_requirements.require.push("Status.Conscious".to_string());
    ir.application_tag_requirements.ignore.push("Status.Dead".to_string());
    ir.application_tag_requirements.ignore.push("Status.Stunned".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    // Count requirement tags
    let require_count = output.source.matches("ApplicationTagRequirements.RequireTags.AddTag").count();
    let ignore_count = output.source.matches("ApplicationTagRequirements.IgnoreTags.AddTag").count();
    
    assert_eq!(require_count, 2);
    assert_eq!(ignore_count, 2);
}

#[test]
fn test_complex_effect_compression() {
    let mut ir = GameplayEffectIR::new("ComplexEffect".to_string());
    ir.duration_policy = DurationPolicy::HasDuration;
    ir.duration_magnitude = Some(10.0);
    ir.period = Some(2.0);
    ir.execute_on_application = true;
    
    // Multiple modifiers
    ir.modifiers.push(ModifierIR {
        attribute: "health".to_string(),
        attribute_set: "HealthSet".to_string(),
        operation: ModifierOp::Add,
        magnitude: -5.0,
    });
    ir.modifiers.push(ModifierIR {
        attribute: "stamina".to_string(),
        attribute_set: "StaminaSet".to_string(),
        operation: ModifierOp::Add,
        magnitude: -3.0,
    });
    ir.modifiers.push(ModifierIR {
        attribute: "speed".to_string(),
        attribute_set: "MovementSet".to_string(),
        operation: ModifierOp::Multiply,
        magnitude: 0.8,
    });
    
    // Stacking
    ir.stacking = Some(StackingIR {
        stacking_type: StackingType::AggregateBySource,
        limit: 3,
    });
    
    // Tags
    ir.owned_tags.push("Effect.Poison".to_string());
    ir.owned_tags.push("Effect.Debuff".to_string());
    ir.granted_tags.push("Status.Poisoned".to_string());
    ir.granted_tags.push("Status.Slowed".to_string());
    
    // Requirements
    ir.application_tag_requirements.require.push("Status.Alive".to_string());
    ir.application_tag_requirements.ignore.push("Immunity.Poison".to_string());
    ir.ongoing_tag_requirements.require.push("Status.Conscious".to_string());
    ir.removal_tag_requirements.require.push("Action.Cleanse".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    let total_lines = output.header.lines().count() + output.source.lines().count();
    
    println!("Complex effect compression: {} C++ lines", total_lines);
    
    // Complex effect should generate 100+ lines
    assert!(total_lines > 100, "Generated {} lines, expected > 100", total_lines);
}
