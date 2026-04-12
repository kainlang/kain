use crate::error::{KainError, KainResult};
use crate::selfhost_profile::{default_selfhost_source_profile_path, SelfHostSourceProfile};
use crate::selfhost_report::{
    render_phase_markdown, CratePhase1Result, InventoryInputEvidence, MacroFinding,
    SelfHostPhase1Report, SelfHostPhaseStatus, Stage2WorkspaceCrateEvidence, TraitDynSummary,
};
use chrono::Utc;
use clap::Subcommand;
use kain_core::ast::{
    Attribute, Block, CallArg, ElseBranch, EnumVariantFields, Expr, Generic, Item, MatchArm, Param,
    Pattern, Program, Stmt, Type, Use, VariantPatternFields, Visibility,
};
use kain_core::parser::RESERVED_KEYWORDS;
use kain_import::rust::{RustSelfHostModuleProgram, RustSelfHostOptions};
use serde::{Deserialize, Serialize};
use std::cell::RefCell;
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use toml::Value;

const SELFHOST_CONTEXTUAL_KEYWORDS: &[&str] = &["state", "weak", "compute", "shader"];
const SELFHOST_STAGE2_VERSION_SUFFIX: &str = "-selfhost.0";
const INVENTORY_FILE_SPECS: &[(&str, &str)] = &[
    ("macro_inventory", "macro_inventory.json"),
    ("module_map", "module_map.json"),
    ("selfhost_allowlist", "selfhost_allowlist.json"),
    ("trait_inventory", "trait_inventory.json"),
];

thread_local! {
    static CURRENT_SELFHOST_FUNCTION: RefCell<Option<String>> = const { RefCell::new(None) };
    static CURRENT_SELFHOST_IMPL: RefCell<Option<String>> = const { RefCell::new(None) };
    static CURRENT_SELFHOST_MODULE: RefCell<Option<String>> = const { RefCell::new(None) };
}

#[derive(Debug, Clone, Serialize)]
struct SelfHostSourceCorrespondenceManifest {
    generated_at_utc: String,
    phase_name: String,
    repo_root: String,
    profile_path: String,
    profile_name: String,
    canonical_source_root: String,
    output_mirror_root: String,
    roundtrip_rust_root: String,
    crates: Vec<SelfHostCrateCorrespondence>,
}

#[derive(Debug, Clone, Serialize)]
struct SelfHostCrateCorrespondence {
    crate_name: String,
    crate_root: String,
    aggregate_bundle_path: Option<String>,
    aggregate_roundtrip_rust_path: Option<String>,
    canonical_kain_root: String,
    output_kain_root: String,
    roundtrip_rust_tree_root: String,
    mirrored_files: Vec<SelfHostFileCorrespondence>,
}

#[derive(Debug, Clone, Serialize)]
struct SelfHostFileCorrespondence {
    module_name: String,
    rust_source_path: String,
    rust_source_relative_path: String,
    canonical_kain_path: String,
    output_kain_path: String,
    stage2_roundtrip_rust_path: String,
    module_path: Vec<String>,
    source_kind: String,
    ownership_state: String,
    stage2_roundtrip_strategy: String,
}

#[derive(Debug, Clone)]
struct SelfHostCrateEmissionArtifacts {
    mirrored_file_count: usize,
}

#[derive(Debug, Clone)]
struct CrateRoundtripRustArtifacts {
    aggregate_rust_path: PathBuf,
    source_tree_root: PathBuf,
    main_rs_path: Option<PathBuf>,
    file_count: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum SelfHostRustSourceKind {
    LibRoot,
    MainRoot,
    ModuleFile,
    ModuleDirectoryRoot,
}

impl SelfHostRustSourceKind {
    fn as_str(self) -> &'static str {
        match self {
            Self::LibRoot => "lib_root",
            Self::MainRoot => "main_root",
            Self::ModuleFile => "module_file",
            Self::ModuleDirectoryRoot => "module_directory_root",
        }
    }
}

#[derive(Debug, Clone)]
struct SelfHostFileMirrorPlan {
    module_name: String,
    rust_source_path: PathBuf,
    rust_source_relative_path: PathBuf,
    canonical_kain_path: PathBuf,
    output_kain_path: PathBuf,
    stage2_roundtrip_rust_path: PathBuf,
    module_path: Vec<String>,
    source_kind: SelfHostRustSourceKind,
}

#[derive(Subcommand, Debug)]
pub enum SelfHostCommand {
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
        force: bool,
    },
}

pub fn run(command: SelfHostCommand) -> KainResult<()> {
    match command {
        SelfHostCommand::Phase1 {
            inventory_dir,
            output_dir,
            profile_path,
            emit_bundles,
            force,
        } => run_phase1(inventory_dir, output_dir, profile_path, emit_bundles, force),
        SelfHostCommand::Phase2 {
            inventory_dir,
            output_dir,
            profile_path,
            emit_bundles,
            emit_roundtrip_rust,
            assemble_stage2,
            build_stage2,
            force,
        } => run_phase2(
            inventory_dir,
            output_dir,
            profile_path,
            emit_bundles,
            emit_roundtrip_rust,
            assemble_stage2,
            build_stage2,
            force,
        ),
    }
}

fn run_phase1(
    inventory_dir: Option<PathBuf>,
    output_dir: Option<PathBuf>,
    profile_path: Option<PathBuf>,
    emit_bundles: bool,
    force: bool,
) -> KainResult<()> {
    run_phase(
        "phase1",
        inventory_dir,
        output_dir,
        profile_path,
        emit_bundles,
        false,
        false,
        false,
        force,
    )
}

fn run_phase2(
    inventory_dir: Option<PathBuf>,
    output_dir: Option<PathBuf>,
    profile_path: Option<PathBuf>,
    emit_bundles: bool,
    emit_roundtrip_rust: bool,
    assemble_stage2: bool,
    build_stage2: bool,
    force: bool,
) -> KainResult<()> {
    run_phase(
        "phase2",
        inventory_dir,
        output_dir,
        profile_path,
        emit_bundles,
        emit_roundtrip_rust,
        assemble_stage2,
        build_stage2,
        force,
    )
}

fn run_phase(
    phase_name: &str,
    inventory_dir: Option<PathBuf>,
    output_dir: Option<PathBuf>,
    profile_path: Option<PathBuf>,
    emit_bundles: bool,
    emit_roundtrip_rust: bool,
    assemble_stage2: bool,
    build_stage2: bool,
    force: bool,
) -> KainResult<()> {
    let repo_root = find_repo_root(&std::env::current_dir().map_err(KainError::Io)?)?;
    let inventory_dir = inventory_dir.unwrap_or_else(|| default_inventory_dir(&repo_root));
    let output_dir =
        output_dir.unwrap_or_else(|| default_output_dir_for_phase(&repo_root, phase_name));
    let profile_path =
        profile_path.unwrap_or_else(|| default_selfhost_source_profile_path(&repo_root));
    let profile = SelfHostSourceProfile::load(&profile_path)?;
    let inventory_inputs = collect_inventory_input_evidence(&inventory_dir)?;
    let inventories = load_inventories(&inventory_dir)?;
    let mut options = RustSelfHostOptions::from_inventory_dir(&inventory_dir).map_err(|err| {
        KainError::runtime(format!("Failed to load strict self-host options: {err}"))
    })?;
    options.include_tests = false;

    fs::create_dir_all(&output_dir).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create self-host output directory {}: {}",
            output_dir.display(),
            err
        ))
    })?;

    let crates_processed = resolve_crates_for_phase(&profile, &inventories.module_map, phase_name);
    let mut modules_discovered: BTreeMap<String, Vec<String>> = BTreeMap::new();
    let mut crate_results = Vec::new();
    let mut all_rejected = Vec::new();
    let mut all_required_preserved = Vec::new();
    let mut roundtrip_rust_outputs = BTreeMap::<String, CrateRoundtripRustArtifacts>::new();
    let mut source_correspondence_crates = Vec::new();
    let canonical_source_root = profile.canonical_source_root(&repo_root);
    let output_mirror_root = profile.output_mirror_root(&output_dir);
    let roundtrip_rust_root = profile.roundtrip_rust_root(&output_dir);

    for crate_name in &crates_processed {
        let crate_root = repo_root.join("crates").join(crate_name);
        let rejected_macros_found = macro_findings_for(
            crate_name,
            &inventories.macro_inventory,
            &inventories.allowlist.macro_policy.reject,
        );

        let mut diagnostics = Vec::new();
        let mut import_success = true;
        let mut import_error = None;
        let mut output_kn_path = None;
        let canonical_kain_root = canonical_source_root.join(crate_name);
        let output_kain_root = output_mirror_root.join(crate_name);
        let roundtrip_tree_root = roundtrip_rust_root.join(crate_name);
        let mut aggregate_roundtrip_rust_path = None;
        let mut roundtrip_rust_tree_root = None;
        let mut mirrored_file_count = 0usize;
        let mut item_count = 0usize;
        let mut required_direct_lowering_still_preserved = Vec::new();
        let mut discovered_modules = Vec::new();

        let mut file_plans = Vec::new();
        let mut aggregate_bundle_path: Option<PathBuf> = None;
        let mut aggregate_roundtrip_path: Option<PathBuf> = None;

        if !crate_root.exists() {
            import_success = false;
            let message = format!(
                "Initial self-host slice crate not found: {}",
                crate_root.display()
            );
            diagnostics.push(message.clone());
            import_error = Some(message.clone());
            if !force {
                return Err(KainError::runtime(message));
            }
        } else {
            let import_result =
                kain_import::import_rust_selfhost_dir_detailed(&crate_root, &options);
            let crate_processing = (|| -> KainResult<()> {
                match import_result {
                    Ok(result) => {
                        discovered_modules = result
                            .graph
                            .modules
                            .iter()
                            .map(|module| {
                                module
                                    .file_path
                                    .strip_prefix(&repo_root)
                                    .unwrap_or(&module.file_path)
                                    .display()
                                    .to_string()
                            })
                            .collect::<Vec<_>>();
                        modules_discovered.insert(crate_name.clone(), discovered_modules.clone());

                        file_plans = build_file_mirror_plans(
                            &crate_root,
                            crate_name,
                            &result.module_programs,
                            &canonical_source_root,
                            &output_mirror_root,
                            &roundtrip_rust_root,
                        )?;
                        mirrored_file_count = file_plans.len();

                        let program = result.program;
                        diagnostics.extend(result.diagnostics);
                        item_count = program.items.len();
                        required_direct_lowering_still_preserved = preserved_required_macro_findings(
                            crate_name,
                            &program,
                            &inventories.allowlist.phase1_required_direct_lowering,
                        );
                        if !required_direct_lowering_still_preserved.is_empty() {
                            diagnostics.push(format!(
                                "required direct-lower macros preserved: {}",
                                required_direct_lowering_still_preserved
                                    .iter()
                                    .map(|finding| format!(
                                        "{}({})",
                                        finding.macro_name, finding.occurrence_count
                                    ))
                                    .collect::<Vec<_>>()
                                    .join(", ")
                            ));
                        }
                        if !diagnostics.is_empty() {
                            import_success = false;
                        }
                        if result.rejected {
                            import_error = Some(format!(
                                "self-host import rejected {} diagnostic(s)",
                                diagnostics.len()
                            ));
                        }

                        let rendered = if emit_bundles || emit_roundtrip_rust {
                            Some(render_program(&program)?)
                        } else {
                            None
                        };

                        if emit_bundles {
                            let emission =
                                emit_selfhost_kain_mirrors(&result.module_programs, &file_plans)?;
                            mirrored_file_count = emission.mirrored_file_count;
                            let bundle_path =
                                output_dir.join(profile.aggregate_bundle_file_name(crate_name));
                            let rendered = rendered.as_ref().expect("rendered aggregate bundle");
                            fs::write(&bundle_path, rendered).map_err(|err| {
                                KainError::runtime(format!(
                                    "Failed to write self-host bundle {}: {}",
                                    bundle_path.display(),
                                    err
                                ))
                            })?;
                            output_kn_path = Some(bundle_path.display().to_string());
                            aggregate_bundle_path = Some(bundle_path);
                        }

                        if emit_roundtrip_rust {
                            let rendered = rendered.as_ref().expect("rendered aggregate bundle");
                            let roundtrip_path = output_dir
                                .join(profile.aggregate_roundtrip_file_name(crate_name));
                            let rust_source = compile_kn_source_to_rust(rendered)?;
                            fs::write(&roundtrip_path, &rust_source).map_err(|err| {
                                KainError::runtime(format!(
                                    "Failed to write self-host roundtrip Rust {}: {}",
                                    roundtrip_path.display(),
                                    err
                                ))
                            })?;
                            let roundtrip_artifacts = write_roundtrip_rust_tree(
                                crate_name,
                                &crate_root,
                                &roundtrip_rust_root,
                                &roundtrip_path,
                                &rust_source,
                                &file_plans,
                                &profile,
                            )?;
                            aggregate_roundtrip_rust_path = Some(
                                roundtrip_artifacts
                                    .aggregate_rust_path
                                    .display()
                                    .to_string(),
                            );
                            roundtrip_rust_tree_root =
                                Some(roundtrip_artifacts.source_tree_root.display().to_string());
                            aggregate_roundtrip_path = Some(roundtrip_path);
                            roundtrip_rust_outputs.insert(crate_name.clone(), roundtrip_artifacts);
                        }
                        Ok(())
                    }
                    Err(err) => {
                        import_success = false;
                        let message = format!("{err}");
                        diagnostics.extend(expand_import_diagnostics(&message));
                        import_error = Some(message);
                        modules_discovered.insert(crate_name.clone(), discovered_modules.clone());
                        Ok(())
                    }
                }
            })();

            if let Err(err) = crate_processing {
                import_success = false;
                let message = format!("{err}");
                diagnostics.push(message.clone());
                if import_error.is_none() {
                    import_error = Some(message.clone());
                }
                if !force {
                    return Err(err);
                }
            }
        }

        source_correspondence_crates.push(build_crate_source_correspondence(
            crate_name,
            &crate_root,
            &file_plans,
            aggregate_bundle_path.as_deref(),
            aggregate_roundtrip_path.as_deref(),
            &canonical_kain_root,
            &output_kain_root,
            &roundtrip_tree_root,
            &profile,
        ));

        if !modules_discovered.contains_key(crate_name) {
            modules_discovered.insert(crate_name.clone(), discovered_modules.clone());
        }

        all_rejected.extend(rejected_macros_found.clone());
        all_required_preserved.extend(required_direct_lowering_still_preserved.clone());

        crate_results.push(CratePhase1Result {
            crate_name: crate_name.clone(),
            crate_root: crate_root.display().to_string(),
            modules_discovered: discovered_modules,
            diagnostics,
            import_success,
            import_error,
            output_kn_path,
            canonical_kain_root: Some(canonical_kain_root.display().to_string()),
            output_kain_root: Some(output_kain_root.display().to_string()),
            aggregate_roundtrip_rust_path,
            roundtrip_rust_tree_root,
            mirrored_file_count,
            item_count,
            rejected_macros_found,
            required_direct_lowering_still_preserved,
        });
    }

    let diagnostics_by_category = build_diagnostic_category_summary(&crate_results);
    let trait_dyn_summary =
        build_trait_dyn_summary(&inventories.trait_inventory, &crates_processed);
    let mut final_phase_status = determine_phase_status(&crate_results, &all_required_preserved);
    let mut stage2_workspace_path = None;
    let mut stage2_workspace_crates = Vec::new();
    let mut stage2_build_artifact = None;
    let mut stage2_build_log_path = None;
    let mut stage2_build_success = None;
    let mut stage2_build_exit_code = None;
    let mut stage2_error = None;
    let source_correspondence_manifest = SelfHostSourceCorrespondenceManifest {
        generated_at_utc: Utc::now().to_rfc3339(),
        phase_name: phase_name.to_string(),
        repo_root: repo_root.display().to_string(),
        profile_path: profile_path.display().to_string(),
        profile_name: profile.name.clone(),
        canonical_source_root: canonical_source_root.display().to_string(),
        output_mirror_root: output_mirror_root.display().to_string(),
        roundtrip_rust_root: roundtrip_rust_root.display().to_string(),
        crates: source_correspondence_crates,
    };
    let source_correspondence_manifest_path =
        profile.source_correspondence_manifest_path(&output_dir);
    write_source_correspondence_manifest(
        &source_correspondence_manifest_path,
        &source_correspondence_manifest,
    )?;
    let source_correspondence_file_count = source_correspondence_manifest
        .crates
        .iter()
        .map(|crate_entry| crate_entry.mirrored_files.len())
        .sum();

    if assemble_stage2 {
        let stage2_crates_to_assemble = if force {
            crates_processed
                .iter()
                .filter(|crate_name| roundtrip_rust_outputs.contains_key(*crate_name))
                .cloned()
                .collect::<Vec<_>>()
        } else {
            crates_processed.clone()
        };

        if stage2_crates_to_assemble.is_empty() {
            final_phase_status = SelfHostPhaseStatus::HardFail;
            stage2_error = Some(
                "No roundtrip Rust outputs were available for stage2 workspace assembly"
                    .to_string(),
            );
        } else {
            match assemble_stage2_workspace(
                &repo_root,
                &profile.stage2_workspace_dir(&output_dir),
                &stage2_crates_to_assemble,
                &roundtrip_rust_outputs,
            ) {
                Ok(stage2_assembly) => {
                    stage2_workspace_path =
                        Some(stage2_assembly.workspace_path.display().to_string());
                    stage2_workspace_crates = stage2_assembly.crates;

                    if build_stage2 {
                        match build_stage2_workspace(&stage2_assembly.workspace_path) {
                            Ok(build_result) => {
                                stage2_build_success = Some(build_result.success);
                                stage2_build_artifact = build_result
                                    .artifact_path
                                    .map(|path| path.display().to_string());
                                stage2_build_log_path =
                                    Some(build_result.log_path.display().to_string());
                                stage2_build_exit_code = build_result.exit_code;
                                if !build_result.success {
                                    final_phase_status = SelfHostPhaseStatus::HardFail;
                                }
                            }
                            Err(err) => {
                                final_phase_status = SelfHostPhaseStatus::HardFail;
                                stage2_error = Some(format!("{err}"));
                                if !force {
                                    return Err(err);
                                }
                            }
                        }
                    }
                }
                Err(err) => {
                    final_phase_status = SelfHostPhaseStatus::HardFail;
                    stage2_error = Some(format!("{err}"));
                    if !force {
                        return Err(err);
                    }
                }
            }
        }
    }

    let report = SelfHostPhase1Report {
        generated_at_utc: Utc::now().to_rfc3339(),
        repo_root: repo_root.display().to_string(),
        profile_path: profile_path.display().to_string(),
        profile_name: profile.name.clone(),
        force_mode: force,
        inventory_dir: inventory_dir.display().to_string(),
        inventory_inputs,
        output_dir: output_dir.display().to_string(),
        canonical_source_root: canonical_source_root.display().to_string(),
        output_mirror_root: output_mirror_root.display().to_string(),
        roundtrip_rust_root: roundtrip_rust_root.display().to_string(),
        source_correspondence_manifest_path: source_correspondence_manifest_path
            .display()
            .to_string(),
        source_correspondence_file_count,
        crates_processed,
        modules_discovered,
        diagnostics_by_category,
        rejected_macros_found: all_rejected,
        required_direct_lowering_still_preserved: all_required_preserved,
        trait_dyn_summary,
        crate_results,
        stage2_workspace_path,
        stage2_workspace_crates,
        stage2_build_artifact,
        stage2_build_log_path,
        stage2_build_success,
        stage2_build_exit_code,
        stage2_error,
        final_phase_status: final_phase_status.clone(),
    };

    write_report_files(phase_name, &output_dir, &report)?;
    print_summary(phase_name, &report);

    match final_phase_status {
        SelfHostPhaseStatus::Pass => Ok(()),
        SelfHostPhaseStatus::SoftFail => Err(KainError::runtime(format!(
            "Self-host {phase_name} completed with soft failures"
        ))),
        SelfHostPhaseStatus::HardFail => {
            Err(KainError::runtime(format!("Self-host {phase_name} failed")))
        }
    }
}

fn resolve_crates_for_phase(
    profile: &SelfHostSourceProfile,
    module_map: &ModuleMapInventory,
    phase_name: &str,
) -> Vec<String> {
    if let Some(profile_crates) = profile.crates_for_phase(phase_name) {
        if !profile_crates.is_empty() {
            return profile_crates.to_vec();
        }
    }

    match phase_name {
        "phase2" if !module_map.phase2_slice.is_empty() => module_map.phase2_slice.clone(),
        _ => module_map.initial_slice.clone(),
    }
}

fn write_source_correspondence_manifest(
    path: &Path,
    manifest: &SelfHostSourceCorrespondenceManifest,
) -> KainResult<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            KainError::runtime(format!(
                "Failed to create source correspondence manifest dir {}: {}",
                parent.display(),
                err
            ))
        })?;
    }
    let json = serde_json::to_string_pretty(manifest).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize source correspondence manifest {}: {}",
            path.display(),
            err
        ))
    })?;
    fs::write(path, json).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write source correspondence manifest {}: {}",
            path.display(),
            err
        ))
    })
}

fn build_file_mirror_plans(
    crate_root: &Path,
    crate_name: &str,
    module_programs: &[RustSelfHostModuleProgram],
    canonical_source_root: &Path,
    output_mirror_root: &Path,
    roundtrip_rust_root: &Path,
) -> KainResult<Vec<SelfHostFileMirrorPlan>> {
    let mut plans = Vec::with_capacity(module_programs.len());
    for module_program in module_programs {
        let rust_source_relative_path =
            rust_source_relative_path(crate_root, &module_program.module.file_path)?;
        let canonical_kain_relative_path =
            canonical_kain_relative_path(&rust_source_relative_path)?;
        let source_kind = rust_source_kind_for_relative_path(&rust_source_relative_path);
        let module_path = module_path_for_relative_path(&rust_source_relative_path);

        plans.push(SelfHostFileMirrorPlan {
            module_name: module_program.module.module_name.clone(),
            rust_source_path: module_program.module.file_path.clone(),
            rust_source_relative_path: rust_source_relative_path.clone(),
            canonical_kain_path: canonical_source_root
                .join(crate_name)
                .join(&canonical_kain_relative_path),
            output_kain_path: output_mirror_root
                .join(crate_name)
                .join(&canonical_kain_relative_path),
            stage2_roundtrip_rust_path: roundtrip_rust_root
                .join(crate_name)
                .join(&rust_source_relative_path),
            module_path,
            source_kind,
        });
    }
    plans.sort_by(|left, right| {
        left.rust_source_relative_path
            .cmp(&right.rust_source_relative_path)
    });
    Ok(plans)
}

fn rust_source_relative_path(crate_root: &Path, file_path: &Path) -> KainResult<PathBuf> {
    file_path
        .strip_prefix(crate_root)
        .map(Path::to_path_buf)
        .map_err(|_| {
            KainError::runtime(format!(
                "Self-host source file {} is not inside crate root {}",
                file_path.display(),
                crate_root.display()
            ))
        })
}

fn canonical_kain_relative_path(rust_source_relative_path: &Path) -> KainResult<PathBuf> {
    let rust_relative = rust_source_relative_path
        .strip_prefix("src")
        .unwrap_or(rust_source_relative_path);
    let Some(file_name) = rust_relative.file_name() else {
        return Err(KainError::runtime(format!(
            "Cannot derive canonical Kain path from {}",
            rust_source_relative_path.display()
        )));
    };
    let mut relative = rust_relative.to_path_buf();
    let mut output_name = PathBuf::from(file_name);
    output_name.set_extension("kn");
    relative.set_file_name(output_name);
    Ok(relative)
}

fn rust_source_kind_for_relative_path(relative_path: &Path) -> SelfHostRustSourceKind {
    let trimmed = relative_path.strip_prefix("src").unwrap_or(relative_path);
    let file_name = trimmed.file_name().and_then(|value| value.to_str());
    let parent_is_root = trimmed
        .parent()
        .map_or(true, |parent| parent.as_os_str().is_empty());
    match file_name {
        Some("lib.rs") if parent_is_root => SelfHostRustSourceKind::LibRoot,
        Some("main.rs") if parent_is_root => SelfHostRustSourceKind::MainRoot,
        Some("mod.rs") => SelfHostRustSourceKind::ModuleDirectoryRoot,
        _ => SelfHostRustSourceKind::ModuleFile,
    }
}

fn module_path_for_relative_path(relative_path: &Path) -> Vec<String> {
    let trimmed = relative_path.strip_prefix("src").unwrap_or(relative_path);
    let mut segments = trimmed
        .iter()
        .filter_map(|part| part.to_str())
        .map(str::to_string)
        .collect::<Vec<_>>();
    if let Some(last) = segments.last_mut() {
        match last.as_str() {
            "lib.rs" | "main.rs" => {
                segments.pop();
            }
            "mod.rs" => {
                segments.pop();
            }
            other if other.ends_with(".rs") => {
                *last = other.trim_end_matches(".rs").to_string();
            }
            _ => {}
        }
    }
    segments
}

