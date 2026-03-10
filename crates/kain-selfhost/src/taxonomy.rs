use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct TaxonomyBucket {
    pub id: String,
    pub description: String,
    pub severity: String,
    #[serde(default)]
    pub regexes: Vec<String>,
    #[serde(default)]
    pub candidate_rule_ids: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct Taxonomy {
    #[serde(default)]
    pub buckets: Vec<TaxonomyBucket>,
}
