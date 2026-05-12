use crate::event::InputEvent;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct InputFrame {
    pub frame_index: u64,
    pub delta_millis: f64,
    pub events: Vec<InputEvent>,
    pub actions_pressed: BTreeSet<String>,
    pub actions_down: BTreeSet<String>,
    pub actions_released: BTreeSet<String>,
    pub axes: BTreeMap<String, f64>,
    pub text_commits: Vec<String>,
    pub source_kinds: BTreeSet<String>,
}

impl InputFrame {
    pub fn new(frame_index: u64, delta_millis: f64) -> Self {
        Self {
            frame_index,
            delta_millis,
            ..Self::default()
        }
    }

    pub fn action_pressed(&self, action: &str) -> bool {
        self.actions_pressed.contains(action)
    }

    pub fn action_down(&self, action: &str) -> bool {
        self.actions_down.contains(action)
    }

    pub fn action_released(&self, action: &str) -> bool {
        self.actions_released.contains(action)
    }

    pub fn axis_value(&self, axis: &str) -> f64 {
        self.axes.get(axis).copied().unwrap_or(0.0)
    }
}
