use std::path::PathBuf;

use clap::{Parser, Subcommand, ValueEnum};
use kade_desktop_controller::{KadeDesktopController, NewSessionRequest, ToolApprovalDecision};

#[derive(Debug, Parser)]
#[command(name = "kade-desktop-controller")]
#[command(about = "Manifest-backed controller for the Kade desktop app")]
struct Cli {
    #[arg(long, default_value = ".")]
    app_root: PathBuf,
    #[command(subcommand)]
    command: Command,
}

#[derive(Debug, Subcommand)]
enum Command {
    Bootstrap,
    Summary,
    GenerateShell,
    CreateSession {
        #[arg(long)]
        title: String,
        #[arg(long)]
        workspace_root: Option<String>,
    },
    AddMessage {
        #[arg(long)]
        session_id: String,
        #[arg(long)]
        role: String,
        #[arg(long)]
        text: String,
    },
    SetProvider {
        #[arg(long)]
        provider: String,
    },
    SetProviderProfile {
        #[arg(long)]
        provider: String,
        #[arg(long)]
        json: Option<String>,
        #[arg(long)]
        file: Option<PathBuf>,
    },
    ClearProviderProfile {
        #[arg(long)]
        provider: String,
    },
    ApproveTool {
        #[arg(long)]
        tool: String,
        #[arg(long, default_value = "workspace")]
        scope: String,
        #[arg(long)]
        decision: ApprovalDecisionArg,
    },
}

#[derive(Debug, Clone, ValueEnum)]
enum ApprovalDecisionArg {
    Allow,
    Deny,
    Ask,
}

impl From<ApprovalDecisionArg> for ToolApprovalDecision {
    fn from(value: ApprovalDecisionArg) -> Self {
        match value {
            ApprovalDecisionArg::Allow => Self::Allow,
            ApprovalDecisionArg::Deny => Self::Deny,
            ApprovalDecisionArg::Ask => Self::Ask,
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let cli = Cli::parse();
    let controller = KadeDesktopController::load(cli.app_root)?;

    match cli.command {
        Command::Bootstrap => {
            let snapshot = controller.bootstrap_state()?;
            println!("{}", serde_json::to_string_pretty(&snapshot)?);
        }
        Command::Summary => {
            let snapshot = controller.write_runtime_snapshot()?;
            println!("{}", serde_json::to_string_pretty(&snapshot)?);
        }
        Command::GenerateShell => {
            controller.bootstrap_state()?;
            let path = controller.write_generated_shell()?;
            println!("{}", path.display());
        }
        Command::CreateSession {
            title,
            workspace_root,
        } => {
            controller.bootstrap_state()?;
            let session = controller.create_session(NewSessionRequest {
                title,
                workspace_root,
            })?;
            println!("{}", serde_json::to_string_pretty(&session)?);
        }
        Command::AddMessage {
            session_id,
            role,
            text,
        } => {
            controller.bootstrap_state()?;
            let session = controller.append_message(&session_id, &role, &text)?;
            println!("{}", serde_json::to_string_pretty(&session)?);
        }
        Command::SetProvider { provider } => {
            controller.bootstrap_state()?;
            let store = controller.set_active_provider(&provider)?;
            println!("{}", serde_json::to_string_pretty(&store)?);
        }
        Command::SetProviderProfile {
            provider,
            json,
            file,
        } => {
            controller.bootstrap_state()?;
            let profile = load_profile_payload(json, file)?;
            let store = controller.set_provider_profile(&provider, profile)?;
            println!("{}", serde_json::to_string_pretty(&store)?);
        }
        Command::ClearProviderProfile { provider } => {
            controller.bootstrap_state()?;
            let store = controller.clear_provider_profile(&provider)?;
            println!("{}", serde_json::to_string_pretty(&store)?);
        }
        Command::ApproveTool {
            tool,
            scope,
            decision,
        } => {
            controller.bootstrap_state()?;
            let store = controller.set_tool_approval(&tool, &scope, decision.into())?;
            println!("{}", serde_json::to_string_pretty(&store)?);
        }
    }

    Ok(())
}

fn load_profile_payload(
    json: Option<String>,
    file: Option<PathBuf>,
) -> Result<serde_json::Value, Box<dyn std::error::Error>> {
    match (json, file) {
        (Some(json), None) => Ok(serde_json::from_str(&json)?),
        (None, Some(file)) => Ok(serde_json::from_str(&std::fs::read_to_string(file)?)?),
        _ => Err("set-provider-profile requires exactly one of --json or --file".into()),
    }
}
