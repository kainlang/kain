use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OrchestratePlannerPolicy {
    Static,
    TelemetryPreferGpu,
    TelemetryPreferCpu,
    TelemetryBalanceLatency,
}

impl Default for OrchestratePlannerPolicy {
    fn default() -> Self {
        Self::Static
    }
}

impl OrchestratePlannerPolicy {
    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "static" => Some(Self::Static),
            "telemetry_prefer_gpu" => Some(Self::TelemetryPreferGpu),
            "telemetry_prefer_cpu" => Some(Self::TelemetryPreferCpu),
            "telemetry_balance_latency" => Some(Self::TelemetryBalanceLatency),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Static => "static",
            Self::TelemetryPreferGpu => "telemetry_prefer_gpu",
            Self::TelemetryPreferCpu => "telemetry_prefer_cpu",
            Self::TelemetryBalanceLatency => "telemetry_balance_latency",
        }
    }

    pub fn adaptive(self) -> bool {
        !matches!(self, Self::Static)
    }
}
