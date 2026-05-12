use clap::Subcommand;
use std::path::PathBuf;

#[derive(Subcommand, Debug)]
pub enum SelfHostCommand {
    Bootstrap {
        #[arg(long)]
        manifest_path: Option<PathBuf>,

        #[arg(long, default_value = "llvm")]
        backend: String,

        #[arg(long)]
        combine_only: bool,

        #[arg(long)]
        emit_llvm_only: bool,

        #[arg(long)]
        link_native: bool,

        #[arg(long)]
        verify_ouroboros: bool,
    },
    Phase1 {
        #[arg(long)]
        inventory_dir: Option<PathBuf>,

        #[arg(long)]
        output_dir: Option<PathBuf>,

        #[arg(long)]
        profile_path: Option<PathBuf>,

        #[arg(long, default_value_t = true, action = clap::ArgAction::Set)]
        emit_bundles: bool,

        #[arg(long)]
        all_crates: bool,

        #[arg(long)]
        force: bool,
    },
    Phase2 {
        #[arg(long)]
        inventory_dir: Option<PathBuf>,

        #[arg(long)]
        output_dir: Option<PathBuf>,

        #[arg(long)]
        profile_path: Option<PathBuf>,

        #[arg(long, default_value_t = true, action = clap::ArgAction::Set)]
        emit_bundles: bool,

        #[arg(long, default_value_t = true, action = clap::ArgAction::Set)]
        emit_roundtrip_rust: bool,

        #[arg(long, default_value_t = true, action = clap::ArgAction::Set)]
        assemble_stage2: bool,

        #[arg(long, default_value_t = true, action = clap::ArgAction::Set)]
        build_stage2: bool,

        #[arg(long)]
        all_crates: bool,

        #[arg(long)]
        force: bool,
    },
}
