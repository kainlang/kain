// ============================================================================
// Attribute Set Codegen — Generate UE5 C++ for GAS Attribute Sets
// ============================================================================
// Generates complete UAttributeSet subclasses with:
// - ATTRIBUTE_ACCESSORS macros
// - Replication (GetLifetimeReplicatedProps, DOREPLIFETIME_CONDITION_NOTIFY)
// - RepNotify functions (GAMEPLAYATTRIBUTE_REPNOTIFY)
// - Lifecycle hooks (PreAttributeChange, PostGameplayEffectExecute, etc.)
// - Meta attribute handling
// - Delegates for attribute events
// ============================================================================

use crate::attribute_set_ir::{AttributeIR, AttributeSetIR, DelegateIR};
use kain_core::error::KainResult;

/// Output structure for attribute set codegen
#[derive(Debug, Clone)]
pub struct AttributeSetOutput {
    pub header: String,
    pub source: String,
}

/// Generate complete C++ code for an attribute set
pub fn generate(ir: &AttributeSetIR, plugin_name: &str) -> KainResult<AttributeSetOutput> {
    let class_name = format!("U{}", ir.name);

    let header = generate_header(ir, &class_name, plugin_name)?;
    let source = generate_source(ir, &class_name, plugin_name)?;

    Ok(AttributeSetOutput { header, source })
}

/// Generate header file (.h)
fn generate_header(
    ir: &AttributeSetIR,
    class_name: &str,
    _plugin_name: &str,
) -> KainResult<String> {
    let mut output = String::new();

    // Header guard
    output.push_str(&format!("#pragma once\n\n"));

    // Includes
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"AttributeSet.h\"\n");
    output.push_str("#include \"AbilitySystemComponent.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", ir.name));

    // Delegate declarations (before class)
    for delegate in &ir.delegates {
        output.push_str(&generate_delegate_declaration(&delegate)?);
    }

    if !ir.delegates.is_empty() {
        output.push_str("\n");
    }

    // Class declaration
    output.push_str(&format!("UCLASS(MinimalAPI, BlueprintType)\n"));
    output.push_str(&format!("class {} : public UAttributeSet\n", class_name));
    output.push_str("{\n");
    output.push_str("\tGENERATED_BODY()\n\n");

    // Public section
    output.push_str("public:\n");
    output.push_str(&format!("\t{}();\n\n", class_name));

    // ATTRIBUTE_ACCESSORS macros
    for attr in &ir.attributes {
        output.push_str(&format!(
            "\tATTRIBUTE_ACCESSORS({}, {});\n",
            class_name,
            capitalize_first(&attr.name)
        ));
    }
    output.push_str("\n");

    // GetLifetimeReplicatedProps (if any replicated attributes)
    if ir.attributes.iter().any(|a| a.replicated) {
        output.push_str("\tvirtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;\n\n");
    }

    // Delegates
    if !ir.delegates.is_empty() {
        output.push_str("\t// Delegates\n");
        for delegate in &ir.delegates {
            output.push_str(&format!(
                "\t{} {};\n",
                delegate.delegate_type,
                capitalize_first(&delegate.name)
            ));
        }
        output.push_str("\n");
    }

    // Protected section
    output.push_str("protected:\n");

    // RepNotify functions
    for attr in &ir.attributes {
        if attr.rep_notify {
            output.push_str(&format!("\tUFUNCTION()\n"));
            output.push_str(&format!(
                "\tvoid OnRep_{}(const FGameplayAttributeData& OldValue);\n\n",
                capitalize_first(&attr.name)
            ));
        }
    }

    // Lifecycle hooks
    if ir.lifecycle_hooks.pre_gameplay_effect_execute.is_some() {
        output.push_str("\tvirtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;\n");
    }
    if ir.lifecycle_hooks.post_gameplay_effect_execute.is_some() {
        output.push_str("\tvirtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;\n");
    }
    if ir.lifecycle_hooks.pre_attribute_change.is_some() {
        output.push_str("\tvirtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;\n");
    }
    if ir.lifecycle_hooks.post_attribute_change.is_some() {
        output.push_str("\tvirtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;\n");
    }

    if ir.lifecycle_hooks.pre_gameplay_effect_execute.is_some()
        || ir.lifecycle_hooks.post_gameplay_effect_execute.is_some()
        || ir.lifecycle_hooks.pre_attribute_change.is_some()
        || ir.lifecycle_hooks.post_attribute_change.is_some()
    {
        output.push_str("\n");
    }

    // Private section
    output.push_str("private:\n");

    // Attribute properties
    for attr in &ir.attributes {
        output.push_str(&generate_attribute_property(attr, class_name)?);
    }

    // Close class
    output.push_str("};\n");

    Ok(output)
}

