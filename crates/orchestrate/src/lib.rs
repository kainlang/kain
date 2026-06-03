use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OrchestrateStageKind {
    Kain,
    C,
    Cpu,
    Gpu,
    Dispatch,
    Converge,
    Law,
    Patch,
    World,
    Python,
    Rust,
    Node,
}

impl OrchestrateStageKind {
    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "kain" => Some(Self::Kain),
            "c" => Some(Self::C),
            "cpu" => Some(Self::Cpu),
            "gpu" => Some(Self::Gpu),
            "dispatch" => Some(Self::Dispatch),
            "converge" => Some(Self::Converge),
            "law" => Some(Self::Law),
            "patch" => Some(Self::Patch),
            "world" => Some(Self::World),
            "python" => Some(Self::Python),
            "rust" => Some(Self::Rust),
            "node" => Some(Self::Node),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Kain => "kain",
            Self::C => "c",
            Self::Cpu => "cpu",
            Self::Gpu => "gpu",
            Self::Dispatch => "dispatch",
            Self::Converge => "converge",
            Self::Law => "law",
            Self::Patch => "patch",
            Self::World => "world",
            Self::Python => "python",
            Self::Rust => "rust",
            Self::Node => "node",
        }
    }

    pub fn is_compat_adapter(self) -> bool {
        matches!(self, Self::Rust | Self::Node)
    }

    pub fn is_primary_interop(self) -> bool {
        matches!(self, Self::C | Self::Python)
    }

    pub fn is_silicon_native(self) -> bool {
        matches!(
            self,
            Self::Cpu
                | Self::Gpu
                | Self::Dispatch
                | Self::Converge
                | Self::Law
                | Self::Patch
                | Self::World
        )
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OrchestrateSelector {
    Capability(String),
    Target(String),
}

impl OrchestrateSelector {
    pub fn capability(value: impl Into<String>) -> Self {
        Self::Capability(value.into())
    }

    pub fn target(value: impl Into<String>) -> Self {
        Self::Target(value.into())
    }

    pub fn key(&self) -> &'static str {
        match self {
            Self::Capability(_) => "capability",
            Self::Target(_) => "target",
        }
    }

    pub fn value(&self) -> &str {
        match self {
            Self::Capability(value) | Self::Target(value) => value,
        }
    }

    pub fn authored(&self) -> String {
        format!("{}(\"{}\")", self.key(), self.value())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct OrchestrateStagePlan {
    pub binding_name: String,
    pub kind: OrchestrateStageKind,
    pub function: String,
    pub selector: Option<OrchestrateSelector>,
}

impl OrchestrateStagePlan {
    pub fn new(
        binding_name: impl Into<String>,
        kind: OrchestrateStageKind,
        function: impl Into<String>,
        selector: Option<OrchestrateSelector>,
    ) -> Self {
        Self {
            binding_name: binding_name.into(),
            kind,
            function: function.into(),
            selector,
        }
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
}

#[derive(Debug, thiserror::Error)]
pub enum OrchestrateError {
    #[error("unknown orchestrate stage kind `{0}`")]
    UnknownStageKind(String),
}
