use clap::Args as ClapArgs;
use std::fs;
use std::path::PathBuf;

#[derive(ClapArgs, Debug, Default, Clone)]
pub struct DoctorRepairArgs {
    /// Repair a Kain source file before printing doctor output
    #[arg(long = "repair", value_name = "FILE")]
    pub repair: Option<PathBuf>,

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
        Some(if self.write {
            kain_repair::RepairMode::Write
        } else if self.suggest {
            kain_repair::RepairMode::Suggest
        } else {
            kain_repair::RepairMode::DryRun
        })
    }
}

pub fn run(path: &PathBuf, mode: kain_repair::RepairMode) -> Result<kain_repair::RepairReport, String> {
    let source = fs::read_to_string(path)
        .map_err(|err| format!("failed to read {}: {}", path.display(), err))?;
    let report = kain_repair::repair_source(&source, mode);
    if mode.writes() && report.changed() {
        fs::write(path, &report.repaired)
            .map_err(|err| format!("failed to write {}: {}", path.display(), err))?;
    }
    Ok(report)
}
