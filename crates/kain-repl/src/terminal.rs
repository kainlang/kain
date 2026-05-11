use std::io::{self, BufRead, Write};

use crate::command::REPL_HELP_TEXT;
use crate::evaluation::{ReplEvaluation, ReplEvaluator};
use crate::metadata::ReplBuildMetadata;
use crate::session::{ReplLineAction, ReplSession};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ReplTerminalConfig {
    pub metadata: ReplBuildMetadata,
    pub source_name: String,
}

impl ReplTerminalConfig {
    pub fn new(metadata: ReplBuildMetadata) -> Self {
        Self {
            metadata,
            source_name: "<repl>".to_string(),
        }
    }
}

impl Default for ReplTerminalConfig {
    fn default() -> Self {
        Self::new(ReplBuildMetadata::default())
    }
}

pub fn run_terminal_repl(config: ReplTerminalConfig) -> bool {
    let stdin = io::stdin();
    let stdin = stdin.lock();
    let stdout = io::stdout();
    let stderr = io::stderr();
    run_terminal_repl_with_io(stdin, stdout.lock(), stderr.lock(), config)
}

pub fn run_terminal_repl_with_io<R, W, E>(
    mut input: R,
    mut output: W,
    mut error: E,
    config: ReplTerminalConfig,
) -> bool
where
    R: BufRead,
    W: Write,
    E: Write,
{
    if writeln!(output, "{}", config.metadata.banner()).is_err() {
        return false;
    }

    let evaluator = ReplEvaluator::default();
    let mut session = ReplSession::new();
    let mut line = String::new();

    loop {
        if write!(output, "{}", session.prompt())
            .and_then(|_| output.flush())
            .is_err()
        {
            let _ = writeln!(error, " Failed to write REPL prompt.");
            return false;
        }

        line.clear();
        let bytes_read = match input.read_line(&mut line) {
            Ok(value) => value,
            Err(err) => {
                let _ = writeln!(error, " Failed to read REPL input: {err}");
                return false;
            }
        };

        if bytes_read == 0 {
            return finish_on_eof(&mut session, &evaluator, &config, &mut output, &mut error);
        }

        match session.accept_raw_line(&line) {
            ReplLineAction::Continue | ReplLineAction::Clear => {}
            ReplLineAction::Exit => {
                let _ = writeln!(output);
                return true;
            }
            ReplLineAction::Help => {
                if writeln!(output, "{REPL_HELP_TEXT}").is_err() {
                    return false;
                }
            }
            ReplLineAction::Evaluate(source) => {
                if !evaluate_and_write(&evaluator, &config, &source, &mut output, &mut error) {
                    return false;
                }
            }
        }
    }
}

fn finish_on_eof<W, E>(
    session: &mut ReplSession,
    evaluator: &ReplEvaluator,
    config: &ReplTerminalConfig,
    output: &mut W,
    error: &mut E,
) -> bool
where
    W: Write,
    E: Write,
{
    let Some(source) = session.finish_input() else {
        let _ = writeln!(output);
        return true;
    };

    if !evaluate_and_write(evaluator, config, &source, output, error) {
        return false;
    }
    let _ = writeln!(output);
    true
}

fn evaluate_and_write<W, E>(
    evaluator: &ReplEvaluator,
    config: &ReplTerminalConfig,
    source: &str,
    output: &mut W,
    error: &mut E,
) -> bool
where
    W: Write,
    E: Write,
{
    match evaluator.evaluate_interpret_source(&config.source_name, source) {
        Ok(evaluation) => write_evaluation(output, &evaluation).is_ok(),
        Err(failure) => {
            let _ = write!(error, "{}", failure.formatted_error);
            false
        }
    }
}

fn write_evaluation<W>(output: &mut W, evaluation: &ReplEvaluation) -> io::Result<()>
where
    W: Write,
{
    if let Some(value) = evaluation.visible_value.as_deref() {
        writeln!(output, "{value}")?;
    }
    if evaluation.execution_complete {
        writeln!(output, " Execution complete")?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::io::Cursor;

    use super::*;

    #[test]
    fn help_and_exit_run_without_evaluator() {
        let input = Cursor::new(".help\n.exit\n");
        let mut output = Vec::new();
        let mut error = Vec::new();

        assert!(run_terminal_repl_with_io(
            input,
            &mut output,
            &mut error,
            ReplTerminalConfig::default()
        ));

        let output = String::from_utf8(output).expect("output should be utf-8");
        assert!(output.contains(".run"));
        assert!(error.is_empty());
    }

    #[test]
    fn evaluates_buffer_on_blank_line() {
        let input = Cursor::new("fn main() -> Int:\n    return 42\n\n.exit\n");
        let mut output = Vec::new();
        let mut error = Vec::new();

        assert!(run_terminal_repl_with_io(
            input,
            &mut output,
            &mut error,
            ReplTerminalConfig::default()
        ));

        let output = String::from_utf8(output).expect("output should be utf-8");
        assert!(output.contains("42"));
        assert!(output.contains("Execution complete"));
        assert!(error.is_empty());
    }
}
