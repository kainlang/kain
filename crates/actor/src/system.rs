use crate::definition::ActorDefinition;
use crate::scheduler::ActorSchedulerPolicy;
use crate::supervision::SupervisorSpec;
use crate::validation::{validate_actor_definition, ActorValidationError};
use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use thiserror::Error;

pub type ActorSystemValidationResult<T> = Result<T, ActorSystemValidationError>;

#[derive(Debug, Clone, PartialEq, Eq, Error)]
pub enum ActorSystemValidationError {
    #[error("actor system name cannot be empty")]
    EmptySystemName,
    #[error("actor system `{system}` defines duplicate actor `{actor}`")]
    DuplicateActor { system: String, actor: String },
    #[error("actor system `{system}` has invalid actor `{actor}`: {source}")]
    InvalidActor {
        system: String,
        actor: String,
        source: ActorValidationError,
    },
    #[error("root supervisor `{supervisor}` references unknown actor type `{actor_type}`")]
    UnknownSupervisorChild {
        supervisor: String,
        actor_type: String,
    },
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorSystemDefinition {
    pub name: String,
    pub actors: Vec<ActorDefinition>,
    pub scheduler: ActorSchedulerPolicy,
    pub root_supervisor: Option<SupervisorSpec>,
}

impl ActorSystemDefinition {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            actors: Vec::new(),
            scheduler: ActorSchedulerPolicy::default(),
            root_supervisor: None,
        }
    }

    pub fn validate(&self) -> ActorSystemValidationResult<()> {
        if self.name.trim().is_empty() {
            return Err(ActorSystemValidationError::EmptySystemName);
        }

        let mut actor_names = HashSet::new();
        for actor in &self.actors {
            if !actor_names.insert(actor.name.clone()) {
                return Err(ActorSystemValidationError::DuplicateActor {
                    system: self.name.clone(),
                    actor: actor.name.clone(),
                });
            }
            validate_actor_definition(actor).map_err(|source| {
                ActorSystemValidationError::InvalidActor {
                    system: self.name.clone(),
                    actor: actor.name.clone(),
                    source,
                }
            })?;
        }

        if let Some(supervisor) = &self.root_supervisor {
            for child in &supervisor.children {
                if !actor_names.contains(&child.actor_type) {
                    return Err(ActorSystemValidationError::UnknownSupervisorChild {
                        supervisor: supervisor.name.clone(),
                        actor_type: child.actor_type.clone(),
                    });
                }
            }
        }

        Ok(())
    }
}
