// ============================================================================
// Gameplay Ability Codegen — Generate UE5 C++ for GAS Abilities
// ============================================================================
// Generates complete UGameplayAbility subclasses with:
// - Policy configuration (instancing, replication, net execution)
// - Tag initialization (ability tags, activation requirements, blocking)
// - Cost and cooldown effects
// - Lifecycle hook declarations and implementations
// - Proper UCLASS and UFUNCTION macros
// ============================================================================

use crate::ability_ir::{
    GameplayAbilityIR, InstancingPolicy, ReplicationPolicy, NetExecutionPolicy
};
use kain_core::error::KainResult;

/// Output structure for ability codegen
#[derive(Debug, Clone)]
pub struct AbilityOutput {
    pub header: String,
    pub source: String,
}

/// Generate complete C++ code for a gameplay ability
pub fn generate(ir: &GameplayAbilityIR, plugin_name: &str) -> KainResult<AbilityOutput> {
    let class_name = format!("U{}", ir.name);
    
    let header = generate_header(ir, &class_name, plugin_name)?;
    let source = generate_source(ir, &class_name, plugin_name)?;
    
    Ok(AbilityOutput { header, source })
}

/// Generate header file (.h)
fn generate_header(ir: &GameplayAbilityIR, class_name: &str, _plugin_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Header guard
    output.push_str("#pragma once\n\n");
    
    // Includes
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"Abilities/GameplayAbility.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", ir.name));
    
    // Class declaration
    output.push_str("UCLASS(MinimalAPI, Blueprintable)\n");
    output.push_str(&format!("class {} : public UGameplayAbility\n", class_name));
    output.push_str("{\n");
    output.push_str("\tGENERATED_BODY()\n\n");
    
    // Public section
    output.push_str("public:\n");
    output.push_str(&format!("\t{}();\n\n", class_name));
    
    // Protected section - lifecycle hooks
    output.push_str("protected:\n");
    
    // CanActivateAbility
    if ir.lifecycle_hooks.can_activate_ability.is_some() {
        output.push_str("\tvirtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,\n");
        output.push_str("\t                                 const FGameplayAbilityActorInfo* ActorInfo,\n");
        output.push_str("\t                                 const FGameplayTagContainer* SourceTags = nullptr,\n");
        output.push_str("\t                                 const FGameplayTagContainer* TargetTags = nullptr,\n");
        output.push_str("\t                                 OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;\n\n");
    }
    
    // ActivateAbility
    if ir.lifecycle_hooks.activate_ability.is_some() {
        output.push_str("\tvirtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,\n");
        output.push_str("\t                              const FGameplayAbilityActorInfo* ActorInfo,\n");
        output.push_str("\t                              const FGameplayAbilityActivationInfo ActivationInfo,\n");
        output.push_str("\t                              const FGameplayEventData* TriggerEventData) override;\n\n");
    }
    
    // EndAbility
    if ir.lifecycle_hooks.end_ability.is_some() {
        output.push_str("\tvirtual void EndAbility(const FGameplayAbilitySpecHandle Handle,\n");
        output.push_str("\t                         const FGameplayAbilityActorInfo* ActorInfo,\n");
        output.push_str("\t                         const FGameplayAbilityActivationInfo ActivationInfo,\n");
        output.push_str("\t                         bool bReplicateEndAbility,\n");
        output.push_str("\t                         bool bWasCancelled) override;\n\n");
    }
    
    // InputPressed
    if ir.lifecycle_hooks.input_pressed.is_some() {
        output.push_str("\tvirtual void InputPressed(const FGameplayAbilitySpecHandle Handle,\n");
        output.push_str("\t                           const FGameplayAbilityActorInfo* ActorInfo,\n");
        output.push_str("\t                           const FGameplayAbilityActivationInfo ActivationInfo) override;\n\n");
    }
    
    // InputReleased
    if ir.lifecycle_hooks.input_released.is_some() {
        output.push_str("\tvirtual void InputReleased(const FGameplayAbilitySpecHandle Handle,\n");
        output.push_str("\t                            const FGameplayAbilityActorInfo* ActorInfo,\n");
        output.push_str("\t                            const FGameplayAbilityActivationInfo ActivationInfo) override;\n\n");
    }
    
    // Close class
    output.push_str("};\n");
    
    Ok(output)
}