fn emit_selfhost_kain_mirrors(
    module_programs: &[RustSelfHostModuleProgram],
    file_plans: &[SelfHostFileMirrorPlan],
) -> KainResult<SelfHostCrateEmissionArtifacts> {
    let mut programs_by_source_path = BTreeMap::new();
    for module_program in module_programs {
        programs_by_source_path.insert(
            module_program.module.file_path.clone(),
            &module_program.program,
        );
    }

    for plan in file_plans {
        let Some(program) = programs_by_source_path.get(&plan.rust_source_path) else {
            return Err(KainError::runtime(format!(
                "Missing module program for self-host mirror {}",
                plan.rust_source_path.display()
            )));
        };
        let rendered = render_program(program)?;
        write_selfhost_text_file(
            &plan.canonical_kain_path,
            &rendered,
            "self-host canonical Kain mirror",
        )?;
        write_selfhost_text_file(
            &plan.output_kain_path,
            &rendered,
            "self-host output Kain mirror",
        )?;
    }

    Ok(SelfHostCrateEmissionArtifacts {
        mirrored_file_count: file_plans.len(),
    })
}

fn write_selfhost_text_file(path: &Path, content: &str, label: &str) -> KainResult<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            KainError::runtime(format!(
                "Failed to create {} parent directory {}: {}",
                label,
                parent.display(),
                err
            ))
        })?;
    }
    fs::write(path, content).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write {} {}: {}",
            label,
            path.display(),
            err
        ))
    })
}

fn build_crate_source_correspondence(
    crate_name: &str,
    crate_root: &Path,
    file_plans: &[SelfHostFileMirrorPlan],
    aggregate_bundle_path: Option<&Path>,
    aggregate_roundtrip_rust_path: Option<&Path>,
    canonical_kain_root: &Path,
    output_kain_root: &Path,
    roundtrip_tree_root: &Path,
    profile: &SelfHostSourceProfile,
) -> SelfHostCrateCorrespondence {
    let mirrored_files = file_plans
        .iter()
        .map(|plan| SelfHostFileCorrespondence {
            module_name: plan.module_name.clone(),
            rust_source_path: plan.rust_source_path.display().to_string(),
            rust_source_relative_path: plan.rust_source_relative_path.display().to_string(),
            canonical_kain_path: plan.canonical_kain_path.display().to_string(),
            output_kain_path: plan.output_kain_path.display().to_string(),
            stage2_roundtrip_rust_path: plan.stage2_roundtrip_rust_path.display().to_string(),
            module_path: plan.module_path.clone(),
            source_kind: plan.source_kind.as_str().to_string(),
            ownership_state: profile.ownership.default_file_ownership_state.clone(),
            stage2_roundtrip_strategy: match plan.source_kind {
                SelfHostRustSourceKind::MainRoot => {
                    profile.ownership.synthesized_main_root_strategy.clone()
                }
                _ => profile.ownership.roundtrip_strategy.clone(),
            },
        })
        .collect();

    SelfHostCrateCorrespondence {
        crate_name: crate_name.to_string(),
        crate_root: crate_root.display().to_string(),
        aggregate_bundle_path: aggregate_bundle_path.map(|path| path.display().to_string()),
        aggregate_roundtrip_rust_path: aggregate_roundtrip_rust_path
            .map(|path| path.display().to_string()),
        canonical_kain_root: canonical_kain_root.display().to_string(),
        output_kain_root: output_kain_root.display().to_string(),
        roundtrip_rust_tree_root: roundtrip_tree_root.display().to_string(),
        mirrored_files,
    }
}

fn write_roundtrip_rust_tree(
    crate_name: &str,
    _crate_root: &Path,
    roundtrip_rust_root: &Path,
    aggregate_rust_path: &Path,
    rust_source: &str,
    file_plans: &[SelfHostFileMirrorPlan],
    _profile: &SelfHostSourceProfile,
) -> KainResult<CrateRoundtripRustArtifacts> {
    let parsed_file = syn::parse_file(rust_source).map_err(|err| {
        KainError::runtime(format!(
            "Failed to parse generated roundtrip Rust for {}: {}",
            crate_name, err
        ))
    })?;
    let crate_roundtrip_root = roundtrip_rust_root.join(crate_name);
    let src_root = crate_roundtrip_root.join("src");
    fs::create_dir_all(&src_root).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create roundtrip Rust source root {}: {}",
            src_root.display(),
            err
        ))
    })?;

    let lib_rs_path = src_root.join("lib.rs");
    let mut module_plans_by_path = BTreeMap::new();
    let has_main_root = file_plans
        .iter()
        .any(|plan| plan.source_kind == SelfHostRustSourceKind::MainRoot);
    for plan in file_plans {
        if matches!(
            plan.source_kind,
            SelfHostRustSourceKind::ModuleFile | SelfHostRustSourceKind::ModuleDirectoryRoot
        ) {
            module_plans_by_path.insert(plan.module_path.clone(), plan);
        }
    }

    let mut written_files = BTreeSet::new();
    write_split_roundtrip_rust_file(
        &lib_rs_path,
        &[],
        parsed_file.items,
        &module_plans_by_path,
        &mut written_files,
    )?;

    let mut main_rs_path = None;
    if has_main_root {
        let main_path = src_root.join("main.rs");
        write_selfhost_text_file(&main_path, "include!(\"lib.rs\");\n", "stage2 main.rs")?;
        written_files.insert(main_path.clone());
        main_rs_path = Some(main_path);
    }

    Ok(CrateRoundtripRustArtifacts {
        aggregate_rust_path: aggregate_rust_path.to_path_buf(),
        source_tree_root: crate_roundtrip_root,
        main_rs_path,
        file_count: written_files.len(),
    })
}

fn write_split_roundtrip_rust_file(
    file_path: &Path,
    current_module_path: &[String],
    input_items: Vec<syn::Item>,
    module_plans_by_path: &BTreeMap<Vec<String>, &SelfHostFileMirrorPlan>,
    written_files: &mut BTreeSet<PathBuf>,
) -> KainResult<()> {
    let mut file_items = Vec::new();

    for item in input_items {
        match item {
            syn::Item::Mod(mut item_mod) => {
                let Some((brace, inline_items)) = item_mod.content.take() else {
                    file_items.push(syn::Item::Mod(item_mod));
                    continue;
                };
                let mut next_path = current_module_path.to_vec();
                next_path.push(item_mod.ident.to_string());
                if let Some(plan) = module_plans_by_path.get(&next_path) {
                    let mut declaration = item_mod.clone();
                    declaration.content = None;
                    declaration.semi = Some(Default::default());
                    file_items.push(syn::Item::Mod(declaration));
                    write_split_roundtrip_rust_file(
                        &plan.stage2_roundtrip_rust_path,
                        &next_path,
                        inline_items,
                        module_plans_by_path,
                        written_files,
                    )?;
                } else {
                    item_mod.content = Some((brace, inline_items));
                    file_items.push(syn::Item::Mod(item_mod));
                }
            }
            other => file_items.push(other),
        }
    }

    write_pretty_syn_file(file_path, file_items)?;
    written_files.insert(file_path.to_path_buf());
    Ok(())
}

fn write_pretty_syn_file(path: &Path, items: Vec<syn::Item>) -> KainResult<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            KainError::runtime(format!(
                "Failed to create roundtrip Rust parent directory {}: {}",
                parent.display(),
                err
            ))
        })?;
    }
    let rendered = prettyplease::unparse(&syn::File {
        shebang: None,
        attrs: Vec::new(),
        items,
    });
    fs::write(path, rendered).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write roundtrip Rust source file {}: {}",
            path.display(),
            err
        ))
    })
}

#[derive(Debug, Deserialize)]
struct InventoryBundle {
    macro_inventory: MacroInventory,
    module_map: ModuleMapInventory,
    allowlist: AllowlistInventory,
    trait_inventory: TraitInventory,
}

#[derive(Debug, Deserialize)]
struct MacroInventory {
    crates: BTreeMap<String, MacroInventoryCrate>,
}

#[derive(Debug, Deserialize)]
struct MacroInventoryCrate {
    bang_macros: BTreeMap<String, usize>,
    files: BTreeMap<String, Vec<String>>,
}

#[derive(Debug, Deserialize)]
struct ModuleMapInventory {
    initial_slice: Vec<String>,
    #[serde(default)]
    phase2_slice: Vec<String>,
}

#[derive(Debug, Deserialize)]
struct AllowlistInventory {
    phase1_required_direct_lowering: Vec<String>,
    macro_policy: AllowlistMacroPolicy,
}

#[derive(Debug, Deserialize)]
struct AllowlistMacroPolicy {
    reject: Vec<String>,
}

#[derive(Debug, Deserialize)]
struct TraitInventory {
    crates: BTreeMap<String, TraitInventoryCrate>,
}

#[derive(Debug, Deserialize)]
struct TraitInventoryCrate {
    trait_defs: Vec<TraitInventoryEntry>,
    trait_impls: Vec<TraitInventoryEntry>,
    dyn_usages: Vec<TraitInventoryEntry>,
}

#[derive(Debug, Deserialize)]
struct TraitInventoryEntry {
    file: String,
}

fn load_inventories(inventory_dir: &Path) -> KainResult<InventoryBundle> {
    Ok(InventoryBundle {
        macro_inventory: read_inventory_json(inventory_path_for_key(
            inventory_dir,
            "macro_inventory",
        )?)?,
        module_map: read_inventory_json(inventory_path_for_key(inventory_dir, "module_map")?)?,
        allowlist: read_inventory_json(inventory_path_for_key(
            inventory_dir,
            "selfhost_allowlist",
        )?)?,
        trait_inventory: read_inventory_json(inventory_path_for_key(
            inventory_dir,
            "trait_inventory",
        )?)?,
    })
}

fn collect_inventory_input_evidence(
    inventory_dir: &Path,
) -> KainResult<Vec<InventoryInputEvidence>> {
    let mut inputs = Vec::with_capacity(INVENTORY_FILE_SPECS.len());
    for (key, _) in INVENTORY_FILE_SPECS {
        let path = inventory_path_for_key(inventory_dir, key)?;
        let metadata = fs::metadata(&path).map_err(|err| {
            KainError::runtime(format!(
                "Failed to read inventory metadata {}: {}",
                path.display(),
                err
            ))
        })?;
        inputs.push(InventoryInputEvidence {
            inventory_key: (*key).to_string(),
            path: path.display().to_string(),
            byte_size: metadata.len(),
        });
    }
    Ok(inputs)
}

fn inventory_path_for_key(inventory_dir: &Path, key: &str) -> KainResult<PathBuf> {
    let Some((_, file_name)) = INVENTORY_FILE_SPECS
        .iter()
        .find(|(candidate, _)| *candidate == key)
    else {
        return Err(KainError::runtime(format!(
            "Unknown inventory key '{}'; expected one of: {}",
            key,
            INVENTORY_FILE_SPECS
                .iter()
                .map(|(known_key, _)| *known_key)
                .collect::<Vec<_>>()
                .join(", ")
        )));
    };
    Ok(inventory_dir.join(file_name))
}

fn read_inventory_json<T: for<'de> Deserialize<'de>>(path: PathBuf) -> KainResult<T> {
    let raw = fs::read_to_string(&path).map_err(|err| {
        KainError::runtime(format!(
            "Failed to read inventory {}: {}",
            path.display(),
            err
        ))
    })?;
    serde_json::from_str(&raw).map_err(|err| {
        KainError::runtime(format!(
            "Failed to parse inventory {}: {}",
            path.display(),
            err
        ))
    })
}

fn macro_findings_for(
    crate_name: &str,
    macro_inventory: &MacroInventory,
    macro_names: &[String],
) -> Vec<MacroFinding> {
    let mut findings = Vec::new();
    let Some(crate_macros) = macro_inventory.crates.get(crate_name) else {
        return findings;
    };

    for macro_name in macro_names {
        let count = crate_macros
            .bang_macros
            .get(macro_name)
            .copied()
            .unwrap_or(0);
        if count == 0 {
            continue;
        }
        let files = crate_macros
            .files
            .get(macro_name)
            .cloned()
            .unwrap_or_default();
        findings.push(MacroFinding {
            crate_name: crate_name.to_string(),
            macro_name: macro_name.clone(),
            occurrence_count: count,
            files,
        });
    }

    findings
}

fn preserved_required_macro_findings(
    crate_name: &str,
    program: &Program,
    required_macros: &[String],
) -> Vec<MacroFinding> {
    let required = required_macros.iter().cloned().collect::<BTreeSet<_>>();
    let mut counts = BTreeMap::<String, usize>::new();
    for item in &program.items {
        collect_macro_calls_from_item(item, &required, &mut counts);
    }

    counts
        .into_iter()
        .map(|(macro_name, occurrence_count)| MacroFinding {
            crate_name: crate_name.to_string(),
            macro_name,
            occurrence_count,
            files: Vec::new(),
        })
        .collect()
}

fn collect_macro_calls_from_item(
    item: &Item,
    required: &BTreeSet<String>,
    counts: &mut BTreeMap<String, usize>,
) {
    match item {
        Item::Function(function) => {
            collect_macro_calls_from_block(&function.body, required, counts)
        }
        Item::Struct(value) => {
            for field in &value.fields {
                if let Some(default) = &field.default {
                    collect_macro_calls_from_expr(default, required, counts);
                }
            }
        }
        Item::Enum(value) => {
            for variant in &value.variants {
                match &variant.fields {
                    kain_core::ast::VariantFields::Tuple(values) => {
                        for ty in values {
                            collect_macro_calls_from_type(ty, required, counts);
                        }
                    }
                    kain_core::ast::VariantFields::Struct(fields) => {
                        for field in fields {
                            collect_macro_calls_from_type(&field.ty, required, counts);
                        }
                    }
                    kain_core::ast::VariantFields::Unit => {}
                }
            }
        }
        Item::Mod(module) => {
            if let Some(children) = &module.inline {
                for child in children {
                    collect_macro_calls_from_item(child, required, counts);
                }
            }
        }
        Item::Const(value) => {
            collect_macro_calls_from_type(&value.ty, required, counts);
            collect_macro_calls_from_expr(&value.value, required, counts);
        }
        Item::Impl(value) => {
            collect_macro_calls_from_type(&value.target_type, required, counts);
            for method in &value.methods {
                collect_macro_calls_from_block(&method.body, required, counts);
            }
        }
        Item::Trait(value) => {
            for method in &value.methods {
                for param in &method.params {
                    collect_macro_calls_from_type(&param.ty, required, counts);
                }
                if let Some(return_type) = &method.return_type {
                    collect_macro_calls_from_type(return_type, required, counts);
                }
                if let Some(default_impl) = &method.default_impl {
                    collect_macro_calls_from_block(default_impl, required, counts);
                }
            }
        }
        Item::TypeAlias(value) => collect_macro_calls_from_type(&value.target, required, counts),
        Item::Comptime(value) => collect_macro_calls_from_block(&value.body, required, counts),
        _ => {}
    }
}

fn collect_macro_calls_from_block(
    block: &Block,
    required: &BTreeSet<String>,
    counts: &mut BTreeMap<String, usize>,
) {
    for stmt in &block.stmts {
        collect_macro_calls_from_stmt(stmt, required, counts);
    }
}

fn collect_macro_calls_from_stmt(
    stmt: &Stmt,
    required: &BTreeSet<String>,
    counts: &mut BTreeMap<String, usize>,
) {
    match stmt {
        Stmt::Let { ty, value, .. } => {
            if let Some(ty) = ty {
                collect_macro_calls_from_type(ty, required, counts);
            }
            if let Some(value) = value {
                collect_macro_calls_from_expr(value, required, counts);
            }
        }
        Stmt::Expr(expr) => collect_macro_calls_from_expr(expr, required, counts),
        Stmt::Return(value, _) | Stmt::Break(value, _) => {
            if let Some(value) = value {
                collect_macro_calls_from_expr(value, required, counts);
            }
        }
        Stmt::For { iter, body, .. } => {
            collect_macro_calls_from_expr(iter, required, counts);
            collect_macro_calls_from_block(body, required, counts);
        }
        Stmt::While {
            condition, body, ..
        } => {
            collect_macro_calls_from_expr(condition, required, counts);
            collect_macro_calls_from_block(body, required, counts);
        }
        Stmt::Loop { body, .. } => collect_macro_calls_from_block(body, required, counts),
        Stmt::Item(item) => collect_macro_calls_from_item(item, required, counts),
        Stmt::Continue(_) => {}
    }
}

fn collect_macro_calls_from_expr(
    expr: &Expr,
    required: &BTreeSet<String>,
    counts: &mut BTreeMap<String, usize>,
) {
    match expr {
        Expr::MacroCall { name, args, .. } => {
            if required.contains(name) {
                *counts.entry(name.clone()).or_insert(0) += 1;
            }
            for arg in args {
                collect_macro_calls_from_expr(arg, required, counts);
            }
        }
        Expr::Binary { left, right, .. } => {
            collect_macro_calls_from_expr(left, required, counts);
            collect_macro_calls_from_expr(right, required, counts);
        }
        Expr::Unary { operand, .. }
        | Expr::Ref { value: operand, .. }
        | Expr::AddrOf { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Try(operand, _)
        | Expr::Await(operand, _)
        | Expr::AsyncBlock(operand, _)
        | Expr::Comptime(operand, _)
        | Expr::Paren(operand, _) => collect_macro_calls_from_expr(operand, required, counts),
        Expr::Call { callee, args, .. } => {
            collect_macro_calls_from_expr(callee, required, counts);
            for arg in args {
                collect_macro_calls_from_expr(&arg.value, required, counts);
            }
        }
        Expr::StageCall { args, .. } => {
            for arg in args {
                collect_macro_calls_from_expr(&arg.value, required, counts);
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            collect_macro_calls_from_expr(receiver, required, counts);
            for arg in args {
                collect_macro_calls_from_expr(&arg.value, required, counts);
            }
        }
        Expr::Field { object, .. } => collect_macro_calls_from_expr(object, required, counts),
        Expr::Index { object, index, .. } => {
            collect_macro_calls_from_expr(object, required, counts);
            collect_macro_calls_from_expr(index, required, counts);
        }
        Expr::Assign { target, value, .. } => {
            collect_macro_calls_from_expr(target, required, counts);
            collect_macro_calls_from_expr(value, required, counts);
        }
        Expr::Struct { fields, .. } | Expr::AggregateInit { fields, .. } => {
            for (_, value) in fields {
                collect_macro_calls_from_expr(value, required, counts);
            }
        }
        Expr::EnumVariant { fields, .. } => match fields {
            EnumVariantFields::Unit => {}
            EnumVariantFields::Tuple(values) => {
                for value in values {
                    collect_macro_calls_from_expr(value, required, counts);
                }
            }
            EnumVariantFields::Struct(fields) => {
                for (_, value) in fields {
                    collect_macro_calls_from_expr(value, required, counts);
                }
            }
        },
        Expr::Array(values, _) | Expr::Tuple(values, _) | Expr::FString(values, _) => {
            for value in values {
                collect_macro_calls_from_expr(value, required, counts);
            }
        }
        Expr::Range { start, end, .. } => {
            if let Some(start) = start {
                collect_macro_calls_from_expr(start, required, counts);
            }
            if let Some(end) = end {
                collect_macro_calls_from_expr(end, required, counts);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            collect_macro_calls_from_expr(condition, required, counts);
            collect_macro_calls_from_block(then_branch, required, counts);
            if let Some(else_branch) = else_branch {
                collect_macro_calls_from_else_branch(else_branch, required, counts);
            }
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            collect_macro_calls_from_expr(scrutinee, required, counts);
            for arm in arms {
                collect_macro_calls_from_match_arm(arm, required, counts);
            }
        }
        Expr::Lambda {
            params,
            return_type,
            body,
            ..
        } => {
            for param in params {
                collect_macro_calls_from_type(&param.ty, required, counts);
                if let Some(default) = &param.default {
                    collect_macro_calls_from_expr(default, required, counts);
                }
            }
            if let Some(return_type) = return_type {
                collect_macro_calls_from_type(return_type, required, counts);
            }
            collect_macro_calls_from_expr(body, required, counts);
        }
        Expr::PtrOffset {
            pointer,
            offset,
            element_ty,
            ..
        } => {
            collect_macro_calls_from_expr(pointer, required, counts);
            collect_macro_calls_from_expr(offset, required, counts);
            if let Some(element_ty) = element_ty {
                collect_macro_calls_from_type(element_ty, required, counts);
            }
        }
        Expr::MemLoad {
            pointer, load_ty, ..
        } => {
            collect_macro_calls_from_expr(pointer, required, counts);
            if let Some(load_ty) = load_ty {
                collect_macro_calls_from_type(load_ty, required, counts);
            }
        }
        Expr::MemStore {
            pointer,
            value,
            store_ty,
            ..
        } => {
            collect_macro_calls_from_expr(pointer, required, counts);
            collect_macro_calls_from_expr(value, required, counts);
            if let Some(store_ty) = store_ty {
                collect_macro_calls_from_type(store_ty, required, counts);
            }
        }
        Expr::SizeOfType { target, .. }
        | Expr::AlignOfType { target, .. }
        | Expr::Alloca { ty: target, .. }
        | Expr::Uninit { ty: target, .. } => {
            collect_macro_calls_from_type(target, required, counts)
        }
        Expr::Alloc { size, ty, .. } => {
            collect_macro_calls_from_expr(size, required, counts);
            if let Some(ty) = ty {
                collect_macro_calls_from_type(ty, required, counts);
            }
        }
        Expr::Realloc {
            pointer, size, ty, ..
        } => {
            collect_macro_calls_from_expr(pointer, required, counts);
            collect_macro_calls_from_expr(size, required, counts);
            if let Some(ty) = ty {
                collect_macro_calls_from_type(ty, required, counts);
            }
        }
        Expr::Cast { value, target, .. } => {
            collect_macro_calls_from_expr(value, required, counts);
            collect_macro_calls_from_type(target, required, counts);
        }
        Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } => {
            for (_, value) in init {
                collect_macro_calls_from_expr(value, required, counts);
            }
        }
        Expr::Block(block, _) => collect_macro_calls_from_block(block, required, counts),
        Expr::Return(value, _) | Expr::Break(value, _) => {
            if let Some(value) = value {
                collect_macro_calls_from_expr(value, required, counts);
            }
        }
        Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _)
        | Expr::Continue(_)
        | Expr::JSX(_, _) => {}
    }
}

fn collect_macro_calls_from_else_branch(
    else_branch: &ElseBranch,
    required: &BTreeSet<String>,
    counts: &mut BTreeMap<String, usize>,
) {
    match else_branch {
        ElseBranch::Else(block) => collect_macro_calls_from_block(block, required, counts),
        ElseBranch::ElseIf(expr, block, nested) => {
            collect_macro_calls_from_expr(expr, required, counts);
            collect_macro_calls_from_block(block, required, counts);
            if let Some(nested) = nested {
                collect_macro_calls_from_else_branch(nested, required, counts);
            }
        }
    }
}

fn collect_macro_calls_from_match_arm(
    arm: &MatchArm,
    required: &BTreeSet<String>,
    counts: &mut BTreeMap<String, usize>,
) {
    collect_macro_calls_from_pattern(&arm.pattern, required, counts);
    if let Some(guard) = &arm.guard {
        collect_macro_calls_from_expr(guard, required, counts);
    }
    collect_macro_calls_from_expr(&arm.body, required, counts);
}

fn collect_macro_calls_from_pattern(
    pattern: &Pattern,
    required: &BTreeSet<String>,
    counts: &mut BTreeMap<String, usize>,
) {
    match pattern {
        Pattern::Literal(expr) => collect_macro_calls_from_expr(expr, required, counts),
        Pattern::Struct { fields, .. } => {
            for (_, pattern) in fields {
                collect_macro_calls_from_pattern(pattern, required, counts);
            }
        }
        Pattern::Tuple(values, _) | Pattern::Or(values, _) => {
            for value in values {
                collect_macro_calls_from_pattern(value, required, counts);
            }
        }
        Pattern::Variant { fields, .. } => match fields {
            VariantPatternFields::Unit => {}
            VariantPatternFields::Tuple(values) => {
                for value in values {
                    collect_macro_calls_from_pattern(value, required, counts);
                }
            }
            VariantPatternFields::Struct(fields) => {
                for (_, value) in fields {
                    collect_macro_calls_from_pattern(value, required, counts);
                }
            }
        },
        Pattern::Slice { patterns, .. } => {
            for value in patterns {
                collect_macro_calls_from_pattern(value, required, counts);
            }
        }
        Pattern::Range { start, end, .. } => {
            if let Some(start) = start {
                collect_macro_calls_from_expr(start, required, counts);
            }
            if let Some(end) = end {
                collect_macro_calls_from_expr(end, required, counts);
            }
        }
        Pattern::Wildcard(_) | Pattern::Binding { .. } => {}
    }
}

fn collect_macro_calls_from_type(
    ty: &Type,
    required: &BTreeSet<String>,
    counts: &mut BTreeMap<String, usize>,
) {
    match ty {
        Type::Named { generics, .. } | Type::Impl { generics, .. } => {
            for generic in generics {
                collect_macro_calls_from_type(generic, required, counts);
            }
        }
        Type::Tuple(values, _) => {
            for value in values {
                collect_macro_calls_from_type(value, required, counts);
            }
        }
        Type::Array(inner, _, _)
        | Type::Slice(inner, _)
        | Type::Option(inner, _)
        | Type::Ref { inner, .. }
        | Type::Ptr { inner, .. } => collect_macro_calls_from_type(inner, required, counts),
        Type::Function {
            params,
            return_type,
            ..
        } => {
            for param in params {
                collect_macro_calls_from_type(param, required, counts);
            }
            collect_macro_calls_from_type(return_type, required, counts);
        }
        Type::Result(ok, err, _) => {
            collect_macro_calls_from_type(ok, required, counts);
            collect_macro_calls_from_type(err, required, counts);
        }
        Type::Infer(_) | Type::Never(_) | Type::Unit(_) => {}
    }
}

fn build_diagnostic_category_summary(
    crate_results: &[CratePhase1Result],
) -> BTreeMap<String, usize> {
    let mut counts = BTreeMap::new();
    for crate_result in crate_results {
        for diagnostic in &crate_result.diagnostics {
            let category = classify_diagnostic(diagnostic);
            *counts.entry(category).or_insert(0) += 1;
        }
    }
    counts
}

fn build_trait_dyn_summary(
    trait_inventory: &TraitInventory,
    crates_processed: &[String],
) -> Vec<TraitDynSummary> {
    crates_processed
        .iter()
        .filter_map(|crate_name| {
            let crate_info = trait_inventory.crates.get(crate_name)?;
            Some(TraitDynSummary {
                crate_name: crate_name.clone(),
                trait_def_count: crate_info.trait_defs.len(),
                trait_impl_count: crate_info.trait_impls.len(),
                dyn_usage_count: crate_info.dyn_usages.len(),
                dyn_usage_files: crate_info
                    .dyn_usages
                    .iter()
                    .map(|entry| entry.file.clone())
                    .collect(),
            })
        })
        .collect()
}

fn determine_phase_status(
    crate_results: &[CratePhase1Result],
    required_preserved: &[MacroFinding],
) -> SelfHostPhaseStatus {
    if crate_results
        .iter()
        .any(|crate_result| !crate_result.import_success)
        || !required_preserved.is_empty()
    {
        return SelfHostPhaseStatus::HardFail;
    }

    if crate_results
        .iter()
        .any(|crate_result| !crate_result.rejected_macros_found.is_empty())
    {
        return SelfHostPhaseStatus::SoftFail;
    }

    SelfHostPhaseStatus::Pass
}

struct Stage2BuildResult {
    success: bool,
    artifact_path: Option<PathBuf>,
    log_path: PathBuf,
    exit_code: Option<i32>,
}

struct Stage2WorkspaceAssembly {
    workspace_path: PathBuf,
    crates: Vec<Stage2WorkspaceCrateEvidence>,
}

fn write_report_files(
    phase_name: &str,
    output_dir: &Path,
    report: &SelfHostPhase1Report,
) -> KainResult<()> {
    let json_path = output_dir.join(format!("{phase_name}_report.json"));
    let markdown_path = output_dir.join(format!("{phase_name}_report.md"));
    let json = serde_json::to_string_pretty(report).map_err(|err| {
        KainError::runtime(format!("Failed to serialize {phase_name} report: {}", err))
    })?;
    fs::write(&json_path, json).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write {phase_name} report JSON {}: {}",
            json_path.display(),
            err
        ))
    })?;
    fs::write(
        &markdown_path,
        render_phase_markdown(&format!("Self-Host {}", phase_name.to_uppercase()), report),
    )
    .map_err(|err| {
        KainError::runtime(format!(
            "Failed to write {phase_name} report Markdown {}: {}",
            markdown_path.display(),
            err
        ))
    })?;
    Ok(())
}

