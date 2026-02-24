// ============================================================================
// Gameplay Effect Codegen — Generate UE5 C++ for GAS Effects
// ============================================================================
// Generates complete UGameplayEffect subclasses with:
// - Duration policy and magnitude configuration
// - Period and execute on application settings
// - Modifiers (attribute, operation, magnitude)
// - Stacking configuration (type, limit)
// - Tag initialization (owned, granted, requirements)
// - Proper UCLASS macros
// ============================================================================

use crate::effect_ir::{
    GameplayEffectIR, DurationPolicy, ModifierOp, StackingType
};
use kain_core::error::KainResult;

/// Output structure for effect codegen
#[derive(Debug, Clone)]
pub struct GameplayEffectOutput {
    pub header: String,
    pub source: String,
}

/// Generate complete C++ code for a gameplay effect
pub fn generate(effect_ir: &GameplayEffectIR, plugin_name: &str) -> KainResult<GameplayEffectOutput> {
    let class_name = format!("U{}", effect_ir.name);
    
    let header = generate_header(effect_ir, &class_name, plugin_name)?;
    let source = generate_source(effect_ir, &class_name, plugin_name)?;
    
    Ok(GameplayEffectOutput { header, source })
}

/// Generate header file (.h)
fn generate_header(effect_ir: &GameplayEffectIR, class_name: &str, _plugin_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Header guard
    output.push_str("#pragma once\n\n");
    
    // Includes
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"GameplayEffect.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", effect_ir.name));
    
    // Class declaration
    output.push_str("UCLASS(MinimalAPI, BlueprintType)\n");
    output.push_str(&format!("class {} : public UGameplayEffect\n", class_name));
    output.push_str("{\n");
    output.push_str("\tGENERATED_BODY()\n\n");
    
    // Public section
    output.push_str("public:\n");
    output.push_str(&format!("\t{}();\n", class_name));
    
    // Close class
    output.push_str("};\n");
    
    Ok(output)
}

/// Generate source file (.cpp)
fn generate_source(effect_ir: &GameplayEffectIR, class_name: &str, _plugin_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Includes
    output.push_str(&format!("#include \"{}.h\"\n", effect_ir.name));
    output.push_str("#include \"GameplayTags.h\"\n\n");
    
    // Constructor
    output.push_str(&generate_constructor(effect_ir, class_name)?);
    
    Ok(output)
}

