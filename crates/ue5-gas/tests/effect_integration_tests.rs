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

fn create_empty_effect(name: &str) -> GameplayEffectIR {
    GameplayEffectIR {
        name: name.to_string(),
        duration_policy: DurationPolicy::Instant,
        duration_magnitude: None,
        period: None,
        execute_on_application: false,
        modifiers: vec![],
        stacking: None,
        owned_tags: vec![],
        granted_tags: vec![],
        application_tag_requirements: TagRequirementsIR::default(),
        ongoing_tag_requirements: TagRequirementsIR::default(),
        removal_tag_requirements: TagRequirementsIR::default(),
    }
}

fn create_simple_burn_effect() -> GameplayEffectIR {
    GameplayEffectIR {
        name: "BurnEffect".to_string(),
        duration_policy: DurationPolicy::HasDuration,
        duration_magnitude: Some(5.0),
        period: Some(1.0),
        execute_on_application: true,
        modifiers: vec![ModifierIR {
            attribute: "HealthSet.Health".to_string(),
            operation: ModifierOp::Add,
            magnitude: -10.0,
        }],
        stacking: Some(StackingIR {
            stacking_type: StackingType::AggregateBySource,
            limit: 5,
        }),
        owned_tags: vec!["Effect.Burn".to_string()],
        granted_tags: vec!["Status.Burning".to_string()],
        application_tag_requirements: TagRequirementsIR {
            require: vec!["Weakness.Fire".to_string()],
            ignore: vec!["Immunity.Fire".to_string()],
        },
        ongoing_tag_requirements: TagRequirementsIR::default(),
        removal_tag_requirements: TagRequirementsIR::default(),
    }
}

// ============================================================================
// Duration Tests
// ============================================================================

#[test]
fn test_instant_duration() {
    let ir = create_empty_effect("InstantEffect");
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::Instant"));
    assert!(!output.source.contains("DurationMagnitude"));
}

#[test]
fn test_infinite_duration() {
    let mut ir = create_empty_effect("InfiniteEffect");
    ir.duration_policy = DurationPolicy::Infinite;
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::Infinite"));
}

#[test]
fn test_has_duration_with_magnitude() {
    let mut ir = create_empty_effect("TimedEffect");
    ir.duration_policy = DurationPolicy::HasDuration;
    ir.duration_magnitude = Some(10.0);
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    println!("Generated source:\n{}", output.source);
    
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::HasDuration"));
    assert!(output.source.contains("DurationMagnitude = FScalableFloat(10"));
}

// ============================================================================
// Period Tests
// ============================================================================

#[test]
fn test_periodic_execution() {
    let mut ir = create_empty_effect("PeriodicEffect");
    ir.period = Some(2.0);
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("Period = FScalableFloat(2.0f)"));
}

#[test]
fn test_execute_on_application() {
    let mut ir = create_empty_effect("ExecuteOnAppEffect");
    ir.period = Some(1.0);
    ir.execute_on_application = true;
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("Period = FScalableFloat(1.0f)"));
    assert!(output.source.contains("bExecutePeriodicEffectOnApplication = true"));
}

