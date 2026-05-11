use clap::{Parser, Subcommand};
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(name = "blade")]
#[command(about = "Standalone Kain blade workspace tool")]
struct BladeCli {
    #[command(subcommand)]
    command: BladeCommand,
}

#[derive(Subcommand, Debug)]
enum BladeCommand {
    /// Build the full local blade workspace
    Build {
        /// Path inside the workspace to build
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Build profile used for artifact layout and tool adapters
        #[arg(long)]
        profile: Option<String>,

        /// Target lane label used for artifact layout
        #[arg(long)]
        target: Option<String>,

        /// Print the resolved task graph without executing it
        #[arg(long)]
        dry_run: bool,

        /// Clean .kain build/cache/report roots before executing
        #[arg(long)]
        clean: bool,

        /// Also run GPU Fabric manifests that dispatch Vulkan compute
        #[arg(long)]
        include_vulkan: bool,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// List blades discovered from the current workspace
    List {
        /// Path inside the workspace to inspect
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Print the local blade dependency graph
    Graph {
        /// Path inside the workspace to inspect
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Validate blade manifests and referenced local artifacts
    Check {
        /// Path inside the workspace to inspect
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Equip a local blade by name and print its resolved build/import plan
    Equip {
        /// Blade name to resolve
        blade: String,

        /// Path inside the workspace to inspect
        #[arg(long, default_value = ".")]
        path: PathBuf,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },
}

fn main() {
    let cli = BladeCli::parse();
    if let Err(error) = run(cli.command) {
        eprintln!("Blade command failed: {error}");
        std::process::exit(1);
    }
}

fn run(command: BladeCommand) -> Result<(), String> {
    match command {
        BladeCommand::Build {
            path,
            profile,
            target,
            dry_run,
            clean,
            include_vulkan,
            json,
        } => cli::blades::run_build(path, profile, target, dry_run, clean, include_vulkan, json),
        BladeCommand::List { path, json } => {
            cli::blades::run(cli::blades::BladesCommand::List { path, json })
        }
        BladeCommand::Graph { path, json } => {
            cli::blades::run(cli::blades::BladesCommand::Graph { path, json })
        }
        BladeCommand::Check { path, json } => {
            cli::blades::run(cli::blades::BladesCommand::Check { path, json })
        }
        BladeCommand::Equip { blade, path, json } => cli::blades::run_equip(blade, path, json),
    }
}