/// Generate constructor with all effect configuration
fn generate_constructor(effect_ir: &GameplayEffectIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str(&format!("{}::{}()\n", class_name, class_name));
    output.push_str("{\n");
    
    // Duration policy and magnitude
    output.push_str("\t// Duration\n");
    output.push_str(&format!("\tDurationPolicy = {};\n", 
        duration_policy_to_ue5(&effect_ir.duration_policy)));
    
    if let Some(duration) = effect_ir.duration_magnitude {
        // Format with .0 if it's a whole number
        let duration_str = if duration.fract() == 0.0 {
            format!("{:.1}", duration)
        } else {
            format!("{}", duration)
        };
        output.push_str(&format!("\tDurationMagnitude = FScalableFloat({}f);\n", duration_str));
    }
    output.push_str("\n");
    
    // Period configuration
    if let Some(period) = effect_ir.period {
        output.push_str("\t// Period\n");
        let period_str = if period.fract() == 0.0 {
            format!("{:.1}", period)
        } else {
            format!("{}", period)
        };
        output.push_str(&format!("\tPeriod = FScalableFloat({}f);\n", period_str));
        
        if effect_ir.execute_on_application {
            output.push_str("\tbExecutePeriodicEffectOnApplication = true;\n");
        }
        output.push_str("\n");
    }
    
    // Modifiers
    if !effect_ir.modifiers.is_empty() {
        output.push_str("\t// Modifiers\n");
        for modifier in &effect_ir.modifiers {
            output.push_str("\t{\n");
            output.push_str("\t\tFGameplayModifierInfo Modifier;\n");
            
            // Parse attribute (format: "AttributeSet.Attribute" or just "Attribute")
            let (attribute_set, attribute_name) = if modifier.attribute.contains('.') {
                let parts: Vec<&str> = modifier.attribute.split('.').collect();
                (parts[0].to_string(), parts[1].to_string())
            } else {
                // If no set specified, assume it's just the attribute name
                // This will need to be resolved at a higher level
                ("UnknownSet".to_string(), modifier.attribute.clone())
            };
            
            output.push_str(&format!("\t\tModifier.Attribute = U{}::Get{}Attribute();\n", 
                attribute_set, capitalize_first(&attribute_name)));
            output.push_str(&format!("\t\tModifier.ModifierOp = {};\n", 
                modifier_op_to_ue5(&modifier.operation)));
            
            // Format magnitude with .0 if it's a whole number
            let magnitude_str = if modifier.magnitude.fract() == 0.0 {
                format!("{:.1}", modifier.magnitude)
            } else {
                format!("{}", modifier.magnitude)
            };
            output.push_str(&format!("\t\tModifier.ModifierMagnitude = FScalableFloat({}f);\n", 
                magnitude_str));
            output.push_str("\t\tModifiers.Add(Modifier);\n");
            output.push_str("\t}\n");
        }
        output.push_str("\n");
    }
    
    // Stacking configuration
    if let Some(ref stacking) = effect_ir.stacking {
        output.push_str("\t// Stacking\n");
        output.push_str(&format!("\tStackingType = {};\n", 
            stacking_type_to_ue5(&stacking.stacking_type)));
        output.push_str(&format!("\tStackLimitCount = {};\n", stacking.limit));
        output.push_str("\n");
    }
    
    // Owned tags
    if !effect_ir.owned_tags.is_empty() {
        output.push_str("\t// Owned tags\n");
        for tag in &effect_ir.owned_tags {
            output.push_str(&format!("\tInheritableOwnedTagsContainer.AddTag(\n"));
            output.push_str(&format!("\t\tFGameplayTag::RequestGameplayTag(FName(\"{}\"))\n", tag));
            output.push_str("\t);\n");
        }
        output.push_str("\n");
    }
    
    // Granted tags
    if !effect_ir.granted_tags.is_empty() {
        output.push_str("\t// Granted tags\n");
        for tag in &effect_ir.granted_tags {
            output.push_str(&format!("\tInheritableGameplayEffectTags.Added.AddTag(\n"));
            output.push_str(&format!("\t\tFGameplayTag::RequestGameplayTag(FName(\"{}\"))\n", tag));
            output.push_str("\t);\n");
        }
        output.push_str("\n");
    }
    
    // Application tag requirements
    if !effect_ir.application_tag_requirements.require.is_empty() 
        || !effect_ir.application_tag_requirements.ignore.is_empty() {
        output.push_str("\t// Application requirements\n");
        
        for tag in &effect_ir.application_tag_requirements.require {
            output.push_str(&format!("\tApplicationTagRequirements.RequireTags.AddTag(\n"));
            output.push_str(&format!("\t\tFGameplayTag::RequestGameplayTag(FName(\"{}\"))\n", tag));
            output.push_str("\t);\n");
        }
        
        for tag in &effect_ir.application_tag_requirements.ignore {
            output.push_str(&format!("\tApplicationTagRequirements.IgnoreTags.AddTag(\n"));
            output.push_str(&format!("\t\tFGameplayTag::RequestGameplayTag(FName(\"{}\"))\n", tag));
            output.push_str("\t);\n");
        }
        output.push_str("\n");
    }
    
    // Ongoing tag requirements
    if !effect_ir.ongoing_tag_requirements.require.is_empty() 
        || !effect_ir.ongoing_tag_requirements.ignore.is_empty() {
        output.push_str("\t// Ongoing requirements\n");
        
        for tag in &effect_ir.ongoing_tag_requirements.require {
            output.push_str(&format!("\tOngoingTagRequirements.RequireTags.AddTag(\n"));
            output.push_str(&format!("\t\tFGameplayTag::RequestGameplayTag(FName(\"{}\"))\n", tag));
            output.push_str("\t);\n");
        }
        
        for tag in &effect_ir.ongoing_tag_requirements.ignore {
            output.push_str(&format!("\tOngoingTagRequirements.IgnoreTags.AddTag(\n"));
            output.push_str(&format!("\t\tFGameplayTag::RequestGameplayTag(FName(\"{}\"))\n", tag));
            output.push_str("\t);\n");
        }
        output.push_str("\n");
    }
    
    // Removal tag requirements
    if !effect_ir.removal_tag_requirements.require.is_empty() 
        || !effect_ir.removal_tag_requirements.ignore.is_empty() {
        output.push_str("\t// Removal requirements\n");
        
        for tag in &effect_ir.removal_tag_requirements.require {
            output.push_str(&format!("\tRemovalTagRequirements.RequireTags.AddTag(\n"));
            output.push_str(&format!("\t\tFGameplayTag::RequestGameplayTag(FName(\"{}\"))\n", tag));
            output.push_str("\t);\n");
        }
        
        for tag in &effect_ir.removal_tag_requirements.ignore {
            output.push_str(&format!("\tRemovalTagRequirements.IgnoreTags.AddTag(\n"));
            output.push_str(&format!("\t\tFGameplayTag::RequestGameplayTag(FName(\"{}\"))\n", tag));
            output.push_str("\t);\n");
        }
        output.push_str("\n");
    }
    
    output.push_str("}\n");
    
    Ok(output)
}

