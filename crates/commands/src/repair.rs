use clap::{Args as ClapArgs, ValueEnum};
use std::path::PathBuf;

#[derive(Copy, Clone, Debug, Eq, PartialEq, ValueEnum)]
pub enum DoctorRepairProfile {
    /// Conservative profile: only low-risk normalization steps.
    Safe,
    /// Full profile: include aggressive parser-recovery repairs.
    Aggressive,
}

impl Default for DoctorRepairProfile {
    fn default() -> Self {
        Self::Safe
    }
}

impl DoctorRepairProfile {
    pub fn label(self) -> &'static str {
        match self {
            Self::Safe => "safe",
            Self::Aggressive => "aggressive",
        }
    }

    pub fn is_aggressive(self) -> bool {
        matches!(self, Self::Aggressive)
    }
}

#[derive(ClapArgs, Debug, Default, Clone)]
pub struct DoctorRepairArgs {
    /// Repair a Kain source file before printing doctor output
    #[arg(long = "repair", value_name = "FILE", conflicts_with = "repair_tree")]
    pub repair: Option<PathBuf>,

    /// Repair every .kn file under a directory tree
    #[arg(long = "repair-tree", value_name = "DIR")]
    pub repair_tree: Option<PathBuf>,

    /// Select the repair profile (safe, aggressive)
    #[arg(long, value_enum, default_value_t = DoctorRepairProfile::Safe)]
    pub profile: DoctorRepairProfile,

    /// Show suggested repairs without writing them
    #[arg(long)]
    pub suggest: bool,

    /// Preview the repair run without writing changes
    #[arg(long)]
    pub dry_run: bool,

    /// Write the repaired file back to disk
    #[arg(long)]
    pub write: bool,
}
