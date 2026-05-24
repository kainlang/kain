//! Kain REPL system.
//!
//! This crate owns the interactive source-buffering and interpret-target
//! evaluation loop used by the `kain` and `kn` launchers. The CLI should stay
//! a thin host that supplies build metadata and process IO.

mod app;
pub mod command;
pub mod evaluation;
mod highlight;
pub mod metadata;
pub mod session;
pub mod source;
pub mod terminal;

pub use command::{ReplDirective, REPL_HELP_TEXT};
pub use evaluation::{ReplEvaluation, ReplEvaluationError, ReplEvaluationResult, ReplEvaluator};
pub use metadata::ReplBuildMetadata;
pub use session::{ReplLineAction, ReplSession};
pub use source::normalize_script_source;
pub use terminal::{run_terminal_repl, run_terminal_repl_with_io, ReplTerminalConfig};
