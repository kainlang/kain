use crate::{
    OrchestrateFallback, OrchestratePlannerPolicy, OrchestrateResidency, OrchestrateStagePlan,
    OrchestrateTransfer,
};
use serde::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct OrchestrateStageGraphMetadata {
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub dependencies: Vec<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub residency: Option<OrchestrateResidency>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub transfer: Option<OrchestrateTransfer>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub guard: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub fallback: Option<OrchestrateFallback>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub requires: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub policy: Option<OrchestratePlannerPolicy>,
}

impl OrchestrateStageGraphMetadata {
    pub fn dependency_list(&self) -> String {
        self.dependencies.join(",")
    }

    pub fn residency_name(&self) -> &'static str {
        self.residency
            .map(OrchestrateResidency::as_str)
            .unwrap_or("unspecified")
    }

    pub fn transfer_name(&self) -> &'static str {
        self.transfer
            .map(OrchestrateTransfer::as_str)
            .unwrap_or("unspecified")
    }

    pub fn guard_name(&self) -> &str {
        self.guard.as_deref().unwrap_or("none")
    }

    pub fn fallback_name(&self) -> String {
        self.fallback
            .as_ref()
            .map(OrchestrateFallback::authored)
            .unwrap_or_else(|| "none".to_string())
    }

    pub fn requires_name(&self) -> &str {
        self.requires.as_deref().unwrap_or("none")
    }

    pub fn policy_name(&self) -> &'static str {
        self.policy
            .unwrap_or_default()
            .as_str()
    }

    pub fn adaptive(&self) -> bool {
        self.policy.unwrap_or_default().adaptive()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct OrchestrateGraphPlan {
    pub name: String,
    pub stages: Vec<OrchestrateStagePlan>,
}

impl OrchestrateGraphPlan {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            stages: Vec::new(),
        }
    }

    pub fn push_stage(&mut self, stage: OrchestrateStagePlan) {
        self.stages.push(stage);
    }

    pub fn validate(&self) -> OrchestrateGraphValidation {
        OrchestrateGraphValidation::from_plan(self)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Default)]
pub struct OrchestrateGraphValidation {
    pub valid: bool,
    pub diagnostics: Vec<String>,
}

impl OrchestrateGraphValidation {
    pub fn from_plan(plan: &OrchestrateGraphPlan) -> Self {
        let mut diagnostics = Vec::new();
        let mut stage_names = HashSet::new();
        let mut deps: HashMap<String, Vec<String>> = HashMap::new();

        for stage in &plan.stages {
            if !stage_names.insert(stage.binding_name.clone()) {
                diagnostics.push(format!("duplicate stage `{}`", stage.binding_name));
            }
            deps.insert(
                stage.binding_name.clone(),
                stage.metadata.dependencies.clone(),
            );
        }

        for stage in &plan.stages {
            for dependency in &stage.metadata.dependencies {
                if !stage_names.contains(dependency) {
                    diagnostics.push(format!(
                        "stage `{}` depends on unknown stage `{dependency}`",
                        stage.binding_name
                    ));
                }
            }

            if let Some(required) = &stage.metadata.requires {
                if !stage_names.contains(required) {
                    diagnostics.push(format!(
                        "stage `{}` requires unknown law stage `{required}`",
                        stage.binding_name
                    ));
                }
            }

            if let Some(fallback) = &stage.metadata.fallback {
                if let Some(target) = fallback.stage_name() {
                    if !stage_names.contains(target) {
                        diagnostics.push(format!(
                            "stage `{}` fallback references unknown stage `{target}`",
                            stage.binding_name
                        ));
                    }
                }
            }
        }

        for stage in &plan.stages {
            let mut visiting = HashSet::new();
            let mut visited = HashSet::new();
            if has_cycle(&stage.binding_name, &deps, &mut visiting, &mut visited) {
                diagnostics.push(format!(
                    "stage `{}` participates in a dependency cycle",
                    stage.binding_name
                ));
            }
        }

        diagnostics.sort();
        diagnostics.dedup();
        Self {
            valid: diagnostics.is_empty(),
            diagnostics,
        }
    }
}

fn has_cycle(
    stage: &str,
    deps: &HashMap<String, Vec<String>>,
    visiting: &mut HashSet<String>,
    visited: &mut HashSet<String>,
) -> bool {
    if visited.contains(stage) {
        return false;
    }
    if !visiting.insert(stage.to_string()) {
        return true;
    }
    if let Some(children) = deps.get(stage) {
        for child in children {
            if deps.contains_key(child) && has_cycle(child, deps, visiting, visited) {
                return true;
            }
        }
    }
    visiting.remove(stage);
    visited.insert(stage.to_string());
    false
}