/// Generate source file (.cpp)
fn generate_source(
    ir: &AttributeSetIR,
    class_name: &str,
    _plugin_name: &str,
) -> KainResult<String> {
    let mut output = String::new();

    // Includes
    output.push_str(&format!("#include \"{}.h\"\n", ir.name));
    output.push_str("#include \"Net/UnrealNetwork.h\"\n");
    output.push_str("#include \"GameplayEffectExtension.h\"\n\n");

    // Constructor
    output.push_str(&generate_constructor(ir, class_name)?);
    output.push_str("\n");

    // GetLifetimeReplicatedProps
    if ir.attributes.iter().any(|a| a.replicated) {
        output.push_str(&generate_replication_function(ir, class_name)?);
        output.push_str("\n");
    }

    // RepNotify functions
    for attr in &ir.attributes {
        if attr.rep_notify {
            output.push_str(&generate_rep_notify_function(attr, class_name)?);
            output.push_str("\n");
        }
    }

    // Lifecycle hooks
    if let Some(ref hook) = ir.lifecycle_hooks.pre_gameplay_effect_execute {
        output.push_str(&generate_pre_gameplay_effect_execute(ir, class_name, hook)?);
        output.push_str("\n");
    }

    if let Some(ref hook) = ir.lifecycle_hooks.post_gameplay_effect_execute {
        output.push_str(&generate_post_gameplay_effect_execute(
            ir, class_name, hook,
        )?);
        output.push_str("\n");
    }

    if let Some(ref hook) = ir.lifecycle_hooks.pre_attribute_change {
        output.push_str(&generate_pre_attribute_change(ir, class_name, hook)?);
        output.push_str("\n");
    }

    if let Some(ref hook) = ir.lifecycle_hooks.post_attribute_change {
        output.push_str(&generate_post_attribute_change(ir, class_name, hook)?);
        output.push_str("\n");
    }

    Ok(output)
}

/// Generate delegate declaration
fn generate_delegate_declaration(delegate: &DelegateIR) -> KainResult<String> {
    // For now, assume AttributeEvent is a standard delegate type
    // In a full implementation, we'd parse the delegate type and generate the appropriate macro
    Ok(format!(
        "DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams({}, AActor*, Instigator, AActor*, Causer, const FGameplayEffectSpec&, EffectSpec, float, Magnitude, float, OldValue, float, NewValue);\n",
        delegate.delegate_type
    ))
}

/// Generate UPROPERTY for an attribute
fn generate_attribute_property(attr: &AttributeIR, _class_name: &str) -> KainResult<String> {
    let mut output = String::new();

    // Build UPROPERTY specifiers
    let mut specifiers = vec!["BlueprintReadOnly".to_string()];

    if attr.replicated && attr.rep_notify {
        specifiers.push(format!(
            "ReplicatedUsing = OnRep_{}",
            capitalize_first(&attr.name)
        ));
    }

    specifiers.push(format!("Category = \"{}\"", attr.category));

    // Build Meta specifiers
    let mut meta_specs = vec!["AllowPrivateAccess = true".to_string()];
    if attr.hide_from_modifiers {
        meta_specs.push("HideFromModifiers".to_string());
    }

    specifiers.push(format!("Meta = ({})", meta_specs.join(", ")));

    output.push_str(&format!("\tUPROPERTY({})\n", specifiers.join(", ")));
    output.push_str(&format!(
        "\tFGameplayAttributeData {};\n\n",
        capitalize_first(&attr.name)
    ));

    Ok(output)
}

/// Generate constructor
fn generate_constructor(ir: &AttributeSetIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();

    output.push_str(&format!("{}::{}()\n", class_name, class_name));
    output.push_str("{\n");

    // Initialize attributes with default values
    for attr in &ir.attributes {
        if let Some(ref default_val) = attr.default_value {
            output.push_str(&format!(
                "\t{} = {}f;\n",
                capitalize_first(&attr.name),
                default_val
            ));
        }
    }

    output.push_str("}\n");

    Ok(output)
}

/// Generate GetLifetimeReplicatedProps
fn generate_replication_function(ir: &AttributeSetIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();

    output.push_str(&format!(
        "void {}::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const\n",
        class_name
    ));
    output.push_str("{\n");
    output.push_str("\tSuper::GetLifetimeReplicatedProps(OutLifetimeProps);\n\n");

    for attr in &ir.attributes {
        if attr.replicated {
            if attr.rep_notify {
                output.push_str(&format!(
                    "\tDOREPLIFETIME_CONDITION_NOTIFY({}, {}, COND_None, REPNOTIFY_Always);\n",
                    class_name,
                    capitalize_first(&attr.name)
                ));
            } else {
                output.push_str(&format!(
                    "\tDOREPLIFETIME({}, {});\n",
                    class_name,
                    capitalize_first(&attr.name)
                ));
            }
        }
    }

    output.push_str("}\n");

    Ok(output)
}

/// Generate RepNotify function
fn generate_rep_notify_function(attr: &AttributeIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    let attr_name = capitalize_first(&attr.name);

    output.push_str(&format!(
        "void {}::OnRep_{}(const FGameplayAttributeData& OldValue)\n",
        class_name, attr_name
    ));
    output.push_str("{\n");
    output.push_str(&format!(
        "\tGAMEPLAYATTRIBUTE_REPNOTIFY({}, {}, OldValue);\n",
        class_name, attr_name
    ));
    output.push_str("}\n");

    Ok(output)
}

