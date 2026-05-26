use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum MatchType {
    Literal,
    Regex,
    Structured,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct RuleScope {
    #[serde(default)]
    pub globs: Vec<String>,
    #[serde(default)]
    pub crate_names: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct RepairRule {
    pub id: String,
    pub description: String,
    pub target_kind: String,
    pub match_type: MatchType,
    pub pattern: String,
    pub replacement: String,
    #[serde(default)]
    pub scope: RuleScope,
    #[serde(default)]
    pub phase: Vec<String>,
    pub severity: String,
    pub enabled: bool,
    pub confidence: Option<f64>,
    #[serde(default)]
    pub notes: String,
}
