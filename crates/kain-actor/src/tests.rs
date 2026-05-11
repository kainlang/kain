use crate::*;

#[test]
fn actor_ids_are_monotonic_and_skip_invalid_zero() {
    let mut allocator = ActorIdAllocator::default();

    assert_eq!(allocator.allocate().as_u64(), 1);
    assert_eq!(allocator.allocate().as_u64(), 2);

    allocator.reserve_raw(10);
    assert_eq!(allocator.allocate().as_u64(), 11);
}

#[test]
fn message_catalog_reports_duplicate_names_once() {
    let catalog = MessageCatalog::new(vec![
        MessageSignature::cast("tick", vec![]),
        MessageSignature::cast("tick", vec![]),
        MessageSignature::cast("stop", vec![]),
        MessageSignature::cast("tick", vec![]),
    ]);

    assert_eq!(catalog.duplicate_names(), vec!["tick".to_string()]);
}

#[test]
fn validator_accepts_complete_actor_contract() {
    let mut actor = ActorDefinition::new("Counter");
    actor.state.push(ActorStateSlot::new("count", "Int"));
    actor
        .handlers
        .push(ActorHandlerSignature::cast(MessageSignature::cast(
            "increment",
            vec![MessageParameter::required("amount", "Int")],
        )));
    actor.methods.push(ActorMethodSignature::new(
        "current",
        Vec::new(),
        "Int",
        Vec::new(),
    ));

    validate_actor_definition(&actor).unwrap();
}

#[test]
fn validator_rejects_duplicate_handlers() {
    let mut actor = ActorDefinition::new("Counter");
    actor
        .handlers
        .push(ActorHandlerSignature::cast(MessageSignature::cast(
            "increment",
            Vec::new(),
        )));
    actor
        .handlers
        .push(ActorHandlerSignature::cast(MessageSignature::cast(
            "increment",
            Vec::new(),
        )));

    let error = validate_actor_definition(&actor).unwrap_err();
    assert!(matches!(
        error,
        ActorValidationError::DuplicateHandler { .. }
    ));
}

#[test]
fn supervisor_validation_rejects_duplicate_child_ids() {
    let mut actor = ActorDefinition::new("RootSupervisor");
    let mut supervisor = SupervisorSpec::new("RootSupervisor", SupervisionStrategy::OneForOne);
    supervisor
        .children
        .push(ChildSpec::worker("counter", "Counter"));
    supervisor
        .children
        .push(ChildSpec::worker("counter", "Counter"));
    actor.supervisor = Some(supervisor);

    let error = validate_actor_definition(&actor).unwrap_err();
    assert!(matches!(
        error,
        ActorValidationError::DuplicateSupervisorChild { .. }
    ));
}

#[test]
fn actor_system_validation_checks_supervisor_child_types() {
    let mut system = ActorSystemDefinition::new("game");
    system.actors.push(ActorDefinition::new("Counter"));

    let mut supervisor = SupervisorSpec::new("Root", SupervisionStrategy::OneForOne);
    supervisor
        .children
        .push(ChildSpec::worker("counter", "Counter"));
    supervisor
        .children
        .push(ChildSpec::worker("missing", "MissingActor"));
    system.root_supervisor = Some(supervisor);

    let error = system.validate().unwrap_err();
    assert!(matches!(
        error,
        ActorSystemValidationError::UnknownSupervisorChild { .. }
    ));
}

#[test]
fn actor_paths_are_stable_runtime_addresses() {
    let path = ActorPath::root("main", "Root").child("Counter");
    assert_eq!(path.to_string(), "kain://main/Root/Counter");
}
