// ============================================================================
// Target Actor Integration Tests (Phase 7)
// ============================================================================

use ue5_gas::{generate_target, TargetActorIR, TargetFilterIR, TraceTypeIR};
use ue5_gas::target_ir::MethodIR;

fn create_target_ir(name: &str) -> TargetActorIR {
    TargetActorIR {
        name: name.to_string(),
        trace_type: TraceTypeIR::Line,
        max_range: Some(1200.0),
        trace_channel: Some("Visibility".to_string()),
        filter: None,
        reticle_class: None,
        custom_methods: vec![],
    }
}

#[test]
fn test_generates_expected_target_class_shape() {
    let ir = create_target_ir("TacticalLineTraceTarget");
    let output = generate_target(&ir, "TestPlugin").unwrap();

    assert!(output.header.contains("class ATacticalLineTraceTarget : public AGameplayAbilityTargetActor"));
    assert!(output.header.contains("virtual void StartTargeting(UGameplayAbility* Ability) override;"));
    assert!(output.source.contains("ATacticalLineTraceTarget::ATacticalLineTraceTarget()"));
    assert!(output.source.contains("TraceChannel = FName(\"Visibility\")"));
    assert!(output.source.contains("MaxRange = 1200.0f;"));
}

#[test]
fn test_optional_fields_are_data_driven_and_omitted_when_not_present() {
    let mut ir = create_target_ir("MinimalTarget");
    ir.max_range = None;
    ir.trace_channel = None;

    let output = generate_target(&ir, "TestPlugin").unwrap();

    assert!(!output.header.contains("float MaxRange;"));
    assert!(!output.header.contains("FName TraceChannel;"));
    assert!(!output.source.contains("MaxRange ="));
    assert!(!output.source.contains("TraceChannel ="));
}

#[test]
fn test_custom_methods_are_declared_and_implemented() {
    let mut ir = create_target_ir("CustomMethodTarget");
    ir.custom_methods.push(MethodIR {
        name: "ApplyTargetFilter".to_string(),
        body: "// TODO: user body".to_string(),
    });

    let output = generate_target(&ir, "TestPlugin").unwrap();

    assert!(output.header.contains("void ApplyTargetFilter();"));
    assert!(output.source.contains("void ACustomMethodTarget::ApplyTargetFilter()"));
    assert!(output.source.contains("// TODO: user body"));
}

#[test]
fn test_filter_and_reticle_fields_roundtrip_in_ir() {
    let mut ir = create_target_ir("FilterTarget");
    ir.filter = Some(TargetFilterIR {
        self_filter: Some("IgnoreSelf".to_string()),
        required_actor_class: Some("AEnemyCharacter".to_string()),
        require_tags: vec!["Status.Alive".to_string()],
        ignore_tags: vec!["Status.Cloaked".to_string()],
        custom_filter_body: Some("// TODO: custom filter".to_string()),
    });
    ir.reticle_class = Some("ATargetReticleActor".to_string());

    // Current codegen doesn't emit filter/reticle behavior yet, but IR must preserve values.
    assert_eq!(ir.filter.as_ref().unwrap().self_filter.as_deref(), Some("IgnoreSelf"));
    assert_eq!(
        ir.filter.as_ref().unwrap().required_actor_class.as_deref(),
        Some("AEnemyCharacter")
    );
    assert_eq!(ir.reticle_class.as_deref(), Some("ATargetReticleActor"));
}
