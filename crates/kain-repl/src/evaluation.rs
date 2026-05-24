use kain_core::{diagnostics::Diagnostics, CompileTarget};
use kain_driver::DriverSession;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ReplEvaluation {
    pub visible_value: Option<String>,
    pub execution_complete: bool,
}

impl ReplEvaluation {
    pub fn from_interpret_output(output: String) -> Self {
        let trimmed = output.trim();
        let visible_value = if trimmed.is_empty() || trimmed == "()" {
            None
        } else {
            Some(output)
        };

        Self {
            visible_value,
            execution_complete: true,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ReplEvaluationError {
    pub formatted_error: String,
}

impl ReplEvaluationError {
    pub fn plain_text(&self) -> String {
        strip_ansi_sequences(&self.formatted_error)
    }
}

pub type ReplEvaluationResult = Result<ReplEvaluation, ReplEvaluationError>;

#[derive(Debug, Clone, Default)]
pub struct ReplEvaluator {
    driver: DriverSession,
}

impl ReplEvaluator {
    pub fn new(driver: DriverSession) -> Self {
        Self { driver }
    }

    pub fn evaluate_interpret_source(
        &self,
        source_name: &str,
        source: &str,
    ) -> ReplEvaluationResult {
        match self.driver.compile(source, CompileTarget::Interpret) {
            Ok(output) => Ok(ReplEvaluation::from_interpret_output(output)),
            Err(error) => {
                let diagnostics = Diagnostics::new(source, source_name);
                Err(ReplEvaluationError {
                    formatted_error: diagnostics.format_error(&error),
                })
            }
        }
    }
}

fn strip_ansi_sequences(input: &str) -> String {
    let mut output = String::with_capacity(input.len());
    let mut chars = input.chars().peekable();

    while let Some(ch) = chars.next() {
        if ch != '\u{1b}' {
            output.push(ch);
            continue;
        }

        if !matches!(chars.peek(), Some('[')) {
            continue;
        }

        chars.next();
        while let Some(next) = chars.next() {
            if ('@'..='~').contains(&next) {
                break;
            }
        }
    }

    output
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hides_unit_and_empty_outputs() {
        assert_eq!(
            ReplEvaluation::from_interpret_output("()".to_string()),
            ReplEvaluation {
                visible_value: None,
                execution_complete: true,
            }
        );
        assert_eq!(
            ReplEvaluation::from_interpret_output("\n".to_string()).visible_value,
            None
        );
    }

    #[test]
    fn preserves_non_unit_output() {
        assert_eq!(
            ReplEvaluation::from_interpret_output("42".to_string()).visible_value,
            Some("42".to_string())
        );
    }

    #[test]
    fn strips_ansi_sequences_for_plain_text_errors() {
        let error = ReplEvaluationError {
            formatted_error: "\u{1b}[31merror\u{1b}[0m: bad news".to_string(),
        };

        assert_eq!(error.plain_text(), "error: bad news");
    }
}
