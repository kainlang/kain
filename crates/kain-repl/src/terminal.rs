use std::fs;
use std::io::{self, BufRead, IsTerminal, Write};
use std::path::PathBuf;

use crate::app::run_tui_repl;
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
    if io::stdin().is_terminal() && io::stdout().is_terminal() {
        return match run_tui_repl(config) {
            Ok(()) => true,
            Err(err) => {
                eprintln!(" REPL TUI failed: {err}");
                false
            }
        };
    }

    let stdin = io::stdin();
    let stdin = stdin.lock();
    let stdout = io::stdout();
    let stderr = io::stderr();
    run_terminal_repl_with_io(stdin, stdout.lock(), stderr.lock(), config)
}

pub fn run_terminal_repl_with_io<R, W, E>(
    input: R,
    output: W,
    error: E,
    config: ReplTerminalConfig,
) -> bool
where
    R: BufRead,
    W: Write,
    E: Write,
{
    run_terminal_repl_with_io_and_evaluator(input, output, error, config, ReplEvaluator::default())
}

fn run_terminal_repl_with_io_and_evaluator<R, W, E>(
    mut input: R,
    mut output: W,
    mut error: E,
    config: ReplTerminalConfig,
    evaluator: ReplEvaluator,
) -> bool
where
    R: BufRead,
    W: Write,
    E: Write,
{
    if writeln!(output, "{}", config.metadata.banner()).is_err() {
        return false;
    }

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
            ReplLineAction::Theme(theme) => {
                let message = match theme {
                    Some(name) => format!("theme {name}"),
                    None => "theme".to_string(),
                };
                if writeln!(output, "{message}").is_err() {
                    return false;
                }
            }
            ReplLineAction::Open(path) => {
                let message = match path {
                    Some(path) => open_file_into_session(&mut session, &path),
                    None => Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "path required for .open",
                    )),
                };
                match message {
                    Ok(path) => {
                        if writeln!(output, "opened {path}").is_err() {
                            return false;
                        }
                    }
                    Err(err) => {
                        if writeln!(error, " open failed: {err}").is_err() {
                            return false;
                        }
                    }
                }
            }
            ReplLineAction::Evaluate(source) => {
                if evaluate_and_write(&evaluator, &config, &source, &mut output, &mut error)
                    .is_err()
                {
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

    if evaluate_and_write(evaluator, config, &source, output, error).is_err() {
        return false;
    };
    let _ = writeln!(output);
    true
}

fn evaluate_and_write<W, E>(
    evaluator: &ReplEvaluator,
    config: &ReplTerminalConfig,
    source: &str,
    output: &mut W,
    error: &mut E,
) -> io::Result<()>
where
    W: Write,
    E: Write,
{
    match evaluator.evaluate_source(&config.source_name, source) {
        Ok(evaluation) => write_evaluation(output, &evaluation),
        Err(failure) => write!(error, "{}", failure.formatted_error),
    }
}

fn open_file_into_session(session: &mut ReplSession, raw_path: &str) -> io::Result<String> {
    let path = if PathBuf::from(raw_path).is_absolute() {
        PathBuf::from(raw_path)
    } else {
        std::env::current_dir()
            .unwrap_or_else(|_| PathBuf::from("."))
            .join(raw_path)
    };
    let source = fs::read_to_string(&path)?;
    session.replace_buffer(source);
    Ok(path.display().to_string())
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

        assert!(run_terminal_repl_with_io_and_evaluator(
            input,
            &mut output,
            &mut error,
            ReplTerminalConfig::default(),
            ReplEvaluator::interpret_only_for_testing(),
        ));

        let output = String::from_utf8(output).expect("output should be utf-8");
        assert!(output.contains("42"));
        assert!(output.contains("Execution complete"));
        assert!(error.is_empty());
    }

    #[test]
    fn compile_errors_do_not_eject_plain_repl_sessions() {
        let input = Cursor::new(
            "fn main() -> Int:\n    return missing_name\n\nfn main() -> Int:\n    return 7\n\n.exit\n",
        );
        let mut output = Vec::new();
        let mut error = Vec::new();

        assert!(run_terminal_repl_with_io_and_evaluator(
            input,
            &mut output,
            &mut error,
            ReplTerminalConfig::default(),
            ReplEvaluator::interpret_only_for_testing(),
        ));

        let output = String::from_utf8(output).expect("output should be utf-8");
        let error = String::from_utf8(error).expect("error should be utf-8");
        assert!(output.contains("7"));
        assert!(output.contains("Execution complete"));
        assert!(error.contains("error"));
    }
}
