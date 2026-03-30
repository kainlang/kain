use clap::{Args as ClapArgs, ValueEnum};
use std::fs;
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
    #[arg(long = "repair", value_name = "FILE")]
    pub repair: Option<PathBuf>,

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

impl DoctorRepairArgs {
    pub fn selected_mode(&self) -> Option<kain_repair::RepairMode> {
        if self.repair.is_none() {
            return None;
        }
        Some(if self.suggest {
            kain_repair::RepairMode::Suggest
        } else if self.dry_run {
            kain_repair::RepairMode::Check
        } else if self.profile.is_aggressive() {
            kain_repair::RepairMode::ApplyAggressive
        } else {
            kain_repair::RepairMode::ApplySafe
        })
    }

    pub fn selected_profile_label(&self) -> &'static str {
        self.profile.label()
    }
}

pub fn run(
    path: &PathBuf,
    profile: DoctorRepairProfile,
    mode: kain_repair::RepairMode,
) -> Result<kain_repair::RepairReport, String> {
    let source = fs::read_to_string(path)
        .map_err(|err| format!("failed to read {}: {}", path.display(), err))?;
    let repair_profile = match profile {
        DoctorRepairProfile::Safe => kain_repair::RepairProfile {
            reconstruct_parser_safe_blocks: false,
            rewrite_reserved_identifiers: false,
            rewrite_inline_initializers: false,
            normalize_namespace_paths: false,
            ..kain_repair::RepairProfile::default()
        },
        DoctorRepairProfile::Aggressive => kain_repair::RepairProfile::default(),
    };
    let report = kain_repair::repair_source_with_profile(&source, repair_profile, mode);
    if mode.writes() && report.changed() {
        fs::write(path, &report.repaired)
            .map_err(|err| format!("failed to write {}: {}", path.display(), err))?;
    }
    Ok(report)
}