/// Generate source file (.cpp)
fn generate_source(ir: &GameplayAbilityIR, class_name: &str, _plugin_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Includes
    output.push_str(&format!("#include \"Abilities/{}.h\"\n", ir.name));
    if let Some(ref cost) = ir.cost_effect {
        output.push_str(&format!("#include \"Effects/{}.h\"\n", cost));
    }
    if let Some(ref cooldown) = ir.cooldown_effect {
        output.push_str(&format!("#include \"Effects/{}.h\"\n", cooldown));
    }
    output.push_str("#include \"GameplayTags.h\"\n\n");
    
    // Constructor
    output.push_str(&generate_constructor(ir, class_name)?);
    output.push_str("\n");
    
    // CanActivateAbility
    if ir.lifecycle_hooks.can_activate_ability.is_some() {
        output.push_str(&generate_can_activate_ability(ir, class_name)?);
        output.push_str("\n");
    }
    
    // ActivateAbility
    if ir.lifecycle_hooks.activate_ability.is_some() {
        output.push_str(&generate_activate_ability(ir, class_name)?);
        output.push_str("\n");
    }
    
    // EndAbility
    if ir.lifecycle_hooks.end_ability.is_some() {
        output.push_str(&generate_end_ability(ir, class_name)?);
        output.push_str("\n");
    }
    
    // InputPressed
    if ir.lifecycle_hooks.input_pressed.is_some() {
        output.push_str(&generate_input_pressed(ir, class_name)?);
        output.push_str("\n");
    }
    
    // InputReleased
    if ir.lifecycle_hooks.input_released.is_some() {
        output.push_str(&generate_input_released(ir, class_name)?);
        output.push_str("\n");
    }
    
    Ok(output)
}

/// Generate constructor with policy and tag initialization
fn generate_constructor(ir: &GameplayAbilityIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str(&format!("{}::{}()\n", class_name, class_name));
    output.push_str("{\n");
    
    // Set policies
    output.push_str(&format!("\tInstancingPolicy = {};\n", 
        instancing_policy_to_cpp(&ir.instancing_policy)));
    output.push_str(&format!("\tReplicationPolicy = {};\n", 
        replication_policy_to_cpp(&ir.replication_policy)));
    output.push_str(&format!("\tNetExecutionPolicy = {};\n", 
        net_execution_policy_to_cpp(&ir.net_execution_policy)));
    output.push_str("\n");
    
    // Initialize ability tags
    if !ir.ability_tags.is_empty() {
        output.push_str("\t// Ability tags\n");
        for tag in &ir.ability_tags {
            output.push_str(&format!("\tAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"{}\")));\n", tag));
        }
        output.push_str("\n");
    }
    
    // Initialize activation required tags
    if !ir.activation_required_tags.is_empty() {
        output.push_str("\t// Activation required tags\n");
        for tag in &ir.activation_required_tags {
            output.push_str(&format!("\tActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"{}\")));\n", tag));
        }
        output.push_str("\n");
    }
    
    // Initialize activation blocked tags
    if !ir.activation_blocked_tags.is_empty() {
        output.push_str("\t// Activation blocked tags\n");
        for tag in &ir.activation_blocked_tags {
            output.push_str(&format!("\tActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"{}\")));\n", tag));
        }
        output.push_str("\n");
    }
    
    // Initialize activation owned tags
    if !ir.activation_owned_tags.is_empty() {
        output.push_str("\t// Activation owned tags\n");
        for tag in &ir.activation_owned_tags {
            output.push_str(&format!("\tActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(\"{}\")));\n", tag));
        }
        output.push_str("\n");
    }
    
    // Initialize cancel abilities with tag
    if !ir.cancel_abilities_with_tag.is_empty() {
        output.push_str("\t// Cancel abilities with tag\n");
        for tag in &ir.cancel_abilities_with_tag {
            output.push_str(&format!("\tCancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName(\"{}\")));\n", tag));
        }
        output.push_str("\n");
    }
    
    // Initialize block abilities with tag
    if !ir.block_abilities_with_tag.is_empty() {
        output.push_str("\t// Block abilities with tag\n");
        for tag in &ir.block_abilities_with_tag {
            output.push_str(&format!("\tBlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName(\"{}\")));\n", tag));
        }
        output.push_str("\n");
    }
    
    // Set cost effect
    if let Some(ref cost) = ir.cost_effect {
        output.push_str("\t// Cost effect\n");
        output.push_str(&format!("\tCostGameplayEffectClass = U{}::StaticClass();\n\n", cost));
    }
    
    // Set cooldown effect
    if let Some(ref cooldown) = ir.cooldown_effect {
        output.push_str("\t// Cooldown effect\n");
        output.push_str(&format!("\tCooldownGameplayEffectClass = U{}::StaticClass();\n\n", cooldown));
    }
    
    output.push_str("}\n");
    
    Ok(output)
}

