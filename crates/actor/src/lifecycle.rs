use serde::{Deserialize, Serialize};

pub const DEFAULT_ASK_TIMEOUT_MS: u64 = 30_000;
pub const DEFAULT_SHUTDOWN_GRACE_MS: u64 = 5_000;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ActorState {
    New,
    Starting,
    Running,
    Suspended,
    Stopping,
    Stopped,
    Failed,
}

impl ActorState {
    pub const fn is_terminal(self) -> bool {
        matches!(self, Self::Stopped | Self::Failed)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ActorExitReason {
    Normal,
    Shutdown,
    Panic(String),
    HandlerError(String),
    MailboxClosed,
    Killed,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum RestartPolicy {
    Permanent,
    Transient,
    Temporary,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SupervisionStrategy {
    OneForOne,
    OneForAll,
    RestForOne,
    SimpleOneForOne,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorLifecyclePolicy {
    pub restart: RestartPolicy,
    pub shutdown_grace_ms: u64,
    pub kill_on_parent_exit: bool,
}

impl Default for ActorLifecyclePolicy {
    fn default() -> Self {
        Self {
            restart: RestartPolicy::Permanent,
            shutdown_grace_ms: DEFAULT_SHUTDOWN_GRACE_MS,
            kill_on_parent_exit: true,
        }
    }
}