fn print_summary(phase_name: &str, report: &SelfHostPhase1Report) {
    println!("🧬 Self-host {}", phase_name);
    println!("   Crates: {}", report.crates_processed.join(", "));
    println!(
        "   Profile: {} ({})",
        report.profile_name, report.profile_path
    );
    if report.force_mode {
        println!("   Force mode: true");
    }
    println!(
        "   Status: {}",
        match report.final_phase_status {
            SelfHostPhaseStatus::Pass => "pass",
            SelfHostPhaseStatus::SoftFail => "soft_fail",
            SelfHostPhaseStatus::HardFail => "hard_fail",
        }
    );
    println!("   Canonical source root: {}", report.canonical_source_root);
    println!("   Output mirror root: {}", report.output_mirror_root);
    println!(
        "   Source correspondence manifest: {}",
        report.source_correspondence_manifest_path
    );
    if let Some(path) = &report.stage2_workspace_path {
        println!("   Stage2 workspace: {}", path);
    }
    if let Some(path) = &report.stage2_build_artifact {
        println!("   Stage2 artifact: {}", path);
    }
    if let Some(path) = &report.stage2_build_log_path {
        println!("   Stage2 build log: {}", path);
    }
    if let Some(success) = report.stage2_build_success {
        println!("   Stage2 build: {}", if success { "pass" } else { "fail" });
    }
    if let Some(exit_code) = report.stage2_build_exit_code {
        println!("   Stage2 build exit code: {}", exit_code);
    }
    if let Some(stage2_error) = &report.stage2_error {
        println!("   Stage2 error: {}", stage2_error);
    }
    println!(
        "   Report JSON: {}",
        Path::new(&report.output_dir)
            .join(format!("{phase_name}_report.json"))
            .display()
    );
    println!(
        "   Report MD: {}",
        Path::new(&report.output_dir)
            .join(format!("{phase_name}_report.md"))
            .display()
    );
}

fn compile_kn_source_to_rust(source: &str) -> KainResult<String> {
    let typed_program = crate::frontend_to_typed_program(source, crate::CompileTarget::Rust)?;
    #[cfg(feature = "sys")]
    {
        kain_sys_codegen::generate_rust(&typed_program).map_err(|err| {
            KainError::runtime(format!(
                "Failed to generate Rust self-host roundtrip: {}",
                err
            ))
        })
    }
    #[cfg(not(feature = "sys"))]
    {
        let _ = typed_program;
        Err(KainError::runtime(
            "Rust self-host roundtrip requires cli sys feature",
        ))
    }
}

fn assemble_stage2_workspace(
    repo_root: &Path,
    workspace_dir: &Path,
    crates_processed: &[String],
    roundtrip_rust_outputs: &BTreeMap<String, CrateRoundtripRustArtifacts>,
) -> KainResult<Stage2WorkspaceAssembly> {
    let workspace_dir = prepare_stage2_workspace_dir(workspace_dir)?;
    fs::create_dir_all(workspace_dir.join("crates")).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create stage2 workspace {}: {}",
            workspace_dir.display(),
            err
        ))
    })?;

    let root_manifest: Value = toml::from_str(
        &fs::read_to_string(repo_root.join("Cargo.toml")).map_err(|err| {
            KainError::runtime(format!("Failed to read workspace Cargo.toml: {}", err))
        })?,
    )
    .map_err(|err| KainError::runtime(format!("Failed to parse workspace Cargo.toml: {}", err)))?;

    let stage2_set = crates_processed.iter().cloned().collect::<BTreeSet<_>>();
    let root_toml =
        render_root_workspace_toml(crates_processed, &root_manifest, &repo_root, &stage2_set)?;
    fs::write(workspace_dir.join("Cargo.toml"), root_toml).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write stage2 workspace Cargo.toml: {}",
            err
        ))
    })?;

    let mut stage2_workspace_crates = Vec::with_capacity(crates_processed.len());

    for crate_name in crates_processed {
        let roundtrip_artifacts = roundtrip_rust_outputs.get(crate_name).ok_or_else(|| {
            KainError::runtime(format!(
                "Missing roundtrip Rust output for stage2 crate {crate_name}"
            ))
        })?;
        let crate_dir = workspace_dir.join("crates").join(crate_name);
        fs::create_dir_all(&crate_dir).map_err(|err| {
            KainError::runtime(format!(
                "Failed to create stage2 crate dir {}: {}",
                crate_dir.display(),
                err
            ))
        })?;

        let original_manifest_path = repo_root.join("crates").join(crate_name).join("Cargo.toml");
        let original_manifest = fs::read_to_string(&original_manifest_path).map_err(|err| {
            KainError::runtime(format!(
                "Failed to read {}: {}",
                original_manifest_path.display(),
                err
            ))
        })?;
        let rewritten_manifest = rewrite_crate_manifest(
            &original_manifest,
            &repo_root.join("crates").join(crate_name),
            crate_name,
            &stage2_set,
        )?;
        let stage2_manifest_path = crate_dir.join("Cargo.toml");
        fs::write(&stage2_manifest_path, rewritten_manifest).map_err(|err| {
            KainError::runtime(format!(
                "Failed to write stage2 crate manifest for {}: {}",
                crate_name, err
            ))
        })?;

        let source_tree_src_dir = roundtrip_artifacts.source_tree_root.join("src");
        let stage2_src_dir = crate_dir.join("src");
        copy_directory_recursive(&source_tree_src_dir, &stage2_src_dir)?;

        let aggregate_source_metadata = fs::metadata(&roundtrip_artifacts.aggregate_rust_path)
            .map_err(|err| {
                KainError::runtime(format!(
                    "Failed to stat roundtrip Rust {}: {}",
                    roundtrip_artifacts.aggregate_rust_path.display(),
                    err
                ))
            })?;
        let lib_rs_path = stage2_src_dir.join("lib.rs");
        let main_rs_path = roundtrip_artifacts
            .main_rs_path
            .as_ref()
            .map(|_| stage2_src_dir.join("main.rs").display().to_string());
        stage2_workspace_crates.push(Stage2WorkspaceCrateEvidence {
            crate_name: crate_name.clone(),
            source_roundtrip_path: roundtrip_artifacts
                .aggregate_rust_path
                .display()
                .to_string(),
            source_roundtrip_byte_size: aggregate_source_metadata.len(),
            source_tree_root: roundtrip_artifacts.source_tree_root.display().to_string(),
            roundtrip_file_count: roundtrip_artifacts.file_count,
            manifest_path: stage2_manifest_path.display().to_string(),
            lib_rs_path: lib_rs_path.display().to_string(),
            main_rs_path,
        });
    }

    Ok(Stage2WorkspaceAssembly {
        workspace_path: workspace_dir,
        crates: stage2_workspace_crates,
    })
}

fn copy_directory_recursive(source_dir: &Path, target_dir: &Path) -> KainResult<()> {
    if !source_dir.exists() {
        return Err(KainError::runtime(format!(
            "Roundtrip Rust source tree does not exist: {}",
            source_dir.display()
        )));
    }
    fs::create_dir_all(target_dir).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create stage2 source directory {}: {}",
            target_dir.display(),
            err
        ))
    })?;
    for entry in fs::read_dir(source_dir).map_err(|err| {
        KainError::runtime(format!(
            "Failed to read roundtrip Rust source directory {}: {}",
            source_dir.display(),
            err
        ))
    })? {
        let entry = entry.map_err(KainError::Io)?;
        let source_path = entry.path();
        let target_path = target_dir.join(entry.file_name());
        let metadata = entry.metadata().map_err(KainError::Io)?;
        if metadata.is_dir() {
            copy_directory_recursive(&source_path, &target_path)?;
        } else {
            fs::copy(&source_path, &target_path).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to copy roundtrip Rust file {} to {}: {}",
                    source_path.display(),
                    target_path.display(),
                    err
                ))
            })?;
        }
    }
    Ok(())
}

fn prepare_stage2_workspace_dir(base_dir: &Path) -> KainResult<PathBuf> {
    if !base_dir.exists() {
        return Ok(base_dir.to_path_buf());
    }

    match fs::remove_dir_all(base_dir) {
        Ok(()) => Ok(base_dir.to_path_buf()),
        Err(_) => {
            let parent = base_dir.parent().unwrap_or_else(|| Path::new("."));
            let stem = base_dir
                .file_name()
                .and_then(|name| name.to_str())
                .unwrap_or("stage2_workspace");
            for index in 1..=32 {
                let candidate = parent.join(format!("{stem}_run_{index}"));
                if candidate.exists() {
                    continue;
                }
                return Ok(candidate);
            }
            Err(KainError::runtime(format!(
                "Failed to prepare a writable stage2 workspace next to {}",
                base_dir.display()
            )))
        }
    }
}

fn render_root_workspace_toml(
    crates_processed: &[String],
    root_manifest: &Value,
    repo_root: &Path,
    stage2_crates: &BTreeSet<String>,
) -> KainResult<String> {
    let mut root = String::new();
    root.push_str("[workspace]\n");
    root.push_str("members = [\n");
    for crate_name in crates_processed {
        root.push_str(&format!("    \"crates/{}\",\n", crate_name));
    }
    let resolver = root_manifest
        .get("workspace")
        .and_then(|v| v.get("resolver"))
        .and_then(Value::as_str)
        .unwrap_or("2");
    root.push_str("]\n");
    root.push_str(&format!("resolver = \"{}\"\n\n", resolver));

    if let Some(package) = root_manifest
        .get("workspace")
        .and_then(|v| v.get("package"))
    {
        root.push_str("[workspace.package]\n");
        let package_table = package.as_table().ok_or_else(|| {
            KainError::runtime("workspace.package must be a TOML table".to_string())
        })?;
        for (key, value) in package_table {
            root.push_str(&format!("{} = {}\n", key, render_toml_inline_value(value)?));
        }
        root.push('\n');
    }

    if let Some(dependencies) = root_manifest
        .get("workspace")
        .and_then(|v| v.get("dependencies"))
    {
        root.push_str("[workspace.dependencies]\n");
        if let Value::Table(table) = dependencies {
            let mut rewritten = table.clone();
            rewrite_dependency_table(&mut rewritten, repo_root, stage2_crates)?;
            for (key, value) in &rewritten {
                root.push_str(&format!(
                    "{} = {}\n",
                    key,
                    render_toml_inline_value(value).map_err(|err| {
                        KainError::runtime(format!(
                            "Failed to serialize workspace dependency {}: {}",
                            key, err
                        ))
                    })?
                ));
            }
        }
    }

    Ok(root)
}

fn rewrite_crate_manifest(
    original_manifest: &str,
    original_crate_dir: &Path,
    crate_name: &str,
    stage2_crates: &BTreeSet<String>,
) -> KainResult<String> {
    let mut manifest: Value = toml::from_str(original_manifest).map_err(|err| {
        KainError::runtime(format!(
            "Failed to parse crate manifest for {}: {}",
            crate_name, err
        ))
    })?;

    rewrite_stage2_package_version(&mut manifest)?;

    for key in ["dependencies", "dev-dependencies", "build-dependencies"] {
        if let Some(table) = manifest.get_mut(key).and_then(Value::as_table_mut) {
            rewrite_dependency_table(table, original_crate_dir, stage2_crates)?;
        }
    }

    toml::to_string_pretty(&manifest).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize crate manifest for {}: {}",
            crate_name, err
        ))
    })
}

fn rewrite_stage2_package_version(manifest: &mut Value) -> KainResult<()> {
    let Some(package) = manifest.get_mut("package").and_then(Value::as_table_mut) else {
        return Ok(());
    };
    let Some(version) = package.get_mut("version") else {
        return Ok(());
    };
    let Some(version_str) = version.as_str() else {
        return Err(KainError::runtime(
            "package.version must be a string for stage2 manifest rewriting".to_string(),
        ));
    };
    if version_str.contains("-selfhost.") {
        return Ok(());
    }
    *version = Value::String(format!("{version_str}{SELFHOST_STAGE2_VERSION_SUFFIX}"));
    Ok(())
}

fn rewrite_dependency_table(
    table: &mut toml::map::Map<String, Value>,
    original_crate_dir: &Path,
    stage2_crates: &BTreeSet<String>,
) -> KainResult<()> {
    for (dep_name, value) in table.iter_mut() {
        let Some(dep_table) = value.as_table_mut() else {
            continue;
        };
        let Some(path_value) = dep_table.get_mut("path") else {
            continue;
        };
        let Some(path_str) = path_value.as_str() else {
            continue;
        };
        let resolved = original_crate_dir.join(path_str);
        let new_path = if stage2_crates.contains(dep_name) {
            format!("../{}", dep_name)
        } else {
            resolved
                .canonicalize()
                .unwrap_or(resolved)
                .display()
                .to_string()
        };
        *path_value = Value::String(new_path);
    }
    Ok(())
}

fn render_toml_inline_value(value: &Value) -> KainResult<String> {
    match value {
        Value::String(text) => Ok(format!("{text:?}")),
        Value::Integer(number) => Ok(number.to_string()),
        Value::Float(number) => Ok(number.to_string()),
        Value::Boolean(flag) => Ok(flag.to_string()),
        Value::Datetime(datetime) => Ok(datetime.to_string()),
        Value::Array(values) => {
            let rendered = values
                .iter()
                .map(render_toml_inline_value)
                .collect::<KainResult<Vec<_>>>()?;
            Ok(format!("[{}]", rendered.join(", ")))
        }
        Value::Table(table) => {
            let rendered = table
                .iter()
                .map(|(key, value)| {
                    render_toml_inline_value(value).map(|rendered| format!("{key} = {rendered}"))
                })
                .collect::<KainResult<Vec<_>>>()?;
            Ok(format!("{{ {} }}", rendered.join(", ")))
        }
    }
}

fn build_stage2_workspace(workspace_dir: &Path) -> KainResult<Stage2BuildResult> {
    let build_log = workspace_dir.join("stage2_build.log");
    let output = Command::new("cargo")
        .args(["build", "-p", "cli", "--bin", "kain"])
        .current_dir(workspace_dir)
        .output()
        .map_err(|err| {
            KainError::runtime(format!(
                "Failed to run cargo build in stage2 workspace: {}",
                err
            ))
        })?;

    let mut log = String::new();
    log.push_str(&String::from_utf8_lossy(&output.stdout));
    log.push_str(&String::from_utf8_lossy(&output.stderr));
    fs::write(&build_log, log).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write stage2 build log {}: {}",
            build_log.display(),
            err
        ))
    })?;

    let artifact = workspace_dir
        .join("target")
        .join("debug")
        .join(if cfg!(windows) { "kain.exe" } else { "kain" });
    Ok(Stage2BuildResult {
        success: output.status.success(),
        artifact_path: artifact.exists().then_some(artifact),
        log_path: build_log,
        exit_code: output.status.code(),
    })
}

fn render_program(program: &Program) -> KainResult<String> {
    let mut output = String::new();
    for item in &program.items {
        write_item(&mut output, item, 0)?;
    }
    Ok(repair_selfhost_bundle(output))
}

fn repair_selfhost_bundle(source: String) -> String {
    let source = repair_named_function_block(&source, "fn item_span(", |_| {
        [
            "fn item_span(item: &Item) -> crate::span::Span:",
            "    match item:",
            "        Item::Function(f) => f.span.clone()",
            "        Item::Struct(s) => s.span.clone()",
            "        Item::Enum(e) => e.span.clone()",
            "        Item::Component(c) => c.span.clone()",
            "        Item::Shader(s) => s.span.clone()",
            "        Item::Actor(a) => a.span.clone()",
            "        Item::Comptime(b) => b.span.clone()",
            "        Item::Const(c) => c.span.clone()",
            "        Item::Macro(m) => m.span.clone()",
            "        Item::Use(u) => u.span.clone()",
            "        Item::Mod(m) => m.span.clone()",
            "        Item::Impl(i) => i.span.clone()",
            "        Item::Test(t) => t.span.clone()",
            "        _ => Span__new_(0, 0)",
        ]
        .join("\n")
    });
    let source = repair_named_function_block(&source, "fn lower_type_memory(", |_| {
        [
            "fn lower_type_memory(ty: &Type) -> Type:",
            "    match ty:",
            "        Type::Ptr { span: span } => Type::Named { name: \"Int\".to_string(), generics: [], span: span.clone() }",
            "        Type::Array(inner, size, span) => Type__Array(Box__new_(lower_type_memory(inner)), (*size), span.clone())",
            "        Type::Slice(inner, span) => Type__Slice(Box__new_(lower_type_memory(inner)), span.clone())",
            "        Type::Tuple(types, span) => Type__Tuple(types.iter().map(lower_type_memory).collect(), span.clone())",
            "        Type::Ref { mutable_: mutable_, inner: inner, lifetime: lifetime, span: span } => Type::Ref { mutable_: (*mutable_), inner: Box__new_(lower_type_memory(inner)), lifetime: lifetime.clone(), span: span.clone() }",
            "        Type::Function { params: params, return_type: return_type, effects: effects, span: span } => Type::Function { params: params.iter().map(lower_type_memory).collect(), return_type: Box__new_(lower_type_memory(return_type)), effects: effects.clone(), span: span.clone() }",
            "        Type::Option(inner, span) => Type__Option(Box__new_(lower_type_memory(inner)), span.clone())",
            "        Type::Result(ok, err, span) => Type__Result(Box__new_(lower_type_memory(ok)), Box__new_(lower_type_memory(err)), span.clone())",
            "        Type::Named { name: name, generics: generics, span: span } => Type::Named { name: name.clone(), generics: generics.iter().map(lower_type_memory).collect(), span: span.clone() }",
            "        Type::Impl { trait_name: trait_name, generics: generics, span: span } => Type::Impl { trait_name: trait_name.clone(), generics: generics.iter().map(lower_type_memory).collect(), span: span.clone() }",
            "        _ => ty.clone()",
        ]
        .join("\n")
    });
    let source = repair_named_function_block(&source, "fn select_converge_lane<", |_| {
        [
            "fn select_converge_lane(env: &Env, converge: &ConvergeDef) -> &ConvergeLane:",
            "    for lane in &converge.fast_lanes:",
            "        match lane.selector.as_ref():",
            "            Some(selector) =>",
            "                if converge_selector_matches(env, selector):",
            "                    return lane",
            "            None => return lane",
            "    &converge.spec_lane",
        ]
        .join("\n")
    });
    let source = repair_named_function_block(
        &source,
        "fn synthesize_converge_sample_args(",
        |_| {
            [
            "fn synthesize_converge_sample_args(converge: &ConvergeDef, sample_index: u32) -> Result<Array<Value>, Error>:",
            "    let mut synthesizer = DeterministicValueSynthesizer__new_(stable_converge_sample_seed(&converge.name, sample_index))",
            "    let mut args = Vec__new_()",
            "    for param in &converge.params:",
            "        args.push(synthesize_value_for_type(&param.ty, &mut synthesizer)?)",
            "    Ok(args)",
        ]
        .join("\n")
        },
    );
    let source = repair_named_function_block(
        &source,
        "fn extract_compute_metadata_from_comptime_block(",
        |_| {
            [
                "fn extract_compute_metadata_from_comptime_block(block: &Block) -> Result<Option<ComputeMetadata>, ComputeMetadataError>:",
                "    for stmt in &(*block).stmts:",
                "        match stmt:",
                "            Stmt::Let { pattern: Pattern::Binding { name: name }, value: Some(expr) } =>",
                "                if is_compute_plan_binding(name):",
                "                    return parse_compute_metadata_expr(expr).map(fn(metadata): Option::Some(metadata))",
                "                else:",
                "                    ()",
                "            Stmt::Item(item) =>",
                "                match item:",
                "                    Item::Comptime(comptime_) =>",
                "                        match (extract_compute_metadata_from_comptime_block(&comptime_.body)?):",
                "                            Some(metadata) => return Result::Ok(Option::Some(metadata))",
                "                            _ => ()",
                "                    _ => ()",
                "            _ => ()",
                "    Result::Ok(none)",
            ]
            .join("\n")
        },
    );
    let source = source.replace(
        "                _ => let __selfhost_empty = none",
        "                _ =>\n                    let __selfhost_empty = none",
    );
    let source = source.replace(
        "        _ => let __selfhost_empty = none",
        "        _ =>\n            let __selfhost_empty = none",
    );
    let source = source.replace(
        "    fn infer_expr_type(expr: &Expr, ctx: &FunctionMemoryCtx) -> Option<Type>:\n    match expr:",
        "    fn infer_expr_type(expr: &Expr, ctx: &FunctionMemoryCtx) -> Option<Type>:\n        match expr:",
    );
    let source = source.replace(
        "    fn rewrite_access_to_self(block: &mut Block, fields: &Map<String, ResolvedType>):\n    for stmt in &mut block.stmts:",
        "    fn rewrite_access_to_self(block: &mut Block, fields: &Map<String, ResolvedType>):\n        for stmt in &mut block.stmts:",
    );
    let source = source.replace(
        "    fn rewrite_stmt(stmt: &mut Stmt, fields: &Map<String, ResolvedType>):\n    match stmt:",
        "    fn rewrite_stmt(stmt: &mut Stmt, fields: &Map<String, ResolvedType>):\n        match stmt:",
    );
    let source = source.replace(
        "    fn rewrite_expr(expr: &mut Expr, fields: &Map<String, ResolvedType>):\n    match expr:",
        "    fn rewrite_expr(expr: &mut Expr, fields: &Map<String, ResolvedType>):\n        match expr:",
    );
    let source = source.replace(
        "    fn eval_jsx(env: &mut crate::runtime::Env, node: &crate::ast::JSXNode) -> Result<crate::runtime::Value, Error>:\n    match node:",
        "    fn eval_jsx(env: &mut crate::runtime::Env, node: &crate::ast::JSXNode) -> Result<crate::runtime::Value, Error>:\n        match node:",
    );
    let source = source.replace(
        "    fn attrs_to_props_map(attrs: &[UIAttr]) -> Map<String, crate::runtime::Value>:\n    let mut props = std__collections__HashMap__new_()",
        "    fn attrs_to_props_map(attrs: &[UIAttr]) -> Map<String, crate::runtime::Value>:\n        let mut props = std__collections__HashMap__new_()",
    );
    let source = source.replace(
        "        Expr::Return(Some(inner), _) | Expr::Break(Some(inner), _) => collect_type_names_from_expr(inner, out_)",
        "        Expr::Return(Some(inner), _) => collect_type_names_from_expr(inner, out_)\n        Expr::Break(Some(inner), _) => collect_type_names_from_expr(inner, out_)",
    );
    let source = source.replace(
        "        Expr::Array(exprs, _) | Expr::Tuple(exprs, _) =>\n            for e in exprs:\n                check_expr_for_syntax_errors(env, e)?",
        "        Expr::Array(exprs, _) =>\n            for e in exprs:\n                check_expr_for_syntax_errors(env, e)?\n        Expr::Tuple(exprs, _) =>\n            for e in exprs:\n                check_expr_for_syntax_errors(env, e)?",
    );
    let source = repair_named_function_block(&source, "fn wrap_return_in_poll_ready(", |_| {
        [
            "fn wrap_return_in_poll_ready(block: Block, span: crate::span::Span) -> Expr:",
            "    let mut block = block",
            "    for stmt in &mut block.stmts:",
            "        wrap_stmt_returns(stmt, span.clone())",
            "    Expr__Block(block, span.clone())",
        ]
        .join("\n")
    });
    repair_named_function_block(&source, "fn wrap_stmt_returns(", |_| {
        [
            "fn wrap_stmt_returns(stmt: &mut Stmt, span: crate::span::Span):",
            "    match stmt:",
            "        Stmt::Return(Some(expr), _) =>",
            "            let inner = std__mem__replace(expr, Expr__None(span.clone()))",
            "            (*expr) = Expr::EnumVariant { enum_name: \"Poll\".to_string(), variant: \"Ready\".to_string(), fields: EnumVariantFields__Tuple([inner]), span: span.clone() }",
            "        Stmt::Return(None, s) => (*stmt) = Stmt__Return(Some(Expr::EnumVariant { enum_name: \"Poll\".to_string(), variant: \"Ready\".to_string(), fields: EnumVariantFields__Tuple([Expr__None(span.clone())]), span: span.clone() }), s.clone())",
            "        Stmt::While { body: body } =>",
            "            for s in &mut body.stmts:",
            "                wrap_stmt_returns(s, span.clone())",
            "        Stmt::Loop { body: body } =>",
            "            for s in &mut body.stmts:",
            "                wrap_stmt_returns(s, span.clone())",
            "        Stmt::For { body: body } =>",
            "            for s in &mut body.stmts:",
            "                wrap_stmt_returns(s, span.clone())",
            "        _ =>",
            "            let __selfhost_empty = none",
        ]
        .join("\n")
    })
}

