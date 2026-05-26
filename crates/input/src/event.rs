use crate::source::{InputSource, InputSourceKind};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum InputEventKind {
    KeyDown,
    KeyUp,
    Text,
    PointerDown,
    PointerUp,
    PointerMove,
    Axis,
    Action,
    ActionDown,
    ActionUp,
    Lifecycle,
}

impl InputEventKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::KeyDown => "key_down",
            Self::KeyUp => "key_up",
            Self::Text => "text",
            Self::PointerDown => "pointer_down",
            Self::PointerUp => "pointer_up",
            Self::PointerMove => "pointer_move",
            Self::Axis => "axis",
            Self::Action => "action",
            Self::ActionDown => "action_down",
            Self::ActionUp => "action_up",
            Self::Lifecycle => "lifecycle",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct InputEvent {
    pub sequence: u64,
    pub timestamp_millis: u64,
    pub source: InputSource,
    pub kind: String,
    pub code: String,
    pub action: String,
    pub axis: String,
    pub value: f64,
    pub x: f64,
    pub y: f64,
    pub delta_x: f64,
    pub delta_y: f64,
    pub text: String,
    pub confidence: f64,
}

impl InputEvent {
    pub fn new(source: InputSource, kind: InputEventKind) -> Self {
        Self {
            sequence: 0,
            timestamp_millis: 0,
            source,
            kind: kind.as_str().to_string(),
            code: String::new(),
            action: String::new(),
            axis: String::new(),
            value: 0.0,
            x: 0.0,
            y: 0.0,
            delta_x: 0.0,
            delta_y: 0.0,
            text: String::new(),
            confidence: 1.0,
        }
    }

    pub fn key_down(source: InputSource, code: impl Into<String>) -> Self {
        let mut event = Self::new(source, InputEventKind::KeyDown);
        event.code = code.into();
        event.value = 1.0;
        event
    }

    pub fn key_up(source: InputSource, code: impl Into<String>) -> Self {
        let mut event = Self::new(source, InputEventKind::KeyUp);
        event.code = code.into();
        event.value = 0.0;
        event
    }

    pub fn text(source: InputSource, code: impl Into<String>, text: impl Into<String>) -> Self {
        let mut event = Self::new(source, InputEventKind::Text);
        event.code = code.into();
        event.text = text.into();
        event.value = 1.0;
        event
    }

    pub fn axis(source: InputSource, code: impl Into<String>, value: f64) -> Self {
        let mut event = Self::new(source, InputEventKind::Axis);
        event.code = code.into();
        event.value = value;
        event
    }

    pub fn action(action: impl Into<String>, source: InputSource) -> Self {
        let mut event = Self::new(source, InputEventKind::Action);
        event.action = action.into();
        event.value = 1.0;
        event
    }

    pub fn agent_intent(
        source_id: impl Into<String>,
        action: impl Into<String>,
        command_text: impl Into<String>,
        confidence: f64,
    ) -> Self {
        let mut event = Self::action(
            action,
            InputSource::custom(InputSourceKind::AgentIntent.as_str(), source_id),
        );
        event.text = command_text.into();
        event.confidence = confidence;
        event
    }
}
