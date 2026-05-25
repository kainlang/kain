#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ReplDirective {
    Exit,
    Clear,
    Run,
    Help,
    Theme,
    Open,
}

pub const REPL_HELP_TEXT: &str = "\
.run             run buffer
.clear           clear buffer
.quit            quit
.help            command list
.theme           theme status
.theme <name>    switch theme
.open <path>     open file";

impl ReplDirective {
    pub fn parse(trimmed_line: &str) -> Option<Self> {
        match trimmed_line {
            ".exit" | ".quit" => Some(Self::Exit),
            ".clear" => Some(Self::Clear),
            ".run" => Some(Self::Run),
            ".help" => Some(Self::Help),
            _ if trimmed_line == ".theme" || trimmed_line.starts_with(".theme ") => {
                Some(Self::Theme)
            }
            _ if trimmed_line == ".open" || trimmed_line.starts_with(".open ") => Some(Self::Open),
            _ => None,
        }
    }
}

pub fn parse_theme_argument(trimmed_line: &str) -> Option<Option<String>> {
    if ReplDirective::parse(trimmed_line) != Some(ReplDirective::Theme) {
        return None;
    }
    let rest = trimmed_line
        .strip_prefix(".theme")
        .expect("theme directive prefix")
        .trim();
    if rest.is_empty() {
        Some(None)
    } else {
        Some(Some(rest.to_string()))
    }
}

pub fn parse_open_argument(trimmed_line: &str) -> Option<Option<String>> {
    if ReplDirective::parse(trimmed_line) != Some(ReplDirective::Open) {
        return None;
    }
    let rest = trimmed_line
        .strip_prefix(".open")
        .expect("open directive prefix")
        .trim();
    if rest.is_empty() {
        Some(None)
    } else {
        Some(Some(rest.to_string()))
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
        assert_eq!(ReplDirective::parse(".theme"), Some(ReplDirective::Theme));
        assert_eq!(ReplDirective::parse(".open"), Some(ReplDirective::Open));
        assert_eq!(
            ReplDirective::parse(".theme plain"),
            Some(ReplDirective::Theme)
        );
        assert_eq!(
            ReplDirective::parse(".open demo.kn"),
            Some(ReplDirective::Open)
        );
    }

    #[test]
    fn leaves_source_like_dot_lines_for_the_language() {
        assert_eq!(ReplDirective::parse(".unknown"), None);
    }

    #[test]
    fn parses_theme_argument_when_present() {
        assert_eq!(parse_theme_argument(".theme"), Some(None));
        assert_eq!(
            parse_theme_argument(".theme graphite"),
            Some(Some("graphite".to_string()))
        );
    }

    #[test]
    fn parses_open_argument_when_present() {
        assert_eq!(parse_open_argument(".open"), Some(None));
        assert_eq!(
            parse_open_argument(".open demo.kn"),
            Some(Some("demo.kn".to_string()))
        );
    }
}
