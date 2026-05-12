use std::path::PathBuf;

use clap::{Subcommand, ValueEnum};

#[derive(Subcommand, Debug)]
pub enum FabricCommand {
    Init {
        #[arg(default_value = ".")]
        path: PathBuf,
        #[arg(long, value_enum, default_value_t = FabricTemplateArg::Polyglot)]
        template: FabricTemplateArg,
    },
    Validate {
        #[arg(short, long, default_value = "KAIN.fabric.toml")]
        manifest: PathBuf,
    },
    Run {
        #[arg(short, long, default_value = "KAIN.fabric.toml")]
        manifest: PathBuf,
    },
}

#[derive(Clone, Copy, Debug, ValueEnum)]
pub enum FabricTemplateArg {
    Local,
    Polyglot,
}
