use crate::{InputBindingMap, InputEvent, InputEventKind, InputFrame, InputTrace};
use serde::{Deserialize, Serialize};
use std::collections::BTreeSet;
use thiserror::Error;

pub type InputResult<T> = Result<T, InputError>;

#[derive(Debug, Error, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum InputError {
    #[error("input session id must be positive")]
    InvalidSessionId,
    #[error("input action must not be empty")]
    EmptyAction,
    #[error("input axis must not be empty")]
    EmptyAxis,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InputSession {
    pub id: i64,
    pub name: String,
    pub frame_index: u64,
    pub binding_map: InputBindingMap,
    pending_events: Vec<InputEvent>,
    current_frame: InputFrame,
    actions_down: BTreeSet<String>,
    next_sequence: u64,
    trace: InputTrace,
}

impl InputSession {
    pub fn new(id: i64, name: impl Into<String>) -> Self {
        Self {
            id,
            name: name.into(),
            frame_index: 0,
            binding_map: InputBindingMap::default(),
            pending_events: Vec::new(),
            current_frame: InputFrame::default(),
            actions_down: BTreeSet::new(),
            next_sequence: 1,
            trace: InputTrace::default(),
        }
    }

    pub fn bind_action(
        &mut self,
        source_kind: impl Into<String>,
        event_kind: impl Into<String>,
        code: impl Into<String>,
        action: impl Into<String>,
    ) {
        self.binding_map
            .bind_action(source_kind, event_kind, code, action);
    }

    pub fn bind_axis(
        &mut self,
        source_kind: impl Into<String>,
        event_kind: impl Into<String>,
        code: impl Into<String>,
        axis: impl Into<String>,
        scale: f64,
    ) {
        self.binding_map
            .bind_axis(source_kind, event_kind, code, axis, scale);
    }

    pub fn push_event(&mut self, mut event: InputEvent) -> u64 {
        if event.sequence == 0 {
            event.sequence = self.next_sequence;
            self.next_sequence += 1;
        } else {
            self.next_sequence = self.next_sequence.max(event.sequence + 1);
        }
        let sequence = event.sequence;
        self.pending_events.push(event);
        sequence
    }

    pub fn begin_frame(&mut self, delta_millis: f64) -> InputFrame {
        self.frame_index += 1;
        let mut frame = InputFrame::new(self.frame_index, delta_millis);
        let events = std::mem::take(&mut self.pending_events);

        for event in events {
            frame.source_kinds.insert(event.source.kind.clone());
            self.reduce_event(&mut frame, &event);
            frame.events.push(event);
        }

        frame.actions_down = self.actions_down.clone();
        self.current_frame = frame.clone();
        self.trace.push_frame(frame.clone());
        frame
    }

    pub fn current_frame(&self) -> &InputFrame {
        &self.current_frame
    }

    pub fn trace(&self) -> &InputTrace {
        &self.trace
    }

    pub fn replay_trace(&mut self, trace: &InputTrace) {
        for frame in &trace.frames {
            for event in &frame.events {
                self.push_event(event.clone());
            }
            self.begin_frame(frame.delta_millis);
        }
    }

    fn reduce_event(&mut self, frame: &mut InputFrame, event: &InputEvent) {
        if event.kind == InputEventKind::Text.as_str() {
            if !event.text.is_empty() {
                frame.text_commits.push(event.text.clone());
            }
        }

        if event.kind == InputEventKind::Axis.as_str() {
            let (axis, scale) = self
                .binding_map
                .resolve_axis(event)
                .map(|(axis, scale)| (axis.to_string(), scale))
                .unwrap_or_else(|| {
                    let axis = if event.axis.is_empty() {
                        event.code.clone()
                    } else {
                        event.axis.clone()
                    };
                    (axis, 1.0)
                });
            if !axis.is_empty() {
                *frame.axes.entry(axis).or_insert(0.0) += event.value * scale;
            }
        }

        let action = self
            .binding_map
            .resolve_action(event)
            .map(ToOwned::to_owned)
            .or_else(|| (!event.action.is_empty()).then(|| event.action.clone()));
        let Some(action) = action else {
            return;
        };
        if action.is_empty() {
            return;
        }

        if event.kind == InputEventKind::KeyUp.as_str()
            || event.kind == InputEventKind::PointerUp.as_str()
            || event.kind == InputEventKind::ActionUp.as_str()
        {
            if self.actions_down.remove(&action) {
                frame.actions_released.insert(action);
            }
            return;
        }

        if event.kind == InputEventKind::Action.as_str() {
            frame.actions_pressed.insert(action);
            return;
        }

        if self.actions_down.insert(action.clone()) {
            frame.actions_pressed.insert(action);
        }
    }
}
