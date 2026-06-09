use crate::error::{KainError, KainResult};
use crate::llvm_native_stage::{stage_native_backend_artifacts, LlvmNativeArtifactStage};
use crate::selfhost_report::{
    render_bootstrap_markdown, SelfHostBootstrapArtifacts, SelfHostBootstrapReport,
    SelfHostBootstrapStepReport, SelfHostPhaseStatus,
};
use crate::{compile, CompileTarget};
use chrono::Utc;
use serde::Deserialize;
use std::collections::BTreeSet;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

const DEFAULT_BOOTSTRAP_MANIFEST: &str = "src/KAIN.toml";
const DEFAULT_RUNTIME_MANIFEST: &str = "runtime/native_core_runtime.toml";
const DEFAULT_RUNTIME_BUILD_SCRIPT: &str = "runtime/compile_native_runtime.sh";
const DEFAULT_COMBINED_SOURCE_PATH: &str =
    "src/.selfhost/bootstrap/combined/kain_core_bootstrap.kn";
const DEFAULT_C_OUTPUT_PATH: &str = "src/.selfhost/bootstrap/out/kain_core_bootstrap.c";
const DEFAULT_LLVM_OUTPUT_PATH: &str = "src/.selfhost/bootstrap/out/kain_core_bootstrap.ll";
const DEFAULT_NATIVE_OUTPUT_PATH: &str = "src/.selfhost/bootstrap/out/kainc";
const DEFAULT_OUROBOROS_C_OUTPUT_PATH: &str =
    "src/.selfhost/ouroboros/kain_core_bootstrap.stage2.c";
const DEFAULT_OUROBOROS_LLVM_OUTPUT_PATH: &str =
    "src/.selfhost/ouroboros/kain_core_bootstrap.stage2.ll";
const DEFAULT_JSON_REPORT_PATH: &str = "src/.selfhost/reports/bootstrap_report.json";
const DEFAULT_MARKDOWN_REPORT_PATH: &str = "src/.selfhost/reports/bootstrap_report.md";
const RUNTIME_BUILD_TYPE: &str = "debug";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum BootstrapMode {
    CombineOnly,
    EmitCodegenOnly,
    LinkNative,
    VerifyOuroboros,
}

impl BootstrapMode {
    fn label(self, backend: BootstrapBackend) -> &'static str {
        match (self, backend) {
            (Self::CombineOnly, _) => "combine_only",
            (Self::EmitCodegenOnly, BootstrapBackend::Llvm) => "emit_llvm_only",
            (Self::EmitCodegenOnly, BootstrapBackend::C) => "emit_c_only",
            (Self::LinkNative, BootstrapBackend::Llvm) => "link_native_llvm",
            (Self::LinkNative, BootstrapBackend::C) => "link_native_c",
            (Self::VerifyOuroboros, BootstrapBackend::Llvm) => "verify_ouroboros_llvm",
            (Self::VerifyOuroboros, BootstrapBackend::C) => "verify_ouroboros_c",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum BootstrapBackend {
    Llvm,
    C,
}

impl BootstrapBackend {
    fn parse(value: &str) -> Result<Self, String> {
        match value.trim().to_ascii_lowercase().as_str() {
            "llvm" => Ok(Self::Llvm),
            "c" => Ok(Self::C),
            other => Err(format!(
                "unsupported selfhost bootstrap backend '{other}'; expected 'llvm' or 'c'"
            )),
        }
    }

    fn compile_target(self) -> CompileTarget {
        match self {
            Self::Llvm => CompileTarget::Llvm,
            Self::C => CompileTarget::C,
        }
    }

    fn phase_name(self) -> &'static str {
        match self {
            Self::Llvm => "llvm_codegen",
            Self::C => "c_codegen",
        }
    }

    fn noun(self) -> &'static str {
        match self {
            Self::Llvm => "LLVM",
            Self::C => "C",
        }
    }
}

#[derive(Debug, Deserialize)]
struct BootstrapManifest {
    package: BootstrapPackage,
    build: BootstrapBuild,
    selfhost: BootstrapSelfHost,
    #[serde(default)]
    ffi: BootstrapFfi,
}

#[derive(Debug, Deserialize)]
struct BootstrapPackage {
    name: String,
    version: String,
}

#[derive(Debug, Deserialize)]
struct BootstrapBuild {
    entry: PathBuf,
    #[serde(default)]
    source_root: Option<PathBuf>,
    #[serde(default)]
    source_order: Vec<PathBuf>,
    #[serde(default)]
    module_roots: Vec<PathBuf>,
    #[serde(default, alias = "module_search_paths")]
    module_search_paths: Vec<PathBuf>,
    #[serde(default)]
    root_component: Option<String>,
}

#[derive(Debug, Deserialize)]
struct BootstrapSelfHost {
    #[serde(default)]
    mode: Option<String>,
    runtime: BootstrapRuntime,
    outputs: BootstrapOutputs,
}

#[derive(Debug, Deserialize)]
struct BootstrapRuntime {
    #[serde(default)]
    manifest_path: Option<PathBuf>,
    #[serde(default)]
    compile_script: Option<PathBuf>,
    #[serde(default)]
    cache_root: Option<PathBuf>,
}

#[derive(Debug, Deserialize)]
struct BootstrapOutputs {
    #[serde(default)]
    combined_source_path: Option<PathBuf>,
    #[serde(default)]
    c_output_path: Option<PathBuf>,
    #[serde(default)]
    llvm_output_path: Option<PathBuf>,
    #[serde(default)]
    native_output_path: Option<PathBuf>,
    #[serde(default)]
    json_report_path: Option<PathBuf>,
    #[serde(default)]
    markdown_report_path: Option<PathBuf>,
    #[serde(default)]
    ouroboros_c_path: Option<PathBuf>,
    #[serde(default)]
    ouroboros_llvm_path: Option<PathBuf>,
}

#[derive(Debug, Default, Deserialize)]
struct BootstrapFfi {
    #[serde(default)]
    shared_libraries: Vec<PathBuf>,
    #[serde(default)]
    link_libs: Vec<String>,
}

