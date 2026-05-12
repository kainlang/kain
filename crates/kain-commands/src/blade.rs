use clap::{Parser, Subcommand};
use std::path::PathBuf;

#[derive(Subcommand, Debug)]
pub enum BladesCommand {
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

    /// Build the full local blade workspace through the Kain build orchestrator
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
}

#[derive(Parser, Debug)]
#[command(name = "blade")]
#[command(version = env!("CARGO_PKG_VERSION"))]
#[command(about = "Standalone Kain blade workspace tool")]
pub struct BladeCli {
    #[command(subcommand)]
    pub command: BladeCommand,
}

#[derive(Subcommand, Debug)]
pub enum BladeCommand {
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

    /// Runtime-contributed blade command fallback.
    #[command(external_subcommand)]
    External(Vec<String>),
}

#[cfg(test)]
mod tests {
    use super::*;
    use clap::Parser;

    #[test]
    fn parses_standalone_blade_build() {
        let cli = BladeCli::parse_from(["blade", "build", ".", "--json", "--clean"]);
        match cli.command {
            BladeCommand::Build {
                path, json, clean, ..
            } => {
                assert_eq!(path, PathBuf::from("."));
                assert!(json);
                assert!(clean);
            }
            other => panic!("expected blade build, got {other:?}"),
        }
    }
}
