use crate::command::ReplDirective;
use crate::source::normalize_script_source;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ReplLineAction {
    Continue,
    Exit,
    Clear,
    Help,
    Evaluate(String),
}

#[derive(Debug, Clone, Default)]
pub struct ReplSession {
    buffer: String,
}

impl ReplSession {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn prompt(&self) -> &'static str {
        if self.buffer.trim().is_empty() {
            ">>> "
        } else {
            "... "
        }
    }

    pub fn buffered_source(&self) -> &str {
        &self.buffer
    }

    pub fn clear(&mut self) {
        self.buffer.clear();
    }

    pub fn accept_raw_line(&mut self, raw_line: &str) -> ReplLineAction {
        let trimmed = raw_line.trim_end_matches(['\r', '\n']);

        match ReplDirective::parse(trimmed) {
            Some(ReplDirective::Exit) => return ReplLineAction::Exit,
            Some(ReplDirective::Clear) => {
                self.clear();
                return ReplLineAction::Clear;
            }
            Some(ReplDirective::Run) => {
                return self.take_buffer_for_evaluation_if_present();
            }
            Some(ReplDirective::Help) => return ReplLineAction::Help,
            None => {}
        }

        if trimmed.is_empty() {
            return self.take_buffer_for_evaluation_if_present();
        }

        self.buffer.push_str(raw_line);
        ReplLineAction::Continue
    }

    pub fn finish_input(&mut self) -> Option<String> {
        if self.buffer.trim().is_empty() {
            None
        } else {
            Some(self.take_normalized_buffer())
        }
    }

    fn take_buffer_for_evaluation_if_present(&mut self) -> ReplLineAction {
        if self.buffer.trim().is_empty() {
            ReplLineAction::Continue
        } else {
            ReplLineAction::Evaluate(self.take_normalized_buffer())
        }
    }

    fn take_normalized_buffer(&mut self) -> String {
        normalize_script_source(std::mem::take(&mut self.buffer))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn prompt_tracks_buffer_state() {
        let mut session = ReplSession::new();
        assert_eq!(session.prompt(), ">>> ");
        assert_eq!(
            session.accept_raw_line("fn main() -> Int:\n"),
            ReplLineAction::Continue
        );
        assert_eq!(session.prompt(), "... ");
    }

    #[test]
    fn blank_line_evaluates_non_empty_buffer() {
        let mut session = ReplSession::new();
        session.accept_raw_line("fn main() -> Int:\n");
        session.accept_raw_line("    return 7\n");
        assert_eq!(
            session.accept_raw_line("\n"),
            ReplLineAction::Evaluate("fn main() -> Int:\n    return 7\n".to_string())
        );
        assert_eq!(session.prompt(), ">>> ");
    }

    #[test]
    fn clear_discards_buffer_without_evaluation() {
        let mut session = ReplSession::new();
        session.accept_raw_line("fn main() -> Int:\n");
        assert_eq!(session.accept_raw_line(".clear\n"), ReplLineAction::Clear);
        assert!(session.buffered_source().is_empty());
    }
}