#[derive(Debug, Deserialize)]
struct NativeRuntimeManifest {
    #[serde(default = "default_runtime_name")]
    name: String,
    #[serde(default)]
    sources: Vec<PathBuf>,
    #[serde(default)]
    windows_sources: Vec<PathBuf>,
    #[serde(default)]
    linux_sources: Vec<PathBuf>,
    #[serde(default)]
    macos_sources: Vec<PathBuf>,
    #[serde(default)]
    archive_groups: Vec<NativeRuntimeArchiveGroupManifest>,
    #[serde(default)]
    link: NativeRuntimeLinkManifest,
}

#[derive(Debug, Deserialize)]
struct NativeRuntimeArchiveGroupManifest {
    name: String,
    #[serde(default)]
    source_prefixes: Vec<PathBuf>,
}

#[derive(Debug, Default, Deserialize)]
struct NativeRuntimeLinkManifest {
    #[serde(default)]
    windows: Vec<String>,
    #[serde(default)]
    linux: Vec<String>,
    #[serde(default)]
    macos: Vec<String>,
}

#[derive(Debug, Clone)]
struct BootstrapContract {
    repo_root: PathBuf,
    manifest_path: PathBuf,
    package_name: String,
    package_version: String,
    compiler_entry: PathBuf,
    bootstrap_host_mode: String,
    source_root: PathBuf,
    source_files: Vec<PathBuf>,
    module_roots: Vec<PathBuf>,
    search_paths: Vec<PathBuf>,
    runtime_manifest_path: PathBuf,
    runtime_build_script_path: PathBuf,
    runtime_cache_base: PathBuf,
    combined_source_path: PathBuf,
    c_output_path: PathBuf,
    llvm_output_path: PathBuf,
    native_output_path: PathBuf,
    json_report_path: PathBuf,
    markdown_report_path: PathBuf,
    ouroboros_c_path: PathBuf,
    ouroboros_llvm_path: PathBuf,
    root_component: Option<String>,
    ffi_shared_libraries: Vec<PathBuf>,
    ffi_link_libs: Vec<String>,
}

#[derive(Debug, Clone)]
struct RuntimeArtifacts {
    cache_root: PathBuf,
    object_paths: Vec<PathBuf>,
    archive_paths: Vec<PathBuf>,
    link_libs: Vec<String>,
}

#[derive(Debug, Clone)]
struct BootstrapReportOutputPaths {
    json_path: PathBuf,
    markdown_path: PathBuf,
}

pub fn run_bootstrap(
    manifest_path: Option<PathBuf>,
    backend: &str,
    combine_only: bool,
    emit_llvm_only: bool,
    link_native: bool,
    verify_ouroboros: bool,
) -> KainResult<()> {
    let backend = BootstrapBackend::parse(backend).map_err(KainError::runtime)?;
    let mode = resolve_bootstrap_mode(combine_only, emit_llvm_only, link_native, verify_ouroboros)?;
    let (repo_root, manifest_path) =
        resolve_repo_root_and_manifest_path(manifest_path).map_err(KainError::runtime)?;
    let mut report = initialize_report(&repo_root, &manifest_path, mode, backend);
    let mut report_paths = fallback_report_output_paths(&repo_root);

    let result = execute_bootstrap(
        mode,
        backend,
        &manifest_path,
        &mut report,
        &mut report_paths,
    );
    let report_write_result = write_report_files(&report_paths, &report);

    match result {
        Ok(()) => report_write_result.map_err(KainError::runtime),
        Err(err) => {
            if let Err(report_err) = report_write_result {
                eprintln!("failed to write bootstrap report: {report_err}");
            }
            Err(err)
        }
    }
}

fn resolve_bootstrap_mode(
    combine_only: bool,
    emit_llvm_only: bool,
    link_native: bool,
    verify_ouroboros: bool,
) -> KainResult<BootstrapMode> {
    let explicit_modes = [combine_only, emit_llvm_only, link_native, verify_ouroboros]
        .into_iter()
        .filter(|flag| *flag)
        .count();
    if explicit_modes > 1 {
        return Err(KainError::runtime(
            "selfhost bootstrap modes are mutually exclusive; choose one of --combine-only, --emit-llvm-only, --link-native, or --verify-ouroboros",
        ));
    }

    Ok(if combine_only {
        BootstrapMode::CombineOnly
    } else if emit_llvm_only {
        BootstrapMode::EmitCodegenOnly
    } else if verify_ouroboros {
        BootstrapMode::VerifyOuroboros
    } else {
        BootstrapMode::LinkNative
    })
}

