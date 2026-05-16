use crate::id::ActorId;
use crate::lifecycle::{DEFAULT_ASK_TIMEOUT_MS, DEFAULT_SHUTDOWN_GRACE_MS};
use crate::mailbox::DEFAULT_MAILBOX_CAPACITY;
use crate::supervision::{
    DEFAULT_RESTART_INTENSITY_MAX_RESTARTS, DEFAULT_RESTART_INTENSITY_WINDOW_MS,
};
use serde::{Deserialize, Serialize};

pub const NATIVE_ACTOR_ABI_VERSION: u32 = 1;
pub const NATIVE_ACTOR_ID_BITS: u16 = 64;
pub const NATIVE_ACTOR_UNBOUNDED_MAILBOX_CAPACITY: usize = 0;
pub const NATIVE_ACTOR_NAME_MAX_BYTES: usize = 128;
pub const NATIVE_ACTOR_TABLE_CAPACITY: usize = 1024;
pub const NATIVE_ACTOR_REGISTRY_CAPACITY: usize = 256;
pub const NATIVE_ACTOR_SCHEDULER_WORKER_COUNT: usize = 4;
pub const NATIVE_ACTOR_MONITOR_EXIT_TAG_BASE: u64 = 0xDEAD_0000;

pub const REQUIRED_NATIVE_ACTOR_SYMBOLS: &[&str] = &[
    "kain_actor_runtime_init",
    "kain_actor_runtime_shutdown",
    "kain_actor_abi_descriptor",
    "kain_actor_abi_descriptor_is_compatible",
    "kain_actor_spawn_config_init",
    "kain_actor_spawn",
    "kain_actor_send",
    "kain_actor_receive",
    "kain_actor_try_receive",
    "kain_actor_reply_port_new",
    "kain_actor_reply_port_actor_id",
    "kain_actor_reply_port_destroy",
    "kain_actor_reply_port_send",
    "kain_actor_reply_port_wait",
    "kain_actor_reply_port_wait_i64",
    "kain_actor_shutdown",
    "kain_actor_kill",
    "kain_actor_get_state",
    "kain_actor_get_supervision_snapshot",
    "kain_actor_monitor",
    "kain_actor_demonitor",
    "kain_actor_link",
    "kain_actor_unlink",
    "kain_actor_registry_register",
    "kain_actor_registry_lookup",
    "kain_actor_registry_unregister",
    "kain_actor_mailbox_count",
    "kain_actor_mailbox_capacity",
    "kain_actor_mailbox_is_full",
    "kain_actor_scheduler_snapshot",
];

pub const REQUIRED_NATIVE_STDLIB_ACTOR_SYMBOLS: &[&str] = &[
    "kain_native_actor_abi_version",
    "kain_native_actor_invalid_id",
    "kain_native_actor_default_mailbox_capacity",
    "kain_native_actor_unbounded_mailbox_capacity",
    "kain_native_actor_default_ask_timeout_ms",
    "kain_native_actor_default_shutdown_grace_ms",
    "kain_native_actor_supervision_max_restarts",
    "kain_native_actor_supervision_restart_window_millis",
    "kain_native_actor_spawn",
    "kain_native_actor_send",
    "kain_native_actor_state_invalid",
    "kain_native_actor_get_state",
    "kain_native_actor_shutdown",
    "kain_native_actor_kill",
    "kain_native_actor_registry_lookup",
    "kain_native_actor_registry_register",
    "kain_native_actor_registry_unregister",
    "kain_native_actor_monitor",
    "kain_native_actor_demonitor",
    "kain_native_actor_link",
    "kain_native_actor_unlink",
    "kain_native_actor_supervision_observed_child_exit_count",
    "kain_native_actor_supervision_restart_attempt_count",
    "kain_native_actor_supervision_escalation_count",
    "kain_native_actor_supervision_limit_hit",
    "kain_native_actor_scheduler_queue_depth",
    "kain_native_actor_scheduler_max_queue_depth",
    "kain_native_actor_scheduler_total_enqueued",
    "kain_native_actor_scheduler_total_dequeued",
    "kain_native_actor_scheduler_worker_count",
    "kain_native_actor_scheduler_active_workers",
    "kain_native_actor_scheduler_busy_workers",
    "kain_native_actor_scheduler_overflow_thread_spawns",
];

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct NativeActorAbi {
    pub abi_version: u32,
    pub actor_id_bits: u16,
    pub invalid_actor_id: u64,
    pub default_mailbox_capacity: usize,
    pub unbounded_mailbox_capacity: usize,
    pub default_ask_timeout_ms: u64,
    pub default_shutdown_grace_ms: u64,
    pub supervision_max_restarts: u32,
    pub supervision_restart_window_ms: u64,
    pub actor_name_max_bytes: usize,
    pub scheduler_worker_count: usize,
    pub actor_table_capacity: usize,
    pub registry_capacity: usize,
    pub monitor_exit_tag_base: u64,
    pub symbol_prefix: String,
}

