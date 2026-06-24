// KAIN Compiler CLI

use cli::amalgamate;
use cli::clean;
use cli::codebase;
use cli::fabric;
#[cfg(all(feature = "gpu", feature = "sys"))]
use cli::gpu_artifacts;
use cli::import_asm;
use cli::import_c;
use cli::import_crate;
use cli::import_platform;
use cli::import_rust;
use cli::import_typescript;
use cli::llvm_native_stage;
use cli::lsp;
use cli::native_ui_build;
use cli::native_ui_dev;
use cli::omni;
use cli::packager;
use cli::packages;
use cli::repair;
use cli::run as run_cli;
use cli::runtime_tools;
use cli::selfhost;
use cli::{
    detect_launcher_from_path, parse_compile_target, render_launcher_menu,
    resolve_legacy_target_alias, should_show_launcher_menu, supported_targets_csv,
    target_extension, CompileTarget, LauncherKind, BUILD_GIT_COMMIT_COUNT, BUILD_GIT_DIRTY,
    BUILD_GIT_SHA, BUILD_HOST_TRIPLE, BUILD_NUMBER, BUILD_PROFILE, BUILD_TARGET_TRIPLE,
    BUILD_TRACKING_MODE, BUILD_UNIX_TIME, LANGUAGE_NAME, VERSION,
};
use kain_c_ffi::{
    ArtifactMode as CArtifactMode, ImportCOptions as CImportCOptions,
    PrepareContext as CPrepareContext,
};
use kain_commands::kain::{
    BridgeCommand, BuildCommand, EmitMode, ImportCommand, KainCli as Args,
    KainCommand as Commands, NativeUiCommand, RegistryCommand, RunCommand,
};
use kain_crate_ffi::{ArtifactMode, ImportCrateOptions};
use kain_fmt::{format_source_with_options, FormatOptions};
use kain_repl::{
    normalize_script_source, run_terminal_repl, ReplBuildMetadata, ReplTerminalConfig,
};
use kain_lattice::Painter;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::io::{self, IsTerminal, Read};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

fn p() -> Painter {
    kain_core::tooling_config::active_painter()
}

#[derive(Debug, Default, Deserialize)]
struct NativeRuntimeManifest {
    #[serde(default)]
    name: Option<String>,
    #[serde(default)]
    sources: Vec<PathBuf>,
    #[serde(default)]
    windows_sources: Vec<PathBuf>,
    #[serde(default)]
    linux_sources: Vec<PathBuf>,
    #[serde(default)]
    macos_sources: Vec<PathBuf>,
    #[serde(default)]
    include_dirs: Vec<PathBuf>,
    #[serde(default)]
    defines: Vec<String>,
    #[serde(default)]
    windows_defines: Vec<String>,
    #[serde(default)]
    linux_defines: Vec<String>,
    #[serde(default)]
    macos_defines: Vec<String>,
    #[serde(default)]
    archive_groups: Vec<NativeRuntimeArchiveManifest>,
    #[serde(default)]
    link: NativeRuntimeLinkManifest,
}

#[derive(Debug, Default, Deserialize)]
struct NativeRuntimeArchiveManifest {
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

#[derive(Debug)]
struct ResolvedNativeRuntimeBundle {
    name: String,
    sources: Vec<PathBuf>,
    include_dirs: Vec<PathBuf>,
    defines: Vec<String>,
    archive_groups: Vec<ResolvedNativeRuntimeArchiveGroup>,
    cache_root: PathBuf,
    link_libs: Vec<String>,
    uses_cpp_runtime: bool,
}

#[derive(Debug, Clone)]
struct ResolvedNativeRuntimeArchiveGroup {
    name: String,
    source_paths: Vec<PathBuf>,
    uses_cpp_runtime: bool,
}

#[derive(Debug)]
struct NativeRuntimeObjectCachePaths {
    object_path: PathBuf,
    depfile_path: PathBuf,
    fingerprint_path: PathBuf,
}

#[derive(Debug, Clone)]
struct NativeRuntimeObjectBuildRecord {
    source: PathBuf,
    object_path: PathBuf,
    fingerprint: String,
    was_reused: bool,
}

#[derive(Debug)]
struct NativeRuntimeStaticArchivePaths {
    archive_path: PathBuf,
    fingerprint_path: PathBuf,
}

#[derive(Debug, Default)]
struct NativeRuntimeCompiledArtifacts {
    loose_objects: Vec<PathBuf>,
    static_archives: Vec<PathBuf>,
}

#[derive(Debug, Default)]
struct CffiNativeLinkInputs {
    link_inputs: Vec<PathBuf>,
    link_libs: Vec<String>,
    library_search_paths: Vec<PathBuf>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct NativeRuntimeElisionDecision {
    can_elide: bool,
    reason: String,
    reachable_functions: usize,
    reachable_external_targets: Vec<String>,
}

#[derive(Debug, Clone)]
struct NativeBackendOutputPaths {
    backend_path: PathBuf,
    executable_path: PathBuf,
}

#[derive(Debug, Clone)]
struct NativeExecutableCacheHit {
    key: String,
    backend_bytes: u64,
    executable_bytes: u64,
    restored_sidecars: Vec<PathBuf>,
}

/// A precompiled native runtime archive (.a/.lib) ready to link.
struct PrecompiledRuntimeArchive {
    path: PathBuf,
    link_libs: Vec<String>,
}

/// Resolve the precompiled native runtime archive path.
///
/// Priority:
/// 1. `KAIN_RUNTIME_LIB_PATH` env var (explicit override)
/// 2. `~/.kain/lib/libkain_runtime.a` or `kain_runtime.lib` (installed toolchain)
fn resolve_precompiled_runtime_archive() -> Option<PrecompiledRuntimeArchive> {
    use kain_core::install_layout;

    // 1. Explicit env var
    if let Ok(path) = std::env::var(install_layout::KAIN_RUNTIME_LIB_ENV_VAR) {
        let p = PathBuf::from(path);
        if p.exists() {
            return Some(PrecompiledRuntimeArchive {
                path: p,
                link_libs: default_native_runtime_link_libs(),
            });
        }
    }

    // 2. Toolchain layout (~/.kain/lib/)
    if let Some(layout) = install_layout::default_kain_install_layout() {
        let lib_name = if cfg!(windows) {
            "kain_runtime.lib"
        } else {
            "libkain_runtime.a"
        };
        let candidate = layout.lib_dir.join(lib_name);
        if candidate.exists() {
            return Some(PrecompiledRuntimeArchive {
                path: candidate,
                link_libs: default_native_runtime_link_libs(),
            });
        }

        // Also check KAIN_HOME/lib/ for fallback
        let fallback = layout.home_dir.join("lib").join(lib_name);
        if fallback.exists() {
            return Some(PrecompiledRuntimeArchive {
                path: fallback,
                link_libs: default_native_runtime_link_libs(),
            });
        }
    }

    None
}

impl NativeRuntimeElisionDecision {
    fn elide(reachable_functions: usize) -> Self {
        Self {
            can_elide: true,
            reason: "no reachable non-intrinsic external calls from LLVM main".to_string(),
            reachable_functions,
            reachable_external_targets: Vec::new(),
        }
    }

    fn keep(reason: impl Into<String>) -> Self {
        Self {
            can_elide: false,
            reason: reason.into(),
            reachable_functions: 0,
            reachable_external_targets: Vec::new(),
        }
    }

    fn keep_for_external_targets(
        reachable_functions: usize,
        reachable_external_targets: BTreeSet<String>,
    ) -> Self {
        Self {
            can_elide: false,
            reason: "reachable external calls require the native runtime/link lane".to_string(),
            reachable_functions,
            reachable_external_targets: reachable_external_targets.into_iter().collect(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum NativeToolchainProfile {
    Debug,
    Release,
    BenchmarkRelease,
}

impl NativeToolchainProfile {
    fn as_str(self) -> &'static str {
        match self {
            Self::Debug => "debug",
            Self::Release => "release",
            Self::BenchmarkRelease => "benchmark-release",
        }
    }

    fn defaults(self) -> NativeToolchainTuning {
        match self {
            Self::Debug => NativeToolchainTuning {
                profile: self,
                opt_level: "0".to_string(),
                target_cpu: None,
                debug_info: true,
            },
            Self::Release => NativeToolchainTuning {
                profile: self,
                opt_level: "2".to_string(),
                target_cpu: None,
                debug_info: false,
            },
            Self::BenchmarkRelease => NativeToolchainTuning {
                profile: self,
                opt_level: "3".to_string(),
                target_cpu: Some("native".to_string()),
                debug_info: false,
            },
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct NativeToolchainTuning {
    profile: NativeToolchainProfile,
    opt_level: String,
    target_cpu: Option<String>,
    debug_info: bool,
}

impl NativeToolchainTuning {
    fn apply_to_clang_command(&self, command: &mut Command) {
        command.arg(format!("-O{}", self.opt_level));
        if self.debug_info {
            command.arg("-g");
        } else {
            command.arg("-g0");
        }
        if let Some(target_cpu) = &self.target_cpu {
            command.arg(format!("-march={target_cpu}"));
            command.arg(format!("-mtune={target_cpu}"));
        }
        if self.profile != NativeToolchainProfile::Debug {
            command.arg("-ffunction-sections");
            command.arg("-fdata-sections");
        }
    }

    fn clang_compile_args(&self) -> Vec<String> {
        let mut args = vec![format!("-O{}", self.opt_level)];
        if self.debug_info {
            args.push("-g".to_string());
        } else {
            args.push("-g0".to_string());
        }
        if let Some(target_cpu) = &self.target_cpu {
            args.push(format!("-march={target_cpu}"));
            args.push(format!("-mtune={target_cpu}"));
        }
        if self.profile != NativeToolchainProfile::Debug {
            args.push("-ffunction-sections".to_string());
            args.push("-fdata-sections".to_string());
        }
        args
    }

    fn apply_link_gc_flags(&self, command: &mut Command) {
        if cfg!(windows) && !self.debug_info {
            command.arg("-Wl,/DEBUG:NONE");
        }
        if self.profile == NativeToolchainProfile::Debug {
            return;
        }
        if cfg!(windows) {
            command.arg("-Wl,/OPT:REF");
            command.arg("-Wl,/OPT:ICF");
        } else if cfg!(target_os = "macos") {
            command.arg("-Wl,-dead_strip");
        } else {
            command.arg("-Wl,--gc-sections");
        }
    }

    fn fingerprint_lines(&self) -> [String; 4] {
        [
            format!("native_profile={}", self.profile.as_str()),
            format!("native_opt_level={}", self.opt_level),
            format!(
                "native_target_cpu={}",
                self.target_cpu.as_deref().unwrap_or("")
            ),
            format!("native_debug_info={}", self.debug_info),
        ]
    }
}

#[derive(Debug, Clone, Copy)]
enum NativeRuntimeArchiverFlavor {
    GnuAr,
    MsvcLib,
}

#[derive(Debug)]
struct NativeRuntimeArchiver {
    command: String,
    flavor: NativeRuntimeArchiverFlavor,
    archive_ext: &'static str,
}

#[derive(Debug)]
enum ResolvedBuildInput {
    File(PathBuf),
    Project(PathBuf),
}

fn parse_native_ui_host_kind(value: &str) -> Result<native_ui_build::NativeUiHostKind, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "qt" => Ok(native_ui_build::NativeUiHostKind::Qt),
        "tauri" | "webview" => Ok(native_ui_build::NativeUiHostKind::Tauri),
        other => Err(format!(
            "Unsupported native UI host '{}'. Expected 'qt' or 'tauri'.",
            other
        )),
    }
}

fn parse_build_lane(value: Option<&str>) -> Result<Option<kain_build::BuildLane>, String> {
    value
        .map(|value| {
            kain_build::BuildLane::parse(value).ok_or_else(|| {
                format!(
                    "unknown build lane '{}'; use bootstrap, dev, release, dist, or selfhost",
                    value
                )
            })
        })
        .transpose()
}

fn resolve_emit_mode(mode: Option<EmitMode>) -> kain_build::NativeEmit {
    match mode {
        Some(EmitMode::SharedLib) => kain_build::NativeEmit::SharedLib,
        Some(EmitMode::StaticLib) => kain_build::NativeEmit::StaticLib,
        Some(EmitMode::Object) => kain_build::NativeEmit::Object,
        Some(EmitMode::Exe) | None => kain_build::NativeEmit::Exe,
    }
}

fn read_source_from_path(input: &Path) -> Result<String, String> {
    Ok(normalize_script_source(read_source_text(input)?))
}

fn read_source_text(input: &Path) -> Result<String, String> {
    let source = if input == Path::new("-") {
        let mut buffer = String::new();
        io::stdin()
            .read_to_string(&mut buffer)
            .map_err(|err| format!("Failed to read stdin: {err}"))?;
        buffer
    } else {
        fs::read_to_string(input)
            .map_err(|err| format!("Failed to read {}: {err}", input.display()))?
    };
    Ok(source)
}

fn read_format_stdin() -> Result<(String, String), String> {
    if io::stdin().is_terminal() {
        return Err("Format requires an input file path or piped stdin.".to_string());
    }
    let mut buffer = String::new();
    io::stdin()
        .read_to_string(&mut buffer)
        .map_err(|err| format!("Failed to read stdin: {err}"))?;
    Ok((buffer, "<stdin>".to_string()))
}

fn expand_format_inputs(inputs: &[PathBuf]) -> Result<Vec<PathBuf>, String> {
    let mut expanded = BTreeSet::new();
    for input in inputs {
        let discovered = kain_check::discover_kain_files(input)?;
        if discovered.is_empty() {
            return Err(format!(
                "No Kain source files found under {}",
                input.display()
            ));
        }
        expanded.extend(discovered);
    }
    Ok(expanded.into_iter().collect())
}

fn source_path_from_inputs(inputs: &[PathBuf]) -> Option<&Path> {
    inputs.first().and_then(|path| {
        if path == Path::new("-") {
            None
        } else {
            Some(path.as_path())
        }
    })
}

fn format_source_for_cli(
    source: &str,
    source_name: &str,
    source_path: Option<&Path>,
    options: FormatOptions,
) -> Result<String, ()> {
    match format_source_with_options(source, options) {
        Ok(value) => Ok(value),
        Err(err) => {
            let diag = kain_core::diagnostics::Diagnostics::new(source, source_name);
            let rendered = diag.format_error(&err);
            eprint!("{rendered}");
            capture_kain_error_failure(
                "format",
                None,
                None,
                source_name,
                source_path,
                source,
                &err,
                &rendered,
            );
            Err(())
        }
    }
}

fn run_format_command(inputs: Vec<PathBuf>, check: bool, write: bool) -> bool {
    let options = FormatOptions::default();
    let using_stdin = inputs.is_empty() || (inputs.len() == 1 && inputs[0] == Path::new("-"));

    if using_stdin {
        if write {
            eprintln!("{} Format --write does not support stdin.", p().status_error(""));
            return false;
        }
        let (source, source_name) = match if inputs.is_empty() {
            read_format_stdin()
        } else {
            read_source_text(Path::new("-")).map(|source| (source, "<stdin>".to_string()))
        } {
            Ok(value) => value,
            Err(err) => {
                eprintln!(" {}", err);
                capture_rendered_failure(
                    "command-failure",
                    "format",
                    format!(" {}\n", err),
                    None,
                    None,
                    None,
                    None,
                    None,
                    serde_json::json!({"phase": "read-source"}),
                );
                return false;
            }
        };
        let Ok(formatted) = format_source_for_cli(&source, &source_name, None, options) else {
            return false;
        };
        if check {
            if formatted != source {
                eprintln!("{} Formatting changes required: {}", p().status_error(""), source_name);
                return false;
            }
            return true;
        }
        print!("{formatted}");
        return true;
    }

    if inputs.iter().any(|path| path == Path::new("-")) {
        eprintln!("{} Format stdin cannot be combined with file or directory paths.", p().status_error(""));
        return false;
    }

    let expanded = match expand_format_inputs(&inputs) {
        Ok(paths) => paths,
        Err(err) => {
            eprintln!(" {}", err);
            capture_rendered_failure(
                "command-failure",
                "format",
                format!(" {}\n", err),
                None,
                None,
                None,
                source_path_from_inputs(&inputs),
                None,
                serde_json::json!({"phase": "expand-inputs"}),
            );
            return false;
        }
    };

    if expanded.len() > 1 && !check && !write {
        eprintln!("{} Format multiple files requires --check or --write.", p().status_error(""));
        return false;
    }

    let mut changed_paths = Vec::new();
    let mut success = true;

    for path in expanded {
        let source = match read_source_text(&path) {
            Ok(source) => source,
            Err(err) => {
                eprintln!(" {}", err);
                capture_rendered_failure(
                    "command-failure",
                    "format",
                    format!(" {}\n", err),
                    None,
                    None,
                    None,
                    Some(path.as_path()),
                    None,
                    serde_json::json!({"phase": "read-source"}),
                );
                success = false;
                continue;
            }
        };
        let source_name = path.display().to_string();
        let Ok(formatted) =
            format_source_for_cli(&source, &source_name, Some(path.as_path()), options)
        else {
            success = false;
            continue;
        };
        let changed = formatted != source;

        if check {
            if changed {
                changed_paths.push(path);
                success = false;
            }
            continue;
        }

        if write {
            if changed {
                if !ensure_parent_dir(&path) {
                    success = false;
                    continue;
                }
                if let Err(err) = fs::write(&path, &formatted) {
                    eprintln!("{} Failed to write {}: {}", p().status_error(""), path.display(), err);
                    success = false;
                    continue;
                }
                println!("{} Formatted {}", p().status_ok(""), path.display());
            } else {
                println!("{} Already formatted {}", p().status_muted(""), path.display());
            }
            continue;
        }

        print!("{formatted}");
    }

    if check && !changed_paths.is_empty() {
        let total = changed_paths.len();
        for (index, path) in changed_paths.iter().enumerate() {
            eprintln!(
                "{} Formatting changes required {}/{}: {}",
                p().status_error(""),
                index + 1,
                total,
                path.display()
            );
        }
    }

    success
}

fn run_stdlib_map_command(
    repo_root: Option<PathBuf>,
    stdlib_root: Option<PathBuf>,
    native_manifests: Vec<PathBuf>,
    json_out: Option<PathBuf>,
    llm_out: Option<PathBuf>,
    write: bool,
    check: bool,
    json: bool,
) -> Result<(), Box<dyn std::error::Error>> {
    let repo_root = match repo_root {
        Some(path) => path,
        None => kain_stdlib_map::discover_repo_root(std::env::current_dir()?)?,
    };
    let options = kain_stdlib_map::StdlibMapOptions::from_repo_root(repo_root)
        .with_stdlib_root(stdlib_root)
        .with_native_manifests(native_manifests)
        .with_json_out(json_out)
        .with_llm_out(llm_out);

    if check {
        kain_stdlib_map::check_generated_files(&options)?;
        println!(
            " Stdlib map is current: {}, {}",
            options.json_out.display(),
            options.llm_out.display()
        );
        return Ok(());
    }

    if write {
        let report = kain_stdlib_map::write_generated_files(&options)?;
        println!(
            " Wrote stdlib map: {} symbols across {} modules",
            report.map.summary.symbol_count, report.map.summary.module_count
        );
        println!("   json: {}", report.json_path.display());
        println!("   llm: {}", report.llm_path.display());
        return Ok(());
    }

    let map = kain_stdlib_map::generate_stdlib_map(&options)?;
    if json {
        println!("{}", serde_json::to_string_pretty(&map)?);
    } else {
        print!("{}", kain_stdlib_map::render_llm_markdown(&map));
    }
    Ok(())
}

#[derive(Debug, Serialize)]
struct HybridBundleDescriptor {
    schema_version: u32,
    target: &'static str,
    js: String,
    ts: String,
    wasm: String,
    wasm_exports: Vec<String>,
}

#[derive(Debug)]
struct HybridBundleWriteSummary {
    descriptor_path: PathBuf,
    js_path: PathBuf,
    ts_path: PathBuf,
    wasm_path: PathBuf,
}

fn resolve_output_path_with_extension(
    output: Option<&PathBuf>,
    source_path: Option<&Path>,
    extension: &str,
) -> Option<PathBuf> {
    if let Some(path) = output {
        let mut normalized = path.clone();
        normalized.set_extension(extension);
        return Some(normalized);
    }

    source_path.map(|path| path.with_extension(extension))
}

fn patch_hybrid_wasm_reference(source: String, wasm_file_name: &str) -> String {
    let wasm_url_expression = format!(
        "new URL('{wasm_file_name}', document.currentScript?.src ?? window.location.href).toString()"
    );

    source
        .replace("'main.wasm'", &wasm_url_expression)
        .replace("\"main.wasm\"", &wasm_url_expression)
}

fn write_hybrid_bundle(
    descriptor_path: &Path,
    artifacts: cli::HybridArtifactOutput,
) -> Result<HybridBundleWriteSummary, String> {
    let js_path = descriptor_path.with_extension("js");
    let ts_path = descriptor_path.with_extension("ts");
    let wasm_path = descriptor_path.with_extension("wasm");
    let wasm_file_name = wasm_path
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| format!("Invalid hybrid wasm sidecar path: {}", wasm_path.display()))?;
    let js_file_name = js_path
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| format!("Invalid hybrid JS sidecar path: {}", js_path.display()))?;
    let ts_file_name = ts_path
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| format!("Invalid hybrid TS sidecar path: {}", ts_path.display()))?;
    let descriptor = HybridBundleDescriptor {
        schema_version: 1,
        target: "hybrid",
        js: js_file_name.to_string(),
        ts: ts_file_name.to_string(),
        wasm: wasm_file_name.to_string(),
        wasm_exports: artifacts.wasm_export_names,
    };
    let descriptor_json = serde_json::to_string_pretty(&descriptor)
        .map_err(|err| format!("Failed to serialize hybrid descriptor: {err}"))?;
    let patched_js = patch_hybrid_wasm_reference(artifacts.js, wasm_file_name);
    let patched_ts = patch_hybrid_wasm_reference(artifacts.ts, wasm_file_name);

    if !ensure_parent_dir(descriptor_path) {
        return Err(format!(
            "Failed to create parent directory for {}",
            descriptor_path.display()
        ));
    }

    fs::write(descriptor_path, descriptor_json)
        .map_err(|err| format!("Failed to write {}: {err}", descriptor_path.display()))?;
    fs::write(&js_path, patched_js)
        .map_err(|err| format!("Failed to write {}: {err}", js_path.display()))?;
    fs::write(&ts_path, patched_ts)
        .map_err(|err| format!("Failed to write {}: {err}", ts_path.display()))?;
    fs::write(&wasm_path, artifacts.wasm)
        .map_err(|err| format!("Failed to write {}: {err}", wasm_path.display()))?;

    Ok(HybridBundleWriteSummary {
        descriptor_path: descriptor_path.to_path_buf(),
        js_path,
        ts_path,
        wasm_path,
    })
}

fn run_source(
    source_name: &str,
    source_path: Option<&Path>,
    source: &str,
    target: CompileTarget,
    output: Option<&PathBuf>,
    emit_ast: bool,
    emit_typed: bool,
    verbose: bool,
    analyze: bool,
    plugin_name: Option<&str>,
) -> bool {
    let session = kain_driver::DriverSession::new();
    run_source_with_session(
        &session,
        source_name,
        source_path,
        source,
        target,
        output,
        emit_ast,
        emit_typed,
        verbose,
        analyze,
        plugin_name,
    )
}

fn legacy_source_prefers_native_script(requested_target: &str, has_output: bool) -> bool {
    if has_output {
        return false;
    }
    matches!(
        requested_target.trim().to_ascii_lowercase().as_str(),
        "wasm" | "llvm" | "native" | "n"
    )
}

fn legacy_file_prefers_native_script(
    launcher: LauncherKind,
    requested_target: &str,
    has_output: bool,
) -> bool {
    launcher.prefers_interpret_default()
        && legacy_source_prefers_native_script(requested_target, has_output)
}

fn print_compact_run_report(report: &kain_run::RunReport, success: bool) {
    let compact = kain_run::render_compact_output(report);
    if compact.trim().is_empty() {
        return;
    }
    if success {
        print!("{compact}");
    } else {
        eprint!("{compact}");
    }
}

fn capture_rendered_failure(
    event_kind: &str,
    command: &str,
    rendered_output: impl Into<String>,
    launcher: Option<LauncherKind>,
    target: Option<&str>,
    source_name: Option<&str>,
    source_path: Option<&Path>,
    structured_diagnostic: Option<serde_json::Value>,
    context: serde_json::Value,
) -> bool {
    kain_core::diagnostic_capture::capture_event_if_enabled(
        kain_core::diagnostic_capture::CapturedDiagnosticEventInput {
            event_kind: event_kind.to_string(),
            command: command.to_string(),
            argv: std::env::args().collect(),
            cwd: std::env::current_dir()
                .unwrap_or_else(|_| PathBuf::from("."))
                .display()
                .to_string(),
            launcher: launcher.map(|value| value.display_name().to_string()),
            target: target.map(str::to_string),
            source_name: source_name.map(str::to_string),
            source_path: source_path.map(|path| path.display().to_string()),
            rendered_output: rendered_output.into(),
            structured_diagnostic,
            tags: Vec::new(),
            context,
        },
    )
    .unwrap_or(false)
}

fn capture_kain_error_failure(
    command: &str,
    launcher: Option<LauncherKind>,
    target: Option<&str>,
    source_name: &str,
    source_path: Option<&Path>,
    source: &str,
    error: &kain_core::KainError,
    rendered_output: &str,
) {
    let phase = match error {
        kain_core::KainError::Lexer { .. } => "lexer",
        kain_core::KainError::Parser { .. } => "parser",
        kain_core::KainError::Type { .. } => "type",
        kain_core::KainError::Effect { .. } => "effect",
        kain_core::KainError::Borrow { .. } => "borrow",
        kain_core::KainError::Codegen { .. } | kain_core::KainError::CodegenWithLocation { .. } => {
            "codegen"
        }
        kain_core::KainError::Runtime { .. } => "runtime",
        kain_core::KainError::Io(_) => "io",
        kain_core::KainError::Enhanced { .. } => "enhanced",
        kain_core::KainError::Rich(_) => "rich",
        kain_core::KainError::Multi(_) => "multi",
    };
    let captured = capture_rendered_failure(
        "kain-error",
        command,
        rendered_output,
        launcher,
        target,
        Some(source_name),
        source_path,
        error.diagnostic_json(),
        serde_json::json!({
            "phase": phase,
            "source_bytes": source.len(),
            "source_lines": source.lines().count(),
            "error_display": error.to_string(),
        }),
    );
    // THETA-4: One-time notification when capture is active. Without this,
    // diagnostic capture can feel like a silent JSON dump: the user sees a
    // nice rustc-style error in the terminal and never knows that the same
    // event is being appended to `diagnostics/errors.jsonl`. We use a
    // process-wide AtomicBool to ensure we only emit the notice once,
    // regardless of how many errors fire in this `kain` invocation.
    if captured {
        static CAPTURE_NOTIFIED: AtomicBool = AtomicBool::new(false);
        if !CAPTURE_NOTIFIED.swap(true, Ordering::Relaxed) {
            let config = kain_core::tooling_config::active_kain_tooling_config();
            eprintln!(
                "{} Diagnostic capture active - events written to: {}",
                p().status_info(""),
                config.diagnostics.path.display()
            );
        }
    }
}

fn capture_run_report_failure(
    command: &str,
    launcher: Option<LauncherKind>,
    report: &kain_run::RunReport,
) {
    let rendered_output = kain_run::render_compact_output(report);
    if rendered_output.trim().is_empty() {
        return;
    }
    let requested_target = format!("{:?}", report.requested_target).to_ascii_lowercase();
    capture_rendered_failure(
        "run-report-failure",
        command,
        rendered_output,
        launcher,
        Some(&requested_target),
        None,
        None,
        None,
        serde_json::json!({
            "report_path": report.report_path.display().to_string(),
            "events_path": report.events_path.display().to_string(),
            "workspace_root": report.workspace_root.display().to_string(),
            "status": format!("{:?}", report.status),
            "mode": format!("{:?}", report.mode),
            "units": report.units.iter().map(|unit| serde_json::json!({
                "id": &unit.id,
                "target": format!("{:?}", unit.target),
                "status": format!("{:?}", unit.status),
                "exit_code": unit.exit_code,
                "error": &unit.error,
                "inputs": unit.inputs.iter().map(|path| path.display().to_string()).collect::<Vec<_>>(),
            })).collect::<Vec<_>>(),
        }),
    );
}

fn run_inline_native_script(source_name: &str, source: &str) -> bool {
    let cwd = match std::env::current_dir() {
        Ok(path) => path,
        Err(err) => {
            eprintln!(
                " Failed to resolve current directory for inline script: {}",
                err
            );
            return false;
        }
    };
    let mut request = kain_run::InlineKainSourceRequest::new(source_name, source, cwd);
    request.progress = cli::progress::stderr_progress_sink(true);
    request.stream_output = true;
    match kain_run::execute_inline_kain_source(&request) {
        Ok(report) => {
            let success = report.is_success();
            print_compact_run_report(&report, success);
            if !success {
                capture_run_report_failure("inline-script", None, &report);
            }
            success
        }
        Err(err) => {
            eprintln!(" Inline native script failed: {}", err);
            capture_rendered_failure(
                "command-failure",
                "inline-script",
                format!(" Inline native script failed: {err}\n"),
                None,
                Some("llvm"),
                Some(source_name),
                None,
                None,
                serde_json::json!({"phase": "execute-inline"}),
            );
            false
        }
    }
}

fn run_file_as_native_script(input: PathBuf, watch: bool) -> bool {
    let request = match run_cli::make_request(
        Some(input),
        if watch {
            kain_run::RunMode::Dev
        } else {
            kain_run::RunMode::Once
        },
        "llvm".to_string(),
        Vec::new(),
        false,
        false,
        false,
        false,
        false,
    ) {
        Ok(request) => request,
        Err(err) => {
            eprintln!(" Native script setup failed: {}", err);
            capture_rendered_failure(
                "command-failure",
                "native-script",
                format!(" Native script setup failed: {err}\n"),
                None,
                Some("llvm"),
                None,
                None,
                None,
                serde_json::json!({"phase": "setup"}),
            );
            return false;
        }
    };

    if watch {
        if let Err(err) = run_cli::execute(request) {
            eprintln!(" Native script watch failed: {}", err);
            capture_rendered_failure(
                "command-failure",
                "native-script-watch",
                format!(" Native script watch failed: {err}\n"),
                None,
                Some("llvm"),
                None,
                None,
                None,
                serde_json::json!({"phase": "watch"}),
            );
            return false;
        }
        return true;
    }

    match kain_run::execute_run(&request) {
        Ok(report) => {
            let success = report.is_success();
            print_compact_run_report(&report, success);
            if !success {
                capture_run_report_failure("native-script", None, &report);
            }
            success
        }
        Err(err) => {
            eprintln!(" Native script execution failed: {}", err);
            capture_rendered_failure(
                "command-failure",
                "native-script",
                format!(" Native script execution failed: {err}\n"),
                None,
                Some("llvm"),
                None,
                None,
                None,
                serde_json::json!({"phase": "execute-run"}),
            );
            false
        }
    }
}

fn run_source_with_session(
    session: &kain_driver::DriverSession,
    source_name: &str,
    source_path: Option<&Path>,
    source: &str,
    target: CompileTarget,
    output: Option<&PathBuf>,
    _emit_ast: bool,
    _emit_typed: bool,
    verbose: bool,
    _analyze: bool,
    plugin_name: Option<&str>,
) -> bool {
    let source = source.to_string();

    if matches!(target, CompileTarget::Llvm | CompileTarget::C | CompileTarget::BareMetal) {
        if source_has_c_ffi_imports(&source) {
            let prepare = CPrepareContext {
                current_dir: std::env::current_dir().ok(),
                manifest_path: None,
            };
            let import_options = CImportCOptions {
                mode: CArtifactMode::Generate,
                ..CImportCOptions::default()
            };
            if let Err(err) =
                kain_c_ffi::import_libraries_for_source(&source, &import_options, &prepare)
            {
                let target_name = format!("{target:?}").to_ascii_lowercase();
                eprintln!(" Failed to prepare C FFI source: {}", err);
                capture_rendered_failure(
                    "command-failure",
                    "compile",
                    format!(" Failed to prepare C FFI source: {err}\n"),
                    None,
                    Some(&target_name),
                    Some(source_name),
                    source_path,
                    None,
                    serde_json::json!({"phase": "c-ffi-prepare"}),
                );
                return false;
            }
        }
    }

    let native_backend_output_paths = if matches!(target, CompileTarget::Llvm | CompileTarget::C | CompileTarget::BareMetal) {
        let Some(paths) = resolve_native_backend_output_paths(target, output, source_path) else {
            eprintln!(" Output path is required when compiling inline or stdin source.");
            return false;
        };

        match restore_native_executable_cache(target, &source, &paths) {
            Ok(Some(hit)) => {
                println!(
                    "{} Compiled to: {} ({} bytes)",
                    p().status_ok(""),
                    paths.backend_path.display(),
                    hit.backend_bytes
                );
                for sidecar in &hit.restored_sidecars {
                    if sidecar
                        .extension()
                        .and_then(|value| value.to_str())
                        .map_or(false, |ext| ext.eq_ignore_ascii_case("json"))
                    {
                        if sidecar
                            .file_name()
                            .and_then(|value| value.to_str())
                            .map_or(false, |name| name.contains("runtime_contract"))
                        {
                            println!("{} Runtime contract: {}", p().status_info(""), sidecar.display());
                        } else if sidecar
                            .file_name()
                            .and_then(|value| value.to_str())
                            .map_or(false, |name| name.contains("realtime_app"))
                        {
                            println!("{} Realtime bundle: {}", p().status_info(""), sidecar.display());
                        }
                    }
                }
                println!("{} Linking executable...", p().status_info(""));
                eprintln!(
                    "{} Native executable cache hit: {} ({} exe bytes)",
                    p().status_cached(""),
                    hit.key, hit.executable_bytes
                );
                println!("{} Generated executable: {}", p().status_ok(""), paths.executable_path.display());
                return true;
            }
            Ok(None) => {}
            Err(err) => {
                eprintln!(" Failed to restore native executable cache: {}", err);
                return false;
            }
        }

        Some(paths)
    } else {
        None
    };

    if verbose {
        println!(" Compiling: {}", source_name);
        println!(
            " Source: {} bytes, {} lines",
            source.len(),
            source.lines().count()
        );
    }

    // Compile SPIR-V as binary bytes (not the string summary used by compile()).
    if target == CompileTarget::Spirv {
        let output_path =
            match resolve_output_path_with_extension(output, source_path, target_extension(target))
            {
                Some(path) => path,
                None => {
                    eprintln!(" Output path is required when compiling inline or stdin source.");
                    return false;
                }
            };
        match session.compile_spirv_binary(&source) {
            Ok(spv_bytes) => {
                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if let Err(e) = fs::write(&output_path, &spv_bytes) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }
                println!(
                    "{} Compiled to: {} ({} bytes)",
                    p().status_ok(""),
                    output_path.display(),
                    spv_bytes.len()
                );
                return true;
            }
            Err(e) => {
                let rendered = session.format_error(source_name, &source, &e);
                eprint!("{rendered}");
                capture_kain_error_failure(
                    "compile",
                    None,
                    Some("spirv"),
                    source_name,
                    source_path,
                    &source,
                    &e,
                    &rendered,
                );
                return false;
            }
        }
    }