/// Generate CanActivateAbility implementation
fn generate_can_activate_ability(_ir: &GameplayAbilityIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str(&format!("bool {}::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,\n", class_name));
    output.push_str("                                     const FGameplayAbilityActorInfo* ActorInfo,\n");
    output.push_str("                                     const FGameplayTagContainer* SourceTags,\n");
    output.push_str("                                     const FGameplayTagContainer* TargetTags,\n");
    output.push_str("                                     FGameplayTagContainer* OptionalRelevantTags) const\n");
    output.push_str("{\n");
    output.push_str("\tif (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))\n");
    output.push_str("\t{\n");
    output.push_str("\t\treturn false;\n");
    output.push_str("\t}\n\n");
    output.push_str("\t// TODO: User-defined logic\n\n");
    output.push_str("\treturn true;\n");
    output.push_str("}\n");
    
    Ok(output)
}

/// Generate ActivateAbility implementation
fn generate_activate_ability(_ir: &GameplayAbilityIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str(&format!("void {}::ActivateAbility(const FGameplayAbilitySpecHandle Handle,\n", class_name));
    output.push_str("                                  const FGameplayAbilityActorInfo* ActorInfo,\n");
    output.push_str("                                  const FGameplayAbilityActivationInfo ActivationInfo,\n");
    output.push_str("                                  const FGameplayEventData* TriggerEventData)\n");
    output.push_str("{\n");
    output.push_str("\tSuper::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);\n\n");
    output.push_str("\tif (!CommitAbility(Handle, ActorInfo, ActivationInfo))\n");
    output.push_str("\t{\n");
    output.push_str("\t\tEndAbility(Handle, ActorInfo, ActivationInfo, true, true);\n");
    output.push_str("\t\treturn;\n");
    output.push_str("\t}\n\n");
    output.push_str("\t// TODO: User-defined logic\n\n");
    output.push_str("\tEndAbility(Handle, ActorInfo, ActivationInfo, true, false);\n");
    output.push_str("}\n");
    
    Ok(output)
}

/// Generate EndAbility implementation
fn generate_end_ability(_ir: &GameplayAbilityIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str(&format!("void {}::EndAbility(const FGameplayAbilitySpecHandle Handle,\n", class_name));
    output.push_str("                         const FGameplayAbilityActorInfo* ActorInfo,\n");
    output.push_str("                         const FGameplayAbilityActivationInfo ActivationInfo,\n");
    output.push_str("                         bool bReplicateEndAbility,\n");
    output.push_str("                         bool bWasCancelled)\n");
    output.push_str("{\n");
    output.push_str("\tSuper::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);\n\n");
    output.push_str("\t// TODO: User-defined cleanup logic\n");
    output.push_str("}\n");
    
    Ok(output)
}

