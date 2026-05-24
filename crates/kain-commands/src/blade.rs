use clap::{Parser, Subcommand};
use std::path::PathBuf;

use crate::kain::CliColorArg;

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

    /// Clean generated .kain roots under the current blade workspace
    Clean {
        /// Path inside the workspace to clean
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Clean scope: build, run, amalgamate, or all
        #[arg(long, default_value = "all")]
        scope: String,

        /// Print the clean plan without removing anything
        #[arg(long)]
        dry_run: bool,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Run a local blade through the Kain run pipeline
    Run {
        /// Blade name or path inside the workspace
        blade: Option<String>,

        /// Path inside the workspace to inspect
        #[arg(long, default_value = ".")]
        path: PathBuf,

        /// Run target override
        #[arg(long, default_value = "auto")]
        target: String,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,

        /// Include trace-oriented report detail
        #[arg(long)]
        trace: bool,

        /// Keep cached/generated run artifacts
        #[arg(long = "keep-artifacts")]
        keep_artifacts: bool,

        /// Print the resolved run plan without executing
        #[arg(long)]
        dry_run: bool,

        /// Runtime args. Use `--` before this vector.
        #[arg(last = true)]
        args: Vec<String>,
    },
}

#[derive(Parser, Debug)]
#[command(name = "blade")]
#[command(version = env!("CARGO_PKG_VERSION"))]
#[command(about = "Standalone Kain blade workspace tool")]
pub struct BladeCli {
    /// Explicit Kain config path. Defaults to ~/.kain/config.toml
    #[arg(long, global = true)]
    pub config: Option<PathBuf>,

    /// Force CLI color policy
    #[arg(long, global = true, value_enum)]
    pub color: Option<CliColorArg>,

    /// Select a CLI theme: hyperpop, ember, glacier, or oxide
    #[arg(long, global = true)]
    pub theme: Option<String>,

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

    /// Clean generated .kain roots under the current blade workspace
    Clean {
        /// Path inside the workspace to clean
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Clean scope: build, run, amalgamate, or all
        #[arg(long, default_value = "all")]
        scope: String,

        /// Print the clean plan without removing anything
        #[arg(long)]
        dry_run: bool,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Run a local blade through the Kain run pipeline
    Run {
        /// Blade name or path inside the workspace
        blade: Option<String>,

        /// Path inside the workspace to inspect
        #[arg(long, default_value = ".")]
        path: PathBuf,

        /// Run target override
        #[arg(long, default_value = "auto")]
        target: String,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,

        /// Include trace-oriented report detail
        #[arg(long)]
        trace: bool,

        /// Keep cached/generated run artifacts
        #[arg(long = "keep-artifacts")]
        keep_artifacts: bool,

        /// Print the resolved run plan without executing
        #[arg(long)]
        dry_run: bool,

        /// Runtime args. Use `--` before this vector.
        #[arg(last = true)]
        args: Vec<String>,
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
    fn parses_standalone_blade_global_ui_flags() {
        let cli = BladeCli::parse_from([
            "blade",
            "--config",
            "team.toml",
            "--color",
            "never",
            "--theme",
            "glacier",
            "build",
            ".",
        ]);
        assert_eq!(cli.config, Some(PathBuf::from("team.toml")));
        assert_eq!(cli.color, Some(CliColorArg::Never));
        assert_eq!(cli.theme.as_deref(), Some("glacier"));
    }

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

    #[test]
    fn parses_standalone_blade_clean() {
        let cli = BladeCli::parse_from(["blade", "clean", ".", "--scope", "run", "--dry-run"]);
        match cli.command {
            BladeCommand::Clean {
                path,
                scope,
                dry_run,
                ..
            } => {
                assert_eq!(path, PathBuf::from("."));
                assert_eq!(scope, "run");
                assert!(dry_run);
            }
            other => panic!("expected blade clean, got {other:?}"),
        }
    }

    #[test]
    fn parses_standalone_blade_run() {
        let cli = BladeCli::parse_from(["blade", "run", "demo", "--target", "cargo", "--", "x"]);
        match cli.command {
            BladeCommand::Run {
                blade,
                target,
                args,
                ..
            } => {
                assert_eq!(blade.as_deref(), Some("demo"));
                assert_eq!(target, "cargo");
                assert_eq!(args, ["x"]);
            }
            other => panic!("expected blade run, got {other:?}"),
        }
    }
}
