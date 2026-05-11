use crate::lifecycle::DEFAULT_ASK_TIMEOUT_MS;
use serde::{Deserialize, Serialize};
use std::collections::HashSet;

pub type MessageName = String;

/// Runtime message envelope.
///
/// The generic payload lets `kain-core` use interpreter `Value`s while native
/// runtimes can use ABI-safe payload descriptors without duplicating the
/// message transport shape.
#[derive(Debug, Clone)]
pub struct MessageEnvelope<T> {
    pub name: MessageName,
    pub args: Vec<T>,
}

impl<T> MessageEnvelope<T> {
    pub fn new(name: impl Into<MessageName>, args: Vec<T>) -> Self {
        Self {
            name: name.into(),
            args,
        }
    }

    pub fn arity(&self) -> usize {
        self.args.len()
    }

    pub fn is_system_message(&self) -> bool {
        self.name.starts_with("__kain_actor_")
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MessageParameter {
    pub name: String,
    pub type_name: String,
    pub required: bool,
}

impl MessageParameter {
    pub fn required(name: impl Into<String>, type_name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            type_name: type_name.into(),
            required: true,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MessageReplyContract {
    pub type_name: String,
    pub timeout_ms: u64,
}

impl MessageReplyContract {
    pub fn new(type_name: impl Into<String>, timeout_ms: u64) -> Self {
        Self {
            type_name: type_name.into(),
            timeout_ms,
        }
    }
}

impl Default for MessageReplyContract {
    fn default() -> Self {
        Self {
            type_name: "Unit".to_string(),
            timeout_ms: DEFAULT_ASK_TIMEOUT_MS,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DeliverySemantics {
    Cast,
    Call,
    System,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MessageSignature {
    pub name: MessageName,
    pub parameters: Vec<MessageParameter>,
    pub reply: Option<MessageReplyContract>,
    pub delivery: DeliverySemantics,
}

impl MessageSignature {
    pub fn cast(name: impl Into<MessageName>, parameters: Vec<MessageParameter>) -> Self {
        Self {
            name: name.into(),
            parameters,
            reply: None,
            delivery: DeliverySemantics::Cast,
        }
    }

    pub fn call(
        name: impl Into<MessageName>,
        parameters: Vec<MessageParameter>,
        reply: MessageReplyContract,
    ) -> Self {
        Self {
            name: name.into(),
            parameters,
            reply: Some(reply),
            delivery: DeliverySemantics::Call,
        }
    }

    pub fn arity(&self) -> usize {
        self.parameters.len()
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct MessageCatalog {
    pub messages: Vec<MessageSignature>,
}

impl MessageCatalog {
    pub fn new(messages: Vec<MessageSignature>) -> Self {
        Self { messages }
    }

    pub fn find(&self, name: &str) -> Option<&MessageSignature> {
        self.messages.iter().find(|message| message.name == name)
    }

    pub fn names(&self) -> impl Iterator<Item = &str> {
        self.messages.iter().map(|message| message.name.as_str())
    }

    pub fn duplicate_names(&self) -> Vec<String> {
        let mut seen = HashSet::new();
        let mut duplicates = Vec::new();
        for name in self.names() {
            if !seen.insert(name.to_string()) && !duplicates.iter().any(|item| item == name) {
                duplicates.push(name.to_string());
            }
        }
        duplicates
    }
}
