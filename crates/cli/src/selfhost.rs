use crate::error::{KainError, KainResult};
use crate::selfhost_report::{
    render_phase_markdown, CratePhase1Result, MacroFinding, SelfHostPhase1Report,
    SelfHostPhaseStatus, TraitDynSummary,
};
use chrono::Utc;
use clap::Subcommand;
use kain_core::ast::{Block, CallArg, ElseBranch, EnumVariantFields, Expr, Item, MatchArm, Pattern, Program, Stmt, Type, VariantPatternFields};
use kain_core::parser::RESERVED_KEYWORDS;
use kain_import::rust::RustSelfHostOptions;
use serde::Deserialize;
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use toml::Value;

const SELFHOST_CONTEXTUAL_KEYWORDS: &[&str] = &["state", "weak", "compute", "shader"];
const SELFHOST_STAGE2_VERSION_SUFFIX: &str = "-selfhost.0";

#[derive(Subcommand, Debug)]
pub enum SelfHostCommand {
    Phase1 {
        #[arg(long)]
        inventory_dir: Option<PathBuf>,

        #[arg(long)]
        output_dir: Option<PathBuf>,

        #[arg(long, default_value_t = true)]
        emit_bundles: bool,
    },
    Phase2 {
        #[arg(long)]
        inventory_dir: Option<PathBuf>,

        #[arg(long)]
        output_dir: Option<PathBuf>,

        #[arg(long, default_value_t = true)]
        emit_bundles: bool,

        #[arg(long, default_value_t = true)]
        emit_roundtrip_rust: bool,

        #[arg(long, default_value_t = true)]
        assemble_stage2: bool,

        #[arg(long, default_value_t = true)]
        build_stage2: bool,
    },
}

pub fn run(command: SelfHostCommand) -> KainResult<()> {
    match command {
        SelfHostCommand::Phase1 {
            inventory_dir,
            output_dir,
            emit_bundles,
        } => run_phase1(inventory_dir, output_dir, emit_bundles),
        SelfHostCommand::Phase2 {
            inventory_dir,
            output_dir,
            emit_bundles,
            emit_roundtrip_rust,
            assemble_stage2,
            build_stage2,
        } => run_phase2(
            inventory_dir,
            output_dir,
            emit_bundles,
            emit_roundtrip_rust,
            assemble_stage2,
            build_stage2,
        ),
    }
}

fn run_phase1(
    inventory_dir: Option<PathBuf>,
    output_dir: Option<PathBuf>,
    emit_bundles: bool,
) -> KainResult<()> {
    run_phase(
        "phase1",
        inventory_dir,
        output_dir,
        emit_bundles,
        false,
        false,
        false,
    )
}

fn run_phase2(
    inventory_dir: Option<PathBuf>,
    output_dir: Option<PathBuf>,
    emit_bundles: bool,
    emit_roundtrip_rust: bool,
    assemble_stage2: bool,
    build_stage2: bool,
) -> KainResult<()> {
    run_phase(
        "phase2",
        inventory_dir,
        output_dir,
        emit_bundles,
        emit_roundtrip_rust,
        assemble_stage2,
        build_stage2,
    )
}

