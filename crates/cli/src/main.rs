// KAIN Compiler CLI

use clap::{CommandFactory, FromArgMatches, Parser as ClapParser};
use cli::blades;
use cli::codebase;
use cli::fabric;
use cli::import_asm;
use cli::import_c;
use cli::import_crate;
use cli::import_rust;
use cli::import_typescript;
use cli::llvm_native_stage;
use cli::lsp;
use cli::native_ui_build;
use cli::native_ui_dev;
use cli::omni;
use cli::packager;
use cli::repair::{self, DoctorRepairArgs};
use cli::rust_build;
use cli::selfhost;
use cli::{
    compile, detect_launcher_from_path, format_source, parse_compile_target, render_launcher_menu,
    resolve_legacy_target_alias, should_show_launcher_menu, supported_targets_csv,
    target_extension, CompileTarget, LauncherKind, BUILD_GIT_COMMIT_COUNT, BUILD_GIT_DIRTY,
    BUILD_GIT_SHA, BUILD_HOST_TRIPLE, BUILD_NUMBER, BUILD_PROFILE, BUILD_TARGET_TRIPLE,
    BUILD_UNIX_TIME, LANGUAGE_NAME, VERSION,
};
use kain_c_ffi::{
    ArtifactMode as CArtifactMode, ImportCOptions as CImportCOptions,
    PrepareContext as CPrepareContext,
};
use kain_crate_ffi::{ArtifactMode, ImportCrateOptions};
use kain_repl::{
    normalize_script_source, run_terminal_repl, ReplBuildMetadata, ReplTerminalConfig,
};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::io::{self, IsTerminal, Read};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

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

#[derive(ClapParser, Debug)]
#[command(name = "kain")]
#[command(author = "Kipp")]
#[command(version = VERSION)]
#[command(about = "The Ultimate Programming Language Compiler", long_about = None)]
struct Args {
    #[command(subcommand)]
    command: Option<Commands>,

    /// Source file to compile (legacy positional argument)
    input: Option<PathBuf>,

    /// Inline Kain source, similar to `python -c`
    #[arg(short = 'c', long, conflicts_with = "input")]
    code: Option<String>,

    /// Output file
    #[arg(short, long)]
    output: Option<PathBuf>,

    /// Compilation target
    #[arg(short, long, default_value = "wasm")]
    target: String,

    /// Run immediately after compilation
    #[arg(short, long)]
    run: bool,

    /// Watch for file changes and recompile
    #[arg(short, long)]
    watch: bool,

    /// Emit AST for debugging  
    #[arg(long)]
    emit_ast: bool,

    /// Emit typed AST
    #[arg(long)]
    emit_typed: bool,

    /// Verbose output
    #[arg(short, long)]
    verbose: bool,

    /// Target plugin name for UE5 shader copy
    #[arg(long)]
    plugin: Option<String>,

    /// Base plugins directory (defaults to u:\ue_factory\src-plugins)
    #[arg(long)]
    plugins_dir: Option<PathBuf>,

    /// Print planned actions without executing
    #[arg(long)]
    dry_run: bool,

    /// Treat transpiler warnings as errors when supported
    #[arg(long)]
    strict: bool,

    /// Analyze shader complexity (USF target only)
    #[arg(long)]
    analyze: bool,
}

#[derive(clap::Subcommand, Debug)]
enum BuildCommand {
    /// Build a standalone native UI app and optionally a desktop executable
    #[command(name = "native-ui")]
    NativeUi {
        /// Input Kain UI source file
        input: PathBuf,

        /// Root component override
        #[arg(long = "root")]
        root_component: Option<String>,

        /// Native app name / Cargo package name
        #[arg(long)]
        app_name: Option<String>,

        /// Window title for the native host
        #[arg(long)]
        window_title: Option<String>,

        /// Materialized project output directory
        #[arg(short = 'o', long = "out")]
        project_dir: Option<PathBuf>,

        /// Relative or absolute artifact directory inside the materialized project
        #[arg(long)]
        artifact_dir: Option<PathBuf>,

        /// Generate the native app project but skip cargo build
        #[arg(long)]
        bundle_only: bool,

        /// Build the generated app in release mode
        #[arg(long)]
        release: bool,

        /// Override the native runtime crate name
        #[arg(long, default_value = "kain-ui-native")]
        runtime_crate: String,

        /// Use an explicit path dependency for the native runtime crate
        #[arg(long, conflicts_with = "runtime_version")]
        runtime_path: Option<PathBuf>,

        /// Use a published version dependency for the native runtime crate
        #[arg(long, conflicts_with = "runtime_path")]
        runtime_version: Option<String>,

        /// Native desktop host backend
        #[arg(long, default_value = "qt")]
        host: String,

        /// Tauri bundle identifier override
        #[arg(long = "tauri-bundle-id")]
        tauri_bundle_id: Option<String>,

        /// Tauri window label override
        #[arg(long = "tauri-window-label")]
        tauri_window_label: Option<String>,
    },
}

#[derive(clap::Subcommand, Debug)]
enum NativeUiCommand {
    /// Launch a native desktop Kain app with watch + hot reload
    Dev {
        /// Input Kain UI source file
        input: PathBuf,

        /// Root component override
        #[arg(long = "root")]
        root_component: Option<String>,

        /// Native app name / Cargo package name
        #[arg(long)]
        app_name: Option<String>,

        /// Window title for the native host
        #[arg(long)]
        window_title: Option<String>,

        /// Materialized project output directory
        #[arg(long = "project-dir")]
        project_dir: Option<PathBuf>,

        /// Relative or absolute artifact directory inside the materialized project
        #[arg(long = "artifact-dir")]
        artifact_dir: Option<PathBuf>,

        /// Build the generated app in release mode
        #[arg(long)]
        release: bool,

        /// Native desktop host backend
        #[arg(long, default_value = "qt")]
        host: String,

        /// Tauri bundle identifier override
        #[arg(long = "tauri-bundle-id")]
        tauri_bundle_id: Option<String>,

        /// Tauri window label override
        #[arg(long = "tauri-window-label")]
        tauri_window_label: Option<String>,
    },
}

#[derive(clap::Subcommand, Debug)]
enum BridgeCommand {
    /// Run a resident JSON-lines Kain bridge process.
    Serve {
        /// Entry .kn file that defines the dispatch function.
        #[arg(long)]
        entry: PathBuf,

        /// Function called for each request. It receives one JSON string.
        #[arg(long, default_value = "kain_bridge_dispatch")]
        dispatch_function: String,
    },
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

#[derive(clap::Subcommand, Debug)]
enum Commands {
    /// Initialize a new KAIN project
    Init {
        /// Project name
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Explicit project name
        #[arg(long)]
        name: Option<String>,
    },

