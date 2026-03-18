use serde::Deserialize;
use serde_json::json;
use std::collections::BTreeMap;
use std::env;
use std::fmt::Write as _;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

const DEFAULT_PAIRING_MANIFEST: &str = "runtime/parallel/config/runtime_pairing_manifest.json";
const DEFAULT_TOOLCHAIN_CONFIG: &str = "runtime/parallel/config/toolchains.json";

fn main() {
    if let Err(error) = run() {
        eprintln!("error: {error}");
        std::process::exit(1);
    }
}

fn run() -> Result<(), String> {
    let mut args = env::args().skip(1);
    let command = args.next().unwrap_or_else(|| "summary".to_string());
    let workspace_root = discover_workspace_root()?;
    let pairing = load_pairing_manifest(&workspace_root.join(DEFAULT_PAIRING_MANIFEST))?;
    let toolchains = load_toolchain_config(&workspace_root.join(DEFAULT_TOOLCHAIN_CONFIG))?;
    let native_metadata = load_native_metadata(&workspace_root.join(&pairing.native_runtime.metadata_path))?;

    match command.as_str() {
        "summary" => println!(
            "{}",
            format_summary(&workspace_root, &pairing, &toolchains, &native_metadata)
        ),
        "check" => run_check(&workspace_root, &pairing, &toolchains, &native_metadata)?,
        "json" => println!(
            "{}",
            serde_json::to_string_pretty(&build_json_summary(
                &workspace_root,
                &pairing,
                &toolchains,
                &native_metadata,
            ))
            .map_err(|error| error.to_string())?
        ),
        "report" => {
            let issues = validate_pairings(&workspace_root, &pairing, &toolchains, &native_metadata);
            if !issues.is_empty() {
                for issue in &issues {
                    eprintln!("- {issue}");
                }
                return Err(format!("cannot write report; {} validation issue(s) remain", issues.len()));
            }
            let output_path = workspace_root
                .join(&toolchains.outputs.report_dir)
                .join(&toolchains.outputs.rust_report);
            if let Some(parent) = output_path.parent() {
                fs::create_dir_all(parent)
                    .map_err(|error| format!("failed to create {}: {error}", parent.display()))?;
            }
            let encoded = serde_json::to_string_pretty(&build_json_summary(
                &workspace_root,
                &pairing,
                &toolchains,
                &native_metadata,
            ))
            .map_err(|error| error.to_string())?;
            fs::write(&output_path, encoded)
                .map_err(|error| format!("failed to write {}: {error}", output_path.display()))?;
            println!("{}", output_path.display());
        }
        "component" => {
            let component_id = args
                .next()
                .ok_or_else(|| "component command requires an id".to_string())?;
            let component = pairing
                .pairing_components
                .iter()
                .find(|component| component.id == component_id)
                .ok_or_else(|| format!("unknown component '{component_id}'"))?;
            println!("{}", format_component(component));
        }
        "toolchains" => println!("{}", format_toolchains(&toolchains)),
        other => {
            return Err(format!(
                "unknown command '{other}'. expected one of: summary, check, json, report, component, toolchains"
            ));
        }
    }

    Ok(())
}

fn run_check(
    workspace_root: &Path,
    pairing: &PairingManifest,
    toolchains: &ToolchainConfig,
    native_metadata: &NativeRuntimeMetadata,
) -> Result<(), String> {
    let issues = validate_pairings(workspace_root, pairing, toolchains, native_metadata);
    if issues.is_empty() {
        println!("parallel runtime check passed");
        println!("  components: {}", pairing.pairing_components.len());
        println!(
            "  native services: {} total ({} planned)",
            native_metadata.services.len(),
            count_services_by_status(&native_metadata.services, "planned")
        );
        println!(
            "  report dir: {}",
            workspace_root.join(&toolchains.outputs.report_dir).display()
        );
        Ok(())
    } else {
        for issue in &issues {
            eprintln!("- {issue}");
        }
        Err(format!("parallel runtime check failed with {} issue(s)", issues.len()))
    }
}

fn discover_workspace_root() -> Result<PathBuf, String> {
    let mut current = env::current_dir().map_err(|error| error.to_string())?;
    loop {
        if current.join("Cargo.toml").exists() {
            return Ok(current);
        }
        if !current.pop() {
            return Err("could not locate workspace root".to_string());
        }
    }
}

fn load_pairing_manifest(path: &Path) -> Result<PairingManifest, String> {
    let text =
        fs::read_to_string(path).map_err(|error| format!("failed to read {}: {error}", path.display()))?;
    serde_json::from_str(&text).map_err(|error| format!("failed to parse {}: {error}", path.display()))
}

