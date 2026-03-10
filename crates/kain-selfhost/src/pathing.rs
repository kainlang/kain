use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct SelfHostPaths {
    pub repo_root: String,
    pub ouroboros_root: String,
    pub phase2_root: String,
    pub repaired_root: String,
    pub repair_docs: String,
    pub pipeline_out: String,
}