fn repair_named_function_block<F>(source: &str, signature: &str, repair: F) -> String
where
    F: FnOnce(&str) -> String,
{
    let Some(start) = source.find(signature) else {
        return source.to_string();
    };
    let line_start = source[..start]
        .rfind('\n')
        .map(|index| index + 1)
        .unwrap_or(0);
    let line_indent = &source[line_start..start];
    let rest = &source[start..];
    let relative_end = ["fn ", "struct ", "enum ", "impl ", "mod ", "type "]
        .iter()
        .filter_map(|keyword| rest.find(&format!("\n\n{}{}", line_indent, keyword)))
        .min()
        .unwrap_or(rest.len());
    let end = start + relative_end;
    let mut rewritten = String::with_capacity(source.len());
    rewritten.push_str(&source[..start]);
    let repaired = repair(&source[start..end]);
    rewritten.push_str(&indent_repaired_block(&repaired, line_indent));
    rewritten.push_str(&source[end..]);
    rewritten
}

fn indent_repaired_block(block: &str, indent: &str) -> String {
    if indent.is_empty() || block.is_empty() {
        return block.to_string();
    }
    let mut indented = String::with_capacity(block.len() + indent.len() * block.lines().count());
    for (index, line) in block.split('\n').enumerate() {
        if index > 0 {
            indented.push('\n');
            indented.push_str(indent);
        }
        indented.push_str(line);
    }
    indented
}

fn write_attributes(output: &mut String, attrs: &[Attribute], indent: usize) -> KainResult<()> {
    for attr in attrs {
        write_line(output, indent, &attribute_to_string(attr))?;
    }
    Ok(())
}

fn attribute_to_string(attr: &Attribute) -> String {
    if attr.args.is_empty() {
        format!("@{}", sanitize_identifier(&attr.name))
    } else {
        format!(
            "@{}({})",
            sanitize_identifier(&attr.name),
            attr.args
                .iter()
                .map(attribute_arg_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        )
    }
}

fn attribute_arg_to_string(expr: &Expr) -> String {
    match expr {
        Expr::Tuple(parts, _) if parts.len() == 2 => {
            if let Expr::Ident(name, _) = &parts[0] {
                format!(
                    "{}: {}",
                    sanitize_identifier(name),
                    inline_expr_to_string(&parts[1])
                )
            } else {
                inline_expr_to_string(expr)
            }
        }
        _ => inline_expr_to_string(expr),
    }
}

fn visibility_prefix(visibility: Visibility) -> &'static str {
    match visibility {
        Visibility::Public | Visibility::Crate | Visibility::Super => "pub ",
        Visibility::Private => "",
    }
}

