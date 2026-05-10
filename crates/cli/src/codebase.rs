use clap::Subcommand;
use serde_json::json;
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

pub fn run(command: CodebaseCommand) -> Result<(), String> {
    match command {
        CodebaseCommand::Inspect { path, json } => {
            let inspection =
                kain_codebase::inspect_workspace(&path).map_err(|err| err.to_string())?;
            if json {
                let text = serde_json::to_string_pretty(&inspection)
                    .map_err(|err| format!("failed to encode inspection JSON: {err}"))?;
                println!("{text}");
            } else {
                println!("Workspace: {}", inspection.root.display());
                println!("Trust: {}", inspection.trust_mode.as_str());
                println!("Markers: {}", inspection.markers.join(", "));
                println!("KAIN.toml: {}", inspection.has_kain_manifest);
                println!("package.json: {}", inspection.has_package_json);
                println!("Cargo.toml: {}", inspection.has_cargo_manifest);
                println!(".git: {}", inspection.has_git);
            }
            Ok(())
        }
        CodebaseCommand::Run { cwd, command } => {
            let Some((program, args)) = command.split_first() else {
                return Err("codebase run expects a command after --".to_string());
            };
            let args = args.to_vec();
            let result =
                kain_codebase::run_command(&cwd, program, &args).map_err(|err| err.to_string())?;
            let text = serde_json::to_string_pretty(&json!({
                "command": result.command,
                "args": result.args,
                "cwd": result.cwd,
                "status": result.status,
                "success": result.success,
                "stdout": result.stdout,
                "stderr": result.stderr,
            }))
            .map_err(|err| format!("failed to encode command result JSON: {err}"))?;
            println!("{text}");
            Ok(())
        }
    }
}
