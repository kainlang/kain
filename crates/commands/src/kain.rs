use clap::{Parser as ClapParser, Subcommand, ValueEnum};
use std::path::PathBuf;

use crate::codebase::CodebaseCommand;
use crate::fabric::FabricCommand;
use crate::omni::OmniCommand;
use crate::repair::DoctorRepairArgs;
use crate::selfhost::SelfHostCommand;

/// Output artifact type for native builds.
#[derive(ValueEnum, Clone, Debug)]
pub enum EmitMode {
    /// Standalone executable (default)
    Exe,
    /// Shared library / DLL
    SharedLib,
    /// Static library
    StaticLib,
    /// Object file
    Object,
}

#[derive(Subcommand, Debug)]
pub enum AmalgamateCommand {
    /// Inspect a Kain capsule and print its metadata, file inventory, and symbol index
    Inspect {
        /// Capsule artifact path
        input: PathBuf,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Unpack a Kain capsule into a directory tree
    Unpack {
        /// Capsule artifact path
        input: PathBuf,

        /// Output directory (defaults to <capsule>.unpacked)
        #[arg(short, long)]
        output: Option<PathBuf>,
    },
}

#[derive(Subcommand, Debug)]
pub enum RunCommand {
    /// Launch a run plan and keep re-running when inputs change
    Dev {
        /// Entry file, Cargo manifest, Fabric manifest, blade root, or workspace path
        input: Option<PathBuf>,

        /// Run target override
        #[arg(long, default_value = "auto")]
        target: String,

        /// Emit DWARF debug metadata in LLVM IR
        #[arg(short = 'g', long = "debug")]
        debug: bool,

        /// Emit the run report JSON to stdout
        #[arg(long)]
        json: bool,

        /// Include trace-oriented report detail
        #[arg(long)]
        trace: bool,

        /// Keep cached/generated run artifacts
        #[arg(long = "keep-artifacts")]
        keep_artifacts: bool,

        /// Plan the dev loop without executing the first run
        #[arg(long)]
        dry_run: bool,

        /// Runtime args. Use `--` before this vector.
        #[arg(last = true)]
        args: Vec<String>,
    },

    /// Print the resolved run plan without executing it
    Plan {
        /// Entry file, Cargo manifest, Fabric manifest, blade root, or workspace path
        input: Option<PathBuf>,

        /// Run target override
        #[arg(long, default_value = "auto")]
        target: String,

        /// Emit the plan JSON to stdout
        #[arg(long)]
        json: bool,
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum)]
pub enum CliColorArg {
    Auto,
    Always,
    Never,
}

#[derive(ClapParser, Debug)]
#[command(name = "kain")]
#[command(author = "Kipp")]
#[command(version = env!("CARGO_PKG_VERSION"))]
#[command(about = "Kernel Architecture for Interop and Native", long_about = None)]
pub struct KainCli {
    #[command(subcommand)]
    pub command: Option<KainCommand>,

    /// Explicit Kain config path. Otherwise resolves nearest .kain/config.toml, then KAIN_CONFIG, then KAIN_HOME/config.toml.
    #[arg(long, global = true)]
    pub config: Option<PathBuf>,

    /// Force CLI color policy
    #[arg(long, global = true, value_enum)]
    pub color: Option<CliColorArg>,

    /// Select a CLI theme: plain, lattice, slate, graphite, arctic, or sandstone
    #[arg(long, global = true)]
    pub theme: Option<String>,

    /// Source file to compile (legacy positional argument)
    pub input: Option<PathBuf>,

    /// Inline Kain source, similar to `python -c`
    #[arg(short = 'c', long, conflicts_with = "input")]
    pub code: Option<String>,

    /// Output file
    #[arg(short, long)]
    pub output: Option<PathBuf>,

    /// Compilation target
    #[arg(short, long, default_value = "wasm")]
    pub target: String,

    /// Run immediately after compilation
    #[arg(short, long)]
    pub run: bool,

    /// Watch for file changes and recompile
    #[arg(short, long)]
    pub watch: bool,

    /// Emit AST for debugging
    #[arg(long)]
    pub emit_ast: bool,

    /// Emit typed AST
    #[arg(long)]
    pub emit_typed: bool,

    /// Verbose output
    #[arg(short, long)]
    pub verbose: bool,

    /// Target plugin name for UE5 shader copy
    #[arg(long)]
    pub plugin: Option<String>,

    /// Base plugins directory (defaults to u:\ue_factory\src-plugins)
    #[arg(long)]
    pub plugins_dir: Option<PathBuf>,

    /// Print planned actions without executing
    #[arg(long)]
    pub dry_run: bool,

    /// Treat transpiler warnings as errors when supported
    #[arg(long)]
    pub strict: bool,