fn load_toolchain_config(path: &Path) -> Result<ToolchainConfig, String> {
    let text =
        fs::read_to_string(path).map_err(|error| format!("failed to read {}: {error}", path.display()))?;
    serde_json::from_str(&text).map_err(|error| format!("failed to parse {}: {error}", path.display()))
}

fn load_native_metadata(path: &Path) -> Result<NativeRuntimeMetadata, String> {
    let text =
        fs::read_to_string(path).map_err(|error| format!("failed to read {}: {error}", path.display()))?;
    serde_json::from_str(&text).map_err(|error| format!("failed to parse {}: {error}", path.display()))
}

fn format_summary(
    workspace_root: &Path,
    pairing: &PairingManifest,
    toolchains: &ToolchainConfig,
    native_metadata: &NativeRuntimeMetadata,
) -> String {
    let mut output = String::new();
    let _ = writeln!(output, "Kain Parallel Runtime Summary");
    let _ = writeln!(output, "workspace: {}", workspace_root.display());
    let _ = writeln!(
        output,
        "native runtime: {} {} (abi {})",
        native_metadata.runtime_name,
        native_metadata.version.runtime.string,
        native_metadata.version.abi.string
    );
    let _ = writeln!(
        output,
        "native compatibility: {}",
        native_metadata.metadata.compatibility_class
    );
    let _ = writeln!(
        output,
        "native sources: {} declared in metadata",
        count_native_source_entries(&native_metadata.sources)
    );
    let _ = writeln!(
        output,
        "services: {} total, {} available, {} planned",
        native_metadata.services.len(),
        count_services_by_status(&native_metadata.services, "available"),
        count_services_by_status(&native_metadata.services, "planned")
    );
    let _ = writeln!(output, "toolchains:");
    let _ = writeln!(output, "  cargo: {}", resolve_binary_status(&toolchains.tools.cargo));
    let _ = writeln!(output, "  zig: {}", resolve_binary_status(&toolchains.tools.zig));
    let _ = writeln!(
        output,
        "  clang({}): {}",
        toolchains.tools.clang.env,
        resolve_env_tool_status(&toolchains.tools.clang)
    );
    let _ = writeln!(
        output,
        "report dir: {}",
        workspace_root.join(&toolchains.outputs.report_dir).display()
    );
    let _ = writeln!(output, "components:");
    for (lane, count) in count_components_by_lane(&pairing.pairing_components) {
        let _ = writeln!(output, "  {lane}: {count}");
    }
    let _ = writeln!(output, "active pairings:");
    for component in pairing
        .pairing_components
        .iter()
        .filter(|component| component.status == "active")
    {
        let _ = writeln!(
            output,
            "  {} [{}] phases {:?}",
            component.id, component.lane, component.phases
        );
    }
    output
}

fn validate_pairings(
    workspace_root: &Path,
    pairing: &PairingManifest,
    toolchains: &ToolchainConfig,
    native_metadata: &NativeRuntimeMetadata,
) -> Vec<String> {
    let mut issues = Vec::new();
    let service_map: BTreeMap<&str, &NativeServiceMetadata> = native_metadata
        .services
        .iter()
        .map(|service| (service.key.as_str(), service))
        .collect();
    let component_map: BTreeMap<&str, &PairingComponent> = pairing
        .pairing_components
        .iter()
        .map(|component| (component.id.as_str(), component))
        .collect();

    for relative in [
        &pairing.native_runtime.manifest_path,
        &pairing.native_runtime.metadata_path,
        &pairing.native_runtime.tracker_path,
        &pairing.native_runtime.spec_tasks_path,
    ] {
        let absolute = workspace_root.join(relative);
        if !absolute.exists() {
            issues.push(format!("missing declared runtime file: {}", absolute.display()));
        }
    }

    let report_dir = workspace_root.join(&toolchains.outputs.report_dir);
    if !report_dir.exists() {
        issues.push(format!("missing report dir: {}", report_dir.display()));
    }

    if !binary_available(&toolchains.tools.cargo) {
        issues.push(format!("cargo tool unavailable: {}", toolchains.tools.cargo.command));
    }
    if !binary_available(&toolchains.tools.zig) {
        issues.push(format!("zig tool unavailable: {}", toolchains.tools.zig.command));
    }
    if env::var(&toolchains.tools.clang.env)
        .ok()
        .filter(|value| !value.is_empty())
        .is_none()
    {
        issues.push(format!(
            "missing clang environment variable: {}",
            toolchains.tools.clang.env
        ));
    }

    for component in &pairing.pairing_components {
        for dependency in &component.depends_on_components {
            if !component_map.contains_key(dependency.as_str()) {
                issues.push(format!(
                    "{} depends on missing component '{}'",
                    component.id, dependency
                ));
            }
        }

        for service_key in component
            .depends_on_services
            .iter()
            .chain(component.pairs_with_services.iter())
        {
            if !service_map.contains_key(service_key.as_str()) {
                issues.push(format!(
                    "{} references unknown native service '{}'",
                    component.id, service_key
                ));
            }
        }

        for relative in &component.inputs {
            let absolute = workspace_root.join(relative);
            if !absolute.exists() {
                issues.push(format!(
                    "{} declares missing input '{}'",
                    component.id,
                    absolute.display()
                ));
            }
        }
    }

    issues
}

