use blade::{discover_workspace, resolve_blade, BladeWorkspace, ResolvedBlade};
pub use kain_commands::blade::BladesCommand;
use serde::Serialize;
use std::path::PathBuf;

#[derive(Debug, Serialize)]
struct BladeCheckReport {
    workspace: BladeWorkspace,
    missing_paths: Vec<MissingBladePath>,
    ok: bool,
}

#[derive(Debug, Serialize)]
struct MissingBladePath {
    blade: String,
    field: String,
    path: PathBuf,
}

pub fn run(command: BladesCommand) -> Result<(), String> {
    match command {
        BladesCommand::List { path, json } => {
            let workspace = discover_workspace(&path).map_err(|err| err.to_string())?;
            if json {
                print_json(&workspace)?;
            } else {
                print_blade_list(&workspace);
            }
            Ok(())
        }
        BladesCommand::Graph { path, json } => {
            let workspace = discover_workspace(&path).map_err(|err| err.to_string())?;
            if json {
                print_json(&workspace.dependency_edges())?;
            } else {
                print_blade_graph(&workspace);
            }
            Ok(())
        }
        BladesCommand::Check { path, json } => {
            let workspace = discover_workspace(&path).map_err(|err| err.to_string())?;
            let report = check_workspace(workspace);
            if json {
                print_json(&report)?;
            } else {
                print_blade_check(&report);
            }
            if report.ok {
                Ok(())
            } else {
                Err("blade check failed".to_string())
            }
        }
        BladesCommand::Build {
            path,
            profile,
            target,
            dry_run,
            clean,
            include_vulkan,
            json,
        } => run_build(path, profile, target, dry_run, clean, include_vulkan, json),
    }
}

pub fn run_equip(blade_name: String, path: PathBuf, json: bool) -> Result<(), String> {
    let blade = resolve_blade(&path, &blade_name).map_err(|err| err.to_string())?;
    if json {
        print_json(&blade)?;
    } else {
        print_equipped_blade(&blade);
    }
    Ok(())
}

fn print_json<T: Serialize>(value: &T) -> Result<(), String> {
    let text = serde_json::to_string_pretty(value)
        .map_err(|err| format!("failed to serialize blades JSON: {err}"))?;
    println!("{text}");
    Ok(())
}

fn print_blade_list(workspace: &BladeWorkspace) {
    println!("Blade workspace: {}", workspace.root.display());
    if let Some(path) = &workspace.manifest_path {
        println!("Workspace manifest: {}", path.display());
    }
    println!("Blades: {}", workspace.blades.len());
    for blade in &workspace.blades {
        println!(
            "  {}  kind={}  root={}",
            blade.name,
            blade.kind,
            blade.root.display()
        );
    }
    print_diagnostics(workspace);
}

fn print_blade_graph(workspace: &BladeWorkspace) {
    println!("Blade graph: {}", workspace.root.display());
    let edges = workspace.dependency_edges();
    if edges.is_empty() {
        println!("  no declared blade dependencies");
    } else {
        for edge in edges {
            let optional = if edge.optional { " optional" } else { "" };
            let kind = edge
                .kind
                .as_deref()
                .map(|value| format!(" kind={value}"))
                .unwrap_or_default();
            println!("  {} -> {}{}{}", edge.from, edge.to, optional, kind);
        }
    }
    print_diagnostics(workspace);
}

fn print_blade_check(report: &BladeCheckReport) {
    println!("Blade check: {}", report.workspace.root.display());
    for diagnostic in &report.workspace.diagnostics {
        println!("  {}: {}", diagnostic.severity, diagnostic.message);
    }
    if report.missing_paths.is_empty() {
        println!("  all referenced local blade paths exist");
    } else {
        println!("  missing paths: {}", report.missing_paths.len());
        for missing in &report.missing_paths {
            println!(
                "  {} {} -> {}",
                missing.blade,
                missing.field,
                missing.path.display()
            );
        }
    }
}

fn print_equipped_blade(blade: &ResolvedBlade) {
    println!("Equipped blade: {}", blade.name);
    println!("  kind: {}", blade.kind);
    println!("  root: {}", blade.root.display());
    if let Some(version) = &blade.version {
        println!("  version: {version}");
    }
    if let Some(entry) = &blade.entry {
        println!("  entry: {}", entry.display());
    }
    if let Some(cargo_manifest) = &blade.cargo_manifest {
        println!("  cargo: {}", cargo_manifest.display());
    }
    if let Some(kain_manifest) = &blade.kain_manifest {
        println!("  kain: {}", kain_manifest.display());
    }
    if let Some(fabric_manifest) = &blade.fabric_manifest {
        println!("  fabric: {}", fabric_manifest.display());
    }
    if !blade.module_roots.is_empty() {
        println!("  module roots:");
        for root in &blade.module_roots {
            println!("    {}", root.display());
        }
    }
    if !blade.c_ffi_libraries.is_empty() {
        println!("  c ffi:");
        for library in &blade.c_ffi_libraries {
            println!("    {} -> {}", library.name, library.header.display());
        }
    }
    if !blade.compute_keys.is_empty() {
        println!("  compute keys: {}", blade.compute_keys.join(", "));
    }
}