    if target == CompileTarget::Wasm {
        let output_path =
            match resolve_output_path_with_extension(output, source_path, target_extension(target))
            {
                Some(path) => path,
                None => {
                    eprintln!(" Output path is required when compiling inline or stdin source.");
                    return false;
                }
            };
        match session.compile_wasm_binary(&source) {
            Ok(wasm_bytes) => {
                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if let Err(e) = fs::write(&output_path, &wasm_bytes) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }
                println!(
                    "{} Compiled to: {} ({} bytes)",
                    p().status_ok(""),
                    output_path.display(),
                    wasm_bytes.len()
                );
                return true;
            }
            Err(e) => {
                let rendered = session.format_error(source_name, &source, &e);
                eprint!("{rendered}");
                capture_kain_error_failure(
                    "compile",
                    None,
                    Some("wasm"),
                    source_name,
                    source_path,
                    &source,
                    &e,
                    &rendered,
                );
                return false;
            }
        }
    }

    if target == CompileTarget::Hybrid {
        let descriptor_path =
            match resolve_output_path_with_extension(output, source_path, "hybrid") {
                Some(path) => path,
                None => {
                    eprintln!(" Output path is required when compiling inline or stdin source.");
                    return false;
                }
            };
        match session.compile_hybrid_artifacts(&source) {
            Ok(artifacts) => match write_hybrid_bundle(&descriptor_path, artifacts) {
                Ok(written) => {
                    println!("{} Compiled to: {}", p().status_ok(""), written.descriptor_path.display());
                    println!("{} Hybrid JS: {}", p().status_info(""), written.js_path.display());
                    println!("{} Hybrid TS: {}", p().status_info(""), written.ts_path.display());
                    println!("{} Hybrid WASM: {}", p().status_info(""), written.wasm_path.display());
                    return true;
                }
                Err(err) => {
                    eprintln!(" Failed to materialize hybrid bundle: {}", err);
                    return false;
                }
            },
            Err(e) => {
                let rendered = session.format_error(source_name, &source, &e);
                eprint!("{rendered}");
                capture_kain_error_failure(
                    "compile",
                    None,
                    Some("hybrid"),
                    source_name,
                    source_path,
                    &source,
                    &e,
                    &rendered,
                );
                return false;
            }
        }
    }

    // Compile
    match session.compile_with_source_path(&source, source_path, target) {
        Ok(mut compiled_output) => {
            if target == CompileTarget::Interpret || target == CompileTarget::Test {
                let trimmed_output = compiled_output.trim();
                if !trimmed_output.is_empty() && trimmed_output != "()" {
                    println!("{}", compiled_output);
                }
                println!("{} Execution complete", p().status_ok(""));
            } else {
                let default_ext = target_extension(target);

                // Determine where to write the primary output
                let output_path = if matches!(target, CompileTarget::Llvm | CompileTarget::C | CompileTarget::BareMetal) {
                    // For raw-native backends, always write the backend source artifact first.
                    native_backend_output_paths
                        .as_ref()
                        .map(|paths| paths.backend_path.clone())
                        .expect("native backend output paths are resolved before compile")
                } else if target == CompileTarget::Usf {
                    // For USF, always ensure .usf extension
                    if let Some(out) = output {
                        let mut p = out.clone();
                        p.set_extension("usf");
                        p
                    } else if let Some(path) = source_path {
                        path.with_extension("usf")
                    } else {
                        eprintln!(
                            " Output path is required when compiling inline or stdin source."
                        );
                        return false;
                    }
                } else {
                    match output
                        .cloned()
                        .or_else(|| source_path.map(|path| path.with_extension(default_ext)))
                    {
                        Some(path) => path,
                        None => {
                            eprintln!(
                                " Output path is required when compiling inline or stdin source."
                            );
                            return false;
                        }
                    }
                };

                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if target == CompileTarget::Llvm || target == CompileTarget::BareMetal {
                    match slice_llvm_native_executable_ir(&compiled_output) {
                        Ok(Some((sliced_output, stats))) => {
                            eprintln!(
                                " Native LLVM IR sliced: {} -> {} bytes, kept {}/{} functions (removed {}, declarations {})",
                                stats.original_bytes,
                                stats.sliced_bytes,
                                stats.kept_functions,
                                stats.original_functions,
                                stats.removed_functions,
                                stats.removed_declarations
                            );
                            compiled_output = sliced_output;
                        }
                        Ok(None) => {}
                        Err(err) => {
                            eprintln!(" Failed to slice native LLVM IR: {}", err);
                            return false;
                        }
                    }
                }

                if let Err(e) = fs::write(&output_path, &compiled_output) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }

                println!(
                    "{} Compiled to: {} ({} bytes)",
                    p().status_ok(""),
                    output_path.display(),
                    compiled_output.len()
                );

                let mut native_artifacts_require_gpu_runtime = false;

                if matches!(target, CompileTarget::Llvm | CompileTarget::C | CompileTarget::BareMetal) {
                    match llvm_native_stage::stage_native_backend_artifacts_with_session(
                        session,
                        &source,
                        source_path,
                        target,
                        &output_path,
                        None,
                    ) {
                        Ok(staged) => {
                            native_artifacts_require_gpu_runtime =
                                staged.requires_gpu_runtime_dll();
                            println!(
                                "{} Runtime contract: {}",
                                p().status_info(""),
                                staged.runtime_contract_path.display()
                            );
                            println!("{} Realtime bundle: {}", p().status_info(""), staged.realtime_app_path.display());
                            if let Some(compute_residency_path) = staged.compute_residency_path {
                                println!(
                                    "{} Compute residency: {}",
                                    p().status_info(""),
                                    compute_residency_path.display()
                                );
                            }
                            if let Some(shader_bundle_path) = staged.shader_bundle_path {
                                println!("{} Shader bundle: {}", p().status_info(""), shader_bundle_path.display());
                            }
                        }
                        Err(err) => {
                            let rendered = session.format_error(source_name, &source, &err);
                            eprint!("{rendered}");
                            capture_kain_error_failure(
                                "compile",
                                None,
                                Some("llvm"),
                                source_name,
                                source_path,
                                &source,
                                &err,
                                &rendered,
                            );
                            return false;
                        }
                    }
                }

                for advisory in session.frontend_advisories() {
                    eprintln!("{} Warning: {}", p().status_warning(""), advisory);
                }

                // Generate C++ reflection header for USF shaders (GODMODE Phase 3)
                if target == CompileTarget::Usf {
                    // Extract shader name from AST instead of filename
                    let shader_name = {
                        // Parse the source to get the actual shader name
                        match kain_core::Lexer::new(&source).tokenize() {
                            Ok(tokens) => {
                                let span_mapper = kain_core::diagnostics::SpanMapper::new(&source);
                                match kain_core::Parser::new(&tokens, &span_mapper, source_name)
                                    .parse()
                                {
                                    Ok(ast) => {
                                        // Find the first shader in the AST
                                        ast.items
                                            .iter()
                                            .find_map(|item| {
                                                if let kain_core::ast::Item::Shader(shader) = item {
                                                    Some(shader.name.clone())
                                                } else {
                                                    None
                                                }
                                            })
                                            .unwrap_or_else(|| {
                                                source_path
                                                    .and_then(|path| path.file_stem())
                                                    .and_then(|s| s.to_str())
                                                    .unwrap_or("Shader")
                                                    .to_string()
                                            })
                                    }
                                    Err(_) => source_path
                                        .and_then(|path| path.file_stem())
                                        .and_then(|s| s.to_str())
                                        .unwrap_or("Shader")
                                        .to_string(),
                                }
                            }
                            Err(_) => source_path
                                .and_then(|path| path.file_stem())
                                .and_then(|s| s.to_str())
                                .unwrap_or("Shader")
                                .to_string(),
                        }
                    };

                    // Generate header (.h)
                    match cli::generate_usf_header(&source, &shader_name) {
                        Ok(header_code) => {
                            let header_path = output_path.with_extension("h");
                            let header_path = output_path.with_extension("h");
                            if !ensure_parent_dir(&header_path) {
                                eprintln!(" Warning: Failed to create directory for header");
                            } else if let Err(e) = fs::write(&header_path, header_code.as_bytes()) {
                                eprintln!(" Warning: Failed to write header: {}", e);
                            } else {
                                println!(" Generated header: {}", header_path.display());
                            }
                        }
                        Err(e) => {
                            eprintln!(" Warning: Failed to generate header: {}", e);
                        }
                    }

                    // Generate implementation (.cpp) with shader registration
                    let plugin_name_str = plugin_name.unwrap_or("YourPlugin");
                    match cli::generate_usf_implementation(&source, &shader_name, plugin_name_str) {
                        Ok(cpp_code) => {
                            let cpp_path = output_path.with_extension("cpp");
                            let cpp_path = output_path.with_extension("cpp");
                            if !ensure_parent_dir(&cpp_path) {
                                eprintln!(
                                    " Warning: Failed to create directory for implementation"
                                );
                            } else if let Err(e) = fs::write(&cpp_path, cpp_code.as_bytes()) {
                                eprintln!(" Warning: Failed to write implementation: {}", e);
                            } else {
                                println!(" Generated implementation: {}", cpp_path.display());
                            }
                        }
                        Err(e) => {
                            eprintln!(" Warning: Failed to generate implementation: {}", e);
                        }
                    }

                    // GODMODE Phase 7: Shader Complexity Analysis
                    // TODO: Re-implement analyze_shader_complexity
                    // if verbose || analyze {
                    //     match ue5_shaders::analyze_shader_complexity(&source) {
                    //         Ok(report) => {
                    //             println!("{}", report);
                    //         },
                    //         Err(e) => {
                    //             eprintln!(" Warning: Failed to analyze shader: {}", e);
                    //         }
                    //     }
                    // }
                }

                // Generate separate .h and .cpp files for UE5 target (GODMODE)
                if target == CompileTarget::Ue5 {
                    let output_name = output_path.file_stem().and_then(|s| s.to_str());

                    match cli::compile_ue5(&source, output_name, None) {
                        Ok(ue5_output) => {
                            // Write header file
                            let header_path = output_path.with_extension("h");
                            let header_path = output_path.with_extension("h");
                            if !ensure_parent_dir(&header_path) {
                                eprintln!(" Warning: Failed to create directory for header");
                            } else if let Err(e) =
                                fs::write(&header_path, ue5_output.header.as_bytes())
                            {
                                eprintln!(" Warning: Failed to write header: {}", e);
                            } else {
                                println!(
                                    " Generated header: {} ({} bytes)",
                                    header_path.display(),
                                    ue5_output.header.len()
                                );
                            }

                            // Write source file
                            let source_path = output_path.with_extension("cpp");
                            let source_path = output_path.with_extension("cpp");
                            if !ensure_parent_dir(&source_path) {
                                eprintln!(" Warning: Failed to create directory for source");
                            } else if let Err(e) =
                                fs::write(&source_path, ue5_output.source.as_bytes())
                            {
                                eprintln!(" Warning: Failed to write source: {}", e);
                            } else {
                                println!(
                                    " Generated source: {} ({} bytes)",
                                    source_path.display(),
                                    ue5_output.source.len()
                                );
                            }

                            // Write shader files (USF + shader headers + shader cpp)
                            if !ue5_output.shader_files.is_empty() {
                                println!(
                                    " Generated {} shader files:",
                                    ue5_output.shader_files.len()
                                );
                                for (filename, content) in &ue5_output.shader_files {
                                    let shader_path = output_path.with_file_name(filename);
                                    let shader_path = output_path.with_file_name(filename);
                                    if !ensure_parent_dir(&shader_path) {
                                        eprintln!(
                                            "   Warning: Failed to create directory for {}",
                                            filename
                                        );
                                    } else if let Err(e) =
                                        fs::write(&shader_path, content.as_bytes())
                                    {
                                        eprintln!(
                                            "   Warning: Failed to write {}: {}",
                                            filename, e
                                        );
                                    } else {
                                        println!(
                                            "   - {} ({} bytes)",
                                            shader_path.display(),
                                            content.len()
                                        );
                                    }
                                }
                            }
                        }
                        Err(e) => {
                            eprintln!(" Warning: Failed to generate split UE5 files: {}", e);
                        }
                    }
                }

                // Generate separate .h and .cpp files for UE5 Editor target
                if target == CompileTarget::Ue5Editor {
                    match cli::compile_ue5editor(&source, "EditorTools", None) {
                        Ok(editor_output) => {
                            // Write header file
                            let header_path = output_path.with_extension("h");
                            let header_path = output_path.with_extension("h");
                            if !ensure_parent_dir(&header_path) {
                                eprintln!(" Warning: Failed to create directory for header");
                            } else if let Err(e) =
                                fs::write(&header_path, editor_output.header.as_bytes())
                            {
                                eprintln!(" Warning: Failed to write header: {}", e);
                            } else {
                                println!(
                                    " Generated header: {} ({} bytes)",
                                    header_path.display(),
                                    editor_output.header.len()
                                );
                            }

                            // Write source file
                            let source_path = output_path.with_extension("cpp");
                            let source_path = output_path.with_extension("cpp");
                            if !ensure_parent_dir(&source_path) {
                                eprintln!(" Warning: Failed to create directory for source");
                            } else if let Err(e) =
                                fs::write(&source_path, editor_output.source.as_bytes())
                            {
                                eprintln!(" Warning: Failed to write source: {}", e);
                            } else {
                                println!(
                                    " Generated source: {} ({} bytes)",
                                    source_path.display(),
                                    editor_output.source.len()
                                );
                            }
                        }
                        Err(e) => {
                            eprintln!(" Warning: Failed to generate UE5 Editor files: {}", e);
                        }
                    }
                }

                // Post-processing for raw-native backends
                if matches!(target, CompileTarget::Llvm | CompileTarget::C | CompileTarget::BareMetal) {
                    let exe_path = native_backend_output_paths
                        .as_ref()
                        .map(|paths| paths.executable_path.clone())
                        .expect("native backend output paths are resolved before link");

                    println!(" Linking executable...");

                    // Find clang via canonical discovery
                    let clang_cmd = kain_core::install_layout::find_clang()
                        .map(|p| p.to_string_lossy().to_string())
                        .unwrap_or_else(|| "clang".to_string());

                    let native_toolchain_tuning = match resolve_native_toolchain_tuning() {
                        Ok(tuning) => tuning,
                        Err(err) => {
                            eprintln!(" Failed to resolve native toolchain tuning: {}", err);
                            return false;
                        }
                    };
                    let mut runtime_link_libs = Vec::new();
                    let mut runtime_artifacts = NativeRuntimeCompiledArtifacts::default();
                    let cffi_source = session
                        .frontend_full_source()
                        .unwrap_or_else(|| source.clone());
                    let cffi_link_inputs = match resolve_c_ffi_native_link_inputs(
                        &cffi_source,
                        source_path,
                        &clang_cmd,
                        &native_toolchain_tuning,
                    ) {
                        Ok(inputs) => inputs,
                        Err(err) => {
                            eprintln!(" Failed to resolve C FFI link inputs: {}", err);
                            return false;
                        }
                    };

                    // DEPRECATED(2026-06-08): --gc-sections handles dead-stripping.
                    // This elision check will be removed after platform stability.
                    let native_runtime_elision = match llvm_native_runtime_elision_decision(
                        target,
                        &output_path,
                        &cffi_link_inputs,
                        native_artifacts_require_gpu_runtime,
                    ) {
                        Ok(decision) => decision,
                        Err(err) => {
                            eprintln!(" Failed to analyze native runtime elision: {}", err);
                            return false;
                        }
                    };

                    if native_runtime_elision.can_elide {
                        eprintln!(
                            " Native runtime elided: {} ({} reachable functions)",
                            native_runtime_elision.reason,
                            native_runtime_elision.reachable_functions
                        );
                    } else {
                        if !native_runtime_elision.reachable_external_targets.is_empty() {
                            eprintln!(
                                " Native runtime required: {}: {}",
                                native_runtime_elision.reason,
                                native_runtime_elision.reachable_external_targets.join(", ")
                            );
                        }
                        // Prefer precompiled runtime archive over source compilation.
                        // The archive is built with -ffunction-sections and linked
                        // with --gc-sections so unused functions are dead-stripped.
                        if let Some(archive) = resolve_precompiled_runtime_archive() {
                            eprintln!(
                                " Native runtime archive: {}",
                                archive.path.display()
                            );
                            runtime_artifacts = NativeRuntimeCompiledArtifacts {
                                loose_objects: Vec::new(),
                                static_archives: vec![archive.path],
                            };
                            runtime_link_libs = archive.link_libs;
                        } else if let Some(runtime_bundle) = match resolve_native_runtime_bundle()
                        {
                            Ok(bundle) => bundle,
                            Err(err) => {
                                eprintln!(
                                    " Failed to resolve native runtime bundle: {}",
                                    err
                                );
                                return false;
                            }
                        } {
                            runtime_artifacts = match compile_native_runtime_bundle(
                                &runtime_bundle,
                                &clang_cmd,
                                &native_toolchain_tuning,
                            ) {
                                Ok(artifacts) => artifacts,
                                Err(err) => {
                                    eprintln!(" Failed to compile runtime library: {}", err);
                                    return false;
                                }
                            };
                            runtime_link_libs = runtime_bundle.link_libs;
                            if runtime_bundle.uses_cpp_runtime {
                                runtime_link_libs = unique_link_libs(
                                    [runtime_link_libs, default_native_runtime_cpp_link_libs()]
                                        .concat(),
                                );
                            }
                        }
                    }

                    // ── Build toolchain tuning extra args ────────────
                    let mut clang_extra_args = native_toolchain_tuning.clang_compile_args();
                    if target == CompileTarget::C {
                        clang_extra_args.push("-std=c11".to_string());
                    }
                    // Additional GC/dead-strip tuning beyond what link_native_binary adds
                    if cfg!(windows) && !native_toolchain_tuning.debug_info {
                        clang_extra_args.push("-Wl,/DEBUG:NONE".to_string());
                    }
                    if native_toolchain_tuning.profile != NativeToolchainProfile::Debug {
                        if cfg!(windows) {
                            clang_extra_args.push("-Wl,/OPT:ICF".to_string());
                        }
                    }

                    // ── Build NativeRuntimeArtifacts ─────────────────
                    let link_artifacts = if native_runtime_elision.can_elide
                        && cffi_link_inputs.link_inputs.is_empty()
                    {
                        // Pure compute — empty artifacts, link_native_binary uses -nostdlib
                        kain_build::NativeRuntimeArtifacts::default()
                    } else if native_runtime_elision.can_elide {
                        // Runtime elided but C FFI inputs exist
                        kain_build::NativeRuntimeArtifacts {
                            loose_objects: cffi_link_inputs.link_inputs.clone(),
                            static_archives: Vec::new(),
                            link_libs: cffi_link_inputs.link_libs.clone(),
                            library_search_paths: cffi_link_inputs.library_search_paths.clone(),
                        }
                    } else {
                        // Runtime needed — combine runtime artifacts + C FFI inputs
                        let mut loose = runtime_artifacts.loose_objects.clone();
                        loose.extend(cffi_link_inputs.link_inputs.clone());
                        // Don't include platform link libs — link_native_binary adds them
                        let libs = unique_link_libs(
                            [
                                runtime_link_libs.clone(),
                                cffi_link_inputs.link_libs.clone(),
                            ]
                            .concat(),
                        );
                        kain_build::NativeRuntimeArtifacts {
                            loose_objects: loose,
                            static_archives: runtime_artifacts.static_archives.clone(),
                            link_libs: libs,
                            library_search_paths: cffi_link_inputs.library_search_paths.clone(),
                        }
                    };

                    // ── Link via shared native_link path ─────────────
                    let link_req = kain_build::NativeLinkRequest {
                        emit: kain_build::NativeEmit::Exe,
                        llvm_ir_path: &output_path,
                        output_path: &exe_path,
                        source_text: &source,
                        runtime_artifacts: link_artifacts,
                        extra_args: clang_extra_args,
                        compile_target: target,
                    };

                    match kain_build::link_native_binary(&link_req) {
                        Ok(_) => {
                            println!(" Generated executable: {}", exe_path.display());
                            if native_runtime_elision.can_elide {
                                match store_native_executable_cache(
                                    target,
                                    &source,
                                    &output_path,
                                    &exe_path,
                                    &native_toolchain_tuning,
                                ) {
                                    Ok(Some(key)) => {
                                        eprintln!(" Native executable cache stored: {}", key);
                                    }
                                    Ok(None) => {}
                                    Err(err) => {
                                        eprintln!(
                                            " Warning: Failed to store native executable cache: {}",
                                            err
                                        );
                                    }
                                }
                            }
                            if native_artifacts_require_gpu_runtime {
                                match llvm_native_stage::stage_gpu_runtime_dll(&exe_path) {
                                    Ok(Some(dll_path)) => {
                                        println!(" Compute runtime DLL: {}", dll_path.display());
                                    }
                                    Ok(None) => {}
                                    Err(err) => {
                                        let gpu_error = kain_core::error::KainError::rich(
                                            kain_core::error::DiagnosticReport::new(
                                                kain_core::error::ErrorKind::Codegen,
                                                kain_error::DiagnosticCode::CodegenBackendFailed,
                                                format!("Failed to stage compute runtime DLL: {}", err),
                                            )
                                            .severity(kain_core::error::DiagnosticSeverity::Error)
                                            .phase(kain_core::error::CompilerPhase::Codegen)
                                            .help("Ensure the GPU runtime library is built and accessible."),
                                        );
                                        let rendered = session.format_error(source_name, &source, &gpu_error);
                                        eprint!("{rendered}");
                                        capture_kain_error_failure(
                                            "compile",
                                            None,
                                            Some("llvm"),
                                            source_name,
                                            source_path,
                                            &source,
                                            &gpu_error,
                                            &rendered,
                                        );
                                        return false;
                                    }
                                }
                            }
                        }
                        Err(err) => {
                            let link_error = kain_core::error::KainError::rich(
                                kain_core::error::DiagnosticReport::new(
                                    kain_core::error::ErrorKind::Codegen,
                                    kain_error::DiagnosticCode::CodegenLinkingFailed,
                                    format!("Native linking failed: {}", err),
                                )
                                .severity(kain_core::error::DiagnosticSeverity::Error)
                                .phase(kain_core::error::CompilerPhase::Linking)
                                .note("The native linker reported an error.")
                                .help("Check that all required native libraries are installed and accessible."),
                            );
                            let rendered = session.format_error(source_name, &source, &link_error);
                            eprint!("{rendered}");
                            capture_kain_error_failure(
                                "compile",
                                None,
                                Some("llvm"),
                                source_name,
                                source_path,
                                &source,
                                &link_error,
                                &rendered,
                            );
                            return false;
                        }
                    }
                }
            }
            true
        }
        Err(e) => {
            let rendered = session.format_error(source_name, &source, &e);
            let target_name = format!("{target:?}").to_ascii_lowercase();
            eprint!("{rendered}");
            capture_kain_error_failure(
                "compile",
                None,
                Some(&target_name),
                source_name,
                source_path,
                &source,
                &e,
                &rendered,
            );
            false
        }
    }
}

fn run_compile(
    input: &PathBuf,
    target: CompileTarget,
    output: Option<&PathBuf>,
    emit_ast: bool,
    emit_typed: bool,
    verbose: bool,
    analyze: bool,
    plugin_name: Option<&str>,
) -> bool {
    let session = kain_driver::DriverSession::new();
    run_compile_with_session(
        &session,
        input,
        target,
        output,
        emit_ast,
        emit_typed,
        verbose,
        analyze,
        plugin_name,
    )
}

fn run_compile_with_session(
    session: &kain_driver::DriverSession,
    input: &PathBuf,
    target: CompileTarget,
    output: Option<&PathBuf>,
    emit_ast: bool,
    emit_typed: bool,
    verbose: bool,
    analyze: bool,
    plugin_name: Option<&str>,
) -> bool {
    let source = match read_source_from_path(input) {
        Ok(value) => value,
        Err(err) => {
            eprintln!(" {}", err);
            return false;
        }
    };
    let source_name = if input == Path::new("-") {
        "<stdin>"
    } else {
        input.to_str().unwrap_or("<input>")
    };
    let source_path = if input == Path::new("-") {
        None
    } else {
        Some(input.as_path())
    };
    run_source_with_session(
        session,
        source_name,
        source_path,
        &source,
        target,
        output,
        emit_ast,
        emit_typed,
        verbose,
        analyze,
        plugin_name,
    )
}

fn watch_inputs_for_session(session: &kain_driver::DriverSession, input: &Path) -> Vec<PathBuf> {
    let mut watch_roots = Vec::new();
    let frontend_inputs = session.frontend_watch_inputs();
    if frontend_inputs.is_empty() {
        push_unique_watch_root(&mut watch_roots, input.to_path_buf());
        return watch_roots;
    }

    for watched_input in frontend_inputs {
        push_unique_watch_root(&mut watch_roots, watched_input);
    }
    watch_roots
}

fn push_unique_watch_root(watch_roots: &mut Vec<PathBuf>, path: PathBuf) {
    let watch_root = if path.is_dir() {
        path
    } else if let Some(parent) = path.parent() {
        parent.to_path_buf()
    } else {
        path
    };
    if !watch_roots.iter().any(|existing| existing == &watch_root) {
        watch_roots.push(watch_root);
    }
}

fn build_watch_mode_watcher(
    tx: std::sync::mpsc::Sender<Result<notify::Event, notify::Error>>,
    watch_roots: &[PathBuf],
) -> notify::Result<notify::RecommendedWatcher> {
    use notify::{RecursiveMode, Watcher};

    let mut watcher = notify::recommended_watcher(move |res| {
        let _ = tx.send(res);
    })?;

    for watch_root in watch_roots {
        if !watch_root.exists() {
            continue;
        }
        watcher.watch(watch_root, RecursiveMode::NonRecursive)?;
    }

    Ok(watcher)
}

