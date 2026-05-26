use crate::id::ActorId;
use crate::lifecycle::{ActorExitReason, ActorState};
use crate::mailbox::MailboxStats;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorRefMetadata {
    pub id: ActorId,
    pub actor_type: String,
    pub name: Option<String>,
}

impl ActorRefMetadata {
    pub fn anonymous(id: ActorId, actor_type: impl Into<String>) -> Self {
        Self {
            id,
            actor_type: actor_type.into(),
            name: None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorRegistryEntry {
    pub id: ActorId,
    pub actor_type: String,
    pub state: ActorState,
    pub mailbox: MailboxStats,
    pub parent: Option<ActorId>,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorRuntimeSnapshot {
    pub actors: Vec<ActorRegistryEntry>,
    pub delivered_messages: u64,
    pub dropped_messages: u64,
}

impl ActorRuntimeSnapshot {
    pub fn running_count(&self) -> usize {
        self.actors
            .iter()
            .filter(|entry| entry.state == ActorState::Running)
            .count()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SendOutcome {
    Delivered,
    MailboxClosed,
    MailboxFull,
    UnknownActor,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ActorRuntimeEvent {
    Spawned {
        id: ActorId,
        actor_type: String,
    },
    MessageDelivered {
        target: ActorId,
        message: String,
    },
    Exited {
        id: ActorId,
        reason: ActorExitReason,
    },
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorRuntimeCapabilities {
    pub supports_threads: bool,
    pub supports_mailbox_backpressure: bool,
    pub supports_supervision: bool,
    pub supports_native_abi: bool,
}

impl Default for ActorRuntimeCapabilities {
    fn default() -> Self {
        Self {
            supports_threads: true,
            supports_mailbox_backpressure: false,
            supports_supervision: false,
            supports_native_abi: false,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorRuntimeModel {
    pub capabilities: ActorRuntimeCapabilities,
    pub snapshot: ActorRuntimeSnapshot,
    pub events: Vec<ActorRuntimeEvent>,
}

impl Default for ActorRuntimeModel {
    fn default() -> Self {
        Self {
            capabilities: ActorRuntimeCapabilities::default(),
            snapshot: ActorRuntimeSnapshot::default(),
            events: Vec::new(),
        }
    }
}
