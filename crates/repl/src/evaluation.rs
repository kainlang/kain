use kain_core::{diagnostics::Diagnostics, CompileTarget};
use kain_driver::DriverSession;
use kain_run::{execute_inline_kain_source, render_compact_output, InlineKainSourceRequest};
use serde_json::json;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ReplExecutionMode {
    NativeLlvm,
    #[cfg_attr(not(test), allow(dead_code))]
    Interpret,
}

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

    pub fn from_native_report(report: &kain_run::RunReport) -> Self {
        let compact_output = render_compact_output(report);
        let trimmed = compact_output.trim();
        let visible_value = if !trimmed.is_empty() {
            Some(trimmed.to_string())
        } else {
            report
                .units
                .first()
                .and_then(|unit| unit.exit_code)
                .map(|value| value.to_string())
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

#[derive(Debug, Clone)]
pub struct ReplEvaluator {
    driver: DriverSession,
    mode: ReplExecutionMode,
}

impl Default for ReplEvaluator {
    fn default() -> Self {
        Self {
            driver: DriverSession::default(),
            mode: ReplExecutionMode::NativeLlvm,
        }
    }
}

impl ReplEvaluator {
    pub fn new(driver: DriverSession) -> Self {
        Self {
            driver,
            mode: ReplExecutionMode::NativeLlvm,
        }
    }

    #[cfg(test)]
    pub fn interpret_only_for_testing() -> Self {
        Self {
            driver: DriverSession::default(),
            mode: ReplExecutionMode::Interpret,
        }
    }

    pub fn evaluate_source(&self, source_name: &str, source: &str) -> ReplEvaluationResult {
        match self.mode {
            ReplExecutionMode::NativeLlvm => self.evaluate_native_source(source_name, source),
            ReplExecutionMode::Interpret => self.evaluate_interpret_source(source_name, source),
        }
    }

    pub fn progress_verb(&self) -> &'static str {
        match self.mode {
            ReplExecutionMode::NativeLlvm => "Compiling",
            ReplExecutionMode::Interpret => "Interpreting",
        }
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
                let formatted_error = diagnostics.format_error(&error);
                capture_repl_failure(
                    "interpret",
                    source_name,
                    source,
                    &formatted_error,
                    error.diagnostic_json(),
                );
                Err(ReplEvaluationError { formatted_error })
            }
        }
    }

    fn evaluate_native_source(&self, source_name: &str, source: &str) -> ReplEvaluationResult {
        let cwd = std::env::current_dir().map_err(|err| ReplEvaluationError {
            formatted_error: format!(
                "failed to resolve current working directory for native REPL run: {err}"
            ),
        })?;
        let request = InlineKainSourceRequest::new(source_name, source, cwd);
        match execute_inline_kain_source(&request) {
            Ok(report) if report.is_success() => Ok(ReplEvaluation::from_native_report(&report)),
            Ok(report) => {
                if let Some(evaluation) = native_exit_code_evaluation(&report) {
                    Ok(evaluation)
                } else {
                    let formatted_error = render_compact_output(&report);
                    capture_repl_failure(
                        "native-llvm",
                        source_name,
                        source,
                        &formatted_error,
                        None,
                    );
                    Err(ReplEvaluationError { formatted_error })
                }
            }
            Err(error) => {
                let formatted_error = error.to_string();
                capture_repl_failure("native-llvm", source_name, source, &formatted_error, None);
                Err(ReplEvaluationError { formatted_error })
            }
        }
    }
}

fn native_exit_code_evaluation(report: &kain_run::RunReport) -> Option<ReplEvaluation> {
    let [unit] = report.units.as_slice() else {
        return None;
    };
    if !unit.stdout.trim().is_empty() || !unit.stderr.trim().is_empty() {
        return None;
    }
    let error = unit.error.as_deref()?.trim();
    if !error.starts_with("process exited with status") {
        return None;
    }
    Some(ReplEvaluation {
        visible_value: unit.exit_code.map(|value| value.to_string()),
        execution_complete: true,
    })
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

fn capture_repl_failure(
    mode: &str,
    source_name: &str,
    source: &str,
    formatted_error: &str,
    structured_diagnostic: Option<serde_json::Value>,
) {
    if formatted_error.trim().is_empty() {
        return;
    }
    let _ = kain_core::diagnostic_capture::capture_event_if_enabled(
        kain_core::diagnostic_capture::CapturedDiagnosticEventInput {
            event_kind: "repl-failure".to_string(),
            command: "repl".to_string(),
            argv: std::env::args().collect(),
            cwd: std::env::current_dir()
                .unwrap_or_else(|_| std::path::PathBuf::from("."))
                .display()
                .to_string(),
            launcher: Some("kain".to_string()),
            target: Some(mode.to_string()),
            source_name: Some(source_name.to_string()),
            source_path: None,
            rendered_output: formatted_error.to_string(),
            structured_diagnostic,
            tags: Vec::new(),
            context: json!({
                "mode": mode,
                "source_bytes": source.len(),
                "source_lines": source.lines().count(),
            }),
        },
    );
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
