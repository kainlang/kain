use crate::event::InputEvent;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum InputBindingTarget {
    Action { name: String },
    Axis { name: String, scale: f64 },
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct InputBinding {
    pub source_kind: String,
    pub event_kind: String,
    pub code: String,
    pub target: InputBindingTarget,
}

impl InputBinding {
    pub fn action(
        source_kind: impl Into<String>,
        event_kind: impl Into<String>,
        code: impl Into<String>,
        action: impl Into<String>,
    ) -> Self {
        Self {
            source_kind: source_kind.into(),
            event_kind: event_kind.into(),
            code: code.into(),
            target: InputBindingTarget::Action {
                name: action.into(),
            },
        }
    }

    pub fn axis(
        source_kind: impl Into<String>,
        event_kind: impl Into<String>,
        code: impl Into<String>,
        axis: impl Into<String>,
        scale: f64,
    ) -> Self {
        Self {
            source_kind: source_kind.into(),
            event_kind: event_kind.into(),
            code: code.into(),
            target: InputBindingTarget::Axis {
                name: axis.into(),
                scale,
            },
        }
    }

    pub fn matches(&self, event: &InputEvent) -> bool {
        (self.source_kind.is_empty()
            || self.source_kind == "*"
            || self.source_kind == event.source.kind)
            && self.event_kind == event.kind
            && self.code == event.code
    }
}

#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct InputBindingMap {
    pub bindings: Vec<InputBinding>,
}

impl InputBindingMap {
    pub fn bind_action(
        &mut self,
        source_kind: impl Into<String>,
        event_kind: impl Into<String>,
        code: impl Into<String>,
        action: impl Into<String>,
    ) {
        self.bindings
            .push(InputBinding::action(source_kind, event_kind, code, action));
    }

    pub fn bind_axis(
        &mut self,
        source_kind: impl Into<String>,
        event_kind: impl Into<String>,
        code: impl Into<String>,
        axis: impl Into<String>,
        scale: f64,
    ) {
        self.bindings.push(InputBinding::axis(
            source_kind,
            event_kind,
            code,
            axis,
            scale,
        ));
    }

    pub fn resolve_action<'a>(&'a self, event: &InputEvent) -> Option<&'a str> {
        self.bindings
            .iter()
            .find(|binding| binding.matches(event))
            .and_then(|binding| match &binding.target {
                InputBindingTarget::Action { name } => Some(name.as_str()),
                InputBindingTarget::Axis { .. } => None,
            })
    }

    pub fn resolve_axis<'a>(&'a self, event: &InputEvent) -> Option<(&'a str, f64)> {
        self.bindings
            .iter()
            .find(|binding| binding.matches(event))
            .and_then(|binding| match &binding.target {
                InputBindingTarget::Action { .. } => None,
                InputBindingTarget::Axis { name, scale } => Some((name.as_str(), *scale)),
            })
    }
}
