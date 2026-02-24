// ============================================================================
// Gameplay Ability Integration Tests — Test codegen output
// ============================================================================

use ue5_gas::{
    GameplayAbilityIR, AbilityLifecycleHooksIR,
    InstancingPolicy, ReplicationPolicy, NetExecutionPolicy,
    generate_ability,
};
use ue5_gas::ability_ir::FunctionIR;

// ============================================================================
// Helper Functions
// ============================================================================

fn create_simple_jump_ability() -> GameplayAbilityIR {
    GameplayAbilityIR {
        name: "JumpAbility".to_string(),
        instancing_policy: InstancingPolicy::InstancedPerExecution,
        replication_policy: ReplicationPolicy::ReplicateYes,
        net_execution_policy: NetExecutionPolicy::LocalPredicted,
        ability_tags: vec!["Ability.Jump".to_string()],
        activation_required_tags: vec!["Status.Grounded".to_string()],
        activation_blocked_tags: vec!["Status.Stunned".to_string()],
        activation_owned_tags: vec!["Status.Jumping".to_string()],
        cancel_abilities_with_tag: vec![],
        block_abilities_with_tag: vec![],
        cost_effect: None,
        cooldown_effect: None,
        lifecycle_hooks: AbilityLifecycleHooksIR::default(),
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

#[test]
fn test_class_declaration() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("class UJumpAbility : public UGameplayAbility"));
    assert!(output.header.contains("GENERATED_BODY()"));
    assert!(output.header.contains("UCLASS(MinimalAPI, Blueprintable)"));
}

#[test]
fn test_constructor_declaration() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("UJumpAbility();"));
}

#[test]
fn test_includes() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // Header includes
    assert!(output.header.contains("#include \"CoreMinimal.h\""));
    assert!(output.header.contains("#include \"Abilities/GameplayAbility.h\""));
    assert!(output.header.contains("#include \"JumpAbility.generated.h\""));
    
    // Source includes
    assert!(output.source.contains("#include \"JumpAbility.h\""));
    assert!(output.source.contains("#include \"GameplayTags.h\""));
}

#[test]
fn test_constructor_policies() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("UJumpAbility::UJumpAbility()"));
    assert!(output.source.contains("InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution"));
    assert!(output.source.contains("ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes"));
    assert!(output.source.contains("NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted"));
}

#[test]
fn test_ability_tags_initialization() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Ability tags"));
    assert!(output.source.contains("AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Ability.Jump\")))"));
}

#[test]
fn test_activation_required_tags() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Activation required tags"));
    assert!(output.source.contains("ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Status.Grounded\")))"));
}

#[test]
fn test_activation_blocked_tags() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Activation blocked tags"));
    assert!(output.source.contains("ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Status.Stunned\")))"));
}

#[test]
fn test_activation_owned_tags() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Activation owned tags"));
    assert!(output.source.contains("ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Status.Jumping\")))"));
}

#[test]
fn test_multiple_ability_tags() {
    let mut ir = create_simple_jump_ability();
    ir.ability_tags = vec![
        "Ability.Jump".to_string(),
        "Ability.Movement".to_string(),
        "Ability.Traversal".to_string(),
    ];
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Ability.Jump\")))"));
    assert!(output.source.contains("AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Ability.Movement\")))"));
    assert!(output.source.contains("AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Ability.Traversal\")))"));
}

#[test]
fn test_multiple_blocked_tags() {
    let mut ir = create_simple_jump_ability();
    ir.activation_blocked_tags = vec![
        "Status.Stunned".to_string(),
        "Status.Rooted".to_string(),
        "Status.Dead".to_string(),
    ];
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Status.Stunned\")))"));
    assert!(output.source.contains("ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Status.Rooted\")))"));
    assert!(output.source.contains("ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Status.Dead\")))"));
}

#[test]
fn test_cancel_abilities_with_tag() {
    let mut ir = create_simple_jump_ability();
    ir.cancel_abilities_with_tag = vec!["Ability.Sprint".to_string()];
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Cancel abilities with tag"));
    assert!(output.source.contains("CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Ability.Sprint\")))"));
}

