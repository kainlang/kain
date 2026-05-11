use crate::id::ActorId;
use crate::lifecycle::{RestartPolicy, SupervisionStrategy};
use serde::{Deserialize, Serialize};

pub const DEFAULT_RESTART_INTENSITY_MAX_RESTARTS: u32 = 5;
pub const DEFAULT_RESTART_INTENSITY_WINDOW_MS: u64 = 60_000;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum LinkMode {
    Linked,
    Monitored,
    Isolated,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ChildKind {
    Worker,
    Supervisor,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct RestartIntensity {
    pub max_restarts: u32,
    pub within_ms: u64,
}

impl Default for RestartIntensity {
    fn default() -> Self {
        Self {
            max_restarts: DEFAULT_RESTART_INTENSITY_MAX_RESTARTS,
            within_ms: DEFAULT_RESTART_INTENSITY_WINDOW_MS,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ChildSpec {
    pub id: String,
    pub actor_type: String,
    pub kind: ChildKind,
    pub restart: RestartPolicy,
    pub link_mode: LinkMode,
}

impl ChildSpec {
    pub fn worker(id: impl Into<String>, actor_type: impl Into<String>) -> Self {
        Self {
            id: id.into(),
            actor_type: actor_type.into(),
            kind: ChildKind::Worker,
            restart: RestartPolicy::Permanent,
            link_mode: LinkMode::Linked,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SupervisorSpec {
    pub name: String,
    pub strategy: SupervisionStrategy,
    pub restart_intensity: RestartIntensity,
    pub children: Vec<ChildSpec>,
}

impl SupervisorSpec {
    pub fn new(name: impl Into<String>, strategy: SupervisionStrategy) -> Self {
        Self {
            name: name.into(),
            strategy,
            restart_intensity: RestartIntensity::default(),
            children: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct MonitorSpec {
    pub watcher: ActorId,
    pub watched: ActorId,
    pub link_mode: LinkMode,
}