/// Generate InputPressed implementation
fn generate_input_pressed(_ir: &GameplayAbilityIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str(&format!("void {}::InputPressed(const FGameplayAbilitySpecHandle Handle,\n", class_name));
    output.push_str("                           const FGameplayAbilityActorInfo* ActorInfo,\n");
    output.push_str("                           const FGameplayAbilityActivationInfo ActivationInfo)\n");
    output.push_str("{\n");
    output.push_str("\tSuper::InputPressed(Handle, ActorInfo, ActivationInfo);\n\n");
    output.push_str("\t// TODO: User-defined input pressed logic\n");
    output.push_str("}\n");
    
    Ok(output)
}

/// Generate InputReleased implementation
fn generate_input_released(_ir: &GameplayAbilityIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str(&format!("void {}::InputReleased(const FGameplayAbilitySpecHandle Handle,\n", class_name));
    output.push_str("                            const FGameplayAbilityActorInfo* ActorInfo,\n");
    output.push_str("                            const FGameplayAbilityActivationInfo ActivationInfo)\n");
    output.push_str("{\n");
    output.push_str("\tSuper::InputReleased(Handle, ActorInfo, ActivationInfo);\n\n");
    output.push_str("\t// TODO: User-defined input released logic\n");
    output.push_str("}\n");
    
    Ok(output)
}

// ============================================================================
// Policy Conversion Functions
// ============================================================================

fn instancing_policy_to_cpp(policy: &InstancingPolicy) -> &'static str {
    match policy {
        InstancingPolicy::InstancedPerExecution => "EGameplayAbilityInstancingPolicy::InstancedPerExecution",
        InstancingPolicy::InstancedPerActor => "EGameplayAbilityInstancingPolicy::InstancedPerActor",
        InstancingPolicy::NonInstanced => "EGameplayAbilityInstancingPolicy::NonInstanced",
    }
}

fn replication_policy_to_cpp(policy: &ReplicationPolicy) -> &'static str {
    match policy {
        ReplicationPolicy::ReplicateNo => "EGameplayAbilityReplicationPolicy::ReplicateNo",
        ReplicationPolicy::ReplicateYes => "EGameplayAbilityReplicationPolicy::ReplicateYes",
    }
}

fn net_execution_policy_to_cpp(policy: &NetExecutionPolicy) -> &'static str {
    match policy {
        NetExecutionPolicy::LocalPredicted => "EGameplayAbilityNetExecutionPolicy::LocalPredicted",
        NetExecutionPolicy::LocalOnly => "EGameplayAbilityNetExecutionPolicy::LocalOnly",
        NetExecutionPolicy::ServerInitiated => "EGameplayAbilityNetExecutionPolicy::ServerInitiated",
        NetExecutionPolicy::ServerOnly => "EGameplayAbilityNetExecutionPolicy::ServerOnly",
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_instancing_policy_conversion() {
        assert_eq!(
            instancing_policy_to_cpp(&InstancingPolicy::InstancedPerExecution),
            "EGameplayAbilityInstancingPolicy::InstancedPerExecution"
        );
        assert_eq!(
            instancing_policy_to_cpp(&InstancingPolicy::InstancedPerActor),
            "EGameplayAbilityInstancingPolicy::InstancedPerActor"
        );
        assert_eq!(
            instancing_policy_to_cpp(&InstancingPolicy::NonInstanced),
            "EGameplayAbilityInstancingPolicy::NonInstanced"
        );
    }

    #[test]
    fn test_replication_policy_conversion() {
        assert_eq!(
            replication_policy_to_cpp(&ReplicationPolicy::ReplicateNo),
            "EGameplayAbilityReplicationPolicy::ReplicateNo"
        );
        assert_eq!(
            replication_policy_to_cpp(&ReplicationPolicy::ReplicateYes),
            "EGameplayAbilityReplicationPolicy::ReplicateYes"
        );
    }

    #[test]
    fn test_net_execution_policy_conversion() {
        assert_eq!(
            net_execution_policy_to_cpp(&NetExecutionPolicy::LocalPredicted),
            "EGameplayAbilityNetExecutionPolicy::LocalPredicted"
        );
        assert_eq!(
            net_execution_policy_to_cpp(&NetExecutionPolicy::ServerOnly),
            "EGameplayAbilityNetExecutionPolicy::ServerOnly"
        );
    }
}