fn build_json_summary(
    workspace_root: &Path,
    pairing: &PairingManifest,
    toolchains: &ToolchainConfig,
    native_metadata: &NativeRuntimeMetadata,
) -> serde_json::Value {
    json!({
        "native_runtime": {
            "name": native_metadata.runtime_name,
            "lane": native_metadata.runtime_lane,
            "runtime_version": native_metadata.version.runtime.string,
            "abi_version": native_metadata.version.abi.string,
            "compatibility_class": native_metadata.metadata.compatibility_class,
            "source_count": count_native_source_entries(&native_metadata.sources),
        },
        "toolchains": {
            "cargo": resolve_binary_status(&toolchains.tools.cargo),
            "zig": resolve_binary_status(&toolchains.tools.zig),
            "clang_env": toolchains.tools.clang.env,
            "clang_status": resolve_env_tool_status(&toolchains.tools.clang),
        },
        "outputs": {
            "report_dir": workspace_root.join(&toolchains.outputs.report_dir).display().to_string(),
            "rust_report": toolchains.outputs.rust_report,
            "zig_report": toolchains.outputs.zig_report,
            "combined_report": toolchains.outputs.combined_report,
        },
        "service_counts": {
            "total": native_metadata.services.len(),
            "available": count_services_by_status(&native_metadata.services, "available"),
            "planned": count_services_by_status(&native_metadata.services, "planned"),
        },
        "components": pairing.pairing_components.iter().map(|component| {
            json!({
                "id": component.id,
                "lane": component.lane,
                "status": component.status,
                "phases": component.phases,
                "pairs_with_services": component.pairs_with_services,
                "depends_on_services": component.depends_on_services,
            })
        }).collect::<Vec<_>>()
    })
}

fn format_component(component: &PairingComponent) -> String {
    let mut output = String::new();
    let _ = writeln!(output, "component: {}", component.id);
    let _ = writeln!(output, "lane: {}", component.lane);
    let _ = writeln!(output, "status: {}", component.status);
    let _ = writeln!(output, "phases: {:?}", component.phases);
    let _ = writeln!(output, "summary: {}", component.summary);
    let _ = writeln!(output, "pairs with: {}", component.pairs_with_services.join(", "));
    let _ = writeln!(
        output,
        "depends on services: {}",
        component.depends_on_services.join(", ")
    );
    let _ = writeln!(
        output,
        "depends on components: {}",
        component.depends_on_components.join(", ")
    );
    output
}

fn format_toolchains(toolchains: &ToolchainConfig) -> String {
    let mut output = String::new();
    let _ = writeln!(output, "cargo: {}", resolve_binary_status(&toolchains.tools.cargo));
    let _ = writeln!(output, "zig: {}", resolve_binary_status(&toolchains.tools.zig));
    let _ = writeln!(
        output,
        "clang({}): {}",
        toolchains.tools.clang.env,
        resolve_env_tool_status(&toolchains.tools.clang)
    );
    output
}

fn count_services_by_status(services: &[NativeServiceMetadata], status: &str) -> usize {
    services.iter().filter(|service| service.status == status).count()
}

fn count_native_source_entries(groups: &BTreeMap<String, Vec<String>>) -> usize {
    groups.values().map(Vec::len).sum()
}

fn count_components_by_lane(components: &[PairingComponent]) -> BTreeMap<&str, usize> {
    let mut counts = BTreeMap::new();
    for component in components {
        *counts.entry(component.lane.as_str()).or_insert(0) += 1;
    }
    counts
}

fn binary_available(tool: &ToolchainBinary) -> bool {
    if command_exists(&tool.command) {
        return true;
    }
    tool.fallback.iter().any(|candidate| command_exists(candidate))
}

fn resolve_binary_status(tool: &ToolchainBinary) -> String {
    if command_exists(&tool.command) {
        return format!("available ({})", tool.command);
    }
    if let Some(candidate) = tool.fallback.iter().find(|candidate| command_exists(candidate)) {
        return format!("available via fallback ({candidate})");
    }
    format!("missing ({})", tool.command)
}

fn resolve_env_tool_status(tool: &EnvTool) -> String {
    match env::var(&tool.env) {
        Ok(value) if !value.is_empty() => format!("available ({value})"),
        _ => "missing".to_string(),
    }
}