    /// Start the Language Server
    Lsp,

    /// Show binary/build diagnostics and resolved compiler capabilities
    Doctor {
        #[command(flatten)]
        repair: DoctorRepairArgs,
    },

    /// Format Kain source using the compiler-owned canonical printer
    #[command(visible_alias = "fmt")]
    Format {
        /// Input Kain source file. Use '-' to read from stdin.
        input: Option<PathBuf>,

        /// Check whether the source is already formatted
        #[arg(long, conflicts_with = "write")]
        check: bool,

        /// Rewrite the input file in place
        #[arg(short = 'w', long, conflicts_with = "check")]
        write: bool,
    },

    /// Check Kain source without emitting backend artifacts
    Check {
        /// Input Kain source file or directory. Use '-' to read from stdin.
        input: PathBuf,

        /// Target profile to typecheck against
        #[arg(short, long, default_value = "run")]
        target: String,

        /// Stop after the first failed file
        #[arg(long)]
        fail_fast: bool,

        /// Write a structured JSON check report
        #[arg(long)]
        json: Option<PathBuf>,
    },

    /// Run Kain source tests using Rust-style pass/fail directives
    Test {
        /// Input Kain source file or directory
        input: PathBuf,

        /// Override test mode: check-pass, check-fail, run-pass, run-fail, kain-test
        #[arg(long)]
        mode: Option<String>,

        /// Default target profile for check modes
        #[arg(short, long, default_value = "run")]
        target: String,

        /// Stop after the first failed case
        #[arg(long)]
        fail_fast: bool,

        /// Run cases marked with //@ ignore instead of skipping them
        #[arg(long)]
        ignored: bool,

        /// Write a structured JSON test report
        #[arg(long)]
        json: Option<PathBuf>,
    },

    /// Run self-host bootstrap workflows
    Selfhost {
        #[command(subcommand)]
        command: selfhost::SelfHostCommand,
    },

    /// Build mixed-language omni manifests through the dedicated orchestration layer
    Omni {
        #[command(subcommand)]
        command: omni::OmniCommand,
    },

    /// Validate and scaffold local-first Fabric manifests for polyglot execution
    Fabric {
        #[command(subcommand)]
        command: fabric::FabricCommand,
    },

    /// Resolve and inspect local Kain blade workspaces
    Blades {
        #[command(subcommand)]
        command: blades::BladesCommand,
    },