#[test]
fn test_block_abilities_with_tag() {
    let mut ir = create_simple_jump_ability();
    ir.block_abilities_with_tag = vec!["Ability.Attack".to_string(), "Ability.Skill".to_string()];
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Block abilities with tag"));
    assert!(output.source.contains("BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Ability.Attack\")))"));
    assert!(output.source.contains("BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName(\"Ability.Skill\")))"));
}

#[test]
fn test_cost_effect() {
    let mut ir = create_simple_jump_ability();
    ir.cost_effect = Some("StaminaCostEffect".to_string());
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Cost effect"));
    assert!(output.source.contains("CostGameplayEffectClass = UStaminaCostEffect::StaticClass()"));
}

#[test]
fn test_cooldown_effect() {
    let mut ir = create_simple_jump_ability();
    ir.cooldown_effect = Some("JumpCooldownEffect".to_string());
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Cooldown effect"));
    assert!(output.source.contains("CooldownGameplayEffectClass = UJumpCooldownEffect::StaticClass()"));
}

#[test]
fn test_cost_and_cooldown() {
    let mut ir = create_simple_jump_ability();
    ir.cost_effect = Some("StaminaCostEffect".to_string());
    ir.cooldown_effect = Some("JumpCooldownEffect".to_string());
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("CostGameplayEffectClass = UStaminaCostEffect::StaticClass()"));
    assert!(output.source.contains("CooldownGameplayEffectClass = UJumpCooldownEffect::StaticClass()"));
}

#[test]
fn test_can_activate_ability_hook() {
    let mut ir = create_simple_jump_ability();
    ir.lifecycle_hooks.can_activate_ability = Some(FunctionIR {
        name: "can_activate_ability".to_string(),
        body: "// Custom logic".to_string(),
    });
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // Header declaration
    assert!(output.header.contains("virtual bool CanActivateAbility"));
    assert!(output.header.contains("const FGameplayAbilitySpecHandle Handle"));
    assert!(output.header.contains("const FGameplayAbilityActorInfo* ActorInfo"));
    assert!(output.header.contains("const FGameplayTagContainer* SourceTags"));
    assert!(output.header.contains("const FGameplayTagContainer* TargetTags"));
    assert!(output.header.contains("OUT FGameplayTagContainer* OptionalRelevantTags"));
    
    // Source implementation
    assert!(output.source.contains("bool UJumpAbility::CanActivateAbility"));
    assert!(output.source.contains("Super::CanActivateAbility"));
    assert!(output.source.contains("// TODO: User-defined logic"));
    assert!(output.source.contains("return true"));
}

#[test]
fn test_activate_ability_hook() {
    let mut ir = create_simple_jump_ability();
    ir.lifecycle_hooks.activate_ability = Some(FunctionIR {
        name: "activate_ability".to_string(),
        body: "// Custom logic".to_string(),
    });
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // Header declaration
    assert!(output.header.contains("virtual void ActivateAbility"));
    assert!(output.header.contains("const FGameplayAbilitySpecHandle Handle"));
    assert!(output.header.contains("const FGameplayAbilityActorInfo* ActorInfo"));
    assert!(output.header.contains("const FGameplayAbilityActivationInfo ActivationInfo"));
    assert!(output.header.contains("const FGameplayEventData* TriggerEventData"));
    
    // Source implementation
    assert!(output.source.contains("void UJumpAbility::ActivateAbility"));
    assert!(output.source.contains("Super::ActivateAbility"));
    assert!(output.source.contains("CommitAbility"));
    assert!(output.source.contains("EndAbility"));
    assert!(output.source.contains("// TODO: User-defined logic"));
}

