use crate::id::ActorId;
use serde::{Deserialize, Serialize};
use std::fmt;
use thiserror::Error;

#[derive(Debug, Clone, PartialEq, Eq, Error)]
pub enum ActorAddressError {
    #[error("actor name cannot be empty")]
    EmptyName,
    #[error("actor path must contain at least one segment")]
    EmptyPath,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ActorName(String);

impl ActorName {
    pub fn new(name: impl Into<String>) -> Result<Self, ActorAddressError> {
        let name = name.into();
        if name.trim().is_empty() {
            return Err(ActorAddressError::EmptyName);
        }
        Ok(Self(name))
    }

    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }
}

impl fmt::Display for ActorName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ActorPath {
    pub system: String,
    pub segments: Vec<String>,
}

impl ActorPath {
    pub fn new(
        system: impl Into<String>,
        segments: Vec<String>,
    ) -> Result<Self, ActorAddressError> {
        if segments.is_empty() {
            return Err(ActorAddressError::EmptyPath);
        }
        Ok(Self {
            system: system.into(),
            segments,
        })
    }

    pub fn root(system: impl Into<String>, actor: impl Into<String>) -> Self {
        Self {
            system: system.into(),
            segments: vec![actor.into()],
        }
    }

    pub fn child(&self, actor: impl Into<String>) -> Self {
        let mut segments = self.segments.clone();
        segments.push(actor.into());
        Self {
            system: self.system.clone(),
            segments,
        }
    }
}

impl fmt::Display for ActorPath {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "kain://{}/{}", self.system, self.segments.join("/"))
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorAddress {
    pub id: ActorId,
    pub path: Option<ActorPath>,
    pub name: Option<ActorName>,
}

impl ActorAddress {
    pub fn anonymous(id: ActorId) -> Self {
        Self {
            id,
            path: None,
            name: None,
        }
    }
}
