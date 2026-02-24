// ============================================================================
// Target Actor Codegen — Generate UE5 C++ for GAS Target Actors
// ============================================================================

use crate::target_ir::{TargetActorIR, TraceTypeIR};
use kain_core::error::KainResult;

#[derive(Debug, Clone)]
pub struct TargetActorOutput {
    pub header: String,
    pub source: String,
}

pub fn generate(target_ir: &TargetActorIR, _plugin_name: &str) -> KainResult<TargetActorOutput> {
    let class_name = format!("A{}", target_ir.name);
    
    let header = generate_header(target_ir, &class_name)?;
    let source = generate_source(target_ir, &class_name)?;
    
    Ok(TargetActorOutput { header, source })
}

fn generate_header(target_ir: &TargetActorIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str("#pragma once\n\n");
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"Abilities/GameplayAbilityTargetActor.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", target_ir.name));
    
    output.push_str("UCLASS()\n");
    output.push_str(&format!("class {} : public AGameplayAbilityTargetActor\n", class_name));
    output.push_str("{\n");
    output.push_str("\tGENERATED_BODY()\n\n");
    
    output.push_str("public:\n");
    output.push_str(&format!("\t{}();\n\n", class_name));
    
    // Max range property
    if target_ir.max_range.is_some() {
        output.push_str("\tUPROPERTY(BlueprintReadWrite, EditAnywhere, Category = \"Targeting\")\n");
        output.push_str("\tfloat MaxRange;\n\n");
    }
    
    // Trace channel property
    if target_ir.trace_channel.is_some() {
        output.push_str("\tUPROPERTY(BlueprintReadWrite, EditAnywhere, Category = \"Targeting\")\n");
        output.push_str("\tFName TraceChannel;\n\n");
    }
    
    // Override methods
    output.push_str("\tvirtual void StartTargeting(UGameplayAbility* Ability) override;\n");
    output.push_str("\tvirtual FGameplayAbilityTargetDataHandle MakeTargetData() const override;\n");
    
    // Custom methods
    for method in &target_ir.custom_methods {
        output.push_str(&format!("\tvoid {}();\n", method.name));
    }
    
    output.push_str("};\n");
    
    Ok(output)
}

fn generate_source(target_ir: &TargetActorIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    output.push_str(&format!("#include \"{}.h\"\n", target_ir.name));
    output.push_str("#include \"AbilitySystemComponent.h\"\n\n");
    
    // Constructor
    output.push_str(&format!("{}::{}()\n", class_name, class_name));
    output.push_str("{\n");
    output.push_str("\tPrimaryActorTick.bCanEverTick = true;\n");
    
    if let Some(max_range) = target_ir.max_range {
        output.push_str(&format!("\tMaxRange = {}f;\n", max_range));
    }
    
    if let Some(ref channel) = target_ir.trace_channel {
        output.push_str(&format!("\tTraceChannel = FName(\"{}\");\n", channel));
    }
    
    output.push_str("}\n\n");
    
    // StartTargeting
    output.push_str(&format!("void {}::StartTargeting(UGameplayAbility* Ability)\n", class_name));
    output.push_str("{\n");
    output.push_str("\tSuper::StartTargeting(Ability);\n");
    output.push_str("\t// TODO: Initialize targeting\n");
    output.push_str("}\n\n");
    
    // MakeTargetData
    output.push_str(&format!("FGameplayAbilityTargetDataHandle {}::MakeTargetData() const\n", class_name));
    output.push_str("{\n");
    output.push_str("\tFGameplayAbilityTargetDataHandle Handle;\n");
    output.push_str("\t// TODO: Perform trace and create target data\n");
    output.push_str("\treturn Handle;\n");
    output.push_str("}\n\n");
    
    // Custom methods
    for method in &target_ir.custom_methods {
        output.push_str(&format!("void {}::{}()\n", class_name, method.name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", method.body));
        output.push_str("}\n\n");
    }
    
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::target_ir::{TargetActorIR, TraceTypeIR};

    fn create_simple_target() -> TargetActorIR {
        TargetActorIR {
            name: "TestTarget".to_string(),
            trace_type: TraceTypeIR::Line,
            max_range: Some(1000.0),
            trace_channel: Some("Visibility".to_string()),
            filter: None,
            reticle_class: None,
            custom_methods: vec![],
        }
    }

    #[test]
    fn test_target_generation() {
        let target_ir = create_simple_target();
        let output = generate(&target_ir, "TestPlugin").unwrap();
        
        assert!(output.header.contains("class ATestTarget"));
        assert!(output.header.contains("AGameplayAbilityTargetActor"));
        assert!(output.source.contains("MaxRange = 1000"));
    }
}
