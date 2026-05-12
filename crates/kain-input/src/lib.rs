//! Typed, replayable input semantics for Kain.
//!
//! This crate owns the target-neutral model. Platform code should translate raw
//! host events into these events, while Kain source consumes frames, actions,
//! axes, text commits, and traces.

mod binding;
mod event;
mod frame;
mod session;
mod source;
mod trace;

pub use binding::{InputBinding, InputBindingMap, InputBindingTarget};
pub use event::{InputEvent, InputEventKind};
pub use frame::InputFrame;
pub use session::{InputError, InputResult, InputSession};
pub use source::{InputSource, InputSourceKind};
pub use trace::InputTrace;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn key_bindings_reduce_into_pressed_down_and_released_actions() {
        let mut session = InputSession::new(1, "input-test");
        session.bind_action(
            InputSourceKind::HumanKeyboard.as_str(),
            InputEventKind::KeyDown.as_str(),
            "Enter",
            "confirm",
        );
        session.bind_action(
            InputSourceKind::HumanKeyboard.as_str(),
            InputEventKind::KeyUp.as_str(),
            "Enter",
            "confirm",
        );

        session.push_event(InputEvent::key_down(
            InputSource::new(InputSourceKind::HumanKeyboard, "keyboard.primary"),
            "Enter",
        ));
        let first = session.begin_frame(16.0);
        assert!(first.action_pressed("confirm"));
        assert!(first.action_down("confirm"));
        assert!(!first.action_released("confirm"));

        session.push_event(InputEvent::key_up(
            InputSource::new(InputSourceKind::HumanKeyboard, "keyboard.primary"),
            "Enter",
        ));
        let second = session.begin_frame(16.0);
        assert!(!second.action_pressed("confirm"));
        assert!(!second.action_down("confirm"));
        assert!(second.action_released("confirm"));
    }

    #[test]
    fn agent_intents_are_first_class_momentary_actions() {
        let mut session = InputSession::new(7, "agent-test");
        session.push_event(InputEvent::agent_intent(
            "codex",
            "confirm",
            "activate the focused command",
            0.92,
        ));

        let frame = session.begin_frame(8.0);
        assert_eq!(frame.events.len(), 1);
        assert_eq!(
            frame.events[0].source.kind,
            InputSourceKind::AgentIntent.as_str()
        );
        assert!(frame.action_pressed("confirm"));
        assert!(!frame.action_down("confirm"));
        assert_eq!(frame.events[0].text, "activate the focused command");
        assert_eq!(frame.events[0].confidence, 0.92);
    }

    #[test]
    fn text_and_axis_bindings_are_frame_local_and_replayable() {
        let mut session = InputSession::new(11, "trace-test");
        session.bind_axis(
            InputSourceKind::HumanPointer.as_str(),
            InputEventKind::Axis.as_str(),
            "look_x",
            "viewport.look_x",
            0.5,
        );
        session.bind_action(
            InputSourceKind::CliStdin.as_str(),
            InputEventKind::Text.as_str(),
            "launch",
            "confirm",
        );

        session.push_event(InputEvent::axis(
            InputSource::new(InputSourceKind::HumanPointer, "mouse.primary"),
            "look_x",
            4.0,
        ));
        session.push_event(InputEvent::text(
            InputSource::new(InputSourceKind::CliStdin, "stdin"),
            "launch",
            "launch",
        ));

        let frame = session.begin_frame(16.0);
        assert_eq!(frame.axis_value("viewport.look_x"), 2.0);
        assert_eq!(frame.text_commits, vec!["launch".to_string()]);
        assert!(frame.action_pressed("confirm"));

        let trace_json = session.trace().to_json().unwrap();
        let trace = InputTrace::from_json(&trace_json).unwrap();
        assert_eq!(trace.frames.len(), 1);
        assert_eq!(trace.frames[0].events.len(), 2);
    }
}