fn execute_bootstrap(
    mode: BootstrapMode,
    backend: BootstrapBackend,
    manifest_path: &Path,
    report: &mut SelfHostBootstrapReport,
    report_paths: &mut BootstrapReportOutputPaths,
) -> KainResult<()> {
    let contract = load_bootstrap_manifest(manifest_path).map_err(|message| {
        finalize_failure(report, "manifest_resolution", &message);
        KainError::runtime(message)
    })?;
    apply_contract_to_report(report, &contract, mode, backend);
    report_paths.json_path = contract.json_report_path.clone();
    report_paths.markdown_path = contract.markdown_report_path.clone();
    push_step(
        report,
        "manifest_resolution",
        SelfHostPhaseStatus::Pass,
        format!(
            "resolved bootstrap manifest with {} source files",
            contract.source_files.len()
        ),
        vec![contract.manifest_path.display().to_string()],
    );

    let combined_source = assemble_combined_source(&contract).map_err(|message| {
        finalize_failure(report, "source_assembly", &message);
        KainError::runtime(message)
    })?;
    write_text_artifact(&contract.combined_source_path, &combined_source).map_err(|message| {
        finalize_failure(report, "source_assembly", &message);
        KainError::runtime(message)
    })?;
    ensure_path_exists(&contract.combined_source_path, "combined bootstrap source").map_err(
        |message| {
            finalize_failure(report, "source_assembly", &message);
            KainError::runtime(message)
        },
    )?;
    report.artifacts.combined_source_path =
        Some(contract.combined_source_path.display().to_string());
    push_step(
        report,
        "source_assembly",
        SelfHostPhaseStatus::Pass,
        "assembled ordered src/core bootstrap source".to_string(),
        vec![contract.combined_source_path.display().to_string()],
    );
    if mode == BootstrapMode::CombineOnly {
        report.final_phase_status = SelfHostPhaseStatus::Pass;
        report.blocker_classification = "clear".to_string();
        return Ok(());
    }

    let codegen_phase = backend.phase_name();
    let codegen_output_path = selected_codegen_output_path(&contract, backend);
    let codegen_output = compile(&combined_source, backend.compile_target()).map_err(|err| {
        finalize_failure(report, codegen_phase, &err.to_string());
        err
    })?;
    write_text_artifact(codegen_output_path, &codegen_output).map_err(|message| {
        finalize_failure(report, codegen_phase, &message);
        KainError::runtime(message)
    })?;
    ensure_path_exists(codegen_output_path, backend.noun()).map_err(|message| {
        finalize_failure(report, codegen_phase, &message);
        KainError::runtime(message)
    })?;
    match backend {
        BootstrapBackend::Llvm => {
            report.artifacts.llvm_output_path = Some(codegen_output_path.display().to_string());
        }
        BootstrapBackend::C => {
            report.artifacts.c_output_path = Some(codegen_output_path.display().to_string());
        }
    }

    let staged_artifacts = stage_native_backend_artifacts(
        &combined_source,
        backend.compile_target(),
        codegen_output_path,
        contract.root_component.as_deref(),
    )
    .map_err(|message| {
        finalize_failure(report, codegen_phase, &message);
        KainError::runtime(message)
    })?;
    apply_staged_artifacts(report, &staged_artifacts);
    push_step(
        report,
        codegen_phase,
        SelfHostPhaseStatus::Pass,
        format!(
            "compiled owned bootstrap source to {} and staged native sidecars",
            backend.noun()
        ),
        staged_codegen_artifact_list(&contract, backend, &staged_artifacts),
    );
    if mode == BootstrapMode::EmitCodegenOnly {
        report.final_phase_status = SelfHostPhaseStatus::Pass;
        report.blocker_classification = "clear".to_string();
        return Ok(());
    }

    invoke_runtime_build_script(&contract).map_err(|message| {
        finalize_failure(report, "runtime_build", &message);
        KainError::runtime(message)
    })?;
    push_step(
        report,
        "runtime_build",
        SelfHostPhaseStatus::Pass,
        "compiled or reused the manifest-driven native runtime cache".to_string(),
        vec![
            contract.runtime_build_script_path.display().to_string(),
            contract.runtime_manifest_path.display().to_string(),
        ],
    );

    let runtime_artifacts = discover_runtime_artifacts(&contract).map_err(|message| {
        finalize_failure(report, "runtime_discovery", &message);
        KainError::runtime(message)
    })?;
    report.runtime_cache_root = runtime_artifacts.cache_root.display().to_string();
    report.artifacts.runtime_object_paths = runtime_artifacts
        .object_paths
        .iter()
        .map(|path| path.display().to_string())
        .collect();
    report.artifacts.runtime_archive_paths = runtime_artifacts
        .archive_paths
        .iter()
        .map(|path| path.display().to_string())
        .collect();
    push_step(
        report,
        "runtime_discovery",
        SelfHostPhaseStatus::Pass,
        format!(
            "resolved {} runtime objects and {} runtime archives",
            runtime_artifacts.object_paths.len(),
            runtime_artifacts.archive_paths.len()
        ),
        report
            .artifacts
            .runtime_object_paths
            .iter()
            .chain(report.artifacts.runtime_archive_paths.iter())
            .cloned()
            .collect(),
    );

    link_native_output(&contract, backend, &runtime_artifacts).map_err(|message| {
        finalize_failure(report, "native_link", &message);
        KainError::runtime(message)
    })?;
    ensure_path_exists(&contract.native_output_path, "native bootstrap binary").map_err(
        |message| {
            finalize_failure(report, "native_link", &message);
            KainError::runtime(message)
        },
    )?;
    report.artifacts.native_output_path = Some(contract.native_output_path.display().to_string());
    push_step(
        report,
        "native_link",
        SelfHostPhaseStatus::Pass,
        "linked native kainc against the C runtime".to_string(),
        vec![contract.native_output_path.display().to_string()],
    );

    if mode == BootstrapMode::VerifyOuroboros {
        verify_ouroboros(&contract, backend).map_err(|message| {
            finalize_failure(report, "ouroboros_verification", &message);
            KainError::runtime(message)
        })?;
        match backend {
            BootstrapBackend::Llvm => {
                report.artifacts.ouroboros_llvm_output_path =
                    Some(contract.ouroboros_llvm_path.display().to_string());
            }
            BootstrapBackend::C => {
                report.artifacts.ouroboros_c_output_path =
                    Some(contract.ouroboros_c_path.display().to_string());
            }
        }
        push_step(
            report,
            "ouroboros_verification",
            SelfHostPhaseStatus::Pass,
            format!(
                "native bootstrap compiler recompiled the combined source with matching {} output",
                backend.noun()
            ),
            vec![
                contract.native_output_path.display().to_string(),
                selected_ouroboros_output_path(&contract, backend)
                    .display()
                    .to_string(),
            ],
        );
    }

    report.final_phase_status = SelfHostPhaseStatus::Pass;
    report.blocker_classification = "clear".to_string();
    Ok(())
}

fn resolve_repo_root_and_manifest_path(
    requested_manifest_path: Option<PathBuf>,
) -> Result<(PathBuf, PathBuf), String> {
    match requested_manifest_path {
        Some(path) if path.is_absolute() => {
            let repo_root = find_repo_root(path.parent().unwrap_or_else(|| Path::new("/")))?;
            Ok((repo_root, path))
        }
        Some(path) => {
            let repo_root =
                find_repo_root(&std::env::current_dir().map_err(|err| err.to_string())?)?;
            Ok((repo_root.clone(), repo_root.join(path)))
        }
        None => {
            let repo_root =
                find_repo_root(&std::env::current_dir().map_err(|err| err.to_string())?)?;
            Ok((
                repo_root.clone(),
                repo_root.join(DEFAULT_BOOTSTRAP_MANIFEST),
            ))
        }
    }
}

