// ============================================================================
// Ability Task Codegen — Generate UE5 C++ for GAS Tasks
// ============================================================================
// Generates complete UAbilityTask subclasses with delegates, state fields,
// and lifecycle methods.
// ============================================================================

use crate::task_ir::{AbilityTaskIR, DelegateIR, DelegateTypeIR};
use kain_core::error::KainResult;

/// Output structure for task codegen
#[derive(Debug, Clone)]
pub struct AbilityTaskOutput {
    pub header: String,
    pub source: String,
}

/// Generate complete C++ code for an ability task
pub fn generate(task_ir: &AbilityTaskIR, plugin_name: &str) -> KainResult<AbilityTaskOutput> {
    let class_name = format!("U{}", task_ir.name);
    
    let header = generate_header(task_ir, &class_name)?;
    let source = generate_source(task_ir, &class_name)?;
    
    Ok(AbilityTaskOutput { header, source })
}

/// Generate task header
fn generate_header(task_ir: &AbilityTaskIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Header guard
    output.push_str("#pragma once\n\n");
    
    // Includes
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"Abilities/Tasks/AbilityTask.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", task_ir.name));
    
    // Delegate declarations
    for delegate in &task_ir.delegates {
        let delegate_macro = get_delegate_macro(&delegate.delegate_type);
        output.push_str(&format!("{}({});\n", delegate_macro, get_delegate_signature(&delegate.delegate_type)));
    }
    
    if !task_ir.delegates.is_empty() {
        output.push_str("\n");
    }
    
    // Class declaration
    output.push_str("UCLASS()\n");
    output.push_str(&format!("class {} : public UAbilityTask\n", class_name));
    output.push_str("{\n");
    output.push_str("\tGENERATED_BODY()\n\n");
    
    // Public section
    output.push_str("public:\n");
    output.push_str(&format!("\t{}();\n\n", class_name));
    
    // Delegates
    for delegate in &task_ir.delegates {
        output.push_str("\tUPROPERTY(BlueprintAssignable)\n");
        output.push_str(&format!("\t{} {};\n\n", get_delegate_type_name(&delegate.delegate_type), delegate.name));
    }
    
    // Static factory method
    output.push_str("\tUFUNCTION(BlueprintCallable, Category = \"Ability|Tasks\")\n");
    output.push_str(&format!("\tstatic {}* Create{}(UGameplayAbility* OwningAbility);\n\n", class_name, task_ir.name));
    
    // Lifecycle methods
    output.push_str("\tvirtual void Activate() override;\n");
    
    if task_ir.on_destroy_body.is_some() {
        output.push_str("\tvirtual void OnDestroy(bool bInOwnerFinished) override;\n");
    }
    
    // Custom methods
    for method in &task_ir.custom_methods {
        output.push_str(&format!("\tvoid {}();\n", method.name));
    }
    
    // Protected section
    if !task_ir.state_fields.is_empty() {
        output.push_str("\nprotected:\n");
        for field in &task_ir.state_fields {
            output.push_str("\tUPROPERTY()\n");
            output.push_str(&format!("\t{} {};\n", field.field_type, field.name));
        }
    }
    
    // Close class
    output.push_str("};\n");
    
    Ok(output)
}