    /// Equip a local blade by name and print its resolved build/import plan
    Equip {
        /// Blade name to resolve
        blade: String,

        /// Path inside the workspace to inspect
        #[arg(long, default_value = ".")]
        path: PathBuf,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Build project or file. Without input, reads KAIN.toml for multi-target build.
    Build {
        #[command(subcommand)]
        command: Option<BuildCommand>,

        /// Optional input file. If omitted, builds all targets from KAIN.toml
        input: Option<PathBuf>,

        /// Output file path
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Single target override for file builds (e.g. ts, rust, wasm)
        #[arg(short, long)]
        target: Option<String>,

        /// Override targets (comma-separated: wasm,js,rust)
        #[arg(long, value_delimiter = ',')]
        targets: Option<Vec<String>>,

        /// Build UE5 plugin from KAIN.toml [ue5] config
        #[arg(long)]
        ue5: bool,

        #[arg(long)]
        r#rust: bool,

        /// Embed original KAIN source as comments in generated C++ (debugging/round-trip)
        #[arg(long)]
        embed: bool,
    },

    /// Native desktop app workflows
    #[command(name = "native-ui")]
    NativeUi {
        #[command(subcommand)]
        command: NativeUiCommand,
    },

    /// Resident Kain host bridge workflows
    Bridge {
        #[command(subcommand)]
        command: BridgeCommand,
    },

    /// Trusted local codebase control and package/runtime operators
    Codebase {
        #[command(subcommand)]
        command: codebase::CodebaseCommand,
    },

    /// Start the interactive Kain REPL
    Repl,

    /// Run a file (explicit command)
    Run { input: PathBuf },

    /// Generate paired GPU artifacts (SPIR-V, Rust host wrappers, reflection JSON)
    GpuArtifacts {
        input: PathBuf,

        /// Output base path for generated GPU artifacts
        #[arg(short, long)]
        output: Option<PathBuf>,
    },

    /// Inject KAIN file into existing plugin (non-destructive)
    Inject {
        /// Input .kn file(s)
        inputs: Vec<PathBuf>,

        /// Target plugin directory (auto-detected if omitted)
        #[arg(long)]
        plugin_dir: Option<PathBuf>,

        /// Plugin name (auto-detected if omitted)
        #[arg(long)]
        plugin: Option<String>,

        /// Force overwrite existing files
        #[arg(long)]
        force: bool,

        /// Dry run (show what would be generated)
        #[arg(long)]
        dry_run: bool,

        /// Use UE5 codegen
        #[arg(long)]
        ue5: bool,
    },

    /// Import legacy assembly and transliterate into KAIN firmware scaffolding
    ImportAsm {
        /// Input assembly source file
        input: PathBuf,

        /// Input dialect format
        #[arg(long, default_value = "6502-furby")]
        format: String,

        /// Output .kn file (default: Kain/generated/furby_firmware.kn)
        #[arg(long)]
        out: Option<PathBuf>,

        /// Parse/canonicalize and report only, without writing generated .kn and map
        #[arg(long)]
        validate_only: bool,
    },

    /// Import C source code into KAIN
    ImportC {
        /// Input C source file or directory
        input: PathBuf,

        /// Output .kn file (optional - if omitted, only compiles if --target specified)
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Compilation target (optional - compile directly without writing .kn)
        #[arg(short, long)]
        target: Option<String>,

        /// Include paths for C preprocessor (-I flags)
        #[arg(short = 'I', long)]
        include_paths: Vec<String>,

        /// Preprocessor defines (-D flags)
        #[arg(short = 'D', long)]
        defines: Vec<String>,

        /// Flatten all imported symbols into one global scope (disables per-file modules)
        #[arg(long)]
        flat: bool,

        /// Include only files whose relative path contains one of these filters
        #[arg(long = "include", value_delimiter = ',')]
        include_filters: Vec<String>,

        /// Exclude files whose relative path contains one of these filters
        #[arg(long = "exclude", value_delimiter = ',')]
        exclude_filters: Vec<String>,

        /// Stop on first failed file import (default: continue and report failures)
        #[arg(long)]
        fail_fast: bool,

        /// Write import failure/report JSON (defaults automatically for directory imports with failures)
        #[arg(long)]
        report_json: Option<PathBuf>,
    },

    /// Import Rust source code into KAIN (Project Ouroboros)
    ImportRust {
        /// Input Rust source file or directory
        input: PathBuf,

        /// Output .kn file (optional - if omitted, only compiles if --target specified)
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Compilation target (optional - compile directly without writing .kn)
        #[arg(short, long)]
        target: Option<String>,

        /// Flatten all imported symbols into one global scope (disables per-file modules)
        #[arg(long)]
        flat: bool,

        /// Include only files whose relative path contains one of these filters
        #[arg(long = "include", value_delimiter = ',')]
        include_filters: Vec<String>,

        /// Exclude files whose relative path contains one of these filters
        #[arg(long = "exclude", value_delimiter = ',')]
        exclude_filters: Vec<String>,

        /// Stop on first failed file import (default: continue and report failures)
        #[arg(long)]
        fail_fast: bool,

        /// Write import failure/report JSON (defaults automatically for directory imports with failures)
        #[arg(long)]
        report_json: Option<PathBuf>,
    },

    /// Import a Rust crate into KAIN through the crate FFI layer
    ImportCrate {
        /// Crate import name used by `use rust::<crate_name>`
        crate_name: String,

        /// Cargo manifest used for workspace/dependency resolution
        #[arg(long)]
        manifest_path: Option<PathBuf>,

        /// Explicit local crate folder or Cargo.toml for standalone crates
        #[arg(long)]
        crate_path: Option<PathBuf>,

        /// Generated artifact mode: live, generate, or both
        #[arg(long, default_value = "both")]
        mode: String,

        /// Output directory for generated KAIN files and reports
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Override report JSON path
        #[arg(long)]
        report_json: Option<PathBuf>,

        /// Cargo feature list for the resolved crate
        #[arg(long, value_delimiter = ',')]
        features: Vec<String>,

        /// Enable all crate features
        #[arg(long)]
        all_features: bool,

        /// Disable default crate features
        #[arg(long)]
        no_default_features: bool,
    },

    /// Import TypeScript source code into KAIN
    ImportTs {
        /// Input TypeScript source file or directory
        input: PathBuf,

        /// Output .kn file (optional - if omitted, only compiles if --target specified)
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Compilation target (optional - compile directly without writing .kn)
        #[arg(short, long)]
        target: Option<String>,

        /// Flatten all imported symbols into one global scope (disables per-file modules)
        #[arg(long)]
        flat: bool,

        /// Include only files whose relative path contains one of these filters
        #[arg(long = "include", value_delimiter = ',')]
        include_filters: Vec<String>,

        /// Exclude files whose relative path contains one of these filters
        #[arg(long = "exclude", value_delimiter = ',')]
        exclude_filters: Vec<String>,

        /// Stop on first failed file import (default: continue and report failures)
        #[arg(long)]
        fail_fast: bool,

        /// Write import failure/report JSON (defaults automatically for directory imports with failures)
        #[arg(long)]
        report_json: Option<PathBuf>,
    },
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

fn read_format_source(
    input: Option<&PathBuf>,
) -> Result<(String, Option<PathBuf>, String), String> {
    match input {
        Some(path) => {
            let source = read_source_text(path)?;
            let source_name = if path == Path::new("-") {
                "<stdin>".to_string()
            } else {
                path.display().to_string()
            };
            let source_path = if path == Path::new("-") {
                None
            } else {
                Some(path.clone())
            };
            Ok((source, source_path, source_name))
        }
        None => {
            if io::stdin().is_terminal() {
                return Err("Format requires an input file path or piped stdin.".to_string());
            }
            let mut buffer = String::new();
            io::stdin()
                .read_to_string(&mut buffer)
                .map_err(|err| format!("Failed to read stdin: {err}"))?;
            Ok((buffer, None, "<stdin>".to_string()))
        }
    }
}

fn run_format_command(input: Option<PathBuf>, check: bool, write: bool) -> bool {
    if write {
        let Some(path) = input.as_ref() else {
            eprintln!(" Format --write requires an input file path.");
            return false;
        };
        if path == Path::new("-") {
            eprintln!(" Format --write does not support stdin.");
            return false;
        }
    }

    let (source, source_path, source_name) = match read_format_source(input.as_ref()) {
        Ok(value) => value,
        Err(err) => {
            eprintln!(" {}", err);
            return false;
        }
    };

    let formatted = match format_source(&source) {
        Ok(value) => value,
        Err(err) => {
            let diag = kain_core::diagnostics::Diagnostics::new(&source, &source_name);
            eprint!("{}", diag.format_error(&err));
            return false;
        }
    };

    let changed = formatted != source;

    if check {
        if changed {
            eprintln!(" Formatting changes required: {}", source_name);
            return false;
        }
        return true;
    }

    if write {
        let Some(path) = source_path else {
            eprintln!(" Format --write requires an input file path.");
            return false;
        };
        if changed {
            if !ensure_parent_dir(&path) {
                return false;
            }
            if let Err(err) = fs::write(&path, &formatted) {
                eprintln!(" Failed to write {}: {}", path.display(), err);
                return false;
            }
        }
        println!(" Formatted {}", path.display());
        return true;
    }

    print!("{formatted}");
    true
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
    _emit_ast: bool,
    _emit_typed: bool,
    verbose: bool,
    _analyze: bool,
    plugin_name: Option<&str>,
) -> bool {
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
        match cli::compile_spirv_binary(&source) {
            Ok(spv_bytes) => {
                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if let Err(e) = fs::write(&output_path, &spv_bytes) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }
                println!(
                    " Compiled to: {} ({} bytes)",
                    output_path.display(),
                    spv_bytes.len()
                );
                return true;
            }
            Err(e) => {
                eprintln!(" Compile error: {}", e);
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
        match cli::compile_wasm_binary(&source) {
            Ok(wasm_bytes) => {
                if !ensure_parent_dir(&output_path) {
                    return false;
                }
                if let Err(e) = fs::write(&output_path, &wasm_bytes) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }
                println!(
                    " Compiled to: {} ({} bytes)",
                    output_path.display(),
                    wasm_bytes.len()
                );
                return true;
            }
            Err(e) => {
                eprintln!(" Compile error: {}", e);
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
        match cli::compile_hybrid_artifacts(&source) {
            Ok(artifacts) => match write_hybrid_bundle(&descriptor_path, artifacts) {
                Ok(written) => {
                    println!(" Compiled to: {}", written.descriptor_path.display());
                    println!(" Hybrid JS: {}", written.js_path.display());
                    println!(" Hybrid TS: {}", written.ts_path.display());
                    println!(" Hybrid WASM: {}", written.wasm_path.display());
                    return true;
                }
                Err(err) => {
                    eprintln!(" Failed to materialize hybrid bundle: {}", err);
                    return false;
                }
            },
            Err(e) => {
                eprintln!(" Compile error: {}", e);
                return false;
            }
        }
    }

    // Compile
    match compile(&source, target) {
        Ok(compiled_output) => {
            if target == CompileTarget::Interpret || target == CompileTarget::Test {
                let trimmed_output = compiled_output.trim();
                if !trimmed_output.is_empty() && trimmed_output != "()" {
                    println!("{}", compiled_output);
                }
                println!(" Execution complete");
            } else {
                let default_ext = target_extension(target);

                // Determine where to write the primary output
                let output_path = if matches!(target, CompileTarget::Llvm | CompileTarget::C) {
                    // For raw-native backends, always write the backend source artifact first.
                    if let Some(out) = output {
                        if out
                            .extension()
                            .map_or(false, |e| e == target_extension(target))
                        {
                            out.clone()
                        } else {
                            let mut p = out.clone();
                            p.set_extension(target_extension(target));
                            p
                        }
                    } else if let Some(path) = source_path {
                        path.with_extension(target_extension(target))
                    } else {
                        eprintln!(
                            " Output path is required when compiling inline or stdin source."
                        );
                        return false;
                    }
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
                if let Err(e) = fs::write(&output_path, &compiled_output) {
                    eprintln!(" Failed to write output: {}", e);
                    return false;
                }

                println!(
                    " Compiled to: {} ({} bytes)",
                    output_path.display(),
                    compiled_output.len()
                );

                let mut native_artifacts_require_gpu_runtime = false;

                if matches!(target, CompileTarget::Llvm | CompileTarget::C) {
                    match llvm_native_stage::stage_native_backend_artifacts(
                        &source,
                        target,
                        &output_path,
                        None,
                    ) {
                        Ok(staged) => {
                            native_artifacts_require_gpu_runtime =
                                staged.requires_gpu_runtime_dll();
                            println!(
                                " Runtime contract: {}",
                                staged.runtime_contract_path.display()
                            );
                            println!(" Realtime bundle: {}", staged.realtime_app_path.display());
                            if let Some(compute_residency_path) = staged.compute_residency_path {
                                println!(
                                    " Compute residency: {}",
                                    compute_residency_path.display()
                                );
                            }
                            if let Some(shader_bundle_path) = staged.shader_bundle_path {
                                println!(" Shader bundle: {}", shader_bundle_path.display());
                            }
                        }
                        Err(err) => {
                            eprintln!(" Failed to stage native backend artifacts: {}", err);
                            return false;
                        }
                    }
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
                if matches!(target, CompileTarget::Llvm | CompileTarget::C) {
                    let exe_path = if let Some(out) = output {
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
                    } else {
                        let Some(path) = source_path else {
                            eprintln!(
                                " Output path is required when compiling inline or stdin source."
                            );
                            return false;
                        };
                        if cfg!(windows) {
                            path.with_extension("exe")
                        } else {
                            path.with_extension("")
                        }
                    };

                    println!(" Linking executable...");

                    // Find clang: bundled toolchain > PATH > system install
                    let clang_cmd = find_bundled_clang()
                        .or_else(|| {
                            // Try PATH
                            if std::process::Command::new("clang")
                                .arg("--version")
                                .output()
                                .is_ok()
                            {
                                Some("clang".to_string())
                            } else {
                                None
                            }
                        })
                        .or_else(|| {
                            // Try standard Windows install
                            let default_path = r"C:\Program Files\LLVM\bin\clang.exe";
                            if std::path::Path::new(default_path).exists() {
                                Some(default_path.to_string())
                            } else {
                                None
                            }
                        })
                        .unwrap_or_else(|| "clang".to_string());

                    let mut cmd = std::process::Command::new(&clang_cmd);
                    let mut runtime_link_libs = Vec::new();
                    let mut runtime_artifacts = NativeRuntimeCompiledArtifacts::default();
                    let cffi_link_inputs = match resolve_c_ffi_shared_libraries_for_linking(&source)
                    {
                        Ok(paths) => paths,
                        Err(err) => {
                            eprintln!(" Failed to resolve C FFI link inputs: {}", err);
                            return false;
                        }
                    };

                    if let Some(runtime_bundle) = match resolve_native_runtime_bundle() {
                        Ok(bundle) => bundle,
                        Err(err) => {
                            eprintln!(" Failed to resolve native runtime bundle: {}", err);
                            return false;
                        }
                    } {
                        runtime_artifacts =
                            match compile_native_runtime_bundle(&runtime_bundle, &clang_cmd) {
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

                    if target == CompileTarget::C {
                        cmd.arg("-std=c11");
                    }

                    cmd.arg(&output_path);

                    for object in runtime_artifacts.loose_objects {
                        cmd.arg(object);
                    }
                    for archive in runtime_artifacts.static_archives {
                        cmd.arg(archive);
                    }

                    cmd.arg("-o").arg(&exe_path).arg("-g");
                    if target == CompileTarget::Llvm {
                        cmd.arg("-Wno-override-module");
                    }

                    for shared_library in cffi_link_inputs {
                        cmd.arg(shared_library);
                    }

                    runtime_link_libs = unique_link_libs(
                        [runtime_link_libs, default_native_runtime_link_libs()].concat(),
                    );

                    for link_lib in runtime_link_libs {
                        cmd.arg(format!("-l{}", link_lib));
                    }

                    let status = cmd.status();

                    match status {
                        Ok(s) if s.success() => {
                            println!(" Generated executable: {}", exe_path.display());
                            if native_artifacts_require_gpu_runtime {
                                match llvm_native_stage::stage_gpu_runtime_dll(&exe_path) {
                                    Ok(Some(dll_path)) => {
                                        println!(" Compute runtime DLL: {}", dll_path.display());
                                    }
                                    Ok(None) => {}
                                    Err(err) => {
                                        eprintln!(" Failed to stage compute runtime DLL: {}", err);
                                        return false;
                                    }
                                }
                            }
                        }
                        Ok(_) => {
                            eprintln!(" Linking failed.");
                            return false;
                        }
                        Err(_) => {
                            eprintln!(" 'clang' not found in PATH or standard locations.");
                            eprintln!("   To generate an executable, install LLVM and run:");
                            eprintln!(
                                "   clang {} -o {}",
                                output_path.display(),
                                exe_path.display()
                            );
                            return false;
                        }
                    }
                }
            }
            true
        }
        Err(e) => {
            // Use pretty error formatting
            let diag = kain_core::diagnostics::Diagnostics::new(&source, source_name);
            eprint!("{}", diag.format_error(&e));
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
    run_source(
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

fn run_kn_repl() -> bool {
    run_terminal_repl(ReplTerminalConfig::new(ReplBuildMetadata::new(
        "Kain",
        VERSION,
        BUILD_NUMBER,
        BUILD_TARGET_TRIPLE,
    )))
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

fn run_check_command(input: &Path, target: &str, fail_fast: bool, json: Option<&Path>) -> bool {
    let Some(target) = parse_compile_target(target) else {
        eprintln!(" Unknown check target. Use: {}", supported_targets_csv());
        return false;
    };
    let mut options = kain_check::CheckOptions::new(target);
    options.fail_fast = fail_fast;

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
                }],
            },
        }
    } else {
        kain_check::check_path(input, &options)
    };
    if let Some(path) = json {
        if !write_structured_report(path, &report, "check") {
            return false;
        }
    }

    println!(
        " Check {}: {}/{} passed",
        if report.is_success() {
            "passed"
        } else {
            "failed"
        },
        report.passed,
        report.total
    );
    for file in report.files.iter().filter(|file| !file.passed()) {
        if let Some(error) = &file.error {
            eprintln!("  {}: {}", file.path, error);
        }
    }
    report.is_success()
}

fn run_test_command(
    input: &Path,
    mode: Option<&str>,
    target: &str,
    fail_fast: bool,
    ignored: bool,
    json: Option<&Path>,
) -> bool {
    let Some(default_target) = parse_compile_target(target) else {
        eprintln!(" Unknown test target. Use: {}", supported_targets_csv());
        return false;
    };
    let mode_override = match mode {
        Some(value) => match kain_test::KainTestMode::parse(value) {
            Some(mode) => Some(mode),
            None => {
                eprintln!(
                    " Unknown test mode '{}'. Use: check-pass, check-fail, run-pass, run-fail, kain-test",
                    value
                );
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
    if let Some(path) = json {
        if !write_structured_report(path, &report, "test") {
            return false;
        }
    }

    println!(
        " Test {}: {}/{} passed; {} skipped",
        if report.is_success() {
            "passed"
        } else {
            "failed"
        },
        report.passed,
        report.total,
        report.skipped
    );
    for case in report.cases.iter().filter(|case| case.skipped()) {
        if let Some(reason) = &case.skip_reason {
            println!("  skipped {} [{}]: {}", case.path, case.mode, reason);
        }
    }
    for case in report.cases.iter().filter(|case| !case.passed()) {
        if case.skipped() {
            continue;
        }
        if let Some(error) = &case.error {
            eprintln!("  {} [{}]: {}", case.path, case.mode, error);
        }
    }
    report.is_success()
}

fn write_structured_report<T: Serialize>(path: &Path, value: &T, label: &str) -> bool {
    if !ensure_parent_dir(path) {
        return false;
    }
    match serde_json::to_string_pretty(value) {
        Ok(encoded) => match fs::write(path, encoded) {
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
        },
        Err(error) => {
            eprintln!(" Failed to serialize {} report: {}", label, error);
            false
        }
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
    use notify::{Event, RecursiveMode, Watcher};
    use std::sync::mpsc::channel;

    let running = Arc::new(AtomicBool::new(true));
    let r = running.clone();

    ctrlc::set_handler(move || {
        println!("\n Stopping watch mode...");
        r.store(false, Ordering::SeqCst);
    })
    .expect("Error setting Ctrl-C handler");

    println!(
        " Watching {} for changes... (Ctrl+C to stop)",
        input.display()
    );
    println!("");

    // Initial compile
    run_compile(
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

    let mut watcher = notify::recommended_watcher(move |res: Result<Event, notify::Error>| {
        if let Ok(event) = res {
            if event.kind.is_modify() {
                let _ = tx.send(());
            }
        }
    })
    .expect("Failed to create watcher");

    watcher
        .watch(&input, RecursiveMode::NonRecursive)
        .expect("Failed to watch file");

    // Also watch parent directory in case file is replaced
    if let Some(parent) = input.parent() {
        let _ = watcher.watch(parent, RecursiveMode::NonRecursive);
    }

    while running.load(Ordering::SeqCst) {
        match rx.recv_timeout(Duration::from_millis(100)) {
            Ok(_) => {
                // Debounce - wait a bit for writes to settle
                std::thread::sleep(Duration::from_millis(50));
                // Drain any pending events
                while rx.try_recv().is_ok() {}

                println!(" File changed, recompiling...");
                println!("");
                run_compile(
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

fn main() {
    let builder = std::thread::Builder::new()
        .name("main-thread".into())
        .stack_size(8 * 1024 * 1024); // 8MB

    let handler = builder
        .spawn(|| {
            let launcher = detect_launcher_from_path(std::env::current_exe().ok().as_deref());
            let matches = Args::command()
                .bin_name(launcher.display_name())
                .get_matches();
            let args = Args::from_arg_matches(&matches).unwrap_or_else(|err| err.exit());
            let suppress_banner = matches!(&args.command, Some(Commands::Format { .. }));
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
                    if let Some(mode) = repair_args.selected_mode() {
                        let profile_label = repair_args.selected_profile_label();
                        match repair_args.target_kind() {
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
                Some(Commands::Format {
                    input,
                    check,
                    write,
                }) => {
                    if !run_format_command(input, check, write) {
                        std::process::exit(1);
                    }
                }
                Some(Commands::Check {
                    input,
                    target,
                    fail_fast,
                    json,
                }) => {
                    if !run_check_command(&input, &target, fail_fast, json.as_deref()) {
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
                }) => {
                    if !run_test_command(
                        &input,
                        mode.as_deref(),
                        &target,
                        fail_fast,
                        ignored,
                        json.as_deref(),
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
                Some(Commands::Blades { command }) => {
                    if let Err(e) = blades::run(command) {
                        eprintln!(" Blades command failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Equip { blade, path, json }) => {
                    if let Err(e) = blades::run_equip(blade, path, json) {
                        eprintln!(" Equip failed: {}", e);
                        std::process::exit(1);
                    }
                }
                Some(Commands::Build {
                    command,
                    input,
                    output,
                    target,
                    targets,
                    ue5,
                    r#rust,
                    embed,
                }) => {
                    if let Some(BuildCommand::NativeUi {
                        input,
                        root_component,
                        app_name,
                        window_title,
                        project_dir,
                        artifact_dir,
                        bundle_only,
                        release,
                        runtime_crate,
                        runtime_path,
                        runtime_version,
                        host,
                        tauri_bundle_id,
                        tauri_window_label,
                    }) = command
                    {
                        let host = match parse_native_ui_host_kind(&host) {
                            Ok(host) => host,
                            Err(err) => {
                                eprintln!(" Native UI build failed: {}", err);
                                std::process::exit(1);
                            }
                        };
                        let runtime_dependency = if let Some(path) = runtime_path {
                            native_ui_build::NativeUiRuntimeDependencyConfig::Path(path)
                        } else if let Some(version) = runtime_version {
                            native_ui_build::NativeUiRuntimeDependencyConfig::Version(version)
                        } else {
                            native_ui_build::NativeUiRuntimeDependencyConfig::WorkspacePath
                        };
                        let config = native_ui_build::NativeUiBuildConfig {
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
                            build_executable: !bundle_only,
                            release,
                            runtime_crate_name: runtime_crate,
                            runtime_dependency,
                            ..Default::default()
                        };

                        match native_ui_build::run_native_ui_build_pipeline(&input, &config) {
                            Ok(result) => {
                                println!(
                                    " Native UI app: {} ({})",
                                    result.metadata.app_name, result.metadata.root_component
                                );
                                for path in result.written_paths() {
                                    println!("   ✓ {}", path.display());
                                }
                            }
                            Err(e) => {
                                eprintln!(" Native UI build failed: {}", e);
                                std::process::exit(1);
                            }
                        }
                    } else if ue5 {
                        // UE5 plugin build
                        if let Err(e) = packager::build_ue5_plugin_with_options(embed) {
                            // Error already contains formatted details with file:line:col
                            eprintln!("{}", e);
                            std::process::exit(1);
                        }
                    } else if r#rust {
                        match input {
                            Some(file) => {
                                match rust_build::run_rust_build_pipeline(
                                    &file,
                                    output.as_ref(),
                                    None,
                                ) {
                                    Ok(paths) => {
                                        for path in paths {
                                            println!("   ✓ {}", path.display());
                                        }
                                    }
                                    Err(e) => {
                                        eprintln!(" Rust build failed: {}", e);
                                        std::process::exit(1);
                                    }
                                }
                            }
                            None => {
                                if let Err(e) = packager::build_rust_project() {
                                    eprintln!(" Build failed: {}", e);
                                    std::process::exit(1);
                                }
                            }
                        }
                    } else {
                        match input {
                            Some(file) => {
                                let target_alias =
                                    target.as_deref().unwrap_or(args.target.as_str());
                                let Some(resolved_target) = parse_compile_target(target_alias)
                                else {
                                    eprintln!(
                                        " Unknown target: {}. Use: {}",
                                        target_alias,
                                        supported_targets_csv()
                                    );
                                    std::process::exit(1);
                                };
                                run_compile(
                                    &file,
                                    resolved_target,
                                    output.as_ref(),
                                    args.emit_ast,
                                    args.emit_typed,
                                    args.verbose,
                                    args.analyze,
                                    args.plugin.as_deref(),
                                );
                            }
                            None => {
                                // Project build from KAIN.toml
                                let target_overrides = if let Some(single_target) = target {
                                    Some(vec![single_target])
                                } else {
                                    targets.clone()
                                };
                                if let Err(e) = packager::build_project(target_overrides) {
                                    eprintln!(" Build failed: {}", e);
                                    std::process::exit(1);
                                }
                            }
                        }
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
                Some(Commands::Repl) => {
                    if !run_kn_repl() {
                        std::process::exit(1);
                    }
                }
                Some(Commands::Run { input }) => {
                    run_compile(
                        &input,
                        CompileTarget::Interpret,
                        None,
                        args.emit_ast,
                        args.emit_typed,
                        args.verbose,
                        args.analyze,
                        args.plugin.as_deref(),
                    );
                }
                Some(Commands::GpuArtifacts { input, output }) => {
                    let config = packager::RustBuildConfig {
                        output: None,
                        artifacts: vec![
                            packager::RustBuildArtifact::ShaderHost,
                            packager::RustBuildArtifact::ShaderReflection,
                            packager::RustBuildArtifact::Spirv,
                        ],
                        native_ui: None,
                    };

                    match rust_build::run_rust_build_pipeline(
                        &input,
                        output.as_ref(),
                        Some(&config),
                    ) {
                        Ok(paths) => {
                            println!(" Generated {} Rust shader artifact files:", paths.len());
                            for path in paths {
                                println!("   - {}", path.display());
                            }
                        }
                        Err(e) => {
                            eprintln!(" Rust shader artifact generation failed: {}", e);
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
    println!(" Repair Target: {}", path.display());
    println!(" Selected Profile: {}", profile);
    println!(" Repair Mode: {:?}", mode);
    println!(" Safety Class: {:?}", report.safety_class);
    println!(" Unknown Risk: {:?}", report.remaining_unknown_risk);
    println!(
        " Parser Proof: {}",
        match report.parser_proof_status() {
            Some(kain_repair::ParserProofStatus::Passed) => "passed",
            Some(kain_repair::ParserProofStatus::Failed) => "failed",
            None => "unverified",
        }
    );

    if report.fixes.is_empty() {
        println!(" Fixes Applied: none needed");
        println!(" Remaining Diagnostics: 0");
        return;
    }

    println!(" Fixes Applied: {}", report.fixes_applied);
    println!(" Fixes:");
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
    println!(" Remaining Diagnostics: 0");
}

fn print_repair_tree_report(
    root: &Path,
    report: &repair::DoctorRepairBatchReport,
    mode: kain_repair::RepairMode,
    profile: &str,
) {
    println!(" Repair Root: {}", root.display());
    println!(" Selected Profile: {}", profile);
    println!(" Repair Mode: {:?}", mode);
    println!(
        " Action Class: {}",
        if matches!(mode, kain_repair::RepairMode::ApplyAggressive) {
            "aggressive"
        } else {
            "safe"
        }
    );
    println!(" Files Scanned: {}", report.scanned);
    println!(" Files Changed: {}", report.changed);
    println!(" Files Written: {}", report.written);
    println!(" Files Failed: {}", report.failed);
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

fn print_doctor(active_launcher: LauncherKind) {
    let current_exe = std::env::current_exe().ok();
    let kain_path_command = which::which("kain").ok();
    let kn_path_command = which::which("kn").ok();
    let kain_path_matches = collect_path_matches("kain");
    let kn_path_matches = collect_path_matches("kn");
    let stdlib_roots = kain_core::stdlib::find_stdlib_search_roots();
    let runtime_c = find_runtime_c();
    let runtime_manifest = find_native_runtime_manifest();
    let resolved_clang = if cfg!(feature = "sys") {
        find_bundled_clang()
    } else {
        None
    };

    println!(" KAIN Doctor");
    println!(" Version: {}", VERSION);
    println!(" Build: {}", BUILD_NUMBER);
    println!(" Built At (UTC): {}", format_build_time(BUILD_UNIX_TIME));
    println!(
        " Git: {} (commit #{}, {})",
        BUILD_GIT_SHA, BUILD_GIT_COMMIT_COUNT, BUILD_GIT_DIRTY
    );
    println!(" Profile: {}", BUILD_PROFILE);
    println!(
        " Target Triple: {} (host {})",
        BUILD_TARGET_TRIPLE, BUILD_HOST_TRIPLE
    );

    match &current_exe {
        Some(path) => {
            println!(" Binary Path: {}", path.display());
            println!(" Binary Kind: {}", classify_binary_path(path));
        }
        None => println!(" Binary Path: <unknown>"),
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

    match std::env::current_dir() {
        Ok(cwd) => {
            println!(" Current Dir: {}", cwd.display());
            if let Some(root) = find_project_root(&cwd) {
                println!(" Project Root: {}", root.display());
            } else {
                println!(" Project Root: <not found (no KAIN.toml in parent chain)>");
            }
        }
        Err(err) => println!(" Current Dir: <unknown> ({})", err),
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

    println!(
        " PATH Match Status (kain): {}",
        doctor_path_status(current_exe.as_deref(), kain_path_command.as_deref(), "kain")
    );
    println!(
        " PATH Match Status (kn): {}",
        doctor_path_status(current_exe.as_deref(), kn_path_command.as_deref(), "kn")
    );

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
    }

    if let Some(path) = current_exe.as_deref() {
        if is_repo_target_binary(path) {
            println!(" Warning: active kain comes from a repo target directory.");
            if cfg!(windows) {
                println!(
                    "          Refresh/install a stable PATH binary with scripts/windows/sync-kain-source-of-truth.ps1."
                );
            } else {
                println!(
                    "          Refresh/install a stable PATH binary with python3 install_kain.py and source generated/kain-env.sh."
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
    if is_repo_target_binary(path) {
        "repo-target"
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
        (Some(_), Some(_)) => "drift: current process differs from PATH".to_string(),
        (Some(_), None) => {
            format!("current process exists, but {command_name} is not resolvable from PATH")
        }
        (None, Some(_)) => {
            format!("PATH resolves {command_name}, but current process path is unknown")
        }
        (None, None) => "unknown".to_string(),
    }
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
    if let Ok(env_path) = std::env::var("KAIN_RUNTIME_C_PATH") {
        let p = PathBuf::from(env_path);
        if p.exists() {
            return Some(p);
        }
    }

    let candidates = [
        PathBuf::from("runtime/kain_runtime.c"),
        PathBuf::from("runtime/KAIN_runtime.c"),
        PathBuf::from("src/runtime/c/KAIN_runtime.c"),
    ];

    for candidate in candidates {
        if candidate.exists() {
            return Some(candidate);
        }
    }

    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(mut dir) = exe_path.parent().map(|p| p.to_path_buf()) {
            loop {
                let candidate = dir.join("runtime").join("kain_runtime.c");
                if candidate.exists() {
                    return Some(candidate);
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    None
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

fn resolve_c_ffi_shared_libraries_for_linking(source: &str) -> Result<Vec<PathBuf>, String> {
    let prepare = CPrepareContext {
        current_dir: std::env::current_dir().ok(),
        manifest_path: None,
    };
    let outputs = kain_c_ffi::import_libraries_for_source(
        source,
        &CImportCOptions {
            mode: CArtifactMode::Generate,
            ..CImportCOptions::default()
        },
        &prepare,
    )
    .map_err(|err| err.to_string())?;

    let mut shared_libraries = Vec::new();
    for output in outputs {
        let shared_lib_path = output.resolved.shared_lib_path.ok_or_else(|| {
            format!(
                "C FFI library '{}' does not declare a shared library for LLVM linking",
                output.resolved.import_name
            )
        })?;
        if !shared_lib_path.exists() {
            return Err(format!(
                "C FFI shared library {} does not exist",
                shared_lib_path.display()
            ));
        }
        shared_libraries.push(shared_lib_path);
    }

    Ok(shared_libraries)
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
) -> Result<NativeRuntimeCompiledArtifacts, String> {
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
    let mut reused_object_count = 0usize;
    let mut compiled_object_count = 0usize;

    for (index, source) in bundle.sources.iter().enumerate() {
        let cache_paths =
            build_native_runtime_object_cache_paths(&runtime_obj_dir, index, source, object_ext);
        let compile_fingerprint =
            build_native_runtime_compile_fingerprint(bundle, clang_cmd, source)?;

        if native_runtime_object_cache_is_fresh(&cache_paths, &compile_fingerprint) {
            reused_object_count += 1;
            object_paths_by_source.insert(source.clone(), cache_paths.object_path.clone());
            continue;
        }

        let mut compile_cmd = std::process::Command::new(clang_cmd);
        compile_cmd
            .arg("-c")
            .arg(source)
            .arg("-o")
            .arg(&cache_paths.object_path)
            .arg("-g")
            .arg("-MMD")
            .arg("-MF")
            .arg(&cache_paths.depfile_path)
            .arg("-MT")
            .arg("kain_runtime_target");

        if runtime_source_uses_cpp(source) {
            compile_cmd.arg("-std=c++20");
        }

        for include_dir in &bundle.include_dirs {
            compile_cmd.arg("-I").arg(include_dir);
        }
        for define in &bundle.defines {
            compile_cmd.arg(format!("-D{}", define));
        }

        let status = compile_cmd
            .status()
            .map_err(|err| format!("unable to invoke clang for {}: {}", source.display(), err))?;
        if !status.success() {
            let _ = fs::remove_file(&cache_paths.object_path);
            return Err(format!(
                "clang returned a non-zero status while compiling {}",
                source.display()
            ));
        }
        fs::write(&cache_paths.fingerprint_path, &compile_fingerprint).map_err(|err| {
            format!(
                "unable to write runtime build fingerprint {}: {}",
                cache_paths.fingerprint_path.display(),
                err
            )
        })?;
        compiled_object_count += 1;
        object_paths_by_source.insert(source.clone(), cache_paths.object_path.clone());
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
            let archive_fingerprint =
                build_native_runtime_archive_fingerprint(bundle, &archiver, archive_group);

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
            reused_object_count,
            compiled_object_count,
            reused_archive_count,
            rebuilt_archive_count
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

fn build_native_runtime_compile_fingerprint(
    bundle: &ResolvedNativeRuntimeBundle,
    clang_cmd: &str,
    source: &Path,
) -> Result<String, String> {
    let mut fingerprint_lines = vec![
        "kain-native-runtime-cache-v1".to_string(),
        format!("bundle={}", bundle.name),
        format!("clang={}", clang_cmd),
        format!("source={}", source.display()),
        format!("cpp={}", runtime_source_uses_cpp(source)),
    ];

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
            match chars.next() {
                Some('\n') => {}
                Some('\r') => {
                    if matches!(chars.peek(), Some('\n')) {
                        chars.next();
                    }
                }
                Some(escaped) => current_token.push(escaped),
                None => current_token.push('\\'),
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
    if cfg!(windows) {
        vec![
            "legacy_stdio_definitions".to_string(),
            "user32".to_string(),
            "gdi32".to_string(),
            "opengl32".to_string(),
            "ws2_32".to_string(),
        ]
    } else if cfg!(target_os = "linux") {
        vec!["m".to_string()]
    } else {
        Vec::new()
    }
}

fn default_native_runtime_cpp_link_libs() -> Vec<String> {
    if cfg!(windows) {
        Vec::new()
    } else if cfg!(target_os = "macos") {
        vec!["c++".to_string()]
    } else {
        vec!["stdc++".to_string()]
    }
}

fn find_native_runtime_manifest() -> Option<PathBuf> {
    if let Ok(env_path) = std::env::var("KAIN_RUNTIME_MANIFEST_PATH") {
        let path = PathBuf::from(env_path);
        if path.exists() {
            return Some(path);
        }
    }

    for root in runtime_search_roots() {
        for suffix in native_runtime_manifest_candidate_suffixes() {
            let candidate = root.join(suffix);
            if candidate.exists() {
                return Some(candidate);
            }
        }
    }

    None
}

fn native_runtime_manifest_candidate_suffixes() -> [PathBuf; 3] {
    [
        PathBuf::from("runtime/native_core_runtime.toml"),
        PathBuf::from("runtime/native_runtime.toml"),
        PathBuf::from("runtime/native/runtime.toml"),
    ]
}

fn runtime_search_roots() -> Vec<PathBuf> {
    let mut roots = Vec::new();
    if let Ok(cwd) = std::env::current_dir() {
        roots.push(cwd);
    }
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(mut dir) = exe_path.parent().map(|path| path.to_path_buf()) {
            loop {
                roots.push(dir.clone());
                if !dir.pop() {
                    break;
                }
            }
        }
    }
    roots
}

#[cfg(test)]
mod tests {
    use super::{
        build_native_runtime_archive_fingerprint, build_native_runtime_compile_fingerprint,
        build_native_runtime_object_cache_paths, default_native_runtime_link_libs,
        default_runtime_cache_root, load_native_runtime_manifest,
        native_runtime_object_cache_is_fresh, parse_native_runtime_depfile, platform_link_libs,
        resolve_native_runtime_archive_groups, runtime_source_uses_cpp, sanitize_runtime_name,
        unique_link_libs, NativeRuntimeArchiveManifest, NativeRuntimeArchiver,
        NativeRuntimeArchiverFlavor, NativeRuntimeLinkManifest, ResolvedNativeRuntimeArchiveGroup,
        ResolvedNativeRuntimeBundle,
    };
    use std::{fs, path::Path, path::PathBuf, thread::sleep, time::Duration};

    #[test]
    fn sanitize_runtime_name_keeps_object_filenames_stable() {
        assert_eq!(sanitize_runtime_name("Kain Runtime"), "kain_runtime");
        assert_eq!(sanitize_runtime_name("###"), "runtime");
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
    fn runtime_manifest_resolves_relative_paths() {
        let temp_dir = tempfile::tempdir().expect("temp dir");
        let manifest_dir = temp_dir.path().join("runtime");
        let source_dir = manifest_dir.join("src");
        let include_dir = manifest_dir.join("include");
        fs::create_dir_all(&source_dir).expect("source dir");
        fs::create_dir_all(&include_dir).expect("include dir");
        fs::write(
            source_dir.join("kain_runtime.c"),
            "int main(void) { return 0; }\n",
        )
        .expect("source file");
        fs::write(
            manifest_dir.join("native_runtime.toml"),
            r#"
name = "test-runtime"
sources = ["src/kain_runtime.c"]
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

        let vendor_fingerprint =
            build_native_runtime_archive_fingerprint(&bundle, &archiver, &vendor_group);
        let ui_fingerprint =
            build_native_runtime_archive_fingerprint(&bundle, &archiver, &ui_group);

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

        assert_eq!(
            dependencies,
            vec![
                PathBuf::from("/tmp/runtime source.c"),
                PathBuf::from("/tmp/include/header file.h"),
                PathBuf::from("/tmp/include/next.h"),
            ]
        );
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
        let fingerprint =
            build_native_runtime_compile_fingerprint(&bundle, "clang", &source_path).expect("fp");

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
}

fn find_bundled_clang() -> Option<String> {
    if let Ok(env_path) = std::env::var("KAIN_CLANG_PATH") {
        let path = PathBuf::from(env_path);
        if path.exists() {
            return Some(path.to_string_lossy().into_owned());
        }
    }

    let candidate_suffixes = [
        PathBuf::from("toolchain/llvm/bin/clang.exe"),
        PathBuf::from("toolchain/llvm/bin/clang"),
        PathBuf::from("third_party/llvm/bin/clang.exe"),
        PathBuf::from("third_party/llvm/bin/clang"),
        PathBuf::from("llvm/bin/clang.exe"),
        PathBuf::from("llvm/bin/clang"),
    ];

    let mut search_roots = Vec::new();
    if let Ok(cwd) = std::env::current_dir() {
        search_roots.push(cwd);
    }
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(dir) = exe_path.parent() {
            let mut cursor = dir.to_path_buf();
            loop {
                search_roots.push(cursor.clone());
                if !cursor.pop() {
                    break;
                }
            }
        }
    }

    for root in search_roots {
        for suffix in &candidate_suffixes {
            let candidate = root.join(suffix);
            if candidate.exists() {
                return Some(candidate.to_string_lossy().into_owned());
            }
        }
    }

    None
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