fn print_kain_build_report(report: &kain_build::BladeBuildReport) {
    let painter = p();
    let status_word = if report.is_success() {
        painter.status_ok("succeeded")
    } else {
        painter.status_error("failed")
    };
    println!(
        " Build {}: lane={} target={} host={}",
        status_word, report.lane.as_str(), report.target, report.host
    );
    println!(" Artifact root: {}", report.artifact_root.display());
    println!(" Report: {}", report.report_path.display());
    let cached_tasks = report
        .tasks
        .iter()
        .filter(|task| task.status == kain_build::BuildTaskStatus::Cached || task.cache_hit)
        .count();
    for task in &report.tasks {
        let marker = match task.status {
            kain_build::BuildTaskStatus::Cached => painter.status_cached("cached"),
            kain_build::BuildTaskStatus::Succeeded => painter.status_ok("ok"),
            kain_build::BuildTaskStatus::Planned => painter.status_info("planned"),
            kain_build::BuildTaskStatus::Skipped => painter.status_muted("skipped"),
            kain_build::BuildTaskStatus::Failed => painter.status_error("failed"),
        };
        println!("   {} {}", marker, task.id);
        for output in &task.outputs {
            println!("      {}", output.display());
        }
        if let Some(error) = &task.error {
            eprintln!("      {}", error);
        }
    }
    println!(
        " Freshness: SHA-256 task stamps guard build cache reuse across inputs, adapter state, and output layout"
    );
    println!(" Cache hits: {}", cached_tasks);
}

fn run_kn_repl() -> bool {
    run_terminal_repl(ReplTerminalConfig::new(ReplBuildMetadata::new(
        "Kain",
        VERSION,
        BUILD_NUMBER,
        BUILD_TARGET_TRIPLE,
    )))
}

fn env_var_truthy(name: &str) -> bool {
    match std::env::var(name) {
        Ok(value) => {
            let normalized = value.trim().to_ascii_lowercase();
            matches!(normalized.as_str(), "1" | "true" | "yes" | "on")
        }
        Err(_) => false,
    }
}

fn parse_bool_env_value(name: &str, value: &str) -> Result<bool, String> {
    let normalized = value.trim().to_ascii_lowercase();
    match normalized.as_str() {
        "1" | "true" | "yes" | "on" => Ok(true),
        "0" | "false" | "no" | "off" => Ok(false),
        _ => Err(format!(
            "{name} must be one of 1, 0, true, false, yes, no, on, off"
        )),
    }
}

fn native_runtime_elision_enabled() -> Result<bool, String> {
    match std::env::var("KAIN_NATIVE_RUNTIME_ELISION") {
        Ok(value) => parse_bool_env_value("KAIN_NATIVE_RUNTIME_ELISION", &value),
        Err(_) => Ok(true),
    }
}

fn native_llvm_ir_slicing_enabled() -> Result<bool, String> {
    match std::env::var("KAIN_NATIVE_LLVM_IR_SLICING") {
        Ok(value) => parse_bool_env_value("KAIN_NATIVE_LLVM_IR_SLICING", &value),
        Err(_) => Ok(true),
    }
}

fn native_executable_cache_enabled() -> Result<bool, String> {
    match std::env::var("KAIN_NATIVE_EXEC_CACHE") {
        Ok(value) => parse_bool_env_value("KAIN_NATIVE_EXEC_CACHE", &value),
        Err(_) => Ok(true),
    }
}

fn source_has_c_ffi_imports(source: &str) -> bool {
    !kain_c_ffi::detect_c_library_imports(source).is_empty()
}

fn resolve_native_backend_output_paths(
    target: CompileTarget,
    output: Option<&PathBuf>,
    source_path: Option<&Path>,
) -> Option<NativeBackendOutputPaths> {
    if !matches!(target, CompileTarget::Llvm | CompileTarget::C | CompileTarget::BareMetal) {
        return None;
    }

    let backend_path = if let Some(out) = output {
        if out
            .extension()
            .map_or(false, |e| e == target_extension(target))
        {
            out.clone()
        } else {
            let mut path = out.clone();
            path.set_extension(target_extension(target));
            path
        }
    } else {
        source_path?.with_extension(target_extension(target))
    };

    let executable_path = if let Some(out) = output {
        if out
            .extension()
            .map_or(false, |e| e == target_extension(target))
        {
            if cfg!(windows) {
                out.with_extension("exe")
            } else {
                out.with_extension("")
            }
        } else {
            out.clone()
        }
    } else if cfg!(windows) {
        source_path?.with_extension("exe")
    } else {
        source_path?.with_extension("")
    };

    Some(NativeBackendOutputPaths {
        backend_path,
        executable_path,
    })
}

fn restore_native_executable_cache(
    target: CompileTarget,
    source: &str,
    paths: &NativeBackendOutputPaths,
) -> Result<Option<NativeExecutableCacheHit>, String> {
    if !native_executable_cache_enabled()? {
        return Ok(None);
    }

    let toolchain_tuning = resolve_native_toolchain_tuning()?;
    let fingerprint = build_native_executable_cache_fingerprint(target, source, &toolchain_tuning);
    let Some(cache_root) = default_native_executable_cache_root() else {
        return Ok(None);
    };
    let key = native_executable_cache_key(&fingerprint, source);
    let cache_dir = cache_root.join(&key);
    let fingerprint_path = cache_dir.join("fingerprint.txt");
    let source_path = cache_dir.join("source.kn");
    let backend_cache_path = cache_dir.join(format!("artifact.{}", target_extension(target)));
    let executable_cache_path = cache_dir.join(native_executable_cache_file_name());

    if !fingerprint_path.exists()
        || !source_path.exists()
        || !backend_cache_path.exists()
        || !executable_cache_path.exists()
    {
        return Ok(None);
    }

    let stored_fingerprint = fs::read_to_string(&fingerprint_path).map_err(|err| {
        format!(
            "unable to read native executable cache fingerprint {}: {}",
            fingerprint_path.display(),
            err
        )
    })?;
    if stored_fingerprint != fingerprint {
        return Ok(None);
    }

    let stored_source = fs::read_to_string(&source_path).map_err(|err| {
        format!(
            "unable to read native executable cache source {}: {}",
            source_path.display(),
            err
        )
    })?;
    if stored_source != source {
        return Ok(None);
    }

    ensure_copy_parent(&paths.backend_path)?;
    ensure_copy_parent(&paths.executable_path)?;
    fs::copy(&backend_cache_path, &paths.backend_path).map_err(|err| {
        format!(
            "unable to restore native backend cache {} -> {}: {}",
            backend_cache_path.display(),
            paths.backend_path.display(),
            err
        )
    })?;
    fs::copy(&executable_cache_path, &paths.executable_path).map_err(|err| {
        format!(
            "unable to restore native executable cache {} -> {}: {}",
            executable_cache_path.display(),
            paths.executable_path.display(),
            err
        )
    })?;

    let restored_sidecars =
        restore_native_executable_cache_sidecars(&cache_dir, &paths.backend_path)?;
    let backend_bytes = fs::metadata(&paths.backend_path)
        .map(|metadata| metadata.len())
        .unwrap_or(0);
    let executable_bytes = fs::metadata(&paths.executable_path)
        .map(|metadata| metadata.len())
        .unwrap_or(0);

    Ok(Some(NativeExecutableCacheHit {
        key,
        backend_bytes,
        executable_bytes,
        restored_sidecars,
    }))
}

fn store_native_executable_cache(
    target: CompileTarget,
    source: &str,
    output_path: &Path,
    exe_path: &Path,
    toolchain_tuning: &NativeToolchainTuning,
) -> Result<Option<String>, String> {
    if !native_executable_cache_enabled()? {
        return Ok(None);
    }
    if !output_path.exists() || !exe_path.exists() {
        return Ok(None);
    }

    let Some(cache_root) = default_native_executable_cache_root() else {
        return Ok(None);
    };
    let fingerprint = build_native_executable_cache_fingerprint(target, source, toolchain_tuning);
    let key = native_executable_cache_key(&fingerprint, source);
    let cache_dir = cache_root.join(&key);
    fs::create_dir_all(&cache_dir).map_err(|err| {
        format!(
            "unable to create native executable cache directory {}: {}",
            cache_dir.display(),
            err
        )
    })?;

    let backend_cache_path = cache_dir.join(format!("artifact.{}", target_extension(target)));
    let executable_cache_path = cache_dir.join(native_executable_cache_file_name());
    fs::copy(output_path, &backend_cache_path).map_err(|err| {
        format!(
            "unable to store native backend cache {} -> {}: {}",
            output_path.display(),
            backend_cache_path.display(),
            err
        )
    })?;
    fs::copy(exe_path, &executable_cache_path).map_err(|err| {
        format!(
            "unable to store native executable cache {} -> {}: {}",
            exe_path.display(),
            executable_cache_path.display(),
            err
        )
    })?;
    store_native_executable_cache_sidecars(&cache_dir, output_path)?;
    fs::write(cache_dir.join("source.kn"), source).map_err(|err| {
        format!(
            "unable to write native executable cache source under {}: {}",
            cache_dir.display(),
            err
        )
    })?;
    fs::write(cache_dir.join("fingerprint.txt"), fingerprint).map_err(|err| {
        format!(
            "unable to write native executable cache fingerprint under {}: {}",
            cache_dir.display(),
            err
        )
    })?;

    Ok(Some(key))
}

fn default_native_executable_cache_root() -> Option<PathBuf> {
    if let Ok(raw) = std::env::var("KAIN_NATIVE_EXEC_CACHE_DIR") {
        let trimmed = raw.trim();
        if !trimmed.is_empty() {
            return Some(PathBuf::from(trimmed));
        }
    }

    kain_core::install_layout::default_kain_install_layout()
        .map(|layout| layout.cache_dir.join("native-exec"))
}

fn build_native_executable_cache_fingerprint(
    target: CompileTarget,
    source: &str,
    toolchain_tuning: &NativeToolchainTuning,
) -> String {
    let mut lines = vec![
        "kain-native-executable-cache-v1".to_string(),
        format!("target={target:?}"),
        format!("target_extension={}", target_extension(target)),
        format!("version={VERSION}"),
        format!("build_number={BUILD_NUMBER}"),
        format!("build_profile={BUILD_PROFILE}"),
        format!("build_target={BUILD_TARGET_TRIPLE}"),
        format!("build_host={BUILD_HOST_TRIPLE}"),
        format!("build_git_sha={BUILD_GIT_SHA}"),
        format!("build_git_dirty={BUILD_GIT_DIRTY}"),
        format!("build_tracking_mode={BUILD_TRACKING_MODE}"),
        format!("source_len={}", source.len()),
        format!("source_hash={:016x}", stable_fnv1a64(source.as_bytes())),
        format!(
            "clang_env={}",
            std::env::var(kain_core::install_layout::KAIN_CLANG_ENV_VAR).unwrap_or_default()
        ),
        "runtime_mode=elided".to_string(),
    ];
    if BUILD_GIT_DIRTY != "false" {
        lines.push(format!("build_unix_time={BUILD_UNIX_TIME}"));
    }
    lines.extend(toolchain_tuning.fingerprint_lines());
    lines.join("\n")
}

fn native_executable_cache_key(fingerprint: &str, source: &str) -> String {
    let mut hash = FNV1A64_OFFSET;
    hash = stable_fnv1a64_extend(hash, fingerprint.as_bytes());
    hash = stable_fnv1a64_extend(hash, b"\n--source--\n");
    hash = stable_fnv1a64_extend(hash, source.as_bytes());
    format!("{hash:016x}")
}

const FNV1A64_OFFSET: u64 = 0xcbf29ce484222325;
const FNV1A64_PRIME: u64 = 0x100000001b3;

fn stable_fnv1a64(bytes: &[u8]) -> u64 {
    stable_fnv1a64_extend(FNV1A64_OFFSET, bytes)
}

fn stable_fnv1a64_extend(mut hash: u64, bytes: &[u8]) -> u64 {
    for byte in bytes {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(FNV1A64_PRIME);
    }
    hash
}

fn native_executable_cache_file_name() -> &'static str {
    if cfg!(windows) {
        "artifact.exe"
    } else {
        "artifact"
    }
}

fn ensure_copy_parent(path: &Path) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            format!(
                "unable to create native executable cache restore directory {}: {}",
                parent.display(),
                err
            )
        })?;
    }
    Ok(())
}

fn native_backend_named_sidecars(output_path: &Path) -> Vec<(&'static str, PathBuf)> {
    let output_dir = output_path.parent().unwrap_or_else(|| Path::new("."));
    vec![
        (
            "runtime_contract.json",
            llvm_native_stage::runtime_contract_artifact_path(output_path),
        ),
        (
            "realtime_app.json",
            llvm_native_stage::realtime_app_artifact_path(output_path),
        ),
        (
            "shader_bundle.json",
            llvm_native_stage::shader_bundle_artifact_path(output_path),
        ),
        (
            "compute_residency.json",
            output_dir.join(kain_driver::COMPUTE_RESIDENCY_FILE_NAME),
        ),
    ]
}

fn store_native_executable_cache_sidecars(
    cache_dir: &Path,
    output_path: &Path,
) -> Result<(), String> {
    let sidecar_dir = cache_dir.join("sidecars");
    fs::create_dir_all(&sidecar_dir).map_err(|err| {
        format!(
            "unable to create native executable cache sidecar directory {}: {}",
            sidecar_dir.display(),
            err
        )
    })?;

    for (cache_name, source_path) in native_backend_named_sidecars(output_path) {
        if !source_path.exists() {
            continue;
        }
        let cache_path = sidecar_dir.join(cache_name);
        fs::copy(&source_path, &cache_path).map_err(|err| {
            format!(
                "unable to store native executable sidecar cache {} -> {}: {}",
                source_path.display(),
                cache_path.display(),
                err
            )
        })?;
    }
    Ok(())
}

fn restore_native_executable_cache_sidecars(
    cache_dir: &Path,
    output_path: &Path,
) -> Result<Vec<PathBuf>, String> {
    let sidecar_dir = cache_dir.join("sidecars");
    if !sidecar_dir.is_dir() {
        return Ok(Vec::new());
    }

    let mut restored = Vec::new();
    for (cache_name, output_sidecar_path) in native_backend_named_sidecars(output_path) {
        let cache_path = sidecar_dir.join(cache_name);
        if !cache_path.exists() {
            continue;
        }
        ensure_copy_parent(&output_sidecar_path)?;
        fs::copy(&cache_path, &output_sidecar_path).map_err(|err| {
            format!(
                "unable to restore native executable sidecar cache {} -> {}: {}",
                cache_path.display(),
                output_sidecar_path.display(),
                err
            )
        })?;
        restored.push(output_sidecar_path);
    }
    Ok(restored)
}

fn llvm_native_runtime_elision_decision(
    target: CompileTarget,
    output_path: &Path,
    cffi_link_inputs: &CffiNativeLinkInputs,
    native_artifacts_require_gpu_runtime: bool,
) -> Result<NativeRuntimeElisionDecision, String> {
    // DEPRECATED(2026-06-08): The linker's --gc-sections flag now dead-strips
    // unreferenced runtime functions. This entire function (IR text scanner +
    // reachability analysis) is redundant and will be removed once the linker
    // path is confirmed stable across all platforms.
    //
    // Set KAIN_NATIVE_RUNTIME_ELISION=bypass to skip IR scanning and always
    // link the full runtime (default after stabilization). With --gc-sections
    // on Linux and /OPT:REF on Windows, unused runtime functions are
    // dead-stripped regardless, so there is no size penalty.
    //
    // See native_toolchain_tuning::apply_link_gc_flags() for the linker flags
    // and crates/build/src/native_link.rs for the runtime build pipeline.
    if std::env::var("KAIN_NATIVE_RUNTIME_ELISION").as_deref() == Ok("bypass") {
        return Ok(NativeRuntimeElisionDecision::keep(
            "elision bypassed (--gc-sections handles dead-stripping)".to_string()
        ));
    }
    if target != CompileTarget::Llvm {
        return Ok(NativeRuntimeElisionDecision::keep(
            "native runtime elision is only available for LLVM artifacts",
        ));
    }
    if native_artifacts_require_gpu_runtime {
        return Ok(NativeRuntimeElisionDecision::keep(
            "GPU artifacts require runtime DLL staging",
        ));
    }
    if !cffi_link_inputs.link_inputs.is_empty() || !cffi_link_inputs.link_libs.is_empty() {
        return Ok(NativeRuntimeElisionDecision::keep(
            "C FFI link inputs require the native link lane",
        ));
    }
    if !native_runtime_elision_enabled()? {
        return Ok(NativeRuntimeElisionDecision::keep(
            "KAIN_NATIVE_RUNTIME_ELISION disabled",
        ));
    }

    let ir = fs::read_to_string(output_path).map_err(|err| {
        format!(
            "failed to read LLVM artifact {} for runtime elision: {}",
            output_path.display(),
            err
        )
    })?;
    Ok(llvm_native_runtime_elision_decision_from_ir(&ir))
}

// DEPRECATED(2026-06-08): --gc-sections linker flag now dead-strips unreferenced
// runtime functions. This IR text scanner is redundant and will be removed once
// the linker path is confirmed stable across all platforms.
fn llvm_native_runtime_elision_decision_from_ir(ir: &str) -> NativeRuntimeElisionDecision {
    let reachability = kain_driver::analyze_llvm_ir_reachability(ir, &["main"]);
    if !reachability.defined_functions.contains("main") {
        return NativeRuntimeElisionDecision::keep("LLVM artifact has no defined @main");
    }
    if reachability.reachable_external_targets.is_empty() {
        NativeRuntimeElisionDecision::elide(reachability.reachable_functions.len())
    } else {
        NativeRuntimeElisionDecision::keep_for_external_targets(
            reachability.reachable_functions.len(),
            reachability.reachable_external_targets,
        )
    }
}

fn slice_llvm_native_executable_ir(
    ir: &str,
) -> Result<Option<(String, kain_driver::LlvmIrSliceStats)>, String> {
    if !native_llvm_ir_slicing_enabled()? {
        return Ok(None);
    }
    Ok(kain_driver::slice_llvm_native_executable_ir(ir))
}

fn parse_native_toolchain_profile(value: &str) -> Result<NativeToolchainProfile, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "debug" | "dev" => Ok(NativeToolchainProfile::Debug),
        "release" | "rel" => Ok(NativeToolchainProfile::Release),
        "benchmark-release" | "benchmark" | "bench" => Ok(NativeToolchainProfile::BenchmarkRelease),
        other => Err(format!(
            "KAIN_NATIVE_PROFILE `{other}` is invalid; expected debug, release, or benchmark-release"
        )),
    }
}

fn parse_native_opt_level(value: &str) -> Result<String, String> {
    let normalized = value.trim().to_ascii_lowercase();
    match normalized.as_str() {
        "0" | "1" | "2" | "3" | "s" | "z" => Ok(normalized),
        _ => Err(format!(
            "KAIN_NATIVE_OPT_LEVEL `{}` is invalid; expected 0, 1, 2, 3, s, or z",
            value.trim()
        )),
    }
}

fn parse_optional_env_string(value: &str) -> Option<String> {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        None
    } else {
        Some(trimmed.to_string())
    }
}

fn parse_native_toolchain_tuning(
    profile: Option<&str>,
    opt_level: Option<&str>,
    target_cpu: Option<&str>,
    debug_info: Option<&str>,
) -> Result<NativeToolchainTuning, String> {
    let selected_profile = match profile {
        Some(raw) => parse_native_toolchain_profile(raw)?,
        None => NativeToolchainProfile::Debug,
    };
    let mut tuning = selected_profile.defaults();
    if let Some(raw_opt_level) = opt_level {
        tuning.opt_level = parse_native_opt_level(raw_opt_level)?;
    }
    if let Some(raw_target_cpu) = target_cpu {
        tuning.target_cpu = parse_optional_env_string(raw_target_cpu);
    }
    if let Some(raw_debug_info) = debug_info {
        tuning.debug_info = parse_bool_env_value("KAIN_NATIVE_DEBUG_INFO", raw_debug_info)?;
    }
    Ok(tuning)
}

fn resolve_native_toolchain_tuning() -> Result<NativeToolchainTuning, String> {
    let tooling_config = kain_core::tooling_config::active_kain_tooling_config();
    let config_debug_info = tooling_config
        .build
        .native_debug_info
        .map(|value| if value { "true" } else { "false" }.to_string());
    parse_native_toolchain_tuning(
        std::env::var("KAIN_NATIVE_PROFILE")
            .ok()
            .as_deref()
            .or(tooling_config.build.native_profile.as_deref()),
        std::env::var("KAIN_NATIVE_OPT_LEVEL")
            .ok()
            .as_deref()
            .or(tooling_config.build.native_opt_level.as_deref()),
        std::env::var("KAIN_NATIVE_TARGET_CPU")
            .ok()
            .as_deref()
            .or(tooling_config.build.native_target_cpu.as_deref()),
        std::env::var("KAIN_NATIVE_DEBUG_INFO")
            .ok()
            .as_deref()
            .or(config_debug_info.as_deref()),
    )
}

fn should_suppress_cli_banner(args: &Args) -> bool {
    if matches!(&args.command, Some(Commands::Format { .. })) {
        return true;
    }

    if env_var_truthy("KAIN_NO_BANNER") || env_var_truthy("KAIN_ENGINE_NO_BANNER") {
        return true;
    }

    !io::stdout().is_terminal()
}

fn parse_artifact_mode(value: &str) -> Result<ArtifactMode, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "live" => Ok(ArtifactMode::Live),
        "generate" | "gen" => Ok(ArtifactMode::Generate),
        "both" => Ok(ArtifactMode::Both),
        other => Err(format!(
            "Unknown crate FFI mode '{other}'. Use one of: live, generate, both"
        )),
    }
}

fn run_check_command(
    input: &Path,
    target: &str,
    fail_fast: bool,
    json_stdout: bool,
    json_out: Option<&Path>,
    pedantic: bool,
    audit: bool,
) -> bool {
    let Some(target) = parse_compile_target(target) else {
        if json_stdout {
            emit_json_error(format!(
                "Unknown check target. Use: {}",
                supported_targets_csv()
            ));
        } else {
            eprintln!("{} Unknown check target. Use: {}", p().status_error(""), supported_targets_csv());
        }
        return false;
    };
    let mut options = kain_check::CheckOptions::new(target);
    options.fail_fast = fail_fast;
    options.pedantic = pedantic;
    options.progress = cli::progress::stderr_progress_sink(!(json_stdout || json_out.is_some()));

    let report = if input == Path::new("-") {
        match read_source_from_path(input) {
            Ok(source) => {
                let file_report = kain_check::check_source("<stdin>", &source, &options);
                let passed = if file_report.passed() { 1 } else { 0 };
                kain_check::CheckReport {
                    target: kain_check::compile_target_name(target).to_string(),
                    total: 1,
                    passed,
                    failed: 1usize.saturating_sub(passed),
                    files: vec![file_report],
                }
            }
            Err(error) => kain_check::CheckReport {
                target: kain_check::compile_target_name(target).to_string(),
                total: 1,
                passed: 0,
                failed: 1,
                files: vec![kain_check::CheckFileReport {
                    path: "<stdin>".to_string(),
                    target: kain_check::compile_target_name(target).to_string(),
                    status: kain_check::CheckStatus::Failed,
                    item_count: 0,
                    test_count: 0,
                    required_capabilities: Vec::new(),
                    error: Some(error),
                    diagnostic: None,
                    diagnostics_json: None,
                    confidence_score: None,
                    validator_count: None,
                    validators_skipped: None,
                    gap_summary: None,
                    missing_categories: None,
                    pedantic: None,
                }],
            },
        }
    } else {
        match resolve_check_input(input) {
            Ok(path) => kain_check::check_path(&path, &options),
            Err(error) => kain_check::CheckReport {
                target: kain_check::compile_target_name(target).to_string(),
                total: 1,
                passed: 0,
                failed: 1,
                files: vec![kain_check::CheckFileReport {
                    path: input.display().to_string(),
                    target: kain_check::compile_target_name(target).to_string(),
                    status: kain_check::CheckStatus::Failed,
                    item_count: 0,
                    test_count: 0,
                    required_capabilities: Vec::new(),
                    error: Some(error),
                    diagnostic: None,
                    diagnostics_json: None,
                    confidence_score: None,
                    validator_count: None,
                    validators_skipped: None,
                    gap_summary: None,
                    missing_categories: None,
                    pedantic: None,
                }],
            },
        }
    };
    if json_stdout {
        // THETA-3: prefer the full per-file diagnostic JSON envelope when
        // available. `kain-check` populates `diagnostics_json` from
        // `kain_error::diagnostics_to_json_string(...)` for every file that
        // failed through the typed frontend. That gives `--json` consumers
        // (LLMs, CI) the rich payload (severity, kind, code, title, message,
        // file, location, labels, notes, help, phase) instead of a summary.
        let full_diag: Vec<&str> = report
            .files
            .iter()
            .filter_map(|f| f.diagnostics_json.as_deref())
            .collect();
        if !full_diag.is_empty() {
            for json_str in &full_diag {
                println!("{json_str}");
            }
        } else if !emit_structured_report(StructuredReportDestination::Stdout, &report, "check") {
            return false;
        }
        // When --audit is also set, append the audit JSON to the same payload.
        if audit {
            if let Some(audit_report) = run_check_audit(input, target, &report) {
                if !emit_structured_report(
                    StructuredReportDestination::Stdout,
                    &audit_report,
                    "audit",
                ) {
                    return false;
                }
            }
        }
        return report.is_success();
    }

    if let Some(path) = json_out {
        // THETA-3: same preference for file output. Per-file diagnostic JSON
        // envelopes when available, falling back to the summary-level
        // `CheckReport` envelope.
        let full_diag: Vec<&str> = report
            .files
            .iter()
            .filter_map(|f| f.diagnostics_json.as_deref())
            .collect();
        if !full_diag.is_empty() {
            let merged = full_diag.join("\n");
            if let Err(error) = std::fs::write(path, &merged) {
                eprintln!(
                    "{} Failed to write JSON report: {}",
                    p().status_error(""),
                    error
                );
                return false;
            }
            println!("{} Wrote check report: {}", p().status_ok(""), path.display());
        } else if !emit_structured_report(StructuredReportDestination::File(path), &report, "check") {
            return false;
        }
    }

    // --audit: run the build pipeline and compare against check results.
    if audit {
        if let Some(audit_report) = run_check_audit(input, target, &report) {
            match serde_json::to_string_pretty(&audit_report) {
                Ok(pretty) => println!("{pretty}"),
                Err(err) => eprintln!(
                    "{} Failed to serialize audit report: {}",
                    p().status_error(""),
                    err
                ),
            }
        }
    }

    let painter = p();
    // THETA-1: rustc-style inline error display. Errors first, summary last.
    // `file.error` already contains a fully-rendered rustc-style block with
    // ANSI colors, source snippets, line numbers, and carets (built by
    // `session.format_error()` via `active_painter()`). Print it verbatim.
    for file in report.files.iter().filter(|file| !file.passed()) {
        if let Some(error) = &file.error {
            eprint!("{error}");
        }
    }
    if report.is_success() {
        println!(
            "{} Check passed: {}/{} files ok",
            painter.status_ok(""),
            report.passed,
            report.total
        );
    } else {
        let failed_paths: Vec<&str> = report
            .files
            .iter()
            .filter(|f| !f.passed())
            .map(|f| f.path.as_str())
            .collect();
        eprintln!(
            "{} Check FAILED: {} - {}/{} passed",
            painter.status_error(""),
            failed_paths.join(", "),
            report.passed,
            report.total
        );
    }
    // ── Telemetry footer (ETA-B) ─────────────────────────────────
    // For the human-readable output, surface confidence and gap summary so
    // users understand what `kain check` actually validated.
    for file in &report.files {
        if let (Some(confidence), Some(ran), Some(skipped)) =
            (file.confidence_score, file.validator_count, file.validators_skipped)
        {
            let mode = if file.pedantic.unwrap_or(false) {
                "pedantic"
            } else {
                "default"
            };
            println!(
                "  confidence: {:.0}% ({} mode, {} ran, {} skipped)",
                confidence * 100.0,
                mode,
                ran,
                skipped
            );
            if let Some(gap) = &file.gap_summary {
                if !gap.is_empty() {
                    println!("  gap: {gap}");
                }
            }
        }
    }
    // THETA-1 cleanup: the previous "check-failure" capture_rendered_failure
    // block was redundant — individual file errors already capture via the
    // `capture_kain_error_failure` path in the frontend, and the structured
    // JSON for `--json` exports is handled by the diagnostics_json branch
    // in the json_stdout / json_out paths above. The
    // `capture_rendered_failure` function signature is preserved for other
    // call sites; only this redundant check-failure capture is removed.
    report.is_success()
}

/// Run the audit pipeline: invoke the build lane, capture its stderr, and
/// compare against the check report to surface build-only errors that
/// `kain check` did not catch. Returns `None` when audit is skipped (e.g.,
/// the build pipeline could not be invoked from the current context).
fn run_check_audit(
    input: &Path,
    target: CompileTarget,
    check_report: &kain_check::CheckReport,
) -> Option<kain_check::audit::AuditReport> {
    use std::process::Stdio;

    // Resolve the input file or workspace root the same way `kain check` did.
    let resolved = match resolve_check_input(input) {
        Ok(path) => path,
        Err(err) => {
            eprintln!("{} audit: failed to resolve input: {}", p().status_error(""), err);
            return None;
        }
    };

    // Only the LLVM target supports a direct in-process build invocation
    // through the CLI. For other targets, fall back to spawning `kain build`
    // as a subprocess and capturing its stderr.
    let build_output = if matches!(target, CompileTarget::Llvm) {
        match run_inline_build_for_audit(&resolved) {
            Ok(output) => output,
            Err(err) => {
                eprintln!("{} audit: inline build failed: {}", p().status_error(""), err);
                return None;
            }
        }
    } else {
        match spawn_kain_build_subprocess(input, target) {
            Ok(output) => output,
            Err(err) => {
                eprintln!("{} audit: subprocess build failed: {}", p().status_error(""), err);
                return None;
            }
        }
    };

    let check_error_strings: Vec<String> = check_report
        .files
        .iter()
        .filter_map(|file| file.error.clone())
        .collect();

    Some(kain_check::audit::audit_build_vs_check(
        &check_error_strings,
        &build_output,
    ))
}

/// Attempt a direct in-process build to capture build-side errors.
///
/// Phase 1: deliberately conservative. We shell out to `kain build` and
/// aggregate stderr+stdout. This is a faithful (if slow) reflection of
/// what the build pipeline would emit, and keeps the audit logic pure.
fn run_inline_build_for_audit(input: &Path) -> Result<String, String> {
    spawn_kain_build_subprocess(input, CompileTarget::Llvm)
}