/// Generate task source
fn generate_source(task_ir: &AbilityTaskIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Includes
    output.push_str(&format!("#include \"{}.h\"\n", task_ir.name));
    output.push_str("#include \"AbilitySystemComponent.h\"\n\n");
    
    // Constructor
    output.push_str(&format!("{}::{}()\n", class_name, class_name));
    output.push_str("{\n");
    output.push_str("\tbCanBeCanceled = true;\n");
    output.push_str("}\n\n");
    
    // Static factory method
    output.push_str(&format!("{}* {}::Create{}(UGameplayAbility* OwningAbility)\n", class_name, class_name, task_ir.name));
    output.push_str("{\n");
    output.push_str(&format!("\t{}* Task = NewAbilityTask<{}>(OwningAbility);\n", class_name, class_name));
    output.push_str("\treturn Task;\n");
    output.push_str("}\n\n");
    
    // Activate method
    output.push_str(&format!("void {}::Activate()\n", class_name));
    output.push_str("{\n");
    output.push_str("\tSuper::Activate();\n");
    if let Some(ref body) = task_ir.activate_body {
        output.push_str(&format!("\t{}\n", body));
    }
    output.push_str("}\n\n");
    
    // OnDestroy method
    if let Some(ref body) = task_ir.on_destroy_body {
        output.push_str(&format!("void {}::OnDestroy(bool bInOwnerFinished)\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\tSuper::OnDestroy(bInOwnerFinished);\n");
        output.push_str("}\n\n");
    }
    
    // Custom methods
    for method in &task_ir.custom_methods {
        output.push_str(&format!("void {}::{}()\n", class_name, method.name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", method.body));
        output.push_str("}\n\n");
    }
    
    Ok(output)
}

/// Get delegate macro for declaration
fn get_delegate_macro(delegate_type: &DelegateTypeIR) -> String {
    match delegate_type {
        DelegateTypeIR::AttributeChange => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam".to_string(),
        DelegateTypeIR::TaskCancelled => "DECLARE_DYNAMIC_MULTICAST_DELEGATE".to_string(),
        DelegateTypeIR::TargetDataReady => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam".to_string(),
        DelegateTypeIR::GameplayEvent => "DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams".to_string(),
        DelegateTypeIR::Custom(_) => "DECLARE_DYNAMIC_MULTICAST_DELEGATE".to_string(),
    }
}

/// Get delegate signature
fn get_delegate_signature(delegate_type: &DelegateTypeIR) -> String {
    match delegate_type {
        DelegateTypeIR::AttributeChange => "FAttributeChangeDelegate, float, NewValue".to_string(),
        DelegateTypeIR::TaskCancelled => "FTaskCancelledDelegate".to_string(),
        DelegateTypeIR::TargetDataReady => "FTargetDataDelegate, const FGameplayAbilityTargetDataHandle&, Data".to_string(),
        DelegateTypeIR::GameplayEvent => "FGameplayEventDelegate, FGameplayTag, EventTag, const FGameplayEventData*, Payload".to_string(),
        DelegateTypeIR::Custom(name) => format!("F{}Delegate", name),
    }
}

/// Get delegate type name
fn get_delegate_type_name(delegate_type: &DelegateTypeIR) -> String {
    match delegate_type {
        DelegateTypeIR::AttributeChange => "FAttributeChangeDelegate".to_string(),
        DelegateTypeIR::TaskCancelled => "FTaskCancelledDelegate".to_string(),
        DelegateTypeIR::TargetDataReady => "FTargetDataDelegate".to_string(),
        DelegateTypeIR::GameplayEvent => "FGameplayEventDelegate".to_string(),
        DelegateTypeIR::Custom(name) => format!("F{}Delegate", name),
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::task_ir::{AbilityTaskIR, DelegateIR, DelegateTypeIR};

    fn create_simple_task() -> AbilityTaskIR {
        AbilityTaskIR {
            name: "TestTask".to_string(),
            delegates: vec![
                DelegateIR {
                    name: "OnCompleted".to_string(),
                    delegate_type: DelegateTypeIR::TaskCancelled,
                }
            ],
            state_fields: vec![],
            activate_body: Some("// Test activate".to_string()),
            on_destroy_body: None,
            custom_methods: vec![],
        }
    }

    #[test]
    fn test_task_generation() {
        let task_ir = create_simple_task();
        let output = generate(&task_ir, "TestPlugin").unwrap();
        
        assert!(output.header.contains("class UTestTask"));
        assert!(output.header.contains("UAbilityTask"));
        assert!(output.source.contains("NewAbilityTask<UTestTask>"));
    }
}
