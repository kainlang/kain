mod graph;
mod planner;
mod stage;

pub use graph::{
    OrchestrateGraphPlan, OrchestrateGraphValidation, OrchestrateStageGraphMetadata,
};
pub use planner::OrchestratePlannerPolicy;
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
