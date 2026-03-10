use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

use crate::{artifacts::ArtifactExpectation, blockers::BlockerBucket};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct StepExecutionSummary {
    pub id: String,
    pub command: String,
    pub returncode: i32,
    pub success: bool,
    pub log_path: String,
    #[serde(default)]
    pub expected_artifacts: Vec<ArtifactExpectation>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SelfHostLaneSummary {
    pub lane_id: String,
    pub success: bool,
    #[serde(default)]
    pub executed_lanes: Vec<String>,
    #[serde(default)]
    pub required_artifacts: Vec<ArtifactExpectation>,
    #[serde(default)]
    pub steps: Vec<StepExecutionSummary>,
    #[serde(default)]
    pub blocker_bucket_counts: BTreeMap<BlockerBucket, u64>,
    pub stage2_binary_path: Option<String>,
    pub repaired_root: Option<String>,
    pub generated_at: String,
}
