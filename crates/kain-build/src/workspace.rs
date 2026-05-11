use kain_blades::{
    discover_workspace, load_kain_manifest, BladeError, BladeWorkspace, KainBuildTaskSection,
    KainManifest, ResolvedBlade, ResolvedCffiLibrary, FABRIC_MANIFEST_NAME,
};
use kain_core::CompileTarget;
use kain_fs as kfs;
use kain_omni::fabric::{FabricRuntimeKind, FabricSessionStatus};
use serde::{Deserialize, Serialize};
use serde_json::json;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{SystemTime, UNIX_EPOCH};

const DEFAULT_PROFILE: &str = "debug";
const DEFAULT_ARTIFACT_ROOT: &str = ".kain/build";
const DEFAULT_CACHE_ROOT: &str = ".kain/cache/build";
const DEFAULT_REPORT_ROOT: &str = ".kain/reports/build";
const BUILD_ADAPTER_VERSION: &str = "blade-build-v1";

pub type BuildResult<T> = Result<T, BuildError>;

#[derive(Debug, thiserror::Error)]
pub enum BuildError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("filesystem error: {0}")]
    Fs(#[from] kain_fs::FsError),
    #[error("blade workspace error: {0}")]
    Blade(#[from] BladeError),
    #[error("Fabric error: {0}")]
    Omni(#[from] kain_omni::OmniError),
    #[error("Kain error: {0}")]
    Kain(#[from] kain_core::KainError),
    #[error("{0}")]
    Config(String),
    #[error("command failed: {0}")]
    Command(String),
}

#[derive(Debug, Clone)]
pub struct BladeBuildOptions {
    pub path: PathBuf,
    pub profile: Option<String>,
    pub target: Option<String>,
    pub dry_run: bool,
    pub clean: bool,
    pub include_vulkan: bool,
    pub fail_fast: bool,
}

impl BladeBuildOptions {
    pub fn new(path: impl Into<PathBuf>) -> Self {
        Self {
            path: path.into(),
            profile: None,
            target: None,
            dry_run: false,
            clean: false,
            include_vulkan: false,
            fail_fast: true,
        }
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct BladeBuildPlan {
    pub workspace_root: PathBuf,
    pub artifact_root: PathBuf,
    pub cache_root: PathBuf,
    pub report_root: PathBuf,
    pub profile: String,
    pub target: String,
    pub tasks: Vec<BuildTask>,
}

#[derive(Debug, Clone, Serialize)]
pub struct BuildTask {
    pub id: String,
    pub kind: BuildTaskKind,
    pub blade: Option<String>,
    pub description: String,
    pub depends_on: Vec<String>,
    pub inputs: Vec<PathBuf>,
    pub outputs: Vec<PathBuf>,
    pub cacheable: bool,
    #[serde(skip)]
    adapter: BuildTaskAdapter,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
#[serde(rename_all = "kebab-case")]
pub enum BuildTaskKind {
    BladeCheck,
    KainCheck,
    CargoBuild,
    CSharedLibrary,
    GpuArtifacts,
    FabricValidate,
    FabricRun,
    Node,
    Bun,
}

impl BuildTaskKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::BladeCheck => "blade-check",
            Self::KainCheck => "kain-check",
            Self::CargoBuild => "cargo-build",
            Self::CSharedLibrary => "c-shared-library",
            Self::GpuArtifacts => "gpu-artifacts",
            Self::FabricValidate => "fabric-validate",
            Self::FabricRun => "fabric-run",
            Self::Node => "node",
            Self::Bun => "bun",
        }
    }
}

#[derive(Debug, Clone)]
enum BuildTaskAdapter {
    BladeCheck,
    KainCheck {
        entry: PathBuf,
        target: CompileTarget,
    },
    CargoBuild {
        manifest_path: PathBuf,
        target_dir: PathBuf,
        release: bool,
    },
    CSharedLibrary {
        library_name: String,
        sources: Vec<PathBuf>,
        header: PathBuf,
        include_paths: Vec<PathBuf>,
        defines: Vec<String>,
        cpp_options: Vec<String>,
        canonical_output: PathBuf,
        materialized_output: Option<PathBuf>,
    },
    GpuArtifacts {
        source: PathBuf,
        output_base: PathBuf,
    },
    Fabric {
        manifest_path: PathBuf,
        run: bool,
    },
    NodeLike {
        runtime: NodeRuntimeKind,
        entry: Option<PathBuf>,
        command: Option<String>,
        args: Vec<String>,
        cwd: PathBuf,
    },
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
enum NodeRuntimeKind {
    Node,
    Bun,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BladeBuildReport {
    pub workspace_root: PathBuf,
    pub artifact_root: PathBuf,
    pub cache_root: PathBuf,
    pub report_path: PathBuf,
    pub events_path: PathBuf,
    pub profile: String,
    pub target: String,
    pub status: BladeBuildStatus,
    pub started_unix_ms: u128,
    pub finished_unix_ms: u128,
    pub dry_run: bool,
    pub tasks: Vec<BuildTaskExecution>,
}

impl BladeBuildReport {
    pub fn is_success(&self) -> bool {
        self.status == BladeBuildStatus::Succeeded
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum BladeBuildStatus {
    Succeeded,
    Failed,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BuildTaskExecution {
    pub id: String,
    pub kind: BuildTaskKind,
    pub blade: Option<String>,
    pub status: BuildTaskStatus,
    pub cache_hit: bool,
    pub started_unix_ms: Option<u128>,
    pub finished_unix_ms: Option<u128>,
    pub inputs: Vec<PathBuf>,
    pub outputs: Vec<PathBuf>,
    pub message: String,
    pub error: Option<String>,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum BuildTaskStatus {
    Planned,
    Cached,
    Succeeded,
    Failed,
    Skipped,
}

pub fn plan_blade_workspace(options: &BladeBuildOptions) -> BuildResult<BladeBuildPlan> {
    let workspace = discover_workspace(&options.path)?;
    let root_manifest = workspace
        .manifest_path
        .as_ref()
        .map(|path| load_kain_manifest(path))
        .transpose()?;
    let config = BuildWorkspaceConfig::from_workspace(&workspace, root_manifest.as_ref(), options);

    let mut tasks = Vec::new();
    let mut sidecar_task_ids = Vec::new();
    let mut fabric_validate_task_ids = Vec::new();

    for blade in &workspace.blades {
        add_c_tasks(&mut tasks, &mut sidecar_task_ids, &config, blade)?;
        add_cargo_task(&mut tasks, &mut sidecar_task_ids, &config, blade)?;
        add_gpu_tasks(&mut tasks, &config, blade);
        add_explicit_blade_tasks(&mut tasks, &mut sidecar_task_ids, &config, blade)?;
    }

    if !workspace.blades.is_empty() {
        tasks.push(BuildTask {
            id: "blade-check".to_string(),
            kind: BuildTaskKind::BladeCheck,
            blade: None,
            description: "Validate blade manifests and materialized local artifacts".to_string(),
            depends_on: sidecar_task_ids.clone(),
            inputs: workspace
                .blades
                .iter()
                .filter_map(|blade| blade.manifest_path.clone())
                .collect(),
            outputs: Vec::new(),
            cacheable: false,
            adapter: BuildTaskAdapter::BladeCheck,
        });
    }

    let fabric_manifests = discover_fabric_manifests(&workspace)?;
    for manifest_path in fabric_manifests {
        let validate_id = format!(
            "fabric-validate:{}",
            sanitize_id(&path_stem_or_name(&manifest_path))
        );
        let mut depends_on = sidecar_task_ids.clone();
        if !workspace.blades.is_empty() {
            depends_on.push("blade-check".to_string());
        }
        tasks.push(BuildTask {
            id: validate_id.clone(),
            kind: BuildTaskKind::FabricValidate,
            blade: None,
            description: format!("Validate Fabric manifest {}", manifest_path.display()),
            depends_on,
            inputs: vec![manifest_path.clone()],
            outputs: Vec::new(),
            cacheable: false,
            adapter: BuildTaskAdapter::Fabric {
                manifest_path: manifest_path.clone(),
                run: false,
            },
        });
        fabric_validate_task_ids.push(validate_id.clone());

        if should_run_fabric_manifest(&manifest_path, options.include_vulkan)? {
            tasks.push(BuildTask {
                id: format!(
                    "fabric-run:{}",
                    sanitize_id(&path_stem_or_name(&manifest_path))
                ),
                kind: BuildTaskKind::FabricRun,
                blade: None,
                description: format!("Run Fabric manifest {}", manifest_path.display()),
                depends_on: vec![validate_id],
                inputs: vec![manifest_path.clone()],
                outputs: Vec::new(),
                cacheable: false,
                adapter: BuildTaskAdapter::Fabric {
                    manifest_path,
                    run: true,
                },
            });
        }
    }

    if let Some(manifest) = root_manifest.as_ref() {
        add_explicit_root_tasks(&mut tasks, &config, manifest)?;
    }

    let tasks = order_tasks(tasks)?;
    Ok(BladeBuildPlan {
        workspace_root: config.workspace_root,
        artifact_root: config.artifact_root,
        cache_root: config.cache_root,
        report_root: config.report_root,
        profile: config.profile,
        target: config.target,
        tasks,
    })
}

pub fn build_blade_workspace(options: &BladeBuildOptions) -> BuildResult<BladeBuildReport> {
    let plan = plan_blade_workspace(options)?;
    execute_plan(plan, options)
}

struct BuildWorkspaceConfig {
    workspace_root: PathBuf,
    artifact_root: PathBuf,
    cache_root: PathBuf,
    report_root: PathBuf,
    profile: String,
    target: String,
}

impl BuildWorkspaceConfig {
    fn from_workspace(
        workspace: &BladeWorkspace,
        manifest: Option<&KainManifest>,
        options: &BladeBuildOptions,
    ) -> Self {
        let workspace_root = workspace.root.clone();
        let profile = options
            .profile
            .clone()
            .or_else(|| manifest.and_then(|value| value.build.profile.clone()))
            .unwrap_or_else(|| DEFAULT_PROFILE.to_string());
        let target = options.target.clone().unwrap_or_else(default_target_name);
        let artifact_root = resolve_workspace_path(
            &workspace_root,
            manifest
                .and_then(|value| value.build.artifact_root.as_ref())
                .map(PathBuf::as_path)
                .unwrap_or_else(|| Path::new(DEFAULT_ARTIFACT_ROOT)),
        );
        let cache_root = resolve_workspace_path(
            &workspace_root,
            manifest
                .and_then(|value| value.build.cache_root.as_ref())
                .map(PathBuf::as_path)
                .unwrap_or_else(|| Path::new(DEFAULT_CACHE_ROOT)),
        );
        let report_root = workspace_root.join(DEFAULT_REPORT_ROOT);
        Self {
            workspace_root,
            artifact_root,
            cache_root,
            report_root,
            profile,
            target,
        }
    }

    fn task_root(&self, blade_name: &str, task_id: &str) -> PathBuf {
        self.artifact_root
            .join(&self.profile)
            .join(&self.target)
            .join(sanitize_id(blade_name))
            .join(sanitize_id(task_id))
    }
}

fn add_c_tasks(
    tasks: &mut Vec<BuildTask>,
    sidecar_task_ids: &mut Vec<String>,
    config: &BuildWorkspaceConfig,
    blade: &ResolvedBlade,
) -> BuildResult<()> {
    for library in &blade.c_ffi_libraries {
        let Some(materialized_output) = &library.shared_lib else {
            continue;
        };
        let task_id = format!(
            "c:{}:{}",
            sanitize_id(&blade.name),
            sanitize_id(&library.name)
        );
        let task_root = config.task_root(&blade.name, &task_id);
        let canonical_output =
            task_root.join(platform_dynamic_library_name(&library.name).as_str());
        let sources = resolve_c_sources(library);
        let mut inputs = vec![library.header.clone()];
        inputs.extend(sources.iter().cloned());
        inputs.extend(library.include_paths.iter().cloned());
        let outputs = vec![canonical_output.clone(), materialized_output.clone()];
        tasks.push(BuildTask {
            id: task_id.clone(),
            kind: BuildTaskKind::CSharedLibrary,
            blade: Some(blade.name.clone()),
            description: format!("Build C ABI shared library {}", library.name),
            depends_on: Vec::new(),
            inputs,
            outputs,
            cacheable: true,
            adapter: BuildTaskAdapter::CSharedLibrary {
                library_name: library.name.clone(),
                sources,
                header: library.header.clone(),
                include_paths: library.include_paths.clone(),
                defines: library.defines.clone(),
                cpp_options: library.cpp_options.clone(),
                canonical_output,
                materialized_output: Some(materialized_output.clone()),
            },
        });
        sidecar_task_ids.push(task_id);
    }
    Ok(())
}

fn add_cargo_task(
    tasks: &mut Vec<BuildTask>,
    sidecar_task_ids: &mut Vec<String>,
    config: &BuildWorkspaceConfig,
    blade: &ResolvedBlade,
) -> BuildResult<()> {
    let Some(manifest_path) = &blade.cargo_manifest else {
        return Ok(());
    };
    let task_id = format!("cargo:{}", sanitize_id(&blade.name));
    let target_dir = config.task_root(&blade.name, &task_id).join("cargo-target");
    let crate_root = manifest_path.parent().unwrap_or(&blade.root).to_path_buf();
    let src_root = crate_root.join("src");
    let mut inputs = vec![manifest_path.clone()];
    if src_root.exists() {
        inputs.push(src_root);
    }
    tasks.push(BuildTask {
        id: task_id.clone(),
        kind: BuildTaskKind::CargoBuild,
        blade: Some(blade.name.clone()),
        description: format!("Build Cargo blade {}", blade.name),
        depends_on: Vec::new(),
        inputs,
        outputs: vec![target_dir.clone()],
        cacheable: true,
        adapter: BuildTaskAdapter::CargoBuild {
            manifest_path: manifest_path.clone(),
            target_dir,
            release: config.profile == "release",
        },
    });
    sidecar_task_ids.push(task_id);
    Ok(())
}

fn add_gpu_tasks(tasks: &mut Vec<BuildTask>, config: &BuildWorkspaceConfig, blade: &ResolvedBlade) {
    for source in &blade.gpu_shader_sources {
        let task_id = format!(
            "gpu:{}:{}",
            sanitize_id(&blade.name),
            sanitize_id(&path_stem_or_name(source))
        );
        let output_base = config
            .task_root(&blade.name, &task_id)
            .join(path_stem_or_name(source));
        let outputs = gpu_output_paths(&output_base);
        tasks.push(BuildTask {
            id: task_id,
            kind: BuildTaskKind::GpuArtifacts,
            blade: Some(blade.name.clone()),
            description: format!("Emit GPU artifacts for {}", source.display()),
            depends_on: Vec::new(),
            inputs: vec![source.clone()],
            outputs,
            cacheable: true,
            adapter: BuildTaskAdapter::GpuArtifacts {
                source: source.clone(),
                output_base,
            },
        });
    }
}

fn add_explicit_blade_tasks(
    tasks: &mut Vec<BuildTask>,
    sidecar_task_ids: &mut Vec<String>,
    config: &BuildWorkspaceConfig,
    blade: &ResolvedBlade,
) -> BuildResult<()> {
    let Some(manifest_path) = &blade.kain_manifest else {
        return Ok(());
    };
    let manifest = load_kain_manifest(manifest_path)?;
    for task in &manifest.build.tasks {
        let resolved = build_explicit_task(config, Some(blade), &blade.root, task)?;
        if matches!(
            resolved.kind,
            BuildTaskKind::CSharedLibrary | BuildTaskKind::CargoBuild
        ) {
            sidecar_task_ids.push(resolved.id.clone());
        }
        tasks.push(resolved);
    }
    Ok(())
}

fn add_explicit_root_tasks(
    tasks: &mut Vec<BuildTask>,
    config: &BuildWorkspaceConfig,
    manifest: &KainManifest,
) -> BuildResult<()> {
    for task in &manifest.build.tasks {
        tasks.push(build_explicit_task(
            config,
            None,
            &config.workspace_root,
            task,
        )?);
    }
    Ok(())
}

fn build_explicit_task(
    config: &BuildWorkspaceConfig,
    blade: Option<&ResolvedBlade>,
    root: &Path,
    task: &KainBuildTaskSection,
) -> BuildResult<BuildTask> {
    if task.id.trim().is_empty() {
        return Err(BuildError::Config(
            "explicit build task requires a non-empty id".to_string(),
        ));
    }
    let kind = match task.kind.trim().to_ascii_lowercase().as_str() {
        "kain" | "kain-check" | "check" => BuildTaskKind::KainCheck,
        "cargo" | "rust" | "rust-crate" => BuildTaskKind::CargoBuild,
        "c" | "c-shared-library" | "c_ffi" => BuildTaskKind::CSharedLibrary,
        "gpu" | "gpu-artifacts" => BuildTaskKind::GpuArtifacts,
        "fabric" | "fabric-run" => BuildTaskKind::FabricRun,
        "fabric-validate" => BuildTaskKind::FabricValidate,
        "node" => BuildTaskKind::Node,
        "bun" => BuildTaskKind::Bun,
        other => {
            return Err(BuildError::Config(format!(
                "explicit build task '{}' has unsupported kind '{}'",
                task.id, other
            )));
        }
    };
    let task_id = if let Some(blade) = blade {
        format!("{}:{}", sanitize_id(&blade.name), sanitize_id(&task.id))
    } else {
        sanitize_id(&task.id)
    };
    let task_root = config.task_root(
        blade
            .map(|value| value.name.as_str())
            .unwrap_or("workspace"),
        &task_id,
    );
    let inputs = task
        .inputs
        .iter()
        .map(|path| resolve_workspace_path(root, path))
        .collect();
    let outputs = if task.outputs.is_empty() {
        vec![task_root]
    } else {
        task.outputs
            .iter()
            .map(|path| resolve_workspace_path(root, path))
            .collect()
    };
    let adapter = match kind {
        BuildTaskKind::KainCheck => {
            let entry = task.entry.as_ref().ok_or_else(|| {
                BuildError::Config(format!("Kain build task '{}' requires entry", task.id))
            })?;
            let target = task
                .target
                .as_deref()
                .and_then(CompileTarget::from_str)
                .unwrap_or(CompileTarget::Interpret);
            BuildTaskAdapter::KainCheck {
                entry: resolve_workspace_path(root, entry),
                target,
            }
        }
        BuildTaskKind::CargoBuild => {
            let manifest_path =
                task.manifest
                    .as_ref()
                    .or(task.entry.as_ref())
                    .ok_or_else(|| {
                        BuildError::Config(format!(
                            "Cargo build task '{}' requires manifest",
                            task.id
                        ))
                    })?;
            BuildTaskAdapter::CargoBuild {
                manifest_path: resolve_workspace_path(root, manifest_path),
                target_dir: outputs[0].clone(),
                release: task.profile.as_deref().unwrap_or(&config.profile) == "release",
            }
        }
        BuildTaskKind::CSharedLibrary => {
            let sources = task
                .inputs
                .iter()
                .filter(|path| path.extension().and_then(|value| value.to_str()) == Some("c"))
                .map(|path| resolve_workspace_path(root, path))
                .collect::<Vec<_>>();
            let header = task
                .entry
                .as_ref()
                .or_else(|| {
                    task.inputs
                        .iter()
                        .find(|path| path.extension().and_then(|value| value.to_str()) == Some("h"))
                })
                .map(|path| resolve_workspace_path(root, path))
                .unwrap_or_else(|| root.to_path_buf());
            let canonical_output = outputs.first().cloned().ok_or_else(|| {
                BuildError::Config(format!("C build task '{}' requires an output", task.id))
            })?;
            BuildTaskAdapter::CSharedLibrary {
                library_name: task.id.clone(),
                sources,
                header,
                include_paths: Vec::new(),
                defines: Vec::new(),
                cpp_options: task.args.clone(),
                canonical_output,
                materialized_output: None,
            }
        }
        BuildTaskKind::GpuArtifacts => {
            let source = task
                .entry
                .as_ref()
                .or_else(|| task.inputs.first())
                .ok_or_else(|| {
                    BuildError::Config(format!("GPU build task '{}' requires entry", task.id))
                })?;
            let source = resolve_workspace_path(root, source);
            let output_base = outputs.first().cloned().unwrap_or_else(|| {
                config
                    .task_root("workspace", &task_id)
                    .join(path_stem_or_name(&source))
            });
            BuildTaskAdapter::GpuArtifacts {
                source,
                output_base,
            }
        }
        BuildTaskKind::FabricValidate | BuildTaskKind::FabricRun => {
            let manifest_path =
                task.manifest
                    .as_ref()
                    .or(task.entry.as_ref())
                    .ok_or_else(|| {
                        BuildError::Config(format!(
                            "Fabric build task '{}' requires manifest",
                            task.id
                        ))
                    })?;
            BuildTaskAdapter::Fabric {
                manifest_path: resolve_workspace_path(root, manifest_path),
                run: kind == BuildTaskKind::FabricRun,
            }
        }
        BuildTaskKind::Node | BuildTaskKind::Bun => {
            let runtime = if kind == BuildTaskKind::Bun {
                NodeRuntimeKind::Bun
            } else {
                NodeRuntimeKind::Node
            };
            BuildTaskAdapter::NodeLike {
                runtime,
                entry: task
                    .entry
                    .as_ref()
                    .map(|path| resolve_workspace_path(root, path)),
                command: task.command.clone(),
                args: task.args.clone(),
                cwd: task
                    .cwd
                    .as_ref()
                    .map(|path| resolve_workspace_path(root, path))
                    .unwrap_or_else(|| root.to_path_buf()),
            }
        }
        _ => unreachable!("explicit task kinds are filtered above"),
    };
    let outputs = match &adapter {
        BuildTaskAdapter::GpuArtifacts { output_base, .. } => gpu_output_paths(output_base),
        _ => outputs,
    };
    Ok(BuildTask {
        id: task_id,
        kind,
        blade: blade
            .map(|value| value.name.clone())
            .or_else(|| task.blade.clone()),
        description: format!("Explicit build task {}", task.id),
        depends_on: task
            .depends_on
            .iter()
            .map(|value| sanitize_id(value))
            .collect(),
        inputs,
        outputs,
        cacheable: !matches!(kind, BuildTaskKind::Node | BuildTaskKind::Bun),
        adapter,
    })
}

fn execute_plan(
    plan: BladeBuildPlan,
    options: &BladeBuildOptions,
) -> BuildResult<BladeBuildReport> {
    if options.clean && !options.dry_run {
        clean_build_roots(&plan)?;
    }
    kfs::create_dir_all(&plan.report_root)?;
    let started_unix_ms = unix_timestamp_ms();
    let report_path = plan.report_root.join(format!(
        "session-{started_unix_ms}-{}.json",
        std::process::id()
    ));
    let events_path = report_path.with_extension("jsonl");
    let mut event_writer = EventWriter::new(&events_path)?;
    let mut executions = Vec::new();
    let mut failed = false;
    let workspace = discover_workspace(&plan.workspace_root)?;

    for task in &plan.tasks {
        let execution = if options.dry_run {
            BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Planned,
                cache_hit: false,
                started_unix_ms: None,
                finished_unix_ms: None,
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                message: task.description.clone(),
                error: None,
            }
        } else {
            execute_task(task, &plan, &workspace)?
        };
        event_writer.write(&execution)?;
        if execution.status == BuildTaskStatus::Failed {
            failed = true;
            if options.fail_fast {
                executions.push(execution);
                break;
            }
        }
        executions.push(execution);
    }

    if failed && !options.fail_fast {
        for task in plan.tasks.iter().skip(executions.len()) {
            executions.push(BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Skipped,
                cache_hit: false,
                started_unix_ms: None,
                finished_unix_ms: None,
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                message: "skipped after previous failure".to_string(),
                error: None,
            });
        }
    }

    let finished_unix_ms = unix_timestamp_ms();
    let status = if failed {
        BladeBuildStatus::Failed
    } else {
        BladeBuildStatus::Succeeded
    };
    let report = BladeBuildReport {
        workspace_root: plan.workspace_root,
        artifact_root: plan.artifact_root,
        cache_root: plan.cache_root,
        report_path: report_path.clone(),
        events_path,
        profile: plan.profile,
        target: plan.target,
        status,
        started_unix_ms,
        finished_unix_ms,
        dry_run: options.dry_run,
        tasks: executions,
    };
    let encoded = serde_json::to_string_pretty(&report)
        .map_err(|err| BuildError::Config(format!("failed to serialize build report: {err}")))?;
    kfs::atomic_write_text(&report_path, &encoded)?;
    if failed {
        Err(BuildError::Command(format!(
            "blade build failed; report written to {}",
            report_path.display()
        )))
    } else {
        Ok(report)
    }
}

fn execute_task(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    workspace: &BladeWorkspace,
) -> BuildResult<BuildTaskExecution> {
    let started_unix_ms = unix_timestamp_ms();
    if task.cacheable && task_is_cached(task, plan)? {
        return Ok(BuildTaskExecution {
            id: task.id.clone(),
            kind: task.kind,
            blade: task.blade.clone(),
            status: BuildTaskStatus::Cached,
            cache_hit: true,
            started_unix_ms: Some(started_unix_ms),
            finished_unix_ms: Some(unix_timestamp_ms()),
            inputs: task.inputs.clone(),
            outputs: task.outputs.clone(),
            message: "cache hit".to_string(),
            error: None,
        });
    }

    let result = match &task.adapter {
        BuildTaskAdapter::BladeCheck => run_blade_check(workspace),
        BuildTaskAdapter::KainCheck { entry, target } => run_kain_check(entry, *target),
        BuildTaskAdapter::CargoBuild {
            manifest_path,
            target_dir,
            release,
        } => run_cargo_build(manifest_path, target_dir, *release),
        BuildTaskAdapter::CSharedLibrary {
            library_name,
            sources,
            header,
            include_paths,
            defines,
            cpp_options,
            canonical_output,
            materialized_output,
        } => run_c_shared_library(
            &plan.workspace_root,
            library_name,
            sources,
            header,
            include_paths,
            defines,
            cpp_options,
            canonical_output,
            materialized_output.as_ref(),
        ),
        BuildTaskAdapter::GpuArtifacts {
            source,
            output_base,
        } => run_gpu_artifacts(source, output_base),
        BuildTaskAdapter::Fabric { manifest_path, run } => run_fabric(manifest_path, *run),
        BuildTaskAdapter::NodeLike {
            runtime,
            entry,
            command,
            args,
            cwd,
        } => run_node_like(*runtime, entry.as_ref(), command.as_ref(), args, cwd),
    };

    let finished_unix_ms = unix_timestamp_ms();
    match result {
        Ok(message) => {
            if task.cacheable {
                write_task_stamp(task, plan)?;
            }
            Ok(BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Succeeded,
                cache_hit: false,
                started_unix_ms: Some(started_unix_ms),
                finished_unix_ms: Some(finished_unix_ms),
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                message,
                error: None,
            })
        }
        Err(error) => Ok(BuildTaskExecution {
            id: task.id.clone(),
            kind: task.kind,
            blade: task.blade.clone(),
            status: BuildTaskStatus::Failed,
            cache_hit: false,
            started_unix_ms: Some(started_unix_ms),
            finished_unix_ms: Some(finished_unix_ms),
            inputs: task.inputs.clone(),
            outputs: task.outputs.clone(),
            message: "task failed".to_string(),
            error: Some(error.to_string()),
        }),
    }
}

fn run_blade_check(workspace: &BladeWorkspace) -> BuildResult<String> {
    let missing = missing_blade_paths(workspace);
    if missing.is_empty() && workspace.diagnostics.is_empty() {
        Ok("all referenced local blade paths exist".to_string())
    } else {
        let mut message = String::new();
        for diagnostic in &workspace.diagnostics {
            message.push_str(&format!(
                "{}: {}\n",
                diagnostic.severity, diagnostic.message
            ));
        }
        for missing in missing {
            message.push_str(&format!(
                "{} {} -> {}\n",
                missing.blade,
                missing.field,
                missing.path.display()
            ));
        }
        Err(BuildError::Config(message))
    }
}

fn run_kain_check(entry: &Path, target: CompileTarget) -> BuildResult<String> {
    let options = kain_check::CheckOptions::new(target);
    let report = kain_check::check_file(entry, &options);
    if report.passed() {
        Ok(format!("checked {}", entry.display()))
    } else {
        Err(BuildError::Config(report.error.unwrap_or_else(|| {
            format!("Kain check failed for {}", entry.display())
        })))
    }
}

fn run_cargo_build(manifest_path: &Path, target_dir: &Path, release: bool) -> BuildResult<String> {
    kfs::create_dir_all(target_dir)?;
    let mut command = Command::new("cargo");
    command
        .arg("build")
        .arg("--manifest-path")
        .arg(manifest_path)
        .env("CARGO_TARGET_DIR", target_dir)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    if release {
        command.arg("--release");
    }
    run_command_capture(command, "cargo build")
}

#[allow(clippy::too_many_arguments)]
fn run_c_shared_library(
    workspace_root: &Path,
    library_name: &str,
    sources: &[PathBuf],
    header: &Path,
    include_paths: &[PathBuf],
    defines: &[String],
    cpp_options: &[String],
    canonical_output: &Path,
    materialized_output: Option<&PathBuf>,
) -> BuildResult<String> {
    if sources.is_empty() {
        return Err(BuildError::Config(format!(
            "C ABI library '{library_name}' has no sources; add sources = [...] beside header {}",
            header.display()
        )));
    }
    let clang = find_clang(workspace_root)?;
    if let Some(parent) = canonical_output.parent() {
        kfs::create_dir_all(parent)?;
    }
    let mut command = Command::new(&clang);
    command.arg("-shared").arg("-O2");
    if !cfg!(target_os = "windows") {
        command.arg("-fPIC");
    }
    let header_parent = header.parent().unwrap_or_else(|| Path::new("."));
    command.arg("-I").arg(header_parent);
    for include_path in include_paths {
        command.arg("-I").arg(include_path);
    }
    for define in defines {
        command.arg(format!("-D{define}"));
    }
    for option in cpp_options {
        command.arg(option);
    }
    for source in sources {
        command.arg(source);
    }
    command.arg("-o").arg(canonical_output);
    command.stdout(Stdio::piped()).stderr(Stdio::piped());
    let message = run_command_capture(command, "clang shared library build")?;
    if let Some(materialized_output) = materialized_output {
        if materialized_output != canonical_output {
            if let Some(parent) = materialized_output.parent() {
                kfs::create_dir_all(parent)?;
            }
            kfs::copy_file(canonical_output, materialized_output)?;
        }
    }
    Ok(message)
}

fn run_gpu_artifacts(source: &Path, output_base: &Path) -> BuildResult<String> {
    let source_text = kfs::read_text(source)?;
    let artifacts = kain_driver::compile_shader_artifact_bundle(&source_text)?;
    if let Some(parent) = output_base.parent() {
        kfs::create_dir_all(parent)?;
    }
    kfs::atomic_write_bytes(output_base.with_extension("spv"), &artifacts.spirv)?;
    kfs::atomic_write_text(
        with_file_name_suffix(output_base, ".gpu", "rs"),
        &artifacts.rust_host,
    )?;
    kfs::atomic_write_text(
        with_file_name_suffix(output_base, ".reflect", "json"),
        &artifacts.reflection_json,
    )?;
    kfs::atomic_write_text(
        with_file_name_suffix(output_base, ".shader_bundle", "json"),
        &artifacts.bundle_json,
    )?;
    if let Some(hlsl) = artifacts.derived_hlsl {
        kfs::atomic_write_text(output_base.with_extension("hlsl"), &hlsl)?;
    }
    Ok(format!("emitted GPU artifacts for {}", source.display()))
}

fn run_fabric(manifest_path: &Path, run: bool) -> BuildResult<String> {
    if run {
        let result = kain_host::fabric::execute_fabric_manifest_path(manifest_path)?;
        if result.status == FabricSessionStatus::Succeeded {
            Ok(format!(
                "Fabric run succeeded; report {}",
                result.report_path.display()
            ))
        } else {
            Err(BuildError::Command(format!(
                "Fabric run failed for {}; report {}",
                manifest_path.display(),
                result.report_path.display()
            )))
        }
    } else {
        let result = kain_omni::fabric::validate_fabric_manifest_path(manifest_path)?;
        Ok(format!(
            "validated Fabric manifest with {} steps",
            result.step_count
        ))
    }
}

fn run_node_like(
    runtime: NodeRuntimeKind,
    entry: Option<&PathBuf>,
    command: Option<&String>,
    args: &[String],
    cwd: &Path,
) -> BuildResult<String> {
    let program = command.cloned().unwrap_or_else(|| match runtime {
        NodeRuntimeKind::Node => "node".to_string(),
        NodeRuntimeKind::Bun => "bun".to_string(),
    });
    let mut process = Command::new(&program);
    process
        .current_dir(cwd)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    match (runtime, entry, command) {
        (NodeRuntimeKind::Node, Some(entry), None) => {
            process.arg("--check").arg(entry);
        }
        (NodeRuntimeKind::Bun, Some(entry), None) => {
            process.arg(entry);
        }
        _ => {
            for arg in args {
                process.arg(arg);
            }
            if let Some(entry) = entry {
                process.arg(entry);
            }
        }
    }
    run_command_capture(process, &format!("{program} task"))
}

fn run_command_capture(mut command: Command, label: &str) -> BuildResult<String> {
    let output = command
        .output()
        .map_err(|err| BuildError::Command(format!("failed to invoke {label}: {err}")))?;
    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout).trim().to_string();
        Ok(if stdout.is_empty() {
            format!("{label} succeeded")
        } else {
            stdout
        })
    } else {
        let stdout = String::from_utf8_lossy(&output.stdout);
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(BuildError::Command(format!(
            "{label} exited with status {}\n{}\n{}",
            output.status, stdout, stderr
        )))
    }
}

fn task_is_cached(task: &BuildTask, plan: &BladeBuildPlan) -> BuildResult<bool> {
    if task.outputs.is_empty() || !task.outputs.iter().all(|path| path.exists()) {
        return Ok(false);
    }
    let stamp_path = stamp_path(task, plan);
    if !stamp_path.exists() {
        return Ok(false);
    }
    let expected = task_stamp(task, plan)?;
    let actual = kfs::read_text(stamp_path)?;
    Ok(actual == expected)
}

fn write_task_stamp(task: &BuildTask, plan: &BladeBuildPlan) -> BuildResult<()> {
    let stamp_path = stamp_path(task, plan);
    if let Some(parent) = stamp_path.parent() {
        kfs::create_dir_all(parent)?;
    }
    let stamp = task_stamp(task, plan)?;
    kfs::atomic_write_text(stamp_path, &stamp)?;
    Ok(())
}

fn stamp_path(task: &BuildTask, plan: &BladeBuildPlan) -> PathBuf {
    plan.cache_root
        .join("stamps")
        .join(format!("{}.stamp", sanitize_id(&task.id)))
}

fn task_stamp(task: &BuildTask, plan: &BladeBuildPlan) -> BuildResult<String> {
    let mut hasher = Sha256::new();
    hasher.update(BUILD_ADAPTER_VERSION.as_bytes());
    hasher.update(plan.profile.as_bytes());
    hasher.update(plan.target.as_bytes());
    hasher.update(task.id.as_bytes());
    hasher.update(task.kind.as_str().as_bytes());
    hasher.update(format!("{:?}", task.adapter).as_bytes());
    for input in &task.inputs {
        hash_path_into(&mut hasher, input)?;
    }
    for output in &task.outputs {
        hasher.update(output.display().to_string().as_bytes());
    }
    Ok(format!("{:x}", hasher.finalize()))
}

fn hash_path_into(hasher: &mut Sha256, path: &Path) -> BuildResult<()> {
    hasher.update(path.display().to_string().as_bytes());
    if path.is_file() {
        hasher.update(kfs::hash_file(path)?.as_bytes());
    } else if path.is_dir() {
        for entry in kfs::read_dir_entries(path)? {
            let name = entry
                .path
                .file_name()
                .and_then(|value| value.to_str())
                .unwrap_or("");
            if matches!(name, ".kain" | "target" | "node_modules" | ".git") {
                continue;
            }
            hash_path_into(hasher, &entry.path)?;
        }
    }
    Ok(())
}

fn order_tasks(tasks: Vec<BuildTask>) -> BuildResult<Vec<BuildTask>> {
    let by_id = tasks
        .into_iter()
        .map(|task| (task.id.clone(), task))
        .collect::<BTreeMap<_, _>>();
    let mut ordered = Vec::new();
    let mut temporary = BTreeSet::new();
    let mut permanent = BTreeSet::new();
    for id in by_id.keys() {
        visit_task(id, &by_id, &mut temporary, &mut permanent, &mut ordered)?;
    }
    Ok(ordered)
}

fn visit_task(
    id: &str,
    by_id: &BTreeMap<String, BuildTask>,
    temporary: &mut BTreeSet<String>,
    permanent: &mut BTreeSet<String>,
    ordered: &mut Vec<BuildTask>,
) -> BuildResult<()> {
    if permanent.contains(id) {
        return Ok(());
    }
    if !temporary.insert(id.to_string()) {
        return Err(BuildError::Config(format!(
            "cycle detected in blade build task graph at '{id}'"
        )));
    }
    let task = by_id
        .get(id)
        .ok_or_else(|| BuildError::Config(format!("unknown blade build task '{id}'")))?;
    for dependency in &task.depends_on {
        if by_id.contains_key(dependency) {
            visit_task(dependency, by_id, temporary, permanent, ordered)?;
        } else {
            return Err(BuildError::Config(format!(
                "task '{}' depends on missing task '{}'",
                task.id, dependency
            )));
        }
    }
    temporary.remove(id);
    permanent.insert(id.to_string());
    ordered.push(task.clone());
    Ok(())
}

#[derive(Debug)]
struct MissingBladePath {
    blade: String,
    field: String,
    path: PathBuf,
}

fn missing_blade_paths(workspace: &BladeWorkspace) -> Vec<MissingBladePath> {
    let mut missing = Vec::new();
    for blade in &workspace.blades {
        check_optional_path(&mut missing, blade, "entry", blade.entry.as_ref());
        check_optional_path(
            &mut missing,
            blade,
            "kain_manifest",
            blade.kain_manifest.as_ref(),
        );
        check_optional_path(
            &mut missing,
            blade,
            "cargo_manifest",
            blade.cargo_manifest.as_ref(),
        );
        check_optional_path(
            &mut missing,
            blade,
            "fabric_manifest",
            blade.fabric_manifest.as_ref(),
        );
        for root in &blade.module_roots {
            check_path(&mut missing, blade, "module_root", root);
        }
        for library in &blade.c_ffi_libraries {
            check_path(&mut missing, blade, "c_ffi.header", &library.header);
            for source in &library.sources {
                check_path(&mut missing, blade, "c_ffi.source", source);
            }
            check_optional_path(
                &mut missing,
                blade,
                "c_ffi.shared_lib",
                library.shared_lib.as_ref(),
            );
        }
        for shader_source in &blade.gpu_shader_sources {
            check_path(&mut missing, blade, "gpu.shader_source", shader_source);
        }
        for shader_root in &blade.gpu_shader_roots {
            check_path(&mut missing, blade, "gpu.shader_root", shader_root);
        }
        for path in blade.artifacts.values() {
            check_path(&mut missing, blade, "artifact", path);
        }
    }
    missing
}

fn check_optional_path(
    missing: &mut Vec<MissingBladePath>,
    blade: &ResolvedBlade,
    field: &str,
    path: Option<&PathBuf>,
) {
    if let Some(path) = path {
        check_path(missing, blade, field, path);
    }
}

fn check_path(
    missing: &mut Vec<MissingBladePath>,
    blade: &ResolvedBlade,
    field: &str,
    path: &Path,
) {
    if !path.exists() {
        missing.push(MissingBladePath {
            blade: blade.name.clone(),
            field: field.to_string(),
            path: path.to_path_buf(),
        });
    }
}

fn discover_fabric_manifests(workspace: &BladeWorkspace) -> BuildResult<Vec<PathBuf>> {
    let mut manifests = BTreeSet::new();
    let root_conventional = workspace.root.join(FABRIC_MANIFEST_NAME);
    if root_conventional.exists() {
        manifests.insert(root_conventional);
    }
    for entry in kfs::read_dir_entries(&workspace.root)? {
        let path = entry.path;
        let Some(name) = path.file_name().and_then(|value| value.to_str()) else {
            continue;
        };
        if name.starts_with("KAIN") && name.ends_with(".fabric.toml") {
            manifests.insert(path);
        }
    }
    for blade in &workspace.blades {
        if let Some(path) = &blade.fabric_manifest {
            manifests.insert(path.clone());
        }
    }
    Ok(manifests.into_iter().collect())
}

fn should_run_fabric_manifest(path: &Path, include_vulkan: bool) -> BuildResult<bool> {
    let manifest = kain_omni::fabric::load_fabric_manifest(path)?;
    let has_gpu = manifest
        .steps
        .iter()
        .any(|step| step.runtime == FabricRuntimeKind::GpuCompute);
    Ok(!has_gpu || include_vulkan)
}

fn resolve_c_sources(library: &ResolvedCffiLibrary) -> Vec<PathBuf> {
    if !library.sources.is_empty() {
        return library.sources.clone();
    }
    let Some(parent) = library.header.parent() else {
        return Vec::new();
    };
    let Some(stem) = library.header.file_stem().and_then(|value| value.to_str()) else {
        return Vec::new();
    };
    let exact = parent.join(format!("{stem}.c"));
    if exact.exists() {
        return vec![exact];
    }
    let Ok(entries) = kfs::read_dir_entries(parent) else {
        return Vec::new();
    };
    let mut sources = entries
        .into_iter()
        .map(|entry| entry.path)
        .filter(|path| path.extension().and_then(|value| value.to_str()) == Some("c"))
        .collect::<Vec<_>>();
    sources.sort();
    sources
}

fn gpu_output_paths(output_base: &Path) -> Vec<PathBuf> {
    vec![
        output_base.with_extension("spv"),
        with_file_name_suffix(output_base, ".gpu", "rs"),
        with_file_name_suffix(output_base, ".reflect", "json"),
        with_file_name_suffix(output_base, ".shader_bundle", "json"),
        output_base.with_extension("hlsl"),
    ]
}

fn with_file_name_suffix(base: &Path, suffix: &str, extension: &str) -> PathBuf {
    let stem = base
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("artifact");
    base.with_file_name(format!("{stem}{suffix}.{extension}"))
}

fn find_clang(workspace_root: &Path) -> BuildResult<PathBuf> {
    if let Ok(path) = std::env::var("KAIN_CLANG_PATH") {
        let candidate = PathBuf::from(path);
        if candidate.exists() {
            return Ok(candidate);
        }
    }
    for ancestor in workspace_root.ancestors() {
        for relative in [
            "toolchain/llvm/bin/clang.exe",
            "toolchain/llvm/bin/clang",
            "third_party/llvm/bin/clang.exe",
            "third_party/llvm/bin/clang",
        ] {
            let candidate = ancestor.join(relative);
            if candidate.exists() {
                return Ok(candidate);
            }
        }
    }
    Ok(PathBuf::from("clang"))
}

fn clean_build_roots(plan: &BladeBuildPlan) -> BuildResult<()> {
    for root in [&plan.artifact_root, &plan.cache_root, &plan.report_root] {
        if root.exists() {
            ensure_safe_clean_root(&plan.workspace_root, root)?;
            kfs::remove_dir_all(root)?;
        }
    }
    Ok(())
}

fn ensure_safe_clean_root(workspace_root: &Path, root: &Path) -> BuildResult<()> {
    let workspace = PathBuf::from(kfs::canonicalize_path(workspace_root)?);
    let target = kfs::canonicalize_path(root)
        .map(PathBuf::from)
        .unwrap_or_else(|_| root.to_path_buf());
    if target == workspace || !target.starts_with(&workspace) {
        return Err(BuildError::Config(format!(
            "refusing to clean build path outside workspace: {}",
            root.display()
        )));
    }
    if !target
        .components()
        .any(|component| component.as_os_str() == std::ffi::OsStr::new(".kain"))
    {
        return Err(BuildError::Config(format!(
            "refusing to clean non-.kain build path: {}",
            root.display()
        )));
    }
    Ok(())
}

fn path_stem_or_name(path: &Path) -> String {
    path.file_stem()
        .or_else(|| path.file_name())
        .and_then(|value| value.to_str())
        .unwrap_or("artifact")
        .to_string()
}

fn platform_dynamic_library_name(name: &str) -> String {
    if cfg!(target_os = "windows") {
        format!("{name}.dll")
    } else if cfg!(target_os = "macos") {
        format!("lib{name}.dylib")
    } else {
        format!("lib{name}.so")
    }
}

fn sanitize_id(value: &str) -> String {
    let mut output = String::new();
    for ch in value.chars() {
        if ch.is_ascii_alphanumeric() || ch == '-' || ch == '_' {
            output.push(ch.to_ascii_lowercase());
        } else {
            output.push('-');
        }
    }
    while output.contains("--") {
        output = output.replace("--", "-");
    }
    output.trim_matches('-').to_string()
}

fn resolve_workspace_path(root: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        root.join(path)
    }
}

fn default_target_name() -> String {
    format!("{}-{}", std::env::consts::ARCH, std::env::consts::OS)
}

fn unix_timestamp_ms() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|value| value.as_millis())
        .unwrap_or(0)
}

struct EventWriter {
    path: PathBuf,
}

impl EventWriter {
    fn new(path: &Path) -> BuildResult<Self> {
        if let Some(parent) = path.parent() {
            kfs::create_dir_all(parent)?;
        }
        kfs::atomic_write_text(path, "")?;
        Ok(Self {
            path: path.to_path_buf(),
        })
    }

    fn write(&mut self, execution: &BuildTaskExecution) -> BuildResult<()> {
        let encoded = serde_json::to_string(&json!({
            "timestamp_unix_ms": unix_timestamp_ms(),
            "task": execution.id,
            "kind": execution.kind,
            "status": execution.status,
            "cache_hit": execution.cache_hit,
            "message": execution.message,
            "error": execution.error,
        }))
        .map_err(|err| BuildError::Config(format!("failed to serialize build event: {err}")))?;
        kfs::append_text(&self.path, &format!("{encoded}\n"))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn orders_dependencies_before_dependents() {
        let tasks = vec![
            test_task("b", vec!["a"]),
            test_task("a", Vec::new()),
            test_task("c", vec!["b"]),
        ];
        let ordered = order_tasks(tasks).unwrap();
        let ids = ordered.into_iter().map(|task| task.id).collect::<Vec<_>>();
        assert_eq!(ids, vec!["a", "b", "c"]);
    }

    #[test]
    fn detects_task_cycles() {
        let tasks = vec![test_task("a", vec!["b"]), test_task("b", vec!["a"])];
        assert!(order_tasks(tasks).is_err());
    }

    fn test_task(id: &str, depends_on: Vec<&str>) -> BuildTask {
        BuildTask {
            id: id.to_string(),
            kind: BuildTaskKind::BladeCheck,
            blade: None,
            description: id.to_string(),
            depends_on: depends_on.into_iter().map(str::to_string).collect(),
            inputs: Vec::new(),
            outputs: Vec::new(),
            cacheable: false,
            adapter: BuildTaskAdapter::BladeCheck,
        }
    }
}