fn run_phase(
    phase_name: &str,
    inventory_dir: Option<PathBuf>,
    output_dir: Option<PathBuf>,
    emit_bundles: bool,
    emit_roundtrip_rust: bool,
    assemble_stage2: bool,
    build_stage2: bool,
) -> KainResult<()> {
    let repo_root = find_repo_root(&std::env::current_dir().map_err(KainError::Io)?)?;
    let inventory_dir = inventory_dir.unwrap_or_else(|| default_inventory_dir(&repo_root));
    let output_dir = output_dir.unwrap_or_else(|| default_output_dir_for_phase(&repo_root, phase_name));
    let inventories = load_inventories(&inventory_dir)?;
    let mut options = RustSelfHostOptions::from_inventory_dir(&inventory_dir)
        .map_err(|err| KainError::runtime(format!("Failed to load strict self-host options: {err}")))?;
    options.include_tests = false;

    fs::create_dir_all(&output_dir).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create self-host output directory {}: {}",
            output_dir.display(),
            err
        ))
    })?;

    let crates_processed = match phase_name {
        "phase2" if !inventories.module_map.phase2_slice.is_empty() => inventories.module_map.phase2_slice.clone(),
        _ => inventories.module_map.initial_slice.clone(),
    };
    let mut modules_discovered: BTreeMap<String, Vec<String>> = BTreeMap::new();
    let mut crate_results = Vec::new();
    let mut all_rejected = Vec::new();
    let mut all_required_preserved = Vec::new();
    let mut roundtrip_rust_outputs = BTreeMap::<String, PathBuf>::new();

    for crate_name in &crates_processed {
        let crate_root = repo_root.join("crates").join(crate_name);
        if !crate_root.exists() {
            return Err(KainError::runtime(format!(
                "Initial self-host slice crate not found: {}",
                crate_root.display()
            )));
        }

        let rejected_macros_found = macro_findings_for(
            crate_name,
            &inventories.macro_inventory,
            &inventories.allowlist.macro_policy.reject,
        );
        let import_result = kain_import::import_rust_selfhost_dir_detailed(&crate_root, &options);

        let mut diagnostics = Vec::new();
        let mut import_success = true;
        let mut import_error = None;
        let mut output_kn_path = None;
        let mut item_count = 0usize;
        let mut required_direct_lowering_still_preserved = Vec::new();
        let mut discovered_modules = Vec::new();

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
                            .map(|finding| format!("{}({})", finding.macro_name, finding.occurrence_count))
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
                if emit_bundles {
                    let bundle_path = output_dir.join(format!("{}.kn", crate_name));
                    let rendered = render_program(&program)?;
                    fs::write(&bundle_path, &rendered).map_err(|err| {
                        KainError::runtime(format!(
                            "Failed to write self-host bundle {}: {}",
                            bundle_path.display(),
                            err
                        ))
                    })?;
                    output_kn_path = Some(bundle_path.display().to_string());
                    if emit_roundtrip_rust {
                        let roundtrip_path = output_dir.join(format!("{}.roundtrip.rs", crate_name));
                        let rust_source = compile_kn_source_to_rust(&rendered)?;
                        fs::write(&roundtrip_path, rust_source).map_err(|err| {
                            KainError::runtime(format!(
                                "Failed to write self-host roundtrip Rust {}: {}",
                                roundtrip_path.display(),
                                err
                            ))
                        })?;
                        roundtrip_rust_outputs.insert(crate_name.clone(), roundtrip_path);
                    }
                }
            }
            Err(err) => {
                import_success = false;
                let message = format!("{err}");
                diagnostics.extend(expand_import_diagnostics(&message));
                import_error = Some(message);
                modules_discovered.insert(crate_name.clone(), discovered_modules.clone());
            }
        }

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
            item_count,
            rejected_macros_found,
            required_direct_lowering_still_preserved,
        });
    }

    let diagnostics_by_category = build_diagnostic_category_summary(&crate_results);
    let trait_dyn_summary = build_trait_dyn_summary(&inventories.trait_inventory, &crates_processed);
    let mut final_phase_status = determine_phase_status(&crate_results, &all_required_preserved);
    let mut stage2_workspace_path = None;
    let mut stage2_build_artifact = None;
    let mut stage2_build_success = None;

    if assemble_stage2 {
        let stage2_workspace = output_dir.join("stage2_workspace");
        assemble_stage2_workspace(&repo_root, &stage2_workspace, &crates_processed, &roundtrip_rust_outputs)?;
        stage2_workspace_path = Some(stage2_workspace.display().to_string());

        if build_stage2 {
            let build_result = build_stage2_workspace(&stage2_workspace)?;
            stage2_build_success = Some(build_result.success);
            stage2_build_artifact = build_result.artifact_path.map(|path| path.display().to_string());
            if !build_result.success {
                final_phase_status = SelfHostPhaseStatus::HardFail;
            }
        }
    }

    let report = SelfHostPhase1Report {
        generated_at_utc: Utc::now().to_rfc3339(),
        repo_root: repo_root.display().to_string(),
        inventory_dir: inventory_dir.display().to_string(),
        output_dir: output_dir.display().to_string(),
        crates_processed,
        modules_discovered,
        diagnostics_by_category,
        rejected_macros_found: all_rejected,
        required_direct_lowering_still_preserved: all_required_preserved,
        trait_dyn_summary,
        crate_results,
        stage2_workspace_path,
        stage2_build_artifact,
        stage2_build_success,
        final_phase_status: final_phase_status.clone(),
    };

    write_report_files(phase_name, &output_dir, &report)?;
    print_summary(phase_name, &report);

    match final_phase_status {
        SelfHostPhaseStatus::Pass => Ok(()),
        SelfHostPhaseStatus::SoftFail => Err(KainError::runtime(format!("Self-host {phase_name} completed with soft failures"))),
        SelfHostPhaseStatus::HardFail => Err(KainError::runtime(format!("Self-host {phase_name} failed"))),
    }
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
        macro_inventory: read_inventory_json(inventory_dir.join("macro_inventory.json"))?,
        module_map: read_inventory_json(inventory_dir.join("module_map.json"))?,
        allowlist: read_inventory_json(inventory_dir.join("selfhost_allowlist.json"))?,
        trait_inventory: read_inventory_json(inventory_dir.join("trait_inventory.json"))?,
    })
}