#[test]
fn test_no_execute_on_application() {
    let mut ir = create_empty_effect("NoExecuteEffect");
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
    let mut ir = create_empty_effect("AdditiveEffect");
    ir.modifiers.push(ModifierIR {
        attribute: "HealthSet.Health".to_string(),
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
    let mut ir = create_empty_effect("MultiplicativeEffect");
    ir.modifiers.push(ModifierIR {
        attribute: "CombatSet.Damage".to_string(),
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
    let mut ir = create_empty_effect("DivisionEffect");
    ir.modifiers.push(ModifierIR {
        attribute: "MovementSet.Speed".to_string(),
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
    let mut ir = create_empty_effect("OverrideEffect");
    ir.modifiers.push(ModifierIR {
        attribute: "HealthSet.MaxHealth".to_string(),
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
    let mut ir = create_empty_effect("MultiModifierEffect");
    ir.modifiers.push(ModifierIR {
        attribute: "HealthSet.Health".to_string(),
        operation: ModifierOp::Add,
        magnitude: -10.0,
    });
    ir.modifiers.push(ModifierIR {
        attribute: "StaminaSet.Stamina".to_string(),
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
    let mut ir = create_empty_effect("NoStackEffect");
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
    let mut ir = create_empty_effect("SourceStackEffect");
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
    let mut ir = create_empty_effect("TargetStackEffect");
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
    let mut ir = create_empty_effect("LimitedStackEffect");
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
    let mut ir = create_empty_effect("OwnedTagEffect");
    ir.owned_tags.push("Effect.Burn".to_string());
    ir.owned_tags.push("Effect.Damage".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("InheritableOwnedTagsContainer.AddTag"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Effect.Burn\"))"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Effect.Damage\"))"));
}

#[test]
fn test_granted_tags() {
    let mut ir = create_empty_effect("GrantedTagEffect");
    ir.granted_tags.push("Status.Burning".to_string());
    ir.granted_tags.push("Status.Damaged".to_string());
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("InheritableGameplayEffectTags.Added.AddTag"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Status.Burning\"))"));
    assert!(output.source.contains("FGameplayTag::RequestGameplayTag(FName(\"Status.Damaged\"))"));
}

#[test]
fn test_application_requirements() {
    let mut ir = create_empty_effect("RequirementEffect");
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
    let mut ir = create_empty_effect("OngoingEffect");
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
    let mut ir = create_empty_effect("RemovalEffect");
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
    
    // Complete effect should generate at least 50 lines
    assert!(total_lines > 50, "Generated {} lines, expected > 50", total_lines);
    
    println!("Compression ratio: 1 effect → {} C++ lines (1:{})", total_lines, total_lines);
}

#[test]
fn test_includes() {
    let ir = create_empty_effect("TestEffect");
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
    let ir = create_empty_effect("MyEffect");
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("class UMyEffect : public UGameplayEffect"));
    assert!(output.header.contains("GENERATED_BODY()"));
    assert!(output.header.contains("UCLASS(MinimalAPI, BlueprintType)"));
}

#[test]
fn test_constructor_initialization() {
    let ir = create_empty_effect("InitEffect");
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("UInitEffect::UInitEffect()"));
}

#[test]
fn test_full_output_structure() {
    let ir = create_empty_effect("StructureEffect");
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
    let ir = create_empty_effect("MinimalEffect");
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    // Should still generate valid C++
    assert!(output.header.contains("class UMinimalEffect"));
    assert!(output.source.contains("UMinimalEffect::UMinimalEffect()"));
    assert!(output.source.contains("DurationPolicy = EGameplayEffectDurationType::Instant"));
}

#[test]
fn test_snake_case_to_pascal_case() {
    let mut ir = create_empty_effect("TestEffect");
    ir.modifiers.push(ModifierIR {
        attribute: "HealthSet.MaxHealth".to_string(),
        operation: ModifierOp::Add,
        magnitude: 50.0,
    });
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    // Should convert MaxHealth correctly
    assert!(output.source.contains("GetMaxHealthAttribute()"));
}

#[test]
fn test_multiple_application_requirements() {
    let mut ir = create_empty_effect("MultiReqEffect");
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
    let ir = GameplayEffectIR {
        name: "ComplexEffect".to_string(),
        duration_policy: DurationPolicy::HasDuration,
        duration_magnitude: Some(10.0),
        period: Some(2.0),
        execute_on_application: true,
        modifiers: vec![
            ModifierIR {
                attribute: "HealthSet.Health".to_string(),
                operation: ModifierOp::Add,
                magnitude: -5.0,
            },
            ModifierIR {
                attribute: "StaminaSet.Stamina".to_string(),
                operation: ModifierOp::Add,
                magnitude: -3.0,
            },
            ModifierIR {
                attribute: "MovementSet.Speed".to_string(),
                operation: ModifierOp::Multiply,
                magnitude: 0.8,
            },
        ],
        stacking: Some(StackingIR {
            stacking_type: StackingType::AggregateBySource,
            limit: 3,
        }),
        owned_tags: vec!["Effect.Poison".to_string(), "Effect.Debuff".to_string()],
        granted_tags: vec!["Status.Poisoned".to_string(), "Status.Slowed".to_string()],
        application_tag_requirements: TagRequirementsIR {
            require: vec!["Status.Alive".to_string()],
            ignore: vec!["Immunity.Poison".to_string()],
        },
        ongoing_tag_requirements: TagRequirementsIR {
            require: vec!["Status.Conscious".to_string()],
            ignore: vec![],
        },
        removal_tag_requirements: TagRequirementsIR {
            require: vec!["Action.Cleanse".to_string()],
            ignore: vec![],
        },
    };
    
    let output = generate_effect(&ir, "TestPlugin").unwrap();
    
    let total_lines = output.header.lines().count() + output.source.lines().count();
    
    println!("Complex effect compression: {} C++ lines", total_lines);
    
    // Complex effect should generate 80+ lines
    assert!(total_lines > 80, "Generated {} lines, expected > 80", total_lines);
}
