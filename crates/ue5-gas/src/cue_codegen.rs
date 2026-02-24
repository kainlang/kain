// ============================================================================
// Gameplay Cue Codegen — Generate UE5 C++ for GAS Cues
// ============================================================================
// Generates complete UGameplayCueNotify_Static or AGameplayCueNotify_Actor
// subclasses with lifecycle methods and state fields.
// ============================================================================

use crate::cue_ir::{GameplayCueIR, CueTypeIR};
use kain_core::error::KainResult;

/// Output structure for cue codegen
#[derive(Debug, Clone)]
pub struct GameplayCueOutput {
    pub header: String,
    pub source: String,
}

/// Generate complete C++ code for a gameplay cue
pub fn generate(cue_ir: &GameplayCueIR, plugin_name: &str) -> KainResult<GameplayCueOutput> {
    match cue_ir.cue_type {
        CueTypeIR::Static => generate_static_cue(cue_ir, plugin_name),
        CueTypeIR::Actor => generate_actor_cue(cue_ir, plugin_name),
    }
}

/// Generate static cue (UGameplayCueNotify_Static)
fn generate_static_cue(cue_ir: &GameplayCueIR, _plugin_name: &str) -> KainResult<GameplayCueOutput> {
    let class_name = format!("U{}", cue_ir.name);
    
    let header = generate_static_header(cue_ir, &class_name)?;
    let source = generate_static_source(cue_ir, &class_name)?;
    
    Ok(GameplayCueOutput { header, source })
}

/// Generate actor cue (AGameplayCueNotify_Actor)
fn generate_actor_cue(cue_ir: &GameplayCueIR, _plugin_name: &str) -> KainResult<GameplayCueOutput> {
    let class_name = format!("A{}", cue_ir.name);
    
    let header = generate_actor_header(cue_ir, &class_name)?;
    let source = generate_actor_source(cue_ir, &class_name)?;
    
    Ok(GameplayCueOutput { header, source })
}

/// Generate static cue header
fn generate_static_header(cue_ir: &GameplayCueIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Header guard
    output.push_str("#pragma once\n\n");
    
    // Includes
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"GameplayCueNotify_Static.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", cue_ir.name));
    
    // Class declaration
    output.push_str("UCLASS()\n");
    output.push_str(&format!("class {} : public UGameplayCueNotify_Static\n", class_name));
    output.push_str("{\n");
    output.push_str("\tGENERATED_BODY()\n\n");
    
    // Public section
    output.push_str("public:\n");
    output.push_str(&format!("\t{}();\n\n", class_name));
    
    // Lifecycle methods
    if cue_ir.on_execute_body.is_some() {
        output.push_str("\tvirtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;\n");
    }
    
    if cue_ir.on_add_body.is_some() {
        output.push_str("\tvirtual bool OnAdd_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;\n");
    }
    
    if cue_ir.on_remove_body.is_some() {
        output.push_str("\tvirtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;\n");
    }
    
    if cue_ir.while_active_body.is_some() {
        output.push_str("\tvirtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;\n");
    }
    
    // Close class
    output.push_str("};\n");
    
    Ok(output)
}