#[test]
fn test_end_ability_hook() {
    let mut ir = create_simple_jump_ability();
    ir.lifecycle_hooks.end_ability = Some(FunctionIR {
        name: "end_ability".to_string(),
        body: "// Cleanup".to_string(),
    });
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // Header declaration
    assert!(output.header.contains("virtual void EndAbility"));
    assert!(output.header.contains("bool bReplicateEndAbility"));
    assert!(output.header.contains("bool bWasCancelled"));
    
    // Source implementation
    assert!(output.source.contains("void UJumpAbility::EndAbility"));
    assert!(output.source.contains("Super::EndAbility"));
    assert!(output.source.contains("// TODO: User-defined cleanup logic"));
}

#[test]
fn test_input_pressed_hook() {
    let mut ir = create_simple_jump_ability();
    ir.lifecycle_hooks.input_pressed = Some(FunctionIR {
        name: "input_pressed".to_string(),
        body: "// Input logic".to_string(),
    });
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // Header declaration
    assert!(output.header.contains("virtual void InputPressed"));
    
    // Source implementation
    assert!(output.source.contains("void UJumpAbility::InputPressed"));
    assert!(output.source.contains("Super::InputPressed"));
    assert!(output.source.contains("// TODO: User-defined input pressed logic"));
}

#[test]
fn test_input_released_hook() {
    let mut ir = create_simple_jump_ability();
    ir.lifecycle_hooks.input_released = Some(FunctionIR {
        name: "input_released".to_string(),
        body: "// Input logic".to_string(),
    });
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // Header declaration
    assert!(output.header.contains("virtual void InputReleased"));
    
    // Source implementation
    assert!(output.source.contains("void UJumpAbility::InputReleased"));
    assert!(output.source.contains("Super::InputReleased"));
    assert!(output.source.contains("// TODO: User-defined input released logic"));
}

#[test]
fn test_all_lifecycle_hooks() {
    let mut ir = create_simple_jump_ability();
    ir.lifecycle_hooks = AbilityLifecycleHooksIR {
        can_activate_ability: Some(FunctionIR {
            name: "can_activate_ability".to_string(),
            body: "".to_string(),
        }),
        activate_ability: Some(FunctionIR {
            name: "activate_ability".to_string(),
            body: "".to_string(),
        }),
        end_ability: Some(FunctionIR {
            name: "end_ability".to_string(),
            body: "".to_string(),
        }),
        cancel_ability: None,
        commit_ability: None,
        input_pressed: Some(FunctionIR {
            name: "input_pressed".to_string(),
            body: "".to_string(),
        }),
        input_released: Some(FunctionIR {
            name: "input_released".to_string(),
            body: "".to_string(),
        }),
    };
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // All hooks should be declared
    assert!(output.header.contains("virtual bool CanActivateAbility"));
    assert!(output.header.contains("virtual void ActivateAbility"));
    assert!(output.header.contains("virtual void EndAbility"));
    assert!(output.header.contains("virtual void InputPressed"));
    assert!(output.header.contains("virtual void InputReleased"));
    
    // All hooks should be implemented
    assert!(output.source.contains("bool UJumpAbility::CanActivateAbility"));
    assert!(output.source.contains("void UJumpAbility::ActivateAbility"));
    assert!(output.source.contains("void UJumpAbility::EndAbility"));
    assert!(output.source.contains("void UJumpAbility::InputPressed"));
    assert!(output.source.contains("void UJumpAbility::InputReleased"));
}

#[test]
fn test_different_policies() {
    let mut ir = create_simple_jump_ability();
    ir.instancing_policy = InstancingPolicy::InstancedPerActor;
    ir.replication_policy = ReplicationPolicy::ReplicateNo;
    ir.net_execution_policy = NetExecutionPolicy::ServerOnly;
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor"));
    assert!(output.source.contains("ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo"));
    assert!(output.source.contains("NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly"));
}

#[test]
fn test_non_instanced_policy() {
    let mut ir = create_simple_jump_ability();
    ir.instancing_policy = InstancingPolicy::NonInstanced;
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("InstancingPolicy = EGameplayAbilityInstancingPolicy::NonInstanced"));
}

#[test]
fn test_local_only_execution() {
    let mut ir = create_simple_jump_ability();
    ir.net_execution_policy = NetExecutionPolicy::LocalOnly;
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly"));
}

