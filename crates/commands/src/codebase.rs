use clap::Subcommand;
use std::path::PathBuf;

#[derive(Subcommand, Debug)]
pub enum CodebaseCommand {
    /// Inspect a workspace root discovered from KAIN.toml/package.json/Cargo.toml/.git
    Inspect {
        /// Path inside or at the workspace
        path: PathBuf,

        /// Emit machine-readable JSON
        #[arg(long)]
        json: bool,
    },

    /// Run a local command inside a trusted workspace and capture structured output
    Run {
        /// Workspace directory used as the command cwd
        cwd: PathBuf,

        /// Command and args. Use `--` before this vector.
        #[arg(last = true, required = true)]
        command: Vec<String>,
    },
}
