use serde::{Deserialize, Serialize};

pub const RESONATE_CAPABILITY_KEY: &str = "state.resonate";
pub const DEFAULT_DAMPEN_UNIT: &str = "ns";
pub const DEFAULT_DAMPEN_VALUE: i64 = 0;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ResonanceTarget {
    pub segments: Vec<String>,
}

impl ResonanceTarget {
    pub fn new(segments: Vec<String>) -> Result<Self, ResonateError> {
        if segments.len() < 2 {
            return Err(ResonateError::InvalidTarget(
                "resonate targets must use a stable dotted path such as World.field".to_string(),
            ));
        }
        if segments.iter().any(|segment| segment.trim().is_empty()) {
            return Err(ResonateError::InvalidTarget(
                "resonate targets cannot contain empty path segments".to_string(),
            ));
        }
        Ok(Self { segments })
    }

    pub fn authored_path(&self) -> String {
        self.segments.join(".")
    }

    pub fn root(&self) -> &str {
        self.segments.first().map(String::as_str).unwrap_or("")
    }

    pub fn leaf(&self) -> &str {
        self.segments.last().map(String::as_str).unwrap_or("")
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct DampenWindow {
    pub value: i64,
    pub unit: DampenUnit,
}

impl DampenWindow {
    pub fn none() -> Self {
        Self {
            value: DEFAULT_DAMPEN_VALUE,
            unit: DampenUnit::Ns,
        }
    }

    pub fn new(value: i64, unit: &str) -> Result<Self, ResonateError> {
        if value < 0 {
            return Err(ResonateError::InvalidDampen(
                "dampen duration cannot be negative".to_string(),
            ));
        }
        Ok(Self {
            value,
            unit: DampenUnit::from_name(unit).ok_or_else(|| {
                ResonateError::InvalidDampen(format!("unsupported dampen unit `{unit}`"))
            })?,
        })
    }

    pub fn authored(&self) -> String {
        format!("{}{}", self.value, self.unit.as_str())
    }

    pub fn nanos(&self) -> u64 {
        let value = self.value.max(0) as u64;
        match self.unit {
            DampenUnit::Ns => value,
            DampenUnit::Us => value.saturating_mul(1_000),
            DampenUnit::Ms => value.saturating_mul(1_000_000),
            DampenUnit::S => value.saturating_mul(1_000_000_000),
            DampenUnit::Tick | DampenUnit::Ticks => value.saturating_mul(1_000_000),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DampenUnit {
    Ns,
    Us,
    Ms,
    S,
    Tick,
    Ticks,
}

impl DampenUnit {
    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "ns" => Some(Self::Ns),
            "us" => Some(Self::Us),
            "ms" => Some(Self::Ms),
            "s" => Some(Self::S),
            "tick" => Some(Self::Tick),
            "ticks" => Some(Self::Ticks),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Ns => "ns",
            Self::Us => "us",
            Self::Ms => "ms",
            Self::S => "s",
            Self::Tick => "tick",
            Self::Ticks => "ticks",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ResonancePlan {
    pub name: String,
    pub target: ResonanceTarget,
    pub dampen: DampenWindow,
    pub direct_mutation_paths: Vec<String>,
}

impl ResonancePlan {
    pub fn new(
        name: impl Into<String>,
        target: ResonanceTarget,
        dampen: DampenWindow,
        mut direct_mutation_paths: Vec<String>,
    ) -> Self {
        direct_mutation_paths.sort();
        direct_mutation_paths.dedup();
        Self {
            name: name.into(),
            target,
            dampen,
            direct_mutation_paths,
        }
    }

    pub fn directly_mutates_target(&self) -> bool {
        let target = self.target.authored_path();
        self.direct_mutation_paths
            .iter()
            .any(|path| path == &target || path.starts_with(&(target.clone() + ".")))
    }
}

#[derive(Debug, thiserror::Error)]
pub enum ResonateError {
    #[error("invalid resonate target: {0}")]
    InvalidTarget(String),
    #[error("invalid resonate dampen window: {0}")]
    InvalidDampen(String),
}