fn print_diagnostics(workspace: &BladeWorkspace) {
    for diagnostic in &workspace.diagnostics {
        println!("  {}: {}", diagnostic.severity, diagnostic.message);
    }
}

pub fn run_build(
    path: PathBuf,
    profile: Option<String>,
    target: Option<String>,
    dry_run: bool,
    clean: bool,
    include_vulkan: bool,
    json: bool,
) -> Result<(), String> {
    let mut options = kain_build::BladeBuildOptions::new(path);
    options.profile = profile;
    options.target = target;
    options.dry_run = dry_run;
    options.clean = clean;
    options.include_vulkan = include_vulkan;

    match kain_build::build_blade_workspace(&options) {
        Ok(report) => {
            if json {
                print_json(&report)?;
            } else {
                print_build_report(&report);
            }
            Ok(())
        }
        Err(error) => Err(error.to_string()),
    }
}

fn print_build_report(report: &kain_build::BladeBuildReport) {
    println!("Blade build: {}", report.workspace_root.display());
    println!("  status: {:?}", report.status);
    println!("  profile: {}", report.profile);
    println!("  target: {}", report.target);
    println!("  artifacts: {}", report.artifact_root.display());
    println!("  report: {}", report.report_path.display());
    println!("  tasks: {}", report.tasks.len());
    for task in &report.tasks {
        let blade = task
            .blade
            .as_deref()
            .map(|value| format!(" blade={value}"))
            .unwrap_or_default();
        let cached = if task.cache_hit { " cached" } else { "" };
        println!("    {:?} {}{}{}", task.status, task.id, blade, cached);
        if let Some(error) = &task.error {
            println!("      error: {error}");
        }
    }
}

fn check_workspace(workspace: BladeWorkspace) -> BladeCheckReport {
    let mut missing_paths = Vec::new();
    for blade in &workspace.blades {
        check_optional_path(&mut missing_paths, blade, "entry", blade.entry.as_ref());
        check_optional_path(
            &mut missing_paths,
            blade,
            "kain_manifest",
            blade.kain_manifest.as_ref(),
        );
        check_optional_path(
            &mut missing_paths,
            blade,
            "cargo_manifest",
            blade.cargo_manifest.as_ref(),
        );
        check_optional_path(
            &mut missing_paths,
            blade,
            "fabric_manifest",
            blade.fabric_manifest.as_ref(),
        );
        for path in &blade.module_roots {
            check_path(&mut missing_paths, blade, "module_root", path);
        }
        for library in &blade.c_ffi_libraries {
            check_path(&mut missing_paths, blade, "c_ffi.header", &library.header);
            check_optional_path(
                &mut missing_paths,
                blade,
                "c_ffi.shared_lib",
                library.shared_lib.as_ref(),
            );
        }
        for shader_source in &blade.gpu_shader_sources {
            check_path(
                &mut missing_paths,
                blade,
                "gpu.shader_source",
                shader_source,
            );
        }
        for shader_root in &blade.gpu_shader_roots {
            check_path(&mut missing_paths, blade, "gpu.shader_root", shader_root);
        }
        for path in blade.artifacts.values() {
            check_path(&mut missing_paths, blade, "artifact", path);
        }
    }
    let ok = workspace.diagnostics.is_empty() && missing_paths.is_empty();
    BladeCheckReport {
        workspace,
        missing_paths,
        ok,
    }
}

fn check_optional_path(
    missing_paths: &mut Vec<MissingBladePath>,
    blade: &ResolvedBlade,
    field: &str,
    path: Option<&PathBuf>,
) {
    if let Some(path) = path {
        check_path(missing_paths, blade, field, path);
    }
}

fn check_path(
    missing_paths: &mut Vec<MissingBladePath>,
    blade: &ResolvedBlade,
    field: &str,
    path: &PathBuf,
) {
    if !path.exists() {
        missing_paths.push(MissingBladePath {
            blade: blade.name.clone(),
            field: field.to_string(),
            path: path.clone(),
        });
    }
}