fn spawn_kain_build_subprocess(input: &Path, target: CompileTarget) -> Result<String, String> {
    use std::process::Stdio;
    let exe = std::env::current_exe().map_err(|err| format!("current_exe: {err}"))?;
    let target_name = kain_check::compile_target_name(target);
    let child = std::process::Command::new(exe)
        .arg("build")
        .arg(input)
        .arg("--target")
        .arg(target_name)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .output()
        .map_err(|err| format!("spawn build: {err}"))?;

    let mut combined = String::new();
    if !child.stdout.is_empty() {
        combined.push_str(&String::from_utf8_lossy(&child.stdout));
    }
    if !child.stderr.is_empty() {
        combined.push_str(&String::from_utf8_lossy(&child.stderr));
    }
    Ok(combined)
}

fn resolve_check_input(input: &Path) -> Result<PathBuf, String> {
    match amalgamate::maybe_materialize_input(input)? {
        Some(materialized) => {
            if materialized.manifest_path.is_some() {
                Ok(materialized.workspace_root)
            } else if let Some(entry_path) = materialized.entry_path {
                Ok(entry_path)
            } else {
                Err("capsule does not expose an entry file or KAIN.toml anchor".to_string())
            }
        }
        None => Ok(input.to_path_buf()),
    }
}

fn resolve_build_input(input: &Path) -> Result<ResolvedBuildInput, String> {
    match amalgamate::maybe_materialize_input(input)? {
        Some(materialized) => {
            if materialized.manifest_path.is_some() {
                Ok(ResolvedBuildInput::Project(materialized.workspace_root))
            } else if let Some(entry_path) = materialized.entry_path {
                Ok(ResolvedBuildInput::File(entry_path))
            } else {
                Err("capsule does not expose an entry file or KAIN.toml anchor".to_string())
            }
        }
        None => {
            if let Some(project_root) = project_authority_root_for_input(input) {
                Ok(ResolvedBuildInput::Project(project_root))
            } else {
                Ok(ResolvedBuildInput::File(input.to_path_buf()))
            }
        }
    }
}

fn project_authority_root_for_input(input: &Path) -> Option<PathBuf> {
    if input.is_dir() {
        return Some(input.to_path_buf());
    }

    let name = input.file_name()?.to_str()?.to_ascii_lowercase();
    if matches!(name.as_str(), "build.kn" | "platform.kn" | "kain.toml") {
        return input.parent().map(Path::to_path_buf);
    }

    None
}

fn project_build_uses_build_authority(
    project_root: &Path,
    target_overrides: Option<&Vec<String>>,
) -> bool {
    target_overrides.is_none() && blade::find_build_script_in(project_root).is_some()
}

fn run_project_build_command(
    project_root: PathBuf,
    target_overrides: Option<Vec<String>>,
    lane: Option<kain_build::BuildLane>,
    clean: bool,
    emit: kain_build::NativeEmit,
) -> Result<(), String> {
    if project_build_uses_build_authority(&project_root, target_overrides.as_ref()) {
        let mut options = kain_build::BladeBuildOptions::new(project_root);
        options.lane = lane;
        options.clean = clean;
        options.progress = cli::progress::stderr_progress_sink(true);
        return kain_build::build_blade_workspace(&options)
            .map(|report| print_kain_build_report(&report))
            .map_err(|error| error.to_string());
    }

    let mut options = kain_build::KainProjectBuildOptions::new(project_root);
    options.emit = emit;
    options.target_overrides = target_overrides;
    options.lane = lane;
    options.clean = clean;
    options.progress = cli::progress::stderr_progress_sink(true);
    kain_build::build_kain_project(&options)
        .map(|report| print_kain_build_report(&report))
        .map_err(|error| error.to_string())
}

fn run_test_command(
    input: &Path,
    mode: Option<&str>,
    target: &str,
    fail_fast: bool,
    ignored: bool,
    json_stdout: bool,
    json_out: Option<&Path>,
) -> bool {
    let Some(default_target) = parse_compile_target(target) else {
        if json_stdout {
            emit_json_error(format!(
                "Unknown test target. Use: {}",
                supported_targets_csv()
            ));
        } else {
            eprintln!("{} Unknown test target. Use: {}", p().status_error(""), supported_targets_csv());
        }
        return false;
    };
    let mode_override = match mode {
        Some(value) => match kain_test::KainTestMode::parse(value) {
            Some(mode) => Some(mode),
            None => {
                let message = format!(
                    "Unknown test mode '{}'. Use: check-pass, check-fail, run-pass, run-fail, kain-test, prove-pass, prove-sat",
                    value
                );
                if json_stdout {
                    emit_json_error(message);
                } else {
                    eprintln!(" {}", message);
                }
                return false;
            }
        },
        None => None,
    };
    let mut options = kain_test::KainTestOptions::new(default_target);
    options.mode_override = mode_override;
    options.fail_fast = fail_fast;
    options.run_ignored = ignored;

    let report = kain_test::run_path(input, &options);
    if json_stdout {
        if !emit_structured_report(StructuredReportDestination::Stdout, &report, "test") {
            return false;
        }
        return report.is_success();
    }

    if let Some(path) = json_out {
        if !emit_structured_report(StructuredReportDestination::File(path), &report, "test") {
            return false;
        }
    }

    let painter = p();
    let status_word = if report.is_success() {
        painter.status_ok("passed")
    } else {
        painter.status_error("failed")
    };
    println!(
        " Test {}: {}/{} passed; {} skipped",
        status_word, report.passed, report.total, report.skipped
    );
    for case in report.cases.iter().filter(|case| case.skipped()) {
        if let Some(reason) = &case.skip_reason {
            println!("  {} {} [{}]: {}", painter.status_muted("skipped"), case.path, case.mode, reason);
        }
    }
    for case in report.cases.iter().filter(|case| !case.passed()) {
        if case.skipped() {
            continue;
        }
        if let Some(error) = &case.error {
            eprintln!("  {} [{}]: {}", painter.status_error(&case.path), case.mode, error);
        }
    }
    if !report.is_success() {
        let rendered = report
            .cases
            .iter()
            .filter(|case| !case.passed() && !case.skipped())
            .filter_map(|case| {
                case.error
                    .as_ref()
                    .map(|error| format!("  {} [{}]: {}\n", case.path, case.mode, error))
            })
            .collect::<String>();
        capture_rendered_failure(
            "test-failure",
            "test",
            if rendered.is_empty() {
                " Test failed\n".to_string()
            } else {
                rendered
            },
            None,
            Some(target),
            None,
            Some(input),
            None,
            serde_json::json!({
                "summary": {
                    "target": target,
                    "total": report.total,
                    "passed": report.passed,
                    "skipped": report.skipped,
                },
                "cases": &report.cases,
            }),
        );
    }
    report.is_success()
}

enum StructuredReportDestination<'a> {
    Stdout,
    File(&'a Path),
}

fn emit_structured_report<T: Serialize>(
    destination: StructuredReportDestination<'_>,
    value: &T,
    label: &str,
) -> bool {
    let encoded = match serde_json::to_string_pretty(value) {
        Ok(encoded) => encoded,
        Err(error) => {
            eprintln!(" Failed to serialize {} report: {}", label, error);
            return false;
        }
    };

    match destination {
        StructuredReportDestination::Stdout => {
            println!("{encoded}");
            true
        }
        StructuredReportDestination::File(path) => {
            if !ensure_parent_dir(path) {
                return false;
            }
            match fs::write(path, encoded) {
                Ok(()) => {
                    println!(" Wrote {} report: {}", label, path.display());
                    true
                }
                Err(error) => {
                    eprintln!(
                        " Failed to write {} report '{}': {}",
                        label,
                        path.display(),
                        error
                    );
                    false
                }
            }
        }
    }
}

fn emit_json_error(message: String) {
    println!(
        "{}",
        serde_json::to_string_pretty(&serde_json::json!({ "error": message })).unwrap_or_else(
            |error| format!("{{\"error\":\"failed to serialize CLI error: {error}\"}}")
        )
    );
}

fn run_registry_command(command: RegistryCommand) -> Result<(), String> {
    match command {
        RegistryCommand::List { bin, runtime, json } => {
            let registry = command_registry_for_display(bin.as_deref(), runtime)?;
            if json {
                print_command_registry_json(&registry)
            } else {
                print_command_registry_text(&registry);
                Ok(())
            }
        }
        RegistryCommand::Export { bin, runtime } => {
            let registry = command_registry_for_display(bin.as_deref(), runtime)?;
            print_command_registry_json(&registry)
        }
        RegistryCommand::Packs { json } => {
            let registry = command_registry_for_display(None, false)?;
            if json {
                print_command_packs_json(&registry)
            } else {
                print_command_packs_text(&registry);
                Ok(())
            }
        }
        RegistryCommand::Help { bin, runtime } => {
            let registry = command_registry_for_display(None, runtime)?;
            let leaked_bin = Box::leak(bin.clone().into_boxed_str());
            let help = kain_commands::dynamic_clap::dynamic_help_for_bin_with_ui(
                &registry,
                cli::cli_boot::active_command_ui_preferences(leaked_bin),
            )?;
            print!("{help}");
            Ok(())
        }
    }
}

fn command_registry_for_display(
    bin: Option<&str>,
    include_runtime: bool,
) -> Result<kain_commands::registry::CommandRegistry, String> {
    let registry = if include_runtime {
        let sources = runtime_command_sources_from_workspace(Path::new("."));
        kain_commands::runtime::combined_registry(&sources).map_err(|err| err.to_string())?
    } else {
        kain_commands::registry::builtin_registry()
    };
    Ok(match bin {
        Some(bin) => registry.for_bin(bin),
        None => registry,
    })
}

fn print_command_registry_json(
    registry: &kain_commands::registry::CommandRegistry,
) -> Result<(), String> {
    let text = serde_json::to_string_pretty(registry)
        .map_err(|err| format!("failed to serialize command registry: {err}"))?;
    println!("{text}");
    Ok(())
}

fn print_command_packs_json(
    registry: &kain_commands::registry::CommandRegistry,
) -> Result<(), String> {
    let text = serde_json::to_string_pretty(&registry.packs)
        .map_err(|err| format!("failed to serialize command packs: {err}"))?;
    println!("{text}");
    Ok(())
}

fn print_command_packs_text(registry: &kain_commands::registry::CommandRegistry) {
    for pack in &registry.packs {
        let owner = pack.owner.as_deref().unwrap_or("unknown");
        let about = pack.about.as_deref().unwrap_or("");
        println!(
            "{}  owner={}  title={}  {}",
            pack.id, owner, pack.title, about
        );
    }
}

fn print_command_registry_text(registry: &kain_commands::registry::CommandRegistry) {
    for command in &registry.commands {
        if command.hidden {
            continue;
        }
        let bins = command.bins.join(",");
        let path = command.path.join(" ");
        let about = command.about.as_deref().unwrap_or("");
        let tags = if command.tags.is_empty() {
            String::new()
        } else {
            format!(" tags={}", command.tags.join(","))
        };
        println!(
            "{}  [{}]  pack={}  handler={}  source={}{}  {}",
            path, bins, command.pack_id, command.handler, command.source.kind, tags, about
        );
    }
}

fn runtime_command_sources_from_workspace(
    start: &Path,
) -> Vec<kain_commands::runtime::RuntimeCommandSource> {
    let Ok(workspace) = blade::discover_workspace(start) else {
        return Vec::new();
    };
    let mut sources = Vec::new();
    if let Some(path) = workspace.manifest_path.clone() {
        sources.push(kain_commands::runtime::RuntimeCommandSource {
            label: format!("workspace:{}", workspace.root.display()),
            manifest_path: path,
        });
    }
    for resolved_blade in workspace.blades {
        if let Some(path) = resolved_blade.manifest_path {
            sources.push(kain_commands::runtime::RuntimeCommandSource {
                label: format!("blade:{}", resolved_blade.name),
                manifest_path: path,
            });
        }
    }
    sources
}

fn run_runtime_command_fallback(launcher: LauncherKind, argv: &[String]) -> Result<(), String> {
    if argv.is_empty() {
        return Err("missing runtime command path".to_string());
    }
    let bin = launcher.display_name();
    let sources = runtime_command_sources_from_workspace(Path::new("."));
    let Some(command_match) = kain_commands::runtime::resolve_runtime_command(bin, argv, &sources)
        .map_err(|err| err.to_string())?
    else {
        return Err(format!("unknown command: {}", argv.join(" ")));
    };
    Err(format!(
        "matched runtime command '{}' from {} at '{}', but handler '{}' is not executable by this host yet (remaining args: {}).",
        command_match.command.id,
        command_match.command.source.label,
        command_match.matched_path.join(" "),
        command_match.command.handler,
        if command_match.remaining_args.is_empty() {
            "<none>".to_string()
        } else {
            command_match.remaining_args.join(" ")
        }
    ))
}

fn external_command_argv(
    external_command_name: Option<&str>,
    external_args: Vec<String>,
) -> Vec<String> {
    let Some(name) = external_command_name else {
        return external_args;
    };
    let mut argv = Vec::with_capacity(1 + external_args.len());
    argv.push(name.to_string());
    argv.extend(external_args);
    argv
}

fn removed_legacy_command_error(argv: &[String]) -> Option<String> {
    let command = argv.first()?;
    match command.as_str() {
        "blades" | "equip" => Some(
            "legacy blade commands were removed from the public CLI. Use `kain build`, `kain run`, `kain check`, `kain test`, `kain clean`, or `kain amalgamate` from the project root instead.".to_string(),
        ),
        _ => None,
    }
}

#[cfg(test)]
mod build_command_tests {
    use super::{
        project_authority_root_for_input, project_build_uses_build_authority,
        removed_legacy_command_error, resolve_build_input, ResolvedBuildInput,
    };
    use std::fs;

    #[test]
    fn build_input_treats_project_roots_and_build_scripts_as_projects() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let root = temp_dir.path().join("demo");
        fs::create_dir_all(root.join("src")).expect("create src");
        fs::write(
            root.join("build.kn"),
            "fn build(ctx: BuildContext) -> BuildGraph:\n    return build_graph()\n",
        )
        .expect("write build.kn");
        fs::write(root.join("KAIN.toml"), "[package]\nname = \"demo\"\n").expect("write manifest");

        assert_eq!(project_authority_root_for_input(&root), Some(root.clone()));
        assert_eq!(
            project_authority_root_for_input(&root.join("build.kn")),
            Some(root.clone())
        );
        assert_eq!(
            project_authority_root_for_input(&root.join("KAIN.toml")),
            Some(root.clone())
        );

        match resolve_build_input(&root).expect("resolve project root") {
            ResolvedBuildInput::Project(path) => assert_eq!(path, root),
            other => panic!("expected project input, got {other:?}"),
        }
    }

    #[test]
    fn build_authority_pipeline_only_triggers_without_target_overrides() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let root = temp_dir.path().join("demo");
        fs::create_dir_all(&root).expect("create root");
        fs::write(
            root.join("build.kn"),
            "fn build(ctx: BuildContext) -> BuildGraph:\n    return build_graph()\n",
        )
        .expect("write build.kn");

        assert!(project_build_uses_build_authority(&root, None));
        assert!(!project_build_uses_build_authority(
            &root,
            Some(&vec!["llvm".to_string()])
        ));
    }

    #[test]
    fn removed_blade_commands_return_a_migration_hint() {
        assert!(
            removed_legacy_command_error(&["blades".to_string(), "build".to_string()]).is_some()
        );
        assert!(removed_legacy_command_error(&["equip".to_string(), "demo".to_string()]).is_some());
        assert!(
            removed_legacy_command_error(&["run".to_string(), "demo.kn".to_string()]).is_none()
        );
    }
}

fn watch_mode(
    input: PathBuf,
    target: CompileTarget,
    output: Option<PathBuf>,
    emit_ast: bool,
    emit_typed: bool,
    verbose: bool,
    analyze: bool,
    plugin_name: Option<String>,
) {
    use std::sync::mpsc::channel;

    let session = kain_driver::DriverSession::new();
    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();

    ctrlc::set_handler(move || {
        println!("\n{} Stopping watch mode...", p().status_info(""));
        r.store(false, Ordering::SeqCst);
    })
    .expect("Error setting Ctrl-C handler");

    println!(
        "{} Watching {} for changes... (Ctrl+C to stop)",
        p().status_info(""),
        input.display()
    );
    println!("");

    // Initial compile
    run_compile_with_session(
        &session,
        &input,
        target,
        output.as_ref(),
        emit_ast,
        emit_typed,
        verbose,
        analyze,
        plugin_name.as_deref(),
    );
    println!("");

    let (tx, rx) = channel();
    let mut _watcher =
        build_watch_mode_watcher(tx.clone(), &watch_inputs_for_session(&session, &input))
            .expect("Failed to create watcher");

    while running.load(Ordering::SeqCst) {
        match rx.recv_timeout(Duration::from_millis(100)) {
            Ok(Ok(_event)) => {
                // Debounce - wait a bit for writes to settle
                std::thread::sleep(Duration::from_millis(50));
                // Drain any pending events
                while rx.try_recv().is_ok() {}

                println!("{} File changed, recompiling...", p().status_info(""));
                println!("");
                run_compile_with_session(
                    &session,
                    &input,
                    target,
                    output.as_ref(),
                    emit_ast,
                    emit_typed,
                    verbose,
                    analyze,
                    plugin_name.as_deref(),
                );
                _watcher = build_watch_mode_watcher(
                    tx.clone(),
                    &watch_inputs_for_session(&session, &input),
                )
                .expect("Failed to refresh watcher");
                println!("");
            }
            Ok(Err(err)) => {
                eprintln!(" Watcher error: {}", err);
            }
            Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                // Keep looping
            }
            Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                break;
            }
        }
    }
}

