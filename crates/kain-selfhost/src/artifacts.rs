use serde::{Deserialize, Serialize};

use crate::blockers::BlockerBucket;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ArtifactExpectation {
    pub path: String,
    #[serde(default)]
    pub exists: Option<bool>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ArtifactContract {
    pub lane_id: String,
    #[serde(default)]
    pub required_artifacts: Vec<ArtifactExpectation>,
    #[serde(default)]
    pub produced_artifacts: Vec<ArtifactExpectation>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FrontErrorRecord {
    pub code: Option<String>,
    pub text: String,
    pub file: Option<String>,
    pub line: Option<u64>,
    pub col: Option<u64>,
    pub bucket: BlockerBucket,
}
