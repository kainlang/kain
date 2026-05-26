use crate::message::{MessageParameter, MessageSignature};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ActorBehaviorKind {
    BareActor,
    GenServer,
    Supervisor,
    EventStream,
    WorkerPool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorCallbackSignature {
    pub name: String,
    pub params: Vec<MessageParameter>,
    pub return_type: String,
}

impl ActorCallbackSignature {
    pub fn new(
        name: impl Into<String>,
        params: Vec<MessageParameter>,
        return_type: impl Into<String>,
    ) -> Self {
        Self {
            name: name.into(),
            params,
            return_type: return_type.into(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ActorBehaviorContract {
    pub kind: ActorBehaviorKind,
    pub callbacks: Vec<ActorCallbackSignature>,
    pub required_messages: Vec<MessageSignature>,
}

impl ActorBehaviorContract {
    pub fn bare_actor() -> Self {
        Self {
            kind: ActorBehaviorKind::BareActor,
            callbacks: Vec::new(),
            required_messages: Vec::new(),
        }
    }

    pub fn gen_server() -> Self {
        Self {
            kind: ActorBehaviorKind::GenServer,
            callbacks: vec![
                ActorCallbackSignature::new("init", Vec::new(), "State"),
                ActorCallbackSignature::new(
                    "handle_call",
                    vec![
                        MessageParameter::required("request", "Request"),
                        MessageParameter::required("state", "State"),
                    ],
                    "GenServerCallOutcome",
                ),
                ActorCallbackSignature::new(
                    "handle_cast",
                    vec![
                        MessageParameter::required("message", "Message"),
                        MessageParameter::required("state", "State"),
                    ],
                    "State",
                ),
                ActorCallbackSignature::new(
                    "handle_info",
                    vec![
                        MessageParameter::required("message", "Message"),
                        MessageParameter::required("state", "State"),
                    ],
                    "State",
                ),
                ActorCallbackSignature::new(
                    "terminate",
                    vec![MessageParameter::required("reason", "ActorExitReason")],
                    "Unit",
                ),
            ],
            required_messages: vec![
                MessageSignature::cast(
                    "Call",
                    vec![
                        MessageParameter::required("reply_to", "ActorRef"),
                        MessageParameter::required("request", "Request"),
                    ],
                ),
                MessageSignature::cast(
                    "Cast",
                    vec![MessageParameter::required("message", "Message")],
                ),
                MessageSignature::cast(
                    "Info",
                    vec![MessageParameter::required("message", "Message")],
                ),
            ],
        }
    }
}
