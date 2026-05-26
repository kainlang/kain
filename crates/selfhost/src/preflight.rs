use serde::{Deserialize, Serialize};

use crate::blockers::BlockerBucket;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PreflightFailure {
    pub rule_id: String,
    pub bucket: BlockerBucket,
    pub file: String,
    pub line: Option<u64>,
    pub message: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct StructuralPreflightReport {
    #[serde(default)]
    pub checks: Vec<String>,
    #[serde(default)]
    pub failures: Vec<PreflightFailure>,
}
