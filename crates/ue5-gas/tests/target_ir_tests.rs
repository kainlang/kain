// ============================================================================
// Target Actor IR Tests (Phase 7)
// ============================================================================

use kain_core::ast::{
    Attribute, Block, Function, TargetActorDef, TargetFilter, TraceType, Visibility,
};
use kain_core::span::Span;
use ue5_gas::target_ir::{TargetActorIR, TraceTypeIR};

fn create_target_actor(name: &str) -> TargetActorDef {
    TargetActorDef {
        name: name.to_string(),
        attributes: vec![Attribute {
            name: "target_actor".to_string(),
            args: vec![],
            span: Span::default(),
        }],
        trace_type: TraceType::Line,
        max_range: Some(1500.0),
        trace_channel: Some("Visibility".to_string()),
        filter: None,
        reticle_class: Some("ATargetReticle".to_string()),
        custom_methods: vec![],
        span: Span::default(),
    }
}

fn create_filter_function(name: &str) -> Function {
    Function {
        name: name.to_string(),
        generics: vec![],
        params: vec![],
        return_type: None,
        effects: vec![],
        body: Block {
            stmts: vec![],
            span: Span::default(),
        },
        visibility: Visibility::Public,
        attributes: vec![],
        span: Span::default(),
    }
}

#[test]
fn test_target_actor_requires_target_actor_attribute() {
    let mut target = create_target_actor("MissingAttrTarget");
    target.attributes.clear();

    let result = TargetActorIR::from_ast(&target);

    assert!(result.is_err());
    assert!(result
        .unwrap_err()
        .to_string()
        .contains("must have @target_actor attribute"));
}

#[test]
fn test_trace_type_mapping_variants() {
    let mut target = create_target_actor("TraceMapTarget");

    target.trace_type = TraceType::Line;
    assert_eq!(
        TargetActorIR::from_ast(&target).unwrap().trace_type,
        TraceTypeIR::Line
    );

    target.trace_type = TraceType::Sphere;
    assert_eq!(
        TargetActorIR::from_ast(&target).unwrap().trace_type,
        TraceTypeIR::Sphere
    );

    target.trace_type = TraceType::Cone;
    assert_eq!(
        TargetActorIR::from_ast(&target).unwrap().trace_type,
        TraceTypeIR::Cone
    );

    target.trace_type = TraceType::Box;
    assert_eq!(
        TargetActorIR::from_ast(&target).unwrap().trace_type,
        TraceTypeIR::Box
    );

    target.trace_type = TraceType::Cylinder;
    assert_eq!(
        TargetActorIR::from_ast(&target).unwrap().trace_type,
        TraceTypeIR::Cylinder
    );
}

#[test]
fn test_filter_data_is_translated_to_ir() {
    let mut target = create_target_actor("FilterTarget");
    target.filter = Some(TargetFilter {
        self_filter: Some("IgnoreSelf".to_string()),
        required_actor_class: Some("AEnemyCharacter".to_string()),
        require_tags: vec!["Status.Alive".to_string()],
        ignore_tags: vec!["Status.Stealthed".to_string()],
        custom_filter_method: Some(create_filter_function("custom_filter")),
        span: Span::default(),
    });

    let ir = TargetActorIR::from_ast(&target).unwrap();
    let filter = ir.filter.expect("expected filter to exist");

    assert_eq!(filter.self_filter.as_deref(), Some("IgnoreSelf"));
    assert_eq!(
        filter.required_actor_class.as_deref(),
        Some("AEnemyCharacter")
    );
    assert_eq!(filter.require_tags, vec!["Status.Alive".to_string()]);
    assert_eq!(filter.ignore_tags, vec!["Status.Stealthed".to_string()]);
    assert_eq!(
        filter.custom_filter_body.as_deref(),
        Some("// TODO: Implement custom filter codegen")
    );
}