// ============================================================================
// Conversion Functions
// ============================================================================

fn duration_policy_to_ue5(policy: &DurationPolicy) -> &'static str {
    match policy {
        DurationPolicy::Instant => "EGameplayEffectDurationType::Instant",
        DurationPolicy::Infinite => "EGameplayEffectDurationType::Infinite",
        DurationPolicy::HasDuration => "EGameplayEffectDurationType::HasDuration",
    }
}

fn modifier_op_to_ue5(op: &ModifierOp) -> &'static str {
    match op {
        ModifierOp::Add => "EGameplayModOp::Additive",
        ModifierOp::Multiply => "EGameplayModOp::Multiplicative",
        ModifierOp::Divide => "EGameplayModOp::Division",
        ModifierOp::Override => "EGameplayModOp::Override",
    }
}

fn stacking_type_to_ue5(stacking: &StackingType) -> &'static str {
    match stacking {
        StackingType::None => "EGameplayEffectStackingType::None",
        StackingType::AggregateBySource => "EGameplayEffectStackingType::AggregateBySource",
        StackingType::AggregateByTarget => "EGameplayEffectStackingType::AggregateByTarget",
    }
}

/// Capitalize first letter of a string and convert snake_case to PascalCase
fn capitalize_first(s: &str) -> String {
    s.split('_')
        .map(|word| {
            let mut chars = word.chars();
            match chars.next() {
                None => String::new(),
                Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
            }
        })
        .collect()
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_duration_policy_conversion() {
        assert_eq!(
            duration_policy_to_ue5(&DurationPolicy::Instant),
            "EGameplayEffectDurationType::Instant"
        );
        assert_eq!(
            duration_policy_to_ue5(&DurationPolicy::Infinite),
            "EGameplayEffectDurationType::Infinite"
        );
        assert_eq!(
            duration_policy_to_ue5(&DurationPolicy::HasDuration),
            "EGameplayEffectDurationType::HasDuration"
        );
    }

    #[test]
    fn test_modifier_op_conversion() {
        assert_eq!(
            modifier_op_to_ue5(&ModifierOp::Add),
            "EGameplayModOp::Additive"
        );
        assert_eq!(
            modifier_op_to_ue5(&ModifierOp::Multiply),
            "EGameplayModOp::Multiplicative"
        );
        assert_eq!(
            modifier_op_to_ue5(&ModifierOp::Divide),
            "EGameplayModOp::Division"
        );
        assert_eq!(
            modifier_op_to_ue5(&ModifierOp::Override),
            "EGameplayModOp::Override"
        );
    }

    #[test]
    fn test_stacking_type_conversion() {
        assert_eq!(
            stacking_type_to_ue5(&StackingType::None),
            "EGameplayEffectStackingType::None"
        );
        assert_eq!(
            stacking_type_to_ue5(&StackingType::AggregateBySource),
            "EGameplayEffectStackingType::AggregateBySource"
        );
        assert_eq!(
            stacking_type_to_ue5(&StackingType::AggregateByTarget),
            "EGameplayEffectStackingType::AggregateByTarget"
        );
    }

    #[test]
    fn test_capitalize_first() {
        assert_eq!(capitalize_first("health"), "Health");
        assert_eq!(capitalize_first("max_health"), "MaxHealth");
        assert_eq!(capitalize_first("damage_per_tick"), "DamagePerTick");
        assert_eq!(capitalize_first(""), "");
    }
}