fn find_repo_root(start: &Path) -> Result<PathBuf, String> {
    let mut cursor = start.to_path_buf();
    loop {
        if cursor.join("Cargo.toml").exists() && cursor.join("crates").join("cli").exists() {
            return Ok(cursor);
        }
        if !cursor.pop() {
            break;
        }
    }
    Err(format!(
        "unable to locate repo root from {}",
        start.display()
    ))
}

fn load_bootstrap_manifest(manifest_path: &Path) -> Result<BootstrapContract, String> {
    let manifest_source = fs::read_to_string(manifest_path).map_err(|err| {
        format!(
            "unable to read bootstrap manifest {}: {}",
            manifest_path.display(),
            err
        )
    })?;
    let repo_root = find_repo_root(manifest_path.parent().unwrap_or_else(|| Path::new(".")))?;
    load_bootstrap_manifest_from_repo_root(&repo_root, manifest_path, &manifest_source)
}

fn load_bootstrap_manifest_from_repo_root(
    repo_root: &Path,
    manifest_path: &Path,
    manifest_source: &str,
) -> Result<BootstrapContract, String> {
    let manifest: BootstrapManifest = toml::from_str(manifest_source).map_err(|err| {
        format!(
            "unable to parse bootstrap manifest {}: {}",
            manifest_path.display(),
            err
        )
    })?;

    if manifest.build.source_order.is_empty() {
        return Err(format!(
            "bootstrap manifest {} must declare at least one build.source_order entry",
            manifest_path.display()
        ));
    }

    let source_root = resolve_repo_relative_path(
        repo_root,
        manifest
            .build
            .source_root
            .as_deref()
            .unwrap_or_else(|| Path::new("src/core")),
    );
    let compiler_entry = resolve_repo_relative_path(repo_root, &manifest.build.entry);
    ensure_path_exists(&compiler_entry, "bootstrap compiler entry")?;

    let source_files = manifest
        .build
        .source_order
        .iter()
        .map(|path| resolve_repo_relative_path(repo_root, path))
        .collect::<Vec<_>>();
    for source_file in &source_files {
        ensure_path_exists(source_file, "bootstrap source file")?;
    }

    let module_roots = if manifest.build.module_roots.is_empty() {
        vec![source_root.clone()]
    } else {
        manifest
            .build
            .module_roots
            .iter()
            .map(|path| resolve_repo_relative_path(repo_root, path))
            .collect()
    };
    let search_paths = if manifest.build.module_search_paths.is_empty() {
        vec![source_root.clone()]
    } else {
        manifest
            .build
            .module_search_paths
            .iter()
            .map(|path| resolve_repo_relative_path(repo_root, path))
            .collect()
    };

    let runtime_manifest_path = resolve_repo_relative_path(
        repo_root,
        manifest
            .selfhost
            .runtime
            .manifest_path
            .as_deref()
            .unwrap_or_else(|| Path::new(DEFAULT_RUNTIME_MANIFEST)),
    );
    let runtime_build_script_path = resolve_repo_relative_path(
        repo_root,
        manifest
            .selfhost
            .runtime
            .compile_script
            .as_deref()
            .unwrap_or_else(|| Path::new(DEFAULT_RUNTIME_BUILD_SCRIPT)),
    );
    ensure_path_exists(&runtime_manifest_path, "native runtime manifest")?;
    ensure_path_exists(&runtime_build_script_path, "native runtime build script")?;

    let runtime_cache_base = resolve_repo_relative_path(
        repo_root,
        manifest
            .selfhost
            .runtime
            .cache_root
            .as_deref()
            .unwrap_or_else(|| Path::new("generated/native_runtime/cache")),
    );
    let outputs = manifest.selfhost.outputs;

    Ok(BootstrapContract {
        repo_root: repo_root.to_path_buf(),
        manifest_path: manifest_path.to_path_buf(),
        package_name: manifest.package.name,
        package_version: manifest.package.version,
        compiler_entry,
        bootstrap_host_mode: manifest
            .selfhost
            .mode
            .unwrap_or_else(|| "thin_host".to_string()),
        source_root,
        source_files,
        module_roots,
        search_paths,
        runtime_manifest_path,
        runtime_build_script_path,
        runtime_cache_base,
        combined_source_path: resolve_repo_relative_path(
            repo_root,
            outputs
                .combined_source_path
                .as_deref()
                .unwrap_or_else(|| Path::new(DEFAULT_COMBINED_SOURCE_PATH)),
        ),
        c_output_path: resolve_repo_relative_path(
            repo_root,
            outputs
                .c_output_path
                .as_deref()
                .unwrap_or_else(|| Path::new(DEFAULT_C_OUTPUT_PATH)),
        ),
        llvm_output_path: resolve_repo_relative_path(
            repo_root,
            outputs
                .llvm_output_path
                .as_deref()
                .unwrap_or_else(|| Path::new(DEFAULT_LLVM_OUTPUT_PATH)),
        ),
        native_output_path: resolve_repo_relative_path(
            repo_root,
            outputs
                .native_output_path
                .as_deref()
                .unwrap_or_else(|| Path::new(DEFAULT_NATIVE_OUTPUT_PATH)),
        ),
        json_report_path: resolve_repo_relative_path(
            repo_root,
            outputs
                .json_report_path
                .as_deref()
                .unwrap_or_else(|| Path::new(DEFAULT_JSON_REPORT_PATH)),
        ),
        markdown_report_path: resolve_repo_relative_path(
            repo_root,
            outputs
                .markdown_report_path
                .as_deref()
                .unwrap_or_else(|| Path::new(DEFAULT_MARKDOWN_REPORT_PATH)),
        ),
        ouroboros_c_path: resolve_repo_relative_path(
            repo_root,
            outputs
                .ouroboros_c_path
                .as_deref()
                .unwrap_or_else(|| Path::new(DEFAULT_OUROBOROS_C_OUTPUT_PATH)),
        ),
        ouroboros_llvm_path: resolve_repo_relative_path(
            repo_root,
            outputs
                .ouroboros_llvm_path
                .as_deref()
                .unwrap_or_else(|| Path::new(DEFAULT_OUROBOROS_LLVM_OUTPUT_PATH)),
        ),
        root_component: manifest.build.root_component,
        ffi_shared_libraries: manifest
            .ffi
            .shared_libraries
            .iter()
            .map(|path| resolve_repo_relative_path(repo_root, path))
            .collect(),
        ffi_link_libs: unique_strings(manifest.ffi.link_libs),
    })
}

