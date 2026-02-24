// ============================================================================
// Gameplay Cue Integration Tests — Test codegen output
// ============================================================================

use ue5_gas::{
    GameplayCueIR, CueTypeIR, StateFieldIR, generate_cue,
};

// ============================================================================
// Helper Functions
// ============================================================================

fn create_empty_cue(name: &str) -> GameplayCueIR {
    GameplayCueIR {
        name: name.to_string(),
        tag: format!("GameplayCue.{}", name),
        cue_type: CueTypeIR::Static,
        auto_destroy: false,
        state_fields: vec![],
        on_execute_body: None,
        on_add_body: None,
        on_remove_body: None,
        while_active_body: None,
    }
}

fn create_simple_burn_cue() -> GameplayCueIR {
    GameplayCueIR {
        name: "BurnCue".to_string(),
        tag: "GameplayCue.Effect.Burn".to_string(),
        cue_type: CueTypeIR::Static,
        auto_destroy: false,
        state_fields: vec![],
        on_execute_body: Some("// Spawn burn particle\nUE_LOG(LogTemp, Log, TEXT(\"Burn effect!\"));".to_string()),
        on_add_body: None,
        on_remove_body: None,
        while_active_body: None,
    }
}

// ============================================================================
// Static Cue Tests
// ============================================================================

#[test]
fn test_static_cue_header_structure() {
    let cue_ir = create_simple_burn_cue();
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("#pragma once"));
    assert!(output.header.contains("class UBurnCue"));
    assert!(output.header.contains("UGameplayCueNotify_Static"));
    assert!(output.header.contains("GENERATED_BODY()"));
    assert!(output.header.contains("OnExecute_Implementation"));
}

#[test]
fn test_static_cue_source_structure() {
    let cue_ir = create_simple_burn_cue();
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("UBurnCue::UBurnCue()"));
    assert!(output.source.contains("GameplayCueTag = FGameplayTag::RequestGameplayTag"));
    assert!(output.source.contains("GameplayCue.Effect.Burn"));
    assert!(output.source.contains("OnExecute_Implementation"));
}

#[test]
fn test_static_cue_includes() {
    let cue_ir = create_empty_cue("TestCue");
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    // Header includes
    assert!(output.header.contains("#include \"CoreMinimal.h\""));
    assert!(output.header.contains("#include \"GameplayCueNotify_Static.h\""));
    assert!(output.header.contains("#include \"TestCue.generated.h\""));
    
    // Source includes
    assert!(output.source.contains("#include \"TestCue.h\""));
    assert!(output.source.contains("#include \"GameplayTags.h\""));
}

#[test]
fn test_static_cue_uclass_specifiers() {
    let cue_ir = create_empty_cue("TestCue");
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("UCLASS(MinimalAPI, BlueprintType)"));
}

// ============================================================================
// Actor Cue Tests
// ============================================================================

