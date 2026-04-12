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
    pub canonical_kain_root: Option<String>,
    pub output_kain_root: Option<String>,
    pub aggregate_roundtrip_rust_path: Option<String>,
    pub roundtrip_rust_tree_root: Option<String>,
    pub mirrored_file_count: usize,
    pub item_count: usize,
    pub rejected_macros_found: Vec<MacroFinding>,
    pub required_direct_lowering_still_preserved: Vec<MacroFinding>,
}

#[derive(Debug, Clone, Serialize)]
pub struct InventoryInputEvidence {
    pub inventory_key: String,
    pub path: String,
    pub byte_size: u64,
}

#[derive(Debug, Clone, Serialize)]
pub struct Stage2WorkspaceCrateEvidence {
    pub crate_name: String,
    pub source_roundtrip_path: String,
    pub source_roundtrip_byte_size: u64,
    pub source_tree_root: String,
    pub roundtrip_file_count: usize,
    pub manifest_path: String,
    pub lib_rs_path: String,
    pub main_rs_path: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
pub struct SelfHostPhase1Report {
    pub generated_at_utc: String,
    pub repo_root: String,
    pub profile_path: String,
    pub profile_name: String,
    pub force_mode: bool,
    pub all_crates_mode: bool,
    pub inventory_dir: String,
    pub inventory_inputs: Vec<InventoryInputEvidence>,
    pub output_dir: String,
    pub canonical_source_root: String,
    pub output_mirror_root: String,
    pub roundtrip_rust_root: String,
    pub source_correspondence_manifest_path: String,
    pub source_correspondence_file_count: usize,
    pub crates_processed: Vec<String>,
    pub modules_discovered: BTreeMap<String, Vec<String>>,
    pub diagnostics_by_category: BTreeMap<String, usize>,
    pub rejected_macros_found: Vec<MacroFinding>,
    pub required_direct_lowering_still_preserved: Vec<MacroFinding>,
    pub trait_dyn_summary: Vec<TraitDynSummary>,
    pub crate_results: Vec<CratePhase1Result>,
    pub stage2_workspace_path: Option<String>,
    pub stage2_workspace_crates: Vec<Stage2WorkspaceCrateEvidence>,
    pub stage2_build_artifact: Option<String>,
    pub stage2_build_log_path: Option<String>,
    pub stage2_build_success: Option<bool>,
    pub stage2_build_exit_code: Option<i32>,
    pub stage2_error: Option<String>,
    pub final_phase_status: SelfHostPhaseStatus,
}

pub fn render_phase_markdown(title: &str, report: &SelfHostPhase1Report) -> String {
    let mut out = String::new();
    out.push_str(&format!("# {} Report\n\n", title));
    out.push_str(&format!("- Generated at: `{}`\n", report.generated_at_utc));
    out.push_str(&format!("- Repo root: `{}`\n", report.repo_root));
    out.push_str(&format!(
        "- Profile: `{}` (`{}`)\n",
        report.profile_name, report.profile_path
    ));
    out.push_str(&format!("- Force mode: `{}`\n", report.force_mode));
    out.push_str(&format!(
        "- All crates mode: `{}`\n",
        report.all_crates_mode
    ));
    out.push_str(&format!("- Inventory dir: `{}`\n", report.inventory_dir));
    if report.inventory_inputs.is_empty() {
        out.push_str("- Inventory inputs: `none`\n");
    } else {
        out.push_str("- Inventory inputs:\n");
        for input in &report.inventory_inputs {
            out.push_str(&format!(
                "  - `{}` => `{}` ({} bytes)\n",
                input.inventory_key, input.path, input.byte_size
            ));
        }
    }
    out.push_str(&format!("- Output dir: `{}`\n", report.output_dir));
    out.push_str(&format!(
        "- Canonical source root: `{}`\n",
        report.canonical_source_root
    ));
    out.push_str(&format!(
        "- Output mirror root: `{}`\n",
        report.output_mirror_root
    ));
    out.push_str(&format!(
        "- Roundtrip Rust root: `{}`\n",
        report.roundtrip_rust_root
    ));
    out.push_str(&format!(
        "- Source correspondence manifest: `{}` ({} files)\n",
        report.source_correspondence_manifest_path, report.source_correspondence_file_count
    ));
    out.push_str(&format!(
        "- Final status: `{}`\n",
        status_label(&report.final_phase_status)
    ));
    out.push_str(&format!(
        "- Crates processed: `{}`\n\n",
        report.crates_processed.join(", ")
    ));
    if let Some(path) = &report.stage2_workspace_path {
        out.push_str(&format!("- Stage2 workspace: `{}`\n", path));
    }
    if report.stage2_workspace_crates.is_empty() {
        out.push_str("- Stage2 workspace crates: `none`\n");
    } else {
        out.push_str("- Stage2 workspace crates:\n");
        for crate_evidence in &report.stage2_workspace_crates {
            out.push_str(&format!(
                "  - `{}` source `{}` ({} bytes), tree `{}` ({} files), manifest `{}`, lib `{}`",
                crate_evidence.crate_name,
                crate_evidence.source_roundtrip_path,
                crate_evidence.source_roundtrip_byte_size,
                crate_evidence.source_tree_root,
                crate_evidence.roundtrip_file_count,
                crate_evidence.manifest_path,
                crate_evidence.lib_rs_path
            ));
            if let Some(main_rs_path) = &crate_evidence.main_rs_path {
                out.push_str(&format!(", main `{}`", main_rs_path));
            }
            out.push('\n');
        }
    }
    if let Some(path) = &report.stage2_build_artifact {
        out.push_str(&format!("- Stage2 artifact: `{}`\n", path));
    }
    if let Some(path) = &report.stage2_build_log_path {
        out.push_str(&format!("- Stage2 build log: `{}`\n", path));
    }
    if let Some(success) = report.stage2_build_success {
        out.push_str(&format!("- Stage2 build success: `{}`\n", success));
    }
    if let Some(exit_code) = report.stage2_build_exit_code {
        out.push_str(&format!("- Stage2 build exit code: `{}`\n", exit_code));
    }
    if let Some(stage2_error) = &report.stage2_error {
        out.push_str(&format!("- Stage2 error: `{}`\n", stage2_error));
    }
    out.push('\n');

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
                summary.crate_name,
                summary.trait_def_count,
                summary.trait_impl_count,
                summary.dyn_usage_count
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
        out.push_str(&format!(
            "- Import success: `{}`\n",
            crate_result.import_success
        ));
        out.push_str(&format!("- Item count: `{}`\n", crate_result.item_count));
        match &crate_result.output_kn_path {
            Some(path) => out.push_str(&format!("- Output bundle: `{}`\n", path)),
            None => out.push_str("- Output bundle: `<none>`\n"),
        }
        match &crate_result.canonical_kain_root {
            Some(path) => out.push_str(&format!("- Canonical Kain root: `{}`\n", path)),
            None => out.push_str("- Canonical Kain root: `<none>`\n"),
        }
        match &crate_result.output_kain_root {
            Some(path) => out.push_str(&format!("- Output mirror root: `{}`\n", path)),
            None => out.push_str("- Output mirror root: `<none>`\n"),
        }
        match &crate_result.aggregate_roundtrip_rust_path {
            Some(path) => out.push_str(&format!("- Aggregate roundtrip Rust: `{}`\n", path)),
            None => out.push_str("- Aggregate roundtrip Rust: `<none>`\n"),
        }
        match &crate_result.roundtrip_rust_tree_root {
            Some(path) => out.push_str(&format!("- Roundtrip Rust tree: `{}`\n", path)),
            None => out.push_str("- Roundtrip Rust tree: `<none>`\n"),
        }
        out.push_str(&format!(
            "- Mirrored file count: `{}`\n",
            crate_result.mirrored_file_count
        ));
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
        if crate_result
            .required_direct_lowering_still_preserved
            .is_empty()
        {
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

pub fn render_phase1_markdown(report: &SelfHostPhase1Report) -> String {
    render_phase_markdown("Self-Host Phase 1", report)
}

fn status_label(status: &SelfHostPhaseStatus) -> &'static str {
    match status {
        SelfHostPhaseStatus::Pass => "pass",
        SelfHostPhaseStatus::SoftFail => "soft_fail",
        SelfHostPhaseStatus::HardFail => "hard_fail",
    }
}
