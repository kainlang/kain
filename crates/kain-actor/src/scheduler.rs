use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SchedulerLaneKind {
    Control,
    Cpu,
    Io,
    Realtime,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SchedulerLanePolicy {
    pub name: String,
    pub kind: SchedulerLaneKind,
    pub worker_threads: usize,
    pub mailbox_poll_budget: usize,
}

impl SchedulerLanePolicy {
    pub fn new(name: impl Into<String>, kind: SchedulerLaneKind, worker_threads: usize) -> Self {
        Self {
            name: name.into(),
            kind,
            worker_threads: worker_threads.max(1),
            mailbox_poll_budget: 64,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorSchedulerPolicy {
    pub lanes: Vec<SchedulerLanePolicy>,
    pub work_stealing: bool,
    pub cooperative_yield_after_messages: usize,
}

impl Default for ActorSchedulerPolicy {
    fn default() -> Self {
        Self {
            lanes: vec![
                SchedulerLanePolicy::new("control", SchedulerLaneKind::Control, 1),
                SchedulerLanePolicy::new("cpu", SchedulerLaneKind::Cpu, 1),
                SchedulerLanePolicy::new("io", SchedulerLaneKind::Io, 1),
            ],
            work_stealing: true,
            cooperative_yield_after_messages: 128,
        }
    }
}