fn resolve_repo_relative_path(repo_root: &Path, candidate: &Path) -> PathBuf {
    if candidate.is_absolute() {
        candidate.to_path_buf()
    } else {
        repo_root.join(candidate)
    }
}

fn assemble_combined_source(contract: &BootstrapContract) -> Result<String, String> {
    let mut combined = String::new();
    combined.push_str("# bootstrap aggregate source generated by kain selfhost bootstrap\n\n");
    for source_path in &contract.source_files {
        let contents = fs::read_to_string(source_path).map_err(|err| {
            format!(
                "unable to read bootstrap source {}: {}",
                source_path.display(),
                err
            )
        })?;
        combined.push_str(&format!(
            "# begin {}\n",
            source_path
                .strip_prefix(&contract.repo_root)
                .unwrap_or(source_path)
                .display()
        ));
        combined.push_str(&contents);
        if !contents.ends_with('\n') {
            combined.push('\n');
        }
        combined.push_str(&format!(
            "# end {}\n\n",
            source_path
                .strip_prefix(&contract.repo_root)
                .unwrap_or(source_path)
                .display()
        ));
    }
    Ok(combined)
}

fn invoke_runtime_build_script(contract: &BootstrapContract) -> Result<(), String> {
    let mut command = Command::new("bash");
    command.arg(&contract.runtime_build_script_path);
    command.current_dir(&contract.repo_root);
    command.env("KAIN_RUNTIME_CACHE_DIR", &contract.runtime_cache_base);
    let status = command.status().map_err(|err| {
        format!(
            "unable to invoke runtime build script {}: {}",
            contract.runtime_build_script_path.display(),
            err
        )
    })?;
    if !status.success() {
        return Err(format!(
            "runtime build script {} returned non-zero status {}",
            contract.runtime_build_script_path.display(),
            status
        ));
    }
    Ok(())
}

fn discover_runtime_artifacts(contract: &BootstrapContract) -> Result<RuntimeArtifacts, String> {
    let manifest_source = fs::read_to_string(&contract.runtime_manifest_path).map_err(|err| {
        format!(
            "unable to read native runtime manifest {}: {}",
            contract.runtime_manifest_path.display(),
            err
        )
    })?;
    let manifest: NativeRuntimeManifest = toml::from_str(&manifest_source).map_err(|err| {
        format!(
            "unable to parse native runtime manifest {}: {}",
            contract.runtime_manifest_path.display(),
            err
        )
    })?;

    let selected_sources = current_platform_runtime_sources(&manifest);
    if selected_sources.is_empty() {
        return Err(format!(
            "native runtime manifest {} did not resolve any sources for this platform",
            contract.runtime_manifest_path.display()
        ));
    }

    let runtime_cache_root = contract
        .runtime_cache_base
        .join(runtime_cache_host_tag())
        .join(RUNTIME_BUILD_TYPE)
        .join(&manifest.name);
    let objects_dir = runtime_cache_root.join("objects");
    let archives_dir = runtime_cache_root.join("archives");
    ensure_path_exists(&objects_dir, "runtime object cache directory")?;
    ensure_path_exists(&archives_dir, "runtime archive cache directory")?;

    let object_ext = if cfg!(windows) { "obj" } else { "o" };
    let object_paths = selected_sources
        .iter()
        .enumerate()
        .map(|(index, relative_source)| {
            let stem = relative_source
                .file_stem()
                .and_then(|value| value.to_str())
                .unwrap_or("runtime");
            let object_path = objects_dir.join(format!("{index:02}_{stem}.{object_ext}"));
            ensure_path_exists(&object_path, "runtime object")?;
            Ok((relative_source.clone(), object_path))
        })
        .collect::<Result<Vec<_>, String>>()?;

    let mut archived_source_paths = BTreeSet::new();
    let mut archive_paths = Vec::new();
    for archive_group in &manifest.archive_groups {
        if archive_group.name.trim().is_empty() {
            return Err(format!(
                "native runtime manifest {} contains an archive group with an empty name",
                contract.runtime_manifest_path.display()
            ));
        }
        let matches_group = object_paths
            .iter()
            .filter(|(relative_source, _)| {
                archive_group
                    .source_prefixes
                    .iter()
                    .any(|prefix| relative_source.starts_with(prefix))
            })
            .map(|(relative_source, _)| relative_source.clone())
            .collect::<Vec<_>>();
        if matches_group.is_empty() {
            return Err(format!(
                "runtime archive group `{}` did not match any compiled sources",
                archive_group.name
            ));
        }
        archived_source_paths.extend(matches_group);
        let archive_file_name = if cfg!(windows) {
            format!("{}.lib", archive_group.name)
        } else {
            format!("lib{}.a", archive_group.name)
        };
        let archive_path = archives_dir.join(archive_file_name);
        ensure_path_exists(&archive_path, "runtime archive")?;
        archive_paths.push(archive_path);
    }

    let loose_object_paths = object_paths
        .into_iter()
        .filter(|(relative_source, _)| !archived_source_paths.contains(relative_source))
        .map(|(_, object_path)| object_path)
        .collect::<Vec<_>>();

    let mut link_libs = default_native_runtime_link_libs();
    link_libs.extend(platform_link_libs(&manifest.link));
    let uses_cpp_runtime = selected_sources
        .iter()
        .any(|path| runtime_source_uses_cpp(path));
    if uses_cpp_runtime {
        link_libs.extend(default_native_runtime_cpp_link_libs());
    }

    Ok(RuntimeArtifacts {
        cache_root: runtime_cache_root,
        object_paths: loose_object_paths,
        archive_paths,
        link_libs: unique_strings(link_libs),
    })
}

