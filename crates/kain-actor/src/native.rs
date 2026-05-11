use crate::id::ActorId;
use crate::lifecycle::{DEFAULT_ASK_TIMEOUT_MS, DEFAULT_SHUTDOWN_GRACE_MS};
use crate::mailbox::DEFAULT_MAILBOX_CAPACITY;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct NativeActorAbi {
    pub actor_id_bits: u16,
    pub invalid_actor_id: u64,
    pub default_mailbox_capacity: usize,
    pub default_ask_timeout_ms: u64,
    pub default_shutdown_grace_ms: u64,
    pub symbol_prefix: String,
}

impl Default for NativeActorAbi {
    fn default() -> Self {
        Self {
            actor_id_bits: 64,
            invalid_actor_id: ActorId::INVALID_RAW,
            default_mailbox_capacity: DEFAULT_MAILBOX_CAPACITY,
            default_ask_timeout_ms: DEFAULT_ASK_TIMEOUT_MS,
            default_shutdown_grace_ms: DEFAULT_SHUTDOWN_GRACE_MS,
            symbol_prefix: "kain_actor_".to_string(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct NativeActorLoweringContract {
    pub abi: NativeActorAbi,
    pub requires_runtime_init: bool,
    pub supports_spawn: bool,
    pub supports_send: bool,
    pub supports_ask: bool,
    pub supports_supervision: bool,
}

impl Default for NativeActorLoweringContract {
    fn default() -> Self {
        Self {
            abi: NativeActorAbi::default(),
            requires_runtime_init: true,
            supports_spawn: true,
            supports_send: true,
            supports_ask: true,
            supports_supervision: true,
        }
    }
}
