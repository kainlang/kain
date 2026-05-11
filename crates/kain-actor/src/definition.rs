use crate::lifecycle::ActorLifecyclePolicy;
use crate::mailbox::MailboxPolicy;
use crate::message::{MessageCatalog, MessageParameter, MessageSignature};
use crate::supervision::SupervisorSpec;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ActorCapability {
    Send,
    Ask,
    Link,
    Monitor,
    Supervise,
    NativeRuntime,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorStateSlot {
    pub name: String,
    pub type_name: String,
    pub mutable: bool,
    pub persistent: bool,
}

impl ActorStateSlot {
    pub fn new(name: impl Into<String>, type_name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            type_name: type_name.into(),
            mutable: true,
            persistent: true,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorHandlerSignature {
    pub message: MessageSignature,
    pub effects: Vec<String>,
}

impl ActorHandlerSignature {
    pub fn cast(message: MessageSignature) -> Self {
        Self {
            message,
            effects: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorMethodSignature {
    pub name: String,
    pub params: Vec<MessageParameter>,
    pub return_type: String,
    pub effects: Vec<String>,
}

impl ActorMethodSignature {
    pub fn new(
        name: impl Into<String>,
        params: Vec<MessageParameter>,
        return_type: impl Into<String>,
        effects: Vec<String>,
    ) -> Self {
        Self {
            name: name.into(),
            params,
            return_type: return_type.into(),
            effects,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorDefinition {
    pub name: String,
    pub state: Vec<ActorStateSlot>,
    pub handlers: Vec<ActorHandlerSignature>,
    pub methods: Vec<ActorMethodSignature>,
    pub mailbox: MailboxPolicy,
    pub lifecycle: ActorLifecyclePolicy,
    pub supervisor: Option<SupervisorSpec>,
    pub capabilities: Vec<ActorCapability>,
}

impl ActorDefinition {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            state: Vec::new(),
            handlers: Vec::new(),
            methods: Vec::new(),
            mailbox: MailboxPolicy::default(),
            lifecycle: ActorLifecyclePolicy::default(),
            supervisor: None,
            capabilities: vec![
                ActorCapability::Send,
                ActorCapability::Ask,
                ActorCapability::Monitor,
            ],
        }
    }

    pub fn message_catalog(&self) -> MessageCatalog {
        MessageCatalog::new(
            self.handlers
                .iter()
                .map(|handler| handler.message.clone())
                .collect(),
        )
    }

    pub fn handler(&self, message_name: &str) -> Option<&ActorHandlerSignature> {
        self.handlers
            .iter()
            .find(|handler| handler.message.name == message_name)
    }

    pub fn state_slot(&self, name: &str) -> Option<&ActorStateSlot> {
        self.state.iter().find(|slot| slot.name == name)
    }
}