fn link_native_output(
    contract: &BootstrapContract,
    backend: BootstrapBackend,
    runtime_artifacts: &RuntimeArtifacts,
) -> Result<(), String> {
    let clang = resolve_clang_command()?;
    if let Some(parent) = contract.native_output_path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            format!(
                "unable to create native output directory {}: {}",
                parent.display(),
                err
            )
        })?;
    }

    let mut command = Command::new(&clang);
    kain_core::install_layout::apply_windows_msvc_link_env(&mut command);
    if backend == BootstrapBackend::C {
        command.arg("-std=c11");
    }
    command
        .arg(selected_codegen_output_path(contract, backend))
        .arg("-o")
        .arg(&contract.native_output_path);
    for object_path in &runtime_artifacts.object_paths {
        command.arg(object_path);
    }
    for archive_path in &runtime_artifacts.archive_paths {
        command.arg(archive_path);
    }
    for shared_library in &contract.ffi_shared_libraries {
        ensure_path_exists(shared_library, "FFI shared library")?;
        command.arg(shared_library);
    }
    for link_lib in unique_strings(
        runtime_artifacts
            .link_libs
            .iter()
            .cloned()
            .chain(contract.ffi_link_libs.iter().cloned())
            .collect(),
    ) {
        command.arg(format!("-l{link_lib}"));
    }

    if backend == BootstrapBackend::Llvm {
        command.arg("-Wno-override-module");
    }

    let status = command.status().map_err(|err| {
        format!(
            "unable to invoke clang for native bootstrap link using {}: {}",
            contract.native_output_path.display(),
            err
        )
    })?;
    if !status.success() {
        return Err(format!(
            "clang returned non-zero status while linking {}",
            contract.native_output_path.display()
        ));
    }
    Ok(())
}

fn verify_ouroboros(contract: &BootstrapContract, backend: BootstrapBackend) -> Result<(), String> {
    let ouroboros_output_path = selected_ouroboros_output_path(contract, backend);
    if let Some(parent) = ouroboros_output_path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            format!(
                "unable to create ouroboros output directory {}: {}",
                parent.display(),
                err
            )
        })?;
    }

    let status = Command::new(&contract.native_output_path)
        .arg("build")
        .arg(&contract.combined_source_path)
        .arg("--target")
        .arg(match backend {
            BootstrapBackend::Llvm => "llvm",
            BootstrapBackend::C => "c",
        })
        .arg("--output")
        .arg(ouroboros_output_path)
        .status()
        .map_err(|err| {
            format!(
                "unable to invoke native bootstrap compiler {}: {}",
                contract.native_output_path.display(),
                err
            )
        })?;
    if !status.success() {
        return Err(format!(
            "native bootstrap compiler returned non-zero status while recompiling {}",
            contract.combined_source_path.display()
        ));
    }
    ensure_path_exists(ouroboros_output_path, "ouroboros output")?;

    let baseline_path = selected_codegen_output_path(contract, backend);
    let baseline = fs::read_to_string(baseline_path).map_err(|err| {
        format!(
            "unable to read baseline {} output {}: {}",
            backend.noun(),
            baseline_path.display(),
            err
        )
    })?;
    let ouroboros = fs::read_to_string(ouroboros_output_path).map_err(|err| {
        format!(
            "unable to read ouroboros {} output {}: {}",
            backend.noun(),
            ouroboros_output_path.display(),
            err
        )
    })?;
    if normalize_for_compare(&baseline) != normalize_for_compare(&ouroboros) {
        return Err(format!(
            "ouroboros verification failed: {} does not match {}",
            ouroboros_output_path.display(),
            baseline_path.display()
        ));
    }

    Ok(())
}

fn resolve_clang_command() -> Result<String, String> {
    kain_core::install_layout::find_clang()
        .map(|p| p.to_string_lossy().into_owned())
        .ok_or_else(|| "unable to locate clang; set KAIN_CLANG_PATH or install clang".to_string())
}

fn initialize_report(
    repo_root: &Path,
    manifest_path: &Path,
    mode: BootstrapMode,
    backend: BootstrapBackend,
) -> SelfHostBootstrapReport {
    SelfHostBootstrapReport {
        generated_at_utc: Utc::now().to_rfc3339(),
        repo_root: repo_root.display().to_string(),
        manifest_path: manifest_path.display().to_string(),
        package_name: "unknown".to_string(),
        package_version: "unknown".to_string(),
        compiler_entry: String::new(),
        bootstrap_mode: mode.label(backend).to_string(),
        codegen_backend: backend.noun().to_ascii_lowercase(),
        bootstrap_host_mode: "thin_host".to_string(),
        source_root: String::new(),
        source_files: Vec::new(),
        module_roots: Vec::new(),
        search_paths: Vec::new(),
        runtime_manifest_path: repo_root
            .join(DEFAULT_RUNTIME_MANIFEST)
            .display()
            .to_string(),
        runtime_build_script_path: repo_root
            .join(DEFAULT_RUNTIME_BUILD_SCRIPT)
            .display()
            .to_string(),
        runtime_cache_root: repo_root
            .join("generated/native_runtime/cache")
            .display()
            .to_string(),
        root_component: None,
        ffi_shared_libraries: Vec::new(),
        ffi_link_libs: Vec::new(),
        artifacts: SelfHostBootstrapArtifacts::default(),
        steps: Vec::new(),
        blocker_classification: "pending".to_string(),
        error_message: None,
        final_phase_status: SelfHostPhaseStatus::SoftFail,
    }
}

fn apply_contract_to_report(
    report: &mut SelfHostBootstrapReport,
    contract: &BootstrapContract,
    mode: BootstrapMode,
    backend: BootstrapBackend,
) {
    report.generated_at_utc = Utc::now().to_rfc3339();
    report.repo_root = contract.repo_root.display().to_string();
    report.manifest_path = contract.manifest_path.display().to_string();
    report.package_name = contract.package_name.clone();
    report.package_version = contract.package_version.clone();
    report.compiler_entry = contract.compiler_entry.display().to_string();
    report.bootstrap_mode = mode.label(backend).to_string();
    report.codegen_backend = backend.noun().to_ascii_lowercase();
    report.bootstrap_host_mode = contract.bootstrap_host_mode.clone();
    report.source_root = contract.source_root.display().to_string();
    report.source_files = contract
        .source_files
        .iter()
        .map(|path| path.display().to_string())
        .collect();
    report.module_roots = contract
        .module_roots
        .iter()
        .map(|path| path.display().to_string())
        .collect();
    report.search_paths = contract
        .search_paths
        .iter()
        .map(|path| path.display().to_string())
        .collect();
    report.runtime_manifest_path = contract.runtime_manifest_path.display().to_string();
    report.runtime_build_script_path = contract.runtime_build_script_path.display().to_string();
    report.runtime_cache_root = contract.runtime_cache_base.display().to_string();
    report.root_component = contract.root_component.clone();
    report.ffi_shared_libraries = contract
        .ffi_shared_libraries
        .iter()
        .map(|path| path.display().to_string())
        .collect();
    report.ffi_link_libs = contract.ffi_link_libs.clone();
}

