use crate::definition::ActorDefinition;
use crate::message::MessageParameter;
use std::collections::HashSet;
use thiserror::Error;

pub type ActorValidationResult<T> = Result<T, ActorValidationError>;

#[derive(Debug, Clone, PartialEq, Eq, Error)]
pub enum ActorValidationError {
    #[error("actor name cannot be empty")]
    EmptyActorName,
    #[error("actor `{actor}` has duplicate state slot `{slot}`")]
    DuplicateStateSlot { actor: String, slot: String },
    #[error("actor `{actor}` has duplicate handler for message `{message}`")]
    DuplicateHandler { actor: String, message: String },
    #[error("actor `{actor}` has duplicate method `{method}`")]
    DuplicateMethod { actor: String, method: String },
    #[error("actor `{actor}` handler `{message}` has an empty message name")]
    EmptyMessageName { actor: String, message: String },
    #[error("actor `{actor}` callable `{callable}` has duplicate parameter `{parameter}`")]
    DuplicateParameter {
        actor: String,
        callable: String,
        parameter: String,
    },
    #[error("actor `{actor}` callable `{callable}` parameter `{parameter}` has no type")]
    EmptyParameterType {
        actor: String,
        callable: String,
        parameter: String,
    },
    #[error("actor `{actor}` has an invalid bounded mailbox capacity")]
    InvalidMailboxCapacity { actor: String },
    #[error("supervisor `{supervisor}` has duplicate child id `{child}`")]
    DuplicateSupervisorChild { supervisor: String, child: String },
    #[error("supervisor `{supervisor}` restart intensity must allow at least one restart")]
    InvalidRestartIntensity { supervisor: String },
}

#[derive(Debug, Default)]
pub struct ActorModelValidator;

impl ActorModelValidator {
    pub fn validate(definition: &ActorDefinition) -> ActorValidationResult<()> {
        if definition.name.trim().is_empty() {
            return Err(ActorValidationError::EmptyActorName);
        }

        if !definition.mailbox.capacity.is_valid() {
            return Err(ActorValidationError::InvalidMailboxCapacity {
                actor: definition.name.clone(),
            });
        }

        let mut state_names = HashSet::new();
        for slot in &definition.state {
            if !state_names.insert(slot.name.clone()) {
                return Err(ActorValidationError::DuplicateStateSlot {
                    actor: definition.name.clone(),
                    slot: slot.name.clone(),
                });
            }
        }

        let mut handler_names = HashSet::new();
        for handler in &definition.handlers {
            if handler.message.name.trim().is_empty() {
                return Err(ActorValidationError::EmptyMessageName {
                    actor: definition.name.clone(),
                    message: handler.message.name.clone(),
                });
            }
            if !handler_names.insert(handler.message.name.clone()) {
                return Err(ActorValidationError::DuplicateHandler {
                    actor: definition.name.clone(),
                    message: handler.message.name.clone(),
                });
            }
            validate_parameters(
                &definition.name,
                &handler.message.name,
                &handler.message.parameters,
            )?;
        }

        let mut method_names = HashSet::new();
        for method in &definition.methods {
            if !method_names.insert(method.name.clone()) {
                return Err(ActorValidationError::DuplicateMethod {
                    actor: definition.name.clone(),
                    method: method.name.clone(),
                });
            }
            validate_parameters(&definition.name, &method.name, &method.params)?;
        }

        if let Some(supervisor) = &definition.supervisor {
            if supervisor.restart_intensity.max_restarts == 0 {
                return Err(ActorValidationError::InvalidRestartIntensity {
                    supervisor: supervisor.name.clone(),
                });
            }

            let mut child_ids = HashSet::new();
            for child in &supervisor.children {
                if !child_ids.insert(child.id.clone()) {
                    return Err(ActorValidationError::DuplicateSupervisorChild {
                        supervisor: supervisor.name.clone(),
                        child: child.id.clone(),
                    });
                }
            }
        }

        Ok(())
    }
}

pub fn validate_actor_definition(definition: &ActorDefinition) -> ActorValidationResult<()> {
    ActorModelValidator::validate(definition)
}

fn validate_parameters(
    actor: &str,
    callable: &str,
    params: &[MessageParameter],
) -> ActorValidationResult<()> {
    let mut names = HashSet::new();
    for param in params {
        if !names.insert(param.name.clone()) {
            return Err(ActorValidationError::DuplicateParameter {
                actor: actor.to_string(),
                callable: callable.to_string(),
                parameter: param.name.clone(),
            });
        }
        if param.type_name.trim().is_empty() {
            return Err(ActorValidationError::EmptyParameterType {
                actor: actor.to_string(),
                callable: callable.to_string(),
                parameter: param.name.clone(),
            });
        }
    }
    Ok(())
}