fn read_inventory_json<T: for<'de> Deserialize<'de>>(path: PathBuf) -> KainResult<T> {
    let raw = fs::read_to_string(&path).map_err(|err| {
        KainError::runtime(format!("Failed to read inventory {}: {}", path.display(), err))
    })?;
    serde_json::from_str(&raw).map_err(|err| {
        KainError::runtime(format!("Failed to parse inventory {}: {}", path.display(), err))
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
        let count = crate_macros.bang_macros.get(macro_name).copied().unwrap_or(0);
        if count == 0 {
            continue;
        }
        let files = crate_macros.files.get(macro_name).cloned().unwrap_or_default();
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

fn collect_macro_calls_from_item(item: &Item, required: &BTreeSet<String>, counts: &mut BTreeMap<String, usize>) {
    match item {
        Item::Function(function) => collect_macro_calls_from_block(&function.body, required, counts),
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

fn collect_macro_calls_from_block(block: &Block, required: &BTreeSet<String>, counts: &mut BTreeMap<String, usize>) {
    for stmt in &block.stmts {
        collect_macro_calls_from_stmt(stmt, required, counts);
    }
}

fn collect_macro_calls_from_stmt(stmt: &Stmt, required: &BTreeSet<String>, counts: &mut BTreeMap<String, usize>) {
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
        Stmt::While { condition, body, .. } => {
            collect_macro_calls_from_expr(condition, required, counts);
            collect_macro_calls_from_block(body, required, counts);
        }
        Stmt::Loop { body, .. } => collect_macro_calls_from_block(body, required, counts),
        Stmt::Item(item) => collect_macro_calls_from_item(item, required, counts),
        Stmt::Continue(_) => {}
    }
}

fn collect_macro_calls_from_expr(expr: &Expr, required: &BTreeSet<String>, counts: &mut BTreeMap<String, usize>) {
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
        | Expr::Comptime(operand, _)
        | Expr::Paren(operand, _) => collect_macro_calls_from_expr(operand, required, counts),
        Expr::Call { callee, args, .. } => {
            collect_macro_calls_from_expr(callee, required, counts);
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
        Expr::Match { scrutinee, arms, .. } => {
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
        Expr::PtrOffset { pointer, offset, element_ty, .. } => {
            collect_macro_calls_from_expr(pointer, required, counts);
            collect_macro_calls_from_expr(offset, required, counts);
            if let Some(element_ty) = element_ty {
                collect_macro_calls_from_type(element_ty, required, counts);
            }
        }
        Expr::MemLoad { pointer, load_ty, .. } => {
            collect_macro_calls_from_expr(pointer, required, counts);
            if let Some(load_ty) = load_ty {
                collect_macro_calls_from_type(load_ty, required, counts);
            }
        }
        Expr::MemStore { pointer, value, store_ty, .. } => {
            collect_macro_calls_from_expr(pointer, required, counts);
            collect_macro_calls_from_expr(value, required, counts);
            if let Some(store_ty) = store_ty {
                collect_macro_calls_from_type(store_ty, required, counts);
            }
        }
        Expr::SizeOfType { target, .. }
        | Expr::AlignOfType { target, .. }
        | Expr::Alloca { ty: target, .. }
        | Expr::Uninit { ty: target, .. } => collect_macro_calls_from_type(target, required, counts),
        Expr::Alloc { size, ty, .. } => {
            collect_macro_calls_from_expr(size, required, counts);
            if let Some(ty) = ty {
                collect_macro_calls_from_type(ty, required, counts);
            }
        }
        Expr::Realloc { pointer, size, ty, .. } => {
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

fn collect_macro_calls_from_match_arm(arm: &MatchArm, required: &BTreeSet<String>, counts: &mut BTreeMap<String, usize>) {
    collect_macro_calls_from_pattern(&arm.pattern, required, counts);
    if let Some(guard) = &arm.guard {
        collect_macro_calls_from_expr(guard, required, counts);
    }
    collect_macro_calls_from_expr(&arm.body, required, counts);
}

fn collect_macro_calls_from_pattern(pattern: &Pattern, required: &BTreeSet<String>, counts: &mut BTreeMap<String, usize>) {
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

fn collect_macro_calls_from_type(ty: &Type, required: &BTreeSet<String>, counts: &mut BTreeMap<String, usize>) {
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

fn build_diagnostic_category_summary(crate_results: &[CratePhase1Result]) -> BTreeMap<String, usize> {
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
    if crate_results.iter().any(|crate_result| !crate_result.import_success)
        || !required_preserved.is_empty()
    {
        return SelfHostPhaseStatus::HardFail;
    }

    if crate_results.iter().any(|crate_result| !crate_result.rejected_macros_found.is_empty()) {
        return SelfHostPhaseStatus::SoftFail;
    }

    SelfHostPhaseStatus::Pass
}

struct Stage2BuildResult {
    success: bool,
    artifact_path: Option<PathBuf>,
}

fn write_report_files(phase_name: &str, output_dir: &Path, report: &SelfHostPhase1Report) -> KainResult<()> {
    let json_path = output_dir.join(format!("{phase_name}_report.json"));
    let markdown_path = output_dir.join(format!("{phase_name}_report.md"));
    let json = serde_json::to_string_pretty(report)
        .map_err(|err| KainError::runtime(format!("Failed to serialize {phase_name} report: {}", err)))?;
    fs::write(&json_path, json).map_err(|err| {
        KainError::runtime(format!("Failed to write {phase_name} report JSON {}: {}", json_path.display(), err))
    })?;
    fs::write(&markdown_path, render_phase_markdown(&format!("Self-Host {}", phase_name.to_uppercase()), report)).map_err(|err| {
        KainError::runtime(format!("Failed to write {phase_name} report Markdown {}: {}", markdown_path.display(), err))
    })?;
    Ok(())
}

fn print_summary(phase_name: &str, report: &SelfHostPhase1Report) {
    println!("🧬 Self-host {}", phase_name);
    println!("   Crates: {}", report.crates_processed.join(", "));
    println!(
        "   Status: {}",
        match report.final_phase_status {
            SelfHostPhaseStatus::Pass => "pass",
            SelfHostPhaseStatus::SoftFail => "soft_fail",
            SelfHostPhaseStatus::HardFail => "hard_fail",
        }
    );
    if let Some(path) = &report.stage2_workspace_path {
        println!("   Stage2 workspace: {}", path);
    }
    if let Some(path) = &report.stage2_build_artifact {
        println!("   Stage2 artifact: {}", path);
    }
    if let Some(success) = report.stage2_build_success {
        println!("   Stage2 build: {}", if success { "pass" } else { "fail" });
    }
    println!("   Report JSON: {}", Path::new(&report.output_dir).join(format!("{phase_name}_report.json")).display());
    println!("   Report MD: {}", Path::new(&report.output_dir).join(format!("{phase_name}_report.md")).display());
}

fn compile_kn_source_to_rust(source: &str) -> KainResult<String> {
    let typed_program = crate::frontend_to_typed_program(source, crate::CompileTarget::Rust)?;
    #[cfg(feature = "sys")]
    {
        kain_sys_codegen::generate_rust(&typed_program)
            .map_err(|err| KainError::runtime(format!("Failed to generate Rust self-host roundtrip: {}", err)))
    }
    #[cfg(not(feature = "sys"))]
    {
        let _ = typed_program;
        Err(KainError::runtime("Rust self-host roundtrip requires cli sys feature"))
    }
}

fn assemble_stage2_workspace(
    repo_root: &Path,
    workspace_dir: &Path,
    crates_processed: &[String],
    roundtrip_rust_outputs: &BTreeMap<String, PathBuf>,
) -> KainResult<()> {
    if workspace_dir.exists() {
        fs::remove_dir_all(workspace_dir).map_err(|err| {
            KainError::runtime(format!("Failed to clear stage2 workspace {}: {}", workspace_dir.display(), err))
        })?;
    }
    fs::create_dir_all(workspace_dir.join("crates")).map_err(|err| {
        KainError::runtime(format!("Failed to create stage2 workspace {}: {}", workspace_dir.display(), err))
    })?;

    let root_manifest: Value = toml::from_str(
        &fs::read_to_string(repo_root.join("Cargo.toml")).map_err(|err| {
            KainError::runtime(format!("Failed to read workspace Cargo.toml: {}", err))
        })?,
    )
    .map_err(|err| KainError::runtime(format!("Failed to parse workspace Cargo.toml: {}", err)))?;

    let stage2_set = crates_processed.iter().cloned().collect::<BTreeSet<_>>();
    let root_toml = render_root_workspace_toml(
        crates_processed,
        &root_manifest,
        &repo_root,
        &stage2_set,
    )?;
    fs::write(workspace_dir.join("Cargo.toml"), root_toml).map_err(|err| {
        KainError::runtime(format!("Failed to write stage2 workspace Cargo.toml: {}", err))
    })?;

    for crate_name in crates_processed {
        let roundtrip_path = roundtrip_rust_outputs.get(crate_name).ok_or_else(|| {
            KainError::runtime(format!("Missing roundtrip Rust output for stage2 crate {crate_name}"))
        })?;
        let crate_dir = workspace_dir.join("crates").join(crate_name);
        let src_dir = crate_dir.join("src");
        fs::create_dir_all(&src_dir).map_err(|err| {
            KainError::runtime(format!("Failed to create stage2 crate dir {}: {}", src_dir.display(), err))
        })?;

        let original_manifest_path = repo_root.join("crates").join(crate_name).join("Cargo.toml");
        let original_manifest = fs::read_to_string(&original_manifest_path).map_err(|err| {
            KainError::runtime(format!("Failed to read {}: {}", original_manifest_path.display(), err))
        })?;
        let rewritten_manifest = rewrite_crate_manifest(
            &original_manifest,
            &repo_root.join("crates").join(crate_name),
            crate_name,
            &stage2_set,
        )?;
        fs::write(crate_dir.join("Cargo.toml"), rewritten_manifest).map_err(|err| {
            KainError::runtime(format!("Failed to write stage2 crate manifest for {}: {}", crate_name, err))
        })?;

        let rust_source = fs::read_to_string(roundtrip_path).map_err(|err| {
            KainError::runtime(format!("Failed to read roundtrip Rust {}: {}", roundtrip_path.display(), err))
        })?;
        fs::write(src_dir.join("lib.rs"), &rust_source).map_err(|err| {
            KainError::runtime(format!("Failed to write stage2 lib.rs for {}: {}", crate_name, err))
        })?;
        if crate_name == "cli" {
            fs::write(src_dir.join("main.rs"), "include!(\"lib.rs\");\n").map_err(|err| {
                KainError::runtime(format!("Failed to write stage2 main.rs for cli: {}", err))
            })?;
        }
    }

    Ok(())
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

    if let Some(package) = root_manifest.get("workspace").and_then(|v| v.get("package")) {
        root.push_str("[workspace.package]\n");
        let package_table = package.as_table().ok_or_else(|| {
            KainError::runtime("workspace.package must be a TOML table".to_string())
        })?;
        for (key, value) in package_table {
            root.push_str(&format!(
                "{} = {}\n",
                key,
                render_toml_inline_value(value)?
            ));
        }
        root.push('\n');
    }

    if let Some(dependencies) = root_manifest.get("workspace").and_then(|v| v.get("dependencies")) {
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
    let mut manifest: Value = toml::from_str(original_manifest)
        .map_err(|err| KainError::runtime(format!("Failed to parse crate manifest for {}: {}", crate_name, err)))?;

    rewrite_stage2_package_version(&mut manifest)?;

    for key in ["dependencies", "dev-dependencies", "build-dependencies"] {
        if let Some(table) = manifest.get_mut(key).and_then(Value::as_table_mut) {
            rewrite_dependency_table(table, original_crate_dir, stage2_crates)?;
        }
    }

    toml::to_string_pretty(&manifest)
        .map_err(|err| KainError::runtime(format!("Failed to serialize crate manifest for {}: {}", crate_name, err)))
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
            resolved.canonicalize().unwrap_or(resolved).display().to_string()
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
                    render_toml_inline_value(value)
                        .map(|rendered| format!("{key} = {rendered}"))
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
        .map_err(|err| KainError::runtime(format!("Failed to run cargo build in stage2 workspace: {}", err)))?;

    let mut log = String::new();
    log.push_str(&String::from_utf8_lossy(&output.stdout));
    log.push_str(&String::from_utf8_lossy(&output.stderr));
    fs::write(&build_log, log).map_err(|err| {
        KainError::runtime(format!("Failed to write stage2 build log {}: {}", build_log.display(), err))
    })?;

    let artifact = workspace_dir.join("target").join("debug").join(if cfg!(windows) { "kain.exe" } else { "kain" });
    Ok(Stage2BuildResult {
        success: output.status.success(),
        artifact_path: artifact.exists().then_some(artifact),
    })
}

fn render_program(program: &Program) -> KainResult<String> {
    let mut output = String::new();
    for item in &program.items {
        write_item(&mut output, item, 0)?;
    }
    Ok(output)
}

fn write_item(output: &mut String, item: &Item, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    match item {
        Item::Function(function) => write_function(output, function, indent),
        Item::Struct(value) => write_struct(output, value, indent),
        Item::Enum(value) => write_enum(output, value, indent),
        Item::Mod(value) => {
            write_line(output, indent, &format!("mod {}:", sanitize_identifier(&value.name)))?;
            if let Some(children) = &value.inline {
                if !children.is_empty() {
                    for child in children {
                        write_item(output, child, indent + 1)?;
                    }
                }
            }
            writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render module: {}", err)))?;
            Ok(())
        }
        Item::TypeAlias(value) => {
            write_line(
                output,
                indent,
                &format!("type {} = {}", sanitize_type_name(&value.name), type_to_string(&value.target)),
            )?;
            writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render type alias: {}", err)))?;
            Ok(())
        }
        Item::Const(value) => {
            write_line(
                output,
                indent,
                &format!(
                    "const {}: {} = {}",
                    sanitize_identifier(&value.name),
                    type_to_string(&value.ty),
                    inline_expr_to_string(&value.value)
                ),
            )?;
            writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render const: {}", err)))?;
            Ok(())
        }
        Item::Impl(value) => write_impl(output, value, indent),
        Item::Trait(value) => {
            write_line(output, indent, &format!("trait {}:", sanitize_type_name(&value.name)))?;
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
                        signature.push_str(&format!("{}: {}", sanitize_identifier(&param.name), type_to_string(&param.ty)));
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
            writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render trait: {}", err)))?;
            Ok(())
        }
        _ => Ok(()),
    }
}

fn write_function(output: &mut String, function: &kain_core::ast::Function, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    let mut signature = format!("fn {}(", sanitize_identifier(&function.name));
    for (index, param) in function.params.iter().enumerate() {
        if index > 0 {
            signature.push_str(", ");
        }
        signature.push_str(&format!("{}: {}", sanitize_identifier(&param.name), type_to_string(&param.ty)));
    }
    signature.push(')');
    if let Some(return_type) = &function.return_type {
        signature.push_str(&format!(" -> {}", type_to_string(return_type)));
    }
    signature.push(':');
    write_line(output, indent, &signature)?;
    write_block(output, &function.body, indent + 1)?;
    writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render function: {}", err)))?;
    Ok(())
}

fn write_struct(output: &mut String, value: &kain_core::ast::Struct, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("struct {}:", sanitize_type_name(&value.name)))?;
    if value.fields.is_empty() {
        write_line(output, indent + 1, "__selfhost_placeholder: Bool = false")?;
    } else {
        for field in &value.fields {
            let mut line = format!("{}: {}", sanitize_identifier(&field.name), type_to_string(&field.ty));
            if let Some(default) = &field.default {
                line.push_str(&format!(" = {}", inline_expr_to_string(default)));
            }
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render struct: {}", err)))?;
    Ok(())
}

fn write_enum(output: &mut String, value: &kain_core::ast::Enum, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("enum {}:", sanitize_type_name(&value.name)))?;
    if value.variants.is_empty() {
        write_line(output, indent + 1, "__SelfHostEmpty")?;
    } else {
        for variant in &value.variants {
            let line = match &variant.fields {
                kain_core::ast::VariantFields::Unit => sanitize_identifier(&variant.name),
                kain_core::ast::VariantFields::Tuple(types) => {
                    let values = types.iter().map(type_to_string).collect::<Vec<_>>().join(", ");
                    format!("{}({values})", sanitize_identifier(&variant.name))
                }
                kain_core::ast::VariantFields::Struct(fields) => {
                    let values = fields
                        .iter()
                        .map(|field| format!("{}: {}", sanitize_identifier(&field.name), type_to_string(&field.ty)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{} {{ {values} }}", sanitize_identifier(&variant.name))
                }
            };
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render enum: {}", err)))?;
    Ok(())
}

fn write_impl(output: &mut String, value: &kain_core::ast::Impl, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    let header = match &value.trait_name {
        Some(trait_name) => format!("impl {} for {}:", sanitize_path_to_ident(trait_name), type_to_string(&value.target_type)),
        None => format!("impl {}:", type_to_string(&value.target_type)),
    };
    write_line(output, indent, &header)?;
    if value.methods.is_empty() {
        write_line(output, indent + 1, "fn __selfhost_empty_impl__():")?;
        write_line(output, indent + 2, "let __selfhost_empty = none")?;
    } else {
        for method in &value.methods {
            write_function(output, method, indent + 1)?;
        }
    }
    writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render impl: {}", err)))?;
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
        Stmt::Let { pattern, ty, value, .. } => {
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
        Stmt::For { binding, iter, body, .. } => {
            write_line(output, indent, &format!("for {} in {}:", for_binding_name(binding), control_head_expr_to_string(iter)))?;
            write_block(output, body, indent + 1)
        }
        Stmt::While { condition, body, .. } => {
            write_line(output, indent, &format!("while {}:", inline_expr_to_string(condition)))?;
            write_block(output, body, indent + 1)
        }
        Stmt::Loop { body, .. } => {
            write_line(output, indent, "loop:")?;
            write_block(output, body, indent + 1)
        }
        Stmt::Item(item) => write_nested_item_stub(output, item, indent),
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
        Pattern::Tuple(values, _) => format!("({})", values.iter().map(pattern_to_string).collect::<Vec<_>>().join(", ")),
        Pattern::Variant { enum_name, variant, fields, .. } => {
            if enum_name.is_none() && matches!(fields, VariantPatternFields::Struct(_)) {
                return "_".to_string();
            }
            let head = match enum_name {
                Some(name) => format!("{}::{}", sanitize_path_to_ident(name), sanitize_identifier(variant)),
                None => sanitize_identifier(variant),
            };
            match fields {
                VariantPatternFields::Unit => head,
                VariantPatternFields::Tuple(values) => {
                    format!("{}({})", head, values.iter().map(pattern_to_string).collect::<Vec<_>>().join(", "))
                }
                VariantPatternFields::Struct(fields) => {
                    format!(
                        "{} {{ {} }}",
                        head,
                        fields
                            .iter()
                            .map(|(name, value)| format!("{}: {}", sanitize_identifier(name), pattern_to_string(value)))
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
        Pattern::Or(values, _) => values.first().map(pattern_to_string).unwrap_or_else(|| "_".to_string()),
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

fn write_expr_prefixed(output: &mut String, prefix: &str, expr: &Expr, indent: usize) -> KainResult<()> {
    match expr {
        Expr::If { condition, then_branch, else_branch, .. } => {
            write_line(output, indent, &format!("{}if {}:", prefix, control_head_expr_to_string(condition)))?;
            write_block(output, then_branch, indent + 1)?;
            if let Some(else_branch) = else_branch {
                write_else_branch(output, else_branch, indent)?;
            }
            Ok(())
        }
        Expr::Match { scrutinee, arms, .. } => {
            write_line(output, indent, &format!("{}match {}:", prefix, control_head_expr_to_string(scrutinee)))?;
            for arm in arms {
                write_match_arm(output, arm, indent + 1)?;
            }
            Ok(())
        }
        Expr::Block(block, _) if prefix.is_empty() => write_block(output, block, indent),
        _ => write_line(output, indent, &format!("{}{}", prefix, inline_expr_to_string(expr))),
    }
}

fn write_else_branch(output: &mut String, else_branch: &ElseBranch, indent: usize) -> KainResult<()> {
    match else_branch {
        ElseBranch::Else(block) => {
            write_line(output, indent, "else:")?;
            write_block(output, block, indent + 1)
        }
        ElseBranch::ElseIf(condition, then_branch, next) => {
            write_line(output, indent, &format!("elif {}:", control_head_expr_to_string(condition)))?;
            write_block(output, then_branch, indent + 1)?;
            if let Some(next) = next {
                write_else_branch(output, next, indent)?;
            }
            Ok(())
        }
    }
}

fn write_match_arm(output: &mut String, arm: &MatchArm, indent: usize) -> KainResult<()> {
    let pattern = pattern_to_string(&arm.pattern);
    if let Some(guard) = &arm.guard {
        write_line(output, indent, &format!("{pattern} =>"))?;
        write_line(output, indent + 1, &format!("if {}:", inline_expr_to_string(guard)))?;
        write_expr_prefixed(output, "", &arm.body, indent + 2)?;
        write_line(output, indent + 1, "else:")?;
        write_line(output, indent + 2, "none")?;
        return Ok(());
    }
    if let Some(inline_body) = inline_match_arm_body(&arm.body) {
        write_line(output, indent, &format!("{pattern} => {inline_body}"))
    } else {
        write_line(output, indent, &format!("{pattern} =>"))?;
        write_expr_prefixed(output, "", &arm.body, indent + 1)
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
            args.iter().map(inline_expr_to_string).collect::<Vec<_>>().join(", ")
        ),
        Expr::Binary { left, op, right, .. } => format!(
            "({} {} {})",
            inline_expr_to_string(left),
            binary_op_to_string(*op),
            inline_expr_to_string(right)
        ),
        Expr::Unary { op, operand, .. } => format!("({}{})", unary_op_to_string(*op), inline_expr_to_string(operand)),
        Expr::Call { callee, args, .. } => format!(
            "{}({})",
            inline_expr_to_string(callee),
            args.iter().map(call_arg_to_string).collect::<Vec<_>>().join(", ")
        ),
        Expr::MethodCall { receiver, method, args, .. } => format!(
            "{}.{}({})",
            inline_expr_to_string(receiver),
            sanitize_identifier(method),
            args.iter().map(call_arg_to_string).collect::<Vec<_>>().join(", ")
        ),
        Expr::Field { object, field, .. } => format!("{}.{}", inline_expr_to_string(object), sanitize_identifier(field)),
        Expr::Index { object, index, .. } => {
            if let Expr::Range { start, end, .. } = index.as_ref() {
                let start = start.as_ref().map(|value| inline_expr_to_string(value)).unwrap_or_else(|| "none".to_string());
                let end = end.as_ref().map(|value| inline_expr_to_string(value)).unwrap_or_else(|| "none".to_string());
                format!("slice({}, {}, {})", inline_expr_to_string(object), start, end)
            } else {
                format!("{}[{}]", inline_expr_to_string(object), inline_expr_to_string(index))
            }
        }
        Expr::Assign { target, value, .. } => format!("{} = {}", inline_expr_to_string(target), inline_expr_to_string(value)),
        Expr::Struct { name, fields, .. } => render_aggregate_init(sanitize_type_name(name), true, fields),
        Expr::AggregateInit { ty, fields, zero_fill_rest, .. } => render_aggregate_init(type_to_string(ty), *zero_fill_rest, fields),
        Expr::EnumVariant { enum_name, variant, fields, .. } => {
            let head = format!("{}::{}", sanitize_path_to_ident(enum_name), sanitize_identifier(variant));
            match fields {
                EnumVariantFields::Unit => head,
                EnumVariantFields::Tuple(values) => format!("{}({})", head, values.iter().map(inline_expr_to_string).collect::<Vec<_>>().join(", ")),
                EnumVariantFields::Struct(fields) => format!(
                    "{} {{ {} }}",
                    head,
                    fields
                        .iter()
                        .map(|(name, value)| format!("{}: {}", sanitize_identifier(name), inline_expr_to_string(value)))
                        .collect::<Vec<_>>()
                        .join(", ")
                ),
            }
        }
        Expr::Array(values, _) => format!("[{}]", values.iter().map(inline_expr_to_string).collect::<Vec<_>>().join(", ")),
        Expr::Tuple(values, _) => format!("({})", values.iter().map(inline_expr_to_string).collect::<Vec<_>>().join(", ")),
        Expr::Range { start, end, inclusive, .. } => render_range_expr(start.as_deref(), end.as_deref(), *inclusive),
        Expr::If { condition, then_branch, else_branch, .. } => {
            if let (Some(then_expr), Some(else_branch)) = (block_inline_expr(then_branch), else_branch.as_deref()) {
                if let Some(else_expr) = else_branch_inline_expr(else_branch) {
                    return format!(
                        "if {}: {} else: {}",
                        inline_expr_to_string(condition),
                        inline_expr_to_string(then_expr),
                        inline_expr_to_string(else_expr)
                    );
                }
            }
            "none".to_string()
        }
        Expr::Lambda { .. } => "none".to_string(),
        Expr::Ref { mutable, value, .. } => {
            if *mutable { format!("&mut {}", inline_expr_to_string(value)) } else { format!("&{}", inline_expr_to_string(value)) }
        }
        Expr::AddrOf { value, .. } => format!("addr_of({})", inline_expr_to_string(value)),
        Expr::Deref(value, _) => format!("*{}", inline_expr_to_string(value)),
        Expr::PtrOffset { pointer, offset, .. } => format!("ptr_offset({}, {})", inline_expr_to_string(pointer), inline_expr_to_string(offset)),
        Expr::MemLoad { pointer, .. } => format!("mem_load({})", inline_expr_to_string(pointer)),
        Expr::MemStore { pointer, value, .. } => format!("mem_store({}, {})", inline_expr_to_string(pointer), inline_expr_to_string(value)),
        Expr::SizeOfType { target, .. } => format!("sizeof_type({:?})", type_to_string(target)),
        Expr::AlignOfType { target, .. } => format!("alignof_type({:?})", type_to_string(target)),
        Expr::Alloca { ty, .. } => format!("alloca({:?})", type_to_string(ty)),
        Expr::Uninit { ty, .. } => format!("uninit({:?})", type_to_string(ty)),
        Expr::Alloc { size, ty, zeroed, .. } => match ty {
            Some(ty) => format!("alloc_mem({}, {:?}, {})", inline_expr_to_string(size), type_to_string(ty), zeroed),
            None => format!("alloc_mem({}, none, {})", inline_expr_to_string(size), zeroed),
        },
        Expr::Realloc { pointer, size, ty, zeroed_new, .. } => match ty {
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
        Expr::Cast { value, target, .. } => format!("{} as {}", inline_expr_to_string(value), type_to_string(target)),
        Expr::Try(value, _) => format!("{}?", inline_expr_to_string(value)),
        Expr::Await(value, _) => format!("await {}", inline_expr_to_string(value)),
        Expr::Comptime(value, _) => format!("comptime {}", inline_expr_to_string(value)),
        Expr::Block(block, _) => block_inline_expr(block).map(inline_expr_to_string).unwrap_or_else(|| "none".to_string()),
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

fn call_arg_to_string(arg: &CallArg) -> String {
    match &arg.name {
        Some(name) => format!("{} = {}", sanitize_identifier(name), inline_expr_to_string(&arg.value)),
        None => inline_expr_to_string(&arg.value),
    }
}

fn type_to_string(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                sanitize_type_path(name)
            } else {
                format!("{}<{}>", sanitize_type_path(name), generics.iter().map(type_to_string).collect::<Vec<_>>().join(", "))
            }
        }
        Type::Tuple(values, _) => format!("({})", values.iter().map(type_to_string).collect::<Vec<_>>().join(", ")),
        Type::Array(inner, len, _) => format!("[{}; {}]", type_to_string(inner), len),
        Type::Slice(inner, _) => format!("[{}]", type_to_string(inner)),
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", type_to_string(inner))
            } else {
                format!("&{}", type_to_string(inner))
            }
        }
        Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("ptr_mut<{}>", type_to_string(inner))
            } else {
                format!("ptr<{}>", type_to_string(inner))
            }
        }
        Type::Function { params, return_type, .. } => format!(
            "fn({}) -> {}",
            params.iter().map(type_to_string).collect::<Vec<_>>().join(", "),
            type_to_string(return_type)
        ),
        Type::Option(inner, _) => format!("Option<{}>", type_to_string(inner)),
        Type::Result(ok, err, _) => format!("Result<{}, {}>", type_to_string(ok), type_to_string(err)),
        Type::Infer(_) => "_".to_string(),
        Type::Never(_) => "!".to_string(),
        Type::Unit(_) => "()".to_string(),
        Type::Impl { trait_name, generics, .. } => {
            if generics.is_empty() {
                format!("impl {}", sanitize_path_to_ident(trait_name))
            } else {
                format!("impl {}<{}>", sanitize_path_to_ident(trait_name), generics.iter().map(type_to_string).collect::<Vec<_>>().join(", "))
            }
        }
    }
}

fn sanitize_identifier(name: &str) -> String {
    let mut sanitized = name
        .chars()
        .map(|ch| if ch.is_ascii_alphanumeric() || ch == '_' { ch } else { '_' })
        .collect::<String>();
    if sanitized.is_empty() {
        sanitized.push('_');
    }
    if sanitized.chars().next().is_some_and(|ch| ch.is_ascii_digit()) {
        sanitized.insert(0, '_');
    }
    if RESERVED_KEYWORDS.contains(&sanitized.as_str()) || SELFHOST_CONTEXTUAL_KEYWORDS.contains(&sanitized.as_str()) {
        sanitized.push('_');
    }
    sanitized
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

fn sanitize_path_to_ident(path: &str) -> String {
    path.split("::")
        .map(sanitize_identifier)
        .collect::<Vec<_>>()
        .join("__")
}

fn sanitize_expr_path(path: &str) -> String {
    if path.contains("::") {
        sanitize_path_to_ident(path)
    } else {
        sanitize_identifier(path)
    }
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

fn block_inline_expr(block: &Block) -> Option<&Expr> {
    if block.stmts.len() != 1 {
        return None;
    }
    match &block.stmts[0] {
        Stmt::Expr(expr) => Some(expr),
        Stmt::Return(Some(expr), _) => Some(expr),
        _ => None,
    }
}

fn else_branch_inline_expr(branch: &ElseBranch) -> Option<&Expr> {
    match branch {
        ElseBranch::Else(block) => block_inline_expr(block),
        ElseBranch::ElseIf(_, _, _) => None,
    }
}

fn render_aggregate_init(type_name: String, zero_fill_rest: bool, fields: &[(String, Expr)]) -> String {
    let mut args = vec![format!("{:?}", type_name)];
    args.extend(
        fields
            .iter()
            .map(|(field, value)| format!("{} = {}", sanitize_identifier(field), inline_expr_to_string(value))),
    );
    if !zero_fill_rest {
        args.push("false".to_string());
    }
    format!("aggregate_init({})", args.join(", "))
}

fn render_range_expr(start: Option<&Expr>, end: Option<&Expr>, inclusive: bool) -> String {
    let start = start.map(inline_expr_to_string).unwrap_or_else(|| "0".to_string());
    let end = end.map(inline_expr_to_string).unwrap_or_else(|| "0".to_string());
    if inclusive {
        format!("range({}, ({} + 1))", start, end)
    } else {
        format!("range({}, {})", start, end)
    }
}

fn for_binding_name(pattern: &Pattern) -> String {
    match pattern {
        Pattern::Binding { name, .. } => sanitize_identifier(name),
        _ => "_item".to_string(),
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
    Err(KainError::runtime("Failed to find KAIN repo root from current directory"))
}

fn default_inventory_dir(repo_root: &Path) -> PathBuf {
    repo_root
        .parent()
        .map(|parent| parent.join("OuroborosV2").join("docs").join("selfhost").join("inventories"))
        .unwrap_or_else(|| PathBuf::from("OuroborosV2").join("docs").join("selfhost").join("inventories"))
}

fn default_output_dir(repo_root: &Path) -> PathBuf {
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
