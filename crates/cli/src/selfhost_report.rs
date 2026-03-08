use serde::Serialize;
use std::collections::BTreeMap;

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum SelfHostPhaseStatus {
    Pass,
    SoftFail,
    HardFail,
}

#[derive(Debug, Clone, Serialize)]
pub struct MacroFinding {
    pub crate_name: String,
    pub macro_name: String,
    pub occurrence_count: usize,
    pub files: Vec<String>,
}

#[derive(Debug, Clone, Serialize)]
pub struct TraitDynSummary {
    pub crate_name: String,
    pub trait_def_count: usize,
    pub trait_impl_count: usize,
    pub dyn_usage_count: usize,
    pub dyn_usage_files: Vec<String>,
}

#[derive(Debug, Clone, Serialize)]
pub struct CratePhase1Result {
    pub crate_name: String,
    pub crate_root: String,
    pub modules_discovered: Vec<String>,
    pub diagnostics: Vec<String>,
    pub import_success: bool,
    pub import_error: Option<String>,
    pub output_kn_path: Option<String>,
    pub item_count: usize,
    pub rejected_macros_found: Vec<MacroFinding>,
    pub required_direct_lowering_still_preserved: Vec<MacroFinding>,
}

#[derive(Debug, Clone, Serialize)]
pub struct SelfHostPhase1Report {
    pub generated_at_utc: String,
    pub repo_root: String,
    pub inventory_dir: String,
    pub output_dir: String,
    pub crates_processed: Vec<String>,
    pub modules_discovered: BTreeMap<String, Vec<String>>,
    pub diagnostics_by_category: BTreeMap<String, usize>,
    pub rejected_macros_found: Vec<MacroFinding>,
    pub required_direct_lowering_still_preserved: Vec<MacroFinding>,
    pub trait_dyn_summary: Vec<TraitDynSummary>,
    pub crate_results: Vec<CratePhase1Result>,
    pub final_phase_status: SelfHostPhaseStatus,
}

pub fn render_phase1_markdown(report: &SelfHostPhase1Report) -> String {
    let mut out = String::new();
    out.push_str("# Self-Host Phase 1 Report\n\n");
    out.push_str(&format!("- Generated at: `{}`\n", report.generated_at_utc));
    out.push_str(&format!("- Repo root: `{}`\n", report.repo_root));
    out.push_str(&format!("- Inventory dir: `{}`\n", report.inventory_dir));
    out.push_str(&format!("- Output dir: `{}`\n", report.output_dir));
    out.push_str(&format!("- Final status: `{}`\n", status_label(&report.final_phase_status)));
    out.push_str(&format!("- Crates processed: `{}`\n\n", report.crates_processed.join(", ")));

    out.push_str("## Diagnostics by category\n\n");
    if report.diagnostics_by_category.is_empty() {
        out.push_str("- none\n");
    } else {
        for (category, count) in &report.diagnostics_by_category {
            out.push_str(&format!("- `{}`: {}\n", category, count));
        }
    }

    out.push_str("\n## Rejected macros found\n\n");
    if report.rejected_macros_found.is_empty() {
        out.push_str("- none\n");
    } else {
        for finding in &report.rejected_macros_found {
            out.push_str(&format!(
                "- `{}` in `{}`: {} occurrence bucket(s)",
                finding.macro_name, finding.crate_name, finding.occurrence_count
            ));
            if !finding.files.is_empty() {
                out.push_str(&format!(" in {}", finding.files.join(", ")));
            }
            out.push('\n');
        }
    }

    out.push_str("\n## Required direct-lower macros still preserved\n\n");
    if report.required_direct_lowering_still_preserved.is_empty() {
        out.push_str("- none\n");
    } else {
        for finding in &report.required_direct_lowering_still_preserved {
            out.push_str(&format!(
                "- `{}` in `{}`: {} preserved macro call(s)\n",
                finding.macro_name, finding.crate_name, finding.occurrence_count
            ));
        }
    }

    out.push_str("\n## Trait / dyn summary\n\n");
    if report.trait_dyn_summary.is_empty() {
        out.push_str("- none\n");
    } else {
        for summary in &report.trait_dyn_summary {
            out.push_str(&format!(
                "- `{}`: trait defs {}, impls {}, dyn usages {}",
                summary.crate_name, summary.trait_def_count, summary.trait_impl_count, summary.dyn_usage_count
            ));
            if !summary.dyn_usage_files.is_empty() {
                out.push_str(&format!(" ({})", summary.dyn_usage_files.join(", ")));
            }
            out.push('\n');
        }
    }

    out.push_str("\n## Per-crate results\n\n");
    for crate_result in &report.crate_results {
        out.push_str(&format!("### `{}`\n\n", crate_result.crate_name));
        out.push_str(&format!("- Crate root: `{}`\n", crate_result.crate_root));
        out.push_str(&format!("- Import success: `{}`\n", crate_result.import_success));
        out.push_str(&format!("- Item count: `{}`\n", crate_result.item_count));
        match &crate_result.output_kn_path {
            Some(path) => out.push_str(&format!("- Output bundle: `{}`\n", path)),
            None => out.push_str("- Output bundle: `<none>`\n"),
        }
        if let Some(error) = &crate_result.import_error {
            out.push_str(&format!("- Import error: `{}`\n", error.replace('`', "'")));
        }
        out.push_str("- Modules discovered:\n");
        if crate_result.modules_discovered.is_empty() {
            out.push_str("  - none\n");
        } else {
            for module in &crate_result.modules_discovered {
                out.push_str(&format!("  - `{}`\n", module));
            }
        }
        out.push_str("- Diagnostics:\n");
        if crate_result.diagnostics.is_empty() {
            out.push_str("  - none\n");
        } else {
            for diagnostic in &crate_result.diagnostics {
                out.push_str(&format!("  - `{}`\n", diagnostic.replace('`', "'")));
            }
        }
        out.push_str("- Rejected macros found:\n");
        if crate_result.rejected_macros_found.is_empty() {
            out.push_str("  - none\n");
        } else {
            for finding in &crate_result.rejected_macros_found {
                out.push_str(&format!(
                    "  - `{}`: {} occurrence bucket(s)",
                    finding.macro_name, finding.occurrence_count
                ));
                if !finding.files.is_empty() {
                    out.push_str(&format!(" in {}", finding.files.join(", ")));
                }
                out.push('\n');
            }
        }
        out.push_str("- Required direct-lower macros still preserved:\n");
        if crate_result.required_direct_lowering_still_preserved.is_empty() {
            out.push_str("  - none\n");
        } else {
            for finding in &crate_result.required_direct_lowering_still_preserved {
                out.push_str(&format!(
                    "  - `{}`: {} preserved macro call(s)\n",
                    finding.macro_name, finding.occurrence_count
                ));
            }
        }
        out.push('\n');
    }

    out
}

fn status_label(status: &SelfHostPhaseStatus) -> &'static str {
    match status {
        SelfHostPhaseStatus::Pass => "pass",
        SelfHostPhaseStatus::SoftFail => "soft_fail",
        SelfHostPhaseStatus::HardFail => "hard_fail",
    }
}