fn apply_staged_artifacts(
    report: &mut SelfHostBootstrapReport,
    staged_artifacts: &LlvmNativeArtifactStage,
) {
    report.artifacts.runtime_contract_path =
        Some(staged_artifacts.runtime_contract_path.display().to_string());
    report.artifacts.realtime_app_path =
        Some(staged_artifacts.realtime_app_path.display().to_string());
    report.artifacts.compute_residency_path = staged_artifacts
        .compute_residency_path
        .as_ref()
        .map(|path| path.display().to_string());
    report.artifacts.compute_residency_payload_paths = staged_artifacts
        .compute_residency_payload_paths
        .iter()
        .map(|path| path.display().to_string())
        .collect();
    report.artifacts.shader_bundle_path = staged_artifacts
        .shader_bundle_path
        .as_ref()
        .map(|path| path.display().to_string());
}

fn selected_codegen_output_path(contract: &BootstrapContract, backend: BootstrapBackend) -> &Path {
    match backend {
        BootstrapBackend::Llvm => &contract.llvm_output_path,
        BootstrapBackend::C => &contract.c_output_path,
    }
}

fn selected_ouroboros_output_path(
    contract: &BootstrapContract,
    backend: BootstrapBackend,
) -> &Path {
    match backend {
        BootstrapBackend::Llvm => &contract.ouroboros_llvm_path,
        BootstrapBackend::C => &contract.ouroboros_c_path,
    }
}

fn staged_codegen_artifact_list(
    contract: &BootstrapContract,
    backend: BootstrapBackend,
    staged_artifacts: &LlvmNativeArtifactStage,
) -> Vec<String> {
    let mut artifacts = vec![selected_codegen_output_path(contract, backend)
        .display()
        .to_string()];
    artifacts.push(staged_artifacts.runtime_contract_path.display().to_string());
    artifacts.push(staged_artifacts.realtime_app_path.display().to_string());
    if let Some(path) = &staged_artifacts.compute_residency_path {
        artifacts.push(path.display().to_string());
    }
    artifacts.extend(
        staged_artifacts
            .compute_residency_payload_paths
            .iter()
            .map(|path| path.display().to_string()),
    );
    if let Some(path) = &staged_artifacts.shader_bundle_path {
        artifacts.push(path.display().to_string());
    }
    artifacts
}

fn push_step(
    report: &mut SelfHostBootstrapReport,
    step_name: &str,
    status: SelfHostPhaseStatus,
    detail: String,
    artifacts: Vec<String>,
) {
    report.steps.push(SelfHostBootstrapStepReport {
        step_name: step_name.to_string(),
        status,
        detail,
        artifacts,
    });
}

fn finalize_failure(report: &mut SelfHostBootstrapReport, blocker: &str, message: &str) {
    report.final_phase_status = SelfHostPhaseStatus::HardFail;
    report.blocker_classification = blocker.to_string();
    report.error_message = Some(message.to_string());
}

fn write_report_files(
    report_paths: &BootstrapReportOutputPaths,
    report: &SelfHostBootstrapReport,
) -> Result<(), String> {
    write_text_artifact(
        &report_paths.json_path,
        &serde_json::to_string_pretty(report).map_err(|err| {
            format!(
                "unable to serialize bootstrap JSON report {}: {}",
                report_paths.json_path.display(),
                err
            )
        })?,
    )?;
    write_text_artifact(
        &report_paths.markdown_path,
        &render_bootstrap_markdown(report),
    )?;
    Ok(())
}

fn fallback_report_output_paths(repo_root: &Path) -> BootstrapReportOutputPaths {
    BootstrapReportOutputPaths {
        json_path: repo_root.join(DEFAULT_JSON_REPORT_PATH),
        markdown_path: repo_root.join(DEFAULT_MARKDOWN_REPORT_PATH),
    }
}

fn write_text_artifact(path: &Path, contents: &str) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            format!(
                "unable to create artifact directory {}: {}",
                parent.display(),
                err
            )
        })?;
    }
    fs::write(path, contents.as_bytes())
        .map_err(|err| format!("unable to write artifact {}: {}", path.display(), err))
}

fn ensure_path_exists(path: &Path, label: &str) -> Result<(), String> {
    if path.exists() {
        Ok(())
    } else {
        Err(format!("missing {} at {}", label, path.display()))
    }
}

fn runtime_cache_host_tag() -> String {
    format!("{}-{}", std::env::consts::OS, std::env::consts::ARCH)
}

fn current_platform_runtime_sources(manifest: &NativeRuntimeManifest) -> Vec<PathBuf> {
    let mut sources = manifest.sources.clone();
    if cfg!(windows) {
        sources.extend(manifest.windows_sources.clone());
    } else if cfg!(target_os = "macos") {
        sources.extend(manifest.macos_sources.clone());
    } else {
        sources.extend(manifest.linux_sources.clone());
    }
    sources
}

fn platform_link_libs(link: &NativeRuntimeLinkManifest) -> Vec<String> {
    if cfg!(windows) {
        link.windows.clone()
    } else if cfg!(target_os = "macos") {
        link.macos.clone()
    } else {
        link.linux.clone()
    }
}

fn default_native_runtime_link_libs() -> Vec<String> {
    kain_build::platform_link_libs().iter().map(|s| s.to_string()).collect()
}

fn default_native_runtime_cpp_link_libs() -> Vec<String> {
    kain_build::platform_cpp_link_libs().iter().map(|s| s.to_string()).collect()
}

fn runtime_source_uses_cpp(path: &Path) -> bool {
    matches!(
        path.extension().and_then(|extension| extension.to_str()),
        Some("cc" | "cpp" | "cxx" | "mm")
    )
}

