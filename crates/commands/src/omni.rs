use std::path::PathBuf;

use clap::Subcommand;

#[derive(Subcommand, Debug)]
pub enum OmniCommand {
    Init {
        #[arg(default_value = ".")]
        path: PathBuf,
    },
    Build {
        #[arg(short, long, default_value = "KAIN.omni.toml")]
        manifest: PathBuf,
    },
}