#[test]
fn test_actor_cue_base_class() {
    let mut cue_ir = create_empty_cue("ActorCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("class AActorCue"));
    assert!(output.header.contains("AGameplayCueNotify_Actor"));
    assert!(output.source.contains("AActorCue::AActorCue()"));
}

#[test]
fn test_actor_cue_with_auto_destroy() {
    let mut cue_ir = create_empty_cue("AutoDestroyCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    cue_ir.auto_destroy = true;
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("bAutoDestroyOnRemove = true"));
}

#[test]
fn test_actor_cue_without_auto_destroy() {
    let mut cue_ir = create_empty_cue("NoAutoDestroyCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    cue_ir.auto_destroy = false;
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(!output.source.contains("bAutoDestroyOnRemove"));
}

#[test]
fn test_actor_cue_includes() {
    let mut cue_ir = create_empty_cue("ActorCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("#include \"GameplayCueNotify_Actor.h\""));
}

// ============================================================================
// Lifecycle Method Tests
// ============================================================================

#[test]
fn test_on_execute_implementation() {
    let mut cue_ir = create_empty_cue("ExecuteCue");
    cue_ir.on_execute_body = Some("UE_LOG(LogTemp, Log, TEXT(\"Execute!\"));".to_string());
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("OnExecute_Implementation"));
    assert!(output.source.contains("void UExecuteCue::OnExecute_Implementation"));
    assert!(output.source.contains("UE_LOG(LogTemp, Log, TEXT(\"Execute!\"));"));
}

#[test]
fn test_on_add_implementation() {
    let mut cue_ir = create_empty_cue("AddCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    cue_ir.on_add_body = Some("UE_LOG(LogTemp, Log, TEXT(\"Added!\"));".to_string());
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("OnActive_Implementation"));
    assert!(output.source.contains("void AAddCue::OnActive_Implementation"));
    assert!(output.source.contains("UE_LOG(LogTemp, Log, TEXT(\"Added!\"));"));
}

#[test]
fn test_on_remove_implementation() {
    let mut cue_ir = create_empty_cue("RemoveCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    cue_ir.on_remove_body = Some("UE_LOG(LogTemp, Log, TEXT(\"Removed!\"));".to_string());
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("OnRemove_Implementation"));
    assert!(output.source.contains("void ARemoveCue::OnRemove_Implementation"));
    assert!(output.source.contains("UE_LOG(LogTemp, Log, TEXT(\"Removed!\"));"));
}

#[test]
fn test_while_active_implementation() {
    let mut cue_ir = create_empty_cue("ActiveCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    cue_ir.while_active_body = Some("UE_LOG(LogTemp, Log, TEXT(\"Active!\"));".to_string());
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("WhileActive_Implementation"));
    assert!(output.source.contains("void AActiveCue::WhileActive_Implementation"));
    assert!(output.source.contains("UE_LOG(LogTemp, Log, TEXT(\"Active!\"));"));
}

// ============================================================================
// State Field Tests
// ============================================================================

#[test]
fn test_state_field_generation() {
    let mut cue_ir = create_empty_cue("StatefulCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    cue_ir.state_fields.push(StateFieldIR {
        name: "ParticleSystem".to_string(),
        field_type: "UParticleSystemComponent*".to_string(),
    });
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("UPROPERTY(EditAnywhere, BlueprintReadWrite)"));
    assert!(output.header.contains("UParticleSystemComponent* ParticleSystem"));
}

#[test]
fn test_multiple_state_fields() {
    let mut cue_ir = create_empty_cue("MultiStateCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    cue_ir.state_fields.push(StateFieldIR {
        name: "ParticleSystem".to_string(),
        field_type: "UParticleSystemComponent*".to_string(),
    });
    cue_ir.state_fields.push(StateFieldIR {
        name: "AudioComponent".to_string(),
        field_type: "UAudioComponent*".to_string(),
    });
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.header.contains("UParticleSystemComponent* ParticleSystem"));
    assert!(output.header.contains("UAudioComponent* AudioComponent"));
    
    // Count UPROPERTY macros
    let uproperty_count = output.header.matches("UPROPERTY(EditAnywhere, BlueprintReadWrite)").count();
    assert_eq!(uproperty_count, 2);
}

// ============================================================================
// Tag Tests
// ============================================================================

#[test]
fn test_tag_initialization() {
    let mut cue_ir = create_empty_cue("TagCue");
    cue_ir.tag = "GameplayCue.Effect.Fire.Burn".to_string();
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("GameplayCueTag = FGameplayTag::RequestGameplayTag"));
    assert!(output.source.contains("GameplayCue.Effect.Fire.Burn"));
}

#[test]
fn test_tag_with_special_characters() {
    let mut cue_ir = create_empty_cue("SpecialCue");
    cue_ir.tag = "GameplayCue.Status.CC.Stunned".to_string();
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("GameplayCue.Status.CC.Stunned"));
}

// ============================================================================
// Complete Cue Tests
// ============================================================================

#[test]
fn test_complete_static_cue() {
    let cue_ir = GameplayCueIR {
        name: "CompleteBurnCue".to_string(),
        tag: "GameplayCue.Effect.Burn".to_string(),
        cue_type: CueTypeIR::Static,
        auto_destroy: false,
        state_fields: vec![],
        on_execute_body: Some("// Spawn particle\n// Play sound".to_string()),
        on_add_body: None,
        on_remove_body: None,
        while_active_body: None,
    };
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    // Verify all features
    assert!(output.header.contains("class UCompleteBurnCue"));
    assert!(output.header.contains("UGameplayCueNotify_Static"));
    assert!(output.source.contains("GameplayCue.Effect.Burn"));
    assert!(output.source.contains("// Spawn particle"));
    assert!(output.source.contains("// Play sound"));
}