fn generics_to_string(generics: &[Generic]) -> String {
    if generics.is_empty() {
        String::new()
    } else {
        format!(
            "<{}>",
            generics
                .iter()
                .map(generic_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        )
    }
}

fn generic_to_string(generic: &Generic) -> String {
    if generic.bounds.is_empty() {
        sanitize_identifier(&generic.name)
    } else {
        format!(
            "{}: {}",
            sanitize_identifier(&generic.name),
            generic
                .bounds
                .iter()
                .map(|bound| sanitize_type_path(&bound.trait_name))
                .collect::<Vec<_>>()
                .join(" + ")
        )
    }
}

fn use_to_string(value: &Use) -> String {
    let mut rendered = format!(
        "use {}",
        value
            .path
            .iter()
            .map(|segment| sanitize_identifier(segment))
            .collect::<Vec<_>>()
            .join("::")
    );
    if value.glob {
        rendered.push_str("::*");
    }
    if let Some(alias) = &value.alias {
        rendered.push_str(&format!(" as {}", sanitize_identifier(alias)));
    }
    rendered
}

fn impl_trait_to_string(name: &str, generics: &[Type]) -> String {
    let trait_name = sanitize_type_path(name);
    if generics.is_empty() {
        trait_name
    } else {
        format!(
            "{}<{}>",
            trait_name,
            generics
                .iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        )
    }
}

fn write_item(output: &mut String, item: &Item, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    match item {
        Item::Function(function) => {
            write_attributes(output, &function.attributes, indent)?;
            write_function(output, function, indent)
        }
        Item::Struct(value) => {
            write_attributes(output, &value.attributes, indent)?;
            write_struct(output, value, indent)
        }
        Item::Enum(value) => write_enum(output, value, indent),
        Item::Mod(value) => {
            if value.name == "tests" {
                return Ok(());
            }
            write_line(
                output,
                indent,
                &format!(
                    "{}mod {}:",
                    visibility_prefix(value.visibility),
                    sanitize_identifier(&value.name)
                ),
            )?;
            let previous_module =
                CURRENT_SELFHOST_MODULE.with(|slot| slot.replace(Some(value.name.clone())));
            let result = (|| -> KainResult<()> {
                if let Some(children) = &value.inline {
                    if !children.is_empty() {
                        for child in children {
                            write_item(output, child, indent + 1)?;
                        }
                    }
                }
                writeln!(output).map_err(|err| {
                    KainError::runtime(format!("Failed to render module: {}", err))
                })?;
                Ok(())
            })();
            CURRENT_SELFHOST_MODULE.with(|slot| {
                slot.replace(previous_module);
            });
            result
        }
        Item::TypeAlias(value) => {
            write_line(
                output,
                indent,
                &format!(
                    "{}type {}{} = {}",
                    visibility_prefix(value.visibility),
                    sanitize_type_name(&value.name),
                    generics_to_string(&value.generics),
                    type_to_string(&value.target)
                ),
            )?;
            writeln!(output).map_err(|err| {
                KainError::runtime(format!("Failed to render type alias: {}", err))
            })?;
            Ok(())
        }
        Item::Const(value) => {
            write_line(
                output,
                indent,
                &format!(
                    "{}const {}: {} = {}",
                    visibility_prefix(value.visibility),
                    sanitize_identifier(&value.name),
                    type_to_string(&value.ty),
                    inline_expr_to_string(&value.value)
                ),
            )?;
            writeln!(output)
                .map_err(|err| KainError::runtime(format!("Failed to render const: {}", err)))?;
            Ok(())
        }
        Item::Impl(value) => write_impl(output, value, indent),
        Item::Use(value) => {
            write_line(output, indent, &use_to_string(value))?;
            writeln!(output)
                .map_err(|err| KainError::runtime(format!("Failed to render use: {}", err)))?;
            Ok(())
        }
        Item::Trait(value) => {
            write_line(
                output,
                indent,
                &format!(
                    "{}trait {}{}:",
                    visibility_prefix(value.visibility),
                    sanitize_type_name(&value.name),
                    generics_to_string(&value.generics)
                ),
            )?;
            if value.methods.is_empty() {
                write_line(output, indent + 1, "fn __selfhost_empty_trait__():")?;
                write_line(output, indent + 2, "let __selfhost_empty = none")?;
            } else {
                for method in &value.methods {
                    let mut signature = format!("fn {}(", sanitize_identifier(&method.name));
                    for (index, param) in method.params.iter().enumerate() {
                        if index > 0 {
                            signature.push_str(", ");
                        }
                        signature.push_str(&format!(
                            "{}: {}",
                            sanitize_identifier(&param.name),
                            type_to_string(&param.ty)
                        ));
                    }
                    signature.push(')');
                    if let Some(return_type) = &method.return_type {
                        signature.push_str(&format!(" -> {}", type_to_string(return_type)));
                    }
                    signature.push(':');
                    write_line(output, indent + 1, &signature)?;
                    if let Some(default_impl) = &method.default_impl {
                        write_block(output, default_impl, indent + 2)?;
                    } else {
                        write_line(output, indent + 2, "let __selfhost_empty = none")?;
                    }
                }
            }
            writeln!(output)
                .map_err(|err| KainError::runtime(format!("Failed to render trait: {}", err)))?;
            Ok(())
        }
        _ => Ok(()),
    }
}

fn write_function(
    output: &mut String,
    function: &kain_core::ast::Function,
    indent: usize,
) -> KainResult<()> {
    use std::fmt::Write;

    let emitted_name = rendered_function_name(function);
    let mut signature = format!(
        "{}fn {}{}(",
        visibility_prefix(function.visibility),
        sanitize_identifier(&emitted_name),
        generics_to_string(&function.generics)
    );
    for (index, param) in function.params.iter().enumerate() {
        if index > 0 {
            signature.push_str(", ");
        }
        signature.push_str(&format!(
            "{}: {}",
            sanitize_identifier(&param.name),
            type_to_string(&param.ty)
        ));
    }
    signature.push(')');
    if let Some(return_type) = &function.return_type {
        signature.push_str(&format!(" -> {}", type_to_string(return_type)));
    }
    signature.push(':');
    write_line(output, indent, &signature)?;
    let current_impl = CURRENT_SELFHOST_IMPL.with(|slot| slot.borrow().clone());
    if function.name == "new" && current_impl.as_deref() == Some("SourceLocation") {
        write_line(
            output,
            indent + 1,
            "SourceLocation { file: file, line: line, col: col }",
        )?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "new" && current_impl.as_deref() == Some("Diagnostics") {
        write_line(
            output,
            indent + 1,
            "Diagnostics { source: source, filename: filename }",
        )?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "new" && current_impl.as_deref() == Some("EffectSet") {
        write_line(
            output,
            indent + 1,
            "EffectSet { effects: std__collections__HashSet__new_() }",
        )?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "new" && current_impl.as_deref() == Some("DiagnosticBuilder") {
        write_line(output, indent + 1, "DiagnosticBuilder { kind: kind, code: code, file: none, location: none, context: String__new_(), message: message.into(), suggestion: none }")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "format_with_context" && current_impl.as_deref() == Some("Diagnostics") {
        write_line(
            output,
            indent + 1,
            "let (line_num, col, line_content) = self.get_line_info(span)",
        )?;
        write_line(output, indent + 1, "let mut output = String__new_()")?;
        write_line(output, indent + 1, "output.push_str(\"\\nerror[\")")?;
        write_line(output, indent + 1, "output.push_str(error_type)")?;
        write_line(output, indent + 1, "output.push_str(\"]: \")")?;
        write_line(output, indent + 1, "output.push_str(message)")?;
        write_line(output, indent + 1, "output.push_str(\"\\n  --> \")")?;
        write_line(output, indent + 1, "output.push_str(&self.filename)")?;
        write_line(output, indent + 1, "output.push_str(\":\")")?;
        write_line(output, indent + 1, "output.push_str(&line_num.to_string())")?;
        write_line(output, indent + 1, "output.push_str(\":\")")?;
        write_line(output, indent + 1, "output.push_str(&col.to_string())")?;
        write_line(output, indent + 1, "output.push_str(\"\\n\")")?;
        write_line(output, indent + 1, "output.push_str(\"   |\\n\")")?;
        write_line(output, indent + 1, "output.push_str(&line_num.to_string())")?;
        write_line(output, indent + 1, "output.push_str(\" | \")")?;
        write_line(output, indent + 1, "output.push_str(line_content)")?;
        write_line(output, indent + 1, "output.push_str(\"\\n\")")?;
        write_line(
            output,
            indent + 1,
            "let pointer_offset = col.saturating_sub(1)",
        )?;
        write_line(output, indent + 1, "let content_len = line_content.len()")?;
        write_line(
            output,
            indent + 1,
            "let remaining_len = content_len.saturating_sub(pointer_offset)",
        )?;
        write_line(
            output,
            indent + 1,
            "let span_len = span.end.saturating_sub(span.start)",
        )?;
        write_line(
            output,
            indent + 1,
            "let pointer_len = min(span_len, remaining_len).max(1)",
        )?;
        write_line(output, indent + 1, "output.push_str(\"   | \")")?;
        write_line(
            output,
            indent + 1,
            "output.push_str(&\" \".repeat(pointer_offset))",
        )?;
        write_line(
            output,
            indent + 1,
            "output.push_str(&\"^\".repeat(pointer_len))",
        )?;
        write_line(output, indent + 1, "output.push_str(\"\\n\")")?;
        write_line(output, indent + 1, "output.push_str(\"   |\\n\")")?;
        write_line(output, indent + 1, "output")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "tokenize"
        && current_impl
            .as_deref()
            .is_some_and(|name| name.starts_with("Lexer"))
    {
        write_line(
            output,
            indent + 1,
            "__kain_bootstrap_lex_tokens(&(*_self).source)",
        )?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "process_indentation"
        && current_impl
            .as_deref()
            .is_some_and(|name| name.starts_with("Lexer"))
    {
        write_line(output, indent + 1, "let mut result = Vec__new_()")?;
        write_line(output, indent + 1, "let mut indent_stack = [0]")?;
        write_line(
            output,
            indent + 1,
            "let mut iter = raw.into_iter().peekable()",
        )?;
        write_line(output, indent + 1, "loop:")?;
        write_line(output, indent + 2, "match iter.next():")?;
        write_line(output, indent + 3, "Some(token) =>")?;
        write_line(output, indent + 4, "match &token.kind:")?;
        write_line(output, indent + 5, "TokenKind::Newline(ws) =>")?;
        write_line(output, indent + 6, "match iter.peek():")?;
        write_line(output, indent + 7, "Some(next) =>")?;
        write_line(output, indent + 8, "match &next.kind:")?;
        write_line(output, indent + 9, "TokenKind::Newline(_) =>")?;
        write_line(output, indent + 10, "continue")?;
        write_line(output, indent + 9, "_ =>")?;
        write_line(output, indent + 10, "()")?;
        write_line(output, indent + 7, "_ =>")?;
        write_line(output, indent + 8, "()")?;
        write_line(output, indent + 6, "let mut indent: usize = 0")?;
        write_line(output, indent + 6, "for indent_char in ws[1..].chars():")?;
        write_line(output, indent + 7, "if (indent_char == \"\\t\"):")?;
        write_line(output, indent + 8, "indent = (indent + 4)")?;
        write_line(output, indent + 7, "else:")?;
        write_line(output, indent + 8, "indent = (indent + 1)")?;
        write_line(
            output,
            indent + 6,
            "let current = (*indent_stack.last().unwrap())",
        )?;
        write_line(output, indent + 6, "if (indent > current):")?;
        write_line(output, indent + 7, "indent_stack.push(indent)")?;
        write_line(
            output,
            indent + 7,
            "result.push(Token__new_(TokenKind::Newline(ws.clone()), token.span))",
        )?;
        write_line(
            output,
            indent + 7,
            "result.push(Token__new_(TokenKind__Indent, token.span))",
        )?;
        write_line(output, indent + 6, "elif (indent < current):")?;
        write_line(
            output,
            indent + 7,
            "result.push(Token__new_(TokenKind::Newline(ws.clone()), token.span))",
        )?;
        write_line(
            output,
            indent + 7,
            "while ((indent_stack.len() > 1) && ((*indent_stack.last().unwrap()) > indent)):",
        )?;
        write_line(output, indent + 8, "indent_stack.pop()")?;
        write_line(
            output,
            indent + 8,
            "result.push(Token__new_(TokenKind__Dedent, token.span))",
        )?;
        write_line(output, indent + 6, "else:")?;
        write_line(
            output,
            indent + 7,
            "result.push(Token__new_(TokenKind::Newline(ws.clone()), token.span))",
        )?;
        write_line(output, indent + 5, "_ =>")?;
        write_line(output, indent + 6, "result.push(token)")?;
        write_line(output, indent + 3, "_ =>")?;
        write_line(output, indent + 4, "break")?;
        write_line(
            output,
            indent + 1,
            "let final_span = result.last().map(fn(t): t.span).unwrap_or(Span__new_(0, 0))",
        )?;
        write_line(output, indent + 1, "while (indent_stack.len() > 1):")?;
        write_line(output, indent + 2, "indent_stack.pop()")?;
        write_line(
            output,
            indent + 2,
            "result.push(Token__new_(TokenKind__Dedent, final_span))",
        )?;
        write_line(
            output,
            indent + 1,
            "result.push(Token__new_(TokenKind__Eof, final_span))",
        )?;
        write_line(output, indent + 1, "Result::Ok(result)")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "span" && current_impl.as_deref() == Some("Type") {
        write_line(output, indent + 1, "match _self:")?;
        write_line(
            output,
            indent + 2,
            "Type::Named { span: span } => span.clone()",
        )?;
        write_line(output, indent + 2, "Type::Tuple(_, span) => span.clone()")?;
        write_line(
            output,
            indent + 2,
            "Type::Array(_, _, span) => span.clone()",
        )?;
        write_line(output, indent + 2, "Type::Slice(_, span) => span.clone()")?;
        write_line(
            output,
            indent + 2,
            "Type::Ref { span: span } => span.clone()",
        )?;
        write_line(
            output,
            indent + 2,
            "Type::Ptr { span: span } => span.clone()",
        )?;
        write_line(
            output,
            indent + 2,
            "Type::Function { span: span } => span.clone()",
        )?;
        write_line(output, indent + 2, "Type::Option(_, span) => span.clone()")?;
        write_line(
            output,
            indent + 2,
            "Type::Result(_, _, span) => span.clone()",
        )?;
        write_line(output, indent + 2, "Type::Infer(span) => span.clone()")?;
        write_line(output, indent + 2, "Type::Never(span) => span.clone()")?;
        write_line(output, indent + 2, "Type::Unit(span) => span.clone()")?;
        write_line(
            output,
            indent + 2,
            "Type::Impl { span: span } => span.clone()",
        )?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "span" && current_impl.as_deref() == Some("Expr") {
        write_line(output, indent + 1, "match _self:")?;
        write_line(output, indent + 2, "Expr::Int(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Float(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::String(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::FString(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Bool(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::None(s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Ident(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Binary { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Unary { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Call { span: s } => s.clone()")?;
        write_line(
            output,
            indent + 2,
            "Expr::MethodCall { span: s } => s.clone()",
        )?;
        write_line(output, indent + 2, "Expr::Field { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Index { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Struct { span: s } => s.clone()")?;
        write_line(
            output,
            indent + 2,
            "Expr::AggregateInit { span: s } => s.clone()",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::EnumVariant { span: s } => s.clone()",
        )?;
        write_line(output, indent + 2, "Expr::Array(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Tuple(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Range { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::If { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Match { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Lambda { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Ref { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::AddrOf { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Deref(_, s) => s.clone()")?;
        write_line(
            output,
            indent + 2,
            "Expr::PtrOffset { span: s } => s.clone()",
        )?;
        write_line(output, indent + 2, "Expr::MemLoad { span: s } => s.clone()")?;
        write_line(
            output,
            indent + 2,
            "Expr::MemStore { span: s } => s.clone()",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::SizeOfType { span: s } => s.clone()",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::AlignOfType { span: s } => s.clone()",
        )?;
        write_line(output, indent + 2, "Expr::Alloca { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Uninit { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Alloc { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Realloc { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Cast { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Try(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Await(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Spawn { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::SendMsg { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Comptime(_, s) => s.clone()")?;
        write_line(
            output,
            indent + 2,
            "Expr::MacroCall { span: s } => s.clone()",
        )?;
        write_line(output, indent + 2, "Expr::Block(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::JSX(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Assign { span: s } => s.clone()")?;
        write_line(output, indent + 2, "Expr::Paren(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Return(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Break(_, s) => s.clone()")?;
        write_line(output, indent + 2, "Expr::Continue(s) => s.clone()")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if (function.name == "current_span" || emitted_name == "current_span")
        && current_impl.as_deref() == Some("Parser")
    {
        write_line(
            output,
            indent + 1,
            "if (!_self.injected_tokens.is_empty()):",
        )?;
        write_line(
            output,
            indent + 2,
            "return _self.injected_tokens[0].span.clone()",
        )?;
        write_line(
            output,
            indent + 1,
            "_self.tokens.get(_self.pos).map(|tok| tok.span.clone()).unwrap_or(crate__span__Span__new_(0, 0))",
        )?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "render_to_string" && current_selfhost_module_matches(&["ui"]) {
        write_line(output, indent + 1, "match node:")?;
        write_line(
            output,
            indent + 2,
            "VNode::Element { tag: tag, attrs: attrs, children: children } =>",
        )?;
        write_line(output, indent + 3, "let mut rendered_attrs = Vec__new_()")?;
        write_line(output, indent + 3, "for attr in attrs:")?;
        write_line(
            output,
            indent + 4,
            "rendered_attrs.push(render_attr_to_string(attr))",
        )?;
        write_line(
            output,
            indent + 3,
            "let attrs_joined = rendered_attrs.join(\" \".to_string())",
        )?;
        write_line(output, indent + 3, "let attr_suffix = if attrs_joined.is_empty(): String__new_() else: f\" {attrs_joined}\"")?;
        write_line(output, indent + 3, "if children.is_empty():")?;
        write_line(output, indent + 4, "f\"<{tag}{attr_suffix}/>\"")?;
        write_line(output, indent + 3, "else:")?;
        write_line(
            output,
            indent + 4,
            "let mut rendered_children = Vec__new_()",
        )?;
        write_line(output, indent + 4, "for child in children:")?;
        write_line(
            output,
            indent + 5,
            "rendered_children.push(render_to_string(child))",
        )?;
        write_line(
            output,
            indent + 4,
            "let children_joined = rendered_children.join(String__new_())",
        )?;
        write_line(
            output,
            indent + 4,
            "f\"<{tag}{attr_suffix}>{children_joined}</{tag}>\"",
        )?;
        write_line(output, indent + 2, "VNode::Text(text) => text.clone()")?;
        write_line(output, indent + 2, "VNode::Fragment(children) =>")?;
        write_line(
            output,
            indent + 3,
            "let mut rendered_children = Vec__new_()",
        )?;
        write_line(output, indent + 3, "for child in children:")?;
        write_line(
            output,
            indent + 4,
            "rendered_children.push(render_to_string(child))",
        )?;
        write_line(output, indent + 3, "rendered_children.join(String__new_())")?;
        write_line(
            output,
            indent + 2,
            "VNode::Component { rendered: rendered } => render_to_string(rendered)",
        )?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "item_span" {
        write_line(output, indent + 1, "match item:")?;
        write_line(output, indent + 2, "Item::Function(f) => f.span.clone()")?;
        write_line(output, indent + 2, "Item::Struct(s) => s.span.clone()")?;
        write_line(output, indent + 2, "Item::Enum(e) => e.span.clone()")?;
        write_line(output, indent + 2, "Item::Component(c) => c.span.clone()")?;
        write_line(output, indent + 2, "Item::Shader(s) => s.span.clone()")?;
        write_line(output, indent + 2, "Item::Actor(a) => a.span.clone()")?;
        write_line(output, indent + 2, "Item::Comptime(b) => b.span.clone()")?;
        write_line(output, indent + 2, "Item::Const(c) => c.span.clone()")?;
        write_line(output, indent + 2, "Item::Macro(m) => m.span.clone()")?;
        write_line(output, indent + 2, "Item::Use(u) => u.span.clone()")?;
        write_line(output, indent + 2, "Item::Mod(m) => m.span.clone()")?;
        write_line(output, indent + 2, "Item::Impl(i) => i.span.clone()")?;
        write_line(output, indent + 2, "Item::Test(t) => t.span.clone()")?;
        write_line(output, indent + 2, "_ => Span__new_(0, 0)")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "reconcile" && current_selfhost_module_matches(&["ui"]) {
        write_line(output, indent + 1, "match (current, next):")?;
        write_line(output, indent + 2, "(Some(VNode::Element { tag: old_tag }), VNode::Element { tag: new_tag, attrs: attrs, children: children, key: key }) =>")?;
        write_line(output, indent + 3, "if (old_tag == new_tag):")?;
        write_line(output, indent + 4, "VNode::Element { tag: new_tag.clone(), attrs: attrs.clone(), children: children.clone(), key: key.clone() }")?;
        write_line(output, indent + 3, "else:")?;
        write_line(output, indent + 4, "next.clone()")?;
        write_line(
            output,
            indent + 2,
            "(Some(VNode::Text(_)), VNode::Text(text)) => VNode__Text(text.clone())",
        )?;
        write_line(output, indent + 2, "(Some(VNode::Fragment(_)), VNode::Fragment(children)) => VNode__Fragment(children.clone())")?;
        write_line(output, indent + 2, "(Some(VNode::Component { instance: old_instance }), VNode::Component { instance: instance, rendered: rendered }) =>")?;
        write_line(
            output,
            indent + 3,
            "if (old_instance.name == instance.name):",
        )?;
        write_line(output, indent + 4, "VNode::Component { instance: instance.clone(), rendered: Box__new_(reconcile(none, rendered)) }")?;
        write_line(output, indent + 3, "else:")?;
        write_line(output, indent + 4, "next.clone()")?;
        write_line(output, indent + 2, "_ => next.clone()")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "collect_type_names_from_pattern" {
        write_line(output, indent + 1, "match pattern:")?;
        write_line(
            output,
            indent + 2,
            "Pattern::Variant { enum_name: enum_name, fields: fields } =>",
        )?;
        write_line(output, indent + 3, "match enum_name:")?;
        write_line(output, indent + 4, "Some(name) =>")?;
        write_line(
            output,
            indent + 5,
            "let __selfhost_insert = out_.insert(name.clone())",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(output, indent + 3, "match fields:")?;
        write_line(
            output,
            indent + 4,
            "VariantPatternFields::Tuple(patterns) =>",
        )?;
        write_line(output, indent + 5, "for p in patterns:")?;
        write_line(
            output,
            indent + 6,
            "collect_type_names_from_pattern(p, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(output, indent + 2, "Pattern::Tuple(patterns, _) =>")?;
        write_line(output, indent + 3, "for p in patterns:")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_pattern(p, out_)",
        )?;
        write_line(output, indent + 2, "Pattern::Or(patterns, _) =>")?;
        write_line(output, indent + 3, "for p in patterns:")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_pattern(p, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Pattern::Slice { patterns: patterns } =>",
        )?;
        write_line(output, indent + 3, "for p in patterns:")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_pattern(p, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Pattern::Literal(expr) => collect_type_names_from_expr(expr, out_)",
        )?;
        write_line(output, indent + 2, "_ =>")?;
        write_line(output, indent + 3, "let __selfhost_empty = none")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "collect_type_names_from_expr" {
        write_line(output, indent + 1, "match expr:")?;
        write_line(
            output,
            indent + 2,
            "Expr::Cast { value: value, target: target } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(value, out_)",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_type(target, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Struct { name: name, fields: fields, rest: rest } =>",
        )?;
        write_line(output, indent + 3, "out_.insert(name.clone())")?;
        write_line(output, indent + 3, "for field in fields:")?;
        write_line(output, indent + 4, "let (_, field_expr) = field")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_expr(field_expr, out_)",
        )?;
        write_line(output, indent + 3, "match rest:")?;
        write_line(
            output,
            indent + 4,
            "Some(rest_expr) => collect_type_names_from_expr(rest_expr, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::AggregateInit { ty: ty, fields: fields } =>",
        )?;
        write_line(output, indent + 3, "collect_type_names_from_type(ty, out_)")?;
        write_line(output, indent + 3, "for field in fields:")?;
        write_line(output, indent + 4, "let (_, field_expr) = field")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_expr(field_expr, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::EnumVariant { enum_name: enum_name, fields: fields } =>",
        )?;
        write_line(output, indent + 3, "out_.insert(enum_name.clone())")?;
        write_line(output, indent + 3, "match fields:")?;
        write_line(output, indent + 4, "EnumVariantFields::Unit =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(output, indent + 4, "EnumVariantFields::Tuple(exprs) =>")?;
        write_line(output, indent + 5, "for e in exprs:")?;
        write_line(output, indent + 6, "collect_type_names_from_expr(e, out_)")?;
        write_line(
            output,
            indent + 4,
            "EnumVariantFields::Struct(field_pairs) =>",
        )?;
        write_line(output, indent + 5, "for field_pair in field_pairs:")?;
        write_line(output, indent + 6, "let (_, field_expr) = field_pair")?;
        write_line(
            output,
            indent + 6,
            "collect_type_names_from_expr(field_expr, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Spawn { actor_: actor_, init: init } =>",
        )?;
        write_line(output, indent + 3, "out_.insert(actor_.clone())")?;
        write_line(output, indent + 3, "for init_entry in init:")?;
        write_line(output, indent + 4, "let (_, init_expr) = init_entry")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_expr(init_expr, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Call { callee: callee, args: args } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(callee, out_)",
        )?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_expr(&arg.value, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::MethodCall { receiver: receiver, args: args } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(receiver, out_)",
        )?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_expr(&arg.value, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Field { object: object } => collect_type_names_from_expr(object, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Index { object: object, index: index } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(object, out_)",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(index, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Binary { left: left, right: right } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(left, out_)",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(right, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Unary { operand: operand } => collect_type_names_from_expr(operand, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Assign { target: target, value: value } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(target, out_)",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(value, out_)",
        )?;
        write_line(output, indent + 2, "Expr::If { condition: condition, then_branch: then_branch, else_branch: else_branch } =>")?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(condition, out_)",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_block(then_branch, out_)",
        )?;
        write_line(output, indent + 3, "match else_branch:")?;
        write_line(
            output,
            indent + 4,
            "Some(else_b) => collect_type_names_from_else_branch(else_b, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Match { scrutinee: scrutinee, arms: arms } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(scrutinee, out_)",
        )?;
        write_line(output, indent + 3, "for arm in arms:")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_pattern(&arm.pattern, out_)",
        )?;
        write_line(output, indent + 4, "match &arm.guard:")?;
        write_line(
            output,
            indent + 5,
            "Some(guard) => collect_type_names_from_expr(guard, out_)",
        )?;
        write_line(output, indent + 5, "_ =>")?;
        write_line(output, indent + 6, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_expr(&arm.body, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Lambda { params: params, return_type: return_type, body: body } =>",
        )?;
        write_line(output, indent + 3, "for p in params:")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_type(&p.ty, out_)",
        )?;
        write_line(output, indent + 3, "match return_type:")?;
        write_line(
            output,
            indent + 4,
            "Some(ret) => collect_type_names_from_type(ret, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(body, out_)",
        )?;
        write_line(output, indent + 2, "Expr::Array(exprs, _) =>")?;
        write_line(output, indent + 3, "for e in exprs:")?;
        write_line(output, indent + 4, "collect_type_names_from_expr(e, out_)")?;
        write_line(output, indent + 2, "Expr::Tuple(exprs, _) =>")?;
        write_line(output, indent + 3, "for e in exprs:")?;
        write_line(output, indent + 4, "collect_type_names_from_expr(e, out_)")?;
        write_line(output, indent + 2, "Expr::FString(exprs, _) =>")?;
        write_line(output, indent + 3, "for e in exprs:")?;
        write_line(output, indent + 4, "collect_type_names_from_expr(e, out_)")?;
        write_line(output, indent + 2, "Expr::MacroCall { args: args } =>")?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_expr(arg, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::SendMsg { target: target, data: data } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(target, out_)",
        )?;
        write_line(output, indent + 3, "for data_entry in data:")?;
        write_line(output, indent + 4, "let (_, data_expr) = data_entry")?;
        write_line(
            output,
            indent + 4,
            "collect_type_names_from_expr(data_expr, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Block(block, _) => collect_type_names_from_block(block, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Range { start: start, end: end } =>",
        )?;
        write_line(output, indent + 3, "match start:")?;
        write_line(
            output,
            indent + 4,
            "Some(s) => collect_type_names_from_expr(s, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(output, indent + 3, "match end:")?;
        write_line(
            output,
            indent + 4,
            "Some(e) => collect_type_names_from_expr(e, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Ref { value: value } => collect_type_names_from_expr(value, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::AddrOf { value: value } => collect_type_names_from_expr(value, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Deref(inner, _) => collect_type_names_from_expr(inner, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Try(inner, _) => collect_type_names_from_expr(inner, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Await(inner, _) => collect_type_names_from_expr(inner, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Comptime(inner, _) => collect_type_names_from_expr(inner, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Paren(inner, _) => collect_type_names_from_expr(inner, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::PtrOffset { pointer: pointer, offset: offset, element_ty: element_ty } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(pointer, out_)",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(offset, out_)",
        )?;
        write_line(output, indent + 3, "match element_ty:")?;
        write_line(
            output,
            indent + 4,
            "Some(ty) => collect_type_names_from_type(ty, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::MemLoad { pointer: pointer, load_ty: load_ty } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(pointer, out_)",
        )?;
        write_line(output, indent + 3, "match load_ty:")?;
        write_line(
            output,
            indent + 4,
            "Some(ty) => collect_type_names_from_type(ty, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::MemStore { pointer: pointer, value: value, store_ty: store_ty } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(pointer, out_)",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(value, out_)",
        )?;
        write_line(output, indent + 3, "match store_ty:")?;
        write_line(
            output,
            indent + 4,
            "Some(ty) => collect_type_names_from_type(ty, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::SizeOfType { target: target } => collect_type_names_from_type(target, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::AlignOfType { target: target } => collect_type_names_from_type(target, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Alloca { ty: target } => collect_type_names_from_type(target, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Uninit { ty: target } => collect_type_names_from_type(target, out_)",
        )?;
        write_line(output, indent + 2, "Expr::Alloc { size: size, ty: ty } =>")?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(size, out_)",
        )?;
        write_line(output, indent + 3, "match ty:")?;
        write_line(
            output,
            indent + 4,
            "Some(ty) => collect_type_names_from_type(ty, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Realloc { pointer: pointer, size: size, ty: ty } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(pointer, out_)",
        )?;
        write_line(
            output,
            indent + 3,
            "collect_type_names_from_expr(size, out_)",
        )?;
        write_line(output, indent + 3, "match ty:")?;
        write_line(
            output,
            indent + 4,
            "Some(ty) => collect_type_names_from_type(ty, out_)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Return(Some(inner), _) => collect_type_names_from_expr(inner, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Break(Some(inner), _) => collect_type_names_from_expr(inner, out_)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::JSX(node, _) => collect_type_names_from_jsx(node, out_)",
        )?;
        write_line(output, indent + 2, "_ =>")?;
        write_line(output, indent + 3, "let __selfhost_empty = none")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "infer_expr_type" {
        write_line(output, indent + 1, "match expr:")?;
        write_line(output, indent + 2, "Expr::Int(_, span) => Some(Type::Named { name: \"Int\".to_string(), generics: Vec__new_(), span: span.clone() })")?;
        write_line(output, indent + 2, "Expr::Float(_, span) => Some(Type::Named { name: \"Float\".to_string(), generics: Vec__new_(), span: span.clone() })")?;
        write_line(output, indent + 2, "Expr::Bool(_, span) => Some(Type::Named { name: \"Bool\".to_string(), generics: Vec__new_(), span: span.clone() })")?;
        write_line(
            output,
            indent + 2,
            "Expr::Ident(name, _) => ctx.local_types.get(name).cloned()",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Field { object: object, field: field } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "let object_ty = infer_expr_type(object, ctx)?",
        )?;
        write_line(
            output,
            indent + 3,
            "field_type_from_object(&object_ty, field, ctx)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Index { object: object } => infer_element_type(object, ctx)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Cast { target: target } => Some(target.clone())",
        )?;
        write_line(output, indent + 2, "_ => none")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "eval_expr_in_place" {
        write_line(output, indent + 1, "match expr:")?;
        write_line(
            output,
            indent + 2,
            "Expr::Binary { left: left, right: right } =>",
        )?;
        write_line(output, indent + 3, "eval_expr_in_place(env, left)?")?;
        write_line(output, indent + 3, "eval_expr_in_place(env, right)?")?;
        write_line(
            output,
            indent + 2,
            "Expr::Call { callee: callee, args: args } =>",
        )?;
        write_line(output, indent + 3, "eval_expr_in_place(env, callee)?")?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "eval_expr_in_place(env, &mut arg.value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::MethodCall { receiver: receiver, args: args } =>",
        )?;
        write_line(output, indent + 3, "eval_expr_in_place(env, receiver)?")?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "eval_expr_in_place(env, &mut arg.value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Assign { value: value } => eval_expr_in_place(env, value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::AddrOf { value: value } => eval_expr_in_place(env, value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Ref { value: value } => eval_expr_in_place(env, value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Field { object: object } => eval_expr_in_place(env, object)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Deref(object, _) => eval_expr_in_place(env, object)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Try(object, _) => eval_expr_in_place(env, object)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Await(object, _) => eval_expr_in_place(env, object)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Comptime(object, _) => eval_expr_in_place(env, object)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Paren(object, _) => eval_expr_in_place(env, object)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Index { object: object, index: index } =>",
        )?;
        write_line(output, indent + 3, "eval_expr_in_place(env, object)?")?;
        write_line(output, indent + 3, "eval_expr_in_place(env, index)?")?;
        write_line(
            output,
            indent + 2,
            "Expr::MemLoad { pointer: pointer } => eval_expr_in_place(env, pointer)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::MemStore { pointer: pointer, value: value } =>",
        )?;
        write_line(output, indent + 3, "eval_expr_in_place(env, pointer)?")?;
        write_line(output, indent + 3, "eval_expr_in_place(env, value)?")?;
        write_line(
            output,
            indent + 2,
            "Expr::AggregateInit { fields: fields } =>",
        )?;
        write_line(output, indent + 3, "for field in fields:")?;
        write_line(output, indent + 4, "let (_, value) = field")?;
        write_line(output, indent + 4, "eval_expr_in_place(env, value)?")?;
        write_line(
            output,
            indent + 2,
            "Expr::Struct { fields: fields, rest: rest } =>",
        )?;
        write_line(output, indent + 3, "for field in fields:")?;
        write_line(output, indent + 4, "let (_, value) = field")?;
        write_line(output, indent + 4, "eval_expr_in_place(env, value)?")?;
        write_line(output, indent + 3, "match rest:")?;
        write_line(
            output,
            indent + 4,
            "Some(rest_expr) => eval_expr_in_place(env, rest_expr)?",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::EnumVariant { fields: fields } =>",
        )?;
        write_line(output, indent + 3, "match fields:")?;
        write_line(output, indent + 4, "EnumVariantFields::Tuple(values) =>")?;
        write_line(output, indent + 5, "for value in values:")?;
        write_line(output, indent + 6, "eval_expr_in_place(env, value)?")?;
        write_line(output, indent + 4, "EnumVariantFields::Struct(values) =>")?;
        write_line(output, indent + 5, "for named_value in values:")?;
        write_line(output, indent + 6, "let (_, value) = named_value")?;
        write_line(output, indent + 6, "eval_expr_in_place(env, value)?")?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Alloc { size: size } => eval_expr_in_place(env, size)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Realloc { pointer: pointer, size: size } =>",
        )?;
        write_line(output, indent + 3, "eval_expr_in_place(env, pointer)?")?;
        write_line(output, indent + 3, "eval_expr_in_place(env, size)?")?;
        write_line(
            output,
            indent + 2,
            "Expr::PtrOffset { pointer: pointer, offset: size } =>",
        )?;
        write_line(output, indent + 3, "eval_expr_in_place(env, pointer)?")?;
        write_line(output, indent + 3, "eval_expr_in_place(env, size)?")?;
        write_line(
            output,
            indent + 2,
            "Expr::Block(b, _) => eval_block(env, b)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::JSX(node, _) => eval_jsx(env, node)?",
        )?;
        write_line(output, indent + 2, "_ =>")?;
        write_line(output, indent + 3, "let __selfhost_empty = none")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "re_span_type" && current_impl.as_deref() == Some("Parser") {
        write_line(output, indent + 1, "match ty:")?;
        write_line(output, indent + 2, "Type::Named { name: name, generics: generics } => Type::Named { name: name, generics: generics.into_iter().collect(), span: span.clone() }")?;
        write_line(output, indent + 2, "Type::Array(inner, size, _) => Type__Array(Box__new_(Self___re_span_type((*inner), span.clone())), size, span.clone())")?;
        write_line(output, indent + 2, "Type::Slice(inner, _) => Type__Slice(Box__new_(Self___re_span_type((*inner), span.clone())), span.clone())")?;
        write_line(
            output,
            indent + 2,
            "Type::Tuple(types, _) => Type__Tuple(types.into_iter().collect(), span.clone())",
        )?;
        write_line(output, indent + 2, "Type::Ref { mutable_: mutable_, inner: inner, lifetime: lifetime } => Type::Ref { mutable_: mutable_, inner: Box__new_(Self___re_span_type((*inner), span.clone())), lifetime: lifetime, span: span.clone() }")?;
        write_line(output, indent + 2, "Type::Ptr { mutable_: mutable_, inner: inner, provenance: provenance } => Type::Ptr { mutable_: mutable_, inner: Box__new_(Self___re_span_type((*inner), span.clone())), provenance: provenance, span: span.clone() }")?;
        write_line(output, indent + 2, "Type::Function { params: params, return_type: return_type, effects: effects } => Type::Function { params: params.into_iter().collect(), return_type: Box__new_(Self___re_span_type((*return_type), span.clone())), effects: effects, span: span.clone() }")?;
        write_line(output, indent + 2, "Type::Option(inner, _) => Type__Option(Box__new_(Self___re_span_type((*inner), span.clone())), span.clone())")?;
        write_line(output, indent + 2, "Type::Result(ok, err, _) => Type__Result(Box__new_(Self___re_span_type((*ok), span.clone())), Box__new_(Self___re_span_type((*err), span.clone())), span.clone())")?;
        write_line(
            output,
            indent + 2,
            "Type::Infer(_) => Type__Infer(span.clone())",
        )?;
        write_line(
            output,
            indent + 2,
            "Type::Never(_) => Type__Never(span.clone())",
        )?;
        write_line(
            output,
            indent + 2,
            "Type::Unit(_) => Type__Unit(span.clone())",
        )?;
        write_line(output, indent + 2, "Type::Impl { trait_name: trait_name, generics: generics } => Type::Impl { trait_name: trait_name, generics: generics.into_iter().collect(), span: span.clone() }")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "parse_assignment" && current_impl.as_deref() == Some("Parser") {
        write_line(
            output,
            indent + 1,
            "let target = _self.parse_conditional()?",
        )?;
        write_line(output, indent + 1, "match _self.get_assignment_binop():")?;
        write_line(output, indent + 2, "Some(assign_binop) =>")?;
        write_line(output, indent + 3, "_self.advance()")?;
        write_line(output, indent + 3, "let rhs = _self.parse_assignment()?")?;
        write_line(
            output,
            indent + 3,
            "let span = target.span().merge(rhs.span())",
        )?;
        write_line(output, indent + 3, "let value = match assign_binop:")?;
        write_line(output, indent + 4, "Some(op) => Expr::Binary { left: Box__new_(target.clone()), op: op, right: Box__new_(rhs), span: span.clone() }")?;
        write_line(output, indent + 4, "_ => rhs")?;
        write_line(output, indent + 3, "Ok(Expr::Assign { target: Box__new_(target), value: Box__new_(value), span: span.clone() })")?;
        write_line(output, indent + 2, "_ => Ok(target)")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "parse_conditional" && current_impl.as_deref() == Some("Parser") {
        write_line(
            output,
            indent + 1,
            "let condition = _self.parse_coalesce()?",
        )?;
        write_line(
            output,
            indent + 1,
            "if (!_self.check(crate__lexer__TokenKind__Question)):\n",
        )?;
        write_line(output, indent + 2, "return Ok(condition)")?;
        write_line(output, indent + 1, "_self.advance()")?;
        write_line(
            output,
            indent + 1,
            "let then_expr = _self.parse_assignment()?",
        )?;
        write_line(
            output,
            indent + 1,
            "_self.expect(crate__lexer__TokenKind__Colon)?",
        )?;
        write_line(
            output,
            indent + 1,
            "let else_expr = _self.parse_assignment()?",
        )?;
        write_line(output, indent + 1, "let then_span = then_expr.span()")?;
        write_line(output, indent + 1, "let else_span = else_expr.span()")?;
        write_line(
            output,
            indent + 1,
            "let span = condition.span().merge(else_span.clone())",
        )?;
        write_line(output, indent + 1, "Ok(Expr::Match { scrutinee: Box__new_(condition), arms: [aggregate_init(\"MatchArm\", pattern = Pattern__Literal(Expr__Bool(true, then_span.clone())), guard = none, body = then_expr, span = then_span.clone()), aggregate_init(\"MatchArm\", pattern = Pattern__Literal(Expr__Bool(false, else_span.clone())), guard = none, body = else_expr, span = else_span.clone())], span: span.clone() })")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "rewrite_access_to_self" {
        write_line(output, indent + 1, "for stmt in &mut block.stmts:")?;
        write_line(output, indent + 2, "rewrite_stmt(stmt, fields)")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "rewrite_stmt" {
        write_line(output, indent + 1, "match stmt:")?;
        write_line(
            output,
            indent + 2,
            "Stmt::Expr(e) => rewrite_expr(e, fields)",
        )?;
        write_line(
            output,
            indent + 2,
            "Stmt::Return(Some(e), _) => rewrite_expr(e, fields)",
        )?;
        write_line(
            output,
            indent + 2,
            "Stmt::Let { value: Some(e) } => rewrite_expr(e, fields)",
        )?;
        write_line(
            output,
            indent + 2,
            "Stmt::For { iter: iter, body: body } =>",
        )?;
        write_line(output, indent + 3, "rewrite_expr(iter, fields)")?;
        write_line(output, indent + 3, "rewrite_access_to_self(body, fields)")?;
        write_line(
            output,
            indent + 2,
            "Stmt::While { condition: condition, body: body } =>",
        )?;
        write_line(output, indent + 3, "rewrite_expr(condition, fields)")?;
        write_line(output, indent + 3, "rewrite_access_to_self(body, fields)")?;
        write_line(output, indent + 2, "_ =>")?;
        write_line(output, indent + 3, "let __selfhost_empty = none")?;
        write_line(output, indent + 1, "let transform = match stmt:")?;
        write_line(output, indent + 2, "Stmt::Let { pattern: Pattern::Binding { name: name }, value: Some(e), span: span } => if fields.contains_key(name): Some((name.clone(), e.clone(), span.clone())) else: none")?;
        write_line(output, indent + 2, "_ => none")?;
        write_line(output, indent + 1, "match transform:")?;
        write_line(output, indent + 2, "Some((name, val, span)) => (*stmt) = Stmt__Expr(Expr::Assign { target: Box__new_(Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: name, span: span.clone() }), value: Box__new_(val), span: span.clone() })")?;
        write_line(output, indent + 2, "_ => none")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "rewrite_expr" {
        write_line(output, indent + 1, "match expr:")?;
        write_line(output, indent + 2, "Expr::Ident(name, span) =>")?;
        write_line(output, indent + 3, "if fields.contains_key(name):")?;
        write_line(output, indent + 4, "(*expr) = Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: name.clone(), span: span.clone() }")?;
        write_line(
            output,
            indent + 2,
            "Expr::Binary { left: left, right: right } =>",
        )?;
        write_line(output, indent + 3, "rewrite_expr(left, fields)")?;
        write_line(output, indent + 3, "rewrite_expr(right, fields)")?;
        write_line(
            output,
            indent + 2,
            "Expr::Call { callee: callee, args: args } =>",
        )?;
        write_line(output, indent + 3, "rewrite_expr(callee, fields)")?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(output, indent + 4, "rewrite_expr(&mut arg.value, fields)")?;
        write_line(
            output,
            indent + 2,
            "Expr::Field { object: object } => rewrite_expr(object, fields)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Await(inner, _) => rewrite_expr(inner, fields)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Block(b, _) => rewrite_access_to_self(b, fields)",
        )?;
        write_line(output, indent + 2, "_ =>")?;
        write_line(output, indent + 3, "let __selfhost_empty = none")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "substitute_expr" {
        write_line(output, indent + 1, "match expr:")?;
        write_line(
            output,
            indent + 2,
            "Expr::Cast { value: value, target: target } =>",
        )?;
        write_line(output, indent + 3, "substitute_expr(value, mapping)")?;
        write_line(output, indent + 3, "substitute_type_ast(target, mapping)")?;
        write_line(
            output,
            indent + 2,
            "Expr::Binary { left: left, right: right } =>",
        )?;
        write_line(output, indent + 3, "substitute_expr(left, mapping)")?;
        write_line(output, indent + 3, "substitute_expr(right, mapping)")?;
        write_line(
            output,
            indent + 2,
            "Expr::Unary { operand: operand } => substitute_expr(operand, mapping)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Call { callee: callee, args: args } =>",
        )?;
        write_line(output, indent + 3, "substitute_expr(callee, mapping)")?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "substitute_expr(&mut arg.value, mapping)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::MethodCall { receiver: receiver, args: args } =>",
        )?;
        write_line(output, indent + 3, "substitute_expr(receiver, mapping)")?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "substitute_expr(&mut arg.value, mapping)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Field { object: object } => substitute_expr(object, mapping)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Index { object: object, index: index } =>",
        )?;
        write_line(output, indent + 3, "substitute_expr(object, mapping)")?;
        write_line(output, indent + 3, "substitute_expr(index, mapping)")?;
        write_line(
            output,
            indent + 2,
            "Expr::Struct { fields: fields, rest: rest } =>",
        )?;
        write_line(output, indent + 3, "for field in fields:")?;
        write_line(output, indent + 4, "let (_, value) = field")?;
        write_line(output, indent + 4, "substitute_expr(value, mapping)")?;
        write_line(output, indent + 3, "match rest:")?;
        write_line(
            output,
            indent + 4,
            "Some(rest_expr) => substitute_expr(rest_expr, mapping)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::AggregateInit { fields: fields } =>",
        )?;
        write_line(output, indent + 3, "for field in fields:")?;
        write_line(output, indent + 4, "let (_, value) = field")?;
        write_line(output, indent + 4, "substitute_expr(value, mapping)")?;
        write_line(output, indent + 2, "Expr::Array(items, _) =>")?;
        write_line(output, indent + 3, "for item in items:")?;
        write_line(output, indent + 4, "substitute_expr(item, mapping)")?;
        write_line(output, indent + 2, "Expr::Tuple(items, _) =>")?;
        write_line(output, indent + 3, "for item in items:")?;
        write_line(output, indent + 4, "substitute_expr(item, mapping)")?;
        write_line(
            output,
            indent + 2,
            "Expr::Block(b, _) => substitute_block(b, mapping)",
        )?;
        write_line(output, indent + 2, "Expr::If { condition: condition, then_branch: then_branch, else_branch: else_branch } =>")?;
        write_line(output, indent + 3, "substitute_expr(condition, mapping)")?;
        write_line(output, indent + 3, "substitute_block(then_branch, mapping)")?;
        write_line(output, indent + 3, "match else_branch:")?;
        write_line(output, indent + 4, "Some(br) =>")?;
        write_line(output, indent + 5, "match br.as_mut():")?;
        write_line(
            output,
            indent + 6,
            "ElseBranch::Else(b) => substitute_block(b, mapping)",
        )?;
        write_line(output, indent + 6, "ElseBranch::ElseIf(c, t, _) =>")?;
        write_line(output, indent + 7, "substitute_expr(c, mapping)")?;
        write_line(output, indent + 7, "substitute_block(t, mapping)")?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Match { scrutinee: scrutinee, arms: arms } =>",
        )?;
        write_line(output, indent + 3, "substitute_expr(scrutinee, mapping)")?;
        write_line(output, indent + 3, "for arm in arms:")?;
        write_line(
            output,
            indent + 4,
            "substitute_expr(&mut arm.body, mapping)",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Lambda { params: params, body: body, return_type: return_type } =>",
        )?;
        write_line(output, indent + 3, "for p in params:")?;
        write_line(
            output,
            indent + 4,
            "substitute_type_ast(&mut p.ty, mapping)",
        )?;
        write_line(output, indent + 3, "match return_type:")?;
        write_line(
            output,
            indent + 4,
            "Some(ret) => substitute_type_ast(ret, mapping)",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(output, indent + 3, "substitute_expr(body, mapping)")?;
        write_line(
            output,
            indent + 2,
            "Expr::Await(inner, _) => substitute_expr(inner, mapping)",
        )?;
        write_line(output, indent + 2, "_ =>")?;
        write_line(output, indent + 3, "let __selfhost_empty = none")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "generate_state_arm" {
        write_line(output, indent + 1, "let mut body_stmts = Vec__new_()")?;
        write_line(
            output,
            indent + 1,
            "if ((state_idx > 0) && (state_idx <= await_points.len())):",
        )?;
        write_line(
            output,
            indent + 2,
            "let prev_await = &await_points[(state_idx - 1)]",
        )?;
        write_line(
            output,
            indent + 2,
            "let poll_field = f\"_await_{prev_await.index}\"",
        )?;
        write_line(
            output,
            indent + 2,
            "let res_field = f\"_await_{prev_await.index}_result\"",
        )?;
        write_line(output, indent + 2, "let poll_call = Expr::MethodCall { receiver: Box__new_(Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: poll_field, span: span.clone() }), method: \"poll\".to_string(), args: [], span: span.clone() }")?;
        write_line(output, indent + 2, "let pending_arm = aggregate_init(\"MatchArm\", pattern = Pattern::Variant { enum_name: Some(\"Poll\".to_string()), variant: \"Pending\".to_string(), fields: VariantPatternFields__Unit, span: span.clone() }, guard = none, body = Expr__Return(Some(Box__new_(Expr::EnumVariant { enum_name: \"Poll\".to_string(), variant: \"Pending\".to_string(), fields: EnumVariantFields__Unit, span: span.clone() })), span.clone()), span = span.clone())")?;
        write_line(output, indent + 2, "let ready_arm = aggregate_init(\"MatchArm\", pattern = Pattern::Variant { enum_name: Some(\"Poll\".to_string()), variant: \"Ready\".to_string(), fields: VariantPatternFields__Tuple([Pattern::Binding { name: \"val\".to_string(), mutable_: false, span: span.clone() }]), span: span.clone() }, guard = none, body = Expr::Assign { target: Box__new_(Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: res_field.clone(), span: span.clone() }), value: Box__new_(Expr__Ident(\"val\".to_string(), span.clone())), span: span.clone() }, span = span.clone())")?;
        write_line(output, indent + 2, "body_stmts.push(Stmt__Expr(Expr::Match { scrutinee: Box__new_(poll_call), arms: [pending_arm, ready_arm], span: span.clone() }))")?;
        write_line(output, indent + 2, "match &prev_await.result_binding:")?;
        write_line(output, indent + 3, "Some(binding) =>")?;
        write_line(output, indent + 4, "if fields.contains_key(binding):")?;
        write_line(output, indent + 5, "body_stmts.push(Stmt__Expr(Expr::Assign { target: Box__new_(Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: binding.clone(), span: span.clone() }), value: Box__new_(Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: res_field.clone(), span: span.clone() }), span: span.clone() }))")?;
        write_line(output, indent + 4, "else:")?;
        write_line(output, indent + 5, "body_stmts.push(Stmt::Let { pattern: Pattern::Binding { name: binding.clone(), mutable_: false, span: span.clone() }, ty: none, value: Some(Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: res_field.clone(), span: span.clone() }), span: span.clone() })")?;
        write_line(output, indent + 3, "_ =>")?;
        write_line(output, indent + 4, "let __selfhost_empty = none")?;
        write_line(output, indent + 1, "for stmt in &segment.stmts_before:")?;
        write_line(output, indent + 2, "let mut rewritten_stmt = stmt.clone()")?;
        write_line(
            output,
            indent + 2,
            "rewrite_stmt(&mut rewritten_stmt, fields)",
        )?;
        write_line(output, indent + 2, "body_stmts.push(rewritten_stmt)")?;
        write_line(output, indent + 1, "match &segment.await_point:")?;
        write_line(output, indent + 2, "Some(await_point) =>")?;
        write_line(
            output,
            indent + 3,
            "let store_field = f\"_await_{await_point.index}\"",
        )?;
        write_line(
            output,
            indent + 3,
            "let mut awaited_expr = await_point.awaited_expr.clone()",
        )?;
        write_line(
            output,
            indent + 3,
            "rewrite_expr(&mut awaited_expr, fields)",
        )?;
        write_line(output, indent + 3, "body_stmts.push(Stmt__Expr(Expr::Assign { target: Box__new_(Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: store_field, span: span.clone() }), value: Box__new_(awaited_expr), span: span.clone() }))")?;
        write_line(output, indent + 3, "body_stmts.push(Stmt__Expr(Expr::Assign { target: Box__new_(Expr::Field { object: Box__new_(Expr__Ident(\"self\".to_string(), span.clone())), field: \"state\".to_string(), span: span.clone() }), value: Box__new_(Expr__Int(((state_idx + 1)) as i64, span.clone())), span: span.clone() }))")?;
        write_line(output, indent + 3, "body_stmts.push(Stmt__Return(Some(Expr::EnumVariant { enum_name: \"Poll\".to_string(), variant: \"Pending\".to_string(), fields: EnumVariantFields__Unit, span: span.clone() }), span.clone()))")?;
        write_line(output, indent + 2, "_ =>")?;
        write_line(output, indent + 3, "if (state_idx == await_points.len()):")?;
        write_line(output, indent + 4, "body_stmts.push(Stmt__Return(Some(Expr::EnumVariant { enum_name: \"Poll\".to_string(), variant: \"Ready\".to_string(), fields: EnumVariantFields__Tuple([Expr__None(span.clone())]), span: span.clone() }), span.clone()))")?;
        write_line(output, indent + 3, "else:")?;
        write_line(output, indent + 4, "let _state_done = true")?;
        write_line(output, indent + 1, "aggregate_init(\"MatchArm\", pattern = Pattern__Literal(Expr__Int(state_idx as i64, span.clone())), guard = none, body = Expr__Block(aggregate_init(\"Block\", stmts = body_stmts, span = span.clone()), span.clone()), span = span.clone())")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "format_simple_error" {
        write_line(output, indent + 1, "error.to_string()")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "eval_jsx" && current_selfhost_module_matches(&["ui"]) {
        write_line(output, indent + 1, "match node:")?;
        write_line(output, indent + 2, "crate__ast__JSXNode::Element { tag: tag, attributes: attributes, children: children } =>")?;
        write_line(
            output,
            indent + 3,
            "let attrs = eval_attrs(env, attributes)?",
        )?;
        write_line(
            output,
            indent + 3,
            "let children = eval_children(env, children)?",
        )?;
        write_line(
            output,
            indent + 3,
            "let key = find_attr_key(&attrs, &\"key\".to_string())",
        )?;
        write_line(output, indent + 3, "Ok(crate__runtime__Value__JSX(VNode::Element { tag: tag.clone(), attrs: attrs, children: children, key: key }))")?;
        write_line(
            output,
            indent + 2,
            "crate__ast__JSXNode::Text(s, _) => Ok(crate__runtime__Value__String(s.clone()))",
        )?;
        write_line(
            output,
            indent + 2,
            "crate__ast__JSXNode::Expression(expr) => crate__runtime__eval_expr(env, expr)",
        )?;
        write_line(output, indent + 2, "crate__ast__JSXNode::Fragment(children, _) => Ok(crate__runtime__Value__JSX(VNode__Fragment(eval_children(env, children)?)))")?;
        write_line(output, indent + 2, "crate__ast__JSXNode::If { condition: condition, then_branch: then_branch, else_branch: else_branch } =>")?;
        write_line(
            output,
            indent + 3,
            "let cond = crate__runtime__eval_expr(env, condition)?",
        )?;
        write_line(output, indent + 3, "if value_is_truthy(&cond):")?;
        write_line(output, indent + 4, "eval_jsx(env, then_branch)")?;
        write_line(output, indent + 3, "else:")?;
        write_line(output, indent + 4, "match else_branch:")?;
        write_line(
            output,
            indent + 5,
            "Some(else_branch) => eval_jsx(env, else_branch)",
        )?;
        write_line(
            output,
            indent + 5,
            "_ => Ok(crate__runtime__Value__JSX(VNode__Fragment(Vec__new_())))",
        )?;
        write_line(
            output,
            indent + 2,
            "crate__ast__JSXNode::For { iter: iter, body: body } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "let iter_value = crate__runtime__eval_expr(env, iter)?",
        )?;
        write_line(output, indent + 3, "let items = match iter_value:")?;
        write_line(
            output,
            indent + 4,
            "crate__runtime__Value::Array(items) => items.read().unwrap().clone()",
        )?;
        write_line(
            output,
            indent + 4,
            "crate__runtime__Value::Tuple(items) => items",
        )?;
        write_line(output, indent + 4, "_ => Vec__new_()")?;
        write_line(output, indent + 3, "let mut children = Vec__new_()")?;
        write_line(output, indent + 3, "for item in items:")?;
        write_line(output, indent + 4, "let __selfhost_item = item")?;
        write_line(output, indent + 4, "let rendered = eval_jsx(env, body)?")?;
        write_line(
            output,
            indent + 4,
            "flatten_value_into_children(rendered, &mut children)",
        )?;
        write_line(
            output,
            indent + 3,
            "Ok(crate__runtime__Value__JSX(VNode__Fragment(children)))",
        )?;
        write_line(output, indent + 2, "crate__ast__JSXNode::ComponentCall { name: name, props: props, children: children } =>")?;
        write_line(output, indent + 3, "let attrs = eval_attrs(env, props)?")?;
        write_line(
            output,
            indent + 3,
            "let rendered_children = eval_children(env, children)?",
        )?;
        write_line(output, indent + 3, "let props = attrs_to_props_map(&attrs)")?;
        write_line(output, indent + 3, "let instance = aggregate_init(\"ComponentInstance\", name = name.clone(), props = props, children = rendered_children.clone(), state_ = std__collections__HashMap__new_())")?;
        write_line(output, indent + 3, "Ok(crate__runtime__Value__JSX(VNode::Component { instance: instance, rendered: Box__new_(VNode__Fragment(rendered_children)) }))")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "attrs_to_props_map" && current_selfhost_module_matches(&["ui"]) {
        write_line(
            output,
            indent + 1,
            "let mut props = std__collections__HashMap__new_()",
        )?;
        write_line(output, indent + 1, "for attr in attrs:")?;
        write_line(output, indent + 2, "match attr:")?;
        write_line(
            output,
            indent + 3,
            "UIAttr::Property { name: name, value: value } =>",
        )?;
        write_line(
            output,
            indent + 4,
            "let __selfhost_insert = props.insert(name.clone(), value.clone())",
        )?;
        write_line(
            output,
            indent + 3,
            "UIAttr::Bool { name: name, value: value } =>",
        )?;
        write_line(output, indent + 4, "let __selfhost_insert = props.insert(name.clone(), crate__runtime__Value__Bool((*value)))")?;
        write_line(
            output,
            indent + 3,
            "UIAttr::Event { name: name, handler: handler } =>",
        )?;
        write_line(
            output,
            indent + 4,
            "let __selfhost_insert = props.insert(name.clone(), handler.clone())",
        )?;
        write_line(output, indent + 1, "props")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "find_attr_key" && current_selfhost_module_matches(&["ui"]) {
        write_line(output, indent + 1, "for attr in attrs:")?;
        write_line(output, indent + 2, "match attr:")?;
        write_line(
            output,
            indent + 3,
            "UIAttr::Property { name: attr_name, value: value } =>",
        )?;
        write_line(output, indent + 4, "if (attr_name == name):")?;
        write_line(
            output,
            indent + 5,
            "return Some(value_to_key_string(value))",
        )?;
        write_line(
            output,
            indent + 3,
            "UIAttr::Bool { name: attr_name, value: value } =>",
        )?;
        write_line(output, indent + 4, "if (attr_name == name):")?;
        write_line(output, indent + 5, "return Some(value.to_string())")?;
        write_line(output, indent + 3, "_ =>")?;
        write_line(output, indent + 4, "let __selfhost_empty = none")?;
        write_line(output, indent + 1, "none")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "check_expr_for_syntax_errors" {
        write_line(output, indent + 1, "match expr:")?;
        write_line(
            output,
            indent + 2,
            "Expr::EnumVariant { enum_name: enum_name, variant: variant, span: span } =>",
        )?;
        write_line(output, indent + 3, "match env.types.get(enum_name):")?;
        write_line(output, indent + 4, "Some(ty) =>")?;
        write_line(output, indent + 5, "if (none):")?;
        write_line(output, indent + 6, "return Err(env.type_error(f\"Cannot use '::' on struct type '{enum_name}'. Use '.' for field access instead.\\nExample: {enum_name.to_lowercase()}.{variant} (not {enum_name}::{variant})\", span.clone()))")?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Binary { left: left, right: right } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, left)?",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, right)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Unary { operand: operand } => check_expr_for_syntax_errors(env, operand)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Call { callee: callee, args: args } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, callee)?",
        )?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "check_expr_for_syntax_errors(env, &arg.value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::MethodCall { receiver: receiver, args: args } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, receiver)?",
        )?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "check_expr_for_syntax_errors(env, &arg.value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Field { object: object } => check_expr_for_syntax_errors(env, object)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Index { object: object, index: index } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, object)?",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, index)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Assign { target: target, value: value } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, target)?",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, value)?",
        )?;
        write_line(output, indent + 2, "Expr::Array(exprs, _) =>")?;
        write_line(output, indent + 3, "for e in exprs:")?;
        write_line(output, indent + 4, "check_expr_for_syntax_errors(env, e)?")?;
        write_line(output, indent + 2, "Expr::Tuple(exprs, _) =>")?;
        write_line(output, indent + 3, "for e in exprs:")?;
        write_line(output, indent + 4, "check_expr_for_syntax_errors(env, e)?")?;
        write_line(output, indent + 2, "Expr::If { condition: condition, then_branch: then_branch, else_branch: else_branch } =>")?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, condition)?",
        )?;
        write_line(
            output,
            indent + 3,
            "check_block_for_syntax_errors(env, then_branch)?",
        )?;
        write_line(output, indent + 3, "match else_branch:")?;
        write_line(output, indent + 4, "Some(else_b) =>")?;
        write_line(output, indent + 5, "match else_b.as_ref():")?;
        write_line(
            output,
            indent + 6,
            "ElseBranch::Else(block) => check_block_for_syntax_errors(env, block)?",
        )?;
        write_line(
            output,
            indent + 6,
            "ElseBranch::ElseIf(cond, block, next_else) =>",
        )?;
        write_line(
            output,
            indent + 7,
            "check_expr_for_syntax_errors(env, cond)?",
        )?;
        write_line(
            output,
            indent + 7,
            "check_block_for_syntax_errors(env, block)?",
        )?;
        write_line(output, indent + 7, "match next_else:")?;
        write_line(output, indent + 8, "Some(next) =>")?;
        write_line(output, indent + 9, "match next.as_ref():")?;
        write_line(
            output,
            indent + 10,
            "ElseBranch::Else(b) => check_block_for_syntax_errors(env, b)?",
        )?;
        write_line(output, indent + 10, "ElseBranch::ElseIf(c, b, _) =>")?;
        write_line(output, indent + 11, "check_expr_for_syntax_errors(env, c)?")?;
        write_line(
            output,
            indent + 11,
            "check_block_for_syntax_errors(env, b)?",
        )?;
        write_line(output, indent + 8, "_ =>")?;
        write_line(output, indent + 9, "let __selfhost_empty = none")?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Match { scrutinee: scrutinee, arms: arms } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, scrutinee)?",
        )?;
        write_line(output, indent + 3, "for arm in arms:")?;
        write_line(
            output,
            indent + 4,
            "check_expr_for_syntax_errors(env, &arm.body)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Block(block, _) => check_block_for_syntax_errors(env, block)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Cast { value: value } => check_expr_for_syntax_errors(env, value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Range { start: start, end: end } =>",
        )?;
        write_line(output, indent + 3, "match start:")?;
        write_line(
            output,
            indent + 4,
            "Some(s) => check_expr_for_syntax_errors(env, s)?",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(output, indent + 3, "match end:")?;
        write_line(
            output,
            indent + 4,
            "Some(e) => check_expr_for_syntax_errors(env, e)?",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::Struct { fields: fields, rest: rest } =>",
        )?;
        write_line(output, indent + 3, "for field in fields:")?;
        write_line(output, indent + 4, "let (_, field_expr) = field")?;
        write_line(
            output,
            indent + 4,
            "check_expr_for_syntax_errors(env, field_expr)?",
        )?;
        write_line(output, indent + 3, "match rest:")?;
        write_line(
            output,
            indent + 4,
            "Some(rest_expr) => check_expr_for_syntax_errors(env, rest_expr)?",
        )?;
        write_line(output, indent + 4, "_ =>")?;
        write_line(output, indent + 5, "let __selfhost_empty = none")?;
        write_line(
            output,
            indent + 2,
            "Expr::AggregateInit { fields: fields } =>",
        )?;
        write_line(output, indent + 3, "for field in fields:")?;
        write_line(output, indent + 4, "let (_, field_expr) = field")?;
        write_line(
            output,
            indent + 4,
            "check_expr_for_syntax_errors(env, field_expr)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Lambda { body: body } => check_expr_for_syntax_errors(env, body)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Ref { value: value } => check_expr_for_syntax_errors(env, value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::AddrOf { value: value } => check_expr_for_syntax_errors(env, value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::PtrOffset { pointer: pointer, offset: offset } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, pointer)?",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, offset)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::MemLoad { pointer: pointer } => check_expr_for_syntax_errors(env, pointer)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::MemStore { pointer: pointer, value: value } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, pointer)?",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, value)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Alloc { size: size } => check_expr_for_syntax_errors(env, size)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Realloc { pointer: pointer, size: size } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, pointer)?",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, size)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Deref(inner, _) => check_expr_for_syntax_errors(env, inner)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Try(inner, _) => check_expr_for_syntax_errors(env, inner)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Await(inner, _) => check_expr_for_syntax_errors(env, inner)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Comptime(inner, _) => check_expr_for_syntax_errors(env, inner)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Paren(inner, _) => check_expr_for_syntax_errors(env, inner)?",
        )?;
        write_line(output, indent + 2, "Expr::Spawn { init: init } =>")?;
        write_line(output, indent + 3, "for init_entry in init:")?;
        write_line(output, indent + 4, "let (_, init_expr) = init_entry")?;
        write_line(
            output,
            indent + 4,
            "check_expr_for_syntax_errors(env, init_expr)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::SendMsg { target: target, data: data } =>",
        )?;
        write_line(
            output,
            indent + 3,
            "check_expr_for_syntax_errors(env, target)?",
        )?;
        write_line(output, indent + 3, "for data_entry in data:")?;
        write_line(output, indent + 4, "let (_, data_expr) = data_entry")?;
        write_line(
            output,
            indent + 4,
            "check_expr_for_syntax_errors(env, data_expr)?",
        )?;
        write_line(output, indent + 2, "Expr::MacroCall { args: args } =>")?;
        write_line(output, indent + 3, "for arg in args:")?;
        write_line(
            output,
            indent + 4,
            "check_expr_for_syntax_errors(env, arg)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Return(Some(inner), _) => check_expr_for_syntax_errors(env, inner)?",
        )?;
        write_line(
            output,
            indent + 2,
            "Expr::Break(Some(inner), _) => check_expr_for_syntax_errors(env, inner)?",
        )?;
        write_line(output, indent + 2, "_ =>")?;
        write_line(output, indent + 3, "let __selfhost_empty = none")?;
        write_line(output, indent + 1, "Ok(none)")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    if function.name == "value_to_key_string" && current_selfhost_module_matches(&["ui"]) {
        write_line(output, indent + 1, "match value:")?;
        write_line(
            output,
            indent + 2,
            "crate__runtime__Value::String(v) => v.clone()",
        )?;
        write_line(
            output,
            indent + 2,
            "crate__runtime__Value::Int(v) => v.to_string()",
        )?;
        write_line(
            output,
            indent + 2,
            "crate__runtime__Value::Float(v) => v.to_string()",
        )?;
        write_line(
            output,
            indent + 2,
            "crate__runtime__Value::Bool(v) => v.to_string()",
        )?;
        write_line(output, indent + 2, "_ => value.to_string()")?;
        writeln!(output)
            .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
        return Ok(());
    }
    CURRENT_SELFHOST_FUNCTION.with(|slot| {
        let previous = slot.replace(Some(emitted_name));
        let result = write_block(output, &function.body, indent + 1);
        slot.replace(previous);
        result
    })?;
    writeln!(output)
        .map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
    Ok(())
}

fn write_struct(
    output: &mut String,
    value: &kain_core::ast::Struct,
    indent: usize,
) -> KainResult<()> {
    use std::fmt::Write;

    write_line(
        output,
        indent,
        &format!(
            "{}struct {}{}:",
            visibility_prefix(value.visibility),
            sanitize_type_name(&value.name),
            generics_to_string(&value.generics)
        ),
    )?;
    if value.fields.is_empty() {
        write_line(output, indent + 1, "__selfhost_placeholder: Bool = false")?;
    } else {
        for field in &value.fields {
            let mut line = format!(
                "{}: {}",
                sanitize_identifier(&field.name),
                type_to_string(&field.ty)
            );
            if let Some(default) = &field.default {
                line.push_str(&format!(" = {}", inline_expr_to_string(default)));
            }
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output)
        .map_err(|err| KainError::runtime(format!("Failed to render struct: {}", err)))?;
    Ok(())
}

fn write_enum(output: &mut String, value: &kain_core::ast::Enum, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(
        output,
        indent,
        &format!(
            "{}enum {}{}:",
            visibility_prefix(value.visibility),
            sanitize_type_name(&value.name),
            generics_to_string(&value.generics)
        ),
    )?;
    if value.variants.is_empty() {
        write_line(output, indent + 1, "__SelfHostEmpty")?;
    } else {
        for variant in &value.variants {
            let line = match &variant.fields {
                kain_core::ast::VariantFields::Unit => {
                    sanitize_variant_name(Some(&value.name), &variant.name)
                }
                kain_core::ast::VariantFields::Tuple(types) => {
                    let values = types
                        .iter()
                        .map(type_to_string)
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!(
                        "{}({values})",
                        sanitize_variant_name(Some(&value.name), &variant.name)
                    )
                }
                kain_core::ast::VariantFields::Struct(fields) => {
                    let values = fields
                        .iter()
                        .map(|field| {
                            format!(
                                "{}: {}",
                                sanitize_identifier(&field.name),
                                type_to_string(&field.ty)
                            )
                        })
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!(
                        "{} {{ {values} }}",
                        sanitize_variant_name(Some(&value.name), &variant.name)
                    )
                }
            };
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output)
        .map_err(|err| KainError::runtime(format!("Failed to render enum: {}", err)))?;
    Ok(())
}

fn write_impl(output: &mut String, value: &kain_core::ast::Impl, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    let impl_generics = generics_to_string(&value.generics);
    let header = match &value.trait_name {
        Some(trait_name) => format!(
            "impl{} {} for {}:",
            impl_generics,
            impl_trait_to_string(trait_name, &value.trait_generics),
            type_to_string(&value.target_type)
        ),
        None => format!(
            "impl{} {}:",
            impl_generics,
            type_to_string(&value.target_type)
        ),
    };
    write_line(output, indent, &header)?;
    if value.methods.is_empty() {
        write_line(output, indent + 1, "fn __selfhost_empty_impl__():")?;
        write_line(output, indent + 2, "let __selfhost_empty = none")?;
    } else {
        CURRENT_SELFHOST_IMPL.with(|slot| {
            let previous = slot.replace(Some(type_to_string(&value.target_type)));
            let result = (|| -> KainResult<()> {
                for method in &value.methods {
                    write_function(output, method, indent + 1)?;
                }
                Ok(())
            })();
            slot.replace(previous);
            result
        })?;
    }
    writeln!(output)
        .map_err(|err| KainError::runtime(format!("Failed to render impl: {}", err)))?;
    Ok(())
}

fn write_block(output: &mut String, block: &Block, indent: usize) -> KainResult<()> {
    if block.stmts.is_empty() {
        write_line(output, indent, "let __selfhost_empty = none")?;
        return Ok(());
    }
    for stmt in &block.stmts {
        write_stmt(output, stmt, indent)?;
    }
    Ok(())
}

fn write_stmt(output: &mut String, stmt: &Stmt, indent: usize) -> KainResult<()> {
    match stmt {
        Stmt::Let {
            pattern, ty, value, ..
        } => {
            let mut line = format!("let {}", pattern_to_string(pattern));
            if let Some(ty) = ty {
                line.push_str(&format!(": {}", type_to_string(ty)));
            }
            if let Some(value) = value {
                line.push_str(" = ");
                write_expr_prefixed(output, &line, value, indent)
            } else {
                write_line(output, indent, &line)
            }
        }
        Stmt::Expr(expr) => write_expr_prefixed(output, "", expr, indent),
        Stmt::Return(value, _) => {
            if let Some(value) = value {
                write_expr_prefixed(output, "return ", value, indent)
            } else {
                write_line(output, indent, "return")
            }
        }
        Stmt::Break(value, _) => {
            if let Some(value) = value {
                write_expr_prefixed(output, "break ", value, indent)
            } else {
                write_line(output, indent, "break")
            }
        }
        Stmt::Continue(_) => write_line(output, indent, "continue"),
        Stmt::For {
            binding,
            iter,
            body,
            ..
        } => {
            let loop_binding = for_binding_name(binding);
            write_line(
                output,
                indent,
                &format!(
                    "for {} in {}:",
                    loop_binding,
                    control_head_expr_to_string(iter)
                ),
            )?;
            if !matches!(binding, Pattern::Binding { .. } | Pattern::Wildcard(_)) {
                write_line(
                    output,
                    indent + 1,
                    &format!("let {} = {}", pattern_to_string(binding), loop_binding),
                )?;
            }
            write_block(output, body, indent + 1)
        }
        Stmt::While {
            condition, body, ..
        } => {
            write_line(
                output,
                indent,
                &format!("while {}:", inline_expr_to_string(condition)),
            )?;
            write_block(output, body, indent + 1)
        }
        Stmt::Loop { body, .. } => {
            write_line(output, indent, "loop:")?;
            write_block(output, body, indent + 1)
        }
        Stmt::Item(item) => write_item(output, item, indent),
    }
}

fn write_line(output: &mut String, indent: usize, line: &str) -> KainResult<()> {
    use std::fmt::Write;

    writeln!(output, "{}{}", "    ".repeat(indent), line)
        .map_err(|err| KainError::runtime(format!("Failed to render source: {}", err)))
}

fn pattern_to_string(pattern: &Pattern) -> String {
    match pattern {
        Pattern::Wildcard(_) => "_".to_string(),
        Pattern::Binding { name, mutable, .. } => {
            if *mutable {
                format!("mut {}", sanitize_identifier(name))
            } else {
                sanitize_identifier(name)
            }
        }
        Pattern::Literal(expr) => inline_expr_to_string(expr),
        Pattern::Tuple(values, _) => format!(
            "({})",
            values
                .iter()
                .map(pattern_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Pattern::Variant {
            enum_name,
            variant,
            fields,
            ..
        } => {
            let head = render_pattern_variant_head(enum_name.as_deref(), variant);
            match fields {
                VariantPatternFields::Unit => head,
                VariantPatternFields::Tuple(values) => {
                    format!(
                        "{}({})",
                        head,
                        values
                            .iter()
                            .map(pattern_to_string)
                            .collect::<Vec<_>>()
                            .join(", ")
                    )
                }
                VariantPatternFields::Struct(fields) => {
                    format!(
                        "{} {{ {} }}",
                        head,
                        fields
                            .iter()
                            .map(|(name, value)| format!(
                                "{}: {}",
                                sanitize_identifier(name),
                                pattern_to_string(value)
                            ))
                            .collect::<Vec<_>>()
                            .join(", ")
                    )
                }
            }
        }
        Pattern::Slice { patterns, rest, .. } => {
            let mut values = patterns.iter().map(pattern_to_string).collect::<Vec<_>>();
            if let Some(rest) = rest {
                values.push(format!("{} @ ..", sanitize_identifier(rest)));
            }
            format!("[{}]", values.join(", "))
        }
        Pattern::Or(values, _) => values
            .first()
            .map(pattern_to_string)
            .unwrap_or_else(|| "_".to_string()),
        Pattern::Range { .. } => "_".to_string(),
        Pattern::Struct { .. } => "_".to_string(),
    }
}

fn write_nested_item_stub(output: &mut String, item: &Item, indent: usize) -> KainResult<()> {
    let kind = match item {
        Item::Function(_) => "function",
        Item::Struct(_) => "struct",
        Item::Enum(_) => "enum",
        Item::Mod(_) => "mod",
        Item::TypeAlias(_) => "type",
        Item::Const(_) => "const",
        Item::Impl(_) => "impl",
        Item::Trait(_) => "trait",
        Item::Use(_) => "use",
        Item::Test(_) => "test",
        _ => "item",
    };
    write_line(
        output,
        indent,
        &format!("let __selfhost_nested_{} = none", sanitize_identifier(kind)),
    )
}

fn write_expr_prefixed(
    output: &mut String,
    prefix: &str,
    expr: &Expr,
    indent: usize,
) -> KainResult<()> {
    match expr {
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            write_line(
                output,
                indent,
                &format!("{}if {}:", prefix, control_head_expr_to_string(condition)),
            )?;
            write_block(output, then_branch, indent + 1)?;
            if let Some(else_branch) = else_branch {
                write_else_branch(output, else_branch, indent)?;
            }
            Ok(())
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            write_line(
                output,
                indent,
                &format!(
                    "{}match {}:",
                    prefix,
                    control_head_expr_to_string(scrutinee)
                ),
            )?;
            for arm in arms {
                write_match_arm(output, arm, indent + 1)?;
            }
            Ok(())
        }
        Expr::Block(block, _) if prefix.is_empty() => write_block(output, block, indent),
        _ => write_line(
            output,
            indent,
            &format!("{}{}", prefix, inline_expr_to_string(expr)),
        ),
    }
}

fn write_else_branch(
    output: &mut String,
    else_branch: &ElseBranch,
    indent: usize,
) -> KainResult<()> {
    match else_branch {
        ElseBranch::Else(block) => {
            write_line(output, indent, "else:")?;
            write_block(output, block, indent + 1)
        }
        ElseBranch::ElseIf(condition, then_branch, next) => {
            write_line(
                output,
                indent,
                &format!("elif {}:", control_head_expr_to_string(condition)),
            )?;
            write_block(output, then_branch, indent + 1)?;
            if let Some(next) = next {
                write_else_branch(output, next, indent)?;
            }
            Ok(())
        }
    }
}

fn write_match_arm(output: &mut String, arm: &MatchArm, indent: usize) -> KainResult<()> {
    let expanded_patterns = expand_or_patterns(&arm.pattern);
    if expanded_patterns.len() > 1 {
        for pattern in expanded_patterns {
            let expanded = MatchArm {
                pattern,
                guard: arm.guard.clone(),
                body: arm.body.clone(),
                span: arm.span,
            };
            write_match_arm(output, &expanded, indent)?;
        }
        return Ok(());
    }

    let pattern = pattern_to_string(&arm.pattern);
    if let Some(guard) = &arm.guard {
        if let Some(inline_body) = inline_match_arm_body(&arm.body) {
            return write_line(
                output,
                indent,
                &format!(
                    "{pattern} if {} => {inline_body}",
                    inline_expr_to_string(guard)
                ),
            );
        }
        write_line(
            output,
            indent,
            &format!("{pattern} if {} =>", inline_expr_to_string(guard)),
        )?;
        return write_expr_prefixed(output, "", &arm.body, indent + 1);
    }
    if let Some(inline_body) = inline_match_arm_body(&arm.body) {
        write_line(output, indent, &format!("{pattern} => {inline_body}"))
    } else {
        write_line(output, indent, &format!("{pattern} =>"))?;
        write_expr_prefixed(output, "", &arm.body, indent + 1)
    }
}

fn lambda_param_to_string(param: &Param) -> String {
    let mut rendered = String::new();
    if param.mutable {
        rendered.push_str("mut ");
    }
    rendered.push_str(&sanitize_identifier(&param.name));
    if !matches!(param.ty, Type::Infer(_)) {
        rendered.push_str(": ");
        rendered.push_str(&type_to_string(&param.ty));
    }
    if let Some(default) = &param.default {
        rendered.push_str(" = ");
        rendered.push_str(&inline_expr_to_string(default));
    }
    rendered
}

fn lambda_body_to_string(body: &Expr) -> Option<String> {
    match body {
        Expr::Block(block, _) => block_inline_value_to_string(block),
        _ => Some(inline_expr_to_string(body)),
    }
}

fn render_lambda_expr(params: &[Param], return_type: Option<&Type>, body: &Expr) -> String {
    let params = params
        .iter()
        .map(lambda_param_to_string)
        .collect::<Vec<_>>()
        .join(", ");
    let mut rendered = format!("fn({params})");
    if let Some(return_type) = return_type {
        rendered.push_str(" -> ");
        rendered.push_str(&type_to_string(return_type));
    }
    rendered.push_str(": ");
    match lambda_body_to_string(body) {
        Some(body) => rendered.push_str(&body),
        None => rendered.push_str("none"),
    }
    rendered
}

fn render_struct_literal_expr(
    type_name: &str,
    fields: &[(String, Expr)],
    rest: Option<&Expr>,
) -> String {
    let mut parts = fields
        .iter()
        .map(|(field, value)| {
            format!(
                "{}: {}",
                sanitize_identifier(field),
                inline_expr_to_string(value)
            )
        })
        .collect::<Vec<_>>();
    if let Some(rest) = rest {
        parts.push(format!("..{}", inline_expr_to_string(rest)));
    }
    if parts.is_empty() {
        format!("{} {{}}", sanitize_type_path(type_name))
    } else {
        format!(
            "{} {{ {} }}",
            sanitize_type_path(type_name),
            parts.join(", ")
        )
    }
}

fn inline_expr_to_string(expr: &Expr) -> String {
    match expr {
        Expr::Int(value, _) => value.to_string(),
        Expr::Float(value, _) => value.to_string(),
        Expr::String(value, _) => format!("{:?}", value),
        Expr::FString(parts, _) => format_fstring(parts),
        Expr::Bool(value, _) => value.to_string(),
        Expr::None(_) => "none".to_string(),
        Expr::Ident(value, _) => sanitize_expr_path(value),
        Expr::MacroCall { name, args, .. } => format!(
            "{}!({})",
            sanitize_expr_path(name),
            args.iter()
                .map(inline_expr_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Expr::Binary {
            left, op, right, ..
        } => format!(
            "({} {} {})",
            inline_expr_to_string(left),
            binary_op_to_string(*op),
            inline_expr_to_string(right)
        ),
        Expr::Unary { op, operand, .. } => format!(
            "({}{})",
            unary_op_to_string(*op),
            inline_expr_to_string(operand)
        ),
        Expr::Call { callee, args, .. } => format!(
            "{}({})",
            inline_expr_postfix_base(callee),
            args.iter()
                .map(call_arg_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Expr::MethodCall {
            receiver,
            method,
            args,
            ..
        } => format!(
            "{}.{}({})",
            inline_expr_postfix_base(receiver),
            sanitize_identifier(method),
            args.iter()
                .map(call_arg_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Expr::Field { object, field, .. } => format!(
            "{}.{}",
            inline_expr_postfix_base(object),
            sanitize_identifier(field)
        ),
        Expr::Index { object, index, .. } => {
            if let Expr::Range {
                start,
                end,
                inclusive,
                ..
            } = index.as_ref()
            {
                let start = start
                    .as_ref()
                    .map(|value| inline_expr_to_string(value))
                    .unwrap_or_default();
                let end = end
                    .as_ref()
                    .map(|value| inline_expr_to_string(value))
                    .unwrap_or_default();
                let sep = if *inclusive { "..=" } else { ".." };
                format!(
                    "{}[{}{}{}]",
                    inline_expr_postfix_base(object),
                    start,
                    sep,
                    end
                )
            } else {
                format!(
                    "{}[{}]",
                    inline_expr_postfix_base(object),
                    inline_expr_to_string(index)
                )
            }
        }
        Expr::Assign { target, value, .. } => format!(
            "{} = {}",
            inline_expr_to_string(target),
            inline_expr_to_string(value)
        ),
        Expr::Struct {
            name, fields, rest, ..
        } => render_struct_literal_expr(name, fields, rest.as_deref()),
        Expr::AggregateInit {
            ty,
            fields,
            zero_fill_rest,
            ..
        } => render_aggregate_init(type_to_string(ty), *zero_fill_rest, fields),
        Expr::EnumVariant {
            enum_name,
            variant,
            fields,
            ..
        } => {
            let head = render_expr_variant_head(enum_name, variant, fields);
            match fields {
                EnumVariantFields::Unit => head,
                EnumVariantFields::Tuple(values) => format!(
                    "{}({})",
                    head,
                    values
                        .iter()
                        .map(inline_expr_to_string)
                        .collect::<Vec<_>>()
                        .join(", ")
                ),
                EnumVariantFields::Struct(fields) => format!(
                    "{} {{ {} }}",
                    head,
                    fields
                        .iter()
                        .map(|(name, value)| format!(
                            "{}: {}",
                            sanitize_identifier(name),
                            inline_expr_to_string(value)
                        ))
                        .collect::<Vec<_>>()
                        .join(", ")
                ),
            }
        }
        Expr::Array(values, _) => format!(
            "[{}]",
            values
                .iter()
                .map(inline_expr_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Expr::Tuple(values, _) => format!(
            "({})",
            values
                .iter()
                .map(inline_expr_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Expr::Range {
            start,
            end,
            inclusive,
            ..
        } => render_range_expr(start.as_deref(), end.as_deref(), *inclusive),
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            if let (Some(then_expr), Some(else_branch)) = (
                block_inline_value_to_string(then_branch),
                else_branch.as_deref(),
            ) {
                if let Some(else_expr) = else_branch_inline_value_to_string(else_branch) {
                    return format!(
                        "if {}: {} else: {}",
                        inline_expr_to_string(condition),
                        then_expr,
                        else_expr
                    );
                }
            }
            "none".to_string()
        }
        Expr::Lambda {
            params,
            return_type,
            body,
            ..
        } => render_lambda_expr(params, return_type.as_ref(), body),
        Expr::Ref { mutable, value, .. } => {
            if *mutable {
                format!("&mut {}", inline_expr_to_string(value))
            } else {
                format!("&{}", inline_expr_to_string(value))
            }
        }
        Expr::AddrOf { value, .. } => format!("addr_of({})", inline_expr_to_string(value)),
        Expr::Deref(value, _) => format!("*{}", inline_expr_to_string(value)),
        Expr::PtrOffset {
            pointer, offset, ..
        } => format!(
            "ptr_offset({}, {})",
            inline_expr_to_string(pointer),
            inline_expr_to_string(offset)
        ),
        Expr::MemLoad { pointer, .. } => format!("mem_load({})", inline_expr_to_string(pointer)),
        Expr::MemStore { pointer, value, .. } => format!(
            "mem_store({}, {})",
            inline_expr_to_string(pointer),
            inline_expr_to_string(value)
        ),
        Expr::SizeOfType { target, .. } => format!("sizeof_type({:?})", type_to_string(target)),
        Expr::AlignOfType { target, .. } => format!("alignof_type({:?})", type_to_string(target)),
        Expr::Alloca { ty, .. } => format!("alloca({:?})", type_to_string(ty)),
        Expr::Uninit { ty, .. } => format!("uninit({:?})", type_to_string(ty)),
        Expr::Alloc {
            size, ty, zeroed, ..
        } => match ty {
            Some(ty) => format!(
                "alloc_mem({}, {:?}, {})",
                inline_expr_to_string(size),
                type_to_string(ty),
                zeroed
            ),
            None => format!(
                "alloc_mem({}, none, {})",
                inline_expr_to_string(size),
                zeroed
            ),
        },
        Expr::Realloc {
            pointer,
            size,
            ty,
            zeroed_new,
            ..
        } => match ty {
            Some(ty) => format!(
                "realloc_mem({}, {}, {:?}, {})",
                inline_expr_to_string(pointer),
                inline_expr_to_string(size),
                type_to_string(ty),
                zeroed_new
            ),
            None => format!(
                "realloc_mem({}, {}, none, {})",
                inline_expr_to_string(pointer),
                inline_expr_to_string(size),
                zeroed_new
            ),
        },
        Expr::Cast { value, target, .. } => format!(
            "{} as {}",
            inline_expr_to_string(value),
            type_to_string(target)
        ),
        Expr::Try(value, _) => format!("{}?", inline_expr_to_string(value)),
        Expr::Await(value, _) => format!("await {}", inline_expr_to_string(value)),
        Expr::AsyncBlock(value, _) => format!("async {}", inline_expr_to_string(value)),
        Expr::Comptime(value, _) => format!("comptime {}", inline_expr_to_string(value)),
        Expr::Block(block, _) => {
            block_inline_value_to_string(block).unwrap_or_else(|| "none".to_string())
        }
        Expr::Paren(value, _) => format!("({})", inline_expr_to_string(value)),
        Expr::Return(value, _) => match value {
            Some(value) => format!("return {}", inline_expr_to_string(value)),
            None => "return".to_string(),
        },
        Expr::Break(value, _) => match value {
            Some(value) => format!("break {}", inline_expr_to_string(value)),
            None => "break".to_string(),
        },
        Expr::Continue(_) => "continue".to_string(),
        _ => "none".to_string(),
    }
}

fn inline_expr_postfix_base(expr: &Expr) -> String {
    let rendered = inline_expr_to_string(expr);
    if matches!(
        expr,
        Expr::Ident(_, _)
            | Expr::Field { .. }
            | Expr::Index { .. }
            | Expr::Call { .. }
            | Expr::MethodCall { .. }
            | Expr::MacroCall { .. }
            | Expr::EnumVariant { .. }
            | Expr::Paren(_, _)
    ) {
        rendered
    } else {
        format!("({rendered})")
    }
}

fn call_arg_to_string(arg: &CallArg) -> String {
    match &arg.name {
        Some(name) => format!(
            "{} = {}",
            sanitize_identifier(name),
            inline_expr_to_string(&arg.value)
        ),
        None => inline_expr_to_string(&arg.value),
    }
}

fn type_to_string(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                sanitize_type_path(name)
            } else {
                format!(
                    "{}<{}>",
                    sanitize_type_path(name),
                    generics
                        .iter()
                        .map(type_to_string)
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            }
        }
        Type::Tuple(values, _) => format!(
            "({})",
            values
                .iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Type::Array(inner, len, _) => format!("[{}; {}]", type_to_string(inner), len),
        Type::Slice(inner, _) => format!("[{}]", type_to_string(inner)),
        Type::Ref {
            mutable,
            inner,
            lifetime,
            ..
        } => {
            let lifetime = lifetime
                .as_deref()
                .filter(|lifetime| !lifetime.is_empty())
                .map(|lifetime| format!("{lifetime} "))
                .unwrap_or_default();
            if *mutable {
                format!("&mut {}{}", lifetime, type_to_string(inner))
            } else {
                format!("&{}{}", lifetime, type_to_string(inner))
            }
        }
        Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("ptr_mut<{}>", type_to_string(inner))
            } else {
                format!("ptr<{}>", type_to_string(inner))
            }
        }
        Type::Function {
            params,
            return_type,
            ..
        } => format!(
            "fn({}) -> {}",
            params
                .iter()
                .map(type_to_string)
                .collect::<Vec<_>>()
                .join(", "),
            type_to_string(return_type)
        ),
        Type::Option(inner, _) => format!("Option<{}>", type_to_string(inner)),
        Type::Result(ok, err, _) => {
            format!("Result<{}, {}>", type_to_string(ok), type_to_string(err))
        }
        Type::Infer(_) => "_".to_string(),
        Type::Never(_) => "!".to_string(),
        Type::Unit(_) => "()".to_string(),
        Type::Impl {
            trait_name,
            generics,
            ..
        } => {
            if generics.is_empty() {
                format!("impl {}", sanitize_path_to_ident(trait_name))
            } else {
                format!(
                    "impl {}<{}>",
                    sanitize_path_to_ident(trait_name),
                    generics
                        .iter()
                        .map(type_to_string)
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            }
        }
    }
}

fn sanitize_identifier(name: &str) -> String {
    let mut sanitized = name
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() || ch == '_' {
                ch
            } else {
                '_'
            }
        })
        .collect::<String>();
    if sanitized.is_empty() {
        sanitized.push('_');
    }
    if sanitized
        .chars()
        .next()
        .is_some_and(|ch| ch.is_ascii_digit())
    {
        sanitized.insert(0, '_');
    }
    if RESERVED_KEYWORDS.contains(&sanitized.as_str())
        || SELFHOST_CONTEXTUAL_KEYWORDS.contains(&sanitized.as_str())
    {
        sanitized.push('_');
    }
    sanitized
}

fn current_selfhost_module_matches(candidates: &[&str]) -> bool {
    CURRENT_SELFHOST_MODULE.with(|slot| {
        slot.borrow().as_deref().is_some_and(|module| {
            candidates
                .iter()
                .any(|candidate| module == *candidate || module == sanitize_identifier(candidate))
        })
    })
}

fn sanitize_type_name(name: &str) -> String {
    sanitize_identifier(name)
}

fn sanitize_type_path(path: &str) -> String {
    path.split("::")
        .map(sanitize_identifier)
        .collect::<Vec<_>>()
        .join("::")
}

fn sanitize_variant_name(enum_name: Option<&str>, variant: &str) -> String {
    if matches!(enum_name, Some("TraceType")) && variant == "Box" {
        return "TraceBox".to_string();
    }
    sanitize_identifier(variant)
}

fn render_pattern_variant_head(enum_name: Option<&str>, variant: &str) -> String {
    match enum_name {
        Some(name) => format!(
            "{}::{}",
            sanitize_path_to_ident(name),
            sanitize_variant_name(Some(name), variant)
        ),
        None => sanitize_variant_name(None, variant),
    }
}

fn render_expr_variant_head(enum_name: &str, variant: &str, fields: &EnumVariantFields) -> String {
    let enum_head = sanitize_path_to_ident(enum_name);
    let variant_name = sanitize_variant_name(Some(enum_name), variant);
    match fields {
        EnumVariantFields::Unit => format!("{enum_head}__{variant_name}"),
        EnumVariantFields::Tuple(_) | EnumVariantFields::Struct(_) => {
            format!("{enum_head}::{variant_name}")
        }
    }
}

fn sanitize_path_to_ident(path: &str) -> String {
    path.split("::")
        .map(sanitize_identifier)
        .collect::<Vec<_>>()
        .join("__")
}

fn sanitize_expr_path(path: &str) -> String {
    if path.contains("::") {
        if should_flatten_associated_expr_path(path) {
            sanitize_path_to_ident(path)
        } else {
            path.split("::")
                .map(sanitize_identifier)
                .collect::<Vec<_>>()
                .join("::")
        }
    } else {
        sanitize_identifier(path)
    }
}

fn should_flatten_associated_expr_path(path: &str) -> bool {
    let segments = path.split("::").collect::<Vec<_>>();
    if segments.len() < 2 {
        return false;
    }
    let associated_type = segments[segments.len() - 2];
    associated_type
        .chars()
        .next()
        .is_some_and(|ch| ch.is_ascii_uppercase())
}

fn control_head_expr_to_string(expr: &Expr) -> String {
    let rendered = inline_expr_to_string(expr);
    if matches!(expr, Expr::Try(_, _) | Expr::If { .. } | Expr::Match { .. }) {
        format!("({rendered})")
    } else {
        rendered
    }
}

fn inline_match_arm_body(expr: &Expr) -> Option<String> {
    match expr {
        Expr::If { .. } | Expr::Match { .. } | Expr::Block(_, _) => {
            let rendered = inline_expr_to_string(expr);
            if rendered != "none" {
                Some(rendered)
            } else {
                None
            }
        }
        _ => Some(inline_expr_to_string(expr)),
    }
}

fn is_selfhost_empty_block(block: &Block) -> bool {
    matches!(
        block.stmts.as_slice(),
        [Stmt::Let {
            pattern: Pattern::Binding { name, .. },
            value: Some(Expr::None(_)),
            ..
        }] if name == "__selfhost_empty"
    )
}

fn block_inline_value_to_string(block: &Block) -> Option<String> {
    if block.stmts.is_empty() || is_selfhost_empty_block(block) {
        return Some("()".to_string());
    }
    if block.stmts.len() != 1 {
        return None;
    }
    match &block.stmts[0] {
        Stmt::Expr(expr) => Some(inline_expr_to_string(expr)),
        Stmt::Return(Some(expr), _) => Some(inline_expr_to_string(expr)),
        Stmt::Return(None, _) => Some("return".to_string()),
        _ => None,
    }
}

fn else_branch_inline_value_to_string(branch: &ElseBranch) -> Option<String> {
    match branch {
        ElseBranch::Else(block) => block_inline_value_to_string(block),
        ElseBranch::ElseIf(_, _, _) => None,
    }
}

fn render_aggregate_init(
    type_name: String,
    zero_fill_rest: bool,
    fields: &[(String, Expr)],
) -> String {
    if let Some((enum_name, variant)) = type_name.rsplit_once("__") {
        let rendered_fields = fields
            .iter()
            .map(|(field, value)| {
                format!(
                    "{}: {}",
                    sanitize_identifier(field),
                    inline_expr_to_string(value)
                )
            })
            .collect::<Vec<_>>()
            .join(", ");
        if rendered_fields.is_empty() {
            return format!(
                "{}__{}",
                sanitize_type_name(enum_name),
                sanitize_variant_name(Some(enum_name), variant)
            );
        }
        return format!(
            "{}::{} {{ {} }}",
            sanitize_type_path(enum_name),
            sanitize_variant_name(Some(enum_name), variant),
            rendered_fields
        );
    }
    let mut args = vec![format!("{:?}", type_name)];
    args.extend(fields.iter().map(|(field, value)| {
        format!(
            "{} = {}",
            sanitize_identifier(field),
            inline_expr_to_string(value)
        )
    }));
    if !zero_fill_rest {
        args.push("false".to_string());
    }
    format!("aggregate_init({})", args.join(", "))
}

fn rendered_function_name(function: &kain_core::ast::Function) -> String {
    function.name.clone()
}

fn render_range_expr(start: Option<&Expr>, end: Option<&Expr>, inclusive: bool) -> String {
    let start = start
        .map(inline_expr_to_string)
        .unwrap_or_else(|| "0".to_string());
    let end = end
        .map(inline_expr_to_string)
        .unwrap_or_else(|| "0".to_string());
    if inclusive {
        format!("range({}, ({} + 1))", start, end)
    } else {
        format!("range({}, {})", start, end)
    }
}

fn expand_or_patterns(pattern: &Pattern) -> Vec<Pattern> {
    match pattern {
        Pattern::Or(values, _) => values.iter().flat_map(expand_or_patterns).collect(),
        Pattern::Tuple(values, span) => expand_pattern_lists(values)
            .into_iter()
            .map(|values| Pattern::Tuple(values, *span))
            .collect(),
        Pattern::Variant {
            enum_name,
            variant,
            fields: VariantPatternFields::Tuple(values),
            span,
        } => expand_pattern_lists(values)
            .into_iter()
            .map(|values| Pattern::Variant {
                enum_name: enum_name.clone(),
                variant: variant.clone(),
                fields: VariantPatternFields::Tuple(values),
                span: *span,
            })
            .collect(),
        Pattern::Variant {
            enum_name,
            variant,
            fields: VariantPatternFields::Struct(fields),
            span,
        } => expand_named_pattern_lists(fields)
            .into_iter()
            .map(|fields| Pattern::Variant {
                enum_name: enum_name.clone(),
                variant: variant.clone(),
                fields: VariantPatternFields::Struct(fields),
                span: *span,
            })
            .collect(),
        Pattern::Struct {
            name,
            fields,
            rest,
            span,
        } => expand_named_pattern_lists(fields)
            .into_iter()
            .map(|fields| Pattern::Struct {
                name: name.clone(),
                fields,
                rest: *rest,
                span: *span,
            })
            .collect(),
        Pattern::Slice {
            patterns,
            rest,
            span,
        } => expand_pattern_lists(patterns)
            .into_iter()
            .map(|patterns| Pattern::Slice {
                patterns,
                rest: rest.clone(),
                span: *span,
            })
            .collect(),
        _ => vec![pattern.clone()],
    }
}

fn expand_pattern_lists(patterns: &[Pattern]) -> Vec<Vec<Pattern>> {
    let mut combinations = vec![Vec::new()];
    for pattern in patterns {
        let expanded = expand_or_patterns(pattern);
        let mut next = Vec::new();
        for prefix in &combinations {
            for pattern in &expanded {
                let mut combined = prefix.clone();
                combined.push(pattern.clone());
                next.push(combined);
            }
        }
        combinations = next;
    }
    combinations
}

fn expand_named_pattern_lists(fields: &[(String, Pattern)]) -> Vec<Vec<(String, Pattern)>> {
    let mut combinations = vec![Vec::new()];
    for (name, pattern) in fields {
        let expanded = expand_or_patterns(pattern);
        let mut next = Vec::new();
        for prefix in &combinations {
            for pattern in &expanded {
                let mut combined = prefix.clone();
                combined.push((name.clone(), pattern.clone()));
                next.push(combined);
            }
        }
        combinations = next;
    }
    combinations
}

fn for_binding_name(pattern: &Pattern) -> String {
    match pattern {
        Pattern::Binding { name, .. } => sanitize_identifier(name),
        Pattern::Wildcard(_) => "_".to_string(),
        _ => "selfhost_for_item".to_string(),
    }
}

fn format_fstring(parts: &[Expr]) -> String {
    if !parts.iter().all(is_safe_fstring_part) {
        return render_lossy_interpolated_string(parts);
    }

    let mut rendered = String::from("f\"");
    for part in parts {
        match part {
            Expr::String(value, _) => push_string_literal_fragment(&mut rendered, value),
            expr => {
                rendered.push('{');
                rendered.push_str(&inline_expr_to_string(expr));
                rendered.push('}');
            }
        }
    }
    rendered.push('"');
    rendered
}

fn is_safe_fstring_part(part: &Expr) -> bool {
    match part {
        Expr::String(value, _) => !value.contains('{') && !value.contains('}'),
        expr => {
            let rendered = inline_expr_to_string(expr);
            !rendered.contains('"')
                && !rendered.contains('\\')
                && !rendered.contains('{')
                && !rendered.contains('}')
                && !rendered.contains('\n')
                && !rendered.contains('\r')
        }
    }
}

fn render_lossy_interpolated_string(parts: &[Expr]) -> String {
    let mut rendered = String::new();
    for part in parts {
        match part {
            Expr::String(value, _) => rendered.push_str(value),
            expr => {
                rendered.push('{');
                rendered.push_str(&inline_expr_to_string(expr));
                rendered.push('}');
            }
        }
    }
    format!("{rendered:?}")
}

fn push_string_literal_fragment(output: &mut String, value: &str) {
    for ch in value.chars() {
        match ch {
            '\\' => output.push_str("\\\\"),
            '"' => output.push_str("\\\""),
            '\n' => output.push_str("\\n"),
            '\r' => output.push_str("\\r"),
            '\t' => output.push_str("\\t"),
            other => output.push(other),
        }
    }
}

fn binary_op_to_string(op: kain_core::ast::BinaryOp) -> &'static str {
    use kain_core::ast::BinaryOp::*;
    match op {
        Add => "+",
        Sub => "-",
        Mul => "*",
        Div => "/",
        Mod => "%",
        Pow => "**",
        Eq => "==",
        Ne => "!=",
        Lt => "<",
        Gt => ">",
        Le => "<=",
        Ge => ">=",
        And => "&&",
        Or => "||",
        BitAnd => "&",
        BitOr => "|",
        BitXor => "^",
        Shl => "<<",
        Shr => ">>",
        Assign => "=",
        AddAssign => "+=",
        SubAssign => "-=",
        MulAssign => "*=",
        DivAssign => "/=",
        Range => "..",
        RangeInclusive => "..=",
    }
}

fn unary_op_to_string(op: kain_core::ast::UnaryOp) -> &'static str {
    use kain_core::ast::UnaryOp::*;
    match op {
        Neg => "-",
        Not => "!",
        BitNot => "~",
        Ref => "&",
        RefMut => "&mut ",
        Deref => "*",
    }
}

fn classify_diagnostic(diagnostic: &str) -> String {
    if diagnostic.contains("SELFHOST_STRICT") {
        return "strict_selfhost".to_string();
    }
    if diagnostic.contains("required direct-lower macros preserved") {
        return "required_macro_preserved".to_string();
    }
    if diagnostic.contains("rejected") {
        return "rejected_macro".to_string();
    }
    if diagnostic.contains("Unsupported language feature") {
        return "unsupported_feature".to_string();
    }
    "other".to_string()
}

fn expand_import_diagnostics(message: &str) -> Vec<String> {
    let mut diagnostics = Vec::new();
    for line in message.lines() {
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }
        if trimmed.starts_with("SELFHOST_STRICT:") {
            diagnostics.push(trimmed.to_string());
            continue;
        }
        if trimmed.starts_with("Unsupported language feature:")
            || trimmed.starts_with("self-host import rejected")
        {
            diagnostics.push(trimmed.to_string());
        }
    }
    if diagnostics.is_empty() {
        diagnostics.push(message.to_string());
    }
    diagnostics
}

fn find_repo_root(start: &Path) -> KainResult<PathBuf> {
    for dir in start.ancestors() {
        if dir.join("Cargo.toml").exists() && dir.join("crates").exists() {
            return Ok(dir.to_path_buf());
        }
    }
    Err(KainError::runtime(
        "Failed to find KAIN repo root from current directory",
    ))
}

fn default_inventory_dir(repo_root: &Path) -> PathBuf {
    let repo_local = repo_root
        .join("ouroboros")
        .join("docs")
        .join("selfhost")
        .join("inventories");
    if repo_local.exists() {
        return repo_local;
    }

    repo_root
        .parent()
        .map(|parent| {
            parent
                .join("OuroborosV2")
                .join("docs")
                .join("selfhost")
                .join("inventories")
        })
        .unwrap_or_else(|| {
            PathBuf::from("OuroborosV2")
                .join("docs")
                .join("selfhost")
                .join("inventories")
        })
}

fn default_output_dir(repo_root: &Path) -> PathBuf {
    let repo_local = repo_root.join("ouroboros").join("out").join("selfhost");
    if repo_root.join("ouroboros").exists() {
        return repo_local;
    }

    repo_root
        .parent()
        .map(|parent| parent.join("OuroborosV2").join("out").join("selfhost"))
        .unwrap_or_else(|| PathBuf::from("OuroborosV2").join("out").join("selfhost"))
}

fn default_output_dir_for_phase(repo_root: &Path, phase_name: &str) -> PathBuf {
    let base = default_output_dir(repo_root);
    if phase_name == "phase1" {
        base
    } else {
        base.join(phase_name)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn repair_named_function_block_repairs_collapse_and_preserves_following_blocks() {
        let source = r#"fn lower_type_memory(ty: &Type):
    match ty:
    Type::Named { name: name, generics: generics } => Type::Named { name: name, generics: generics.into_iter().collect(), span: span.clone() }

fn untouched_afterwards():
    return none
"#;

        let repaired = repair_named_function_block(source, "fn lower_type_memory(", |_| {
            [
                "fn lower_type_memory(ty: &Type):",
                "    match ty:",
                "        Type::Named { name: name, generics: generics } => Type::Named { name: name, generics: generics.into_iter().collect(), span: span.clone() }",
            ]
            .join("\n")
        });

        assert!(repaired.contains("    match ty:\n        Type::Named"));
        assert!(repaired.contains("\n\nfn untouched_afterwards():"));
    }

    #[test]
    fn indent_repaired_block_matches_nested_selfhost_layout() {
        let block = [
            "fn repair_self(node: Self):",
            "    match node:",
            "        Self::Block { body: body } => body",
            "        Self::Path { path: path } => path",
        ]
        .join("\n");

        let indented = indent_repaired_block(&block, "    ");
        assert!(indented.starts_with("    fn repair_self(node: Self):"));
        assert!(indented.contains("\n        match node:"));
        assert!(indented.contains("\n            Self::Block"));
    }

    #[test]
    fn sanitize_identifier_pushes_reserved_and_contextual_ids_out_of_the_way() {
        assert_eq!(sanitize_identifier("self"), "self_");
        assert_eq!(sanitize_identifier("Self"), "Self_");
        assert_eq!(sanitize_identifier("state"), "state_");
        assert_eq!(sanitize_identifier("shader"), "shader_");
        assert_eq!(sanitize_identifier("3d-path"), "_3d_path");
    }

    #[test]
    fn sanitize_path_to_ident_covers_parser_hostile_path_forms() {
        assert_eq!(
            sanitize_path_to_ident("crate::repair::Self"),
            "crate__repair__Self_"
        );
        assert_eq!(
            sanitize_path_to_ident("std::collections::HashMap"),
            "std__collections__HashMap"
        );
        assert_eq!(sanitize_expr_path("foo::bar::baz"), "foo::bar::baz");
        assert_eq!(
            sanitize_expr_path("crate::repair::Self"),
            "crate::repair::Self_"
        );
        assert_eq!(sanitize_expr_path("Vec::new"), "Vec__new_");
        assert_eq!(sanitize_expr_path("Vec::new_"), "Vec__new_");
        assert_eq!(sanitize_expr_path("Env::new_"), "Env__new_");
        assert_eq!(
            sanitize_expr_path("span::Span::default_"),
            "span__Span__default_"
        );
        assert_eq!(
            sanitize_expr_path("std::collections::HashSet::new"),
            "std__collections__HashSet__new_"
        );
        assert_eq!(
            sanitize_expr_path("std::collections::HashSet::new_"),
            "std__collections__HashSet__new_"
        );
    }

    #[test]
    fn inline_expr_parenthesizes_deref_field_bases() {
        let span = kain_core::span::Span::default();
        let expr = Expr::Ref {
            mutable: false,
            value: Box::new(Expr::Field {
                object: Box::new(Expr::Deref(
                    Box::new(Expr::Ident("_self".to_string(), span)),
                    span,
                )),
                field: "body".to_string(),
                span,
            }),
            span,
        };

        assert_eq!(inline_expr_to_string(&expr), "&(*_self).body");
    }

    #[test]
    fn render_program_keeps_default_impl_blocks() {
        let span = kain_core::span::Span::default();
        let target_type = Type::Named {
            name: "LanguageCapabilities".to_string(),
            generics: Vec::new(),
            span,
        };
        let program = Program {
            items: vec![Item::Impl(kain_core::ast::Impl {
                generics: Vec::new(),
                trait_name: Some("Default".to_string()),
                trait_generics: Vec::new(),
                target_type: target_type.clone(),
                methods: vec![kain_core::ast::Function {
                    name: "default_".to_string(),
                    generics: Vec::new(),
                    params: Vec::new(),
                    return_type: Some(target_type),
                    effects: Vec::new(),
                    body: Block {
                        stmts: vec![Stmt::Expr(Expr::None(span))],
                        span,
                    },
                    visibility: Visibility::Public,
                    attributes: Vec::new(),
                    span,
                }],
                span,
            })],
            span,
        };

        let rendered = render_program(&program).expect("default impl should render");
        assert!(rendered.contains("impl Default for LanguageCapabilities:"));
        assert!(rendered.contains("pub fn default_() -> LanguageCapabilities:"));
    }

    #[test]
    fn render_program_bootstraps_lexer_tokenize_body() {
        let span = kain_core::span::Span::default();
        let program = Program {
            items: vec![Item::Impl(kain_core::ast::Impl {
                generics: Vec::new(),
                trait_name: None,
                trait_generics: Vec::new(),
                target_type: Type::Named {
                    name: "Lexer".to_string(),
                    generics: Vec::new(),
                    span,
                },
                methods: vec![kain_core::ast::Function {
                    name: "tokenize".to_string(),
                    generics: Vec::new(),
                    params: vec![Param {
                        name: "_self".to_string(),
                        ty: Type::Ref {
                            mutable: false,
                            inner: Box::new(Type::Named {
                                name: "Lexer".to_string(),
                                generics: Vec::new(),
                                span,
                            }),
                            lifetime: None,
                            span,
                        },
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: Some(Type::Result(
                        Box::new(Type::Named {
                            name: "Array".to_string(),
                            generics: vec![Type::Named {
                                name: "Token".to_string(),
                                generics: Vec::new(),
                                span,
                            }],
                            span,
                        }),
                        Box::new(Type::Named {
                            name: "KainError".to_string(),
                            generics: Vec::new(),
                            span,
                        }),
                        span,
                    )),
                    effects: Vec::new(),
                    body: Block {
                        stmts: vec![Stmt::Expr(Expr::None(span))],
                        span,
                    },
                    visibility: Visibility::Public,
                    attributes: Vec::new(),
                    span,
                }],
                span,
            })],
            span,
        };

        let rendered = render_program(&program).expect("lexer tokenize should render");
        assert!(rendered.contains("__kain_bootstrap_lex_tokens(&(*_self).source)"));
    }

    #[test]
    fn build_file_mirror_plans_preserves_file_level_paths() {
        let span = kain_core::span::Span::default();
        let crate_root = PathBuf::from("/tmp/kain/crates/kain-import");
        let module_programs = vec![
            RustSelfHostModuleProgram {
                module: kain_import::rust::RustModuleNode {
                    module_name: "crate".to_string(),
                    file_path: crate_root.join("src/lib.rs"),
                },
                program: Program {
                    items: Vec::new(),
                    span,
                },
            },
            RustSelfHostModuleProgram {
                module: kain_import::rust::RustModuleNode {
                    module_name: "rust".to_string(),
                    file_path: crate_root.join("src/rust/mod.rs"),
                },
                program: Program {
                    items: Vec::new(),
                    span,
                },
            },
            RustSelfHostModuleProgram {
                module: kain_import::rust::RustModuleNode {
                    module_name: "rust::transformer".to_string(),
                    file_path: crate_root.join("src/rust/transformer.rs"),
                },
                program: Program {
                    items: Vec::new(),
                    span,
                },
            },
        ];

        let plans = build_file_mirror_plans(
            &crate_root,
            "kain-import",
            &module_programs,
            Path::new("/repo/src"),
            Path::new("/out/mirror/src"),
            Path::new("/out/roundtrip_rust"),
        )
        .expect("mirror plans should build");

        assert_eq!(plans.len(), 3);
        assert_eq!(
            plans[0].canonical_kain_path,
            PathBuf::from("/repo/src/kain-import/lib.kn")
        );
        assert_eq!(
            plans[1].canonical_kain_path,
            PathBuf::from("/repo/src/kain-import/rust/mod.kn")
        );
        assert_eq!(
            plans[2].canonical_kain_path,
            PathBuf::from("/repo/src/kain-import/rust/transformer.kn")
        );
        assert_eq!(plans[0].module_path, Vec::<String>::new());
        assert_eq!(plans[1].module_path, vec!["rust".to_string()]);
        assert_eq!(
            plans[2].module_path,
            vec!["rust".to_string(), "transformer".to_string()]
        );
        assert_eq!(plans[0].source_kind, SelfHostRustSourceKind::LibRoot);
        assert_eq!(
            plans[1].source_kind,
            SelfHostRustSourceKind::ModuleDirectoryRoot
        );
        assert_eq!(plans[2].source_kind, SelfHostRustSourceKind::ModuleFile);
    }

    #[test]
    fn write_roundtrip_rust_tree_splits_inline_modules_into_real_files() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let roundtrip_root = temp_dir.path().join("roundtrip_rust");
        let aggregate_path = temp_dir.path().join("kain-core.roundtrip.rs");
        let rust_source = r#"
pub struct Root;

pub mod parser {
    pub fn parse() {}
}

pub mod nested {
    pub struct Node;

    pub mod deep {
        pub fn helper() {}
    }
}

fn main() {}
"#;

        let file_plans = vec![
            SelfHostFileMirrorPlan {
                module_name: "crate".to_string(),
                rust_source_path: PathBuf::from("/repo/crates/kain-core/src/lib.rs"),
                rust_source_relative_path: PathBuf::from("src/lib.rs"),
                canonical_kain_path: temp_dir.path().join("src/kain-core/lib.kn"),
                output_kain_path: temp_dir.path().join("mirror/src/kain-core/lib.kn"),
                stage2_roundtrip_rust_path: roundtrip_root.join("kain-core/src/lib.rs"),
                module_path: Vec::new(),
                source_kind: SelfHostRustSourceKind::LibRoot,
            },
            SelfHostFileMirrorPlan {
                module_name: "main".to_string(),
                rust_source_path: PathBuf::from("/repo/crates/kain-core/src/main.rs"),
                rust_source_relative_path: PathBuf::from("src/main.rs"),
                canonical_kain_path: temp_dir.path().join("src/kain-core/main.kn"),
                output_kain_path: temp_dir.path().join("mirror/src/kain-core/main.kn"),
                stage2_roundtrip_rust_path: roundtrip_root.join("kain-core/src/main.rs"),
                module_path: Vec::new(),
                source_kind: SelfHostRustSourceKind::MainRoot,
            },
            SelfHostFileMirrorPlan {
                module_name: "parser".to_string(),
                rust_source_path: PathBuf::from("/repo/crates/kain-core/src/parser.rs"),
                rust_source_relative_path: PathBuf::from("src/parser.rs"),
                canonical_kain_path: temp_dir.path().join("src/kain-core/parser.kn"),
                output_kain_path: temp_dir.path().join("mirror/src/kain-core/parser.kn"),
                stage2_roundtrip_rust_path: roundtrip_root.join("kain-core/src/parser.rs"),
                module_path: vec!["parser".to_string()],
                source_kind: SelfHostRustSourceKind::ModuleFile,
            },
            SelfHostFileMirrorPlan {
                module_name: "nested".to_string(),
                rust_source_path: PathBuf::from("/repo/crates/kain-core/src/nested/mod.rs"),
                rust_source_relative_path: PathBuf::from("src/nested/mod.rs"),
                canonical_kain_path: temp_dir.path().join("src/kain-core/nested/mod.kn"),
                output_kain_path: temp_dir.path().join("mirror/src/kain-core/nested/mod.kn"),
                stage2_roundtrip_rust_path: roundtrip_root.join("kain-core/src/nested/mod.rs"),
                module_path: vec!["nested".to_string()],
                source_kind: SelfHostRustSourceKind::ModuleDirectoryRoot,
            },
            SelfHostFileMirrorPlan {
                module_name: "nested::deep".to_string(),
                rust_source_path: PathBuf::from("/repo/crates/kain-core/src/nested/deep.rs"),
                rust_source_relative_path: PathBuf::from("src/nested/deep.rs"),
                canonical_kain_path: temp_dir.path().join("src/kain-core/nested/deep.kn"),
                output_kain_path: temp_dir.path().join("mirror/src/kain-core/nested/deep.kn"),
                stage2_roundtrip_rust_path: roundtrip_root.join("kain-core/src/nested/deep.rs"),
                module_path: vec!["nested".to_string(), "deep".to_string()],
                source_kind: SelfHostRustSourceKind::ModuleFile,
            },
        ];

        let artifacts = write_roundtrip_rust_tree(
            "kain-core",
            Path::new("/repo/crates/kain-core"),
            &roundtrip_root,
            &aggregate_path,
            rust_source,
            &file_plans,
            &SelfHostSourceProfile::default(),
        )
        .expect("roundtrip tree should split");

        assert_eq!(artifacts.file_count, 5);
        let lib_source =
            fs::read_to_string(roundtrip_root.join("kain-core/src/lib.rs")).expect("lib.rs");
        let parser_source =
            fs::read_to_string(roundtrip_root.join("kain-core/src/parser.rs")).expect("parser.rs");
        let nested_source = fs::read_to_string(roundtrip_root.join("kain-core/src/nested/mod.rs"))
            .expect("nested/mod.rs");
        let deep_source = fs::read_to_string(roundtrip_root.join("kain-core/src/nested/deep.rs"))
            .expect("nested/deep.rs");
        let main_source =
            fs::read_to_string(roundtrip_root.join("kain-core/src/main.rs")).expect("main.rs");

        assert!(lib_source.contains("pub struct Root;"));
        assert!(lib_source.contains("pub mod parser;"));
        assert!(lib_source.contains("pub mod nested;"));
        assert!(lib_source.contains("fn main() {}"));
        assert!(parser_source.contains("pub fn parse() {}"));
        assert!(nested_source.contains("pub struct Node;"));
        assert!(nested_source.contains("pub mod deep;"));
        assert!(deep_source.contains("pub fn helper() {}"));
        assert_eq!(main_source, "include!(\"lib.rs\");\n");
        assert_eq!(
            artifacts.main_rs_path,
            Some(roundtrip_root.join("kain-core/src/main.rs"))
        );
    }
}