impl Default for NativeActorAbi {
    fn default() -> Self {
        Self {
            abi_version: NATIVE_ACTOR_ABI_VERSION,
            actor_id_bits: NATIVE_ACTOR_ID_BITS,
            invalid_actor_id: ActorId::INVALID_RAW,
            default_mailbox_capacity: DEFAULT_MAILBOX_CAPACITY,
            unbounded_mailbox_capacity: NATIVE_ACTOR_UNBOUNDED_MAILBOX_CAPACITY,
            default_ask_timeout_ms: DEFAULT_ASK_TIMEOUT_MS,
            default_shutdown_grace_ms: DEFAULT_SHUTDOWN_GRACE_MS,
            supervision_max_restarts: DEFAULT_RESTART_INTENSITY_MAX_RESTARTS,
            supervision_restart_window_ms: DEFAULT_RESTART_INTENSITY_WINDOW_MS,
            actor_name_max_bytes: NATIVE_ACTOR_NAME_MAX_BYTES,
            scheduler_worker_count: NATIVE_ACTOR_SCHEDULER_WORKER_COUNT,
            actor_table_capacity: NATIVE_ACTOR_TABLE_CAPACITY,
            registry_capacity: NATIVE_ACTOR_REGISTRY_CAPACITY,
            monitor_exit_tag_base: NATIVE_ACTOR_MONITOR_EXIT_TAG_BASE,
            symbol_prefix: "kain_actor_".to_string(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum NativeActorStateDiscriminant {
    Uninitialized = 0,
    Initializing = 1,
    Running = 2,
    Suspended = 3,
    ShuttingDown = 4,
    Terminated = 5,
    Failed = 6,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum NativeActorExitReasonDiscriminant {
    Normal = 0,
    Shutdown = 1,
    Killed = 2,
    Crashed = 3,
    SupervisorEscalation = 4,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum NativeSupervisionStrategyDiscriminant {
    OneForOne = 0,
    OneForAll = 1,
    RestForOne = 2,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum NativeRestartPolicyDiscriminant {
    Permanent = 0,
    Temporary = 1,
    Transient = 2,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct NativeActorAbiLayout {
    pub message_type_name: String,
    pub spawn_config_type_name: String,
    pub message_fields: Vec<String>,
    pub spawn_config_fields: Vec<String>,
}

impl Default for NativeActorAbiLayout {
    fn default() -> Self {
        Self {
            message_type_name: "KainActorMessage".to_string(),
            spawn_config_type_name: "KainActorSpawnConfig".to_string(),
            message_fields: vec![
                "unsigned long long type_tag".to_string(),
                "void* data".to_string(),
                "size_t data_size".to_string(),
                "KainActorId sender_id".to_string(),
            ],
            spawn_config_fields: vec![
                "KainActorBootstrapFn bootstrap_fn".to_string(),
                "void* user_data".to_string(),
                "size_t mailbox_capacity".to_string(),
                "KainSupervisionStrategy supervision_strategy".to_string(),
                "KainRestartPolicy restart_policy".to_string(),
                "KainActorId supervisor_id".to_string(),
                "int retain_user_data".to_string(),
                "char name[KAIN_ACTOR_NAME_MAX]".to_string(),
            ],
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct NativeActorLoweringContract {
    pub abi: NativeActorAbi,
    pub layout: NativeActorAbiLayout,
    pub requires_runtime_init: bool,
    pub supports_spawn: bool,
    pub supports_send: bool,
    pub supports_ask: bool,
    pub supports_supervision: bool,
    pub supports_monitoring: bool,
    pub supports_links: bool,
    pub supports_registry: bool,
    pub supports_scheduler_snapshot: bool,
}

impl Default for NativeActorLoweringContract {
    fn default() -> Self {
        Self {
            abi: NativeActorAbi::default(),
            layout: NativeActorAbiLayout::default(),
            requires_runtime_init: true,
            supports_spawn: true,
            supports_send: true,
            supports_ask: true,
            supports_supervision: true,
            supports_monitoring: true,
            supports_links: true,
            supports_registry: true,
            supports_scheduler_snapshot: true,
        }
    }
}

impl NativeActorLoweringContract {
    pub fn required_runtime_symbols(&self) -> &'static [&'static str] {
        REQUIRED_NATIVE_ACTOR_SYMBOLS
    }

    pub fn required_stdlib_symbols(&self) -> &'static [&'static str] {
        REQUIRED_NATIVE_STDLIB_ACTOR_SYMBOLS
    }
}
