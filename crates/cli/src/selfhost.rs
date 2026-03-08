use crate::error::{KainError, KainResult};
use crate::selfhost_report::{
    render_phase1_markdown, CratePhase1Result, MacroFinding, SelfHostPhase1Report,
    SelfHostPhaseStatus, TraitDynSummary,
};
use chrono::Utc;
use clap::Subcommand;
use kain_core::ast::{Block, CallArg, ElseBranch, EnumVariantFields, Expr, Item, MatchArm, Pattern, Program, Stmt, Type, VariantPatternFields};
use kain_import::rust::RustSelfHostOptions;
use serde::Deserialize;
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};

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
}

pub fn run(command: SelfHostCommand) -> KainResult<()> {
    match command {
        SelfHostCommand::Phase1 {
            inventory_dir,
            output_dir,
            emit_bundles,
        } => run_phase1(inventory_dir, output_dir, emit_bundles),
    }
}

fn run_phase1(
    inventory_dir: Option<PathBuf>,
    output_dir: Option<PathBuf>,
    emit_bundles: bool,
) -> KainResult<()> {
    let repo_root = find_repo_root(&std::env::current_dir().map_err(KainError::Io)?)?;
    let inventory_dir = inventory_dir.unwrap_or_else(|| default_inventory_dir(&repo_root));
    let output_dir = output_dir.unwrap_or_else(|| default_output_dir(&repo_root));
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

    let crates_processed = inventories.module_map.initial_slice.clone();
    let mut modules_discovered: BTreeMap<String, Vec<String>> = BTreeMap::new();
    let mut crate_results = Vec::new();
    let mut all_rejected = Vec::new();
    let mut all_required_preserved = Vec::new();

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
                    fs::write(&bundle_path, rendered).map_err(|err| {
                        KainError::runtime(format!(
                            "Failed to write self-host bundle {}: {}",
                            bundle_path.display(),
                            err
                        ))
                    })?;
                    output_kn_path = Some(bundle_path.display().to_string());
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
    let final_phase_status = determine_phase_status(&crate_results, &all_required_preserved);

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
        final_phase_status: final_phase_status.clone(),
    };

    write_report_files(&output_dir, &report)?;
    print_summary(&report);

    match final_phase_status {
        SelfHostPhaseStatus::Pass => Ok(()),
        SelfHostPhaseStatus::SoftFail => Err(KainError::runtime("Self-host phase1 completed with soft failures")),
        SelfHostPhaseStatus::HardFail => Err(KainError::runtime("Self-host phase1 failed")),
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

fn write_report_files(output_dir: &Path, report: &SelfHostPhase1Report) -> KainResult<()> {
    let json_path = output_dir.join("phase1_report.json");
    let markdown_path = output_dir.join("phase1_report.md");
    let json = serde_json::to_string_pretty(report)
        .map_err(|err| KainError::runtime(format!("Failed to serialize phase1 report: {}", err)))?;
    fs::write(&json_path, json).map_err(|err| {
        KainError::runtime(format!("Failed to write phase1 report JSON {}: {}", json_path.display(), err))
    })?;
    fs::write(&markdown_path, render_phase1_markdown(report)).map_err(|err| {
        KainError::runtime(format!("Failed to write phase1 report Markdown {}: {}", markdown_path.display(), err))
    })?;
    Ok(())
}

fn print_summary(report: &SelfHostPhase1Report) {
    println!("🧬 Self-host phase1");
    println!("   Crates: {}", report.crates_processed.join(", "));
    println!(
        "   Status: {}",
        match report.final_phase_status {
            SelfHostPhaseStatus::Pass => "pass",
            SelfHostPhaseStatus::SoftFail => "soft_fail",
            SelfHostPhaseStatus::HardFail => "hard_fail",
        }
    );
    println!("   Report JSON: {}", Path::new(&report.output_dir).join("phase1_report.json").display());
    println!("   Report MD: {}", Path::new(&report.output_dir).join("phase1_report.md").display());
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
            write_line(output, indent, &format!("mod {}:", value.name))?;
            if let Some(children) = &value.inline {
                if children.is_empty() {
                    write_line(output, indent + 1, "pass")?;
                } else {
                    for child in children {
                        write_item(output, child, indent + 1)?;
                    }
                }
            } else {
                write_line(output, indent + 1, "pass")?;
            }
            writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render module: {}", err)))?;
            Ok(())
        }
        Item::TypeAlias(value) => {
            write_line(output, indent, &format!("type {} = {}", value.name, type_to_string(&value.target)))?;
            writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render type alias: {}", err)))?;
            Ok(())
        }
        Item::Const(value) => {
            write_line(
                output,
                indent,
                &format!("const {}: {} = {}", value.name, type_to_string(&value.ty), expr_to_string(&value.value)),
            )?;
            writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render const: {}", err)))?;
            Ok(())
        }
        Item::Impl(value) => write_impl(output, value, indent),
        Item::Trait(value) => {
            write_line(output, indent, &format!("trait {}:", value.name))?;
            if value.methods.is_empty() {
                write_line(output, indent + 1, "pass")?;
            } else {
                for method in &value.methods {
                    let mut signature = format!("fn {}(", method.name);
                    for (index, param) in method.params.iter().enumerate() {
                        if index > 0 {
                            signature.push_str(", ");
                        }
                        signature.push_str(&format!("{}: {}", param.name, type_to_string(&param.ty)));
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
                        write_line(output, indent + 2, "pass")?;
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

    let mut signature = format!("fn {}(", function.name);
    for (index, param) in function.params.iter().enumerate() {
        if index > 0 {
            signature.push_str(", ");
        }
        signature.push_str(&format!("{}: {}", param.name, type_to_string(&param.ty)));
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

    write_line(output, indent, &format!("struct {}:", value.name))?;
    if value.fields.is_empty() {
        write_line(output, indent + 1, "pass")?;
    } else {
        for field in &value.fields {
            let mut line = format!("{}: {}", field.name, type_to_string(&field.ty));
            if let Some(default) = &field.default {
                line.push_str(&format!(" = {}", expr_to_string(default)));
            }
            write_line(output, indent + 1, &line)?;
        }
    }
    writeln!(output).map_err(|err| KainError::runtime(format!("Failed to render struct: {}", err)))?;
    Ok(())
}

fn write_enum(output: &mut String, value: &kain_core::ast::Enum, indent: usize) -> KainResult<()> {
    use std::fmt::Write;

    write_line(output, indent, &format!("enum {}:", value.name))?;
    if value.variants.is_empty() {
        write_line(output, indent + 1, "pass")?;
    } else {
        for variant in &value.variants {
            let line = match &variant.fields {
                kain_core::ast::VariantFields::Unit => variant.name.clone(),
                kain_core::ast::VariantFields::Tuple(types) => {
                    let values = types.iter().map(type_to_string).collect::<Vec<_>>().join(", ");
                    format!("{}({values})", variant.name)
                }
                kain_core::ast::VariantFields::Struct(fields) => {
                    let values = fields
                        .iter()
                        .map(|field| format!("{}: {}", field.name, type_to_string(&field.ty)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    format!("{} {{ {values} }}", variant.name)
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
        Some(trait_name) => format!("impl {} for {}:", trait_name, type_to_string(&value.target_type)),
        None => format!("impl {}:", type_to_string(&value.target_type)),
    };
    write_line(output, indent, &header)?;
    if value.methods.is_empty() {
        write_line(output, indent + 1, "pass")?;
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
        write_line(output, indent, "pass")?;
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
                line.push_str(&format!(" = {}", expr_to_string(value)));
            }
            write_line(output, indent, &line)
        }
        Stmt::Expr(expr) => write_line(output, indent, &expr_to_string(expr)),
        Stmt::Return(value, _) => {
            if let Some(value) = value {
                write_line(output, indent, &format!("return {}", expr_to_string(value)))
            } else {
                write_line(output, indent, "return")
            }
        }
        Stmt::Break(value, _) => {
            if let Some(value) = value {
                write_line(output, indent, &format!("break {}", expr_to_string(value)))
            } else {
                write_line(output, indent, "break")
            }
        }
        Stmt::Item(item) => write_item(output, item, indent),
        _ => write_line(output, indent, "pass"),
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
                format!("mut {name}")
            } else {
                name.clone()
            }
        }
        Pattern::Literal(expr) => expr_to_string(expr),
        _ => "_".to_string(),
    }
}

fn expr_to_string(expr: &Expr) -> String {
    match expr {
        Expr::Int(value, _) => value.to_string(),
        Expr::Float(value, _) => value.to_string(),
        Expr::String(value, _) => format!("{:?}", value),
        Expr::Bool(value, _) => value.to_string(),
        Expr::None(_) => "none".to_string(),
        Expr::Ident(value, _) => value.clone(),
        Expr::Array(values, _) => format!("[{}]", values.iter().map(expr_to_string).collect::<Vec<_>>().join(", ")),
        Expr::Tuple(values, _) => format!("({})", values.iter().map(expr_to_string).collect::<Vec<_>>().join(", ")),
        Expr::MacroCall { name, args, .. } => format!("{}!({})", name, args.iter().map(expr_to_string).collect::<Vec<_>>().join(", ")),
        Expr::Call { callee, args, .. } => format!(
            "{}({})",
            expr_to_string(callee),
            args.iter().map(call_arg_to_string).collect::<Vec<_>>().join(", ")
        ),
        Expr::MethodCall { receiver, method, args, .. } => format!(
            "{}.{}({})",
            expr_to_string(receiver),
            method,
            args.iter().map(call_arg_to_string).collect::<Vec<_>>().join(", ")
        ),
        Expr::Field { object, field, .. } => format!("{}.{}", expr_to_string(object), field),
        Expr::Binary { left, right, .. } => format!("{} #op {}", expr_to_string(left), expr_to_string(right)),
        Expr::Unary { operand, .. } => format!("#unary {}", expr_to_string(operand)),
        Expr::Struct { name, fields, .. } => format!(
            "{} {{ {} }}",
            name,
            fields
                .iter()
                .map(|(field, value)| format!("{}: {}", field, expr_to_string(value)))
                .collect::<Vec<_>>()
                .join(", ")
        ),
        Expr::Match { .. } => "#match".to_string(),
        Expr::If { .. } => "#if".to_string(),
        Expr::Range { .. } => "#range".to_string(),
        Expr::Cast { value, target, .. } => format!("{} as {}", expr_to_string(value), type_to_string(target)),
        Expr::Block(_, _) => "#block".to_string(),
        Expr::Return(value, _) => match value {
            Some(value) => format!("return {}", expr_to_string(value)),
            None => "return".to_string(),
        },
        Expr::Break(value, _) => match value {
            Some(value) => format!("break {}", expr_to_string(value)),
            None => "break".to_string(),
        },
        Expr::Continue(_) => "continue".to_string(),
        _ => "#expr".to_string(),
    }
}

fn call_arg_to_string(arg: &CallArg) -> String {
    match &arg.name {
        Some(name) => format!("{}: {}", name, expr_to_string(&arg.value)),
        None => expr_to_string(&arg.value),
    }
}

fn type_to_string(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                name.clone()
            } else {
                format!("{}<{}>", name, generics.iter().map(type_to_string).collect::<Vec<_>>().join(", "))
            }
        }
        Type::Tuple(values, _) => format!("({})", values.iter().map(type_to_string).collect::<Vec<_>>().join(", ")),
        Type::Array(inner, _, _) => format!("Array<{}>", type_to_string(inner)),
        Type::Slice(inner, _) => format!("Slice<{}>", type_to_string(inner)),
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", type_to_string(inner))
            } else {
                format!("&{}", type_to_string(inner))
            }
        }
        Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("PtrMut<{}>", type_to_string(inner))
            } else {
                format!("Ptr<{}>", type_to_string(inner))
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
                format!("impl {}", trait_name)
            } else {
                format!("impl {}<{}>", trait_name, generics.iter().map(type_to_string).collect::<Vec<_>>().join(", "))
            }
        }
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
