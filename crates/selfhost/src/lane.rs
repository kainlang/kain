use serde::{Deserialize, Serialize};

use crate::artifacts::ArtifactExpectation;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum StepKind {
    Python,
    PowershellFile,
    Shell,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SelfHostStep {
    pub id: String,
    pub kind: StepKind,
    pub command_template: String,
    #[serde(default)]
    pub expected_artifacts: Vec<ArtifactExpectation>,
    pub timeout_seconds: Option<u64>,
    #[serde(default)]
    pub retry_policy: Option<String>,
    #[serde(default)]
    pub produces: Vec<String>,
    #[serde(default)]
    pub consumes: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SelfHostLane {
    pub id: String,
    pub description: String,
    #[serde(default)]
    pub dependencies: Vec<String>,
    #[serde(default)]
    pub required_artifacts: Vec<ArtifactExpectation>,
    #[serde(default)]
    pub crate_slice: Vec<String>,
    #[serde(default)]
    pub steps: Vec<SelfHostStep>,
    pub continue_on_failure: bool,
    #[serde(default)]
    pub failure_policy: Option<String>,
}