#[test]
fn test_complete_actor_cue() {
    let cue_ir = GameplayCueIR {
        name: "CompleteHealCue".to_string(),
        tag: "GameplayCue.Effect.Heal".to_string(),
        cue_type: CueTypeIR::Actor,
        auto_destroy: true,
        state_fields: vec![
            StateFieldIR {
                name: "ParticleSystem".to_string(),
                field_type: "UParticleSystemComponent*".to_string(),
            },
        ],
        on_execute_body: None,
        on_add_body: Some("// Spawn attached particle".to_string()),
        on_remove_body: Some("// Cleanup".to_string()),
        while_active_body: Some("// Update effect".to_string()),
    };
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    // Verify all features
    assert!(output.header.contains("class ACompleteHealCue"));
    assert!(output.header.contains("AGameplayCueNotify_Actor"));
    assert!(output.header.contains("UParticleSystemComponent* ParticleSystem"));
    assert!(output.source.contains("bAutoDestroyOnRemove = true"));
    assert!(output.source.contains("GameplayCue.Effect.Heal"));
    assert!(output.source.contains("// Spawn attached particle"));
    assert!(output.source.contains("// Cleanup"));
    assert!(output.source.contains("// Update effect"));
}

// ============================================================================
// Compression Ratio Tests
// ============================================================================

#[test]
fn test_minimal_cue_compression() {
    let cue_ir = create_empty_cue("MinimalCue");
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    let total_lines = output.header.lines().count() + output.source.lines().count();
    
    // Minimal cue should generate at least 30 lines
    assert!(total_lines > 30, "Generated {} lines, expected > 30", total_lines);
}

#[test]
fn test_complex_cue_compression() {
    let cue_ir = GameplayCueIR {
        name: "ComplexCue".to_string(),
        tag: "GameplayCue.Effect.Complex".to_string(),
        cue_type: CueTypeIR::Actor,
        auto_destroy: true,
        state_fields: vec![
            StateFieldIR {
                name: "ParticleSystem".to_string(),
                field_type: "UParticleSystemComponent*".to_string(),
            },
            StateFieldIR {
                name: "AudioComponent".to_string(),
                field_type: "UAudioComponent*".to_string(),
            },
        ],
        on_execute_body: Some("// Execute logic".to_string()),
        on_add_body: Some("// Add logic".to_string()),
        on_remove_body: Some("// Remove logic".to_string()),
        while_active_body: Some("// Active logic".to_string()),
    };
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    let total_lines = output.header.lines().count() + output.source.lines().count();
    
    println!("Complex cue compression: {} C++ lines", total_lines);
    
    // Complex cue should generate 60+ lines
    assert!(total_lines > 60, "Generated {} lines, expected > 60", total_lines);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

#[test]
fn test_empty_lifecycle_body() {
    let mut cue_ir = create_empty_cue("EmptyCue");
    cue_ir.on_execute_body = Some("".to_string());
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    // Should still generate valid C++
    assert!(output.source.contains("void UEmptyCue::OnExecute_Implementation"));
}

#[test]
fn test_multiline_lifecycle_body() {
    let mut cue_ir = create_empty_cue("MultilineCue");
    cue_ir.on_execute_body = Some("// Line 1\n// Line 2\n// Line 3".to_string());
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    assert!(output.source.contains("// Line 1"));
    assert!(output.source.contains("// Line 2"));
    assert!(output.source.contains("// Line 3"));
}

#[test]
fn test_naming_conventions() {
    let cue_ir = create_empty_cue("TestCue");
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    // Static cues should use U prefix
    assert!(output.header.contains("class UTestCue"));
    assert!(output.source.contains("UTestCue::UTestCue()"));
}

#[test]
fn test_actor_naming_conventions() {
    let mut cue_ir = create_empty_cue("TestCue");
    cue_ir.cue_type = CueTypeIR::Actor;
    
    let output = generate_cue(&cue_ir, "TestPlugin").unwrap();
    
    // Actor cues should use A prefix
    assert!(output.header.contains("class ATestCue"));
    assert!(output.source.contains("ATestCue::ATestCue()"));
}
