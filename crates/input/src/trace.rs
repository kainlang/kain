use crate::frame::InputFrame;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct InputTrace {
    pub frames: Vec<InputFrame>,
}

impl InputTrace {
    pub fn push_frame(&mut self, frame: InputFrame) {
        self.frames.push(frame);
    }

    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string(self)
    }

    pub fn from_json(text: &str) -> Result<Self, serde_json::Error> {
        serde_json::from_str(text)
    }
}
