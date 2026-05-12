pub use kain_commands::repair::{DoctorRepairArgs, DoctorRepairProfile};
use std::fs;
use std::path::PathBuf;

fn validate_parser_conformance(source: &str) -> Result<(), String> {
    let tokens = kain_core::Lexer::new(source)
        .tokenize()
        .map_err(|err| format!("lexer error after repair: {}", err))?;
    let span_mapper = kain_core::diagnostics::SpanMapper::new(source);
    kain_core::Parser::new(&tokens, &span_mapper, "<doctor-repair>")
        .parse()
        .map_err(|err| format!("parser error after repair: {}", err))?;
    Ok(())
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum DoctorRepairTargetKind {
    File,
    Tree,
}

#[derive(Debug, Clone)]
pub struct DoctorRepairOutcome {
    pub path: PathBuf,
    pub result: Result<kain_repair::RepairReport, String>,
}

#[derive(Debug, Default, Clone)]
pub struct DoctorRepairBatchReport {
    pub scanned: usize,
    pub changed: usize,
    pub written: usize,
    pub failed: usize,
    pub outcomes: Vec<DoctorRepairOutcome>,
}

pub fn selected_mode(args: &DoctorRepairArgs) -> Option<kain_repair::RepairMode> {
    if args.repair.is_none() && args.repair_tree.is_none() {
        return None;
    }
    Some(if args.suggest {
        kain_repair::RepairMode::Suggest
    } else if args.dry_run {
        kain_repair::RepairMode::Check
    } else if args.profile.is_aggressive() {
        kain_repair::RepairMode::ApplyAggressive
    } else {
        kain_repair::RepairMode::ApplySafe
    })
}

pub fn selected_profile_label(args: &DoctorRepairArgs) -> &'static str {
    args.profile.label()
}

pub fn target_kind(args: &DoctorRepairArgs) -> Option<DoctorRepairTargetKind> {
    if args.repair_tree.is_some() {
        Some(DoctorRepairTargetKind::Tree)
    } else if args.repair.is_some() {
        Some(DoctorRepairTargetKind::File)
    } else {
        None
    }
}

fn repair_profile(profile: DoctorRepairProfile) -> kain_repair::RepairProfile {
    match profile {
        DoctorRepairProfile::Safe => kain_repair::RepairProfile {
            reconstruct_parser_safe_blocks: false,
            rewrite_reserved_identifiers: false,
            rewrite_inline_initializers: false,
            normalize_namespace_paths: false,
            ..kain_repair::RepairProfile::default()
        },
        DoctorRepairProfile::Aggressive => kain_repair::RepairProfile::default(),
    }
}

pub fn run(
    path: &PathBuf,
    profile: DoctorRepairProfile,
    mode: kain_repair::RepairMode,
) -> Result<kain_repair::RepairReport, String> {
    let source = fs::read_to_string(path)
        .map_err(|err| format!("failed to read {}: {}", path.display(), err))?;
    let repair_profile = repair_profile(profile);
    let report = kain_repair::repair_source_with_profile(&source, repair_profile, mode);
    let validation = match validate_parser_conformance(&report.repaired) {
        Ok(()) => kain_repair::ParseValidation {
            passed: true,
            detail: None,
        },
        Err(detail) => kain_repair::ParseValidation {
            passed: false,
            detail: Some(detail),
        },
    };
    if mode.writes() && report.changed() {
        fs::write(path, &report.repaired)
            .map_err(|err| format!("failed to write {}: {}", path.display(), err))?;
    }
    Ok(report.with_post_repair_parse(validation))
}

pub fn run_tree(
    root: &PathBuf,
    profile: DoctorRepairProfile,
    mode: kain_repair::RepairMode,
) -> Result<DoctorRepairBatchReport, String> {
    if !root.exists() {
        return Err(format!("{} does not exist", root.display()));
    }

    let mut candidates = Vec::new();
    collect_tree_candidates(root, &mut candidates)?;
    candidates.sort();

    let mut report = DoctorRepairBatchReport::default();
    report.scanned = candidates.len();
    for path in candidates {
        let result = run(&path, profile, mode);
        if let Ok(repair_report) = &result {
            if repair_report.changed() {
                report.changed += 1;
            }
            if mode.writes() && repair_report.changed() {
                report.written += 1;
            }
        } else {
            report.failed += 1;
        }
        report.outcomes.push(DoctorRepairOutcome { path, result });
    }
    Ok(report)
}

fn collect_tree_candidates(root: &PathBuf, candidates: &mut Vec<PathBuf>) -> Result<(), String> {
    if root.is_file() {
        if root.extension().and_then(|ext| ext.to_str()) == Some("kn") {
            candidates.push(root.clone());
        }
        return Ok(());
    }

    let mut stack = vec![root.clone()];
    while let Some(dir) = stack.pop() {
        let entries = fs::read_dir(&dir)
            .map_err(|err| format!("failed to read {}: {}", dir.display(), err))?;
        for entry in entries {
            let entry =
                entry.map_err(|err| format!("failed to read {}: {}", dir.display(), err))?;
            let path = entry.path();
            if path.is_dir() {
                stack.push(path);
            } else if path.extension().and_then(|ext| ext.to_str()) == Some("kn") {
                candidates.push(path);
            }
        }
    }
    Ok(())
}