    /// Analyze shader complexity (USF target only)
    #[arg(long)]
    pub analyze: bool,
}

#[derive(Subcommand, Debug)]
pub enum BuildCommand {
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

        /// Clean generated .kain roots before building the native UI app
        #[arg(long)]
        clean: bool,

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

#[derive(Subcommand, Debug)]
pub enum RuntimeCommand {
    /// Compile the manifest-driven native runtime bundle
    Build {
        /// Compile the standalone runtime bundle in release mode
        #[arg(long)]
        release: bool,

        /// Forward verbose output to the runtime build script
        #[arg(long)]
        verbose: bool,
    },

    /// Run the aggregate native runtime validation lane
    Validate {
        /// Compile the standalone runtime bundle in release mode
        #[arg(long)]
        release: bool,

        /// Forward verbose output to runtime scripts
        #[arg(long)]
        verbose: bool,

        /// Skip `cargo build -p cli`
        #[arg(long = "skip-cli-build")]
        skip_cli_build: bool,

        /// Skip the standalone runtime bundle build step
        #[arg(long = "skip-runtime-build")]
        skip_runtime_build: bool,

        /// Skip the native fixture suite
        #[arg(long = "skip-fixtures")]
        skip_fixtures: bool,

        /// Skip the native conformance suite
        #[arg(long = "skip-conformance")]
        skip_conformance: bool,
    },
}

#[derive(Subcommand, Debug)]
pub enum NativeUiCommand {
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

#[derive(Subcommand, Debug)]
pub enum BridgeCommand {
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

#[derive(Subcommand, Debug)]
pub enum ImportCommand {
    /// Import a native platform SDK/package into a target-aware lock and generated typed thunks
    Platform {
        /// Package name or SDK root path, for example `vulkan` or `vendor/tiny_math`
        package: String,

        /// Override package/module name when the positional argument is a path
        #[arg(long = "package-name")]
        package_name: Option<String>,

        /// Package provider label recorded in the lockfile
        #[arg(long, default_value = "system")]
        provider: String,

        /// Explicit SDK root to scan
        #[arg(long)]
        sdk: Option<PathBuf>,

        /// Output directory, defaults to .kain/platform/<package>/<target-triple>
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Target triple to lock against
        #[arg(long = "target-triple")]
        target_triple: Option<String>,

        /// Print the resolved lock without writing artifacts
        #[arg(long)]
        dry_run: bool,

        /// Also write the lock/report JSON to this path
        #[arg(long = "report-json")]
        report_json: Option<PathBuf>,

        /// Explicit registry metadata file, such as Vulkan vk.xml
        #[arg(long)]
        registry: Option<PathBuf>,

        /// Explicit C header entrypoint
        #[arg(long)]
        header: Option<PathBuf>,
    },

