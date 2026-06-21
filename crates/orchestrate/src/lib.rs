mod graph;
mod planner;
mod stage;

pub use graph::{
    AccessKind, OrchestrateGraphPlan, OrchestrateGraphValidation,
    OrchestrateStageGraphMetadata, ResourceAccess, ResourceStage,
};
pub use planner::{
    infer_barrier_metadata, infer_push_constant_eligibility, BarrierSpec,
    OrchestratePlannerPolicy,
};
pub use stage::{
    OrchestrateFallback, OrchestrateResidency, OrchestrateSelector, OrchestrateStageKind,
    OrchestrateStagePlan, OrchestrateTransfer,
};

#[derive(Debug, thiserror::Error)]
pub enum OrchestrateError {
    #[error("unknown orchestrate stage kind `{0}`")]
    UnknownStageKind(String),
    #[error("invalid orchestrate graph `{0}`: {1}")]
    InvalidGraph(String, String),
}