/// Generate static cue source
fn generate_static_source(cue_ir: &GameplayCueIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Includes
    output.push_str(&format!("#include \"{}.h\"\n", cue_ir.name));
    output.push_str("#include \"GameplayTags.h\"\n\n");
    
    // Constructor
    output.push_str(&format!("{}::{}()\n", class_name, class_name));
    output.push_str("{\n");
    output.push_str(&format!("\tGameplayCueTag = FGameplayTag::RequestGameplayTag(FName(\"{}\"));\n", cue_ir.tag));
    output.push_str("}\n\n");
    
    // Lifecycle method implementations
    if let Some(ref body) = cue_ir.on_execute_body {
        output.push_str(&format!("bool {}::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\treturn true;\n");
        output.push_str("}\n\n");
    }
    
    if let Some(ref body) = cue_ir.on_add_body {
        output.push_str(&format!("bool {}::OnAdd_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\treturn true;\n");
        output.push_str("}\n\n");
    }
    
    if let Some(ref body) = cue_ir.on_remove_body {
        output.push_str(&format!("bool {}::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\treturn true;\n");
        output.push_str("}\n\n");
    }
    
    if let Some(ref body) = cue_ir.while_active_body {
        output.push_str(&format!("bool {}::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\treturn true;\n");
        output.push_str("}\n\n");
    }
    
    Ok(output)
}

/// Generate actor cue header
fn generate_actor_header(cue_ir: &GameplayCueIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Header guard
    output.push_str("#pragma once\n\n");
    
    // Includes
    output.push_str("#include \"CoreMinimal.h\"\n");
    output.push_str("#include \"GameplayCueNotify_Actor.h\"\n");
    output.push_str(&format!("#include \"{}.generated.h\"\n\n", cue_ir.name));
    
    // Class declaration
    output.push_str("UCLASS()\n");
    output.push_str(&format!("class {} : public AGameplayCueNotify_Actor\n", class_name));
    output.push_str("{\n");
    output.push_str("\tGENERATED_BODY()\n\n");
    
    // Public section
    output.push_str("public:\n");
    output.push_str(&format!("\t{}();\n\n", class_name));
    
    // State fields
    for field in &cue_ir.state_fields {
        output.push_str("\tUPROPERTY()\n");
        output.push_str(&format!("\t{} {};\n\n", field.field_type, field.name));
    }
    
    // Lifecycle methods
    if cue_ir.on_execute_body.is_some() {
        output.push_str("\tvirtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;\n");
    }
    
    if cue_ir.on_add_body.is_some() {
        output.push_str("\tvirtual bool OnAdd_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;\n");
    }
    
    if cue_ir.on_remove_body.is_some() {
        output.push_str("\tvirtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;\n");
    }
    
    if cue_ir.while_active_body.is_some() {
        output.push_str("\tvirtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;\n");
    }
    
    // Close class
    output.push_str("};\n");
    
    Ok(output)
}

/// Generate actor cue source
fn generate_actor_source(cue_ir: &GameplayCueIR, class_name: &str) -> KainResult<String> {
    let mut output = String::new();
    
    // Includes
    output.push_str(&format!("#include \"{}.h\"\n", cue_ir.name));
    output.push_str("#include \"GameplayTags.h\"\n\n");
    
    // Constructor
    output.push_str(&format!("{}::{}()\n", class_name, class_name));
    output.push_str("{\n");
    output.push_str(&format!("\tGameplayCueTag = FGameplayTag::RequestGameplayTag(FName(\"{}\"));\n", cue_ir.tag));
    
    if cue_ir.auto_destroy {
        output.push_str("\tbAutoDestroyOnRemove = true;\n");
    }
    
    output.push_str("}\n\n");
    
    // Lifecycle method implementations (same as static)
    if let Some(ref body) = cue_ir.on_execute_body {
        output.push_str(&format!("bool {}::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\treturn true;\n");
        output.push_str("}\n\n");
    }
    
    if let Some(ref body) = cue_ir.on_add_body {
        output.push_str(&format!("bool {}::OnAdd_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\treturn true;\n");
        output.push_str("}\n\n");
    }
    
    if let Some(ref body) = cue_ir.on_remove_body {
        output.push_str(&format!("bool {}::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\treturn true;\n");
        output.push_str("}\n\n");
    }
    
    if let Some(ref body) = cue_ir.while_active_body {
        output.push_str(&format!("bool {}::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)\n", class_name));
        output.push_str("{\n");
        output.push_str(&format!("\t{}\n", body));
        output.push_str("\treturn true;\n");
        output.push_str("}\n\n");
    }
    
    Ok(output)
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cue_ir::{GameplayCueIR, CueTypeIR};

    fn create_simple_static_cue() -> GameplayCueIR {
        GameplayCueIR {
            name: "TestCue".to_string(),
            tag: "GameplayCue.Test".to_string(),
            cue_type: CueTypeIR::Static,
            auto_destroy: false,
            state_fields: vec![],
            on_execute_body: Some("// Test execute".to_string()),
            on_add_body: None,
            on_remove_body: None,
            while_active_body: None,
        }
    }

    #[test]
    fn test_static_cue_generation() {
        let cue_ir = create_simple_static_cue();
        let output = generate(&cue_ir, "TestPlugin").unwrap();
        
        assert!(output.header.contains("class UTestCue"));
        assert!(output.header.contains("UGameplayCueNotify_Static"));
        assert!(output.source.contains("GameplayCueTag = FGameplayTag::RequestGameplayTag"));
    }

    #[test]
    fn test_actor_cue_generation() {
        let mut cue_ir = create_simple_static_cue();
        cue_ir.cue_type = CueTypeIR::Actor;
        cue_ir.auto_destroy = true;
        
        let output = generate(&cue_ir, "TestPlugin").unwrap();
        
        assert!(output.header.contains("class ATestCue"));
        assert!(output.header.contains("AGameplayCueNotify_Actor"));
        assert!(output.source.contains("bAutoDestroyOnRemove = true"));
    }
}
