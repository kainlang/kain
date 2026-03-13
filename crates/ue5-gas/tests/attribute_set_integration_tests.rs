// ============================================================================
// Attribute Set Integration Tests — Test codegen output
// ============================================================================

use kain_core::ast::Type;
use kain_core::span::Span;
use ue5_gas::{generate_attribute_set, AttributeIR, AttributeSetIR, LifecycleHooksIR};

// ============================================================================
// Helper Functions
// ============================================================================

fn create_simple_health_set() -> AttributeSetIR {
    AttributeSetIR {
        name: "HealthSet".to_string(),
        attributes: vec![
            AttributeIR {
                name: "health".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                default_value: Some("100.0".to_string()),
                replicated: true,
                rep_notify: true,
                hide_from_modifiers: true,
                is_meta: false,
                clamp_min: None,
                clamp_max: None,
                category: "Health".to_string(),
            },
            AttributeIR {
                name: "max_health".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                default_value: Some("100.0".to_string()),
                replicated: true,
                rep_notify: true,
                hide_from_modifiers: false,
                is_meta: false,
                clamp_min: None,
                clamp_max: None,
                category: "Health".to_string(),
            },
        ],
        lifecycle_hooks: LifecycleHooksIR::default(),
        delegates: vec![],
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

#[test]
fn test_attribute_accessors_generation() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    assert!(output
        .header
        .contains("ATTRIBUTE_ACCESSORS(UHealthSet, Health)"));
    assert!(output
        .header
        .contains("ATTRIBUTE_ACCESSORS(UHealthSet, MaxHealth)"));
}

#[test]
fn test_class_declaration() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    assert!(output
        .header
        .contains("class UHealthSet : public UAttributeSet"));
    assert!(output.header.contains("GENERATED_BODY()"));
    assert!(output.header.contains("UCLASS(MinimalAPI, BlueprintType)"));
}

#[test]
fn test_replication_setup() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    // Header should declare GetLifetimeReplicatedProps
    assert!(output.header.contains("GetLifetimeReplicatedProps"));

    // Source should implement it
    assert!(output
        .source
        .contains("void UHealthSet::GetLifetimeReplicatedProps"));
    assert!(output.source.contains(
        "DOREPLIFETIME_CONDITION_NOTIFY(UHealthSet, Health, COND_None, REPNOTIFY_Always)"
    ));
    assert!(output.source.contains(
        "DOREPLIFETIME_CONDITION_NOTIFY(UHealthSet, MaxHealth, COND_None, REPNOTIFY_Always)"
    ));
}

#[test]
fn test_rep_notify_functions() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    // Header declarations
    assert!(output
        .header
        .contains("void OnRep_Health(const FGameplayAttributeData& OldValue)"));
    assert!(output
        .header
        .contains("void OnRep_MaxHealth(const FGameplayAttributeData& OldValue)"));

    // Source implementations
    assert!(output.source.contains("void UHealthSet::OnRep_Health"));
    assert!(output
        .source
        .contains("GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthSet, Health, OldValue)"));
    assert!(output.source.contains("void UHealthSet::OnRep_MaxHealth"));
    assert!(output
        .source
        .contains("GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthSet, MaxHealth, OldValue)"));
}

#[test]
fn test_constructor_initialization() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    assert!(output.source.contains("UHealthSet::UHealthSet()"));
    assert!(output.source.contains("Health = 100.0f"));
    assert!(output.source.contains("MaxHealth = 100.0f"));
}

#[test]
fn test_uproperty_generation() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    // Health with HideFromModifiers
    assert!(output
        .header
        .contains("UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health"));
    assert!(output.header.contains("HideFromModifiers"));

    // MaxHealth without HideFromModifiers
    assert!(output
        .header
        .contains("UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth"));

    // Both should have FGameplayAttributeData
    assert!(output.header.contains("FGameplayAttributeData Health"));
    assert!(output.header.contains("FGameplayAttributeData MaxHealth"));
}

