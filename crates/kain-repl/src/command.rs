#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ReplDirective {
    Exit,
    Clear,
    Run,
    Help,
}

pub const REPL_HELP_TEXT: &str = "\
.run    evaluate the buffered Kain source
.clear  discard the current buffer
.exit   leave the REPL
.quit   leave the REPL
.help   show this command list";

impl ReplDirective {
    pub fn parse(trimmed_line: &str) -> Option<Self> {
        match trimmed_line {
            ".exit" | ".quit" => Some(Self::Exit),
            ".clear" => Some(Self::Clear),
            ".run" => Some(Self::Run),
            ".help" => Some(Self::Help),
            _ => None,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_known_directives() {
        assert_eq!(ReplDirective::parse(".exit"), Some(ReplDirective::Exit));
        assert_eq!(ReplDirective::parse(".quit"), Some(ReplDirective::Exit));
        assert_eq!(ReplDirective::parse(".clear"), Some(ReplDirective::Clear));
        assert_eq!(ReplDirective::parse(".run"), Some(ReplDirective::Run));
        assert_eq!(ReplDirective::parse(".help"), Some(ReplDirective::Help));
    }

    #[test]
    fn leaves_source_like_dot_lines_for_the_language() {
        assert_eq!(ReplDirective::parse(".unknown"), None);
    }
}
