use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum InputSourceKind {
    HumanKeyboard,
    HumanPointer,
    CliStdin,
    UiRuntime,
    AgentIntent,
    TestSynthetic,
    NativePlatform,
}

impl InputSourceKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::HumanKeyboard => "human.keyboard",
            Self::HumanPointer => "human.pointer",
            Self::CliStdin => "cli.stdin",
            Self::UiRuntime => "ui.runtime",
            Self::AgentIntent => "agent.intent",
            Self::TestSynthetic => "test.synthetic",
            Self::NativePlatform => "native.platform",
        }
    }
}

impl From<InputSourceKind> for String {
    fn from(kind: InputSourceKind) -> Self {
        kind.as_str().to_string()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct InputSource {
    pub kind: String,
    pub id: String,
    pub label: String,
}

impl InputSource {
    pub fn new(kind: InputSourceKind, id: impl Into<String>) -> Self {
        let id = id.into();
        Self {
            kind: kind.as_str().to_string(),
            label: id.clone(),
            id,
        }
    }

    pub fn custom(kind: impl Into<String>, id: impl Into<String>) -> Self {
        let id = id.into();
        Self {
            kind: kind.into(),
            label: id.clone(),
            id,
        }
    }
}