pub fn main_entry() {
    let builder = std::thread::Builder::new()
        .name("main-thread".into())
        .stack_size(8 * 1024 * 1024); // 8MB

    let handler = builder
        .spawn(|| {
            // Ensure service bridge builtins are registered before any frontend work.
            kain_service_bridge::register();
            let raw_argv = std::env::args().collect::<Vec<_>>();
            if let Some(message) = removed_legacy_command_error(raw_argv.get(1..).unwrap_or(&[])) {
                eprintln!(" {message}");
                std::process::exit(2);
            }
            let launcher = detect_launcher_from_path(std::env::current_exe().ok().as_deref());
            let (args, _tooling_config, external_command_name) =
                cli::cli_boot::parse_kain_cli(launcher).unwrap_or_else(|error| {
                    eprintln!(" Kain config failed: {error}");
                    std::process::exit(1);
                });
            let suppress_banner = should_suppress_cli_banner(&args);
            let mut stdin_source = None;
            if args.command.is_none()
                && args.input.is_none()
                && args.code.is_none()
                && launcher.prefers_interpret_default()
                && !io::stdin().is_terminal()
            {
                let mut buffer = String::new();
                if io::stdin().read_to_string(&mut buffer).is_ok() && !buffer.trim().is_empty() {
                    stdin_source = Some(normalize_script_source(buffer));
                }
            }

            if !suppress_banner {
                println!(
                    " {} Compiler v{} (build {})",
                    LANGUAGE_NAME, VERSION, BUILD_NUMBER
                );
            }

            if launcher.prefers_interpret_default()
                && args.command.is_none()
                && args.input.is_none()
                && args.code.is_none()
                && io::stdin().is_terminal()
            {
                if !run_kn_repl() {
                    std::process::exit(1);
                }
                return;
            }

            if should_show_launcher_menu(
                launcher,
                args.command.is_some(),
                args.input.is_some() || args.code.is_some() || stdin_source.is_some(),
            ) {
                if let Some(menu) = render_launcher_menu(launcher) {
                    print!("{menu}");
                }
                return;
            }

            match args.command {
                Some(Commands::Init { path, name }) => {
                    if let Err(e) = packager::init_project(&path, name) {
                        eprintln!(" Init failed: {}", e);
                    }
                }
                Some(Commands::Add {
                    package,
                    version,
                    manifest,
                }) => match packages::add(&package, version, manifest.as_deref()) {
                    Ok(report) => {
                        println!(
                            " Added {} v{}",
                            report.package_name, report.version
                        );
                        println!("  manifest: {}", report.manifest_path.display());
                        println!("  lockfile: {}", report.lockfile_path.display());
                    }
                    Err(err) => {
                        eprintln!(" Add failed: {}", err);
                        std::process::exit(1);
                    }
                },
                Some(Commands::Install { package, version }) => {
                    // Special: "c-extras" triggers vcpkg bootstrap
                    if package == "c-extras" {
                        match kain_c_ffi::vcpkg_setup::setup_vcpkg(
                            &kain_c_ffi::vcpkg_setup::default_vcpkg_root(),
                        ) {
                            Ok(()) => {}
                            Err(e) => {
                                eprintln!(" vcpkg setup failed: {e}");
                                std::process::exit(1);
                            }
                        }
                        return;
                    }
                    match packages::install(&package, version) {
                        Ok(report) => {
                            println!(" Installed {} v{}", report.name, report.version);
                            println!("  package root: {}", report.package_root.display());
                            println!("  workspace: {}", report.workspace_root.display());
                            println!("  source capsule: {}", report.source_capsule.display());
                        }
                        Err(err) => {
                            eprintln!(" Install failed: {}", err);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::Publish {
                    input,
                    output,
                    name,
                    version,
                    artifacts,
                    evidence,
                    archive,
                }) => match packages::publish(
                    &input,
                    output.as_deref(),
                    name,
                    version,
                    artifacts,
                    evidence,
                    archive,
                ) {
                    Ok(report) => {
                        println!(" Published {} v{}", report.name, report.version);
                        println!("  source capsule: {}", report.source_capsule.display());
                        if let Some(path) = report.artifact_capsule {
                            println!("  artifacts capsule: {}", path.display());
                        }
                        if let Some(path) = report.evidence_capsule {
                            println!("  evidence capsule: {}", path.display());
                        }
                    }
                    Err(err) => {
                        eprintln!(" Publish failed: {}", err);
                        std::process::exit(1);
                    }
                },
                Some(Commands::Lsp) => {
                    eprintln!(" Starting KAIN Language Server...");
                    // Manual runtime for LSP
                    let rt = tokio::runtime::Builder::new_multi_thread()
                        .enable_all()
                        .build()
                        .expect("Failed to create tokio runtime");

                    rt.block_on(async {
                        lsp::run_server().await;
                    });
                }
                Some(Commands::Doctor {
                    repair: repair_args,
                }) => {
                    if let Some(mode) = repair::selected_mode(&repair_args) {
                        let profile_label = repair::selected_profile_label(&repair_args);
                        match repair::target_kind(&repair_args) {
                            Some(repair::DoctorRepairTargetKind::File) => {
                                let Some(path) = repair_args.repair.as_ref() else {
                                    eprintln!(" Doctor repair requested without a file path.");
                                    std::process::exit(1);
                                };
                                match repair::run(path, repair_args.profile, mode) {
                                    Ok(report) => {
                                        print_repair_report(path, &report, mode, profile_label);
                                    }
                                    Err(err) => {
                                        eprintln!(" Doctor repair failed: {}", err);
                                        std::process::exit(1);
                                    }
                                }
                            }
                            Some(repair::DoctorRepairTargetKind::Tree) => {
                                let Some(root) = repair_args.repair_tree.as_ref() else {
                                    eprintln!(
                                        " Doctor repair tree requested without a directory path."
                                    );
                                    std::process::exit(1);
                                };
                                match repair::run_tree(root, repair_args.profile, mode) {
                                    Ok(report) => {
                                        print_repair_tree_report(
                                            root,
                                            &report,
                                            mode,
                                            profile_label,
                                        );
                                    }
                                    Err(err) => {
                                        eprintln!(" Doctor repair tree failed: {}", err);
                                        std::process::exit(1);
                                    }
                                }
                            }
                            None => {
                                eprintln!(" Doctor repair requested without a target path.");
                                std::process::exit(1);
                            }
                        }
                        return;
                    }
                    print_doctor(launcher);
                }
                Some(Commands::Config { command }) => match command {
                    kain_commands::kain::ConfigCommand::Show { json } => {
                        if let Err(err) = run_config_show(json) {
                            eprintln!(" Config show failed: {}", err);
                            std::process::exit(1);
                        }
                    }
                    kain_commands::kain::ConfigCommand::Set { key, value } => {
                        if let Err(err) = run_config_set(&key, &value) {
                            eprintln!(" Config set failed: {}", err);
                            std::process::exit(1);
                        }
                    }
                    kain_commands::kain::ConfigCommand::Init { path, force } => {
                        if let Err(err) = run_config_init(path.as_deref(), force) {
                            eprintln!(" Config init failed: {}", err);
                            std::process::exit(1);
                        }
                    }
                },
                Some(Commands::Format {
                    inputs,
                    check,
                    write,
                }) => {
                    if !run_format_command(inputs, check, write) {
                        std::process::exit(1);
                    }
                }
                Some(Commands::StdlibMap {
                    repo_root,
                    stdlib_root,
                    native_manifests,
                    json_out,
                    llm_out,
                    write,
                    check,
                    json,
                }) => {
                    if let Err(err) = run_stdlib_map_command(
                        repo_root,
                        stdlib_root,
                        native_manifests,
                        json_out,
                        llm_out,
                        write,
                        check,
                        json,
                    ) {
                        eprintln!(" Stdlib map failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Check {
                    input,
                    target,
                    fail_fast,
                    json,
                    json_out,
                    pedantic,
                    audit,
                }) => {
                    if !run_check_command(
                        &input,
                        &target,
                        fail_fast,
                        json,
                        json_out.as_deref(),
                        pedantic,
                        audit,
                    ) {
                        std::process::exit(1);
                    }
                }
                Some(Commands::Test {
                    input,
                    mode,
                    target,
                    fail_fast,
                    ignored,
                    json,
                    json_out,
                }) => {
                    if !run_test_command(
                        &input,
                        mode.as_deref(),
                        &target,
                        fail_fast,
                        ignored,
                        json,
                        json_out.as_deref(),
                    ) {
                        std::process::exit(1);
                    }
                }
                Some(Commands::Selfhost { command }) => {
                    if let Err(e) = selfhost::run(command) {
                        eprintln!(" Selfhost failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Omni { command }) => {
                    if let Err(e) = omni::run(command) {
                        eprintln!(" Omni command failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Fabric { command }) => {
                    if let Err(e) = fabric::run(command) {
                        eprintln!(" Fabric command failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::InstallCextras {
                    packages,
                    target,
                    dry_run,
                }) => {
                    if let Err(e) = packages::install_c_extras(&packages, &target, dry_run) {
                        eprintln!(" install-c-extras failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Import { command }) => match command {
                    ImportCommand::Crates {
                        path,
                        source_root,
                        output,
                        blades,
                        target,
                        flat,
                        include_filters,
                        exclude_filters,
                        fail_fast,
                    } => {
                        let workspace_root = match path {
                            Some(path) => path,
                            None => match std::env::current_dir() {
                                Ok(path) => path,
                                Err(err) => {
                                    eprintln!("❌ import crates failed: {}", err);
                                    std::process::exit(1);
                                }
                            },
                        };
                        let batch = import_rust::ImportRustBatchOptions {
                            recursive: true,
                            flat,
                            include_filters,
                            exclude_filters,
                            fail_fast,
                            report_json: None,
                        };
                        let options = import_rust::ImportRustCratesOptions {
                            source_root,
                            output,
                            blades,
                            target,
                            batch,
                        };
                        if let Err(err) =
                            import_rust::import_workspace_crates(&workspace_root, &options)
                        {
                            eprintln!("❌ import crates failed: {}", err);
                            std::process::exit(1);
                        }
                    }
                    ImportCommand::Platform {
                        package,
                        package_name,
                        provider,
                        sdk,
                        output,
                        target_triple,
                        dry_run,
                        report_json,
                        registry,
                        header,
                    } => {
                        if let Err(err) = import_platform::import_platform(
                            &package,
                            import_platform::ImportPlatformCliOptions {
                                package_name,
                                provider,
                                sdk_root: sdk,
                                output_dir: output,
                                target_triple,
                                dry_run,
                                report_json,
                                registry_path: registry,
                                header_path: header,
                            },
                        ) {
                            eprintln!("❌ import platform failed: {}", err);
                            std::process::exit(1);
                        }
                    }
                },
                Some(Commands::Amalgamate {
                    command,
                    input,
                    output,
                    name,
                    version,
                    author,
                    notes,
                    tag,
                    meta,
                    contents,
                    capsule_set,
                    archive,
                    raw,
                    header,
                    preview_symbols,
                    compression,
                    api_index,
                    module_index,
                }) => {
                    if let Err(err) = amalgamate::run(
                        command,
                        input,
                        output,
                        name,
                        version,
                        author,
                        notes,
                        tag,
                        meta,
                        contents,
                        capsule_set,
                        archive,
                        raw,
                        header,
                        preview_symbols,
                        compression,
                        api_index,
                        module_index,
                    ) {
                        eprintln!(" Amalgamate failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Build {
                    command,
                    input,
                    output,
                    target,
                    targets,
                    lane,
                    clean,
                    debug,
                    ue5,
                    r#rust,
                    embed,
                    emit,
                }) => {
                    let lane = match parse_build_lane(lane.as_deref()) {
                        Ok(lane) => lane,
                        Err(err) => {
                            eprintln!(" Build failed: {}", err);
                            std::process::exit(1);
                        }
                    };
                    if let Some(BuildCommand::NativeUi {
                        input,
                        root_component,
                        app_name,
                        window_title,
                        project_dir,
                        artifact_dir,
                        bundle_only,
                        clean: native_ui_clean,
                        release,
                        runtime_crate,
                        runtime_path,
                        runtime_version,
                        host,
                        tauri_bundle_id,
                        tauri_window_label,
                    }) = command
                    {
                        let host = match kain_build::NativeUiBuildHost::parse(&host) {
                            Some(host) => host,
                            None => {
                                eprintln!(" Native UI build failed: unsupported host '{}'", host);
                                std::process::exit(1);
                            }
                        };
                        let runtime_dependency = if let Some(path) = runtime_path {
                            kain_build::NativeUiRuntimeDependency::Path(path)
                        } else if let Some(version) = runtime_version {
                            kain_build::NativeUiRuntimeDependency::Version(version)
                        } else {
                            kain_build::NativeUiRuntimeDependency::WorkspacePath
                        };
                        let mut options = kain_build::KainNativeUiBuildOptions::new(input);
                        options.lane = lane;
                        options.clean = clean || native_ui_clean;
                        options.host = host;
                        options.root_component = root_component;
                        options.window_title = window_title;
                        options.app_name = app_name;
                        options.project_dir = project_dir;
                        options.artifact_output_dir =
                            artifact_dir.unwrap_or_else(|| PathBuf::from("generated"));
                        options.build_executable = !bundle_only;
                        options.release = release;
                        options.runtime_crate_name = runtime_crate;
                        options.runtime_dependency = runtime_dependency;
                        options.tauri_bundle_identifier = tauri_bundle_id;
                        options.tauri_window_label = tauri_window_label;
                        options.progress = cli::progress::stderr_progress_sink(true);

                        match kain_build::build_kain_native_ui(&options) {
                            Ok(report) => print_kain_build_report(&report),
                            Err(e) => {
                                eprintln!(" Native UI build failed: {}", e);
                                std::process::exit(1);
                            }
                        }
                    } else if ue5 {
                        if clean {
                            eprintln!(
                                " Build failed: --clean is not supported with --ue5; run `kain clean` first"
                            );
                            std::process::exit(1);
                        }
                        // UE5 plugin build
                        if let Err(e) = packager::build_ue5_plugin_with_options(embed) {
                            // Error already contains formatted details with file:line:col
                            eprintln!("{}", e);
                            std::process::exit(1);
                        }
                    } else if r#rust {
                        match input {
                            Some(file) => {
                                match resolve_build_input(&file) {
                                    Ok(ResolvedBuildInput::File(file)) => {
                                        let mut options = kain_build::KainRustBuildOptions::new(file);
                                        options.output = output.clone();
                                        options.lane = lane;
                                        options.clean = clean;
                                        options.progress = cli::progress::stderr_progress_sink(true);
                                        match kain_build::build_kain_rust_file(&options) {
                                            Ok(report) => print_kain_build_report(&report),
                                            Err(e) => {
                                                eprintln!(" Rust build failed: {}", e);
                                                std::process::exit(1);
                                            }
                                        }
                                    }
                                    Ok(ResolvedBuildInput::Project(project_root)) => {
                                        if output.is_some() {
                                            eprintln!(
                                                " Rust build failed: -o/--output is not supported when building a project capsule"
                                            );
                                            std::process::exit(1);
                                        }
                                        let mut options =
                                            kain_build::KainProjectBuildOptions::new(project_root);
                                        options.rust_only = true;
                                        options.lane = lane;
                                        options.clean = clean;
                                        options.progress = cli::progress::stderr_progress_sink(true);
                                        if let Err(e) = kain_build::build_kain_project(&options)
                                            .map(|report| print_kain_build_report(&report))
                                        {
                                            eprintln!(" Build failed: {}", e);
                                            std::process::exit(1);
                                        }
                                    }
                                    Err(error) => {
                                        eprintln!(" Rust build failed: {}", error);
                                        std::process::exit(1);
                                    }
                                }
                            }
                            None => {
                                let mut options = kain_build::KainProjectBuildOptions::new(
                                    std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")),
                                );
                                options.rust_only = true;
                                options.lane = lane;
                                options.clean = clean;
                                options.progress = cli::progress::stderr_progress_sink(true);
                                if let Err(e) = kain_build::build_kain_project(&options)
                                    .map(|report| print_kain_build_report(&report))
                                {
                                    eprintln!(" Build failed: {}", e);
                                    std::process::exit(1);
                                }
                            }
                        }
                    } else {
                        match input {
                            Some(file) => {
                                match resolve_build_input(&file) {
                                    Ok(ResolvedBuildInput::File(file)) => {
                                        let target_alias =
                                            target.as_deref().unwrap_or(args.target.as_str());
                                        let Some(resolved_target) =
                                            parse_compile_target(target_alias)
                                        else {
                                            eprintln!(
                                                " Unknown target: {}. Use: {}",
                                                target_alias,
                                                supported_targets_csv()
                                            );
                                            std::process::exit(1);
                                        };
                                        let mut options =
                                            kain_build::KainFileBuildOptions::new(file, resolved_target);
                                        options.emit = resolve_emit_mode(emit.clone());
                                        options.output = output.clone();
                                        options.lane = lane;
                                        options.clean = clean;
                                        options.debug_info = debug;
                                        options.progress = cli::progress::stderr_progress_sink(true);
                                        match kain_build::build_kain_file(&options) {
                                            Ok(report) => print_kain_build_report(&report),
                                            Err(e) => {
                                                eprintln!(" Build failed: {}", e);
                                                std::process::exit(1);
                                            }
                                        }
                                    }
                                    Ok(ResolvedBuildInput::Project(project_root)) => {
                                        if output.is_some() {
                                            eprintln!(
                                                " Build failed: -o/--output is not supported when building a project capsule"
                                            );
                                            std::process::exit(1);
                                        }
                                        let target_overrides = if let Some(single_target) = target {
                                            Some(vec![single_target])
                                        } else {
                                            targets.clone()
                                        };
                                        if let Err(e) = run_project_build_command(
                                            project_root,
                                            target_overrides,
                                            lane,
                                            clean,
                                            resolve_emit_mode(emit.clone()),
                                        ) {
                                            eprintln!(" Build failed: {}", e);
                                            std::process::exit(1);
                                        }
                                    }
                                    Err(error) => {
                                        eprintln!(" Build failed: {}", error);
                                        std::process::exit(1);
                                    }
                                }
                            }
                            None => {
                                // Project build from KAIN.toml
                                let target_overrides = if let Some(single_target) = target {
                                    Some(vec![single_target])
                                } else {
                                    targets.clone()
                                };
                                if let Err(e) = run_project_build_command(
                                    std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")),
                                    target_overrides,
                                    lane,
                                    clean,
                                    resolve_emit_mode(emit.clone()),
                                ) {
                                    eprintln!(" Build failed: {}", e);
                                    std::process::exit(1);
                                }
                            }
                        }
                    }
                }
                Some(Commands::Runtime { command }) => {
                    if let Err(e) = runtime_tools::run(command) {
                        eprintln!(" Runtime command failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::NativeUi { command }) => match command {
                    NativeUiCommand::Dev {
                        input,
                        root_component,
                        app_name,
                        window_title,
                        project_dir,
                        artifact_dir,
                        release,
                        host,
                        tauri_bundle_id,
                        tauri_window_label,
                    } => {
                        let host = match parse_native_ui_host_kind(&host) {
                            Ok(host) => host,
                            Err(err) => {
                                eprintln!(" Native UI dev failed: {}", err);
                                std::process::exit(1);
                            }
                        };
                        let build = native_ui_build::NativeUiBuildConfig {
                            host,
                            tauri: native_ui_build::NativeUiTauriConfig {
                                bundle_identifier: tauri_bundle_id,
                                window_label: tauri_window_label,
                                ..Default::default()
                            },
                            root_component,
                            window_title,
                            app_name,
                            project_dir,
                            artifact_output_dir: artifact_dir
                                .unwrap_or_else(|| PathBuf::from("generated")),
                            build_executable: true,
                            release,
                            runtime_dependency:
                                native_ui_build::NativeUiRuntimeDependencyConfig::WorkspacePath,
                            ..Default::default()
                        };
                        let config = match native_ui_dev::NativeUiDevConfig::new(input, build) {
                            Ok(config) => config,
                            Err(err) => {
                                eprintln!(" Native UI dev failed: {}", err);
                                std::process::exit(1);
                            }
                        };
                        if let Err(err) = native_ui_dev::run_native_ui_dev(config) {
                            eprintln!(" Native UI dev failed: {}", err);
                            std::process::exit(1);
                        }
                    }
                },
                Some(Commands::Bridge { command }) => match command {
                    BridgeCommand::Serve {
                        entry,
                        dispatch_function,
                    } => {
                        if let Err(err) =
                            cli::bridge::run_bridge_server(cli::bridge::BridgeServeConfig {
                                entry,
                                dispatch_function,
                            })
                        {
                            eprintln!(" Kain bridge failed: {}", err);
                            std::process::exit(1);
                        }
                    }
                },
                Some(Commands::Codebase { command }) => {
                    if let Err(err) = codebase::run(command) {
                        eprintln!(" Codebase command failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Clean {
                    path,
                    scope,
                    dry_run,
                    json,
                }) => {
                    if let Err(err) = clean::run_workspace_clean(path, scope, dry_run, json) {
                        eprintln!(" Clean failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Repl) => {
                    if !run_kn_repl() {
                        std::process::exit(1);
                    }
                }
                Some(Commands::Run {
                    command,
                    input,
                    target,
                    debug,
                    json,
                    trace,
                    keep_artifacts,
                    dry_run,
                    args: run_args,
                }) => {
                    let request = match command {
                        Some(RunCommand::Dev {
                            input,
                            target,
                            debug: dev_debug,
                            json,
                            trace,
                            keep_artifacts,
                            dry_run,
                            args,
                        }) => run_cli::make_request(
                            input,
                            kain_run::RunMode::Dev,
                            target,
                            args,
                            debug || dev_debug,
                            json,
                            trace,
                            keep_artifacts,
                            dry_run,
                        ),
                        Some(RunCommand::Plan {
                            input,
                            target,
                            json,
                        }) => run_cli::make_request(
                            input,
                            kain_run::RunMode::Plan,
                            target,
                            Vec::new(),
                            debug,
                            json,
                            false,
                            false,
                            true,
                        ),
                        None => run_cli::make_request(
                            input,
                            kain_run::RunMode::Once,
                            target,
                            run_args,
                            debug,
                            json,
                            trace,
                            keep_artifacts,
                            dry_run,
                        ),
                    };
                    let request = match request {
                        Ok(request) => request,
                        Err(err) => {
                            eprintln!(" Run failed: {}", err);
                            std::process::exit(1);
                        }
                    };
                    if let Err(err) = run_cli::execute(request) {
                        eprintln!(" Run failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Watch {
                    input,
                    target,
                    json,
                    trace,
                    keep_artifacts,
                    dry_run,
                    args: run_args,
                }) => {
                    let request = match run_cli::make_request(
                        input,
                        kain_run::RunMode::Dev,
                        target,
                        run_args,
                        false,
                        json,
                        trace,
                        keep_artifacts,
                        dry_run,
                    ) {
                        Ok(request) => request,
                        Err(err) => {
                            eprintln!(" Watch failed: {}", err);
                            std::process::exit(1);
                        }
                    };
                    if let Err(err) = run_cli::execute(request) {
                        eprintln!(" Watch failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::GpuArtifacts { input, output, target, no_residency, no_derived }) => {
                    let artifact_target = gpu_artifacts::GpuArtifactTarget::from_arg(&target);
                    match gpu_artifacts::run_gpu_artifact_pipeline(&input, output.as_ref(), artifact_target, no_residency, no_derived) {
                        Ok(paths) => {
                            println!(" Generated {} GPU artifact files:", paths.len());
                            for path in paths {
                                println!("   - {}", path.display());
                            }
                        }
                        Err(e) => {
                            eprintln!(" GPU artifact generation failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::Inject {
                    inputs,
                    plugin_dir,
                    plugin,
                    force,
                    dry_run,
                    ue5,
                }) => {
                    if ue5 {
                        if let Err(e) = packager::inject_into_plugin(
                            &inputs,
                            plugin_dir.as_ref(),
                            plugin.as_deref(),
                            force,
                            dry_run,
                        ) {
                            eprintln!(" Injection failed: {}", e);
                            std::process::exit(1);
                        }
                    } else {
                        eprintln!(" Only --ue5 target is supported for inject command");
                        std::process::exit(1);
                    }
                }
                Some(Commands::ImportAsm {
                    input,
                    format,
                    out,
                    validate_only,
                }) => {
                    match import_asm::import_asm(&input, &format, out.as_deref(), validate_only) {
                        Ok(result) => {
                            println!(" Import complete");
                            println!(" Canonical ASM: {}", result.canonical_asm_path.display());
                            if !validate_only {
                                println!(" Generated KAIN: {}", result.generated_kn_path.display());
                                println!(" Mapping JSON: {}", result.map_json_path.display());
                            }
                            println!(" Recovery report: {}", result.report_json_path.display());
                            println!(
                                " Parsed blocks: {}, data tables: {}, directives: {}",
                                result.parsed.blocks.len(),
                                result.parsed.data_tables.len(),
                                result.parsed.directives.len()
                            );
                        }
                        Err(e) => {
                            eprintln!(" import-asm failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::ImportC {
                    input,
                    output,
                    target,
                    include_paths,
                    defines,
                    flat,
                    include_filters,
                    exclude_filters,
                    fail_fast,
                    report_json,
                }) => {
                    let batch = import_c::ImportCBatchOptions {
                        recursive: true,
                        flat,
                        include_filters,
                        exclude_filters,
                        fail_fast,
                        report_json,
                    };

                    match import_c::import_c_with_batch(
                        &input,
                        output.as_deref(),
                        target.as_deref(),
                        &include_paths,
                        &defines,
                        &batch,
                    ) {
                        Ok(_) => {
                            // Success message already printed by import_c
                        }
                        Err(e) => {
                            eprintln!("❌ import-c failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::ImportRust {
                    input,
                    output,
                    target,
                    flat,
                    include_filters,
                    exclude_filters,
                    fail_fast,
                    report_json,
                }) => {
                    let batch = import_rust::ImportRustBatchOptions {
                        recursive: true,
                        flat,
                        include_filters,
                        exclude_filters,
                        fail_fast,
                        report_json,
                    };

                    match import_rust::import_rust_with_batch(
                        &input,
                        output.as_deref(),
                        target.as_deref(),
                        &batch,
                    ) {
                        Ok(_) => {
                            // Success message already printed by import_rust
                        }
                        Err(e) => {
                            eprintln!("❌ import-rust failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::ImportCrate {
                    crate_name,
                    manifest_path,
                    crate_path,
                    mode,
                    output,
                    report_json,
                    features,
                    all_features,
                    no_default_features,
                }) => {
                    let mode = match parse_artifact_mode(&mode) {
                        Ok(value) => value,
                        Err(err) => {
                            eprintln!("❌ import-crate failed: {}", err);
                            std::process::exit(1);
                        }
                    };
                    let options = ImportCrateOptions {
                        manifest_path,
                        crate_path,
                        output_dir: output,
                        report_json,
                        mode,
                        features,
                        all_features,
                        no_default_features,
                    };

                    if let Err(err) = import_crate::import_crate(&crate_name, options) {
                        eprintln!("❌ import-crate failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::ImportTs {
                    input,
                    output,
                    target,
                    flat,
                    include_filters,
                    exclude_filters,
                    fail_fast,
                    report_json,
                }) => {
                    let batch = import_typescript::ImportTypeScriptBatchOptions {
                        recursive: true,
                        flat,
                        include_filters,
                        exclude_filters,
                        fail_fast,
                        strict_generated_output: args.strict,
                        report_json,
                    };

                    match import_typescript::import_typescript_with_batch(
                        &input,
                        output.as_deref(),
                        target.as_deref(),
                        &batch,
                    ) {
                        Ok(_) => {}
                        Err(e) => {
                            eprintln!("❌ import-ts failed: {}", e);
                            std::process::exit(1);
                        }
                    }
                }
                Some(Commands::Commands { command }) => {
                    if let Err(err) = run_registry_command(command) {
                        eprintln!(" Commands registry failed: {}", err);
                        std::process::exit(1);
                    }
                }
                Some(Commands::External(argv)) => {
                    let argv = external_command_argv(external_command_name.as_deref(), argv);
                    if let Some(message) = removed_legacy_command_error(&argv) {
                        eprintln!(" {}", message);
                        std::process::exit(1);
                    }
                    if let Err(err) = run_runtime_command_fallback(launcher, &argv) {
                        eprintln!(" Runtime command failed: {}", err);
                        std::process::exit(1);
                    }
                }
                None => {
                    // Legacy behavior
                    if let Some(ref input) = args.input {
                        if args.target.as_str() == "ue5-shader" {
                            if args.watch {
                                eprintln!(" Watch mode is not supported for ue5-shader target.");
                            }
                            if !run_ue5_shader_pipeline(&input, &args) {
                                std::process::exit(1);
                            }
                        } else if legacy_file_prefers_native_script(
                            launcher,
                            &args.target,
                            args.output.is_some(),
                        ) {
                            if !run_file_as_native_script(input.clone(), args.watch) {
                                std::process::exit(1);
                            }
                        } else {
                            let resolved_target_alias = resolve_legacy_target_alias(
                                launcher,
                                &args.target,
                                args.output.is_some(),
                            );
                            let Some(target) = parse_compile_target(&resolved_target_alias) else {
                                eprintln!(
                                    " Unknown target: {}. Use: {}",
                                    resolved_target_alias,
                                    supported_targets_csv()
                                );
                                std::process::exit(1);
                            };

                            if args.watch {
                                watch_mode(
                                    input.clone(),
                                    target,
                                    args.output.clone(),
                                    args.emit_ast,
                                    args.emit_typed,
                                    args.verbose,
                                    args.analyze,
                                    args.plugin.clone(),
                                );
                            } else {
                                if !run_compile(
                                    &input,
                                    target,
                                    args.output.as_ref(),
                                    args.emit_ast,
                                    args.emit_typed,
                                    args.verbose,
                                    args.analyze,
                                    args.plugin.as_deref(),
                                ) {
                                    std::process::exit(1);
                                }
                            }
                        }
                    } else if let Some(code) = args.code.as_deref() {
                        if legacy_source_prefers_native_script(&args.target, args.output.is_some())
                        {
                            if args.watch {
                                eprintln!(" Watch mode is only supported for file-backed input.");
                                std::process::exit(1);
                            }
                            if !run_inline_native_script("<inline>", code) {
                                std::process::exit(1);
                            }
                            return;
                        }
                        let resolved_target_alias = resolve_legacy_target_alias(
                            launcher,
                            &args.target,
                            args.output.is_some(),
                        );
                        let Some(target) = parse_compile_target(&resolved_target_alias) else {
                            eprintln!(
                                " Unknown target: {}. Use: {}",
                                resolved_target_alias,
                                supported_targets_csv()
                            );
                            std::process::exit(1);
                        };
                        if args.watch {
                            eprintln!(" Watch mode is only supported for file-backed input.");
                            std::process::exit(1);
                        }
                        if !run_source(
                            "<inline>",
                            None,
                            code,
                            target,
                            args.output.as_ref(),
                            args.emit_ast,
                            args.emit_typed,
                            args.verbose,
                            args.analyze,
                            args.plugin.as_deref(),
                        ) {
                            std::process::exit(1);
                        }
                    } else if let Some(stdin_source) = stdin_source.as_deref() {
                        if legacy_source_prefers_native_script(&args.target, args.output.is_some())
                        {
                            if args.watch {
                                eprintln!(" Watch mode is only supported for file-backed input.");
                                std::process::exit(1);
                            }
                            if !run_inline_native_script("<stdin>", stdin_source) {
                                std::process::exit(1);
                            }
                            return;
                        }
                        let resolved_target_alias = resolve_legacy_target_alias(
                            launcher,
                            &args.target,
                            args.output.is_some(),
                        );
                        let Some(target) = parse_compile_target(&resolved_target_alias) else {
                            eprintln!(
                                " Unknown target: {}. Use: {}",
                                resolved_target_alias,
                                supported_targets_csv()
                            );
                            std::process::exit(1);
                        };
                        if args.watch {
                            eprintln!(" Watch mode is only supported for file-backed input.");
                            std::process::exit(1);
                        }
                        if !run_source(
                            "<stdin>",
                            None,
                            stdin_source,
                            target,
                            args.output.as_ref(),
                            args.emit_ast,
                            args.emit_typed,
                            args.verbose,
                            args.analyze,
                            args.plugin.as_deref(),
                        ) {
                            std::process::exit(1);
                        }
                    } else {
                        eprintln!(" No input file provided. Use --help for usage.");
                    }
                }
            }
        })
        .unwrap();

    handler.join().unwrap();
}

fn print_repair_report(
    path: &Path,
    report: &kain_repair::RepairReport,
    mode: kain_repair::RepairMode,
    profile: &str,
) {
    println!("{} Repair Target: {}", p().status_info(""), path.display());
    println!("{} Selected Profile: {}", p().status_info(""), profile);
    println!("{} Repair Mode: {:?}", p().status_info(""), mode);
    println!("{} Safety Class: {:?}", p().status_info(""), report.safety_class);
    println!("{} Unknown Risk: {:?}", p().status_info(""), report.remaining_unknown_risk);
    println!(
        "{} Parser Proof: {}",
        p().status_info(""),
        match report.parser_proof_status() {
            Some(kain_repair::ParserProofStatus::Passed) => "passed",
            Some(kain_repair::ParserProofStatus::Failed) => "failed",
            None => "unverified",
        }
    );

    if report.fixes.is_empty() {
        println!("{} Fixes Applied: none needed", p().status_ok(""));
        println!("{} Remaining Diagnostics: 0", p().status_info(""));
        return;
    }

    println!("{} Fixes Applied: {}", p().status_ok(""), report.fixes_applied);
    println!("{} Fixes:", p().status_info(""));
    for fix in &report.fixes {
        let is_aggressive = is_aggressive_fix(fix);
        let note = fix.note.as_deref().unwrap_or("repair applied");
        println!(
            "   - [{}] {:?}: {}",
            if is_aggressive { "aggressive" } else { "safe" },
            fix.kind,
            note
        );
    }
    println!("{} Remaining Diagnostics: 0", p().status_info(""));
}

fn print_repair_tree_report(
    root: &Path,
    report: &repair::DoctorRepairBatchReport,
    mode: kain_repair::RepairMode,
    profile: &str,
) {
    println!("{} Repair Root: {}", p().status_info(""), root.display());
    println!("{} Selected Profile: {}", p().status_info(""), profile);
    println!("{} Repair Mode: {:?}", p().status_info(""), mode);
    println!(
        "{} Action Class: {}",
        p().status_info(""),
        if matches!(mode, kain_repair::RepairMode::ApplyAggressive) {
            "aggressive"
        } else {
            "safe"
        }
    );
    println!("{} Files Scanned: {}", p().status_info(""), report.scanned);
    println!("{} Files Changed: {}", p().status_info(""), report.changed);
    println!("{} Files Written: {}", p().status_info(""), report.written);
    println!("{} Files Failed: {}", p().status_info(""), report.failed);
    for outcome in &report.outcomes {
        match &outcome.result {
            Ok(file_report) => {
                println!(
                    "   - {} [{}] {}",
                    outcome.path.display(),
                    if file_report.changed() {
                        "changed"
                    } else {
                        "unchanged"
                    },
                    if file_report.fixes.is_empty() {
                        "no fixes"
                    } else {
                        "repairs applied"
                    }
                );
            }
            Err(err) => {
                println!("   - {} [failed] {}", outcome.path.display(), err);
            }
        }
    }
}

fn is_aggressive_fix(fix: &kain_repair::AppliedFix) -> bool {
    matches!(
        fix.kind,
        kain_repair::FixKind::RewriteReservedIdentifier
            | kain_repair::FixKind::NormalizeSelfConstructorSyntax
            | kain_repair::FixKind::RewriteInlineInitialization
            | kain_repair::FixKind::NormalizeNamespacePath
            | kain_repair::FixKind::ReconstructParserSafeBlock
    )
}

#[derive(Debug, Default, Deserialize)]
struct DoctorManagedSyncBinaryStamp {
    #[serde(default)]
    path: Option<String>,
    #[serde(default)]
    size_bytes: Option<u64>,
    #[serde(default)]
    mtime_unix: Option<i64>,
}

#[derive(Debug, Default, Deserialize)]
struct DoctorManagedSyncStamp {
    #[serde(default)]
    repo_sha: Option<String>,
    #[serde(default)]
    runtime_stamp: Option<String>,
    #[serde(default)]
    build_number: Option<String>,
    #[serde(default)]
    synced_at_unix: Option<i64>,
    #[serde(default)]
    binary: Option<DoctorManagedSyncBinaryStamp>,
}

fn default_sync_state_root() -> Option<PathBuf> {
    if let Some(root) = std::env::var_os("KAIN_SYNC_ROOT") {
        let path = PathBuf::from(root);
        if !path.as_os_str().is_empty() {
            return Some(path);
        }
    }

    if cfg!(windows) {
        return std::env::var_os("USERPROFILE").map(|profile| PathBuf::from(profile).join(".kain"));
    }

    std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".kain"))
}

fn resolve_sync_stamp_path() -> Option<PathBuf> {
    if let Some(override_path) = std::env::var_os("KAIN_SYNC_STAMP_PATH") {
        let path = PathBuf::from(override_path);
        if !path.as_os_str().is_empty() {
            return Some(path);
        }
    }
    default_sync_state_root().map(|root| root.join("state").join("kain_sync_stamp.json"))
}

fn load_managed_sync_stamp() -> Option<(PathBuf, DoctorManagedSyncStamp)> {
    let stamp_path = resolve_sync_stamp_path()?;
    let raw = fs::read_to_string(&stamp_path).ok()?;
    let stamp = serde_json::from_str::<DoctorManagedSyncStamp>(&raw).ok()?;
    Some((stamp_path, stamp))
}

fn normalize_path_for_compare(path: &Path) -> String {
    path.to_string_lossy()
        .replace('/', "\\")
        .to_ascii_lowercase()
}

fn paths_match_for_doctor(left: &Path, right: &Path) -> bool {
    normalize_path_for_compare(left) == normalize_path_for_compare(right)
}

fn resolve_repo_head_sha_runtime(repo_root: Option<&Path>) -> Option<String> {
    let mut command = Command::new("git");
    if let Some(root) = repo_root {
        command.arg("-C");
        command.arg(root);
    }
    command.arg("rev-parse");
    command.arg("HEAD");

    let output = command.output().ok()?;
    if !output.status.success() {
        return None;
    }
    let value = String::from_utf8(output.stdout).ok()?;
    let trimmed = value.trim();
    if trimmed.is_empty() {
        return None;
    }
    Some(trimmed.to_string())
}

fn print_doctor(active_launcher: LauncherKind) {
    let current_exe = std::env::current_exe().ok();
    let kain_path_command = which::which("kain").ok();
    let kn_path_command = which::which("kn").ok();
    let kain_path_matches = collect_path_matches("kain");
    let kn_path_matches = collect_path_matches("kn");
    let install_layout = kain_core::install_layout::default_kain_install_layout();
    let stdlib_roots = kain_core::stdlib::find_stdlib_search_roots();
    let runtime_c = find_runtime_c();
    let runtime_manifest = find_native_runtime_manifest();
    let resolved_clang = if cfg!(feature = "sys") {
        kain_core::install_layout::find_clang().map(|p| p.to_string_lossy().to_string())
    } else {
        None
    };
    let resolved_libclang = if cfg!(feature = "sys") {
        kain_core::install_layout::resolve_bundled_libclang_path()
    } else {
        None
    };
    let tooling_config = kain_core::tooling_config::active_kain_tooling_config();
    let resolved_native_tuning = resolve_native_toolchain_tuning().ok();
    let managed_sync_stamp = load_managed_sync_stamp();

    let current_dir = std::env::current_dir().ok();
    let project_root = current_dir.as_ref().and_then(|cwd| find_project_root(cwd));
    let runtime_repo_sha = resolve_repo_head_sha_runtime(project_root.as_deref());

    println!("{} KAIN Doctor", p().banner(""));
    println!("{} Version: {}", p().status_info(""), VERSION);
    println!("{} Build: {}", p().status_info(""), BUILD_NUMBER);
    println!("{} Build Tracking: {}", p().status_info(""), BUILD_TRACKING_MODE);
    println!("{} Built At (UTC): {}", p().status_info(""), format_build_time(BUILD_UNIX_TIME));
    println!(
        "{} Git: {} (commit #{}, {})",
        p().status_info(""),
        BUILD_GIT_SHA, BUILD_GIT_COMMIT_COUNT, BUILD_GIT_DIRTY
    );
    println!("{} Profile: {}", p().status_info(""), BUILD_PROFILE);
    println!(
        "{} Target Triple: {} (host {})",
        p().status_info(""),
        BUILD_TARGET_TRIPLE, BUILD_HOST_TRIPLE
    );

    match &current_exe {
        Some(path) => {
            println!("{} Binary Path: {}", p().status_info(""), path.display());
            println!("{} Binary Kind: {}", p().status_info(""), classify_binary_path(path));
        }
        None => println!("{} Binary Path: <unknown>", p().status_warning("")),
    }
    if let Some(layout) = &install_layout {
        println!("{} Kain Home: {}", p().status_info(""), layout.home_dir.display());
        println!("{} Kain User Bin: {}", p().status_info(""), layout.bin_dir.display());
        println!("{} Kain Config Path: {}", p().status_info(""), layout.config_path.display());
        println!("{} Kain Packages Dir: {}", p().status_info(""), layout.packages_dir.display());
        println!("{} Kain Tooling Dir: {}", p().status_info(""), layout.tooling_dir.display());
        println!("{} Kain Cache Dir: {}", p().status_info(""), layout.cache_dir.display());
        println!("{} Kain Generated Dir: {}", p().status_info(""), layout.generated_dir.display());
        println!(
            "{} Kain Install Manifest: {}",
            p().status_info(""),
            layout.install_manifest_path.display()
        );
    } else {
        println!("{} Kain Home: <unresolved>", p().status_warning(""));
    }
    if let Some(source_path) = &tooling_config.source_path {
        println!("{} Active Config Source: {}", p().status_info(""), source_path.display());
    } else {
        println!("{} Active Config Source: <unresolved>", p().status_warning(""));
    }
    println!(
        "{} Active Config Loaded: {}",
        p().status_info(""),
        if tooling_config.loaded_from_disk {
            "yes"
        } else {
            "no (using defaults)"
        }
    );
    println!(
        "{} CLI Theme: {} / color {} / experimental-help {}",
        p().status_info(""),
        tooling_config.ui.theme,
        describe_color_preference(tooling_config.ui.color),
        tooling_config.ui.experimental_help
    );
    println!(
        "{} Build Parallelism: cargo={} native={} (host cores {})",
        p().status_info(""),
        tooling_config.build.cargo_jobs,
        tooling_config.build.native_jobs,
        tooling_config.build.available_parallelism
    );
    if let Some(tuning) = &resolved_native_tuning {
        println!(
            " Native Toolchain Defaults: {}",
            describe_native_toolchain_tuning(tuning)
        );
    }
    println!(" Active Launcher: {}", active_launcher.display_name());

    match &kain_path_command {
        Some(path) => println!(" PATH kain: {}", path.display()),
        None => println!(" PATH kain: <not found>"),
    }

    match &kn_path_command {
        Some(path) => println!(" PATH kn: {}", path.display()),
        None => println!(" PATH kn: <not found>"),
    }

    if !kain_path_matches.is_empty() {
        println!(" PATH Matches (kain):");
        for path in &kain_path_matches {
            println!("   - {}", path.display());
        }
    }

    if !kn_path_matches.is_empty() {
        println!(" PATH Matches (kn):");
        for path in &kn_path_matches {
            println!("   - {}", path.display());
        }
    }

    match &current_dir {
        Some(cwd) => {
            println!(" Current Dir: {}", cwd.display());
            if let Some(root) = &project_root {
                println!(" Project Root: {}", root.display());
            } else {
                println!(" Project Root: <not found (no KAIN.toml in parent chain)>");
            }
        }
        None => println!(" Current Dir: <unknown>"),
    }

    println!(" Supported Targets: {}", supported_targets_csv());
    println!(
        " TS Target Available: {}",
        if parse_compile_target("ts").is_some() {
            "yes"
        } else {
            "no"
        }
    );
    println!(
        " Features: ue5={}, web={}, gpu={}, sys={}",
        if cfg!(feature = "ue5") { "on" } else { "off" },
        if cfg!(feature = "web") { "on" } else { "off" },
        if cfg!(feature = "gpu") { "on" } else { "off" },
        if cfg!(feature = "sys") { "on" } else { "off" },
    );

    // vcpkg detection (for versioned C includes)
    match kain_c_ffi::vcpkg::find_vcpkg_binary() {
        Ok(path) => println!(" vcpkg Binary: {}", path.display()),
        Err(_) => println!(" vcpkg Binary: <not found> — run `kain install c-extras` to bootstrap"),
    }
    println!(
        " vcpkg Root: {}",
        kain_c_ffi::vcpkg::resolve_vcpkg_root().display()
    );

    println!(
        " PATH Match Status (kain): {}",
        doctor_path_status(current_exe.as_deref(), kain_path_command.as_deref(), "kain")
    );
    println!(
        " PATH Match Status (kn): {}",
        doctor_path_status(current_exe.as_deref(), kn_path_command.as_deref(), "kn")
    );

    match managed_sync_stamp {
        Some((stamp_path, stamp)) => {
            println!(" Managed Sync Stamp: {}", stamp_path.display());
            if let Some(repo_sha) = stamp.repo_sha {
                println!(" Managed Sync Repo SHA: {}", repo_sha);
                if let Some(runtime_sha) = runtime_repo_sha.as_ref() {
                    if runtime_sha == &repo_sha {
                        println!(" Managed Sync Repo Status: current repo head matches stamp");
                    } else {
                        println!(" Managed Sync Repo Status: drift (repo head differs from stamp)");
                    }
                }
            }
            if let Some(runtime_stamp) = stamp.runtime_stamp {
                println!(" Managed Sync Runtime Stamp: {}", runtime_stamp);
            }
            if let Some(build_number) = stamp.build_number {
                if !build_number.trim().is_empty() {
                    println!(" Managed Sync Build: {}", build_number);
                }
            }
            if let Some(synced_at_unix) = stamp.synced_at_unix {
                println!(
                    " Managed Sync At (UTC): {}",
                    format_build_time(&synced_at_unix.to_string())
                );
            }

            if let Some(binary_stamp) = stamp.binary {
                if let Some(sync_binary_path_text) = binary_stamp.path {
                    let sync_binary_path = PathBuf::from(&sync_binary_path_text);
                    println!(" Managed Sync Binary: {}", sync_binary_path.display());
                    if let Some(size_bytes) = binary_stamp.size_bytes {
                        println!(" Managed Sync Binary Size: {} bytes", size_bytes);
                    }
                    if let Some(mtime_unix) = binary_stamp.mtime_unix {
                        println!(
                            " Managed Sync Binary Modified (UTC): {}",
                            format_build_time(&mtime_unix.to_string())
                        );
                    }
                    let binary_match = current_exe
                        .as_deref()
                        .map(|path| paths_match_for_doctor(path, &sync_binary_path))
                        .unwrap_or(false);
                    println!(
                        " Managed Sync Binary Match: {}",
                        if binary_match { "yes" } else { "drift" }
                    );
                    if !binary_match {
                        println!(
                            " Warning: this shell is running a different binary than the managed sync target."
                        );
                    }
                }
            }
        }
        None => println!(" Managed Sync Stamp: <not found>"),
    }

    if stdlib_roots.is_empty() {
        println!(" Resolved Stdlib Roots: <none>");
    } else {
        println!(" Resolved Stdlib Roots:");
        for root in &stdlib_roots {
            println!("   - {}", root.display());
        }
    }

    match runtime_c {
        Some(path) => println!(" Resolved Runtime C: {}", path.display()),
        None => println!(" Resolved Runtime C: <not found>"),
    }

    match runtime_manifest {
        Some(path) => println!(" Resolved Runtime Manifest: {}", path.display()),
        None => println!(" Resolved Runtime Manifest: <not found>"),
    }

    if cfg!(feature = "sys") {
        match resolved_clang {
            Some(path) => println!(" Resolved LLVM Clang: {}", path),
            None => println!(" Resolved LLVM Clang: <not found in bundled locations>"),
        }
        match resolved_libclang {
            Some(path) => println!(" Resolved LLVM libclang: {}", path.display()),
            None => println!(" Resolved LLVM libclang: <not found in bundled locations>"),
        }
    }

    if let Some(path) = current_exe.as_deref() {
        if is_repo_target_binary(path) {
            println!(" Warning: active kain comes from a repo target directory.");
            if cfg!(windows) {
                println!(
                    "          Refresh/install a stable PATH binary with `python install_kain.py`, then open a new shell."
                );
            } else {
                println!(
                    "          Refresh/install a stable PATH binary with `python3 install_kain.py` or source `~/.kain/generated/kain-env.sh`."
                );
            }
        }
    }
}

fn format_build_time(unix_time: &str) -> String {
    let Ok(secs) = unix_time.parse::<i64>() else {
        return format!("unknown (raw: {})", unix_time);
    };

    let Some(dt) = chrono::DateTime::<chrono::Utc>::from_timestamp(secs, 0) else {
        return format!("unknown (raw: {})", unix_time);
    };

    format!("{} (unix {})", dt.to_rfc3339(), unix_time)
}

fn classify_binary_path(path: &Path) -> &'static str {
    if is_bazel_output_binary(path) {
        "bazel-output"
    } else if is_repo_target_binary(path) {
        "repo-target"
    } else if is_kain_home_binary(path) {
        "kain-home-bin"
    } else if is_cargo_bin_binary(path) {
        "cargo-bin"
    } else {
        "custom"
    }
}

fn is_repo_target_binary(path: &Path) -> bool {
    let normalized = path
        .to_string_lossy()
        .replace('/', "\\")
        .to_ascii_lowercase();
    normalized.contains("\\target\\debug\\") || normalized.contains("\\target\\release\\")
}

fn is_bazel_output_binary(path: &Path) -> bool {
    path.to_string_lossy()
        .replace('/', "\\")
        .to_ascii_lowercase()
        .contains("\\bazel-out\\")
}

fn describe_color_preference(
    preference: kain_core::tooling_config::KainColorPreference,
) -> &'static str {
    match preference {
        kain_core::tooling_config::KainColorPreference::Auto => "auto",
        kain_core::tooling_config::KainColorPreference::Always => "always",
        kain_core::tooling_config::KainColorPreference::Never => "never",
    }
}

fn describe_diagnostic_capture_mode(
    mode: kain_core::tooling_config::KainDiagnosticCaptureMode,
) -> &'static str {
    match mode {
        kain_core::tooling_config::KainDiagnosticCaptureMode::Off => "off",
        kain_core::tooling_config::KainDiagnosticCaptureMode::Failures => "failures",
    }
}

fn describe_native_toolchain_tuning(tuning: &NativeToolchainTuning) -> String {
    format!(
        "profile={} opt=O{} target-cpu={} debug-info={}",
        tuning.profile.as_str(),
        tuning.opt_level,
        tuning.target_cpu.as_deref().unwrap_or("default"),
        tuning.debug_info
    )
}

fn toml_string_value(value: &str) -> String {
    value.replace('\\', "\\\\").replace('"', "\\\"")
}

fn render_default_kain_config_toml() -> String {
    let active = kain_core::tooling_config::active_kain_tooling_config();
    format!(
        concat!(
            "# Kain control plane\n",
            "# One install, one config, full send.\n\n",
            "schema = 1\n\n",
            "[build]\n",
            "# jobs presets: smart, all, half, efficiency\n",
            "jobs = \"smart\"\n",
            "# cargo_jobs = {cargo_jobs}\n",
            "# native_jobs = {native_jobs}\n",
            "# native_profile = \"{native_profile}\"\n",
            "# native_opt_level = \"{native_opt_level}\"\n",
            "# native_target_cpu = \"{native_target_cpu}\"\n",
            "# native_debug_info = {native_debug_info}\n\n",
            "[ui]\n",
            "color = \"{color}\"\n",
            "theme = \"{theme}\"\n",
            "experimental_help = {experimental_help}\n\n",
            "[diagnostics]\n",
            "capture = \"{diagnostic_capture}\"\n",
            "path = \"{diagnostic_path}\"\n",
            "store_ansi = {diagnostic_store_ansi}\n"
        ),
        cargo_jobs = active.build.cargo_jobs,
        native_jobs = active.build.native_jobs,
        native_profile = active.build.native_profile.as_deref().unwrap_or("release"),
        native_opt_level = active.build.native_opt_level.as_deref().unwrap_or("2"),
        native_target_cpu = active
            .build
            .native_target_cpu
            .as_deref()
            .unwrap_or("native"),
        native_debug_info = active.build.native_debug_info.unwrap_or(false),
        color = describe_color_preference(active.ui.color),
        theme = active.ui.theme,
        experimental_help = active.ui.experimental_help,
        diagnostic_capture = describe_diagnostic_capture_mode(active.diagnostics.capture),
        diagnostic_path = toml_string_value(&active.diagnostics.path.display().to_string()),
        diagnostic_store_ansi = active.diagnostics.store_ansi,
    )
}

fn supported_config_keys() -> &'static [&'static str] {
    &[
        "build.jobs",
        "build.cargo-jobs",
        "build.native-jobs",
        "build.native-profile",
        "build.native-opt-level",
        "build.native-target-cpu",
        "build.native-debug-info",
        "ui.color",
        "ui.theme",
        "ui.experimental-help",
        "diagnostics.capture",
        "diagnostics.path",
        "diagnostics.store-ansi",
    ]
}

fn normalize_config_key(key: &str) -> String {
    key.trim().replace('_', "-").to_ascii_lowercase()
}

fn load_editable_kain_config(
    path: &Path,
) -> Result<kain_core::tooling_config::KainToolingConfigFile, String> {
    if !path.exists() {
        return Ok(kain_core::tooling_config::KainToolingConfigFile::default());
    }
    let source = fs::read_to_string(path)
        .map_err(|err| format!("failed to read config file {}: {}", path.display(), err))?;
    toml::from_str::<kain_core::tooling_config::KainToolingConfigFile>(&source)
        .map_err(|err| format!("failed to parse config file {}: {}", path.display(), err))
}

fn write_kain_config_file(
    path: &Path,
    config: &kain_core::tooling_config::KainToolingConfigFile,
) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            format!(
                "failed to create config parent directory {}: {}",
                parent.display(),
                err
            )
        })?;
    }
    let encoded = toml::to_string_pretty(config).map_err(|err| {
        format!(
            "failed to serialize config file {}: {}",
            path.display(),
            err
        )
    })?;
    fs::write(path, encoded)
        .map_err(|err| format!("failed to write config file {}: {}", path.display(), err))
}

fn parse_parallelism_setting_value(
    value: &str,
) -> Result<kain_core::tooling_config::KainParallelismSetting, String> {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        return Err("parallelism value cannot be empty".to_string());
    }
    if let Ok(count) = trimmed.parse::<usize>() {
        if count == 0 {
            return Err("parallelism value must be >= 1".to_string());
        }
        return Ok(kain_core::tooling_config::KainParallelismSetting::Count(
            count,
        ));
    }
    Ok(kain_core::tooling_config::KainParallelismSetting::Preset(
        trimmed.to_ascii_lowercase(),
    ))
}

fn parse_config_bool(value: &str) -> Result<bool, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => Ok(true),
        "0" | "false" | "no" | "off" => Ok(false),
        other => Err(format!(
            "invalid boolean value `{other}`; expected true/false, yes/no, on/off, or 1/0"
        )),
    }
}

fn normalize_optional_config_string(value: &str) -> Option<String> {
    let trimmed = value.trim();
    if trimmed.is_empty()
        || trimmed.eq_ignore_ascii_case("default")
        || trimmed.eq_ignore_ascii_case("none")
    {
        None
    } else {
        Some(trimmed.to_string())
    }
}

fn normalize_theme_config_value(value: &str) -> Result<String, String> {
    if value.trim().is_empty() {
        return Err("theme value cannot be empty".to_string());
    }
    kain_core::tooling_config::normalize_ui_theme_name(value)
}

fn apply_config_key_value(
    config: &mut kain_core::tooling_config::KainToolingConfigFile,
    key: &str,
    value: &str,
) -> Result<(), String> {
    match normalize_config_key(key).as_str() {
        "build.jobs" => {
            config.build.jobs = Some(parse_parallelism_setting_value(value)?);
            config.build.cargo_jobs = None;
            config.build.native_jobs = None;
        }
        "build.cargo-jobs" => {
            config.build.cargo_jobs = Some(parse_parallelism_setting_value(value)?);
        }
        "build.native-jobs" => {
            config.build.native_jobs = Some(parse_parallelism_setting_value(value)?);
        }
        "build.native-profile" => {
            config.build.native_profile = normalize_optional_config_string(value);
        }
        "build.native-opt-level" => {
            config.build.native_opt_level = normalize_optional_config_string(value);
        }
        "build.native-target-cpu" => {
            config.build.native_target_cpu = normalize_optional_config_string(value);
        }
        "build.native-debug-info" => {
            config.build.native_debug_info = Some(parse_config_bool(value)?);
        }
        "ui.color" => {
            config.ui.color = Some(kain_core::tooling_config::KainColorPreference::parse_str(
                value,
            )?);
        }
        "ui.theme" => {
            config.ui.theme = Some(normalize_theme_config_value(value)?);
        }
        "ui.experimental-help" => {
            config.ui.experimental_help = Some(parse_config_bool(value)?);
        }
        "diagnostics.capture" => {
            config.diagnostics.capture =
                Some(kain_core::tooling_config::KainDiagnosticCaptureMode::parse_str(value)?);
        }
        "diagnostics.path" => {
            config.diagnostics.path = Some(PathBuf::from(value.trim()));
        }
        "diagnostics.store-ansi" => {
            config.diagnostics.store_ansi = Some(parse_config_bool(value)?);
        }
        _ => {
            return Err(format!(
                "unknown Kain config key `{}`; expected one of {}",
                key.trim(),
                supported_config_keys().join(", ")
            ));
        }
    }
    Ok(())
}

fn run_config_show(json: bool) -> Result<(), String> {
    let active = kain_core::tooling_config::active_kain_tooling_config();
    if json {
        let encoded = serde_json::to_string_pretty(&active)
            .map_err(|err| format!("failed to serialize active config: {err}"))?;
        println!("{encoded}");
        return Ok(());
    }

    println!("{} Kain Config", p().banner(""));
    if let Some(path) = &active.source_path {
        println!("{} Path: {}", p().status_info(""), path.display());
    } else {
        println!("{} Path: <unresolved>", p().status_warning(""));
    }
    println!(
        "{} Loaded: {}",
        p().status_info(""),
        if active.loaded_from_disk {
            "yes"
        } else {
            "no (defaults)"
        }
    );
    println!(
        "{} UI: theme={} color={} experimental-help={}",
        p().status_info(""),
        active.ui.theme,
        describe_color_preference(active.ui.color),
        active.ui.experimental_help
    );
    println!(
        "{} Build: cargo-jobs={} native-jobs={} host-cores={}",
        p().status_info(""),
        active.build.cargo_jobs, active.build.native_jobs, active.build.available_parallelism
    );
    println!(
        "{} Native defaults: profile={} opt={} target-cpu={} debug-info={}",
        p().status_info(""),
        active.build.native_profile.as_deref().unwrap_or("debug"),
        active.build.native_opt_level.as_deref().unwrap_or("0"),
        active
            .build
            .native_target_cpu
            .as_deref()
            .unwrap_or("default"),
        active.build.native_debug_info.unwrap_or(true)
    );
    println!(
        "{} Diagnostics: capture={} path={} store-ansi={}",
        p().status_info(""),
        describe_diagnostic_capture_mode(active.diagnostics.capture),
        active.diagnostics.path.display(),
        active.diagnostics.store_ansi
    );
    Ok(())
}

fn run_config_set(key: &str, value: &str) -> Result<(), String> {
    let active = kain_core::tooling_config::active_kain_tooling_config();
    let target_path = active
        .source_path
        .clone()
        .ok_or_else(|| "unable to resolve a Kain config path".to_string())?;
    let mut config = load_editable_kain_config(&target_path)?;
    apply_config_key_value(&mut config, key, value)?;
    write_kain_config_file(&target_path, &config)?;
    let reloaded = kain_core::tooling_config::load_kain_tooling_config(Some(&target_path))?;
    kain_core::tooling_config::install_active_kain_tooling_config(reloaded);
    println!(" Updated Kain config: {}", target_path.display());
    println!(" Set {} = {}", normalize_config_key(key), value.trim());
    Ok(())
}

fn run_config_init(path_override: Option<&Path>, force: bool) -> Result<(), String> {
    let active = kain_core::tooling_config::active_kain_tooling_config();
    let target_path = path_override
        .map(Path::to_path_buf)
        .or(active.source_path.clone())
        .ok_or_else(|| "unable to resolve a Kain config path".to_string())?;

    if target_path.exists() && !force {
        return Err(format!(
            "config file already exists at {}; rerun with --force to overwrite it",
            target_path.display()
        ));
    }
    if let Some(parent) = target_path.parent() {
        fs::create_dir_all(parent).map_err(|err| {
            format!(
                "failed to create config parent directory {}: {}",
                parent.display(),
                err
            )
        })?;
    }
    fs::write(&target_path, render_default_kain_config_toml()).map_err(|err| {
        format!(
            "failed to write config file {}: {}",
            target_path.display(),
            err
        )
    })?;
    println!(" Wrote Kain config: {}", target_path.display());
    Ok(())
}

fn is_cargo_bin_binary(path: &Path) -> bool {
    let cargo_home = std::env::var_os("CARGO_HOME")
        .map(PathBuf::from)
        .or_else(|| {
            std::env::var_os("USERPROFILE").map(|profile| PathBuf::from(profile).join(".cargo"))
        });
    cargo_home
        .map(|home| path.starts_with(home.join("bin")))
        .unwrap_or(false)
}

fn is_kain_home_binary(path: &Path) -> bool {
    kain_core::install_layout::is_path_within_kain_home_bin(path)
}

fn collect_path_matches(command_name: &str) -> Vec<PathBuf> {
    which::which_all(command_name)
        .ok()
        .map(|mut paths| paths.by_ref().take(4).collect::<Vec<_>>())
        .unwrap_or_default()
}

fn doctor_path_status(
    current_exe: Option<&Path>,
    path_command: Option<&Path>,
    command_name: &str,
) -> String {
    match (current_exe, path_command) {
        (Some(current), Some(path_entry)) if paths_equivalent(current, path_entry) => {
            "current process matches PATH".to_string()
        }
        (Some(_), Some(path_entry)) => {
            if let Some(active_launcher_path) = active_launcher_path_for_doctor(command_name) {
                if paths_equivalent(&active_launcher_path, path_entry) {
                    return "current process launched through PATH Bazel wrapper".to_string();
                }
            }
            "drift: current process differs from PATH".to_string()
        }
        (Some(_), None) => {
            format!("current process exists, but {command_name} is not resolvable from PATH")
        }
        (None, Some(_)) => {
            format!("PATH resolves {command_name}, but current process path is unknown")
        }
        (None, None) => "unknown".to_string(),
    }
}

fn active_launcher_path_for_doctor(command_name: &str) -> Option<PathBuf> {
    let active_launcher_name = std::env::var("KAIN_ACTIVE_LAUNCHER_NAME").ok()?;
    if active_launcher_name != command_name {
        return None;
    }
    std::env::var_os("KAIN_ACTIVE_LAUNCHER_PATH").map(PathBuf::from)
}

fn paths_equivalent(a: &Path, b: &Path) -> bool {
    match (fs::canonicalize(a), fs::canonicalize(b)) {
        (Ok(left), Ok(right)) => left == right,
        _ => a == b,
    }
}

fn find_project_root(start: &std::path::Path) -> Option<PathBuf> {
    for dir in start.ancestors() {
        if dir.join("KAIN.toml").exists() {
            return Some(dir.to_path_buf());
        }
    }
    None
}

fn staging_dir() -> PathBuf {
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let candidate1 = cwd.join("src-plugins");
    if candidate1.exists() {
        return candidate1.join("_shaders");
    }
    if let Some(parent) = cwd.parent() {
        let candidate2 = parent.join("src-plugins");
        if candidate2.exists() {
            return candidate2.join("_shaders");
        }
    }
    cwd.join("src-plugins").join("_shaders")
}

fn ensure_dir(p: &PathBuf) -> bool {
    if let Err(e) = fs::create_dir_all(p) {
        eprintln!(" Failed to create directory {}: {}", p.display(), e);
        return false;
    }
    true
}

/// Ensure parent directory exists for a file path.
/// Creates all missing parent directories recursively.
/// Returns true on success, false on failure (with error printed).
fn ensure_parent_dir(file_path: &Path) -> bool {
    if let Some(parent) = file_path.parent() {
        if !parent.exists() {
            if let Err(e) = fs::create_dir_all(parent) {
                eprintln!(" Failed to create directory {}: {}", parent.display(), e);
                return false;
            }
        }
    }
    true
}

fn find_runtime_c() -> Option<PathBuf> {
    kain_core::install_layout::resolve_runtime_c_path()
}

fn resolve_native_runtime_bundle() -> Result<Option<ResolvedNativeRuntimeBundle>, String> {
    if let Some(manifest_path) = find_native_runtime_manifest() {
        return load_native_runtime_manifest(&manifest_path).map(Some);
    }

    if let Some(runtime_c) = find_runtime_c() {
        return Ok(Some(ResolvedNativeRuntimeBundle {
            name: "kain-runtime-legacy".to_string(),
            sources: vec![runtime_c],
            include_dirs: Vec::new(),
            defines: Vec::new(),
            archive_groups: Vec::new(),
            cache_root: default_runtime_cache_root(),
            link_libs: default_native_runtime_link_libs(),
            uses_cpp_runtime: false,
        }));
    }

    Ok(None)
}

fn resolve_c_ffi_native_link_inputs(
    source: &str,
    source_path: Option<&Path>,
    clang_cmd: &str,
    native_toolchain_tuning: &NativeToolchainTuning,
) -> Result<CffiNativeLinkInputs, String> {
    let prepare_dir = source_path
        .and_then(|path| path.parent())
        .filter(|path| !path.as_os_str().is_empty())
        .map(Path::to_path_buf)
        .or_else(|| std::env::current_dir().ok());
    let import_specs = kain_c_ffi::detect_c_library_import_specs(source);
    let mut imported_names = import_specs
        .iter()
        .map(|spec| spec.import_name.clone())
        .collect::<BTreeSet<_>>();
    let project_import_names =
        discover_project_c_ffi_import_names(source_path, prepare_dir.as_deref())
            .into_iter()
            .filter(|name| imported_names.insert(name.clone()))
            .collect::<Vec<_>>();
    if import_specs.is_empty() && project_import_names.is_empty() {
        return Ok(CffiNativeLinkInputs::default());
    }
    let prepare = CPrepareContext {
        current_dir: prepare_dir,
        manifest_path: None,
    };

    let mut link_inputs = Vec::new();
    let mut link_libs = Vec::new();
    let mut library_search_paths = Vec::new();
    let compile_args = native_toolchain_tuning.clang_compile_args();
    for spec in import_specs {
        let output = kain_c_ffi::import_library_for_spec(
            &spec,
            &CImportCOptions {
                mode: CArtifactMode::Generate,
                ..CImportCOptions::default()
            },
            &prepare,
        )
        .map_err(|err| err.to_string())?;
        let inputs = kain_c_ffi::prepare_native_link_inputs(&output, clang_cmd, &compile_args)
            .map_err(|err| err.to_string())?;
        link_inputs.extend(inputs.link_inputs);
        link_libs.extend(inputs.link_libs);
        library_search_paths.extend(inputs.library_search_paths);
    }
    for import_name in project_import_names {
        let output = kain_c_ffi::import_library(
            &import_name,
            &CImportCOptions {
                mode: CArtifactMode::Generate,
                ..CImportCOptions::default()
            },
            &prepare,
        )
        .map_err(|err| err.to_string())?;
        let inputs = kain_c_ffi::prepare_native_link_inputs(&output, clang_cmd, &compile_args)
            .map_err(|err| err.to_string())?;
        link_inputs.extend(inputs.link_inputs);
        link_libs.extend(inputs.link_libs);
        library_search_paths.extend(inputs.library_search_paths);
    }

    Ok(CffiNativeLinkInputs {
        link_inputs,
        link_libs: unique_link_libs(link_libs),
        library_search_paths,
    })
}

fn discover_project_c_ffi_import_names(
    source_path: Option<&Path>,
    prepare_dir: Option<&Path>,
) -> Vec<String> {
    let mut anchors = Vec::new();
    if let Some(path) = source_path.and_then(|value| value.parent()) {
        anchors.push(path.to_path_buf());
    }
    if let Some(path) = prepare_dir {
        if !anchors.iter().any(|existing| existing == path) {
            anchors.push(path.to_path_buf());
        }
    }
    if let Ok(path) = std::env::current_dir() {
        if !anchors.iter().any(|existing| existing == &path) {
            anchors.push(path);
        }
    }

    let normalized_source = source_path.map(normalize_project_match_path);
    let mut discovered = BTreeSet::new();
    for anchor in anchors {
        let Ok(workspace) = blade::discover_workspace(&anchor) else {
            continue;
        };
        let Some(blade_name) =
            select_project_blade_name_for_source(&workspace, normalized_source.as_deref(), &anchor)
        else {
            continue;
        };
        for library in workspace.transitive_c_ffi_libraries_for(&blade_name) {
            discovered.insert(library.name);
        }
        if !discovered.is_empty() {
            break;
        }
    }

    discovered.into_iter().collect()
}

fn select_project_blade_name_for_source(
    workspace: &blade::BladeWorkspace,
    normalized_source: Option<&Path>,
    anchor: &Path,
) -> Option<String> {
    if let Some(source) = normalized_source {
        if let Some(blade) = workspace
            .blades
            .iter()
            .filter(|blade| source.starts_with(&blade.root))
            .max_by_key(|blade| blade.root.components().count())
        {
            return Some(blade.name.clone());
        }
    }

    if workspace.blades.len() == 1 {
        return workspace.blades.first().map(|blade| blade.name.clone());
    }

    let normalized_anchor = normalize_project_match_path(anchor);
    workspace
        .blades
        .iter()
        .filter(|blade| normalized_anchor.starts_with(&blade.root))
        .max_by_key(|blade| blade.root.components().count())
        .map(|blade| blade.name.clone())
}

fn normalize_project_match_path(path: &Path) -> PathBuf {
    fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf())
}

fn load_native_runtime_manifest(
    manifest_path: &Path,
) -> Result<ResolvedNativeRuntimeBundle, String> {
    let manifest_source = fs::read_to_string(manifest_path).map_err(|err| {
        format!(
            "unable to read runtime manifest {}: {}",
            manifest_path.display(),
            err
        )
    })?;
    let manifest: NativeRuntimeManifest = toml::from_str(&manifest_source).map_err(|err| {
        format!(
            "unable to parse runtime manifest {}: {}",
            manifest_path.display(),
            err
        )
    })?;
    let selected_sources = current_platform_runtime_sources(&manifest);
    if manifest.sources.is_empty() && selected_sources.is_empty() {
        return Err(format!(
            "runtime manifest {} does not declare any sources",
            manifest_path.display()
        ));
    }

    let manifest_dir = manifest_path.parent().ok_or_else(|| {
        format!(
            "runtime manifest {} has no parent directory",
            manifest_path.display()
        )
    })?;
    let source_entries = manifest
        .sources
        .iter()
        .chain(selected_sources.iter())
        .map(|path| (path.clone(), resolve_runtime_path(manifest_dir, path)))
        .collect::<Vec<_>>();
    let sources = source_entries
        .iter()
        .map(|(_, resolved_path)| resolved_path.clone())
        .collect::<Vec<_>>();
    let include_dirs = manifest
        .include_dirs
        .iter()
        .map(|path| resolve_runtime_path(manifest_dir, path))
        .collect::<Vec<_>>();
    let defines = current_platform_runtime_defines(&manifest);
    let archive_groups =
        resolve_native_runtime_archive_groups(&source_entries, &manifest.archive_groups)?;

    for source in &sources {
        if !source.exists() {
            return Err(format!(
                "runtime source {} does not exist",
                source.display()
            ));
        }
    }

    for include_dir in &include_dirs {
        if !include_dir.exists() {
            return Err(format!(
                "runtime include directory {} does not exist",
                include_dir.display()
            ));
        }
    }

    Ok(ResolvedNativeRuntimeBundle {
        name: manifest
            .name
            .clone()
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_else(|| "kain-native-runtime".to_string()),
        sources,
        include_dirs,
        defines,
        archive_groups,
        cache_root: default_runtime_cache_root(),
        link_libs: unique_link_libs({
            let mut libs = default_native_runtime_link_libs();
            libs.extend(platform_link_libs(&manifest.link));
            libs
        }),
        uses_cpp_runtime: manifest
            .sources
            .iter()
            .chain(selected_sources.iter())
            .any(|path| runtime_source_uses_cpp(path)),
    })
}

fn resolve_native_runtime_archive_groups(
    source_entries: &[(PathBuf, PathBuf)],
    archive_manifests: &[NativeRuntimeArchiveManifest],
) -> Result<Vec<ResolvedNativeRuntimeArchiveGroup>, String> {
    let mut groups = Vec::new();
    let mut claimed_sources = BTreeMap::<PathBuf, String>::new();
    let mut seen_group_names = BTreeSet::new();

    for archive_manifest in archive_manifests {
        let group_name = archive_manifest.name.trim();
        if group_name.is_empty() {
            return Err("runtime archive groups must have a non-empty name".to_string());
        }
        if !seen_group_names.insert(group_name.to_string()) {
            return Err(format!(
                "runtime archive group `{}` is declared more than once",
                group_name
            ));
        }
        if archive_manifest.source_prefixes.is_empty() {
            return Err(format!(
                "runtime archive group `{}` must declare at least one source prefix",
                group_name
            ));
        }

        let mut group_sources = Vec::new();
        for (relative_path, resolved_path) in source_entries {
            if archive_manifest
                .source_prefixes
                .iter()
                .any(|prefix| relative_path.starts_with(prefix))
            {
                if let Some(existing_group) =
                    claimed_sources.insert(relative_path.clone(), group_name.to_string())
                {
                    return Err(format!(
                        "runtime source {} is claimed by archive groups `{}` and `{}`",
                        relative_path.display(),
                        existing_group,
                        group_name
                    ));
                }
                group_sources.push(resolved_path.clone());
            }
        }

        if group_sources.is_empty() {
            let prefixes = archive_manifest
                .source_prefixes
                .iter()
                .map(|prefix| prefix.display().to_string())
                .collect::<Vec<_>>()
                .join(", ");
            return Err(format!(
                "runtime archive group `{}` did not match any sources for prefixes [{}]",
                group_name, prefixes
            ));
        }

        groups.push(ResolvedNativeRuntimeArchiveGroup {
            name: group_name.to_string(),
            uses_cpp_runtime: group_sources
                .iter()
                .any(|path| runtime_source_uses_cpp(path)),
            source_paths: group_sources,
        });
    }

    Ok(groups)
}

fn default_runtime_cache_root() -> PathBuf {
    if let Ok(configured_root) = std::env::var("KAIN_RUNTIME_CACHE_DIR") {
        let configured_root = PathBuf::from(configured_root);
        if !configured_root.as_os_str().is_empty() {
            return configured_root.join(runtime_cache_host_tag());
        }
    }

    PathBuf::from("generated")
        .join("native_runtime")
        .join("cache")
        .join(runtime_cache_host_tag())
}

fn runtime_cache_host_tag() -> String {
    format!("{}-{}", std::env::consts::OS, std::env::consts::ARCH)
}

fn current_platform_runtime_sources(manifest: &NativeRuntimeManifest) -> &[PathBuf] {
    if cfg!(windows) {
        &manifest.windows_sources
    } else if cfg!(target_os = "macos") {
        &manifest.macos_sources
    } else {
        &manifest.linux_sources
    }
}

fn current_platform_runtime_defines(manifest: &NativeRuntimeManifest) -> Vec<String> {
    let mut defines = manifest.defines.clone();
    if cfg!(windows) {
        defines.extend(manifest.windows_defines.iter().cloned());
    } else if cfg!(target_os = "macos") {
        defines.extend(manifest.macos_defines.iter().cloned());
    } else {
        defines.extend(manifest.linux_defines.iter().cloned());
    }
    defines
}

fn compile_native_runtime_bundle(
    bundle: &ResolvedNativeRuntimeBundle,
    clang_cmd: &str,
    toolchain_tuning: &NativeToolchainTuning,
) -> Result<NativeRuntimeCompiledArtifacts, String> {
    use rayon::prelude::*;

    let runtime_cache_dir = bundle.cache_root.join(sanitize_runtime_name(&bundle.name));
    let runtime_obj_dir = runtime_cache_dir.join("objects");
    let runtime_archive_dir = runtime_cache_dir.join("archives");
    fs::create_dir_all(&runtime_obj_dir).map_err(|err| {
        format!(
            "unable to create runtime object directory {}: {}",
            runtime_obj_dir.display(),
            err
        )
    })?;
    fs::create_dir_all(&runtime_archive_dir).map_err(|err| {
        format!(
            "unable to create runtime archive directory {}: {}",
            runtime_archive_dir.display(),
            err
        )
    })?;

    let object_ext = if cfg!(windows) { "obj" } else { "o" };
    let mut object_paths_by_source = BTreeMap::<PathBuf, PathBuf>::new();
    let mut object_fingerprints_by_source = BTreeMap::<PathBuf, String>::new();
    let mut reused_object_count = 0usize;
    let mut compiled_object_count = 0usize;

    let native_jobs = kain_core::tooling_config::active_native_parallelism();
    let compile_objects = || -> Result<Vec<NativeRuntimeObjectBuildRecord>, String> {
        bundle
            .sources
            .par_iter()
            .enumerate()
            .map(|(index, source)| {
                let cache_paths = build_native_runtime_object_cache_paths(
                    &runtime_obj_dir,
                    index,
                    source,
                    object_ext,
                );
                let compile_fingerprint = build_native_runtime_compile_fingerprint(
                    bundle,
                    clang_cmd,
                    source,
                    toolchain_tuning,
                )?;

                if native_runtime_object_cache_is_fresh(&cache_paths, &compile_fingerprint) {
                    return Ok(NativeRuntimeObjectBuildRecord {
                        source: source.clone(),
                        object_path: cache_paths.object_path,
                        fingerprint: compile_fingerprint,
                        was_reused: true,
                    });
                }

                compile_native_runtime_object(
                    bundle,
                    clang_cmd,
                    toolchain_tuning,
                    source,
                    &cache_paths,
                )?;
                fs::write(&cache_paths.fingerprint_path, &compile_fingerprint).map_err(|err| {
                    format!(
                        "unable to write runtime build fingerprint {}: {}",
                        cache_paths.fingerprint_path.display(),
                        err
                    )
                })?;

                Ok(NativeRuntimeObjectBuildRecord {
                    source: source.clone(),
                    object_path: cache_paths.object_path,
                    fingerprint: compile_fingerprint,
                    was_reused: false,
                })
            })
            .collect::<Result<Vec<_>, _>>()
    };

    let object_build_records = if native_jobs > 1 && bundle.sources.len() > 1 {
        rayon::ThreadPoolBuilder::new()
            .num_threads(native_jobs)
            .build()
            .map_err(|err| format!("unable to build native runtime worker pool: {err}"))?
            .install(compile_objects)?
    } else {
        compile_objects()?
    };

    for record in object_build_records {
        if record.was_reused {
            reused_object_count += 1;
        } else {
            compiled_object_count += 1;
        }
        object_fingerprints_by_source.insert(record.source.clone(), record.fingerprint.clone());
        object_paths_by_source.insert(record.source, record.object_path);
    }

    let archived_sources = bundle
        .archive_groups
        .iter()
        .flat_map(|group| group.source_paths.iter().cloned())
        .collect::<BTreeSet<_>>();
    let loose_objects = bundle
        .sources
        .iter()
        .filter(|source| !archived_sources.contains(*source))
        .map(|source| {
            object_paths_by_source.get(source).cloned().ok_or_else(|| {
                format!(
                    "runtime object cache is missing a compiled object for {}",
                    source.display()
                )
            })
        })
        .collect::<Result<Vec<_>, _>>()?;

    let mut static_archives = Vec::new();
    let mut reused_archive_count = 0usize;
    let mut rebuilt_archive_count = 0usize;

    if !bundle.archive_groups.is_empty() {
        let archiver = find_native_runtime_archiver(clang_cmd)?;
        for archive_group in &bundle.archive_groups {
            let object_paths = archive_group
                .source_paths
                .iter()
                .map(|source| {
                    object_paths_by_source.get(source).cloned().ok_or_else(|| {
                        format!(
                            "runtime archive group `{}` is missing object output for {}",
                            archive_group.name,
                            source.display()
                        )
                    })
                })
                .collect::<Result<Vec<_>, _>>()?;
            let archive_paths = build_native_runtime_archive_cache_paths(
                &runtime_archive_dir,
                &archive_group.name,
                archiver.archive_ext,
            );
            let archive_fingerprint = build_native_runtime_archive_fingerprint(
                bundle,
                &archiver,
                archive_group,
                &object_fingerprints_by_source,
            );

            if native_runtime_archive_cache_is_fresh(
                &archive_paths,
                &archive_fingerprint,
                &object_paths,
            ) {
                reused_archive_count += 1;
            } else {
                build_native_runtime_static_archive(
                    &archiver,
                    &archive_paths.archive_path,
                    &object_paths,
                )?;
                fs::write(&archive_paths.fingerprint_path, &archive_fingerprint).map_err(
                    |err| {
                        format!(
                            "unable to write runtime archive fingerprint {}: {}",
                            archive_paths.fingerprint_path.display(),
                            err
                        )
                    },
                )?;
                rebuilt_archive_count += 1;
            }

            static_archives.push(archive_paths.archive_path);
        }
    }

    if compiled_object_count > 0
        || reused_object_count > 0
        || rebuilt_archive_count > 0
        || reused_archive_count > 0
    {
        eprintln!(
            " Native runtime cache: {} reused, {} compiled, {} archives reused, {} archives rebuilt",
            reused_object_count, compiled_object_count, reused_archive_count, rebuilt_archive_count
        );
    }

    Ok(NativeRuntimeCompiledArtifacts {
        loose_objects,
        static_archives,
    })
}

fn build_native_runtime_object_cache_paths(
    runtime_obj_dir: &Path,
    index: usize,
    source: &Path,
    object_ext: &str,
) -> NativeRuntimeObjectCachePaths {
    let object_stem = format!(
        "{:02}_{}",
        index,
        sanitize_runtime_name(
            &source
                .file_stem()
                .and_then(|stem| stem.to_str())
                .unwrap_or("runtime")
        )
    );

    NativeRuntimeObjectCachePaths {
        object_path: runtime_obj_dir.join(format!("{object_stem}.{object_ext}")),
        depfile_path: runtime_obj_dir.join(format!("{object_stem}.{object_ext}.d")),
        fingerprint_path: runtime_obj_dir.join(format!("{object_stem}.{object_ext}.fingerprint")),
    }
}

fn build_native_runtime_archive_cache_paths(
    runtime_archive_dir: &Path,
    archive_group_name: &str,
    archive_ext: &str,
) -> NativeRuntimeStaticArchivePaths {
    let archive_stem = sanitize_runtime_name(archive_group_name);
    let archive_file_name = if archive_ext.eq_ignore_ascii_case("lib") {
        format!("{}.{}", archive_stem, archive_ext)
    } else {
        format!("lib{}.{}", archive_stem, archive_ext)
    };

    NativeRuntimeStaticArchivePaths {
        archive_path: runtime_archive_dir.join(&archive_file_name),
        fingerprint_path: runtime_archive_dir.join(format!("{}.fingerprint", archive_file_name)),
    }
}

fn compile_native_runtime_object(
    bundle: &ResolvedNativeRuntimeBundle,
    clang_cmd: &str,
    toolchain_tuning: &NativeToolchainTuning,
    source: &Path,
    cache_paths: &NativeRuntimeObjectCachePaths,
) -> Result<(), String> {
    let max_attempts = if cfg!(windows) { 3usize } else { 1usize };

    for attempt in 0..max_attempts {
        clear_native_runtime_object_cache_slot(cache_paths)?;

        let mut compile_cmd = std::process::Command::new(clang_cmd);
        kain_core::install_layout::apply_windows_msvc_link_env(&mut compile_cmd);
        compile_cmd
            .arg("-c")
            .arg(source)
            .arg("-o")
            .arg(&cache_paths.object_path)
            .arg("-MMD")
            .arg("-MF")
            .arg(&cache_paths.depfile_path)
            .arg("-MT")
            .arg("kain_runtime_target");
        toolchain_tuning.apply_to_clang_command(&mut compile_cmd);

        if runtime_source_uses_cpp(source) {
            compile_cmd.arg("-std=c++20");
        }

        for include_dir in &bundle.include_dirs {
            compile_cmd.arg("-I").arg(include_dir);
        }
        for define in &bundle.defines {
            compile_cmd.arg(format!("-D{}", define));
        }

        let output = compile_cmd
            .output()
            .map_err(|err| format!("unable to invoke clang for {}: {}", source.display(), err))?;
        let stdout = String::from_utf8_lossy(&output.stdout);
        let stderr = String::from_utf8_lossy(&output.stderr);
        if !stdout.is_empty() {
            print!("{stdout}");
        }
        if !stderr.is_empty() {
            eprint!("{stderr}");
        }
        if output.status.success() {
            return Ok(());
        }

        let combined_lowercase = format!("{stdout}{stderr}").to_ascii_lowercase();
        let can_retry_transient_lock = cfg!(windows)
            && attempt + 1 < max_attempts
            && combined_lowercase.contains("permission denied");
        clear_native_runtime_object_cache_slot_best_effort(cache_paths);
        if can_retry_transient_lock {
            std::thread::sleep(Duration::from_millis(25 * (attempt as u64 + 1)));
            continue;
        }

        return Err(format!(
            "clang returned a non-zero status while compiling {}",
            source.display()
        ));
    }

    Err(format!(
        "clang returned a non-zero status while compiling {}",
        source.display()
    ))
}

fn clear_native_runtime_object_cache_slot(
    cache_paths: &NativeRuntimeObjectCachePaths,
) -> Result<(), String> {
    remove_native_runtime_file_with_retry(&cache_paths.object_path)?;
    remove_native_runtime_file_with_retry(&cache_paths.depfile_path)?;
    remove_native_runtime_file_with_retry(&cache_paths.fingerprint_path)?;
    remove_native_runtime_object_tmp_files(cache_paths)?;
    Ok(())
}

fn clear_native_runtime_object_cache_slot_best_effort(cache_paths: &NativeRuntimeObjectCachePaths) {
    let _ = clear_native_runtime_object_cache_slot(cache_paths);
}

fn remove_native_runtime_file_with_retry(path: &Path) -> Result<(), String> {
    let max_attempts = if cfg!(windows) { 12usize } else { 1usize };

    for attempt in 0..max_attempts {
        match fs::remove_file(path) {
            Ok(()) => return Ok(()),
            Err(err) if err.kind() == io::ErrorKind::NotFound => return Ok(()),
            Err(err) => {
                if attempt + 1 < max_attempts {
                    std::thread::sleep(Duration::from_millis(25 * (attempt as u64 + 1)));
                    continue;
                }
                return Err(format!(
                    "unable to remove stale runtime cache artifact {}: {}",
                    path.display(),
                    err
                ));
            }
        }
    }

    Ok(())
}

fn remove_native_runtime_object_tmp_files(
    cache_paths: &NativeRuntimeObjectCachePaths,
) -> Result<(), String> {
    let Some(parent_dir) = cache_paths.object_path.parent() else {
        return Ok(());
    };
    let Some(stem) = cache_paths
        .object_path
        .file_stem()
        .and_then(|value| value.to_str())
    else {
        return Ok(());
    };
    let Some(extension) = cache_paths
        .object_path
        .extension()
        .and_then(|value| value.to_str())
    else {
        return Ok(());
    };
    let tmp_suffix = format!(".{extension}.tmp");

    let entries = fs::read_dir(parent_dir).map_err(|err| {
        format!(
            "unable to inspect runtime object cache directory {}: {}",
            parent_dir.display(),
            err
        )
    })?;
    for entry_result in entries {
        let entry = entry_result.map_err(|err| {
            format!(
                "unable to read runtime object cache entry under {}: {}",
                parent_dir.display(),
                err
            )
        })?;
        let entry_name = entry.file_name();
        let Some(entry_name) = entry_name.to_str() else {
            continue;
        };
        if !entry_name.starts_with(stem) || !entry_name.ends_with(&tmp_suffix) {
            continue;
        }
        // Stale per-object temp files are never part of the live cache key.
        // If Windows still has one open after retries, leave it behind and
        // keep the actual object/depfile/fingerprint lane buildable.
        let _ = remove_native_runtime_file_with_retry(&entry.path());
    }
    Ok(())
}

fn build_native_runtime_compile_fingerprint(
    bundle: &ResolvedNativeRuntimeBundle,
    clang_cmd: &str,
    source: &Path,
    toolchain_tuning: &NativeToolchainTuning,
) -> Result<String, String> {
    let mut fingerprint_lines = vec![
        "kain-native-runtime-cache-v1".to_string(),
        format!("bundle={}", bundle.name),
        format!("clang={}", clang_cmd),
        format!("source={}", source.display()),
        format!("cpp={}", runtime_source_uses_cpp(source)),
    ];
    fingerprint_lines.extend(toolchain_tuning.fingerprint_lines());

    for include_dir in &bundle.include_dirs {
        fingerprint_lines.push(format!("include={}", include_dir.display()));
    }
    for define in &bundle.defines {
        fingerprint_lines.push(format!("define={}", define));
    }

    Ok(fingerprint_lines.join("\n"))
}

fn build_native_runtime_archive_fingerprint(
    bundle: &ResolvedNativeRuntimeBundle,
    archiver: &NativeRuntimeArchiver,
    archive_group: &ResolvedNativeRuntimeArchiveGroup,
    object_fingerprints_by_source: &BTreeMap<PathBuf, String>,
) -> String {
    let mut fingerprint_lines = vec![
        "kain-native-runtime-archive-cache-v1".to_string(),
        format!("bundle={}", bundle.name),
        format!("group={}", archive_group.name),
        format!("archiver={}", archiver.command),
        format!("archiver_ext={}", archiver.archive_ext),
        format!("cpp={}", archive_group.uses_cpp_runtime),
    ];
    for source_path in &archive_group.source_paths {
        fingerprint_lines.push(format!("source={}", source_path.display()));
        if let Some(object_fingerprint) = object_fingerprints_by_source.get(source_path) {
            fingerprint_lines.push(format!("object_fingerprint={}", object_fingerprint));
        }
    }
    fingerprint_lines.join("\n")
}

fn native_runtime_object_cache_is_fresh(
    cache_paths: &NativeRuntimeObjectCachePaths,
    compile_fingerprint: &str,
) -> bool {
    if !cache_paths.object_path.exists()
        || !cache_paths.depfile_path.exists()
        || !cache_paths.fingerprint_path.exists()
    {
        return false;
    }

    let stored_fingerprint = match fs::read_to_string(&cache_paths.fingerprint_path) {
        Ok(value) => value,
        Err(_) => return false,
    };
    if stored_fingerprint != compile_fingerprint {
        return false;
    }

    let object_modified =
        match fs::metadata(&cache_paths.object_path).and_then(|meta| meta.modified()) {
            Ok(value) => value,
            Err(_) => return false,
        };

    let dependency_paths = match parse_native_runtime_depfile(&cache_paths.depfile_path) {
        Ok(paths) => paths,
        Err(_) => return false,
    };
    if dependency_paths.is_empty() {
        return false;
    }

    for dependency_path in dependency_paths {
        let dependency_modified =
            match fs::metadata(&dependency_path).and_then(|meta| meta.modified()) {
                Ok(value) => value,
                Err(_) => return false,
            };
        if dependency_modified > object_modified {
            return false;
        }
    }

    true
}

fn native_runtime_archive_cache_is_fresh(
    cache_paths: &NativeRuntimeStaticArchivePaths,
    archive_fingerprint: &str,
    object_paths: &[PathBuf],
) -> bool {
    if object_paths.is_empty()
        || !cache_paths.archive_path.exists()
        || !cache_paths.fingerprint_path.exists()
    {
        return false;
    }

    let stored_fingerprint = match fs::read_to_string(&cache_paths.fingerprint_path) {
        Ok(value) => value,
        Err(_) => return false,
    };
    if stored_fingerprint != archive_fingerprint {
        return false;
    }

    let archive_modified =
        match fs::metadata(&cache_paths.archive_path).and_then(|meta| meta.modified()) {
            Ok(value) => value,
            Err(_) => return false,
        };

    for object_path in object_paths {
        let object_modified = match fs::metadata(object_path).and_then(|meta| meta.modified()) {
            Ok(value) => value,
            Err(_) => return false,
        };
        if object_modified > archive_modified {
            return false;
        }
    }

    true
}

fn find_native_runtime_archiver(clang_cmd: &str) -> Result<NativeRuntimeArchiver, String> {
    if let Ok(configured_path) = std::env::var("KAIN_AR_PATH") {
        let configured_path = PathBuf::from(configured_path);
        if let Some(resolved) = resolve_runtime_tool_path(&configured_path) {
            return Ok(classify_native_runtime_archiver(resolved));
        }
        return Err(format!(
            "KAIN_AR_PATH points to {}, but that archiver was not found",
            configured_path.display()
        ));
    }

    let mut candidates = Vec::<PathBuf>::new();
    if let Some(compiler_path) = resolve_runtime_tool_path(Path::new(clang_cmd)) {
        if let Some(parent_dir) = compiler_path.parent() {
            if cfg!(windows) {
                candidates.push(parent_dir.join("llvm-lib.exe"));
                candidates.push(parent_dir.join("llvm-ar.exe"));
                candidates.push(parent_dir.join("lib.exe"));
            } else {
                candidates.push(parent_dir.join("llvm-ar"));
                candidates.push(parent_dir.join("ar"));
            }
        }
    }

    if cfg!(windows) {
        candidates.extend([
            PathBuf::from("llvm-lib.exe"),
            PathBuf::from("llvm-ar.exe"),
            PathBuf::from("lib.exe"),
            PathBuf::from("llvm-lib"),
            PathBuf::from("llvm-ar"),
            PathBuf::from("ar"),
        ]);
    } else {
        candidates.extend([PathBuf::from("llvm-ar"), PathBuf::from("ar")]);
    }

    let mut seen_candidates = BTreeSet::new();
    for candidate in candidates {
        let candidate_key = candidate.to_string_lossy().into_owned();
        if !seen_candidates.insert(candidate_key) {
            continue;
        }
        if let Some(resolved) = resolve_runtime_tool_path(&candidate) {
            return Ok(classify_native_runtime_archiver(resolved));
        }
    }

    Err(format!(
        "unable to locate a static archiver for native runtime archives; set KAIN_AR_PATH or install llvm-ar/lib.exe"
    ))
}

fn resolve_runtime_tool_path(candidate: &Path) -> Option<PathBuf> {
    if candidate.components().count() > 1 || candidate.is_absolute() {
        return candidate.exists().then(|| candidate.to_path_buf());
    }
    which::which(candidate).ok()
}

fn classify_native_runtime_archiver(path: PathBuf) -> NativeRuntimeArchiver {
    let file_name = path
        .file_name()
        .and_then(|value| value.to_str())
        .unwrap_or_default()
        .to_ascii_lowercase();
    let flavor = if file_name == "lib.exe" || file_name == "llvm-lib.exe" || file_name == "llvm-lib"
    {
        NativeRuntimeArchiverFlavor::MsvcLib
    } else {
        NativeRuntimeArchiverFlavor::GnuAr
    };

    NativeRuntimeArchiver {
        command: path.to_string_lossy().into_owned(),
        flavor,
        archive_ext: match flavor {
            NativeRuntimeArchiverFlavor::MsvcLib => "lib",
            NativeRuntimeArchiverFlavor::GnuAr => "a",
        },
    }
}

fn build_native_runtime_static_archive(
    archiver: &NativeRuntimeArchiver,
    archive_path: &Path,
    object_paths: &[PathBuf],
) -> Result<(), String> {
    if object_paths.is_empty() {
        return Err(format!(
            "cannot build runtime archive {} with no object files",
            archive_path.display()
        ));
    }

    if archive_path.exists() {
        fs::remove_file(archive_path).map_err(|err| {
            format!(
                "unable to remove stale runtime archive {}: {}",
                archive_path.display(),
                err
            )
        })?;
    }

    let mut command = std::process::Command::new(&archiver.command);
    kain_core::install_layout::apply_windows_msvc_link_env(&mut command);
    match archiver.flavor {
        NativeRuntimeArchiverFlavor::GnuAr => {
            command.arg("rcs").arg(archive_path);
            for object_path in object_paths {
                command.arg(object_path);
            }
        }
        NativeRuntimeArchiverFlavor::MsvcLib => {
            command.arg("/nologo");
            command.arg(format!("/OUT:{}", archive_path.display()));
            for object_path in object_paths {
                command.arg(object_path);
            }
        }
    }

    let status = command.status().map_err(|err| {
        format!(
            "unable to invoke runtime archiver {} for {}: {}",
            archiver.command,
            archive_path.display(),
            err
        )
    })?;
    if !status.success() {
        let _ = fs::remove_file(archive_path);
        return Err(format!(
            "runtime archiver {} returned a non-zero status while building {}",
            archiver.command,
            archive_path.display()
        ));
    }
    if !archive_path.exists() {
        return Err(format!(
            "runtime archiver {} did not create {}",
            archiver.command,
            archive_path.display()
        ));
    }

    Ok(())
}

fn parse_native_runtime_depfile(depfile_path: &Path) -> Result<Vec<PathBuf>, String> {
    let depfile_contents = fs::read_to_string(depfile_path).map_err(|err| {
        format!(
            "unable to read native runtime depfile {}: {}",
            depfile_path.display(),
            err
        )
    })?;
    let depfile_contents = depfile_contents.replace("\\\r\n", "").replace("\\\n", "");
    let dependency_section = depfile_contents
        .split_once(':')
        .map(|(_, dependencies)| dependencies)
        .ok_or_else(|| {
            format!(
                "native runtime depfile {} did not contain a target separator",
                depfile_path.display()
            )
        })?;
    let depfile_cwd = std::env::current_dir().map_err(|err| {
        format!(
            "unable to resolve cwd while parsing native runtime depfile {}: {}",
            depfile_path.display(),
            err
        )
    })?;

    let mut dependency_paths = Vec::new();
    let mut current_token = String::new();
    let mut chars = dependency_section.chars().peekable();
    while let Some(ch) = chars.next() {
        if ch == '\\' {
            match chars.peek().copied() {
                Some('\n') => {
                    chars.next();
                }
                Some('\r') => {
                    chars.next();
                    if matches!(chars.peek(), Some('\n')) {
                        chars.next();
                    }
                }
                Some(escaped) if escaped.is_whitespace() => {
                    current_token.push(escaped);
                    chars.next();
                }
                _ => current_token.push('\\'),
            }
            continue;
        }

        if ch.is_whitespace() {
            if !current_token.is_empty() {
                let dependency_path = PathBuf::from(&current_token);
                if dependency_path.is_absolute() {
                    dependency_paths.push(dependency_path);
                } else {
                    dependency_paths.push(depfile_cwd.join(dependency_path));
                }
                current_token.clear();
            }
            continue;
        }

        current_token.push(ch);
    }

    if !current_token.is_empty() {
        let dependency_path = PathBuf::from(&current_token);
        if dependency_path.is_absolute() {
            dependency_paths.push(dependency_path);
        } else {
            dependency_paths.push(depfile_cwd.join(dependency_path));
        }
    }

    Ok(dependency_paths)
}

fn sanitize_runtime_name(value: &str) -> String {
    let sanitized = value
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() {
                ch.to_ascii_lowercase()
            } else {
                '_'
            }
        })
        .collect::<String>();
    let trimmed = sanitized.trim_matches('_');
    if trimmed.is_empty() {
        "runtime".to_string()
    } else {
        trimmed.to_string()
    }
}

fn resolve_runtime_path(root: &Path, value: &Path) -> PathBuf {
    if value.is_absolute() {
        value.to_path_buf()
    } else {
        root.join(value)
    }
}

fn runtime_source_uses_cpp(value: &Path) -> bool {
    matches!(
        value.extension().and_then(|extension| extension.to_str()),
        Some("cc" | "cpp" | "cxx" | "mm")
    )
}

fn unique_link_libs(values: Vec<String>) -> Vec<String> {
    let mut seen = BTreeSet::new();
    let mut ordered = Vec::new();
    for value in values {
        if seen.insert(value.clone()) {
            ordered.push(value);
        }
    }
    ordered
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

fn find_native_runtime_manifest() -> Option<PathBuf> {
    kain_core::install_layout::resolve_native_runtime_manifest_path()
}

#[cfg(test)]
fn native_runtime_manifest_candidate_suffixes() -> [PathBuf; 3] {
    let suffixes = kain_core::install_layout::native_runtime_manifest_candidate_suffixes();
    [
        PathBuf::from(suffixes[0]),
        PathBuf::from(suffixes[1]),
        PathBuf::from(suffixes[2]),
    ]
}

#[cfg(test)]
mod tests {
    use super::{
        apply_config_key_value, build_native_runtime_archive_fingerprint,
        build_native_runtime_compile_fingerprint, build_native_runtime_object_cache_paths,
        default_native_runtime_link_libs, default_runtime_cache_root,
        // DEPRECATED(2026-06-08): --gc-sections handles dead-stripping now.
        llvm_native_runtime_elision_decision_from_ir, load_editable_kain_config,
        load_native_runtime_manifest, native_runtime_object_cache_is_fresh,
        parse_native_runtime_depfile, parse_native_toolchain_tuning, platform_link_libs,
        resolve_c_ffi_native_link_inputs, resolve_native_runtime_archive_groups,
        run_format_command, runtime_source_uses_cpp, sanitize_runtime_name,
        slice_llvm_native_executable_ir, unique_link_libs, NativeRuntimeArchiveManifest,
        NativeRuntimeArchiver, NativeRuntimeArchiverFlavor, NativeRuntimeLinkManifest,
        NativeToolchainProfile, ResolvedNativeRuntimeArchiveGroup, ResolvedNativeRuntimeBundle,
    };
    use kain_core::tooling_config::{KainColorPreference, KainParallelismSetting};
    use kain_core::CompileTarget;
    use kain_driver::DriverSession;
    use std::{fs, path::Path, path::PathBuf, thread::sleep, time::Duration};

    #[test]
    fn sanitize_runtime_name_keeps_object_filenames_stable() {
        assert_eq!(sanitize_runtime_name("Kain Runtime"), "kain_runtime");
        assert_eq!(sanitize_runtime_name("###"), "runtime");
    }

    #[test]
    fn llvm_runtime_elision_accepts_closed_reachable_graph() {
        let ir = r#"
declare void @KAIN_runtime_init()
declare void @llvm.lifetime.start.p0(i64, ptr)

define i64 @helper(i64 %value) {
entry:
  call void @llvm.lifetime.start.p0(i64 8, ptr null)
  ret i64 %value
}

define void @unreachable_runtime_wrapper() {
entry:
  call void @KAIN_runtime_init()
  ret void
}

define i32 @main() {
entry:
  %value = call i64 @helper(i64 7)
  ret i32 0
}
"#;

        // DEPRECATED(2026-06-08): --gc-sections handles dead-stripping now.
        let decision = llvm_native_runtime_elision_decision_from_ir(ir);

        assert!(decision.can_elide, "{decision:?}");
        assert_eq!(decision.reachable_external_targets, Vec::<String>::new());
        assert_eq!(decision.reachable_functions, 2);
    }

    #[test]
    fn llvm_runtime_elision_rejects_reachable_external_calls() {
        let ir = r#"
declare void @KAIN_runtime_init()

define void @runtime_init() {
entry:
  call void @KAIN_runtime_init()
  ret void
}

define i32 @main() {
entry:
  call void @runtime_init()
  ret i32 0
}
"#;

        // DEPRECATED(2026-06-08): --gc-sections handles dead-stripping now.
        let decision = llvm_native_runtime_elision_decision_from_ir(ir);

        assert!(!decision.can_elide);
        assert_eq!(
            decision.reachable_external_targets,
            vec!["KAIN_runtime_init".to_string()]
        );
    }

    #[test]
    fn llvm_runtime_elision_handles_quoted_symbols() {
        let ir = r#"
define i32 @"main"() {
entry:
  call void @"kain.helper"()
  ret i32 0
}

define void @"kain.helper"() {
entry:
  ret void
}
"#;

        // DEPRECATED(2026-06-08): --gc-sections handles dead-stripping now.
        let decision = llvm_native_runtime_elision_decision_from_ir(ir);

        assert!(decision.can_elide, "{decision:?}");
        assert_eq!(decision.reachable_functions, 2);
    }

    #[test]
    fn llvm_native_ir_slicing_removes_unreachable_runtime_bodies() {
        let ir = r#"
declare void @llvm.lifetime.start.p0(i64, ptr)
declare void @unused_runtime()
@message = internal constant [1 x i8] c"\00"

define i64 @helper(i64 %value) {
entry:
  call void @llvm.lifetime.start.p0(i64 8, ptr null)
  ret i64 %value
}

define void @unreachable_runtime_wrapper() {
entry:
  call void @unused_runtime()
  ret void
}

define i32 @main() {
entry:
  %value = call i64 @helper(i64 7)
  ret i32 0
}
"#;

        let (sliced, stats) = slice_llvm_native_executable_ir(ir)
            .expect("slice")
            .expect("expected slice");

        assert!(sliced.contains("define i64 @helper"));
        assert!(sliced.contains("define i32 @main"));
        assert!(sliced.contains("@message = internal constant"));
        assert!(sliced.contains("declare void @llvm.lifetime.start.p0"));
        assert!(!sliced.contains("define void @unreachable_runtime_wrapper"));
        assert!(!sliced.contains("declare void @unused_runtime"));
        assert_eq!(stats.removed_functions, 1);
    }

    #[test]
    fn llvm_native_ir_slicing_preserves_address_taken_callbacks() {
        let ir = r#"
declare void @register_callback(i8*)

define void @handler() {
entry:
  ret void
}

define void @dead_runtime_wrapper() {
entry:
  ret void
}

define i32 @main() {
entry:
  call void @register_callback(i8* bitcast (void ()* @handler to i8*))
  ret i32 0
}
"#;

        let (sliced, _stats) = slice_llvm_native_executable_ir(ir)
            .expect("slice")
            .expect("expected slice");
        // DEPRECATED(2026-06-08): --gc-sections handles dead-stripping now.
        let decision = llvm_native_runtime_elision_decision_from_ir(&sliced);

        assert!(sliced.contains("define void @handler"));
        assert!(sliced.contains("declare void @register_callback"));
        assert!(!decision.can_elide);
        assert_eq!(
            decision.reachable_external_targets,
            vec!["register_callback".to_string()]
        );
    }

    #[test]
    fn llvm_native_ir_slicing_preserves_top_level_external_declarations() {
        let ir = r#"
declare void @registered_from_global()
@callback_table = internal global [1 x ptr] [ptr @registered_from_global]

define void @dead_runtime_wrapper() {
entry:
  ret void
}

define i32 @main() {
entry:
  ret i32 0
}
"#;

        let (sliced, _stats) = slice_llvm_native_executable_ir(ir)
            .expect("slice")
            .expect("expected slice");
        // DEPRECATED(2026-06-08): --gc-sections handles dead-stripping now.
        let decision = llvm_native_runtime_elision_decision_from_ir(&sliced);

        assert!(sliced.contains("declare void @registered_from_global"));
        assert!(sliced.contains("@callback_table = internal global"));
        assert!(!sliced.contains("define void @dead_runtime_wrapper"));
        assert!(!decision.can_elide);
        assert_eq!(
            decision.reachable_external_targets,
            vec!["registered_from_global".to_string()]
        );
    }

    #[test]
    fn config_set_updates_parallelism_and_theme_keys() {
        let mut config = kain_core::tooling_config::KainToolingConfigFile::default();
        config.build.cargo_jobs = Some(KainParallelismSetting::Count(15));
        config.build.native_jobs = Some(KainParallelismSetting::Count(15));

        apply_config_key_value(&mut config, "build.jobs", "all").expect("jobs");
        apply_config_key_value(&mut config, "ui.theme", "ember").expect("theme");
        apply_config_key_value(&mut config, "ui.color", "never").expect("color");
        apply_config_key_value(&mut config, "ui.experimental-help", "false")
            .expect("experimental help");
        apply_config_key_value(&mut config, "diagnostics.capture", "failures")
            .expect("diagnostics capture");
        apply_config_key_value(&mut config, "diagnostics.path", "logs/errors.jsonl")
            .expect("diagnostics path");
        apply_config_key_value(&mut config, "diagnostics.store-ansi", "false")
            .expect("diagnostics ansi");

        assert_eq!(
            config.build.jobs,
            Some(KainParallelismSetting::Preset("all".to_string()))
        );
        assert!(config.build.cargo_jobs.is_none());
        assert!(config.build.native_jobs.is_none());
        assert_eq!(config.ui.theme.as_deref(), Some("sandstone"));
        assert_eq!(config.ui.color, Some(KainColorPreference::Never));
        assert_eq!(config.ui.experimental_help, Some(false));
        assert_eq!(
            config.diagnostics.capture,
            Some(kain_core::tooling_config::KainDiagnosticCaptureMode::Failures)
        );
        assert_eq!(
            config.diagnostics.path,
            Some(PathBuf::from("logs/errors.jsonl"))
        );
        assert_eq!(config.diagnostics.store_ansi, Some(false));
    }

    #[test]
    fn editable_config_loader_defaults_missing_files() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let missing_path = temp_dir.path().join("config.toml");

        let config = load_editable_kain_config(&missing_path).expect("default config");
        assert_eq!(config.schema, 1);
        assert!(config.build.jobs.is_none());
        assert!(config.ui.theme.is_none());
    }

    #[test]
    fn format_command_check_detects_unformatted_files_in_directories() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let source_dir = temp_dir.path().join("src");
        fs::create_dir_all(&source_dir).expect("source dir");
        fs::write(source_dir.join("clean.kn"), "let clean = 1\n").expect("clean file");
        fs::write(source_dir.join("messy.kn"), "let messy=2\n").expect("messy file");

        assert!(!run_format_command(
            vec![temp_dir.path().to_path_buf()],
            true,
            false
        ));
        assert_eq!(
            fs::read_to_string(source_dir.join("messy.kn")).expect("messy file"),
            "let messy=2\n"
        );
    }

    #[test]
    fn format_command_write_rewrites_multiple_expanded_paths() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let source_dir = temp_dir.path().join("src");
        let generated_dir = temp_dir.path().join(".kain").join("cache");
        fs::create_dir_all(&source_dir).expect("source dir");
        fs::create_dir_all(&generated_dir).expect("generated dir");

        let alpha = source_dir.join("alpha.kn");
        let beta = source_dir.join("beta.kn");
        let skipped = generated_dir.join("skip.kn");
        fs::write(&alpha, "let alpha=1\n").expect("alpha");
        fs::write(&beta, "let beta=alpha+1\n").expect("beta");
        fs::write(&skipped, "let skipped=0\n").expect("skipped");

        assert!(run_format_command(
            vec![temp_dir.path().to_path_buf()],
            false,
            true
        ));
        assert_eq!(
            fs::read_to_string(&alpha).expect("alpha"),
            "let alpha = 1\n"
        );
        assert_eq!(
            fs::read_to_string(&beta).expect("beta"),
            "let beta = alpha + 1\n"
        );
        assert_eq!(
            fs::read_to_string(&skipped).expect("skipped"),
            "let skipped=0\n"
        );
    }

    #[test]
    fn native_runtime_manifest_candidates_prefer_core_runtime() {
        let candidates = super::native_runtime_manifest_candidate_suffixes();

        assert_eq!(
            candidates[0],
            PathBuf::from("runtime/native_core_runtime.toml")
        );
        assert!(candidates.contains(&PathBuf::from("runtime/native_runtime.toml")));
        assert!(candidates.contains(&PathBuf::from("runtime/native/runtime.toml")));
    }

    #[test]
    fn windows_default_native_runtime_link_libs_do_not_force_opengl() {
        if cfg!(windows) {
            assert!(!default_native_runtime_link_libs()
                .iter()
                .any(|value| value == "opengl32"));
        }
    }

    #[test]
    fn runtime_manifest_resolves_relative_paths() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let manifest_dir = temp_dir.path().join("runtime");
        let source_dir = manifest_dir.join("src");
        let include_dir = manifest_dir.join("include");
        fs::create_dir_all(&source_dir).expect("source dir");
        fs::create_dir_all(&include_dir).expect("include dir");
        fs::write(
            source_dir.join("runtime.c"),
            "int main(void) { return 0; }\n",
        )
        .expect("source file");
        fs::write(
            manifest_dir.join("native_runtime.toml"),
            r#"
name = "test-runtime"
sources = ["src/runtime.c"]
include_dirs = ["include"]
defines = ["KAIN_TEST=1"]
windows_defines = ["KAIN_WINDOWS=1"]
linux_defines = ["KAIN_LINUX=1"]
macos_defines = ["KAIN_MACOS=1"]

[link]
windows = ["user32", "gdi32"]
linux = ["m"]
macos = ["Cocoa"]
"#,
        )
        .expect("manifest file");

        let resolved = load_native_runtime_manifest(&manifest_dir.join("native_runtime.toml"))
            .expect("resolved manifest");

        assert_eq!(resolved.name, "test-runtime");
        assert_eq!(resolved.sources.len(), 1);
        assert!(resolved.sources[0].is_absolute());
        assert!(resolved.include_dirs[0].is_absolute());
        assert!(resolved.archive_groups.is_empty());
        assert!(!resolved.uses_cpp_runtime);
        if cfg!(windows) {
            assert_eq!(
                resolved.defines,
                vec!["KAIN_TEST=1".to_string(), "KAIN_WINDOWS=1".to_string()]
            );
        } else if cfg!(target_os = "macos") {
            assert_eq!(
                resolved.defines,
                vec!["KAIN_TEST=1".to_string(), "KAIN_MACOS=1".to_string()]
            );
        } else {
            assert_eq!(
                resolved.defines,
                vec!["KAIN_TEST=1".to_string(), "KAIN_LINUX=1".to_string()]
            );
        }
        assert_eq!(
            resolved.link_libs,
            unique_link_libs({
                let mut libs = default_native_runtime_link_libs();
                libs.extend(platform_link_libs(&NativeRuntimeLinkManifest {
                    windows: vec!["user32".to_string(), "gdi32".to_string()],
                    linux: vec!["m".to_string()],
                    macos: vec!["Cocoa".to_string()],
                }));
                libs
            })
        );
    }

    #[test]
    fn runtime_source_cpp_detection_matches_known_extensions() {
        assert!(runtime_source_uses_cpp(Path::new("renderer.cpp")));
        assert!(runtime_source_uses_cpp(Path::new("renderer.cxx")));
        assert!(!runtime_source_uses_cpp(Path::new("renderer.c")));
    }

    #[test]
    fn runtime_archive_groups_claim_vendor_sources_once() {
        let source_entries = vec![
            (
                PathBuf::from("native/src/core/runtime.c"),
                PathBuf::from("/abs/native/src/core/runtime.c"),
            ),
            (
                PathBuf::from("3rdparty/bgfx/src/amalgamated.cpp"),
                PathBuf::from("/abs/3rdparty/bgfx/src/amalgamated.cpp"),
            ),
            (
                PathBuf::from("3rdparty/imgui/imgui.cpp"),
                PathBuf::from("/abs/3rdparty/imgui/imgui.cpp"),
            ),
        ];
        let archive_groups = resolve_native_runtime_archive_groups(
            &source_entries,
            &[NativeRuntimeArchiveManifest {
                name: "vendor-runtime".to_string(),
                source_prefixes: vec![PathBuf::from("3rdparty")],
            }],
        )
        .expect("archive groups");

        assert_eq!(archive_groups.len(), 1);
        assert_eq!(archive_groups[0].name, "vendor-runtime");
        assert_eq!(archive_groups[0].source_paths.len(), 2);
        assert!(archive_groups[0].uses_cpp_runtime);
    }

    #[test]
    fn runtime_archive_fingerprint_changes_with_group_name() {
        let bundle = ResolvedNativeRuntimeBundle {
            name: "test-runtime".to_string(),
            sources: vec![PathBuf::from("/abs/runtime.c")],
            include_dirs: vec![PathBuf::from("/abs/include")],
            defines: vec!["KAIN_TEST=1".to_string()],
            archive_groups: Vec::new(),
            cache_root: default_runtime_cache_root(),
            link_libs: Vec::new(),
            uses_cpp_runtime: false,
        };
        let archiver = NativeRuntimeArchiver {
            command: "llvm-ar".to_string(),
            flavor: NativeRuntimeArchiverFlavor::GnuAr,
            archive_ext: "a",
        };
        let vendor_group = ResolvedNativeRuntimeArchiveGroup {
            name: "vendor-runtime".to_string(),
            source_paths: vec![PathBuf::from("/abs/3rdparty/vendor.cpp")],
            uses_cpp_runtime: true,
        };
        let ui_group = ResolvedNativeRuntimeArchiveGroup {
            name: "ui-runtime".to_string(),
            source_paths: vec![PathBuf::from("/abs/3rdparty/vendor.cpp")],
            uses_cpp_runtime: true,
        };

        let empty_object_fingerprints = std::collections::BTreeMap::new();
        let vendor_fingerprint = build_native_runtime_archive_fingerprint(
            &bundle,
            &archiver,
            &vendor_group,
            &empty_object_fingerprints,
        );
        let ui_fingerprint = build_native_runtime_archive_fingerprint(
            &bundle,
            &archiver,
            &ui_group,
            &empty_object_fingerprints,
        );

        assert_ne!(vendor_fingerprint, ui_fingerprint);
    }

    #[test]
    fn native_runtime_depfile_parser_handles_escaped_spaces() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let depfile_path = temp_dir.path().join("runtime.o.d");
        fs::write(
            &depfile_path,
            "kain_runtime_target: /tmp/runtime\\ source.c \\\n /tmp/include/header\\ file.h /tmp/include/next.h\n",
        )
        .expect("depfile");

        let dependencies =
            parse_native_runtime_depfile(&depfile_path).expect("parsed runtime depfile");
        let cwd = std::env::current_dir().expect("cwd");
        let expected = [
            PathBuf::from("/tmp/runtime source.c"),
            PathBuf::from("/tmp/include/header file.h"),
            PathBuf::from("/tmp/include/next.h"),
        ]
        .into_iter()
        .map(|path| {
            if path.is_absolute() {
                path
            } else {
                cwd.join(path)
            }
        })
        .collect::<Vec<_>>();

        assert_eq!(dependencies, expected);
    }

    #[cfg(windows)]
    #[test]
    fn native_runtime_depfile_parser_preserves_windows_absolute_paths() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let source_path = temp_dir.path().join("runtime.c");
        let header_path = temp_dir.path().join("runtime.h");
        let depfile_path = temp_dir.path().join("runtime.obj.d");
        fs::write(
            &depfile_path,
            format!(
                "kain_runtime_target: {} {}\n",
                source_path.display(),
                header_path.display()
            ),
        )
        .expect("depfile");

        let dependencies =
            parse_native_runtime_depfile(&depfile_path).expect("parsed runtime depfile");

        assert_eq!(dependencies, vec![source_path, header_path]);
    }

    #[test]
    fn native_toolchain_tuning_defaults_to_debug_profile() {
        let tuning = parse_native_toolchain_tuning(None, None, None, None).expect("default tuning");

        assert_eq!(tuning.profile, NativeToolchainProfile::Debug);
        assert_eq!(tuning.opt_level, "0");
        assert_eq!(tuning.target_cpu, None);
        assert!(tuning.debug_info);
    }

    #[test]
    fn native_toolchain_tuning_supports_benchmark_release_profile() {
        let tuning = parse_native_toolchain_tuning(Some("benchmark-release"), None, None, None)
            .expect("benchmark-release tuning");

        assert_eq!(tuning.profile, NativeToolchainProfile::BenchmarkRelease);
        assert_eq!(tuning.opt_level, "3");
        assert_eq!(tuning.target_cpu.as_deref(), Some("native"));
        assert!(!tuning.debug_info);
    }

    #[test]
    fn native_toolchain_tuning_accepts_overrides() {
        let tuning =
            parse_native_toolchain_tuning(Some("release"), Some("3"), Some("znver4"), Some("true"))
                .expect("override tuning");

        assert_eq!(tuning.profile, NativeToolchainProfile::Release);
        assert_eq!(tuning.opt_level, "3");
        assert_eq!(tuning.target_cpu.as_deref(), Some("znver4"));
        assert!(tuning.debug_info);
    }

    #[test]
    fn native_runtime_compile_fingerprint_changes_with_toolchain_tuning() {
        let bundle = ResolvedNativeRuntimeBundle {
            name: "test-runtime".to_string(),
            sources: vec![PathBuf::from("/abs/runtime.c")],
            include_dirs: vec![PathBuf::from("/abs/include")],
            defines: vec!["KAIN_TEST=1".to_string()],
            archive_groups: Vec::new(),
            cache_root: default_runtime_cache_root(),
            link_libs: Vec::new(),
            uses_cpp_runtime: false,
        };
        let debug_tuning =
            parse_native_toolchain_tuning(Some("debug"), None, None, None).expect("debug tuning");
        let bench_tuning =
            parse_native_toolchain_tuning(Some("benchmark-release"), None, None, None)
                .expect("bench tuning");

        let debug_fingerprint = build_native_runtime_compile_fingerprint(
            &bundle,
            "clang",
            Path::new("/abs/runtime.c"),
            &debug_tuning,
        )
        .expect("debug fingerprint");
        let bench_fingerprint = build_native_runtime_compile_fingerprint(
            &bundle,
            "clang",
            Path::new("/abs/runtime.c"),
            &bench_tuning,
        )
        .expect("bench fingerprint");

        assert_ne!(debug_fingerprint, bench_fingerprint);
    }

    #[test]
    fn native_runtime_object_cache_detects_stale_dependencies() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let source_path = temp_dir.path().join("runtime.c");
        let header_path = temp_dir.path().join("runtime.h");
        fs::write(&source_path, "#include \"runtime.h\"\n").expect("source");
        fs::write(&header_path, "#define KAIN_RUNTIME 1\n").expect("header");

        let cache_paths =
            build_native_runtime_object_cache_paths(temp_dir.path(), 0, &source_path, "o");
        let bundle = ResolvedNativeRuntimeBundle {
            name: "test-runtime".to_string(),
            sources: vec![source_path.clone()],
            include_dirs: vec![temp_dir.path().to_path_buf()],
            defines: vec!["KAIN_TEST=1".to_string()],
            archive_groups: Vec::new(),
            cache_root: default_runtime_cache_root(),
            link_libs: Vec::new(),
            uses_cpp_runtime: false,
        };
        let tuning =
            parse_native_toolchain_tuning(Some("debug"), None, None, None).expect("debug tuning");
        let fingerprint =
            build_native_runtime_compile_fingerprint(&bundle, "clang", &source_path, &tuning)
                .expect("fp");

        sleep(Duration::from_millis(20));
        fs::write(&cache_paths.object_path, "object").expect("object");
        fs::write(
            &cache_paths.depfile_path,
            format!(
                "kain_runtime_target: {} {}\n",
                source_path.display(),
                header_path.display()
            ),
        )
        .expect("depfile");
        fs::write(&cache_paths.fingerprint_path, &fingerprint).expect("fingerprint");

        assert!(native_runtime_object_cache_is_fresh(
            &cache_paths,
            &fingerprint
        ));

        sleep(Duration::from_millis(20));
        fs::write(&header_path, "#define KAIN_RUNTIME 2\n").expect("updated header");

        assert!(!native_runtime_object_cache_is_fresh(
            &cache_paths,
            &fingerprint
        ));
    }

    #[test]
    fn c_ffi_native_link_inputs_follow_imported_helper_modules_from_frontend_bundle() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let src_dir = temp_dir.path().join("src");
        let native_dir = temp_dir.path().join("native");
        fs::create_dir_all(&src_dir).expect("src dir");
        fs::create_dir_all(&native_dir).expect("native dir");

        let main_path = src_dir.join("main.kn");
        let helper_path = src_dir.join("helper.kn");
        let header_path = native_dir.join("tiny.h");
        let source_path = native_dir.join("tiny.c");
        let manifest_path = temp_dir.path().join("KAIN.toml");

        fs::write(
            &manifest_path,
            r#"
[package]
name = "cffi-helper-smoke"
version = "0.1.0"

[blade]
name = "cffi-helper-smoke"
kind = "kain_app"
entry = "src/main.kn"
source_roots = ["src"]
module_roots = ["src"]

[c_ffi]

[[c_ffi.libraries]]
name = "tiny"
tier = "inline"
header = "native/tiny.h"
sources = ["native/tiny.c"]
include_paths = ["native"]
"#,
        )
        .expect("manifest");
        fs::write(&header_path, "int tiny_add(int value);\n").expect("header");
        fs::write(
            &source_path,
            "int tiny_add(int value) { return value + 1; }\n",
        )
        .expect("native source");
        fs::write(
            &main_path,
            r#"
use helper::helper_lane

fn main() -> Int:
    return helper_lane()
"#,
        )
        .expect("main");
        fs::write(
            &helper_path,
            r#"
use c::tiny

pub fn helper_lane() -> Int:
    return tiny_add(41)
"#,
        )
        .expect("helper");

        let source = fs::read_to_string(&main_path).expect("read main");
        let previous_dir = std::env::current_dir().expect("cwd");
        let session = DriverSession::new();
        let frontend_source = (|| {
            std::env::set_current_dir(temp_dir.path()).expect("set cwd");
            session
                .compile_with_source_path(&source, Some(main_path.as_path()), CompileTarget::Llvm)
                .expect("compile main");
            session
                .frontend_full_source()
                .expect("frontend bundle source should exist")
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        assert!(frontend_source.contains("use c::tiny"));

        let clang_cmd = kain_core::install_layout::find_clang()
            .map(|p| p.to_string_lossy().to_string())
            .unwrap_or_else(|| "clang".to_string());
        let tuning =
            parse_native_toolchain_tuning(Some("debug"), None, None, None).expect("debug tuning");
        let link_inputs = resolve_c_ffi_native_link_inputs(
            &frontend_source,
            Some(main_path.as_path()),
            &clang_cmd,
            &tuning,
        )
        .expect("helper-import c ffi link inputs");

        assert_eq!(link_inputs.link_inputs.len(), 1);
        assert_eq!(
            link_inputs.link_inputs[0]
                .extension()
                .and_then(|value| value.to_str()),
            Some("bc")
        );
        assert!(link_inputs.link_inputs[0].exists());
    }

    #[test]
    fn c_ffi_native_link_inputs_preserve_system_include_specs_from_frontend_bundle() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let src_dir = temp_dir.path().join("src");
        fs::create_dir_all(&src_dir).expect("src dir");

        let main_path = src_dir.join("main.kn");
        fs::write(
            &main_path,
            r#"
use c::math
include <math.h> as cmath

fn main() -> Int:
    return 0
"#,
        )
        .expect("main");

        let source = fs::read_to_string(&main_path).expect("read main");
        let clang_cmd = kain_core::install_layout::find_clang()
            .map(|p| p.to_string_lossy().to_string())
            .unwrap_or_else(|| "clang".to_string());
        let tuning =
            parse_native_toolchain_tuning(Some("debug"), None, None, None).expect("debug tuning");
        let link_inputs = resolve_c_ffi_native_link_inputs(
            &source,
            Some(main_path.as_path()),
            &clang_cmd,
            &tuning,
        )
        .expect("system include c ffi link inputs");

        assert!(link_inputs.link_inputs.is_empty());
        if !cfg!(windows) {
            assert!(!link_inputs.link_libs.is_empty());
        }
    }

    #[test]
    fn c_ffi_native_link_inputs_include_project_declared_libraries_for_project_entry() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let src_dir = temp_dir.path().join("src");
        let native_dir = temp_dir.path().join("native");
        fs::create_dir_all(&src_dir).expect("src dir");
        fs::create_dir_all(&native_dir).expect("native dir");

        let main_path = src_dir.join("main.kn");
        let helper_path = src_dir.join("helper.kn");
        let header_path = native_dir.join("tiny.h");
        let source_path = native_dir.join("tiny.c");
        let manifest_path = temp_dir.path().join("KAIN.toml");

        fs::write(
            &manifest_path,
            r#"
[package]
name = "cffi-project-entry"
version = "0.1.0"

[blade]
name = "cffi-project-entry"
kind = "kain_app"
entry = "src/main.kn"
source_roots = ["src"]
module_roots = ["src"]

[c_ffi]

[[c_ffi.libraries]]
name = "tiny"
tier = "inline"
header = "native/tiny.h"
sources = ["native/tiny.c"]
include_paths = ["native"]
"#,
        )
        .expect("manifest");
        fs::write(&header_path, "int tiny_add(int value);\n").expect("header");
        fs::write(
            &source_path,
            "int tiny_add(int value) { return value + 1; }\n",
        )
        .expect("native source");
        fs::write(
            &main_path,
            r#"
use helper::helper_lane

fn main() -> Int:
    return helper_lane()
"#,
        )
        .expect("main");
        fs::write(
            &helper_path,
            r#"
include "../native/tiny.h" as tiny

pub fn helper_lane() -> Int:
    return tiny_add(41)
"#,
        )
        .expect("helper");

        let source = fs::read_to_string(&main_path).expect("read main");
        let clang_cmd = kain_core::install_layout::find_clang()
            .map(|p| p.to_string_lossy().to_string())
            .unwrap_or_else(|| "clang".to_string());
        let tuning =
            parse_native_toolchain_tuning(Some("debug"), None, None, None).expect("debug tuning");
        let link_inputs = resolve_c_ffi_native_link_inputs(
            &source,
            Some(main_path.as_path()),
            &clang_cmd,
            &tuning,
        )
        .expect("project-declared c ffi link inputs");

        assert_eq!(link_inputs.link_inputs.len(), 1);
        assert_eq!(
            link_inputs.link_inputs[0]
                .extension()
                .and_then(|value| value.to_str()),
            Some("bc")
        );
        assert!(link_inputs.link_inputs[0].exists());
    }
}


fn find_binary(name: &str, fallback: Option<&str>) -> Option<PathBuf> {
    if std::process::Command::new(name)
        .arg("--version")
        .output()
        .is_ok()
    {
        return Some(PathBuf::from(name));
    }
    if let Some(f) = fallback {
        let pb = PathBuf::from(f);
        if pb.exists() {
            return Some(pb);
        }
    }
    None
}

fn derive_shader_paths(input: &PathBuf) -> (PathBuf, PathBuf, PathBuf) {
    let stage = staging_dir();
    let stem = input
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("shader");
    let spv = stage.join(format!("{}.spv", stem));
    let hlsl = stage.join(format!("{}.hlsl", stem));
    let usf = stage.join(format!("{}.usf", stem));
    (spv, hlsl, usf)
}

fn resolve_plugin_dir(plugin: &str, base_opt: &Option<PathBuf>) -> PathBuf {
    if let Some(base) = base_opt {
        return base.join(plugin).join("Shaders");
    }
    let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    let candidate1 = cwd.join("src-plugins");
    if candidate1.exists() {
        return candidate1.join(plugin).join("Shaders");
    }
    if let Some(parent) = cwd.parent() {
        let candidate2 = parent.join("src-plugins");
        if candidate2.exists() {
            return candidate2.join(plugin).join("Shaders");
        }
    }
    cwd.join("src-plugins").join(plugin).join("Shaders")
}

fn run_ue5_shader_pipeline(input: &PathBuf, args: &Args) -> bool {
    let (spv_path, hlsl_path, usf_path) = derive_shader_paths(input);
    let stage_dir = staging_dir();
    if !ensure_dir(&stage_dir) {
        return false;
    }

    let source = match fs::read_to_string(input) {
        Ok(s) => s,
        Err(e) => {
            eprintln!(" Failed to read {}: {}", input.display(), e);
            return false;
        }
    };

    if args.verbose {
        println!(" Compiling: {}", input.display());
    }

    let compiled_spv = match cli::compile_spirv_binary(&source) {
        Ok(bytes) => bytes,
        Err(e) => {
            let filename = input
                .file_name()
                .and_then(|s| s.to_str())
                .unwrap_or("input.kn");
            let diag = kain_core::diagnostics::Diagnostics::new(&source, filename);
            eprint!("{}", diag.format_error(&e));
            return false;
        }
    };

    if args.dry_run {
        println!("→ Write SPIR-V {}", spv_path.display());
    } else if let Err(e) = fs::write(&spv_path, &compiled_spv) {
        eprintln!(" Failed to write {}: {}", spv_path.display(), e);
        return false;
    } else {
        if args.verbose {
            println!(" {}", spv_path.display());
        }
    }

    if let Some(val_bin) = find_binary("spirv-val", None) {
        if args.verbose {
            println!(" Validating SPIR-V");
        }
        if !args.dry_run {
            let status = std::process::Command::new(val_bin).arg(&spv_path).status();
            if let Ok(s) = status {
                if !s.success() {
                    eprintln!(" SPIR-V validation failed");
                    return false;
                }
            }
        }
    }

    let naga_bin = match find_binary("naga", None) {
        Some(p) => p,
        None => {
            eprintln!(" 'naga' not found. Install with: cargo install naga-cli");
            return false;
        }
    };

    if args.verbose {
        println!(" Transpiling to HLSL");
    }
    if args.dry_run {
        println!("→ Run naga {} {}", spv_path.display(), hlsl_path.display());
    } else {
        let status = std::process::Command::new(naga_bin)
            .arg(&spv_path)
            .arg(&hlsl_path)
            .status();
        match status {
            Ok(s) if s.success() => {
                if args.verbose {
                    println!(" {}", hlsl_path.display());
                }
            }
            _ => {
                eprintln!(" Naga transpilation failed");
                return false;
            }
        }
    }

    if args.dry_run {
        println!("→ Write USF {}", usf_path.display());
    } else {
        let stem = input
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("shader");
        let content = format!("#include \"{}.hlsl\"\n", stem);
        if let Err(e) = fs::write(&usf_path, content) {
            eprintln!(" Failed to write {}: {}", usf_path.display(), e);
            return false;
        }
    }

    if let Some(plugin) = &args.plugin {
        let target_dir = resolve_plugin_dir(plugin, &args.plugins_dir);
        if args.dry_run {
            println!("→ Copy to {}", target_dir.display());
        } else {
            if !ensure_dir(&target_dir.clone()) {
                return false;
            }
            let hlsl_target = target_dir.join(hlsl_path.file_name().unwrap());
            let usf_target = target_dir.join(usf_path.file_name().unwrap());
            if let Err(e) = fs::copy(&hlsl_path, &hlsl_target) {
                eprintln!(" Copy failed: {}", e);
                return false;
            }
            if let Err(e) = fs::copy(&usf_path, &usf_target) {
                eprintln!(" Copy failed: {}", e);
                return false;
            }
            println!(" {}", target_dir.display());
        }
    } else {
        if args.verbose {
            println!(" Staged in {}", stage_dir.display());
        }
    }

    true
}
