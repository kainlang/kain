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

#[test]
fn native_actor_abi_defaults_match_runtime_contract() {
    let abi = NativeActorAbi::default();
    let layout = NativeActorAbiLayout::default();
    let contract = NativeActorLoweringContract::default();

    assert_eq!(abi.abi_version, NATIVE_ACTOR_ABI_VERSION);
    assert_eq!(abi.actor_id_bits, NATIVE_ACTOR_ID_BITS);
    assert_eq!(abi.invalid_actor_id, ActorId::INVALID_RAW);
    assert_eq!(abi.default_mailbox_capacity, DEFAULT_MAILBOX_CAPACITY);
    assert_eq!(
        abi.unbounded_mailbox_capacity,
        NATIVE_ACTOR_UNBOUNDED_MAILBOX_CAPACITY
    );
    assert_eq!(abi.default_ask_timeout_ms, DEFAULT_ASK_TIMEOUT_MS);
    assert_eq!(abi.default_shutdown_grace_ms, DEFAULT_SHUTDOWN_GRACE_MS);
    assert_eq!(
        abi.supervision_max_restarts,
        DEFAULT_RESTART_INTENSITY_MAX_RESTARTS
    );
    assert_eq!(
        abi.supervision_restart_window_ms,
        DEFAULT_RESTART_INTENSITY_WINDOW_MS
    );
    assert_eq!(abi.actor_name_max_bytes, NATIVE_ACTOR_NAME_MAX_BYTES);
    assert_eq!(
        abi.scheduler_worker_count,
        NATIVE_ACTOR_SCHEDULER_WORKER_COUNT
    );
    assert_eq!(abi.actor_table_capacity, NATIVE_ACTOR_TABLE_CAPACITY);
    assert_eq!(abi.registry_capacity, NATIVE_ACTOR_REGISTRY_CAPACITY);
    assert_eq!(
        abi.monitor_exit_tag_base,
        NATIVE_ACTOR_MONITOR_EXIT_TAG_BASE
    );
    assert_eq!(
        layout.message_fields,
        vec![
            "unsigned long long type_tag".to_string(),
            "void* data".to_string(),
            "size_t data_size".to_string(),
            "KainActorId sender_id".to_string(),
        ]
    );
    assert!(layout
        .spawn_config_fields
        .contains(&"int retain_user_data".to_string()));
    assert!(contract.supports_spawn);
    assert!(contract.supports_send);
    assert!(contract.supports_monitoring);
    assert!(contract
        .required_runtime_symbols()
        .contains(&"kain_actor_abi_descriptor"));
    assert!(contract
        .required_stdlib_symbols()
        .contains(&"kain_native_actor_default_ask_timeout_ms"));
}

#[test]
fn native_actor_header_stays_in_sync_with_rust_contract() {
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let header_path = manifest_dir.join("../../runtime/native/include/kain_runtime_actor.h");
    let header = std::fs::read_to_string(&header_path)
        .unwrap_or_else(|error| panic!("failed to read {}: {error}", header_path.display()));

    let expected_macros = [
        "#define KAIN_ACTOR_ABI_VERSION 1U",
        "#define KAIN_ACTOR_ID_BITS 64U",
        "#define KAIN_ACTOR_ID_INVALID 0ULL",
        "#define KAIN_SUPERVISION_MAX_RESTARTS 5",
        "#define KAIN_SUPERVISION_RESTART_WINDOW_MILLIS 60000ULL",
        "#define KAIN_MAILBOX_DEFAULT_CAPACITY 1024",
        "#define KAIN_MAILBOX_UNBOUNDED_CAPACITY 0",
        "#define KAIN_ACTOR_DEFAULT_ASK_TIMEOUT_MS 30000ULL",
        "#define KAIN_ACTOR_DEFAULT_SHUTDOWN_GRACE_MS 5000ULL",
        "#define KAIN_ACTOR_NAME_MAX 128",
        "#define KAIN_ACTOR_TABLE_CAPACITY 1024",
        "#define KAIN_ACTOR_REGISTRY_CAPACITY 256",
        "#define KAIN_ACTOR_SCHEDULER_WORKER_COUNT 4",
        "#define KAIN_ACTOR_MONITOR_EXIT_TAG_BASE 0xDEAD0000ULL",
    ];

    for expected in expected_macros {
        assert!(
            header.contains(expected),
            "native actor header is missing ABI macro `{expected}`"
        );
    }

    for symbol in REQUIRED_NATIVE_ACTOR_SYMBOLS {
        assert!(
            header.contains(symbol),
            "native actor header is missing required symbol `{symbol}`"
        );
    }

    assert!(header.contains("typedef struct {"));
    assert!(header.contains("unsigned int abi_version;"));
    assert!(header.contains("size_t default_mailbox_capacity;"));
    assert!(header.contains("int retain_user_data;"));
    assert!(header.contains("unsigned long long monitor_exit_tag_base;"));
}

#[test]
fn native_actor_stdlib_header_exports_runtime_contract_symbols() {
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let header_path =
        manifest_dir.join("../../runtime/native/include/kain_runtime_native_stdlib.h");
    let header = std::fs::read_to_string(&header_path)
        .unwrap_or_else(|error| panic!("failed to read {}: {error}", header_path.display()));

    for symbol in REQUIRED_NATIVE_STDLIB_ACTOR_SYMBOLS {
        assert!(
            header.contains(symbol),
            "native stdlib header is missing actor symbol `{symbol}`"
        );
    }
}