#[test]
fn test_includes() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    // Header includes
    assert!(output.header.contains("#include \"CoreMinimal.h\""));
    assert!(output.header.contains("#include \"AttributeSet.h\""));
    assert!(output
        .header
        .contains("#include \"AbilitySystemComponent.h\""));
    assert!(output.header.contains("#include \"HealthSet.generated.h\""));

    // Source includes
    assert!(output.source.contains("#include \"HealthSet.h\""));
    assert!(output.source.contains("#include \"Net/UnrealNetwork.h\""));
    assert!(output
        .source
        .contains("#include \"GameplayEffectExtension.h\""));
}

#[test]
fn test_meta_attributes() {
    let mut ir = create_simple_health_set();

    // Add meta attributes
    ir.attributes.push(AttributeIR {
        name: "damage".to_string(),
        ty: Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        default_value: Some("0.0".to_string()),
        replicated: false,
        rep_notify: false,
        hide_from_modifiers: true,
        is_meta: true,
        clamp_min: None,
        clamp_max: None,
        category: "Health".to_string(),
    });

    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    // Meta attributes should NOT be replicated
    assert!(!output.source.contains("DOREPLIFETIME(UHealthSet, Damage)"));
    assert!(!output
        .source
        .contains("DOREPLIFETIME_CONDITION_NOTIFY(UHealthSet, Damage"));

    // But should still have ATTRIBUTE_ACCESSORS
    assert!(output
        .header
        .contains("ATTRIBUTE_ACCESSORS(UHealthSet, Damage)"));

    // And UPROPERTY
    assert!(output.header.contains("FGameplayAttributeData Damage"));
}

#[test]
fn test_compression_ratio() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    let total_lines = output.header.lines().count() + output.source.lines().count();

    // 2 attributes should generate at least 50 lines
    assert!(
        total_lines > 50,
        "Generated {} lines, expected > 50",
        total_lines
    );

    println!(
        "Compression ratio: 2 attributes → {} C++ lines (1:{})",
        total_lines,
        total_lines / 2
    );
}

#[test]
fn test_lifecycle_hooks_post_gameplay_effect_execute() {
    let mut ir = create_simple_health_set();

    // Add meta attribute for damage
    ir.attributes.push(AttributeIR {
        name: "damage".to_string(),
        ty: Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        default_value: Some("0.0".to_string()),
        replicated: false,
        rep_notify: false,
        hide_from_modifiers: true,
        is_meta: true,
        clamp_min: None,
        clamp_max: None,
        category: "Health".to_string(),
    });

    // Add lifecycle hook
    ir.lifecycle_hooks.post_gameplay_effect_execute = Some(ue5_gas::attribute_set_ir::FunctionIR {
        name: "post_gameplay_effect_execute".to_string(),
        body: "// Custom logic".to_string(),
    });

    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    // Should generate PostGameplayEffectExecute override
    assert!(output
        .header
        .contains("virtual void PostGameplayEffectExecute"));
    assert!(output
        .source
        .contains("void UHealthSet::PostGameplayEffectExecute"));
    assert!(output
        .source
        .contains("Super::PostGameplayEffectExecute(Data)"));

    // Should handle meta attribute
    assert!(output.source.contains("GetDamageAttribute()"));
}

#[test]
fn test_full_output_structure() {
    let ir = create_simple_health_set();
    let output = generate_attribute_set(&ir, "TestPlugin").unwrap();

    // Verify header structure
    assert!(output.header.starts_with("#pragma once"));
    assert!(output
        .header
        .contains("class UHealthSet : public UAttributeSet"));
    assert!(output.header.contains("public:"));
    assert!(output.header.contains("protected:"));
    assert!(output.header.contains("private:"));
    assert!(output.header.ends_with("};\n"));

    // Verify source structure
    assert!(output.source.contains("#include \"HealthSet.h\""));
    assert!(output.source.contains("UHealthSet::UHealthSet()"));
    assert!(output.source.contains("GetLifetimeReplicatedProps"));
}