#[test]
fn test_compression_ratio() {
    let mut ir = create_simple_jump_ability();
    ir.lifecycle_hooks.activate_ability = Some(FunctionIR {
        name: "activate_ability".to_string(),
        body: "".to_string(),
    });
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    let total_lines = output.header.lines().count() + output.source.lines().count();
    
    // Simple ability with 1 lifecycle hook should generate at least 60 lines
    assert!(total_lines > 60, "Generated {} lines, expected > 60", total_lines);
    
    println!("Compression ratio: 1 ability → {} C++ lines (1:{})", total_lines, total_lines);
}

#[test]
fn test_full_output_structure() {
    let ir = create_simple_jump_ability();
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // Verify header structure
    assert!(output.header.starts_with("#pragma once"));
    assert!(output.header.contains("class UJumpAbility : public UGameplayAbility"));
    assert!(output.header.contains("public:"));
    assert!(output.header.contains("protected:"));
    assert!(output.header.ends_with("};\n"));
    
    // Verify source structure
    assert!(output.source.contains("#include \"JumpAbility.h\""));
    assert!(output.source.contains("UJumpAbility::UJumpAbility()"));
}

#[test]
fn test_complex_ability_with_all_features() {
    let ir = GameplayAbilityIR {
        name: "ComplexAbility".to_string(),
        instancing_policy: InstancingPolicy::InstancedPerExecution,
        replication_policy: ReplicationPolicy::ReplicateYes,
        net_execution_policy: NetExecutionPolicy::LocalPredicted,
        ability_tags: vec!["Ability.Complex".to_string(), "Ability.Test".to_string()],
        activation_required_tags: vec!["Status.Ready".to_string()],
        activation_blocked_tags: vec!["Status.Stunned".to_string(), "Status.Dead".to_string()],
        activation_owned_tags: vec!["Status.Active".to_string()],
        cancel_abilities_with_tag: vec!["Ability.Conflicting".to_string()],
        block_abilities_with_tag: vec!["Ability.Movement".to_string()],
        cost_effect: Some("ManaCostEffect".to_string()),
        cooldown_effect: Some("ComplexCooldownEffect".to_string()),
        lifecycle_hooks: AbilityLifecycleHooksIR {
            can_activate_ability: Some(FunctionIR {
                name: "can_activate_ability".to_string(),
                body: "".to_string(),
            }),
            activate_ability: Some(FunctionIR {
                name: "activate_ability".to_string(),
                body: "".to_string(),
            }),
            end_ability: Some(FunctionIR {
                name: "end_ability".to_string(),
                body: "".to_string(),
            }),
            cancel_ability: None,
            commit_ability: None,
            input_pressed: None,
            input_released: None,
        },
    };
    
    let output = generate_ability(&ir, "TestPlugin").unwrap();
    
    // Verify all features are present
    assert!(output.source.contains("Ability.Complex"));
    assert!(output.source.contains("Ability.Test"));
    assert!(output.source.contains("Status.Ready"));
    assert!(output.source.contains("Status.Stunned"));
    assert!(output.source.contains("Status.Dead"));
    assert!(output.source.contains("Status.Active"));
    assert!(output.source.contains("Ability.Conflicting"));
    assert!(output.source.contains("Ability.Movement"));
    assert!(output.source.contains("UManaCostEffect::StaticClass()"));
    assert!(output.source.contains("UComplexCooldownEffect::StaticClass()"));
    
    // Verify lifecycle hooks
    assert!(output.header.contains("virtual bool CanActivateAbility"));
    assert!(output.header.contains("virtual void ActivateAbility"));
    assert!(output.header.contains("virtual void EndAbility"));
    assert!(!output.header.contains("virtual void InputPressed"));
    assert!(!output.header.contains("virtual void InputReleased"));
    
    let total_lines = output.header.lines().count() + output.source.lines().count();
    println!("Complex ability compression: {} C++ lines", total_lines);
    
    // Complex ability should generate 100+ lines
    assert!(total_lines > 100, "Generated {} lines, expected > 100", total_lines);
}