fn command_exists(command: &str) -> bool {
    if Path::new(command).exists() {
        return true;
    }

    let probe = if cfg!(target_os = "windows") {
        Command::new("where").arg(command).output()
    } else {
        Command::new("which").arg(command).output()
    };

    probe.map(|output| output.status.success()).unwrap_or(false)
}

#[derive(Debug, Deserialize)]
struct PairingManifest {
    native_runtime: PairingNativeRuntime,
    pairing_components: Vec<PairingComponent>,
}

#[derive(Debug, Deserialize)]
struct PairingNativeRuntime {
    manifest_path: String,
    metadata_path: String,
    tracker_path: String,
    spec_tasks_path: String,
}

#[derive(Debug, Deserialize)]
struct PairingComponent {
    id: String,
    lane: String,
    status: String,
    summary: String,
    phases: Vec<u32>,
    pairs_with_services: Vec<String>,
    depends_on_services: Vec<String>,
    depends_on_components: Vec<String>,
    inputs: Vec<String>,
}

#[derive(Debug, Deserialize)]
struct ToolchainConfig {
    tools: ToolchainTools,
    outputs: ToolchainOutputs,
}

#[derive(Debug, Deserialize)]
struct ToolchainTools {
    cargo: ToolchainBinary,
    zig: ToolchainBinary,
    clang: EnvTool,
}

#[derive(Debug, Deserialize)]
struct ToolchainBinary {
    command: String,
    #[serde(default)]
    fallback: Vec<String>,
}

#[derive(Debug, Deserialize)]
struct EnvTool {
    env: String,
}

#[derive(Debug, Deserialize)]
struct ToolchainOutputs {
    report_dir: String,
    rust_report: String,
    zig_report: String,
    combined_report: String,
}

#[derive(Debug, Deserialize)]
struct NativeRuntimeMetadata {
    runtime_name: String,
    runtime_lane: String,
    version: NativeVersionEnvelope,
    metadata: NativeRuntimeMetadataBlock,
    sources: BTreeMap<String, Vec<String>>,
    services: Vec<NativeServiceMetadata>,
}

#[derive(Debug, Deserialize)]
struct NativeVersionEnvelope {
    runtime: NativeVersionNumber,
    abi: NativeVersionNumber,
}

#[derive(Debug, Deserialize)]
struct NativeVersionNumber {
    string: String,
}

#[derive(Debug, Deserialize)]
struct NativeRuntimeMetadataBlock {
    compatibility_class: String,
}

#[derive(Debug, Deserialize)]
struct NativeServiceMetadata {
    key: String,
    status: String,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn counts_component_lanes() {
        let components = [
            PairingComponent {
                id: "a".to_string(),
                lane: "rust".to_string(),
                status: "active".to_string(),
                summary: String::new(),
                phases: vec![1],
                pairs_with_services: vec![],
                depends_on_services: vec![],
                depends_on_components: vec![],
                inputs: vec![],
            },
            PairingComponent {
                id: "b".to_string(),
                lane: "rust".to_string(),
                status: "planned".to_string(),
                summary: String::new(),
                phases: vec![3],
                pairs_with_services: vec![],
                depends_on_services: vec![],
                depends_on_components: vec![],
                inputs: vec![],
            },
            PairingComponent {
                id: "c".to_string(),
                lane: "zig".to_string(),
                status: "active".to_string(),
                summary: String::new(),
                phases: vec![5],
                pairs_with_services: vec![],
                depends_on_services: vec![],
                depends_on_components: vec![],
                inputs: vec![],
            },
        ];
        let counts = count_components_by_lane(&components);

        assert_eq!(counts.get("rust"), Some(&2));
        assert_eq!(counts.get("zig"), Some(&1));
    }

    #[test]
    fn counts_services_by_status() {
        let services = vec![
            NativeServiceMetadata {
                key: "a".to_string(),
                status: "available".to_string(),
            },
            NativeServiceMetadata {
                key: "b".to_string(),
                status: "planned".to_string(),
            },
            NativeServiceMetadata {
                key: "c".to_string(),
                status: "planned".to_string(),
            },
        ];

        assert_eq!(count_services_by_status(&services, "planned"), 2);
        assert_eq!(count_services_by_status(&services, "available"), 1);
    }

    #[test]
    fn counts_native_sources_across_groups() {
        let groups = BTreeMap::from([
            ("core".to_string(), vec!["a".to_string(), "b".to_string()]),
            ("ui".to_string(), vec!["c".to_string()]),
        ]);

        assert_eq!(count_native_source_entries(&groups), 3);
    }

    #[test]
    fn respects_fallback_tool_resolution() {
        let tool = ToolchainBinary {
            command: "__definitely_missing_tool__".to_string(),
            fallback: vec!["cargo".to_string()],
        };

        assert!(binary_available(&tool));
    }
}