/// Generate PreGameplayEffectExecute
fn generate_pre_gameplay_effect_execute(
    _ir: &AttributeSetIR,
    class_name: &str,
    _hook: &crate::attribute_set_ir::FunctionIR,
) -> KainResult<String> {
    let mut output = String::new();

    output.push_str(&format!(
        "bool {}::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)\n",
        class_name
    ));
    output.push_str("{\n");
    output.push_str("\tif (!Super::PreGameplayEffectExecute(Data))\n");
    output.push_str("\t{\n");
    output.push_str("\t\treturn false;\n");
    output.push_str("\t}\n\n");
    output.push_str("\t// TODO: Implement PreGameplayEffectExecute logic\n");
    output.push_str("\t// Generated from KAIN lifecycle hook\n\n");
    output.push_str("\treturn true;\n");
    output.push_str("}\n");

    Ok(output)
}

/// Generate PostGameplayEffectExecute
fn generate_post_gameplay_effect_execute(
    ir: &AttributeSetIR,
    class_name: &str,
    _hook: &crate::attribute_set_ir::FunctionIR,
) -> KainResult<String> {
    let mut output = String::new();

    output.push_str(&format!(
        "void {}::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)\n",
        class_name
    ));
    output.push_str("{\n");
    output.push_str("\tSuper::PostGameplayEffectExecute(Data);\n\n");

    // Generate meta attribute handling
    let meta_attrs: Vec<_> = ir.attributes.iter().filter(|a| a.is_meta).collect();

    if !meta_attrs.is_empty() {
        for attr in meta_attrs {
            let attr_name = capitalize_first(&attr.name);
            output.push_str(&format!(
                "\tif (Data.EvaluatedData.Attribute == Get{}Attribute())\n",
                attr_name
            ));
            output.push_str("\t{\n");
            output.push_str(&format!("\t\t// Handle {} meta attribute\n", attr.name));
            output.push_str(&format!(
                "\t\tconst float {}Value = Get{}();\n",
                attr.name, attr_name
            ));
            output.push_str(&format!(
                "\t\tSet{}(0.0f); // Reset meta attribute\n",
                attr_name
            ));
            output.push_str("\t\t// TODO: Apply effect based on meta attribute value\n");
            output.push_str("\t}\n\n");
        }
    }

    output.push_str("}\n");

    Ok(output)
}

/// Generate PreAttributeChange
fn generate_pre_attribute_change(
    ir: &AttributeSetIR,
    class_name: &str,
    _hook: &crate::attribute_set_ir::FunctionIR,
) -> KainResult<String> {
    let mut output = String::new();

    output.push_str(&format!(
        "void {}::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)\n",
        class_name
    ));
    output.push_str("{\n");
    output.push_str("\tSuper::PreAttributeChange(Attribute, NewValue);\n\n");

    // Generate clamping logic for each attribute
    for attr in &ir.attributes {
        if attr.clamp_min.is_some() || attr.clamp_max.is_some() {
            let attr_name = capitalize_first(&attr.name);
            output.push_str(&format!(
                "\tif (Attribute == Get{}Attribute())\n",
                attr_name
            ));
            output.push_str("\t{\n");

            if let (Some(min), Some(max)) = (attr.clamp_min, attr.clamp_max) {
                output.push_str(&format!(
                    "\t\tNewValue = FMath::Clamp(NewValue, {}f, {}f);\n",
                    min, max
                ));
            } else if let Some(min) = attr.clamp_min {
                output.push_str(&format!("\t\tNewValue = FMath::Max(NewValue, {}f);\n", min));
            } else if let Some(max) = attr.clamp_max {
                output.push_str(&format!("\t\tNewValue = FMath::Min(NewValue, {}f);\n", max));
            }

            output.push_str("\t}\n");
        }
    }

    output.push_str("}\n");

    Ok(output)
}

/// Generate PostAttributeChange
fn generate_post_attribute_change(
    _ir: &AttributeSetIR,
    class_name: &str,
    _hook: &crate::attribute_set_ir::FunctionIR,
) -> KainResult<String> {
    let mut output = String::new();

    output.push_str(&format!("void {}::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)\n", class_name));
    output.push_str("{\n");
    output.push_str("\tSuper::PostAttributeChange(Attribute, OldValue, NewValue);\n\n");
    output.push_str("\t// TODO: Implement PostAttributeChange logic\n");
    output.push_str("\t// Generated from KAIN lifecycle hook\n");
    output.push_str("}\n");

    Ok(output)
}

/// Capitalize first letter of a string and convert snake_case to PascalCase
fn capitalize_first(s: &str) -> String {
    // Convert snake_case to PascalCase
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
    fn test_capitalize_first() {
        assert_eq!(capitalize_first("health"), "Health");
        assert_eq!(capitalize_first("max_health"), "MaxHealth");
        assert_eq!(capitalize_first(""), "");
    }
}