fn unique_strings(values: Vec<String>) -> Vec<String> {
    let mut seen = BTreeSet::new();
    let mut ordered = Vec::new();
    for value in values {
        if seen.insert(value.clone()) {
            ordered.push(value);
        }
    }
    ordered
}

fn normalize_for_compare(text: &str) -> String {
    text.replace("\r\n", "\n")
}

fn default_runtime_name() -> String {
    "kain-native-runtime".to_string()
}

#[cfg(test)]
mod tests {
    use super::{assemble_combined_source, load_bootstrap_manifest_from_repo_root};
    use std::fs;
    use std::path::Path;
    use tempfile::TempDir;

    #[test]
    fn load_bootstrap_manifest_resolves_repo_relative_paths() {
        let temp = TempDir::new().expect("temp dir");
        let repo_root = temp.path();
        fs::write(repo_root.join("Cargo.toml"), "[workspace]\nmembers = []\n").expect("cargo");
        fs::create_dir_all(repo_root.join("crates/cli")).expect("cli dir");
        fs::create_dir_all(repo_root.join("src/core")).expect("source root");
        fs::create_dir_all(repo_root.join("runtime")).expect("runtime root");
        fs::write(
            repo_root.join("src/core/kainc.kn"),
            "fn main():\n    pass\n",
        )
        .expect("entry");
        fs::write(repo_root.join("src/core/a.kn"), "fn a():\n    pass\n").expect("a");
        fs::write(repo_root.join("src/core/b.kn"), "fn b():\n    pass\n").expect("b");
        fs::write(
            repo_root.join("runtime/native_core_runtime.toml"),
            "name = \"runtime\"\n",
        )
        .expect("runtime manifest");
        fs::write(
            repo_root.join("runtime/compile_native_runtime.sh"),
            "#!/usr/bin/env bash\nexit 0\n",
        )
        .expect("runtime script");

        let manifest_path = repo_root.join("src/KAIN.toml");
        let manifest = r#"
[package]
name = "owned"
version = "0.1.0"

[build]
entry = "src/core/kainc.kn"
source_root = "src/core"
source_order = ["src/core/a.kn", "src/core/b.kn"]
module_roots = ["src/core"]
module_search_paths = ["src/core"]

[selfhost]
mode = "thin_host"

[selfhost.runtime]
manifest_path = "runtime/native_core_runtime.toml"
compile_script = "runtime/compile_native_runtime.sh"
cache_root = "generated/native_runtime/cache"

[selfhost.outputs]
combined_source_path = "src/.selfhost/combined.kn"
llvm_output_path = "src/.selfhost/combined.ll"
native_output_path = "src/.selfhost/kainc"
json_report_path = "src/.selfhost/report.json"
markdown_report_path = "src/.selfhost/report.md"
ouroboros_llvm_path = "src/.selfhost/stage2.ll"
"#;

        let contract = load_bootstrap_manifest_from_repo_root(repo_root, &manifest_path, manifest)
            .expect("manifest");
        assert_eq!(contract.package_name, "owned");
        assert_eq!(contract.package_version, "0.1.0");
        assert_eq!(contract.compiler_entry, repo_root.join("src/core/kainc.kn"));
        assert_eq!(
            contract.source_files,
            vec![
                repo_root.join("src/core/a.kn"),
                repo_root.join("src/core/b.kn")
            ]
        );
        assert_eq!(
            contract.runtime_manifest_path,
            repo_root.join("runtime/native_core_runtime.toml")
        );
        assert_eq!(
            contract.runtime_build_script_path,
            repo_root.join("runtime/compile_native_runtime.sh")
        );
        assert_eq!(
            contract.combined_source_path,
            repo_root.join("src/.selfhost/combined.kn")
        );
    }

    #[test]
    fn assemble_combined_source_keeps_declared_order() {
        let temp = TempDir::new().expect("temp dir");
        let repo_root = temp.path();
        fs::write(repo_root.join("Cargo.toml"), "[workspace]\nmembers = []\n").expect("cargo");
        fs::create_dir_all(repo_root.join("crates/cli")).expect("cli dir");
        fs::create_dir_all(repo_root.join("src/core")).expect("source root");
        fs::create_dir_all(repo_root.join("runtime")).expect("runtime root");
        fs::write(
            repo_root.join("src/core/kainc.kn"),
            "fn main():\n    pass\n",
        )
        .expect("entry");
        fs::write(
            repo_root.join("src/core/first.kn"),
            "fn first():\n    return 1\n",
        )
        .expect("first");
        fs::write(
            repo_root.join("src/core/second.kn"),
            "fn second():\n    return 2\n",
        )
        .expect("second");
        fs::write(
            repo_root.join("runtime/native_core_runtime.toml"),
            "name = \"runtime\"\n",
        )
        .expect("runtime manifest");
        fs::write(
            repo_root.join("runtime/compile_native_runtime.sh"),
            "#!/usr/bin/env bash\nexit 0\n",
        )
        .expect("runtime script");

        let manifest_path = repo_root.join("src/KAIN.toml");
        let manifest = r#"
[package]
name = "owned"
version = "0.1.0"

[build]
entry = "src/core/kainc.kn"
source_root = "src/core"
source_order = ["src/core/first.kn", "src/core/second.kn"]

[selfhost]

[selfhost.runtime]

[selfhost.outputs]
"#;

        let contract = load_bootstrap_manifest_from_repo_root(repo_root, &manifest_path, manifest)
            .expect("manifest");
        let combined = assemble_combined_source(&contract).expect("combined source");
        let first_index = combined.find("fn first()").expect("first function");
        let second_index = combined.find("fn second()").expect("second function");
        assert!(
            first_index < second_index,
            "source order should be preserved"
        );
        assert!(combined.contains("# begin src/core/first.kn"));
        assert!(combined.contains("# end src/core/second.kn"));
        assert_eq!(
            Path::new(&contract.source_root),
            repo_root.join("src/core").as_path()
        );
    }

    #[test]
    fn windows_default_runtime_link_libs_do_not_force_opengl() {
        if cfg!(windows) {
            assert!(!super::default_native_runtime_link_libs()
                .iter()
                .any(|value| value == "opengl32"));
        }
    }
}