    /// Import every Rust crate in a workspace into one Kain bundle or a mirrored blades tree
    Crates {
        /// Workspace root (defaults to the current directory)
        path: Option<PathBuf>,

        /// Explicit Rust workspace source root (defaults to ./crates, ./rust, or ./src/rust)
        #[arg(long)]
        source_root: Option<PathBuf>,

        /// Output .kn file for bundle mode or output directory for --blades mode
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Mirror each imported crate into a blades-style directory tree
        #[arg(long)]
        blades: bool,

        /// Compilation target for the generated bundle file
        #[arg(short, long, conflicts_with = "blades")]
        target: Option<String>,

        /// Flatten all imported items into one global scope in bundle mode
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
    },
}

#[derive(Subcommand, Debug)]
#[command(disable_help_subcommand = true)]
pub enum RegistryCommand {
    /// List command registry entries
    List {
        /// Filter to one executable view
        #[arg(long)]
        bin: Option<String>,

        /// Include runtime command contributions from the current workspace
        #[arg(long)]
        runtime: bool,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Export command registry metadata as JSON
    Export {
        /// Filter to one executable view
        #[arg(long)]
        bin: Option<String>,

        /// Include runtime command contributions from the current workspace
        #[arg(long)]
        runtime: bool,
    },

    /// List command packs loaded into the built-in registry
    Packs {
        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Render help from the manifest-backed dynamic Clap builder
    Help {
        /// Executable view to render
        #[arg(long, default_value = "kain")]
        bin: String,

        /// Include runtime command contributions from the current workspace
        #[arg(long)]
        runtime: bool,
    },
}

#[derive(Subcommand, Debug)]
pub enum ConfigCommand {
    /// Print the active config path plus the resolved CLI/build settings
    Show {
        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Update a single config key in the active Kain control plane
    Set {
        /// Dotted config key such as build.jobs or ui.theme
        key: String,

        /// New value for the selected key
        value: String,
    },

    /// Write a starter Kain config file for the current machine
    Init {
        /// Override the config file destination
        #[arg(long)]
        path: Option<PathBuf>,

        /// Overwrite an existing config file
        #[arg(long)]
        force: bool,
    },
}

// ─────────────────────────────────────────────────────────────
//  KainCommand — top-level subcommand enum, organized by
//  category via clap's next_help_heading for `kain --help`.
// ─────────────────────────────────────────────────────────────

#[derive(Subcommand, Debug)]
pub enum KainCommand {
    // ── Core Commands ──────────────────────────────────────────
    /// Check Kain source without emitting backend artifacts
    #[command(visible_alias = "c")]
    Check {
        /// Input Kain source file or directory. Use '-' to read from stdin.
        input: PathBuf,

        /// Target profile to typecheck against
        #[arg(short, long, default_value = "run")]
        target: String,

        /// Stop after the first failed file
        #[arg(long)]
        fail_fast: bool,

        /// Emit structured JSON to stdout for LLMs and CI
        #[arg(long, conflicts_with = "json_out")]
        json: bool,

        /// Write a structured JSON check report to a file
        #[arg(long = "json-out", conflicts_with = "json")]
        json_out: Option<PathBuf>,

        /// Run ALL validators including expensive/speculative ones (ETA-B)
        #[arg(long)]
        pedantic: bool,

        /// Run check then build, report errors build caught that check missed (ETA-C)
        #[arg(long)]
        audit: bool,
    },

    /// Build a file, project, or build authority. Without input, builds the current project.
    #[command(visible_alias = "b")]
    Build {
        #[command(subcommand)]
        command: Option<BuildCommand>,

        /// Optional input file or project path. If omitted, builds the current project root.
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

        /// Canonical build lane: bootstrap, dev, release, dist, or selfhost
        #[arg(long)]
        lane: Option<String>,

        /// Clean generated .kain roots before building
        #[arg(long)]
        clean: bool,

        /// Emit DWARF debug metadata in LLVM IR (!DILocation, !DISubprogram, etc.)
        #[arg(short = 'g', long = "debug")]
        debug: bool,

        /// Build UE5 plugin from KAIN.toml [ue5] config
        #[arg(long)]
        ue5: bool,

        #[arg(long)]
        r#rust: bool,

        /// Embed original KAIN source as comments in generated C++ (debugging/round-trip)
        #[arg(long)]
        embed: bool,

        /// Output artifact type for native builds. Implies --target llvm.
        #[arg(long, value_enum)]
        emit: Option<EmitMode>,
    },

    /// Run a file, blade, manifest, or workspace through the unified run pipeline
    #[command(visible_alias = "r")]
    Run {
        #[command(subcommand)]
        command: Option<RunCommand>,

        /// Entry file, Cargo manifest, Fabric manifest, project root, or workspace path
        input: Option<PathBuf>,

        /// Run target override
        #[arg(long, default_value = "auto")]
        target: String,

        /// Emit DWARF debug metadata in LLVM IR (!DILocation, !DISubprogram, etc.)
        #[arg(short = 'g', long = "debug")]
        debug: bool,

        /// Emit the run report JSON to stdout
        #[arg(long)]
        json: bool,

        /// Include trace-oriented report detail
        #[arg(long)]
        trace: bool,

        /// Keep cached/generated run artifacts
        #[arg(long = "keep-artifacts")]
        keep_artifacts: bool,

        /// Print the resolved run plan without executing
        #[arg(long)]
        dry_run: bool,

        /// Runtime args. Use `--` before this vector.
        #[arg(last = true)]
        args: Vec<String>,
    },

    /// Run Kain source tests using Rust-style pass/fail directives
    #[command(visible_alias = "t")]
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

        /// Emit structured JSON to stdout for LLMs and CI
        #[arg(long, conflicts_with = "json_out")]
        json: bool,

        /// Write a structured JSON test report to a file
        #[arg(long = "json-out", conflicts_with = "json")]
        json_out: Option<PathBuf>,
    },

    /// Show binary/build diagnostics and resolved compiler capabilities
    #[command(visible_alias = "d")]
    Doctor {
        #[command(flatten)]
        repair: DoctorRepairArgs,
    },

    /// Clean generated .kain roots for the current workspace
    #[command(visible_alias = "cl")]
    Clean {
        /// Path inside the workspace to clean
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Clean scope: build, run, amalgamate, or all
        #[arg(long, default_value = "all")]
        scope: String,

        /// Print the clean plan without removing anything
        #[arg(long)]
        dry_run: bool,

        /// Emit JSON instead of text
        #[arg(long)]
        json: bool,
    },

    /// Format Kain source using the compiler-owned canonical printer
    #[command(visible_alias = "fmt", visible_alias = "f")]
    Format {
        /// Input Kain source files or directories. Use '-' or piped stdin.
        inputs: Vec<PathBuf>,

        /// Check whether the source is already formatted
        #[arg(long, conflicts_with = "write")]
        check: bool,

        /// Rewrite the resolved input files in place
        #[arg(short = 'w', long, conflicts_with = "check")]
        write: bool,
    },

    /// Start the interactive Kain REPL
    Repl,

    /// Watch a run plan and re-run it when inputs change
    Watch {
        /// Entry file, Cargo manifest, Fabric manifest, project root, or workspace path
        input: Option<PathBuf>,

        /// Run target override
        #[arg(long, default_value = "auto")]
        target: String,

        /// Emit the run report JSON to stdout
        #[arg(long)]
        json: bool,

        /// Include trace-oriented report detail
        #[arg(long)]
        trace: bool,

        /// Keep cached/generated run artifacts
        #[arg(long = "keep-artifacts")]
        keep_artifacts: bool,

        /// Print the resolved run plan without entering the watcher loop
        #[arg(long)]
        dry_run: bool,

        /// Runtime args. Use `--` before this vector.
        #[arg(last = true)]
        args: Vec<String>,
    },

    /// Initialize a new KAIN project
    #[command(visible_alias = "i")]
    Init {
        /// Project name
        #[arg(default_value = ".")]
        path: PathBuf,

        /// Explicit project name
        #[arg(long)]
        name: Option<String>,
    },

    // ── Package Commands ───────────────────────────────────────
    /// Record a capsule-backed dependency in the current project and pin it in KAIN.lock
    Add {
        /// Installed package name, local package root, or source capsule path
        package: String,

        /// Override the package version when installing or pinning
        #[arg(long)]
        version: Option<String>,

        /// Explicit project root or KAIN.toml path to update
        #[arg(long)]
        manifest: Option<PathBuf>,
    },

    /// Install a capsule-backed package into the global Kain package store
    Install {
        /// Installed package name, local package root, or source capsule path
        package: String,

        /// Override the package version when installing or activating
        #[arg(long)]
        version: Option<String>,
    },

    /// Publish a local Kain package as one or more portable source capsules
    Publish {
        /// Package root, project root, workspace path, or entry file to publish
        input: PathBuf,

        /// Output source capsule path. Defaults to <input>/.kain/publish/<name>-<version>.kn
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Override the published package name
        #[arg(long)]
        name: Option<String>,

        /// Override the published package version
        #[arg(long)]
        version: Option<String>,

        /// Also emit an artifacts companion capsule next to the source capsule
        #[arg(long)]
        artifacts: bool,

        /// Also emit an evidence companion capsule next to the source capsule
        #[arg(long)]
        evidence: bool,

        /// Store the capsules as compressed archives instead of editable file blocks
        #[arg(long)]
        archive: bool,
    },

    /// Pack, inspect, and unpack portable Kain source capsules
    #[command(visible_alias = "a")]
    Amalgamate {
        #[command(subcommand)]
        command: Option<AmalgamateCommand>,

        /// Input file or directory when packing a capsule
        input: Option<PathBuf>,

        /// Output capsule path
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Override the capsule display name
        #[arg(long)]
        name: Option<String>,

        /// Override the capsule version label
        #[arg(long)]
        version: Option<String>,

        /// Repeatable author field
        #[arg(long)]
        author: Vec<String>,

        /// Repeatable free-form note
        #[arg(long = "note")]
        notes: Vec<String>,

        /// Repeatable tag
        #[arg(long)]
        tag: Vec<String>,

        /// Repeatable arbitrary metadata key=value
        #[arg(long = "meta")]
        meta: Vec<String>,

        /// Capsule content policy: source, snapshot, assets, artifacts, or evidence
        #[arg(long, default_value = "source")]
        contents: String,

        /// Optional sibling capsule set name for automatic sidecar discovery
        #[arg(long = "capsule-set")]
        capsule_set: Option<String>,

        /// Store a compressed archive payload instead of inline editable file blocks
        #[arg(long)]
        archive: bool,

        /// Emit raw Kain source without capsule sentinels (no //!kain-file wrapping)
        #[arg(long)]
        raw: bool,

        /// Header rendering mode: minimal, rich, or off
        #[arg(long, default_value = "rich")]
        header: String,

        /// Maximum number of preview symbols rendered in the header
        #[arg(long = "preview-symbols", default_value_t = 40)]
        preview_symbols: usize,

        /// Payload compression mode: zstd or none
        #[arg(long, default_value = "zstd")]
        compression: String,

        /// Public API preview index mode: auto or off
        #[arg(long = "api-index", default_value = "auto")]
        api_index: String,

        /// Module preview index mode: auto or off
        #[arg(long = "module-index", default_value = "auto")]
        module_index: String,
    },

    // ── Import Commands ────────────────────────────────────────
    /// Import grouped foreign source workflows
    Import {
        #[command(subcommand)]
        command: ImportCommand,
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

    // ── Tooling Commands ───────────────────────────────────────
    /// Start the Language Server
    Lsp,

    /// Show or initialize the Kain config control plane
    Config {
        #[command(subcommand)]
        command: ConfigCommand,
    },

    /// Run self-host bootstrap workflows
    Selfhost {
        #[command(subcommand)]
        command: SelfHostCommand,
    },

    /// Generate, print, or check the compiler-owned stdlib symbol atlas
    #[command(name = "stdlib-map")]
    StdlibMap {
        /// Repo root. Defaults to auto-discovery from the current directory.
        #[arg(long)]
        repo_root: Option<PathBuf>,

        /// Stdlib source root. Defaults to <repo>/stdlib.
        #[arg(long)]
        stdlib_root: Option<PathBuf>,

        /// Native runtime manifest to include. Repeatable.
        #[arg(long = "native-manifest")]
        native_manifests: Vec<PathBuf>,

        /// JSON output path for --write/--check.
        #[arg(long)]
        json_out: Option<PathBuf>,

        /// LLM markdown output path for --write/--check.
        #[arg(long)]
        llm_out: Option<PathBuf>,

        /// Rewrite checked-in generated atlas files.
        #[arg(long, conflicts_with = "check")]
        write: bool,

        /// Fail if checked-in generated atlas files are stale.
        #[arg(long, conflicts_with = "write")]
        check: bool,

        /// Print JSON instead of LLM markdown when not writing/checking.
        #[arg(long)]
        json: bool,
    },

    /// Inspect and export the command registry.
    Commands {
        #[command(subcommand)]
        command: RegistryCommand,
    },

    // ── Runtime & Platform ─────────────────────────────────────
    /// Build and validate the manifest-driven native runtime bundle
    Runtime {
        #[command(subcommand)]
        command: RuntimeCommand,
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
        command: CodebaseCommand,
    },

    // ── Specialized Commands ───────────────────────────────────
    /// Generate paired GPU artifacts (SPIR-V, Rust host wrappers, reflection JSON)
    GpuArtifacts {
        input: PathBuf,

        /// Output base path for generated GPU artifacts
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Restrict artifact output to a specific target: spirv, cuda, hlsl, all (default: all)
        #[arg(long, default_value = "all")]
        target: String,

        /// Skip compute residency sidecar generation (the .json + .bin staging files)
        #[arg(long)]
        no_residency: bool,

        /// Skip derived cross-target artifacts (HLSL from SPIR-V, PTX from SPIR-V)
        #[arg(long)]
        no_derived: bool,
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

    /// Build mixed-language omni manifests through the dedicated orchestration layer
    Omni {
        #[command(subcommand)]
        command: OmniCommand,
    },

    /// Validate and scaffold local-first Fabric manifests for polyglot execution
    Fabric {
        #[command(subcommand)]
        command: FabricCommand,
    },

    /// Install C/C++ toolchain extras: auto-resolves vcpkg dependencies, sets up
    /// C build tooling, and materializes headers + libs into .kain/toolchain.
    #[command(name = "install-c-extras")]
    InstallCextras {
        /// Specific packages to install (space-separated). If omitted, resolves all
        /// C FFI dependencies declared in the current workspace.
        packages: Vec<String>,

        /// Target triple to resolve packages for
        #[arg(long, default_value = "host")]
        target: String,

        /// Dry run: print the resolved package plan without installing
        #[arg(long)]
        dry_run: bool,
    },

    /// Runtime-contributed command fallback.
    #[command(external_subcommand)]
    External(Vec<String>),
}

#[cfg(test)]
mod tests {
    use super::*;
    use clap::Parser;

    #[test]
    fn parses_global_config_overrides() {
        let cli = KainCli::parse_from([
            "kain",
            "--config",
            "custom.toml",
            "--color",
            "always",
            "--theme",
            "lattice",
            "check",
            "main.kn",
        ]);
        assert_eq!(cli.config, Some(PathBuf::from("custom.toml")));
        assert_eq!(cli.color, Some(CliColorArg::Always));
        assert_eq!(cli.theme.as_deref(), Some("lattice"));
        match cli.command {
            Some(KainCommand::Check { input, .. }) => {
                assert_eq!(input, PathBuf::from("main.kn"));
            }
            other => panic!("expected check command, got {other:?}"),
        }
    }

    #[test]
    fn parses_check_stdout_json_flag() {
        let cli = KainCli::parse_from(["kain", "check", "main.kn", "--json"]);
        match cli.command {
            Some(KainCommand::Check {
                json, json_out, ..
            }) => {
                assert!(json);
                assert_eq!(json_out, None);
            }
            other => panic!("expected check command, got {other:?}"),
        }
    }

    #[test]
    fn parses_check_json_out_path() {
        let cli = KainCli::parse_from(["kain", "check", "main.kn", "--json-out", "report.json"]);
        match cli.command {
            Some(KainCommand::Check {
                json, json_out, ..
            }) => {
                assert!(!json);
                assert_eq!(json_out, Some(PathBuf::from("report.json")));
            }
            other => panic!("expected check command, got {other:?}"),
        }
    }

    #[test]
    fn parses_check_pedantic_flag() {
        let cli = KainCli::parse_from(["kain", "check", "main.kn", "--pedantic"]);
        match cli.command {
            Some(KainCommand::Check { pedantic, .. }) => {
                assert!(pedantic);
            }
            other => panic!("expected check command, got {other:?}"),
        }
    }

    #[test]
    fn parses_check_pedantic_default_false() {
        let cli = KainCli::parse_from(["kain", "check", "main.kn"]);
        match cli.command {
            Some(KainCommand::Check { pedantic, .. }) => {
                assert!(!pedantic);
            }
            other => panic!("expected check command, got {other:?}"),
        }
    }

    #[test]
    fn parses_check_audit_flag() {
        let cli = KainCli::parse_from(["kain", "check", "main.kn", "--audit"]);
        match cli.command {
            Some(KainCommand::Check { audit, .. }) => {
                assert!(audit);
            }
            other => panic!("expected check command, got {other:?}"),
        }
    }

    #[test]
    fn parses_config_init_command() {
        let cli = KainCli::parse_from(["kain", "config", "init", "--path", "alt.toml", "--force"]);
        match cli.command {
            Some(KainCommand::Config {
                command: ConfigCommand::Init { path, force },
            }) => {
                assert_eq!(path, Some(PathBuf::from("alt.toml")));
                assert!(force);
            }
            other => panic!("expected config init command, got {other:?}"),
        }
    }

    #[test]
    fn parses_config_show_command() {
        let cli = KainCli::parse_from(["kain", "config", "show", "--json"]);
        match cli.command {
            Some(KainCommand::Config {
                command: ConfigCommand::Show { json },
            }) => assert!(json),
            other => panic!("expected config show command, got {other:?}"),
        }
    }

    #[test]
    fn parses_config_set_command() {
        let cli = KainCli::parse_from(["kain", "config", "set", "build.jobs", "all"]);
        match cli.command {
            Some(KainCommand::Config {
                command: ConfigCommand::Set { key, value },
            }) => {
                assert_eq!(key, "build.jobs");
                assert_eq!(value, "all");
            }
            other => panic!("expected config set command, got {other:?}"),
        }
    }

    #[test]
    fn parses_build_native_ui_command() {
        let cli = KainCli::parse_from([
            "kain",
            "build",
            "native-ui",
            "app.kn",
            "--host",
            "tauri",
            "--clean",
        ]);
        match cli.command {
            Some(KainCommand::Build {
                command:
                    Some(BuildCommand::NativeUi {
                        input, host, clean, ..
                    }),
                ..
            }) => {
                assert_eq!(input, PathBuf::from("app.kn"));
                assert_eq!(host, "tauri");
                assert!(clean);
            }
            other => panic!("expected build native-ui command, got {other:?}"),
        }
    }

    #[test]
    fn parses_build_clean_flag() {
        let cli = KainCli::parse_from(["kain", "build", "demo.kn", "--clean"]);
        match cli.command {
            Some(KainCommand::Build {
                input,
                clean,
                command,
                ..
            }) => {
                assert_eq!(input, Some(PathBuf::from("demo.kn")));
                assert!(clean);
                assert!(command.is_none());
            }
            other => panic!("expected build command, got {other:?}"),
        }
    }

    #[test]
    fn parses_build_project_path() {
        let cli = KainCli::parse_from(["kain", "build", "smoketest", "--clean"]);
        match cli.command {
            Some(KainCommand::Build {
                input,
                clean,
                command,
                ..
            }) => {
                assert_eq!(input, Some(PathBuf::from("smoketest")));
                assert!(clean);
                assert!(command.is_none());
            }
            other => panic!("expected project build command, got {other:?}"),
        }
    }

    #[test]
    fn parses_clean_command() {
        let cli = KainCli::parse_from(["kain", "clean", "smoketest", "--scope", "build"]);
        match cli.command {
            Some(KainCommand::Clean { path, scope, .. }) => {
                assert_eq!(path, PathBuf::from("smoketest"));
                assert_eq!(scope, "build");
            }
            other => panic!("expected clean command, got {other:?}"),
        }
    }

    #[test]
    fn parses_runtime_build_command() {
        let cli = KainCli::parse_from(["kain", "runtime", "build", "--release", "--verbose"]);
        match cli.command {
            Some(KainCommand::Runtime {
                command: RuntimeCommand::Build { release, verbose },
            }) => {
                assert!(release);
                assert!(verbose);
            }
            other => panic!("expected runtime build command, got {other:?}"),
        }
    }

    #[test]
    fn parses_runtime_validate_command() {
        let cli = KainCli::parse_from([
            "kain",
            "runtime",
            "validate",
            "--skip-cli-build",
            "--skip-runtime-build",
            "--skip-fixtures",
            "--skip-conformance",
        ]);
        match cli.command {
            Some(KainCommand::Runtime {
                command:
                    RuntimeCommand::Validate {
                        skip_cli_build,
                        skip_runtime_build,
                        skip_fixtures,
                        skip_conformance,
                        ..
                    },
            }) => {
                assert!(skip_cli_build);
                assert!(skip_runtime_build);
                assert!(skip_fixtures);
                assert!(skip_conformance);
            }
            other => panic!("expected runtime validate command, got {other:?}"),
        }
    }

    #[test]
    fn parses_add_command() {
        let cli = KainCli::parse_from([
            "kain",
            "add",
            "kaintana",
            "--version",
            "0.3.0",
            "--manifest",
            "demo/KAIN.toml",
        ]);
        match cli.command {
            Some(KainCommand::Add {
                package,
                version,
                manifest,
            }) => {
                assert_eq!(package, "kaintana");
                assert_eq!(version.as_deref(), Some("0.3.0"));
                assert_eq!(manifest, Some(PathBuf::from("demo/KAIN.toml")));
            }
            other => panic!("expected add command, got {other:?}"),
        }
    }

    #[test]
    fn parses_publish_command() {
        let cli = KainCli::parse_from([
            "kain",
            "publish",
            "blades/kaintana",
            "--output",
            "dist/kaintana.kn",
            "--artifacts",
            "--evidence",
        ]);
        match cli.command {
            Some(KainCommand::Publish {
                input,
                output,
                artifacts,
                evidence,
                ..
            }) => {
                assert_eq!(input, PathBuf::from("blades/kaintana"));
                assert_eq!(output, Some(PathBuf::from("dist/kaintana.kn")));
                assert!(artifacts);
                assert!(evidence);
            }
            other => panic!("expected publish command, got {other:?}"),
        }
    }

    #[test]
    fn parses_format_alias() {
        let cli = KainCli::parse_from(["kn", "fmt", "main.kn", "--check"]);
        match cli.command {
            Some(KainCommand::Format { inputs, check, .. }) => {
                assert_eq!(inputs, vec![PathBuf::from("main.kn")]);
                assert!(check);
            }
            other => panic!("expected format command, got {other:?}"),
        }
    }

    #[test]
    fn parses_format_multiple_inputs() {
        let cli = KainCli::parse_from(["kain", "format", "src", "stdlib/fmt.kn", "--write"]);
        match cli.command {
            Some(KainCommand::Format { inputs, write, .. }) => {
                assert_eq!(
                    inputs,
                    vec![PathBuf::from("src"), PathBuf::from("stdlib/fmt.kn")]
                );
                assert!(write);
            }
            other => panic!("expected format command, got {other:?}"),
        }
    }

    #[test]
    fn parses_import_crates_blades_command() {
        let cli = KainCli::parse_from([
            "kain",
            "import",
            "crates",
            "workspace",
            "--source-root",
            "rust",
            "--output",
            "out/blades",
            "--blades",
            "--include",
            "src",
        ]);
        match cli.command {
            Some(KainCommand::Import {
                command:
                    ImportCommand::Crates {
                        path,
                        source_root,
                        output,
                        blades,
                        include_filters,
                        ..
                    },
            }) => {
                assert_eq!(path, Some(PathBuf::from("workspace")));
                assert_eq!(source_root, Some(PathBuf::from("rust")));
                assert_eq!(output, Some(PathBuf::from("out/blades")));
                assert!(blades);
                assert_eq!(include_filters, ["src"]);
            }
            other => panic!("expected import crates command, got {other:?}"),
        }
    }

    #[test]
    fn parses_import_platform_command() {
        let cli = KainCli::parse_from([
            "kain",
            "import",
            "platform",
            "vulkan",
            "--provider",
            "system",
            "--sdk",
            "C:/VulkanSDK",
            "--target-triple",
            "x86_64-pc-windows-msvc",
            "--dry-run",
        ]);
        match cli.command {
            Some(KainCommand::Import {
                command:
                    ImportCommand::Platform {
                        package,
                        provider,
                        sdk,
                        target_triple,
                        dry_run,
                        ..
                    },
            }) => {
                assert_eq!(package, "vulkan");
                assert_eq!(provider, "system");
                assert_eq!(sdk, Some(PathBuf::from("C:/VulkanSDK")));
                assert_eq!(target_triple, Some("x86_64-pc-windows-msvc".to_string()));
                assert!(dry_run);
            }
            other => panic!("expected import platform command, got {other:?}"),
        }
    }

    #[test]
    fn parses_unified_run_command() {
        let cli = KainCli::parse_from(["kain", "run", "hello.c", "--target", "c", "--", "one"]);
        match cli.command {
            Some(KainCommand::Run {
                input,
                target,
                args,
                ..
            }) => {
                assert_eq!(input, Some(PathBuf::from("hello.c")));
                assert_eq!(target, "c");
                assert_eq!(args, ["one"]);
            }
            other => panic!("expected run command, got {other:?}"),
        }
    }

    #[test]
    fn parses_run_dev_command() {
        let cli = KainCli::parse_from(["kain", "run", "dev", "app.kn", "--trace"]);
        match cli.command {
            Some(KainCommand::Run {
                command: Some(RunCommand::Dev { input, trace, .. }),
                ..
            }) => {
                assert_eq!(input, Some(PathBuf::from("app.kn")));
                assert!(trace);
            }
            other => panic!("expected run dev command, got {other:?}"),
        }
    }

    #[test]
    fn parses_watch_dry_run_command() {
        let cli = KainCli::parse_from(["kain", "watch", "app.kn", "--dry-run"]);
        match cli.command {
            Some(KainCommand::Watch { input, dry_run, .. }) => {
                assert_eq!(input, Some(PathBuf::from("app.kn")));
                assert!(dry_run);
            }
            other => panic!("expected watch command, got {other:?}"),
        }
    }

    #[test]
    fn parses_amalgamate_pack_command() {
        let cli = KainCli::parse_from([
            "kain",
            "amalgamate",
            "./capsule-probe",
            "-o",
            "capsule.kn",
            "--header",
            "rich",
            "--preview-symbols",
            "16",
        ]);
        match cli.command {
            Some(KainCommand::Amalgamate {
                command: None,
                input,
                output,
                contents,
                capsule_set,
                archive,
                header,
                preview_symbols,
                ..
            }) => {
                assert_eq!(input, Some(PathBuf::from("./capsule-probe")));
                assert_eq!(output, Some(PathBuf::from("capsule.kn")));
                assert_eq!(contents, "source");
                assert_eq!(capsule_set, None);
                assert!(!archive);
                assert_eq!(header, "rich");
                assert_eq!(preview_symbols, 16);
            }
            other => panic!("expected amalgamate pack command, got {other:?}"),
        }
    }

    #[test]
    fn parses_amalgamate_raw_flag() {
        let cli = KainCli::parse_from([
            "kain",
            "amalgamate",
            "./capsule-probe",
            "-o",
            "capsule.kn",
            "--raw",
        ]);
        match cli.command {
            Some(KainCommand::Amalgamate {
                command: None,
                raw,
                ..
            }) => {
                assert!(raw);
            }
            other => panic!("expected amalgamate pack command, got {other:?}"),
        }
    }

    #[test]
    fn parses_amalgamate_archive_flag() {
        let cli = KainCli::parse_from([
            "kain",
            "amalgamate",
            "./capsule-probe",
            "-o",
            "capsule.kn",
            "--archive",
        ]);
        match cli.command {
            Some(KainCommand::Amalgamate {
                command: None,
                archive,
                ..
            }) => {
                assert!(archive);
            }
            other => panic!("expected amalgamate pack command, got {other:?}"),
        }
    }

    #[test]
    fn parses_amalgamate_contents_and_capsule_set() {
        let cli = KainCli::parse_from([
            "kain",
            "amalgamate",
            "./capsule-probe",
            "-o",
            "capsule.kn",
            "--contents",
            "artifacts",
            "--capsule-set",
            "smoketest",
        ]);
        match cli.command {
            Some(KainCommand::Amalgamate {
                command: None,
                contents,
                capsule_set,
                ..
            }) => {
                assert_eq!(contents, "artifacts");
                assert_eq!(capsule_set.as_deref(), Some("smoketest"));
            }
            other => panic!("expected amalgamate pack command, got {other:?}"),
        }
    }

    #[test]
    fn parses_amalgamate_inspect_command() {
        let cli = KainCli::parse_from(["kain", "amalgamate", "inspect", "capsule.kn", "--json"]);
        match cli.command {
            Some(KainCommand::Amalgamate {
                command: Some(AmalgamateCommand::Inspect { input, json }),
                ..
            }) => {
                assert_eq!(input, PathBuf::from("capsule.kn"));
                assert!(json);
            }
            other => panic!("expected amalgamate inspect command, got {other:?}"),
        }
    }

    #[test]
    fn captures_external_runtime_command() {
        let cli = KainCli::parse_from(["kain", "sharpen", "tool", "--fast"]);
        match cli.command {
            Some(KainCommand::External(argv)) => {
                assert_eq!(argv, ["tool", "--fast"]);
            }
            other => panic!("expected external command, got {other:?}"),
        }
    }
}
