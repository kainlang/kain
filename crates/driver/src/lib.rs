//! Embeddable compiler driver for the KAIN toolchain.
//!
//! This crate owns the "glue" between `kain-core`, language-specific backends,
//! and Rust-hosted applications that want to compile KAIN without going
//! through the CLI binary.

use std::cell::RefCell;
use std::collections::{HashMap, HashSet};
use std::fmt;
use std::path::{Path, PathBuf};
#[cfg(test)]
use std::sync::Mutex;
use std::sync::{Arc, OnceLock};
use std::time::Instant;

use kain_core::ast::{
    Block, Expr, Item, JSXNode, Program, ShaderStage, Stmt, Use, WorldSurfaceKind,
};
use kain_core::diagnostics::SourceOriginSegment;
use kain_core::error::KainError;
use kain_core::module_resolution::{
    resolve_filesystem_module_file_with_context, resolve_stdlib_module_file,
    FilesystemModuleResolutionContext,
};
use kain_core::monomorphize::MonomorphizedProgram;
use kain_core::runtime;
use kain_core::{
    comptime, diagnostics, emit_realtime_app_bundle, emit_runtime_contract_bundle, monomorphize,
    realtime_app_bundle_to_json, types, CompileTarget, Lexer, Parser, RealtimeAppBundle,
    ResolvedType, RuntimeContractBundle, ShaderArtifactBundle, TypedItem, TypedProgram,
};
use serde::Deserialize;
use serde::Serialize;

#[cfg(all(feature = "gpu", feature = "sys"))]
use kain_core::{
    bytes_to_hex, shader_artifact_bundle_to_json, DerivedShaderArtifact, PtxArtifactMetadata,
    ShaderArtifactFormat, ShaderDebugBundle, ShaderEntryPoint, ShaderIoField,
    ShaderReflectionShader, ShaderReflectionSummary, ShaderResourceLayout, ShaderSourceMapEntry,
    ShaderSpecializationConstant, ShaderStageMetadata, SpirvModuleArtifact,
    SHADER_ARTIFACT_SCHEMA_VERSION,
};

#[cfg(feature = "sys")]
mod compute_residency;
pub mod llvm_ir;
#[cfg(feature = "sys")]
mod native_app;
#[cfg(feature = "tauri")]
mod tauri_app;

#[cfg(feature = "sys")]
use kain_core::tooling_config::apply_cargo_command_defaults;
#[cfg(feature = "sys")]
use kain_core::Span;

#[cfg(feature = "gpu")]
use gpu;

#[cfg(feature = "sys")]
use kain_sys_codegen as sys;

#[cfg(feature = "sys")]
pub use compute_residency::{
    write_compute_residency_sidecars, ComputeResidencyBinding, ComputeResidencyBundle,
    ComputeResidencyEntry, ComputeResidencyPtxSidecar, COMPUTE_RESIDENCY_ENV_VAR,
    COMPUTE_RESIDENCY_FILE_NAME,
};
#[cfg(feature = "tauri")]
pub use kain_ui_tauri::{TauriCapabilityPreset, TauriPermissionPreset, TauriPluginPreset};
pub use llvm_ir::{
    analyze_llvm_ir_reachability, slice_llvm_native_executable_ir, LlvmIrReachability,
    LlvmIrSliceStats,
};
#[cfg(feature = "sys")]
pub use native_app::{
    compile_native_app_bundle, discover_native_app_root_component, materialize_native_app_bundle,
    NativeAppBundle, NativeAppBundleConfig, NativeAppHostSidecarBinding,
    NativeAppLauncherEntrypoint, NativeAppMaterializationConfig, NativeAppMaterializedPaths,
    NativeAppMetadata, NativeAppRuntimeDependency,
};
#[cfg(feature = "tauri")]
pub use tauri_app::{
    compile_tauri_app_bundle, materialize_tauri_app_bundle, TauriAppBundle, TauriAppBundleConfig,
    TauriAppMaterializationConfig, TauriAppMaterializedPaths,
};

#[cfg(feature = "ue5")]
use ue5;

#[cfg(feature = "ue5")]
use ue5_editor;

#[cfg(feature = "ue5")]
use ue5_shaders;

#[cfg(feature = "web")]
use kain_web;

#[derive(Clone, Copy)]
struct TargetSpec {
    target: CompileTarget,
    extension: &'static str,
    aliases: &'static [&'static str],
}

const TARGET_SPECS: &[TargetSpec] = &[
    TargetSpec {
        target: CompileTarget::Wasm,
        extension: "wasm",
        aliases: &["wasm", "w"],
    },
    TargetSpec {
        target: CompileTarget::Llvm,
        extension: "ll",
        aliases: &["llvm", "native", "n"],
    },
    TargetSpec {
        target: CompileTarget::C,
        extension: "c",
        aliases: &["c"],
    },
    TargetSpec {
        target: CompileTarget::Spirv,
        extension: "spv",
        aliases: &["spirv", "gpu", "shader", "s"],
    },
    TargetSpec {
        target: CompileTarget::Hlsl,
        extension: "hlsl",
        aliases: &["hlsl", "h"],
    },
    TargetSpec {
        target: CompileTarget::Wgsl,
        extension: "wgsl",
        aliases: &["wgsl", "webgpu"],
    },
    TargetSpec {
        target: CompileTarget::Cuda,
        extension: "ptx",
        aliases: &["cuda", "ptx", "nvptx"],
    },
    TargetSpec {
        target: CompileTarget::Usf,
        extension: "usf",
        aliases: &["usf"],
    },
    TargetSpec {
        target: CompileTarget::Js,
        extension: "js",
        aliases: &["js", "javascript", "j"],
    },
    TargetSpec {
        target: CompileTarget::Ts,
        extension: "ts",
        aliases: &["ts", "typescript"],
    },
    TargetSpec {
        target: CompileTarget::Rust,
        extension: "rs",
        aliases: &["rust", "rs"],
    },
    TargetSpec {
        target: CompileTarget::Hybrid,
        extension: "hybrid",
        aliases: &["hybrid", "web"],
    },
    TargetSpec {
        target: CompileTarget::Cpp,
        extension: "cpp",
        aliases: &["cpp", "c++"],
    },
    TargetSpec {
        target: CompileTarget::Ue5,
        extension: "h",
        aliases: &["ue5", "unreal", "u"],
    },
    TargetSpec {
        target: CompileTarget::Ue5Editor,
        extension: "h",
        aliases: &["ue5editor", "ue5-editor", "editor", "slate"],
    },
    TargetSpec {
        target: CompileTarget::Interpret,
        extension: "txt",
        aliases: &["run", "r", "interpret", "i"],
    },
    TargetSpec {
        target: CompileTarget::Test,
        extension: "txt",
        aliases: &["test", "t"],
    },
    TargetSpec {
        target: CompileTarget::Ks,
        extension: "ks",
        aliases: &["ks", "kainscript", "kscript"],
    },
];

#[derive(Debug, Clone)]
pub struct DriverSession {
    ue5_metadata_dir: Option<PathBuf>,
    frontend_cache: RefCell<Option<CachedFrontendBundle>>,
    checked_frontend_cache: RefCell<Option<CachedCheckedFrontend>>,
    last_frontend_bundle: RefCell<Option<FrontendSourceBundle>>,
    frontend_advisories: RefCell<Vec<String>>,
    /// When true, the LLVM backend emits DWARF debug metadata
    /// (!DILocation, !DISubprogram, !DICompileUnit, etc.).
    pub debug_info: bool,
}

impl Default for DriverSession {
    fn default() -> Self {
        Self {
            ue5_metadata_dir: None,
            frontend_cache: RefCell::new(None),
            checked_frontend_cache: RefCell::new(None),
            last_frontend_bundle: RefCell::new(None),
            frontend_advisories: RefCell::new(Vec::new()),
            debug_info: false,
        }
    }
}

#[derive(Debug, Clone)]
pub struct ShaderArtifactBundleOutput {
    pub bundle: ShaderArtifactBundle,
    pub bundle_json: String,
    pub spirv: Vec<u8>,
    pub rust_host: String,
    pub reflection_json: String,
    pub derived_hlsl: Option<String>,
    pub derived_wgsl: Option<String>,
    pub derived_ptx: Option<String>,
}

pub type GpuArtifactOutput = ShaderArtifactBundleOutput;

#[cfg(feature = "sys")]
pub const GPU_RUNTIME_LIBRARY_ENV_VAR: &str = "KAIN_GPU_RUNTIME_LIBRARY";
#[cfg(feature = "sys")]
pub const GPU_RUNTIME_ALLOW_CARGO_BUILD_ENV_VAR: &str = "KAIN_GPU_RUNTIME_ALLOW_CARGO_BUILD";

#[derive(Debug, Clone)]
pub struct RealtimeAppBundleOutput {
    pub bundle: RealtimeAppBundle,
    pub bundle_json: String,
}

#[derive(Debug, Clone)]
pub struct HybridArtifactOutput {
    pub js: String,
    pub ts: String,
    pub wasm: Vec<u8>,
    pub wasm_export_names: Vec<String>,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum ToolingProgressStatus {
    Planned,
    Started,
    Succeeded,
    Failed,
    Skipped,
    Cached,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum CompilerProgressPhase {
    Resolve,
    Parse,
    Comptime,
    Typecheck,
    Monomorphize,
    Codegen,
    Interpret,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(tag = "kind", rename_all = "kebab-case")]
pub enum ToolingProgressEvent {
    CheckDiscoveryStarted {
        root: PathBuf,
        target: String,
    },
    CheckDiscoveryFinished {
        root: PathBuf,
        target: String,
        total_files: usize,
    },
    CheckFileStarted {
        current: usize,
        total: usize,
        path: PathBuf,
        target: String,
    },
    CheckFileFinished {
        current: usize,
        total: usize,
        path: PathBuf,
        target: String,
        status: ToolingProgressStatus,
        error: Option<String>,
    },
    BuildPlanReady {
        workspace_root: PathBuf,
        lane: String,
        target: String,
        total_tasks: usize,
    },
    BuildTaskStarted {
        current: usize,
        total: usize,
        task_id: String,
        description: String,
        task_kind: String,
        blade: Option<String>,
    },
    BuildTaskFinished {
        current: usize,
        total: usize,
        task_id: String,
        description: String,
        task_kind: String,
        blade: Option<String>,
        status: ToolingProgressStatus,
        cache_hit: bool,
        message: String,
        error: Option<String>,
    },
    RunPlanReady {
        workspace_root: PathBuf,
        mode: String,
        target: String,
        total_units: usize,
    },
    RunUnitStarted {
        current: usize,
        total: usize,
        unit_id: String,
        label: String,
        target: String,
    },
    RunUnitFinished {
        current: usize,
        total: usize,
        unit_id: String,
        label: String,
        target: String,
        status: ToolingProgressStatus,
        exit_code: Option<i64>,
        error: Option<String>,
    },
    RunHandOff {
        unit_id: String,
        label: String,
        target: String,
        command: Option<String>,
    },
    CompilerPhase {
        source_path: Option<PathBuf>,
        target: String,
        phase: CompilerProgressPhase,
    },
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ToolingProgressRecord {
    pub unix_ms: u128,
    #[serde(flatten)]
    pub event: ToolingProgressEvent,
}

impl ToolingProgressRecord {
    pub fn new(unix_ms: u128, event: ToolingProgressEvent) -> Self {
        Self { unix_ms, event }
    }
}

#[derive(Clone)]
pub struct ToolingProgressSink {
    inner: Arc<dyn Fn(&ToolingProgressEvent) + Send + Sync>,
}

impl ToolingProgressSink {
    pub fn new<F>(callback: F) -> Self
    where
        F: Fn(&ToolingProgressEvent) + Send + Sync + 'static,
    {
        Self {
            inner: Arc::new(callback),
        }
    }

    pub fn emit(&self, event: &ToolingProgressEvent) {
        (self.inner)(event);
    }
}

impl fmt::Debug for ToolingProgressSink {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("ToolingProgressSink(..)")
    }
}

#[derive(Debug, Clone)]
pub struct CheckedFrontend {
    pub ast: Program,
    pub typed: TypedProgram,
}

#[derive(Debug, Clone, Default)]
struct ResolvedWorldSelection {
    root_component: Option<String>,
    active_world_name: Option<String>,
}

#[derive(Debug, Clone)]
struct WorldSelectionInfo {
    name: String,
    surfaces: Vec<WorldSurfaceSelection>,
}

#[derive(Debug, Clone)]
struct WorldSurfaceSelection {
    kind: WorldSurfaceKind,
    root_component: Option<String>,
}

#[derive(Debug, Clone)]
struct FrontendSourceBundle {
    full_source: String,
    origins: Vec<SourceOriginSegment>,
    watch_inputs: Vec<PathBuf>,
}

#[derive(Debug, Default)]
struct FrontendImportCollector {
    visited_module_files: HashSet<PathBuf>,
    module_sources: Vec<FrontendSourceUnit>,
    entry_path: Option<PathBuf>,
    advisories: Vec<String>,
}

#[derive(Debug, Clone)]
struct FrontendSourceUnit {
    file_path: PathBuf,
    source: String,
}

#[derive(Debug, Clone)]
struct CachedFrontendBundle {
    target: CompileTarget,
    source_path: Option<PathBuf>,
    source: String,
    advisories: Vec<String>,
    watch_inputs: Vec<FrontendWatchedInput>,
    bundle: FrontendSourceBundle,
}

#[derive(Debug, Clone)]
struct CachedCheckedFrontend {
    target: CompileTarget,
    source_path: Option<PathBuf>,
    source: String,
    watch_inputs: Vec<FrontendWatchedInput>,
    checked: CheckedFrontend,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct FrontendWatchedInput {
    path: PathBuf,
    modified_unix_ms: Option<u128>,
    byte_len: Option<u64>,
}

#[derive(Debug, Deserialize)]
struct StdlibLookupMap {
    modules: Vec<StdlibLookupModule>,
}

#[derive(Debug, Deserialize)]
struct StdlibLookupModule {
    import_path: String,
    symbols: Vec<StdlibLookupSymbol>,
}

#[derive(Debug, Deserialize)]
struct StdlibLookupSymbol {
    name: String,
    visibility: String,
}

const AMBIENT_STDLIB_MODULES: &[&str] = &["runtime", "actor"];
static ROOT_STDLIB_SYMBOL_MODULE_LOOKUP: OnceLock<HashMap<String, String>> = OnceLock::new();
static STDLIB_MAP_PATH: OnceLock<Option<PathBuf>> = OnceLock::new();

#[cfg(feature = "sys")]
#[derive(Debug, Clone)]
pub struct RustBundleOutput {
    pub bundle: sys::RustArtifactBundle,
    pub spirv: Option<Vec<u8>>,
}

impl DriverSession {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn with_ue5_metadata_dir(mut self, path: impl Into<PathBuf>) -> Self {
        self.ue5_metadata_dir = Some(path.into());
        self
    }

    /// Enable DWARF debug metadata in the LLVM backend (`-g` / `--debug`).
    pub fn with_debug_info(mut self, debug_info: bool) -> Self {
        self.debug_info = debug_info;
        self
    }

    pub fn set_ue5_metadata_dir(&mut self, path: impl Into<PathBuf>) {
        self.ue5_metadata_dir = Some(path.into());
    }

    pub fn frontend_to_checked_program(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<CheckedFrontend, KainError> {
        self.frontend_to_checked_program_with_source_path(source, None, target)
    }

    pub fn frontend_to_checked_program_with_source_path(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
    ) -> Result<CheckedFrontend, KainError> {
        self.frontend_to_checked_program_with_source_path_and_progress(
            source,
            source_path,
            target,
            None,
        )
    }

    pub fn frontend_to_checked_program_with_source_path_and_progress(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<CheckedFrontend, KainError> {
        self.frontend_to_checked_program_with_extra_globals_and_progress(
            source,
            source_path,
            target,
            std::iter::empty::<(String, ResolvedType)>(),
            progress,
        )
    }

    pub fn frontend_to_checked_program_with_extra_globals<I>(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        extra_globals: I,
    ) -> Result<CheckedFrontend, KainError>
    where
        I: IntoIterator<Item = (String, ResolvedType)>,
    {
        self.frontend_to_checked_program_with_extra_globals_and_progress(
            source,
            source_path,
            target,
            extra_globals,
            None,
        )
    }

    pub fn frontend_to_checked_program_with_extra_globals_and_progress<I>(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        extra_globals: I,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<CheckedFrontend, KainError>
    where
        I: IntoIterator<Item = (String, ResolvedType)>,
    {
        let extra_globals = extra_globals.into_iter().collect::<Vec<_>>();
        let can_cache_checked = extra_globals.is_empty();
        if can_cache_checked {
            if let Some(cached) =
                try_reuse_cached_checked_frontend(self, source, source_path, target)
            {
                return Ok(cached.checked);
            }
        }

        register_frontend_extensions_for_target(target);
        emit_compiler_phase(
            progress,
            source_path,
            target,
            CompilerProgressPhase::Resolve,
        );
        let frontend = build_frontend_source_bundle(self, source, source_path, target)?;

        emit_compiler_phase(progress, source_path, target, CompilerProgressPhase::Parse);
        let tokens = Lexer::new(&frontend.full_source).tokenize()?;
        let span_mapper =
            diagnostics::SpanMapper::with_origins(&frontend.full_source, frontend.origins.clone());
        let filename = source_path
            .map(|path| path.display().to_string())
            .unwrap_or_else(|| "<input>".to_string());
        let mut ast = Parser::new(&tokens, &span_mapper, &filename).parse()?;
        emit_compiler_phase(
            progress,
            source_path,
            target,
            CompilerProgressPhase::Comptime,
        );
        comptime::eval_program(&mut ast)?;
        emit_compiler_phase(
            progress,
            source_path,
            target,
            CompilerProgressPhase::Typecheck,
        );
        let typed = types::check_with_extra_globals(&ast, &span_mapper, &filename, extra_globals)?;
        let checked = CheckedFrontend { ast, typed };

        if can_cache_checked {
            let watch_inputs = self
                .last_frontend_bundle
                .borrow()
                .as_ref()
                .map(|bundle| fingerprint_watch_inputs(&bundle.watch_inputs))
                .unwrap_or_default();
            self.checked_frontend_cache
                .replace(Some(CachedCheckedFrontend {
                    target,
                    source_path: source_path.map(|path| path.to_path_buf()),
                    source: source.to_string(),
                    watch_inputs,
                    checked: checked.clone(),
                }));
        }

        Ok(checked)
    }

    pub fn frontend_to_monomorphized_program(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<MonomorphizedProgram, KainError> {
        self.frontend_to_monomorphized_program_with_source_path(source, None, target)
    }

    pub fn frontend_to_monomorphized_program_with_source_path(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
    ) -> Result<MonomorphizedProgram, KainError> {
        self.frontend_to_monomorphized_program_with_source_path_and_progress(
            source,
            source_path,
            target,
            None,
        )
    }

    pub fn frontend_to_monomorphized_program_with_source_path_and_progress(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<MonomorphizedProgram, KainError> {
        let checked = self.frontend_to_checked_program_with_source_path_and_progress(
            source,
            source_path,
            target,
            progress,
        )?;
        emit_compiler_phase(
            progress,
            source_path,
            target,
            CompilerProgressPhase::Monomorphize,
        );
        monomorphize::monomorphize(&checked.typed)
    }

    pub fn frontend_to_typed_program(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<TypedProgram, KainError> {
        self.frontend_to_typed_program_with_source_path(source, None, target)
    }

    pub fn frontend_to_typed_program_with_source_path(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
    ) -> Result<TypedProgram, KainError> {
        self.frontend_to_typed_program_with_source_path_and_progress(
            source,
            source_path,
            target,
            None,
        )
    }

    pub fn frontend_to_typed_program_with_source_path_and_progress(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<TypedProgram, KainError> {
        Ok(self
            .frontend_to_checked_program_with_source_path_and_progress(
                source,
                source_path,
                target,
                progress,
            )?
            .typed)
    }

    fn frontend_to_sys_codegen_program_with_source_path_and_progress(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<TypedProgram, KainError> {
        let mono = self.frontend_to_monomorphized_program_with_source_path_and_progress(
            source,
            source_path,
            target,
            progress,
        )?;
        Ok(TypedProgram { items: mono.items })
    }

    pub fn frontend_to_typed_program_with_extra_globals<I>(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        extra_globals: I,
    ) -> Result<TypedProgram, KainError>
    where
        I: IntoIterator<Item = (String, ResolvedType)>,
    {
        Ok(self
            .frontend_to_checked_program_with_extra_globals_and_progress(
                source,
                source_path,
                target,
                extra_globals,
                None,
            )?
            .typed)
    }

    pub fn compile_runtime_contract_bundle(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<RuntimeContractBundle, KainError> {
        self.compile_runtime_contract_bundle_with_source_path(source, None, target)
    }

    pub fn compile_runtime_contract_bundle_with_source_path(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
    ) -> Result<RuntimeContractBundle, KainError> {
        let typed = self.frontend_to_typed_program_with_source_path(source, source_path, target)?;
        Ok(emit_runtime_contract_bundle(&typed, target))
    }

    pub fn format_source(&self, source: &str) -> Result<String, KainError> {
        kain_fmt::format_source(source)
    }

    pub fn format_error(
        &self,
        fallback_source_name: &str,
        fallback_source: &str,
        error: &KainError,
    ) -> String {
        if let Some(bundle) = self.last_frontend_bundle.borrow().as_ref() {
            let mapper =
                diagnostics::SpanMapper::with_origins(&bundle.full_source, bundle.origins.clone());
            let diag = diagnostics::Diagnostics::with_mapper(mapper, fallback_source_name);
            return diag.format_error(error);
        }
        diagnostics::Diagnostics::new(fallback_source, fallback_source_name).format_error(error)
    }

    pub fn frontend_watch_inputs(&self) -> Vec<PathBuf> {
        self.last_frontend_bundle
            .borrow()
            .as_ref()
            .map(|bundle| bundle.watch_inputs.clone())
            .unwrap_or_default()
    }

    pub fn frontend_full_source(&self) -> Option<String> {
        self.last_frontend_bundle
            .borrow()
            .as_ref()
            .map(|bundle| bundle.full_source.clone())
    }

    pub fn frontend_advisories(&self) -> Vec<String> {
        self.frontend_advisories.borrow().clone()
    }

    pub fn compile_realtime_app_bundle(
        &self,
        source: &str,
        target: CompileTarget,
        root_component: Option<&str>,
    ) -> Result<RealtimeAppBundleOutput, KainError> {
        self.compile_realtime_app_bundle_with_source_path(source, None, target, root_component)
    }

    pub fn compile_realtime_app_bundle_with_source_path(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        root_component: Option<&str>,
    ) -> Result<RealtimeAppBundleOutput, KainError> {
        let trace_realtime_bundle = std::env::var("KAIN_DRIVER_TRACE_REALTIME_BUNDLE")
            .map(|value| {
                let normalized = value.trim().to_ascii_lowercase();
                !normalized.is_empty() && normalized != "0" && normalized != "false"
            })
            .unwrap_or(false);
        let trace_started = Instant::now();
        if trace_realtime_bundle {
            eprintln!(
                "[kain-driver][realtime] start target={target:?} root_component={}",
                root_component.unwrap_or("<auto>")
            );
        }
        let typed = self.frontend_to_typed_program_with_source_path(source, source_path, target)?;
        if trace_realtime_bundle {
            eprintln!(
                "[kain-driver][realtime] typed_program_ms={}",
                trace_started.elapsed().as_millis()
            );
        }
        let resolved_world = resolve_world_selection(&typed, target, root_component)?;
        if trace_realtime_bundle {
            eprintln!(
                "[kain-driver][realtime] resolved_world_ms={} active_world={} root_component={}",
                trace_started.elapsed().as_millis(),
                resolved_world
                    .active_world_name
                    .as_deref()
                    .unwrap_or("<none>"),
                resolved_world.root_component.as_deref().unwrap_or("<none>")
            );
        }
        let ui_source = if let Some(bundle_source) = self
            .last_frontend_bundle
            .borrow()
            .as_ref()
            .map(|bundle| bundle.full_source.clone())
        {
            bundle_source
        } else {
            prepare_frontend_source_for_target(source, source_path, target)?
        };
        if trace_realtime_bundle {
            eprintln!(
                "[kain-driver][realtime] ui_source_ms={} bytes={}",
                trace_started.elapsed().as_millis(),
                ui_source.len()
            );
        }
        let ui_output = if let Some(root_component) = resolved_world.root_component.as_deref() {
            Some(build_ui_output_from_frontend_bundle_source(
                &ui_source,
                root_component,
            )?)
        } else {
            None
        };
        if trace_realtime_bundle {
            eprintln!(
                "[kain-driver][realtime] ui_output_ms={} has_ui={}",
                trace_started.elapsed().as_millis(),
                ui_output.is_some()
            );
        }
        let mut bundle = emit_realtime_app_bundle(&typed, ui_output.as_ref(), target);
        if trace_realtime_bundle {
            eprintln!(
                "[kain-driver][realtime] emit_bundle_ms={}",
                trace_started.elapsed().as_millis()
            );
        }
        apply_active_world_selection_to_realtime_bundle(
            &mut bundle,
            resolved_world.active_world_name.as_deref(),
        )?;
        if trace_realtime_bundle {
            eprintln!(
                "[kain-driver][realtime] active_world_ms={}",
                trace_started.elapsed().as_millis()
            );
        }
        let bundle_json = realtime_app_bundle_to_json(&bundle).map_err(|err| {
            KainError::runtime(format!(
                "Failed to serialize realtime app bundle JSON: {err}"
            ))
        })?;
        if trace_realtime_bundle {
            eprintln!(
                "[kain-driver][realtime] bundle_json_ms={} bytes={}",
                trace_started.elapsed().as_millis(),
                bundle_json.len()
            );
        }
        Ok(RealtimeAppBundleOutput {
            bundle,
            bundle_json,
        })
    }

    pub fn compile(&self, source: &str, target: CompileTarget) -> Result<String, KainError> {
        self.compile_with_source_path(source, None, target)
    }

    pub fn compile_with_source_path(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
    ) -> Result<String, KainError> {
        self.compile_with_source_path_and_progress(source, source_path, target, None)
    }

    pub fn compile_with_source_path_and_progress(
        &self,
        source: &str,
        source_path: Option<&Path>,
        target: CompileTarget,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<String, KainError> {
        match target {
            #[cfg(feature = "ue5")]
            CompileTarget::Ue5 => {
                let mono_for_codegen = self
                    .frontend_to_monomorphized_program_with_source_path_and_progress(
                        source,
                        source_path,
                        target,
                        progress,
                    )?;
                emit_compiler_phase(
                    progress,
                    source_path,
                    target,
                    CompilerProgressPhase::Codegen,
                );
                let output = ue5::generate(&mono_for_codegen, None, None)?;
                Ok(format!("{}\n{}", output.header, output.source))
            }
            _ => {
                #[allow(unused_variables)]
                let typed_for_codegen = match target {
                    #[cfg(feature = "sys")]
                    CompileTarget::C
                    | CompileTarget::Llvm
                    | CompileTarget::Rust
                    | CompileTarget::Cpp => self
                        .frontend_to_sys_codegen_program_with_source_path_and_progress(
                            source,
                            source_path,
                            target,
                            progress,
                        )?,
                    _ => self.frontend_to_typed_program_with_source_path_and_progress(
                        source,
                        source_path,
                        target,
                        progress,
                    )?,
                };

                match target {
                    #[cfg(feature = "ue5")]
                    CompileTarget::Usf => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        ue5_shaders::generate_usf(&typed_for_codegen)
                    }

                    #[cfg(feature = "gpu")]
                    CompileTarget::Spirv => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        gpu::generate_spirv(&typed_for_codegen)
                            .map(|bytes| format!("{} bytes", bytes.len()))
                    }

                    #[cfg(feature = "gpu")]
                    CompileTarget::Hlsl => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        gpu::generate_hlsl(&typed_for_codegen)
                    }

                    #[cfg(feature = "gpu")]
                    CompileTarget::Wgsl => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        gpu::generate_wgsl(&typed_for_codegen)
                    }

                    #[cfg(feature = "gpu")]
                    CompileTarget::Cuda => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        gpu::generate_ptx(&typed_for_codegen)
                    }

                    #[cfg(feature = "web")]
                    CompileTarget::Wasm => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        kain_web::generate_wasm(&typed_for_codegen)
                            .map(|bytes| format!("{} bytes", bytes.len()))
                    }

                    #[cfg(feature = "web")]
                    CompileTarget::Js => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        kain_web::generate_js(&typed_for_codegen)
                    }

                    #[cfg(feature = "web")]
                    CompileTarget::Ts => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        kain_web::generate_ts(&typed_for_codegen)
                    }

                    #[cfg(feature = "web")]
                    CompileTarget::Ks => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        kain_web::generate_ks(&typed_for_codegen)
                    }

                    #[cfg(feature = "web")]
                    CompileTarget::Hybrid => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        let output = kain_web::generate_hybrid(&typed_for_codegen)?;
                        Ok(output.js)
                    }

                    #[cfg(feature = "sys")]
                    CompileTarget::C => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        sys::generate_c(&typed_for_codegen)
                    }

                    #[cfg(feature = "sys")]
                    CompileTarget::Llvm => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        if self.debug_info {
                            let filename = source_path
                                .and_then(|p| p.file_name())
                                .and_then(|n| n.to_str())
                                .unwrap_or("unknown.kn");
                            sys::generate_with_debug(&typed_for_codegen, source, filename)
                                .and_then(|bytes| {
                                    String::from_utf8(bytes).map_err(|err| {
                                        KainError::codegen(
                                            format!("LLVM output was not valid UTF-8: {err}"),
                                            Span::default(),
                                        )
                                    })
                                })
                        } else {
                            sys::generate_llvm(&typed_for_codegen).and_then(|bytes| {
                                String::from_utf8(bytes).map_err(|err| {
                                    KainError::codegen(
                                        format!("LLVM output was not valid UTF-8: {err}"),
                                        Span::default(),
                                    )
                                })
                            })
                        }
                    }

                    #[cfg(feature = "sys")]
                    CompileTarget::Rust => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        sys::generate_rust(&typed_for_codegen)
                    }

                    #[cfg(feature = "sys")]
                    CompileTarget::Cpp => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Codegen,
                        );
                        sys::generate_cpp(&typed_for_codegen)
                    }

                    CompileTarget::Interpret => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Interpret,
                        );
                        let value = runtime::interpret(&typed_for_codegen)?;
                        Ok(value.to_string())
                    }

                    CompileTarget::Test => {
                        emit_compiler_phase(
                            progress,
                            source_path,
                            target,
                            CompilerProgressPhase::Interpret,
                        );
                        runtime::run_tests(&typed_for_codegen)?;
                        Ok("Tests passed".to_string())
                    }

                    #[cfg(feature = "ue5")]
                    CompileTarget::Ue5Editor => {
                        let output = ue5_editor::generate(&typed_for_codegen, "EditorTools", None)?;
                        Ok(format!("{}\n{}", output.header, output.source))
                    }

                    #[cfg(not(feature = "ue5"))]
                    CompileTarget::Ue5Editor => {
                        Err(KainError::runtime("UE5 Editor target requires ue5 feature"))
                    }

                    #[allow(unreachable_patterns)]
                    _ => Err(KainError::runtime(format!(
                        "Target {:?} not enabled. Recompile with appropriate feature flag.",
                        target
                    ))),
                }
            }
        }
    }

    #[cfg(feature = "gpu")]
    pub fn compile_spirv_binary(&self, source: &str) -> Result<Vec<u8>, KainError> {
        self.compile_spirv_binary_with_progress(source, None)
    }

    #[cfg(feature = "gpu")]
    pub fn compile_spirv_binary_with_progress(
        &self,
        source: &str,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<Vec<u8>, KainError> {
        let typed_for_codegen = self.frontend_to_typed_program_with_source_path_and_progress(
            source,
            None,
            CompileTarget::Spirv,
            progress,
        )?;
        emit_compiler_phase(
            progress,
            None,
            CompileTarget::Spirv,
            CompilerProgressPhase::Codegen,
        );
        gpu::generate_spirv(&typed_for_codegen)
    }

    #[cfg(not(feature = "gpu"))]
    pub fn compile_spirv_binary(&self, _source: &str) -> Result<Vec<u8>, KainError> {
        Err(KainError::runtime("SPIR-V target requires gpu feature"))
    }

    #[cfg(feature = "gpu")]
    pub fn compile_ptx_source(&self, source: &str) -> Result<String, KainError> {
        let typed_for_codegen = self.frontend_to_typed_program(source, CompileTarget::Cuda)?;
        gpu::generate_ptx(&typed_for_codegen)
    }

    #[cfg(not(feature = "gpu"))]
    pub fn compile_ptx_source(&self, _source: &str) -> Result<String, KainError> {
        Err(KainError::runtime("CUDA/PTX target requires gpu feature"))
    }

    #[cfg(feature = "web")]
    pub fn compile_wasm_binary(&self, source: &str) -> Result<Vec<u8>, KainError> {
        self.compile_wasm_binary_with_progress(source, None)
    }

    #[cfg(feature = "web")]
    pub fn compile_wasm_binary_with_progress(
        &self,
        source: &str,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<Vec<u8>, KainError> {
        let typed_for_codegen = self.frontend_to_typed_program_with_source_path_and_progress(
            source,
            None,
            CompileTarget::Wasm,
            progress,
        )?;
        emit_compiler_phase(
            progress,
            None,
            CompileTarget::Wasm,
            CompilerProgressPhase::Codegen,
        );
        kain_web::generate_wasm(&typed_for_codegen)
    }

    #[cfg(not(feature = "web"))]
    pub fn compile_wasm_binary(&self, _source: &str) -> Result<Vec<u8>, KainError> {
        Err(KainError::runtime("WASM target requires web feature"))
    }

    #[cfg(feature = "web")]
    pub fn compile_hybrid_artifacts(
        &self,
        source: &str,
    ) -> Result<HybridArtifactOutput, KainError> {
        self.compile_hybrid_artifacts_with_progress(source, None)
    }

    #[cfg(feature = "web")]
    pub fn compile_hybrid_artifacts_with_progress(
        &self,
        source: &str,
        progress: Option<&ToolingProgressSink>,
    ) -> Result<HybridArtifactOutput, KainError> {
        let typed_for_codegen = self.frontend_to_typed_program_with_source_path_and_progress(
            source,
            None,
            CompileTarget::Hybrid,
            progress,
        )?;
        emit_compiler_phase(
            progress,
            None,
            CompileTarget::Hybrid,
            CompilerProgressPhase::Codegen,
        );
        let output = kain_web::generate_hybrid(&typed_for_codegen)?;
        Ok(HybridArtifactOutput {
            js: output.js,
            ts: output.ts,
            wasm: output.wasm,
            wasm_export_names: output
                .wasm_exports
                .into_iter()
                .map(|export| export.name)
                .collect(),
        })
    }

    #[cfg(not(feature = "web"))]
    pub fn compile_hybrid_artifacts(
        &self,
        _source: &str,
    ) -> Result<HybridArtifactOutput, KainError> {
        Err(KainError::runtime("Hybrid target requires web feature"))
    }

    #[cfg(all(feature = "gpu", feature = "sys"))]
    pub fn compile_shader_artifact_bundle(
        &self,
        source: &str,
    ) -> Result<ShaderArtifactBundleOutput, KainError> {
        let typed_program = self.frontend_to_typed_program(source, CompileTarget::Spirv)?;
        let spirv = gpu::generate_spirv(&typed_program)?;
        let rust_host = sys::generate_rust_gpu_host_wrappers(&typed_program)?;
        let reflection = sys::collect_gpu_artifacts(&typed_program);
        let reflection_json = sys::collect_gpu_artifacts_json(&typed_program).map_err(|err| {
            KainError::runtime(format!("Failed to serialize GPU reflection JSON: {err}"))
        })?;
        let derived_hlsl = gpu::generate_hlsl(&typed_program).ok();
        let derived_wgsl = gpu::generate_wgsl(&typed_program).ok();
        let ptx_candidate = typed_program_ptx_eligible(&typed_program);
        let (derived_ptx, derived_ptx_variants, ptx_note) = if ptx_candidate {
            match gpu::PtxCodegenOptions::from_env().and_then(|options| {
                gpu::generate_ptx_variant_modules(
                    &typed_program,
                    options,
                    gpu::PtxVariantSelection::AutoFamily,
                )
            }) {
                Ok(modules) => {
                    let primary = modules.first().map(|module| module.ptx.clone());
                    (primary, modules, None)
                }
                Err(err) => (
                    None,
                    Vec::new(),
                    Some(format!("PTX derived output skipped: {err}")),
                ),
            }
        } else if typed_program_has_shader(&typed_program) {
            (
                None,
                Vec::new(),
                Some(
                    "PTX derived output skipped because CUDA/PTX v1 supports compute-stage shader programs only."
                        .to_string(),
                ),
            )
        } else {
            (None, Vec::new(), None)
        };
        let bundle = build_shader_artifact_bundle(
            &reflection,
            ShaderArtifactFormat::Spirv,
            Some(&spirv),
            derived_hlsl.as_deref(),
            derived_wgsl.as_deref(),
            &derived_ptx_variants,
            ptx_note.as_deref(),
            "<input>",
        );
        let bundle_json = shader_artifact_bundle_to_json(&bundle).map_err(|err| {
            KainError::runtime(format!(
                "Failed to serialize shader artifact bundle JSON: {err}"
            ))
        })?;

        Ok(ShaderArtifactBundleOutput {
            bundle,
            bundle_json,
            spirv,
            rust_host,
            reflection_json,
            derived_hlsl,
            derived_wgsl,
            derived_ptx,
        })
    }

    #[cfg(all(feature = "gpu", feature = "sys"))]
    fn compile_cuda_artifact_bundle(
        &self,
        source: &str,
    ) -> Result<ShaderArtifactBundleOutput, KainError> {
        let typed_program = self.frontend_to_typed_program(source, CompileTarget::Cuda)?;
        let ptx_modules = gpu::generate_ptx_variant_modules(
            &typed_program,
            gpu::PtxCodegenOptions::from_env()?,
            gpu::PtxVariantSelection::AutoFamily,
        )?;
        let ptx = ptx_modules
            .first()
            .map(|module| module.ptx.clone())
            .ok_or_else(|| KainError::runtime("PTX backend emitted no CUDA modules"))?;
        let rust_host = sys::generate_rust_gpu_host_wrappers(&typed_program)?;
        let reflection = sys::collect_gpu_artifacts(&typed_program);
        let reflection_json = sys::collect_gpu_artifacts_json(&typed_program).map_err(|err| {
            KainError::runtime(format!("Failed to serialize GPU reflection JSON: {err}"))
        })?;
        let bundle = build_shader_artifact_bundle(
            &reflection,
            ShaderArtifactFormat::Ptx,
            None,
            None,
            None,
            &ptx_modules,
            Some(
                "PTX-first shader artifact bundle emitted because the authored kernel requested CUDA-native intrinsics.",
            ),
            "<input>",
        );
        let bundle_json = shader_artifact_bundle_to_json(&bundle).map_err(|err| {
            KainError::runtime(format!(
                "Failed to serialize shader artifact bundle JSON: {err}"
            ))
        })?;

        Ok(ShaderArtifactBundleOutput {
            bundle,
            bundle_json,
            spirv: Vec::new(),
            rust_host,
            reflection_json,
            derived_hlsl: None,
            derived_wgsl: None,
            derived_ptx: Some(ptx),
        })
    }

    #[cfg(not(all(feature = "gpu", feature = "sys")))]
    pub fn compile_shader_artifact_bundle(
        &self,
        _source: &str,
    ) -> Result<ShaderArtifactBundleOutput, KainError> {
        Err(KainError::runtime(
            "GPU artifact generation requires both gpu and sys features",
        ))
    }

    #[cfg(all(feature = "gpu", feature = "sys"))]
    pub fn compile_gpu_artifacts(&self, source: &str) -> Result<GpuArtifactOutput, KainError> {
        match self.compile_shader_artifact_bundle(source) {
            Ok(output) => Ok(output),
            Err(err) if source_requests_cuda_device_artifacts(source) => {
                self.compile_cuda_artifact_bundle(source).map_err(|fallback| {
                    KainError::runtime(format!(
                        "GPU artifact generation failed through both the canonical SPIR-V lane and the CUDA-native fallback.\nSPIR-V lane: {err}\nCUDA fallback: {fallback}"
                    ))
                })
            }
            Err(err) => Err(err),
        }
    }

    #[cfg(not(all(feature = "gpu", feature = "sys")))]
    pub fn compile_gpu_artifacts(&self, _source: &str) -> Result<GpuArtifactOutput, KainError> {
        Err(KainError::runtime(
            "GPU artifact generation requires both gpu and sys features",
        ))
    }

    #[cfg(feature = "sys")]
    pub fn compile_rust_artifact_bundle(
        &self,
        source: &str,
        include_spirv: bool,
    ) -> Result<RustBundleOutput, KainError> {
        let typed_program = self.frontend_to_typed_program(source, CompileTarget::Rust)?;
        let bundle = sys::generate_rust_artifact_bundle(&typed_program)?;

        let spirv = if include_spirv
            && bundle
                .shader_metadata
                .as_ref()
                .is_some_and(|metadata| !metadata.shaders.is_empty())
        {
            #[cfg(feature = "gpu")]
            {
                Some(self.compile_spirv_binary(source)?)
            }
            #[cfg(not(feature = "gpu"))]
            {
                return Err(KainError::runtime(
                    "Rust shader bundle requested SPIR-V output but gpu feature is disabled",
                ));
            }
        } else {
            None
        };

        Ok(RustBundleOutput { bundle, spirv })
    }

    #[cfg(not(feature = "sys"))]
    pub fn compile_rust_artifact_bundle(
        &self,
        _source: &str,
        _include_spirv: bool,
    ) -> Result<(), KainError> {
        Err(KainError::runtime(
            "Rust artifact bundling requires the sys feature",
        ))
    }

    #[cfg(feature = "ue5")]
    pub fn compile_ue5(
        &self,
        source: &str,
        output_name: Option<&str>,
        copyright: Option<&str>,
    ) -> Result<ue5::Ue5Output, KainError> {
        self.compile_ue5_with_context(source, output_name, copyright, None)
    }

    #[cfg(feature = "ue5")]
    pub fn compile_ue5_with_context(
        &self,
        source: &str,
        output_name: Option<&str>,
        copyright: Option<&str>,
        metadata_dir: Option<PathBuf>,
    ) -> Result<ue5::Ue5Output, KainError> {
        let frontend = build_frontend_source_bundle(self, source, None, CompileTarget::Ue5)?;

        let tokens = Lexer::new(&frontend.full_source).tokenize()?;
        let span_mapper =
            diagnostics::SpanMapper::with_origins(&frontend.full_source, frontend.origins.clone());
        let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
        comptime::eval_program(&mut ast)?;
        let typed_ast = types::check(&ast, &span_mapper, "<input>")?;
        let mono_ast = monomorphize::monomorphize(&typed_ast)?;
        let typed_for_codegen = TypedProgram {
            items: mono_ast.items,
        };

        let metadata_path = self.resolve_metadata_dir(metadata_dir);
        let mut context = ue5::Ue5Context::new(output_name.unwrap_or("Kain"), copyright);

        if metadata_path.exists() && metadata_path.is_dir() {
            if let Ok(entries) = std::fs::read_dir(&metadata_path) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.extension().is_some_and(|ext| ext == "json") {
                        if let Ok(data) = std::fs::read_to_string(&path) {
                            let filename = path
                                .file_name()
                                .and_then(|value| value.to_str())
                                .unwrap_or("");
                            match filename {
                                "widget_registry.json" => {
                                    let _ = context.widget_registry.load(&data);
                                }
                                "editor_attributes.json" => {
                                    let _ = context.editor_attributes.load(&data);
                                }
                                "shader_knowledge.json" => {
                                    let _ = context.shader_knowledge.load(&data);
                                }
                                "uht_rules.json" => {
                                    let _ = context.uht_rules.load(&data);
                                }
                                "module_graph.json" => {
                                    let _ = context.module_graph.load(&data);
                                }
                                "virtual_obligations.json" => {
                                    let _ = context.virtual_obligations.load(&data);
                                }
                                _ => {
                                    let _ = context.knowledge.load_metadata(&data);
                                    let _ = context.resolver.load_from_metadata(&data);
                                }
                            }
                        }
                    }
                }
            }
        }

        ue5::oracle::validate_program_full(
            &typed_for_codegen,
            &context.knowledge,
            &context.uht_rules,
            &span_mapper,
            "<input>",
        )?;

        ue5::generate_with_context_typed(&typed_for_codegen, output_name, copyright, &context)
    }

    #[cfg(feature = "ue5")]
    pub fn generate_usf_header(
        &self,
        source: &str,
        shader_name: &str,
    ) -> Result<String, KainError> {
        let typed_for_codegen = self.frontend_to_typed_program(source, CompileTarget::Usf)?;
        Ok(ue5_shaders::generate_cpp_header(
            &typed_for_codegen,
            shader_name,
        ))
    }

    #[cfg(feature = "ue5")]
    pub fn generate_usf_implementation(
        &self,
        source: &str,
        shader_name: &str,
        plugin_name: &str,
    ) -> Result<String, KainError> {
        let typed_for_codegen = self.frontend_to_typed_program(source, CompileTarget::Usf)?;
        Ok(ue5_shaders::generate_cpp_implementation(
            &typed_for_codegen,
            shader_name,
            plugin_name,
        ))
    }

    #[cfg(feature = "ue5")]
    pub fn compile_ue5editor(
        &self,
        source: &str,
        plugin_name: &str,
        copyright: Option<&str>,
    ) -> Result<ue5_editor::Ue5EditorOutput, KainError> {
        let typed_for_codegen = self.frontend_to_typed_program(source, CompileTarget::Ue5Editor)?;
        ue5_editor::generate(&typed_for_codegen, plugin_name, copyright)
    }

    #[cfg(feature = "ue5")]
    fn resolve_metadata_dir(&self, override_dir: Option<PathBuf>) -> PathBuf {
        override_dir
            .or_else(|| self.ue5_metadata_dir.clone())
            .unwrap_or_else(find_metadata_dir)
    }
}

impl FrontendImportCollector {
    fn collect_target_stdlib_prelude(&mut self, target: CompileTarget) -> Result<(), KainError> {
        for module_name in ambient_stdlib_modules_for_target(target) {
            let Some(module_file) = resolve_stdlib_module_file(module_name) else {
                continue;
            };
            let module_file = canonicalize_existing_path(&module_file);
            if self
                .entry_path
                .as_ref()
                .is_some_and(|entry_path| *entry_path == module_file)
            {
                continue;
            }
            self.collect_module_file(module_file, target)?;
        }
        Ok(())
    }

    fn collect_from_source(
        &mut self,
        source: &str,
        source_file: Option<&Path>,
        target: CompileTarget,
    ) -> Result<(), KainError> {
        let filename = source_file
            .map(|path| path.display().to_string())
            .unwrap_or_else(|| "<frontend-import-scan>".to_string());
        let program = parse_frontend_program(source, &filename)?;
        if source_file
            .map(canonicalize_existing_path)
            .zip(self.entry_path.as_ref())
            .is_some_and(|(path, entry)| path != *entry)
            && program_uses_c_bridge(&program)
        {
            self.advisories.push(format!(
                "Imported helper module {} uses `use c::...`. Prefer `@extern` in helper modules and keep `use c::...` in the entrypoint that owns linking.",
                filename
            ));
        }
        for module_name in implicit_root_stdlib_modules_for_program(&program) {
            let Some(module_file) = resolve_stdlib_module_file(&module_name) else {
                continue;
            };
            self.collect_module_file(module_file, target)?;
        }
        for item in program.items {
            let Item::Use(import) = item else {
                continue;
            };
            let Some(module_file) = resolve_frontend_import_file(&import, source_file) else {
                continue;
            };
            self.collect_module_file(module_file, target)?;
        }
        Ok(())
    }

    fn collect_module_file(
        &mut self,
        module_file: PathBuf,
        target: CompileTarget,
    ) -> Result<(), KainError> {
        let module_file = canonicalize_existing_path(&module_file);
        if self
            .entry_path
            .as_ref()
            .is_some_and(|entry_path| *entry_path == module_file)
        {
            return Ok(());
        }
        if !self.visited_module_files.insert(module_file.clone()) {
            return Ok(());
        }

        let module_source = std::fs::read_to_string(&module_file).map_err(|err| {
            KainError::runtime(format!(
                "Failed to read imported frontend module {}: {err}",
                module_file.display()
            ))
        })?;
        let prepared_module_source =
            prepare_frontend_source_for_target(&module_source, Some(&module_file), target)?;
        self.collect_from_source(&prepared_module_source, Some(&module_file), target)?;
        self.module_sources.push(FrontendSourceUnit {
            file_path: module_file,
            source: prepared_module_source,
        });
        Ok(())
    }
}

fn build_frontend_source_bundle(
    session: &DriverSession,
    source: &str,
    source_path: Option<&Path>,
    target: CompileTarget,
) -> Result<FrontendSourceBundle, KainError> {
    if let Some(cached) = try_reuse_cached_frontend_bundle(session, source, source_path, target) {
        session
            .last_frontend_bundle
            .replace(Some(cached.bundle.clone()));
        session
            .frontend_advisories
            .replace(cached.advisories.clone());
        return Ok(cached.bundle);
    }

    let prepared_source = prepare_frontend_source_for_target(source, source_path, target)?;
    let mut collector = FrontendImportCollector {
        entry_path: source_path.map(canonicalize_existing_path),
        ..FrontendImportCollector::default()
    };
    collector.collect_target_stdlib_prelude(target)?;
    collector.collect_from_source(&prepared_source, source_path, target)?;
    let bundle =
        assemble_frontend_source_bundle(&collector.module_sources, &prepared_source, source_path);
    let cache_entry = CachedFrontendBundle {
        target,
        source_path: source_path.map(|path| path.to_path_buf()),
        source: source.to_string(),
        advisories: collector.advisories.clone(),
        watch_inputs: fingerprint_watch_inputs(&bundle.watch_inputs),
        bundle: bundle.clone(),
    };
    session.frontend_cache.replace(Some(cache_entry));
    session.last_frontend_bundle.replace(Some(bundle.clone()));
    session
        .frontend_advisories
        .replace(collector.advisories.clone());
    Ok(bundle)
}

fn try_reuse_cached_frontend_bundle(
    session: &DriverSession,
    source: &str,
    source_path: Option<&Path>,
    target: CompileTarget,
) -> Option<CachedFrontendBundle> {
    let cached = session.frontend_cache.borrow().clone()?;
    if cached.target != target || cached.source != source {
        return None;
    }
    if cached.source_path.as_deref() != source_path {
        return None;
    }
    if fingerprint_watch_inputs_from_cached(&cached.watch_inputs) != cached.watch_inputs {
        return None;
    }
    Some(cached)
}

fn try_reuse_cached_checked_frontend(
    session: &DriverSession,
    source: &str,
    source_path: Option<&Path>,
    target: CompileTarget,
) -> Option<CachedCheckedFrontend> {
    let cached = session.checked_frontend_cache.borrow().clone()?;
    if cached.target != target || cached.source != source {
        return None;
    }
    if cached.source_path.as_deref() != source_path {
        return None;
    }
    if fingerprint_watch_inputs_from_cached(&cached.watch_inputs) != cached.watch_inputs {
        return None;
    }
    Some(cached)
}

fn assemble_frontend_source_bundle(
    module_sources: &[FrontendSourceUnit],
    entry_source: &str,
    entry_path: Option<&Path>,
) -> FrontendSourceBundle {
    let mut combined = String::new();
    let mut origins = Vec::new();
    let mut offset = 0usize;

    for module_source in module_sources {
        combined.push_str(&module_source.source);
        let end = offset + module_source.source.len();
        origins.push(SourceOriginSegment {
            file: module_source.file_path.display().to_string(),
            combined_span: kain_core::Span::new(offset, end),
            source: module_source.source.clone(),
        });
        offset = end;
        if !module_source.source.ends_with('\n') {
            combined.push('\n');
            offset += 1;
        }
        combined.push('\n');
        offset += 1;
    }
    combined.push_str(entry_source);
    origins.push(SourceOriginSegment {
        file: entry_path
            .map(|path| path.display().to_string())
            .unwrap_or_else(|| "<input>".to_string()),
        combined_span: kain_core::Span::new(offset, offset + entry_source.len()),
        source: entry_source.to_string(),
    });

    FrontendSourceBundle {
        full_source: combined,
        origins,
        watch_inputs: discover_frontend_watch_inputs(entry_path, module_sources),
    }
}

fn strip_use_items_from_program(program: &mut Program) {
    program.items = strip_use_items(&program.items);
}

fn strip_use_items(items: &[Item]) -> Vec<Item> {
    items.iter().filter_map(strip_use_item).collect()
}

fn strip_use_item(item: &Item) -> Option<Item> {
    match item {
        Item::Use(_) => None,
        Item::Mod(module) => {
            let mut filtered = module.clone();
            filtered.inline = module.inline.as_ref().map(|inline| strip_use_items(inline));
            Some(Item::Mod(filtered))
        }
        _ => Some(item.clone()),
    }
}

fn build_ui_output_from_frontend_bundle_source(
    source: &str,
    root_component: &str,
) -> Result<kain_ui::UiBuildOutput, KainError> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let mut parser = Parser::new(&tokens, &span_mapper, "<ui-source-bundle>");
    let mut program = parser.parse()?;
    comptime::eval_program(&mut program)?;
    strip_use_items_from_program(&mut program);
    kain_core::build_ui_output_from_program(&program, root_component)
}

fn parse_frontend_program(source: &str, filename: &str) -> Result<Program, KainError> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    Parser::new(&tokens, &span_mapper, filename).parse()
}

fn ambient_stdlib_modules_for_target(_target: CompileTarget) -> &'static [&'static str] {
    AMBIENT_STDLIB_MODULES
}

fn resolve_frontend_import_file(import: &Use, source_file: Option<&Path>) -> Option<PathBuf> {
    resolve_frontend_stdlib_module_file(import)
        .or_else(|| resolve_frontend_filesystem_module_file(import, source_file))
}

fn resolve_frontend_stdlib_module_file(import: &Use) -> Option<PathBuf> {
    let Some(first_segment) = import.path.first() else {
        return None;
    };
    if !matches!(first_segment.as_str(), "std" | "stdlib") {
        return None;
    }
    if import.path.len() <= 1 {
        return None;
    }

    for path_len in (1..import.path.len()).rev() {
        let module_name = import.path[1..=path_len].join("/");
        if let Some(module_file) = resolve_stdlib_module_file(&module_name) {
            return Some(module_file);
        }
    }

    None
}

fn resolve_frontend_filesystem_module_file(
    import: &Use,
    source_file: Option<&Path>,
) -> Option<PathBuf> {
    let Some(first_segment) = import.path.first() else {
        return None;
    };
    if matches!(
        first_segment.as_str(),
        "std" | "stdlib" | "rust" | "node" | "python" | "js" | "c"
    ) {
        return None;
    }
    let context = FilesystemModuleResolutionContext {
        importer_file: source_file.map(|path| path.to_path_buf()),
    };
    resolve_filesystem_module_file_with_context(&import.path, &context)
        .map(|resolution| resolution.file_path)
}

fn canonicalize_existing_path(path: &Path) -> PathBuf {
    std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf())
}

fn fingerprint_watch_inputs(paths: &[PathBuf]) -> Vec<FrontendWatchedInput> {
    paths
        .iter()
        .map(|path| frontend_watched_input(path))
        .collect()
}

fn fingerprint_watch_inputs_from_cached(
    paths: &[FrontendWatchedInput],
) -> Vec<FrontendWatchedInput> {
    paths
        .iter()
        .map(|entry| frontend_watched_input(&entry.path))
        .collect()
}

fn frontend_watched_input(path: &Path) -> FrontendWatchedInput {
    let metadata = std::fs::metadata(path).ok();
    let modified_unix_ms = metadata
        .as_ref()
        .and_then(|meta| meta.modified().ok())
        .and_then(|time| time.duration_since(std::time::UNIX_EPOCH).ok())
        .map(|duration| duration.as_millis());
    let byte_len = metadata.as_ref().map(|meta| meta.len());
    FrontendWatchedInput {
        path: path.to_path_buf(),
        modified_unix_ms,
        byte_len,
    }
}

fn discover_frontend_watch_inputs(
    entry_path: Option<&Path>,
    module_sources: &[FrontendSourceUnit],
) -> Vec<PathBuf> {
    let mut inputs = Vec::new();
    if let Some(entry_path) = entry_path {
        push_unique_watch_input(&mut inputs, canonicalize_existing_path(entry_path));
        for ancestor in entry_path.ancestors() {
            for manifest_name in ["build.kn", "KAIN.toml", "kain.toml"] {
                let candidate = ancestor.join(manifest_name);
                if candidate.exists() {
                    push_unique_watch_input(&mut inputs, canonicalize_existing_path(&candidate));
                }
            }
        }
    }
    for module_source in module_sources {
        push_unique_watch_input(&mut inputs, module_source.file_path.clone());
    }
    if let Some(map_path) = stdlib_map_path() {
        push_unique_watch_input(&mut inputs, map_path);
    }
    inputs
}

fn push_unique_watch_input(inputs: &mut Vec<PathBuf>, path: PathBuf) {
    if !inputs.iter().any(|existing| existing == &path) {
        inputs.push(path);
    }
}

fn stdlib_map_path() -> Option<PathBuf> {
    STDLIB_MAP_PATH
        .get_or_init(|| {
            kain_core::stdlib::find_stdlib_search_roots()
                .into_iter()
                .find_map(|root| {
                    let candidate = root.join("stdlib.map.json");
                    candidate.is_file().then_some(candidate)
                })
        })
        .clone()
}

fn root_stdlib_symbol_module_lookup() -> &'static HashMap<String, String> {
    ROOT_STDLIB_SYMBOL_MODULE_LOOKUP.get_or_init(load_root_stdlib_symbol_module_lookup)
}

fn load_root_stdlib_symbol_module_lookup() -> HashMap<String, String> {
    let Some(map_path) = stdlib_map_path() else {
        return HashMap::new();
    };
    let Ok(raw_map) = std::fs::read_to_string(map_path) else {
        return HashMap::new();
    };
    let Ok(parsed_map) = serde_json::from_str::<StdlibLookupMap>(&raw_map) else {
        return HashMap::new();
    };

    let mut candidates: HashMap<String, HashSet<String>> = HashMap::new();
    for module in parsed_map.modules {
        let Some(module_name) = module.import_path.strip_prefix("std::") else {
            continue;
        };
        let module_name = module_name.replace("::", "/");
        for symbol in module.symbols {
            if symbol.visibility != "public" {
                continue;
            }
            candidates
                .entry(symbol.name)
                .or_default()
                .insert(module_name.clone());
        }
    }

    let mut lookup = HashMap::new();
    for (symbol_name, module_names) in candidates {
        if module_names.len() != 1 {
            continue;
        }
        if let Some(module_name) = module_names.into_iter().next() {
            lookup.insert(symbol_name, module_name);
        }
    }
    lookup
}

fn implicit_root_stdlib_modules_for_program(program: &Program) -> HashSet<String> {
    let symbol_lookup = root_stdlib_symbol_module_lookup();
    if symbol_lookup.is_empty() {
        return HashSet::new();
    }

    let mut module_names = HashSet::new();
    for item in &program.items {
        collect_implicit_root_stdlib_modules_from_item(item, symbol_lookup, &mut module_names);
    }
    module_names
}

fn collect_implicit_root_stdlib_modules_from_item(
    item: &Item,
    symbol_lookup: &HashMap<String, String>,
    module_names: &mut HashSet<String>,
) {
    match item {
        Item::Function(function) => {
            collect_implicit_root_stdlib_modules_from_block(
                &function.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::Patch(patch) => {
            collect_implicit_root_stdlib_modules_from_block(
                &patch.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::Law(law) => {
            collect_implicit_root_stdlib_modules_from_block(&law.body, symbol_lookup, module_names);
        }
        Item::Converge(converge) => {
            collect_implicit_root_stdlib_modules_from_block(
                &converge.spec_lane.body,
                symbol_lookup,
                module_names,
            );
            for lane in &converge.fast_lanes {
                collect_implicit_root_stdlib_modules_from_block(
                    &lane.body,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Item::Orchestrate(orchestrate) => {
            collect_implicit_root_stdlib_modules_from_block(
                &orchestrate.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::Pulse(pulse) => {
            collect_implicit_root_stdlib_modules_from_block(
                &pulse.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::Resonate(resonate) => {
            collect_implicit_root_stdlib_modules_from_block(
                &resonate.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::World(world) => {
            for state in &world.states {
                collect_implicit_root_stdlib_modules_from_expr(
                    &state.initial,
                    symbol_lookup,
                    module_names,
                );
            }
            for surface in &world.surfaces {
                collect_implicit_root_stdlib_modules_from_expr(
                    &surface.expr,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Item::Component(component) => {
            for state in &component.state {
                collect_implicit_root_stdlib_modules_from_expr(
                    &state.initial,
                    symbol_lookup,
                    module_names,
                );
            }
            for method in &component.methods {
                collect_implicit_root_stdlib_modules_from_block(
                    &method.body,
                    symbol_lookup,
                    module_names,
                );
            }
            collect_implicit_root_stdlib_modules_from_jsx(
                &component.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::Shader(shader) => {
            collect_implicit_root_stdlib_modules_from_block(
                &shader.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::Actor(actor) => {
            for state in &actor.state {
                collect_implicit_root_stdlib_modules_from_expr(
                    &state.initial,
                    symbol_lookup,
                    module_names,
                );
            }
            for handler in &actor.handlers {
                collect_implicit_root_stdlib_modules_from_block(
                    &handler.body,
                    symbol_lookup,
                    module_names,
                );
            }
            for method in &actor.methods {
                collect_implicit_root_stdlib_modules_from_block(
                    &method.body,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Item::Struct(structure) => {
            for field in &structure.fields {
                if let Some(default_value) = &field.default {
                    collect_implicit_root_stdlib_modules_from_expr(
                        default_value,
                        symbol_lookup,
                        module_names,
                    );
                }
            }
            for method in &structure.methods {
                collect_implicit_root_stdlib_modules_from_block(
                    &method.body,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Item::Impl(implementation) => {
            for method in &implementation.methods {
                collect_implicit_root_stdlib_modules_from_block(
                    &method.body,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Item::Const(constant) => {
            collect_implicit_root_stdlib_modules_from_expr(
                &constant.value,
                symbol_lookup,
                module_names,
            );
        }
        Item::Comptime(comptime) => {
            collect_implicit_root_stdlib_modules_from_block(
                &comptime.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::Test(test) => {
            collect_implicit_root_stdlib_modules_from_block(
                &test.body,
                symbol_lookup,
                module_names,
            );
        }
        Item::Mod(module) => {
            if let Some(items) = &module.inline {
                for nested_item in items {
                    collect_implicit_root_stdlib_modules_from_item(
                        nested_item,
                        symbol_lookup,
                        module_names,
                    );
                }
            }
        }
        _ => {}
    }
}

fn collect_implicit_root_stdlib_modules_from_jsx(
    node: &JSXNode,
    symbol_lookup: &HashMap<String, String>,
    module_names: &mut HashSet<String>,
) {
    match node {
        JSXNode::Element {
            attributes,
            children,
            ..
        } => {
            for attribute in attributes {
                match &attribute.value {
                    kain_core::ast::JSXAttrValue::Expr(expr) => {
                        collect_implicit_root_stdlib_modules_from_expr(
                            expr,
                            symbol_lookup,
                            module_names,
                        );
                    }
                    kain_core::ast::JSXAttrValue::String(_)
                    | kain_core::ast::JSXAttrValue::Bool(_) => {}
                }
            }
            for child in children {
                collect_implicit_root_stdlib_modules_from_jsx(child, symbol_lookup, module_names);
            }
        }
        JSXNode::Expression(expr) => {
            collect_implicit_root_stdlib_modules_from_expr(expr, symbol_lookup, module_names);
        }
        JSXNode::ComponentCall {
            props, children, ..
        } => {
            for property in props {
                match &property.value {
                    kain_core::ast::JSXAttrValue::Expr(expr) => {
                        collect_implicit_root_stdlib_modules_from_expr(
                            expr,
                            symbol_lookup,
                            module_names,
                        );
                    }
                    kain_core::ast::JSXAttrValue::String(_)
                    | kain_core::ast::JSXAttrValue::Bool(_) => {}
                }
            }
            for child in children {
                collect_implicit_root_stdlib_modules_from_jsx(child, symbol_lookup, module_names);
            }
        }
        JSXNode::For { iter, body, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(iter, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_jsx(body, symbol_lookup, module_names);
        }
        JSXNode::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            collect_implicit_root_stdlib_modules_from_expr(condition, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_jsx(then_branch, symbol_lookup, module_names);
            if let Some(else_branch) = else_branch {
                collect_implicit_root_stdlib_modules_from_jsx(
                    else_branch,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        JSXNode::Fragment(children, _) => {
            for child in children {
                collect_implicit_root_stdlib_modules_from_jsx(child, symbol_lookup, module_names);
            }
        }
        JSXNode::Text(_, _) => {}
    }
}

fn collect_implicit_root_stdlib_modules_from_block(
    block: &Block,
    symbol_lookup: &HashMap<String, String>,
    module_names: &mut HashSet<String>,
) {
    for stmt in &block.stmts {
        match stmt {
            Stmt::Let { value, .. } => {
                if let Some(value) = value {
                    collect_implicit_root_stdlib_modules_from_expr(
                        value,
                        symbol_lookup,
                        module_names,
                    );
                }
            }
            Stmt::Expr(expr) | Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                collect_implicit_root_stdlib_modules_from_expr(expr, symbol_lookup, module_names);
            }
            Stmt::Defer { expr, .. } => {
                collect_implicit_root_stdlib_modules_from_expr(expr, symbol_lookup, module_names);
            }
            Stmt::Dispatch { dispatch_size, .. } => {
                for expr in dispatch_size {
                    collect_implicit_root_stdlib_modules_from_expr(
                        expr,
                        symbol_lookup,
                        module_names,
                    );
                }
            }
            Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
                collect_implicit_root_stdlib_modules_from_expr(iter, symbol_lookup, module_names);
                collect_implicit_root_stdlib_modules_from_block(body, symbol_lookup, module_names);
            }
            Stmt::While {
                condition, body, ..
            } => {
                collect_implicit_root_stdlib_modules_from_expr(
                    condition,
                    symbol_lookup,
                    module_names,
                );
                collect_implicit_root_stdlib_modules_from_block(body, symbol_lookup, module_names);
            }
            Stmt::Loop { body, .. } => {
                collect_implicit_root_stdlib_modules_from_block(body, symbol_lookup, module_names);
            }
            Stmt::Item(item) => {
                collect_implicit_root_stdlib_modules_from_item(item, symbol_lookup, module_names);
            }
            Stmt::Return(None, _) | Stmt::Break(None, _) | Stmt::Continue(_) => {}
        }
    }
}

fn collect_implicit_root_stdlib_modules_from_expr(
    expr: &Expr,
    symbol_lookup: &HashMap<String, String>,
    module_names: &mut HashSet<String>,
) {
    match expr {
        Expr::Ident(name, _) => {
            if let Some(module_name) = symbol_lookup.get(name) {
                module_names.insert(module_name.clone());
            }
        }
        Expr::Call { callee, args, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(callee, symbol_lookup, module_names);
            for arg in args {
                collect_implicit_root_stdlib_modules_from_expr(
                    &arg.value,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Expr::StageCall { function, args, .. } => {
            if let Some(module_name) = symbol_lookup.get(function) {
                module_names.insert(module_name.clone());
            }
            for arg in args {
                collect_implicit_root_stdlib_modules_from_expr(
                    &arg.value,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Expr::MethodCall { receiver, args, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(receiver, symbol_lookup, module_names);
            for arg in args {
                collect_implicit_root_stdlib_modules_from_expr(
                    &arg.value,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Expr::Struct {
            name, fields, rest, ..
        } => {
            if let Some(module_name) = symbol_lookup.get(name) {
                module_names.insert(module_name.clone());
            }
            for (_, field_expr) in fields {
                collect_implicit_root_stdlib_modules_from_expr(
                    field_expr,
                    symbol_lookup,
                    module_names,
                );
            }
            if let Some(rest) = rest {
                collect_implicit_root_stdlib_modules_from_expr(rest, symbol_lookup, module_names);
            }
        }
        Expr::EnumVariant {
            enum_name, fields, ..
        } => {
            if let Some(module_name) = symbol_lookup.get(enum_name) {
                module_names.insert(module_name.clone());
            }
            match fields {
                kain_core::ast::EnumVariantFields::Unit => {}
                kain_core::ast::EnumVariantFields::Tuple(values) => {
                    for value in values {
                        collect_implicit_root_stdlib_modules_from_expr(
                            value,
                            symbol_lookup,
                            module_names,
                        );
                    }
                }
                kain_core::ast::EnumVariantFields::Struct(entries) => {
                    for (_, value) in entries {
                        collect_implicit_root_stdlib_modules_from_expr(
                            value,
                            symbol_lookup,
                            module_names,
                        );
                    }
                }
            }
        }
        Expr::Binary { left, right, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(left, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_expr(right, symbol_lookup, module_names);
        }
        Expr::Unary { operand, .. }
        | Expr::Ref { value: operand, .. }
        | Expr::AddrOf { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Try(operand, _)
        | Expr::Await(operand, _)
        | Expr::AsyncBlock(operand, _)
        | Expr::Paren(operand, _)
        | Expr::Comptime(operand, _) => {
            collect_implicit_root_stdlib_modules_from_expr(operand, symbol_lookup, module_names);
        }
        Expr::Field { object, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(object, symbol_lookup, module_names);
        }
        Expr::Index { object, index, .. }
        | Expr::Assign {
            target: object,
            value: index,
            ..
        }
        | Expr::PtrOffset {
            pointer: object,
            offset: index,
            ..
        } => {
            collect_implicit_root_stdlib_modules_from_expr(object, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_expr(index, symbol_lookup, module_names);
        }
        Expr::AggregateInit { fields, .. } => {
            for (_, value) in fields {
                collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
            }
        }
        Expr::Array(values, _) | Expr::Tuple(values, _) | Expr::FString(values, _) => {
            for value in values {
                collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
            }
        }
        Expr::Range { start, end, .. } => {
            if let Some(start) = start {
                collect_implicit_root_stdlib_modules_from_expr(start, symbol_lookup, module_names);
            }
            if let Some(end) = end {
                collect_implicit_root_stdlib_modules_from_expr(end, symbol_lookup, module_names);
            }
        }
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            collect_implicit_root_stdlib_modules_from_expr(condition, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_block(
                then_branch,
                symbol_lookup,
                module_names,
            );
            if let Some(else_branch) = else_branch {
                match else_branch.as_ref() {
                    kain_core::ast::ElseBranch::Else(block) => {
                        collect_implicit_root_stdlib_modules_from_block(
                            block,
                            symbol_lookup,
                            module_names,
                        );
                    }
                    kain_core::ast::ElseBranch::ElseIf(expr, block, tail) => {
                        collect_implicit_root_stdlib_modules_from_expr(
                            expr,
                            symbol_lookup,
                            module_names,
                        );
                        collect_implicit_root_stdlib_modules_from_block(
                            block,
                            symbol_lookup,
                            module_names,
                        );
                        if let Some(tail) = tail {
                            match tail.as_ref() {
                                kain_core::ast::ElseBranch::Else(block) => {
                                    collect_implicit_root_stdlib_modules_from_block(
                                        block,
                                        symbol_lookup,
                                        module_names,
                                    );
                                }
                                kain_core::ast::ElseBranch::ElseIf(expr, block, nested_tail) => {
                                    let nested_expr = Expr::If {
                                        condition: expr.clone(),
                                        then_branch: block.clone(),
                                        else_branch: nested_tail.clone(),
                                        span: expr.span(),
                                    };
                                    collect_implicit_root_stdlib_modules_from_expr(
                                        &nested_expr,
                                        symbol_lookup,
                                        module_names,
                                    );
                                }
                            }
                        }
                    }
                }
            }
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            collect_implicit_root_stdlib_modules_from_expr(scrutinee, symbol_lookup, module_names);
            for arm in arms {
                if let Some(guard) = &arm.guard {
                    collect_implicit_root_stdlib_modules_from_expr(
                        guard,
                        symbol_lookup,
                        module_names,
                    );
                }
                collect_implicit_root_stdlib_modules_from_expr(
                    &arm.body,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Expr::Lambda { body, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(body, symbol_lookup, module_names);
        }
        Expr::MemLoad { pointer, .. }
        | Expr::VolatileLoad { pointer, .. }
        | Expr::CpuCacheFlush { pointer, .. }
        | Expr::Decay {
            target: pointer, ..
        } => {
            collect_implicit_root_stdlib_modules_from_expr(pointer, symbol_lookup, module_names);
        }
        Expr::MemStore { pointer, value, .. } | Expr::VolatileStore { pointer, value, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(pointer, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
        }
        Expr::AtomicLoad { pointer, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(pointer, symbol_lookup, module_names);
        }
        Expr::AtomicStore { pointer, value, .. }
        | Expr::AtomicAdd { pointer, value, .. }
        | Expr::AtomicSub { pointer, value, .. }
        | Expr::AtomicAnd { pointer, value, .. }
        | Expr::AtomicOr { pointer, value, .. }
        | Expr::AtomicXor { pointer, value, .. }
        | Expr::AtomicExchange { pointer, value, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(pointer, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
        }
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            ..
        } => {
            collect_implicit_root_stdlib_modules_from_expr(pointer, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_expr(expected, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_expr(desired, symbol_lookup, module_names);
        }
        Expr::InlineAsm { operands, .. } => {
            for operand in operands {
                collect_implicit_root_stdlib_modules_from_expr(
                    operand,
                    symbol_lookup,
                    module_names,
                );
            }
        }
        Expr::Alloc { size, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(size, symbol_lookup, module_names);
        }
        Expr::Realloc { pointer, size, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(pointer, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_expr(size, symbol_lookup, module_names);
        }
        Expr::Share { target, body, .. }
        | Expr::Observe { target, body, .. }
        | Expr::Collapse { target, body, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(target, symbol_lookup, module_names);
            collect_implicit_root_stdlib_modules_from_expr(body, symbol_lookup, module_names);
        }
        Expr::Teleport { value, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
        }
        Expr::Cast { value, .. } | Expr::Bitcast { value, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
        }
        Expr::Spawn { init, .. } => {
            for (_, value) in init {
                collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
            }
        }
        Expr::SendMsg { target, data, .. } => {
            collect_implicit_root_stdlib_modules_from_expr(target, symbol_lookup, module_names);
            for (_, value) in data {
                collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
            }
        }
        Expr::Block(block, _) => {
            collect_implicit_root_stdlib_modules_from_block(block, symbol_lookup, module_names);
        }
        Expr::JSX(node, _) => {
            collect_implicit_root_stdlib_modules_from_jsx(node, symbol_lookup, module_names);
        }
        Expr::Return(Some(value), _) | Expr::Break(Some(value), _) => {
            collect_implicit_root_stdlib_modules_from_expr(value, symbol_lookup, module_names);
        }
        Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::MacroCall { .. }
        | Expr::AtomicFence { .. }
        | Expr::CpuFence { .. }
        | Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Alloca { .. }
        | Expr::Uninit { .. }
        | Expr::Return(None, _)
        | Expr::Break(None, _)
        | Expr::Continue(_) => {}
    }
}

fn program_uses_c_bridge(program: &Program) -> bool {
    program.items.iter().any(item_uses_c_bridge)
}

fn item_uses_c_bridge(item: &Item) -> bool {
    match item {
        Item::Use(import) => import.path.first().is_some_and(|segment| segment == "c"),
        Item::Mod(module) => module
            .inline
            .as_ref()
            .map(|items| items.iter().any(item_uses_c_bridge))
            .unwrap_or(false),
        _ => false,
    }
}

fn source_prepare_dir(source_path: Option<&Path>) -> Option<PathBuf> {
    source_path
        .and_then(|path| path.parent())
        .and_then(|parent| {
            if parent.as_os_str().is_empty() {
                std::env::current_dir().ok()
            } else {
                Some(parent.to_path_buf())
            }
        })
        .or_else(|| std::env::current_dir().ok())
}

fn prepare_rust_ffi_source(
    source: &str,
    source_path: Option<&Path>,
    target: CompileTarget,
) -> Result<String, KainError> {
    let prepare = kain_crate_ffi::PrepareContext {
        current_dir: source_prepare_dir(source_path),
        manifest_path: None,
    };
    kain_crate_ffi::augment_source_for_runtime(source, target, &prepare)
}

fn prepare_c_ffi_source(
    source: &str,
    source_path: Option<&Path>,
    target: CompileTarget,
) -> Result<String, KainError> {
    let prepare = kain_c_ffi::PrepareContext {
        current_dir: source_prepare_dir(source_path),
        manifest_path: None,
    };
    kain_c_ffi::augment_source_for_runtime(source, target, &prepare)
}

fn register_frontend_extensions_for_target(target: CompileTarget) {
    match target {
        CompileTarget::Interpret | CompileTarget::Test | CompileTarget::Llvm => {
            kain_interop::register();
            kain_codebase::register();
            kain_python::register();
            kain_node::register();
            kain_crate_ffi::register();
            kain_c_ffi::register();
        }
        CompileTarget::Rust => {
            kain_c_ffi::register();
        }
        _ => {}
    }
}

fn prepare_frontend_source_for_target(
    source: &str,
    source_path: Option<&Path>,
    target: CompileTarget,
) -> Result<String, KainError> {
    match target {
        CompileTarget::Interpret | CompileTarget::Test => {
            let source = prepare_c_ffi_source(source, source_path, target)?;
            let source = kain_node::prepare_source_for_runtime(&source, target)?;
            prepare_rust_ffi_source(&source, source_path, target)
        }
        CompileTarget::Rust | CompileTarget::C | CompileTarget::Llvm => {
            prepare_c_ffi_source(source, source_path, target)
        }
        _ => Ok(source.to_string()),
    }
}

fn find_target_spec_by_alias(alias: &str) -> Option<&'static TargetSpec> {
    let normalized = alias.trim().to_ascii_lowercase();
    TARGET_SPECS.iter().find(|spec| {
        spec.aliases
            .iter()
            .any(|candidate| *candidate == normalized)
    })
}

fn find_target_spec_by_target(target: CompileTarget) -> Option<&'static TargetSpec> {
    TARGET_SPECS.iter().find(|spec| spec.target == target)
}

pub fn parse_compile_target(alias: &str) -> Option<CompileTarget> {
    find_target_spec_by_alias(alias).map(|spec| spec.target)
}

pub fn compile_target_name(target: CompileTarget) -> &'static str {
    match target {
        CompileTarget::Wasm => "wasm",
        CompileTarget::Llvm => "llvm",
        CompileTarget::C => "c",
        CompileTarget::Spirv => "spirv",
        CompileTarget::Hlsl => "hlsl",
        CompileTarget::Wgsl => "wgsl",
        CompileTarget::Cuda => "cuda",
        CompileTarget::Usf => "usf",
        CompileTarget::Js => "js",
        CompileTarget::Ts => "ts",
        CompileTarget::Rust => "rust",
        CompileTarget::Hybrid => "hybrid",
        CompileTarget::Cpp => "cpp",
        CompileTarget::Ue5 => "ue5",
        CompileTarget::Ue5Editor => "ue5-editor",
        CompileTarget::Interpret => "interpret",
        CompileTarget::Test => "test",
        CompileTarget::Ks => "ks",
    }
}

pub fn target_extension(target: CompileTarget) -> &'static str {
    find_target_spec_by_target(target)
        .map(|spec| spec.extension)
        .unwrap_or("out")
}

pub fn supported_targets_csv() -> String {
    TARGET_SPECS
        .iter()
        .map(|spec| spec.aliases[0])
        .collect::<Vec<_>>()
        .join(", ")
}

pub fn frontend_to_monomorphized_program(
    source: &str,
    target: CompileTarget,
) -> Result<MonomorphizedProgram, KainError> {
    DriverSession::default().frontend_to_monomorphized_program(source, target)
}

pub fn frontend_to_checked_program(
    source: &str,
    target: CompileTarget,
) -> Result<CheckedFrontend, KainError> {
    DriverSession::default().frontend_to_checked_program(source, target)
}

pub fn frontend_to_typed_program(
    source: &str,
    target: CompileTarget,
) -> Result<TypedProgram, KainError> {
    DriverSession::default().frontend_to_typed_program(source, target)
}

fn emit_compiler_phase(
    progress: Option<&ToolingProgressSink>,
    source_path: Option<&Path>,
    target: CompileTarget,
    phase: CompilerProgressPhase,
) {
    if let Some(progress) = progress {
        progress.emit(&ToolingProgressEvent::CompilerPhase {
            source_path: source_path.map(Path::to_path_buf),
            target: compile_target_name(target).to_string(),
            phase,
        });
    }
}

pub fn compile(source: &str, target: CompileTarget) -> Result<String, KainError> {
    DriverSession::default().compile(source, target)
}

pub fn compile_spirv_binary(source: &str) -> Result<Vec<u8>, KainError> {
    DriverSession::default().compile_spirv_binary(source)
}

pub fn compile_ptx_source(source: &str) -> Result<String, KainError> {
    DriverSession::default().compile_ptx_source(source)
}

pub fn compile_wasm_binary(source: &str) -> Result<Vec<u8>, KainError> {
    DriverSession::default().compile_wasm_binary(source)
}

pub fn compile_hybrid_artifacts(source: &str) -> Result<HybridArtifactOutput, KainError> {
    DriverSession::default().compile_hybrid_artifacts(source)
}

pub fn compile_shader_artifact_bundle(
    source: &str,
) -> Result<ShaderArtifactBundleOutput, KainError> {
    DriverSession::default().compile_shader_artifact_bundle(source)
}

pub fn compile_realtime_app_bundle(
    source: &str,
    target: CompileTarget,
    root_component: Option<&str>,
) -> Result<RealtimeAppBundleOutput, KainError> {
    DriverSession::default().compile_realtime_app_bundle(source, target, root_component)
}

pub(crate) fn resolve_root_component_name(
    program: &TypedProgram,
    target: CompileTarget,
    requested_root: Option<&str>,
) -> Result<Option<String>, KainError> {
    Ok(resolve_world_selection(program, target, requested_root)?.root_component)
}

pub(crate) fn resolve_world_selection(
    program: &TypedProgram,
    target: CompileTarget,
    requested_root: Option<&str>,
) -> Result<ResolvedWorldSelection, KainError> {
    let component_names = collect_component_names(&program.items);
    let worlds = collect_world_selection_info(&program.items);
    let required_surface = required_world_surface_for_target(target);

    if let Some(requested_root) = requested_root
        .map(str::trim)
        .filter(|value| !value.is_empty())
    {
        if let Some(world) = worlds.iter().find(|world| world.name == requested_root) {
            ensure_world_supports_target_surface(world, target, required_surface)?;
            return Ok(ResolvedWorldSelection {
                root_component: preferred_world_root_component(world, required_surface),
                active_world_name: Some(world.name.clone()),
            });
        }
        if component_names.iter().any(|name| name == requested_root) {
            let matching_worlds = worlds
                .iter()
                .filter(|world| {
                    world_root_component_for_target(world, required_surface).as_deref()
                        == Some(requested_root)
                })
                .collect::<Vec<_>>();
            if matching_worlds.len() > 1 {
                let world_names = matching_worlds
                    .iter()
                    .map(|world| world.name.as_str())
                    .collect::<Vec<_>>()
                    .join(", ");
                return Err(KainError::runtime(format!(
                    "Configured root '{}' matched multiple worlds for target {:?}: {}",
                    requested_root, target, world_names
                )));
            }
            return Ok(ResolvedWorldSelection {
                root_component: Some(requested_root.to_string()),
                active_world_name: matching_worlds.first().map(|world| world.name.clone()),
            });
        }
        return Err(KainError::runtime(format!(
            "Configured root '{}' did not match any component or world",
            requested_root
        )));
    }

    if let Some(required_surface) = required_surface {
        let eligible_worlds = worlds
            .iter()
            .filter(|world| world.has_surface(required_surface))
            .collect::<Vec<_>>();
        if eligible_worlds.len() > 1 {
            let world_names = eligible_worlds
                .iter()
                .map(|world| world.name.as_str())
                .collect::<Vec<_>>()
                .join(", ");
            return Err(KainError::runtime(format!(
                "Multiple worlds declare {} surfaces ({world_names}); pass --root <world-name> to select one explicitly",
                required_surface.as_str()
            )));
        }
        if let Some(world) = eligible_worlds.first() {
            return Ok(ResolvedWorldSelection {
                root_component: preferred_world_root_component(world, Some(required_surface))
                    .or_else(|| fallback_root_component(&component_names)),
                active_world_name: Some(world.name.clone()),
            });
        }
        if !worlds.is_empty() {
            return Err(KainError::runtime(format!(
                "No world declares the required '{}' surface for target {:?}",
                required_surface.as_str(),
                target
            )));
        }
    }

    if worlds.len() == 1 {
        return Ok(ResolvedWorldSelection {
            root_component: preferred_world_root_component(&worlds[0], required_surface)
                .or_else(|| fallback_root_component(&component_names)),
            active_world_name: Some(worlds[0].name.clone()),
        });
    }

    if let Some(root_component) = fallback_root_component(&component_names) {
        return Ok(ResolvedWorldSelection {
            root_component: Some(root_component),
            active_world_name: None,
        });
    }

    Ok(ResolvedWorldSelection {
        root_component: None,
        active_world_name: None,
    })
}

fn required_world_surface_for_target(target: CompileTarget) -> Option<WorldSurfaceKind> {
    match target {
        CompileTarget::Rust | CompileTarget::C | CompileTarget::Llvm | CompileTarget::Cpp => {
            Some(WorldSurfaceKind::NativeUi)
        }
        CompileTarget::Js | CompileTarget::Ts | CompileTarget::Wasm | CompileTarget::Hybrid => {
            Some(WorldSurfaceKind::Web)
        }
        CompileTarget::Ue5 | CompileTarget::Ue5Editor => Some(WorldSurfaceKind::Ue5),
        _ => None,
    }
}

fn ensure_world_supports_target_surface(
    world: &WorldSelectionInfo,
    target: CompileTarget,
    required_surface: Option<WorldSurfaceKind>,
) -> Result<(), KainError> {
    if let Some(required_surface) = required_surface {
        if !world.has_surface(required_surface) {
            return Err(KainError::runtime(format!(
                "World '{}' does not declare the required '{}' surface for target {:?}",
                world.name,
                required_surface.as_str(),
                target
            )));
        }
    }
    Ok(())
}

fn fallback_root_component(component_names: &[String]) -> Option<String> {
    component_names
        .iter()
        .find(|name| name.as_str() == "App")
        .cloned()
        .or_else(|| component_names.first().cloned())
}

fn preferred_world_root_component(
    world: &WorldSelectionInfo,
    required_surface: Option<WorldSurfaceKind>,
) -> Option<String> {
    world_root_component_for_target(world, required_surface)
        .or_else(|| world.root_component_for(WorldSurfaceKind::NativeUi))
        .or_else(|| world.first_root_component())
}

fn world_root_component_for_target(
    world: &WorldSelectionInfo,
    required_surface: Option<WorldSurfaceKind>,
) -> Option<String> {
    required_surface.and_then(|surface| world.root_component_for(surface))
}

pub(crate) fn apply_active_world_selection_to_realtime_bundle(
    bundle: &mut RealtimeAppBundle,
    active_world_name: Option<&str>,
) -> Result<(), KainError> {
    bundle.active_world = resolve_realtime_active_world(bundle, active_world_name)?;
    Ok(())
}

pub(crate) fn apply_active_world_selection_to_runtime_contract(
    bundle: &mut RuntimeContractBundle,
    active_world_name: Option<&str>,
) -> Result<(), KainError> {
    bundle.active_world = resolve_runtime_contract_active_world(bundle, active_world_name)?;
    Ok(())
}

fn resolve_realtime_active_world(
    bundle: &RealtimeAppBundle,
    active_world_name: Option<&str>,
) -> Result<Option<kain_core::RealtimeWorldBinding>, KainError> {
    if let Some(active_world_name) = active_world_name {
        return bundle
            .worlds
            .iter()
            .find(|world| world.name == active_world_name)
            .cloned()
            .map(Some)
            .ok_or_else(|| {
                KainError::runtime(format!(
                    "Requested active world '{}' was not emitted into the realtime bundle",
                    active_world_name
                ))
            });
    }
    Ok(if bundle.worlds.len() == 1 {
        bundle.worlds.first().cloned()
    } else {
        None
    })
}

fn resolve_runtime_contract_active_world(
    bundle: &RuntimeContractBundle,
    active_world_name: Option<&str>,
) -> Result<Option<kain_core::RuntimeWorldContract>, KainError> {
    if let Some(active_world_name) = active_world_name {
        return bundle
            .worlds
            .iter()
            .find(|world| world.name == active_world_name)
            .cloned()
            .map(Some)
            .ok_or_else(|| {
                KainError::runtime(format!(
                    "Requested active world '{}' was not emitted into the runtime contract bundle",
                    active_world_name
                ))
            });
    }
    Ok(if bundle.worlds.len() == 1 {
        bundle.worlds.first().cloned()
    } else {
        None
    })
}

fn collect_component_names(items: &[TypedItem]) -> Vec<String> {
    let mut names = Vec::new();
    collect_component_names_into(items, &mut names);
    names
}

fn collect_component_names_into(items: &[TypedItem], output: &mut Vec<String>) {
    for item in items {
        match item {
            TypedItem::Component(component) => output.push(component.ast.name.clone()),
            TypedItem::Mod(module) => collect_component_names_into(&module.items, output),
            _ => {}
        }
    }
}

fn collect_world_selection_info(items: &[TypedItem]) -> Vec<WorldSelectionInfo> {
    let mut worlds = Vec::new();
    collect_world_selection_info_into(items, &mut worlds);
    worlds
}

fn collect_world_selection_info_into(items: &[TypedItem], output: &mut Vec<WorldSelectionInfo>) {
    for item in items {
        match item {
            TypedItem::World(world) => output.push(WorldSelectionInfo {
                name: world.ast.name.clone(),
                surfaces: world
                    .ast
                    .surfaces
                    .iter()
                    .map(|surface| WorldSurfaceSelection {
                        kind: surface.kind,
                        root_component: expr_component_name(&surface.expr),
                    })
                    .collect(),
            }),
            TypedItem::Mod(module) => collect_world_selection_info_into(&module.items, output),
            _ => {}
        }
    }
}

impl WorldSelectionInfo {
    fn has_surface(&self, kind: WorldSurfaceKind) -> bool {
        self.surfaces.iter().any(|surface| surface.kind == kind)
    }

    fn root_component_for(&self, kind: WorldSurfaceKind) -> Option<String> {
        self.surfaces
            .iter()
            .find(|surface| surface.kind == kind)
            .and_then(|surface| surface.root_component.clone())
    }

    fn first_root_component(&self) -> Option<String> {
        self.surfaces
            .iter()
            .find_map(|surface| surface.root_component.clone())
    }
}

fn expr_component_name(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Ident(name, _) => Some(name.clone()),
        Expr::Call { callee, .. } => match callee.as_ref() {
            Expr::Ident(name, _) => Some(name.clone()),
            _ => None,
        },
        _ => None,
    }
}

pub fn compile_gpu_artifacts(source: &str) -> Result<GpuArtifactOutput, KainError> {
    DriverSession::default().compile_gpu_artifacts(source)
}

#[cfg(feature = "sys")]
pub fn stage_gpu_runtime_library_sidecar(
    artifact_root: &Path,
    release: bool,
    cargo_target_dir: Option<&Path>,
) -> Result<Option<PathBuf>, KainError> {
    let runtime_library_file_name = gpu_runtime_library_file_name();
    if let Some(existing) = resolve_existing_gpu_runtime_library(release, cargo_target_dir) {
        return copy_gpu_runtime_library_to_artifact_root(
            &existing,
            artifact_root,
            runtime_library_file_name,
        );
    }

    if !gpu_runtime_cargo_build_is_allowed() {
        return Err(KainError::runtime(format!(
            "No prebuilt {runtime_library_file_name} was found for PTX staging. The hot path now prefers cached sidecars and will not silently run cargo. Set {GPU_RUNTIME_LIBRARY_ENV_VAR} to an existing library, stage one under .kain/cache/run/llvm, or set {GPU_RUNTIME_ALLOW_CARGO_BUILD_ENV_VAR}=1 to permit a cold-path `cargo build -p kain-gpu-runtime`."
        )));
    }

    let Some(workspace_root) = find_workspace_root_with_gpu_runtime() else {
        return Ok(None);
    };
    let mut command = std::process::Command::new("cargo");
    command.arg("build").arg("-p").arg("kain-gpu-runtime");
    apply_cargo_command_defaults(&mut command);
    if release {
        command.arg("--release");
    }
    if let Some(cargo_target_dir) = cargo_target_dir {
        std::fs::create_dir_all(cargo_target_dir).map_err(|err| {
            KainError::runtime(format!(
                "Failed to create kain-gpu-runtime cargo target directory {}: {}",
                cargo_target_dir.display(),
                err
            ))
        })?;
        command.env("CARGO_TARGET_DIR", cargo_target_dir);
    }
    command.current_dir(&workspace_root);
    let output = command.output().map_err(|err| {
        KainError::runtime(format!(
            "Failed to invoke cargo to build kain-gpu-runtime at {}: {}",
            workspace_root.display(),
            err
        ))
    })?;
    if !output.status.success() {
        return Err(KainError::runtime(format!(
            "kain-gpu-runtime cargo build failed for {}:\n{}\n{}",
            workspace_root.display(),
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        )));
    }

    let built_library = resolve_existing_gpu_runtime_library(release, cargo_target_dir)
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Cargo reported success for kain-gpu-runtime but no {} was found in the staged search roots",
                runtime_library_file_name
            ))
        })?;
    copy_gpu_runtime_library_to_artifact_root(
        &built_library,
        artifact_root,
        runtime_library_file_name,
    )
}

#[cfg(feature = "sys")]
fn resolve_existing_gpu_runtime_library(
    release: bool,
    cargo_target_dir: Option<&Path>,
) -> Option<PathBuf> {
    if let Some(explicit) = std::env::var_os(GPU_RUNTIME_LIBRARY_ENV_VAR) {
        let candidate = PathBuf::from(explicit);
        if candidate.is_file() {
            return Some(candidate);
        }
    }

    let profile = if release { "release" } else { "debug" };
    let mut candidates = Vec::new();
    if let Some(target_dir) = cargo_target_dir {
        candidates.push(
            target_dir
                .join(profile)
                .join(gpu_runtime_library_file_name()),
        );
        candidates.push(
            target_dir
                .join(profile)
                .join("deps")
                .join(gpu_runtime_library_file_name()),
        );
    }
    if let Some(layout) = kain_core::install_layout::default_kain_install_layout() {
        candidates.push(layout.bin_dir.join(gpu_runtime_library_file_name()));
        candidates.push(
            layout
                .cache_dir
                .join("run")
                .join("llvm")
                .join(gpu_runtime_library_file_name()),
        );
    }
    if let Some(workspace_root) = find_workspace_root_with_gpu_runtime() {
        candidates.push(
            workspace_root
                .join(".kain")
                .join("cache")
                .join("run")
                .join("llvm")
                .join(gpu_runtime_library_file_name()),
        );
        candidates.push(
            workspace_root
                .join("target")
                .join(profile)
                .join(gpu_runtime_library_file_name()),
        );
        candidates.push(
            workspace_root
                .join("target")
                .join(profile)
                .join("deps")
                .join(gpu_runtime_library_file_name()),
        );
    }

    candidates.into_iter().find(|candidate| candidate.is_file())
}

#[cfg(feature = "sys")]
fn copy_gpu_runtime_library_to_artifact_root(
    source: &Path,
    artifact_root: &Path,
    runtime_library_file_name: &str,
) -> Result<Option<PathBuf>, KainError> {
    std::fs::create_dir_all(artifact_root).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create GPU runtime artifact directory {}: {}",
            artifact_root.display(),
            err
        ))
    })?;
    let destination = artifact_root.join(runtime_library_file_name);
    if source != destination {
        std::fs::copy(source, &destination).map_err(|err| {
            KainError::runtime(format!(
                "Failed to copy GPU runtime library {} -> {}: {}",
                source.display(),
                destination.display(),
                err
            ))
        })?;
    }
    Ok(Some(destination))
}

#[cfg(feature = "sys")]
fn gpu_runtime_cargo_build_is_allowed() -> bool {
    std::env::var(GPU_RUNTIME_ALLOW_CARGO_BUILD_ENV_VAR)
        .map(|value| {
            matches!(
                value.trim().to_ascii_lowercase().as_str(),
                "1" | "true" | "yes" | "on"
            )
        })
        .unwrap_or(false)
}

#[cfg(feature = "sys")]
fn gpu_runtime_library_file_name() -> &'static str {
    if cfg!(windows) {
        "kain_gpu_runtime.dll"
    } else if cfg!(target_os = "macos") {
        "libkain_gpu_runtime.dylib"
    } else {
        "libkain_gpu_runtime.so"
    }
}

#[cfg(feature = "sys")]
fn find_workspace_root_with_gpu_runtime() -> Option<PathBuf> {
    let mut roots = Vec::new();
    if let Ok(dir) = std::env::current_dir() {
        roots.push(dir);
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            roots.push(dir.to_path_buf());
        }
    }

    for mut dir in roots {
        for _ in 0..12 {
            if dir
                .join("crates")
                .join("gpu-runtime")
                .join("Cargo.toml")
                .exists()
            {
                return Some(dir);
            }
            if !dir.pop() {
                break;
            }
        }
    }
    None
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn typed_program_has_shader(program: &TypedProgram) -> bool {
    program
        .items
        .iter()
        .any(|item| matches!(item, TypedItem::Shader(_)))
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn typed_program_ptx_eligible(program: &TypedProgram) -> bool {
    let mut saw_shader = false;
    for item in &program.items {
        if let TypedItem::Shader(shader) = item {
            saw_shader = true;
            if shader.ast.stage != ShaderStage::Compute {
                return false;
            }
        }
    }
    saw_shader
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn source_requests_cuda_device_artifacts(source: &str) -> bool {
    source.contains("use std::cuda")
        || source.contains("use std::cuda\n")
        || source.contains("cuda_")
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn parse_ptx_artifact_metadata(ptx: &str) -> Option<PtxArtifactMetadata> {
    let ptx_version = parse_ptx_directive_value(ptx, ".version")?;
    let required_target_arch = parse_ptx_directive_value(ptx, ".target")?;
    let minimum_compute_capability = compute_capability_for_ptx_arch(&required_target_arch)
        .unwrap_or_else(|| "unknown".to_string());
    Some(PtxArtifactMetadata {
        ptx_version,
        required_target_arch,
        minimum_compute_capability,
    })
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn parse_ptx_directive_value(ptx: &str, directive: &str) -> Option<String> {
    for line in ptx.lines() {
        let trimmed = line.trim();
        let Some(rest) = trimmed.strip_prefix(directive) else {
            continue;
        };
        let rest = rest.trim();
        if rest.is_empty() {
            continue;
        }
        let value = rest
            .split(|ch: char| ch == ',' || ch.is_ascii_whitespace())
            .find(|segment| !segment.is_empty())?;
        return Some(value.to_string());
    }
    None
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn compute_capability_for_ptx_arch(required_target_arch: &str) -> Option<String> {
    let digits = required_target_arch.strip_prefix("sm_")?;
    let value = digits.parse::<u32>().ok()?;
    Some(format!("{}.{}", value / 10, value % 10))
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn build_shader_artifact_bundle(
    reflection: &sys::RustGpuArtifactOutput,
    canonical_native_payload: ShaderArtifactFormat,
    spirv: Option<&[u8]>,
    derived_hlsl: Option<&str>,
    derived_wgsl: Option<&str>,
    derived_ptx_modules: &[gpu::GeneratedPtxModule],
    ptx_note: Option<&str>,
    source_origin: &str,
) -> ShaderArtifactBundle {
    let module_name = if reflection.shaders.is_empty() {
        "kain_shader_program".to_string()
    } else {
        reflection
            .shaders
            .iter()
            .map(|shader| shader.name.as_str())
            .collect::<Vec<_>>()
            .join("__")
    };

    let mut resource_layouts = Vec::new();
    let mut entry_points = Vec::new();
    let mut stage_metadata = Vec::new();
    let mut specialization_constants = Vec::new();
    let mut reflection_shaders = Vec::new();
    let mut source_map = Vec::new();
    let mut stage_hints = Vec::new();

    for shader in &reflection.shaders {
        let stage = shader_stage_name(shader.stage).to_string();
        stage_hints.push(stage.clone());
        entry_points.push(ShaderEntryPoint {
            shader: shader.name.clone(),
            module_name: module_name.clone(),
            entry_point: shader.entry_point.clone(),
            stage: stage.clone(),
        });
        stage_metadata.push(ShaderStageMetadata {
            shader: shader.name.clone(),
            stage: stage.clone(),
            entry_point: shader.entry_point.clone(),
            input_count: shader.inputs.len(),
            binding_count: shader.bindings.len(),
            output_type: shader.output_type.clone(),
        });
        source_map.push(ShaderSourceMapEntry {
            shader: shader.name.clone(),
            source_origin: source_origin.to_string(),
            module_name: module_name.clone(),
            entry_point: shader.entry_point.clone(),
        });

        let normalized_bindings = shader
            .bindings
            .iter()
            .map(|binding| {
                let layout = ShaderResourceLayout {
                    shader: shader.name.clone(),
                    name: binding.name.clone(),
                    binding: binding.binding,
                    descriptor_set: binding.descriptor_set,
                    ty: binding.ty.clone(),
                    kind: shader_binding_kind_name(binding.kind).to_string(),
                };
                if matches!(
                    binding.kind,
                    sys::RustGpuBindingKind::LocalSize
                        | sys::RustGpuBindingKind::SpecializationConstant
                ) {
                    specialization_constants.push(ShaderSpecializationConstant {
                        shader: shader.name.clone(),
                        name: binding.name.clone(),
                        binding: binding.binding,
                        descriptor_set: binding.descriptor_set,
                        ty: binding.ty.clone(),
                        source_kind: shader_binding_kind_name(binding.kind).to_string(),
                    });
                }
                layout
            })
            .collect::<Vec<_>>();

        resource_layouts.extend(normalized_bindings.iter().cloned());
        reflection_shaders.push(ShaderReflectionShader {
            shader: shader.name.clone(),
            stage,
            entry_point: shader.entry_point.clone(),
            inputs: shader
                .inputs
                .iter()
                .map(|input| ShaderIoField {
                    name: input.name.clone(),
                    ty: input.ty.clone(),
                })
                .collect(),
            bindings: normalized_bindings,
            output_type: shader.output_type.clone(),
        });
    }

    stage_hints.sort();
    stage_hints.dedup();

    let all_entry_points = entry_points
        .iter()
        .map(|entry| entry.entry_point.clone())
        .collect::<Vec<_>>();
    let mut all_binding_slots = resource_layouts
        .iter()
        .map(|layout| layout.binding)
        .collect::<Vec<_>>();
    all_binding_slots.sort_unstable();
    all_binding_slots.dedup();
    let compute_entry_points = entry_points
        .iter()
        .filter(|entry| entry.stage.eq_ignore_ascii_case("compute"))
        .map(|entry| entry.entry_point.clone())
        .collect::<Vec<_>>();
    let mut compute_binding_slots = resource_layouts
        .iter()
        .filter(|layout| {
            reflection_shaders.iter().any(|shader| {
                shader.stage.eq_ignore_ascii_case("compute")
                    && shader.shader == layout.shader
                    && shader.bindings.iter().any(|binding| {
                        binding.binding == layout.binding
                            && binding.descriptor_set == layout.descriptor_set
                            && binding.name == layout.name
                    })
            })
        })
        .map(|layout| layout.binding)
        .collect::<Vec<_>>();
    compute_binding_slots.sort_unstable();
    compute_binding_slots.dedup();

    let mut derived_outputs = Vec::new();
    if let Some(hlsl) = derived_hlsl {
        derived_outputs.push(DerivedShaderArtifact {
            format: ShaderArtifactFormat::Hlsl,
            module_name: module_name.clone(),
            contents: hlsl.to_string(),
            entry_points: all_entry_points.clone(),
            binding_slots: all_binding_slots.clone(),
            ptx: None,
        });
    }
    if let Some(wgsl) = derived_wgsl {
        derived_outputs.push(DerivedShaderArtifact {
            format: ShaderArtifactFormat::Wgsl,
            module_name: module_name.clone(),
            contents: wgsl.to_string(),
            entry_points: all_entry_points.clone(),
            binding_slots: all_binding_slots.clone(),
            ptx: None,
        });
    }
    for module in derived_ptx_modules {
        derived_outputs.push(DerivedShaderArtifact {
            format: ShaderArtifactFormat::Ptx,
            module_name: module_name.clone(),
            contents: module.ptx.clone(),
            entry_points: compute_entry_points.clone(),
            binding_slots: compute_binding_slots.clone(),
            ptx: parse_ptx_artifact_metadata(&module.ptx),
        });
    }

    let mut reflection_notes = vec![
        "Compiler-owned shader artifact bundle emitted from kain-driver.".to_string(),
        "SPIR-V is the canonical native GPU payload; backend text shaders are derived outputs."
            .to_string(),
    ];
    if derived_ptx_modules.len() > 1 {
        let variant_arches = derived_ptx_modules
            .iter()
            .map(|module| module.target_arch.as_sm())
            .collect::<Vec<_>>()
            .join(", ");
        reflection_notes.push(format!(
            "PTX derived output includes {} ranked architecture variants for runtime auto-dispatch: {}.",
            derived_ptx_modules.len(),
            variant_arches
        ));
    }
    if let Some(note) = ptx_note {
        reflection_notes.push(note.to_string());
    }

    ShaderArtifactBundle {
        schema_version: SHADER_ARTIFACT_SCHEMA_VERSION,
        canonical_native_payload,
        spirv_modules: spirv
            .map(|spirv| {
                vec![SpirvModuleArtifact {
                    module_name: module_name.clone(),
                    byte_len: spirv.len(),
                    bytes_hex: bytes_to_hex(spirv),
                    entry_points: entry_points
                        .iter()
                        .map(|entry| entry.entry_point.clone())
                        .collect(),
                    stage_hints,
                }]
            })
            .unwrap_or_default(),
        reflection: ShaderReflectionSummary {
            emitted: !reflection_shaders.is_empty(),
            shaders: reflection_shaders,
            notes: reflection_notes,
        },
        resource_layouts,
        entry_points,
        stage_metadata,
        specialization_constants,
        debug: ShaderDebugBundle {
            source_map,
            notes: vec![
                "Shader bundle source map is currently scoped to shader entry points.".to_string(),
            ],
        },
        derived_outputs,
    }
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn shader_stage_name(stage: sys::RustGpuShaderStage) -> &'static str {
    match stage {
        sys::RustGpuShaderStage::Vertex => "vertex",
        sys::RustGpuShaderStage::Fragment => "fragment",
        sys::RustGpuShaderStage::Compute => "compute",
        sys::RustGpuShaderStage::Surface => "surface",
    }
}

#[cfg(all(feature = "gpu", feature = "sys"))]
fn shader_binding_kind_name(kind: sys::RustGpuBindingKind) -> &'static str {
    match kind {
        sys::RustGpuBindingKind::StorageBuffer => "storage_buffer",
        sys::RustGpuBindingKind::Sampler2D => "sampler_2d",
        sys::RustGpuBindingKind::Uniform => "uniform",
        sys::RustGpuBindingKind::LocalSize => "local_size",
        sys::RustGpuBindingKind::SpecializationConstant => "specialization_constant",
    }
}

#[cfg(feature = "sys")]
pub fn compile_rust_artifact_bundle(
    source: &str,
    include_spirv: bool,
) -> Result<RustBundleOutput, KainError> {
    DriverSession::default().compile_rust_artifact_bundle(source, include_spirv)
}

pub fn compile_runtime_contract_bundle(
    source: &str,
    target: CompileTarget,
) -> Result<RuntimeContractBundle, KainError> {
    DriverSession::default().compile_runtime_contract_bundle(source, target)
}

pub fn format_source(source: &str) -> Result<String, KainError> {
    DriverSession::default().format_source(source)
}

#[cfg(feature = "ue5")]
pub fn compile_ue5(
    source: &str,
    output_name: Option<&str>,
    copyright: Option<&str>,
) -> Result<ue5::Ue5Output, KainError> {
    DriverSession::default().compile_ue5(source, output_name, copyright)
}

#[cfg(feature = "ue5")]
pub fn compile_ue5_with_context(
    source: &str,
    output_name: Option<&str>,
    copyright: Option<&str>,
    metadata_dir: Option<PathBuf>,
) -> Result<ue5::Ue5Output, KainError> {
    DriverSession::default().compile_ue5_with_context(source, output_name, copyright, metadata_dir)
}

#[cfg(feature = "ue5")]
pub fn generate_usf_header(source: &str, shader_name: &str) -> Result<String, KainError> {
    DriverSession::default().generate_usf_header(source, shader_name)
}

#[cfg(feature = "ue5")]
pub fn generate_usf_implementation(
    source: &str,
    shader_name: &str,
    plugin_name: &str,
) -> Result<String, KainError> {
    DriverSession::default().generate_usf_implementation(source, shader_name, plugin_name)
}

#[cfg(feature = "ue5")]
pub fn compile_ue5editor(
    source: &str,
    plugin_name: &str,
    copyright: Option<&str>,
) -> Result<ue5_editor::Ue5EditorOutput, KainError> {
    DriverSession::default().compile_ue5editor(source, plugin_name, copyright)
}

#[cfg(feature = "ue5")]
fn find_metadata_dir() -> PathBuf {
    if let Ok(explicit) = std::env::var("KAIN_METADATA_DIR") {
        let candidate = PathBuf::from(explicit);
        if candidate.exists() {
            return candidate;
        }
    }

    let suffixes = [
        std::path::Path::new("unreal").join("metadata"),
        std::path::Path::new("Kain").join("unreal").join("metadata"),
    ];

    if let Ok(root) = std::env::var("KAIN_ROOT") {
        let base = PathBuf::from(root);
        for suffix in &suffixes {
            let candidate = base.join(suffix);
            if candidate.exists() {
                return candidate;
            }
        }
    }

    if let Ok(mut dir) = std::env::current_dir() {
        for _ in 0..10 {
            for suffix in &suffixes {
                let candidate = dir.join(suffix);
                if candidate.exists() {
                    return candidate;
                }
            }
            match dir.parent() {
                Some(parent) => dir = parent.to_path_buf(),
                None => break,
            }
        }
    }

    std::env::current_dir()
        .unwrap_or_else(|_| PathBuf::from("."))
        .join(std::path::Path::new("unreal").join("metadata"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::runtime::{interpret_with_env, Env, Value};
    use std::fs;

    static TEST_CWD_LOCK: Mutex<()> = Mutex::new(());

    fn repo_file(relative: &str) -> std::path::PathBuf {
        std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("../..")
            .join(relative)
    }

    #[test]
    fn parse_typescript_aliases() {
        assert_eq!(parse_compile_target("ts"), Some(CompileTarget::Ts));
        assert_eq!(parse_compile_target("typescript"), Some(CompileTarget::Ts));
    }

    #[test]
    fn parse_wgsl_aliases() {
        assert_eq!(parse_compile_target("wgsl"), Some(CompileTarget::Wgsl));
        assert_eq!(parse_compile_target("webgpu"), Some(CompileTarget::Wgsl));
    }

    #[test]
    fn parse_c_alias() {
        assert_eq!(parse_compile_target("c"), Some(CompileTarget::C));
    }

    #[test]
    fn extension_for_typescript_is_ts() {
        assert_eq!(target_extension(CompileTarget::Ts), "ts");
    }

    #[test]
    fn extension_for_wgsl_is_wgsl() {
        assert_eq!(target_extension(CompileTarget::Wgsl), "wgsl");
    }

    #[test]
    fn extension_for_c_is_c() {
        assert_eq!(target_extension(CompileTarget::C), "c");
    }

    #[test]
    fn interpret_target_executes_main_and_returns_result() {
        let output = compile(
            r#"
fn main() -> Int:
    return 42
"#,
            CompileTarget::Interpret,
        )
        .unwrap();

        assert_eq!(output, "42");
    }

    #[test]
    fn test_target_runs_kain_tests() {
        let output = compile(
            r#"
test smoke:
    assert(true, "should pass")
"#,
            CompileTarget::Test,
        )
        .unwrap();

        assert_eq!(output, "Tests passed");
    }

    #[test]
    fn compile_realtime_bundle_uses_single_world_native_ui_surface_as_root() {
        let source = r#"
world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

component App():
    render <panel title="Studio" />
"#;

        let output = compile_realtime_app_bundle(source, CompileTarget::Rust, None)
            .expect("realtime bundle");
        assert_eq!(output.bundle.worlds.len(), 1);
        assert_eq!(output.bundle.worlds[0].name, "Studio");
        assert_eq!(
            output
                .bundle
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("Studio")
        );
        assert!(output.bundle.worlds[0]
            .surfaces
            .iter()
            .any(|surface| surface.kind == "native_ui" && surface.authored_expr == "App"));
    }

    #[test]
    fn compile_realtime_bundle_requires_explicit_selection_when_multiple_worlds_exist() {
        let source = r#"
world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

world ShellWorld:
    state counter: Int = 0
    surface native_ui => Shell
    surface viewport3d => "ShellPreview"
    surface web => Shell
    surface ue5 => "ShellBridge"

component App():
    render <panel />

component Shell():
    render <panel />
"#;

        let error = compile_realtime_app_bundle(source, CompileTarget::Rust, None)
            .expect_err("multiple worlds should require explicit selection");
        assert!(error
            .to_string()
            .contains("Multiple worlds declare native_ui surfaces"));
    }

    #[test]
    fn compile_realtime_bundle_selects_requested_world_by_name() {
        let source = r#"
world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

world ShellWorld:
    state counter: Int = 0
    surface native_ui => Shell
    surface viewport3d => "ShellPreview"
    surface web => Shell
    surface ue5 => "ShellBridge"

component App():
    render <panel title="Studio" />

component Shell():
    render <panel title="Shell" />
"#;

        let output = compile_realtime_app_bundle(source, CompileTarget::Rust, Some("ShellWorld"))
            .expect("explicit world selection should compile");
        assert_eq!(
            output
                .bundle
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("ShellWorld")
        );
    }

    #[test]
    fn compile_realtime_bundle_supports_imported_world_and_entangle_modules() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let main_dir = temp.path().join("src");
        let import_dir = temp.path().join("intentkit");
        let main_path = main_dir.join("main.kn");
        let import_path = import_dir.join("intents.kn");
        fs::create_dir_all(&main_dir).expect("main dir");
        fs::create_dir_all(&import_dir).expect("import dir");
        fs::write(
            &main_path,
            r#"
use intentkit::intents

component App():
    render <panel title="Studio" />
"#,
        )
        .expect("main source");
        fs::write(
            &import_path,
            r#"
world Physics:
    state hp: Int = 7
    surface native_ui => App

world Hud:
    state hp_display: Int = 7
    surface web => App

entangle Physics.hp <-> Hud.hp_display with single_writer
"#,
        )
        .expect("import source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            compile_realtime_app_bundle(&source, CompileTarget::Rust, None)
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let output = result.expect("realtime bundle should load imported entangle module once");
        assert_eq!(
            output
                .bundle
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("Physics")
        );
        assert_eq!(output.bundle.entanglements.len(), 1);
        assert_eq!(output.bundle.entanglements[0].authority, "Physics.hp");
        assert_eq!(output.bundle.entanglements[0].mirror, "Hud.hp_display");
    }

    #[test]
    fn compile_realtime_bundle_supports_repeated_selected_imports_from_blade_module_root() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let main_dir = temp.path().join("src");
        let telemetry_dir = main_dir.join("telemetry");
        let main_path = main_dir.join("main.kn");
        let flow_path = telemetry_dir.join("flow.kn");
        fs::create_dir_all(&telemetry_dir).expect("telemetry dir");
        fs::write(
            temp.path().join("KAIN.toml"),
            r#"
[package]
name = "smoke-import"
version = "0.1.0"

[blade]
name = "smoke-import"
kind = "kain_app"
entry = "src/main.kn"
source_roots = ["src", "src/telemetry"]
module_roots = ["src", "src/telemetry"]
"#,
        )
        .expect("manifest");
        fs::write(
            &main_path,
            r#"
use flow::flow_lane
use flow::benchmark_lane

fn smoke_total() -> Int:
    return flow_lane() + benchmark_lane()
"#,
        )
        .expect("main source");
        fs::write(
            &flow_path,
            r#"
component SmokePanel():
    render <panel title="Telemetry Flow" />

world Authority:
    state signal: Int = 7
    surface native_ui => SmokePanel

world Mirror:
    state signal_copy: Int = 7
    surface web => SmokePanel

entangle Authority.signal <-> Mirror.signal_copy with single_writer

pub fn flow_lane() -> Int:
    return 11

pub fn benchmark_lane() -> Int:
    return 13
"#,
        )
        .expect("flow source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            DriverSession::default().compile_realtime_app_bundle_with_source_path(
                &source,
                Some(&main_path),
                CompileTarget::Rust,
                None,
            )
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let output =
            result.expect("repeated selected imports should not replay module entanglements");
        assert_eq!(
            output
                .bundle
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("Authority")
        );
        assert_eq!(output.bundle.entanglements.len(), 1);
        assert_eq!(output.bundle.entanglements[0].authority, "Authority.signal");
        assert_eq!(output.bundle.entanglements[0].mirror, "Mirror.signal_copy");
    }

    #[test]
    fn compile_realtime_bundle_for_llvm_uses_full_frontend_bundle_for_imported_ui_roots() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let main_dir = temp.path().join("src");
        let telemetry_dir = main_dir.join("telemetry");
        let main_path = main_dir.join("main.kn");
        let flow_path = telemetry_dir.join("flow.kn");
        fs::create_dir_all(&telemetry_dir).expect("telemetry dir");
        fs::write(
            temp.path().join("KAIN.toml"),
            r#"
[package]
name = "smoke-import"
version = "0.1.0"

[blade]
name = "smoke-import"
kind = "kain_app"
entry = "src/main.kn"
source_roots = ["src", "src/telemetry"]
module_roots = ["src", "src/telemetry"]
"#,
        )
        .expect("manifest");
        fs::write(
            &main_path,
            r#"
use flow::flow_lane
use flow::benchmark_lane

fn smoke_total() -> Int:
    return flow_lane() + benchmark_lane()
"#,
        )
        .expect("main source");
        fs::write(
            &flow_path,
            r#"
component SmokePanel():
    render <panel title="Telemetry Flow" />

world Authority:
    state signal: Int = 7
    surface native_ui => SmokePanel

world Mirror:
    state signal_copy: Int = 7
    surface web => SmokePanel

entangle Authority.signal <-> Mirror.signal_copy with single_writer

pub fn flow_lane() -> Int:
    return 11

pub fn benchmark_lane() -> Int:
    return 13
"#,
        )
        .expect("flow source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            DriverSession::default().compile_realtime_app_bundle_with_source_path(
                &source,
                Some(&main_path),
                CompileTarget::Llvm,
                None,
            )
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let output = result.expect(
            "llvm realtime bundles should reuse the assembled frontend source for imported UI roots",
        );
        assert_eq!(
            output
                .bundle
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("Authority")
        );
        assert_eq!(output.bundle.entanglements.len(), 1);
        assert!(output.bundle.ui_contracts.is_some());
    }

    #[test]
    fn compile_realtime_bundle_selects_single_web_world_for_web_targets() {
        let source = r#"
world Studio:
    state counter: Int = 0
    surface web => App

component App():
    render <panel title="Studio" />
"#;

        let output =
            compile_realtime_app_bundle(source, CompileTarget::Js, None).expect("web bundle");
        assert_eq!(
            output
                .bundle
                .active_world
                .as_ref()
                .map(|world| world.name.as_str()),
            Some("Studio")
        );
        assert!(output.bundle.worlds[0]
            .surfaces
            .iter()
            .any(|surface| surface.kind == "web" && surface.authored_expr == "App"));
    }

    #[test]
    fn compile_realtime_bundle_rejects_selected_world_missing_target_surface() {
        let source = r#"
world Studio:
    state counter: Int = 0
    surface native_ui => App

component App():
    render <panel title="Studio" />
"#;

        let error = compile_realtime_app_bundle(source, CompileTarget::Js, Some("Studio"))
            .expect_err("web targets should reject worlds without web surfaces");
        assert!(error
            .to_string()
            .contains("does not declare the required 'web' surface"));
    }

    #[test]
    fn interpret_target_executes_orchestrate_python_and_node_stages_via_registered_bridges() {
        let output = compile(
            r#"
@extern fn py_exec(code: String) -> Unit
@extern fn js_exec(code: String) -> Unit

fn __kain_stage_py_add(value: Int) -> Int:
    return value + 1000

fn __kain_stage_js_add(value: Int) -> Int:
    return value + 2000

orchestrate pipeline(value: Int) -> Int:
    let a: Int = python __kain_stage_py_add(value)
    let b: Int = node __kain_stage_js_add(a)
    return b

fn main() -> Int:
    py_exec("def __kain_stage_py_add(value):\n    return value + 10")
    js_exec("globalThis.__kain_stage_js_add = (value) => value + 20")
    return pipeline(1)
"#,
            CompileTarget::Interpret,
        )
        .expect("interpret pipeline");

        assert_eq!(output, "31");
    }

    #[test]
    fn interpret_target_exposes_patch_runtime_undo_and_replay_builtins() {
        let typed = DriverSession::default()
            .frontend_to_typed_program(
                r#"
world Studio:
    state counter: Int = 0
    surface native_ui => App
    surface viewport3d => "StudioPreview"
    surface web => App
    surface ue5 => "StudioBridge"

component App():
    render <panel title="Studio" />

patch set_counter(studio: Studio, to: Int) -> Int:
    studio.counter = to
    return studio.counter

fn main() -> Int:
    let studio = Studio
    return set_counter(studio, 7)
"#,
                CompileTarget::Interpret,
            )
            .expect("typed program");
        let mut env = Env::new();
        let output = interpret_with_env(&mut env, &typed).expect("interpret patch runtime");
        match output {
            Value::Int(value) => assert_eq!(value, 7),
            other => panic!("expected Int(7), got {other:?}"),
        }

        let history = env
            .call_named_function("patch_history", vec![])
            .expect("patch history");
        match history {
            Value::Array(values) => assert_eq!(values.read().unwrap().len(), 1),
            other => panic!("expected patch history array, got {other:?}"),
        }

        let events = env
            .call_named_function("patch_collaboration_events", vec![])
            .expect("patch collaboration events");
        match events {
            Value::Array(values) => assert_eq!(values.read().unwrap().len(), 1),
            other => panic!("expected patch collaboration event array, got {other:?}"),
        }

        let undone = env
            .call_named_function("patch_undo_last", vec![])
            .expect("patch undo");
        match undone {
            Value::Bool(value) => assert!(value),
            other => panic!("expected patch undo bool, got {other:?}"),
        }
        assert_eq!(read_world_counter(&env, "Studio"), 0);

        let replayed = env
            .call_named_function("patch_replay_last", vec![])
            .expect("patch replay");
        match replayed {
            Value::Bool(value) => assert!(value),
            other => panic!("expected patch replay bool, got {other:?}"),
        }
        assert_eq!(read_world_counter(&env, "Studio"), 7);
    }

    fn read_world_counter(env: &Env, world_name: &str) -> i64 {
        let Some(Value::Struct(_, fields)) = env.lookup_value(world_name) else {
            panic!("expected world struct for {world_name}");
        };
        let fields = fields.read().expect("world fields");
        let Some(Value::Int(value)) = fields.get("counter") else {
            panic!("expected Int counter field for {world_name}");
        };
        *value
    }

    #[test]
    fn interpret_target_supports_rust_crate_imports_from_kain_manifest() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let crate_dir = temp.path().join("sample_ffi");
        let crate_src_dir = crate_dir.join("src");
        fs::create_dir_all(&crate_src_dir).expect("crate src dir");
        fs::write(
            crate_dir.join("Cargo.toml"),
            r#"[package]
name = "sample_ffi"
version = "0.1.0"
edition = "2021"

[lib]
name = "sample_ffi"
path = "src/lib.rs"
"#,
        )
        .expect("sample ffi cargo");
        fs::write(
            crate_src_dir.join("lib.rs"),
            r#"pub fn add(lhs: i64, rhs: i64) -> i64 {
    lhs + rhs
}
"#,
        )
        .expect("sample ffi source");
        fs::write(
            temp.path().join("Cargo.toml"),
            r#"[workspace]
members = ["sample_ffi"]
resolver = "2"
"#,
        )
        .expect("workspace cargo manifest");
        fs::write(
            temp.path().join("KAIN.toml"),
            format!(
                r#"[package]
name = "crate_ffi_smoke"
version = "0.1.0"

[build]
entry = "src/main.kn"
output = "dist"
targets = ["run"]

[rust_ffi]
manifest_path = '{}'

[[rust_ffi.path_crates]]
name = "sample_ffi"
path = '{}'
"#,
                temp.path().join("Cargo.toml").display(),
                crate_dir.display()
            ),
        )
        .expect("kain manifest");

        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            compile(
                r#"
use rust::sample_ffi

fn main() -> Int:
    return add(20, 22)
"#,
                CompileTarget::Interpret,
            )
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let output = result.expect("crate ffi interpret output");
        assert_eq!(output, "42");
        assert!(
            temp.path()
                .join(".kain")
                .join("cache")
                .join("crate_ffi")
                .exists(),
            "crate ffi cache should be materialized"
        );
    }

    #[test]
    fn non_host_targets_reject_rust_crate_ffi_imports() {
        let error = compile(
            r#"
use rust::sample_ffi

fn main() -> Int:
    return 1
"#,
            CompileTarget::Ts,
        )
        .expect_err("non-host target should reject rust crate ffi");

        assert!(error.to_string().contains(
            "Rust crate FFI is only available in host-backed Kain execution lanes for now"
        ));
    }

    #[test]
    fn test_target_supports_rust_crate_imports_from_kain_manifest() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let crate_dir = temp.path().join("sample_test_ffi");
        let crate_src_dir = crate_dir.join("src");
        fs::create_dir_all(&crate_src_dir).expect("crate src dir");
        fs::write(
            crate_dir.join("Cargo.toml"),
            r#"[package]
name = "sample_test_ffi"
version = "0.1.0"
edition = "2021"

[lib]
name = "sample_test_ffi"
path = "src/lib.rs"
"#,
        )
        .expect("sample ffi cargo");
        fs::write(
            crate_src_dir.join("lib.rs"),
            r#"pub fn add(lhs: i64, rhs: i64) -> i64 {
    lhs + rhs
}
"#,
        )
        .expect("sample ffi source");
        fs::write(
            temp.path().join("Cargo.toml"),
            r#"[workspace]
members = ["sample_test_ffi"]
resolver = "2"
"#,
        )
        .expect("workspace cargo manifest");
        fs::write(
            temp.path().join("KAIN.toml"),
            format!(
                r#"[package]
name = "crate_ffi_test_smoke"
version = "0.1.0"

[build]
entry = "src/main.kn"
output = "dist"
targets = ["test"]

[rust_ffi]
manifest_path = '{}'

[[rust_ffi.path_crates]]
name = "sample_test_ffi"
path = '{}'
"#,
                temp.path().join("Cargo.toml").display(),
                crate_dir.display()
            ),
        )
        .expect("kain manifest");

        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            compile(
                r#"
use rust::sample_test_ffi

test crate_ffi:
    assert(add(1, 2) == 3, "crate ffi test should pass")
"#,
                CompileTarget::Test,
            )
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let output = result.expect("crate ffi test output");
        assert_eq!(output, "Tests passed");
    }

    #[test]
    fn frontend_to_typed_program_includes_filesystem_module_items() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let main_path = temp.path().join("main.kn");
        let module_dir = temp.path().join("src");
        let module_path = module_dir.join("module_probe.kn");
        fs::create_dir_all(&module_dir).expect("module dir");
        fs::write(
            &main_path,
            r#"
use module_probe::four

fn main() -> Int:
    return four()
"#,
        )
        .expect("main source");
        fs::write(
            &module_path,
            r#"
pub fn four() -> Int:
    return 4
"#,
        )
        .expect("module source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            DriverSession::default().frontend_to_typed_program(&source, CompileTarget::Llvm)
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let typed = result.expect("typed program");
        assert!(typed.items.iter().any(|item| {
            matches!(item, TypedItem::Function(function) if function.ast.name == "four")
        }));
    }

    #[test]
    fn frontend_source_bundle_materializes_imported_stdlib_modules_without_whole_root_slurp() {
        let session = DriverSession::default();
        let frontend = build_frontend_source_bundle(
            &session,
            r#"
use std::math

fn main() -> Int:
    return native_actor_spawn("probe", "state=0")
"#,
            None,
            CompileTarget::Llvm,
        )
        .expect("frontend bundle");

        assert!(frontend
            .full_source
            .contains("pub fn vec3_length(value: Vec3) -> Float:"));
        assert!(frontend
            .full_source
            .contains("pub fn native_runtime_init() -> Int:"));
        assert!(frontend.full_source.contains(
            "pub fn native_actor_spawn(actor_name: String, init_payload: String) -> Int:"
        ));
        assert!(!frontend.full_source.contains("actor GenServer:"));
    }

    #[test]
    fn frontend_to_typed_program_with_source_path_resolves_c_ffi_relative_to_source_file() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let workspace = temp.path().join("workspace");
        let outside = temp.path().join("outside");
        let cache_dir = workspace
            .join(".kain")
            .join("cache")
            .join("c_ffi")
            .join("probe");
        fs::create_dir_all(workspace.join("native")).expect("native dir");
        fs::create_dir_all(&cache_dir).expect("cache dir");
        fs::create_dir_all(&outside).expect("outside dir");
        fs::write(
            workspace.join("KAIN.toml"),
            r#"
[c_ffi]

[[c_ffi.libraries]]
name = "tiny"
tier = "inline"
header = "native/tiny.h"
sources = ["native/tiny.c"]
"#,
        )
        .expect("manifest");
        fs::write(
            workspace.join("native").join("tiny.h"),
            "int tiny_add(int value);\n",
        )
        .expect("header");
        fs::write(
            workspace.join("native").join("tiny.c"),
            "int tiny_add(int value) { return value + 1; }\n",
        )
        .expect("source");
        let prelude_path = cache_dir.join("tiny_prelude.kn");
        fs::write(
            &prelude_path,
            r#"
use c::tiny::c_tiny_tiny_add as c_tiny_tiny_add

fn main() -> Int:
    return 0
"#,
        )
        .expect("prelude");

        let source = fs::read_to_string(&prelude_path).expect("read prelude");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(&outside).expect("set cwd");
            DriverSession::default().frontend_to_typed_program_with_source_path(
                &source,
                Some(prelude_path.as_path()),
                CompileTarget::Llvm,
            )
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        result.expect("c ffi imports should resolve from the source file workspace");
    }

    #[test]
    fn frontend_bundle_does_not_duplicate_ambient_stdlib_entry_file() {
        let source = include_str!("../../../stdlib/runtime.kn");
        let path_buf = repo_file("stdlib/runtime.kn");
        let path = path_buf.as_path();
        let session = DriverSession::default();
        let frontend =
            build_frontend_source_bundle(&session, source, Some(path), CompileTarget::Llvm)
                .expect("frontend bundle");

        assert_eq!(
            frontend
                .full_source
                .matches("fn abi_runtime_init() -> Int")
                .count(),
            1
        );
        session
            .frontend_to_typed_program_with_source_path(source, Some(path), CompileTarget::Llvm)
            .expect("ambient stdlib entry should be checked once");
    }

    #[test]
    fn frontend_to_typed_program_includes_imported_stdlib_module_items() {
        let typed = DriverSession::default()
            .frontend_to_typed_program(
                r#"
use std::math

fn main() -> Int:
    if vec3_length(vec3(3.0, 4.0, 0.0)) > 0.0:
        return 0
    return 1
"#,
                CompileTarget::Llvm,
            )
            .expect("typed program");

        assert!(typed.items.iter().any(|item| {
            matches!(item, TypedItem::Function(function) if function.ast.name == "vec3_length")
        }));
    }

    #[test]
    fn frontend_bundle_native_cli_keeps_ascii_and_fs_top_level_functions_visible() {
        let path_buf = repo_file("smoketest/src/systems/native_cli.kn");
        let path = path_buf.as_path();
        let source = fs::read_to_string(path).expect("read native cli source");
        let session = DriverSession::default();
        let frontend =
            build_frontend_source_bundle(&session, &source, Some(path), CompileTarget::Llvm)
                .expect("frontend bundle");
        let origin_files: Vec<String> = frontend
            .origins
            .iter()
            .map(|origin| origin.file.clone())
            .collect();
        let ascii_origin = frontend
            .origins
            .iter()
            .find(|origin| origin.file.replace('\\', "/").ends_with("stdlib/ascii.kn"))
            .expect("frontend bundle should include ascii origin");
        let ascii_context_start = ascii_origin.combined_span.start.saturating_sub(80);
        let ascii_context_end =
            (ascii_origin.combined_span.start + 160).min(frontend.full_source.len());
        let ascii_context =
            frontend.full_source[ascii_context_start..ascii_context_end].to_string();
        assert!(
            origin_files
                .iter()
                .any(|file| file.replace('\\', "/").ends_with("stdlib/ascii.kn")),
            "frontend bundle missing stdlib/ascii.kn origin: {origin_files:?}"
        );
        assert!(
            origin_files
                .iter()
                .any(|file| file.replace('\\', "/").ends_with("stdlib/fs.kn")),
            "frontend bundle missing stdlib/fs.kn origin: {origin_files:?}"
        );
        let span_mapper =
            diagnostics::SpanMapper::with_origins(&frontend.full_source, frontend.origins.clone());
        let tokens = Lexer::new(&frontend.full_source)
            .tokenize()
            .expect("frontend bundle should lex");
        let ascii_boundary_tokens: Vec<String> = tokens
            .iter()
            .filter(|token| {
                token.span.start >= ascii_origin.combined_span.start.saturating_sub(16)
                    && token.span.start <= ascii_origin.combined_span.start + 64
            })
            .map(|token| format!("{:?}@{}", token.kind, token.span.start))
            .collect();
        let program = Parser::new(&tokens, &span_mapper, path.to_str().unwrap_or("<test>"))
            .parse()
            .expect("frontend bundle should parse");
        let origin_matches = |file: &str, suffix: &str| file.replace('\\', "/").ends_with(suffix);

        let ascii_functions: Vec<String> = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(function)
                    if span_mapper
                        .span_origin_file(function.span)
                        .is_some_and(|file| origin_matches(file, "stdlib/ascii.kn")) =>
                {
                    Some(function.name.clone())
                }
                _ => None,
            })
            .collect();
        let ascii_items: Vec<String> = program
            .items
            .iter()
            .filter_map(|item| {
                let (kind, name, span) = match item {
                    Item::Function(function) => ("fn", function.name.as_str(), function.span),
                    Item::Const(constant) => ("const", constant.name.as_str(), constant.span),
                    _ => return None,
                };
                let Some(file) = span_mapper.span_origin_file(span) else {
                    return None;
                };
                if !origin_matches(file, "stdlib/ascii.kn") {
                    return None;
                }
                Some(format!("{kind}:{name}"))
            })
            .collect();
        let ascii_named_items: Vec<String> = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(function)
                    if function.name.starts_with("ascii_")
                        || function.name.starts_with("ASCII_") =>
                {
                    Some(format!(
                        "fn:{}@{:?}",
                        function.name,
                        span_mapper.span_origin_file(function.span)
                    ))
                }
                Item::Const(constant)
                    if constant.name.starts_with("ascii_")
                        || constant.name.starts_with("ASCII_") =>
                {
                    Some(format!(
                        "const:{}@{:?}",
                        constant.name,
                        span_mapper.span_origin_file(constant.span)
                    ))
                }
                _ => None,
            })
            .collect();
        let ascii_is_byte_origins: Vec<Option<String>> = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(function) if function.name == "ascii_is_byte" => Some(
                    span_mapper
                        .span_origin_file(function.span)
                        .map(|file| file.to_string()),
                ),
                _ => None,
            })
            .collect();
        let fs_functions: Vec<String> = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(function)
                    if span_mapper
                        .span_origin_file(function.span)
                        .is_some_and(|file| origin_matches(file, "stdlib/fs.kn")) =>
                {
                    Some(function.name.clone())
                }
                _ => None,
            })
            .collect();
        let abi_fs_metadata_text_origins: Vec<Option<String>> = program
            .items
            .iter()
            .filter_map(|item| match item {
                Item::Function(function) if function.name == "abi_fs_metadata_text" => Some(
                    span_mapper
                        .span_origin_file(function.span)
                        .map(|file| file.to_string()),
                ),
                _ => None,
            })
            .collect();

        assert!(
            ascii_functions.iter().any(|name| name == "ascii_is_byte"),
            "ascii bundle functions missing ascii_is_byte: count={} origins={origin_files:?} ascii_items={ascii_items:?} ascii_named={ascii_named_items:?} names={ascii_functions:?} direct={ascii_is_byte_origins:?} context={ascii_context:?} tokens={ascii_boundary_tokens:?}",
            frontend.full_source.matches("pub fn ascii_is_byte(value: Int) -> Bool:").count(),
        );
        assert!(
            fs_functions.iter().any(|name| name == "abi_fs_metadata_text"),
            "fs bundle functions missing abi_fs_metadata_text: count={} origins={origin_files:?} names={fs_functions:?} direct={abi_fs_metadata_text_origins:?}",
            frontend
                .full_source
                .matches("pub fn abi_fs_metadata_text(path: String) -> String")
                .count()
        );
    }

    #[test]
    fn frontend_to_typed_program_deduplicates_runtime_and_machine_stdlib_imports() {
        DriverSession::default()
            .frontend_to_typed_program(
                r#"
use std::runtime
use std::machine

fn main() -> Int:
    let boot = runtime_init()
    if boot != 0:
        return boot
    return vm_page_size()
"#,
                CompileTarget::Llvm,
            )
            .expect("typed program");
    }

    #[test]
    fn frontend_bundle_tracks_origin_files_and_watch_inputs() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let main_path = temp.path().join("main.kn");
        let build_path = temp.path().join("build.kn");
        let module_dir = temp.path().join("src");
        let module_path = module_dir.join("module_probe.kn");
        fs::create_dir_all(&module_dir).expect("module dir");
        fs::write(
            &main_path,
            r#"
use module_probe::four

fn main() -> Int:
    return four()
"#,
        )
        .expect("main source");
        fs::write(&build_path, "package specimen\n").expect("build manifest");
        fs::write(
            &module_path,
            r#"
pub fn four() -> Int:
    return 4
"#,
        )
        .expect("module source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            let session = DriverSession::default();
            let bundle = build_frontend_source_bundle(
                &session,
                &source,
                Some(main_path.as_path()),
                CompileTarget::Llvm,
            )?;
            Ok::<_, KainError>((session, bundle))
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let (session, bundle) = result.expect("frontend bundle");
        assert!(bundle.origins.iter().any(|origin| {
            Path::new(&origin.file)
                .file_name()
                .and_then(|name| name.to_str())
                == Some("module_probe.kn")
        }));
        assert!(bundle.origins.iter().any(|origin| {
            Path::new(&origin.file)
                .file_name()
                .and_then(|name| name.to_str())
                == Some("main.kn")
        }));
        assert!(bundle
            .watch_inputs
            .iter()
            .any(|path| path.ends_with("main.kn")));
        assert!(bundle
            .watch_inputs
            .iter()
            .any(|path| path.ends_with("module_probe.kn")));
        assert!(bundle
            .watch_inputs
            .iter()
            .any(|path| path.ends_with("build.kn")));
        assert_eq!(session.frontend_watch_inputs(), bundle.watch_inputs);
    }

    #[test]
    fn imported_helper_module_c_bridge_emits_guidance_advisory() {
        let module_path = PathBuf::from("helper.kn");
        let mut collector = FrontendImportCollector {
            entry_path: Some(PathBuf::from("main.kn")),
            ..FrontendImportCollector::default()
        };

        collector
            .collect_from_source(
                r#"
use c::version

pub fn helper_value() -> Int:
    return 7
"#,
                Some(module_path.as_path()),
                CompileTarget::Llvm,
            )
            .expect("collector should parse helper source");

        assert!(collector.advisories.iter().any(|advisory| {
            advisory.contains("use c::")
                && advisory.contains("@extern")
                && advisory.contains("helper.kn")
        }));
    }

    #[cfg(feature = "sys")]
    #[test]
    fn compile_llvm_materializes_imported_filesystem_functions() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let main_path = temp.path().join("main.kn");
        let module_dir = temp.path().join("src");
        let module_path = module_dir.join("module_probe.kn");
        fs::create_dir_all(&module_dir).expect("module dir");
        fs::write(
            &main_path,
            r#"
use module_probe::four

fn main() -> Int:
    return four()
"#,
        )
        .expect("main source");
        fs::write(
            &module_path,
            r#"
pub fn four() -> Int:
    return 4
"#,
        )
        .expect("module source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            DriverSession::default().compile(&source, CompileTarget::Llvm)
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let llvm = result.expect("llvm output");
        assert!(llvm.contains("define i64 @four("));
        assert!(llvm.contains("call i64 @four("));
    }

    #[cfg(feature = "sys")]
    #[test]
    fn compile_llvm_supports_imported_string_concat_with_numeric_values() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let main_path = temp.path().join("main.kn");
        let module_dir = temp.path().join("src");
        let module_path = module_dir.join("module_probe.kn");
        fs::create_dir_all(&module_dir).expect("module dir");
        fs::write(
            &main_path,
            r#"
use module_probe::four
use module_probe::label

fn main() -> Int:
    let text = label() + " / " + str(four())
    if text != "":
        return 0
    return 1
"#,
        )
        .expect("main source");
        fs::write(
            &module_path,
            r#"
pub fn four() -> Int:
    return 4

pub fn label() -> String:
    return "label"
"#,
        )
        .expect("module source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            DriverSession::default().compile(&source, CompileTarget::Llvm)
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let llvm = result.expect("llvm output");
        assert!(llvm.contains("call i8* @str_concat("));
        assert!(llvm.contains("call i8* @to_string(i64"));
    }

    #[cfg(feature = "sys")]
    #[test]
    fn compile_llvm_supports_imported_impl_self_builder_methods() {
        let _guard = TEST_CWD_LOCK.lock().expect("cwd lock");
        let temp = tempfile::tempdir().expect("tempdir");
        let main_path = temp.path().join("main.kn");
        let module_dir = temp.path().join("src");
        let module_path = module_dir.join("builders.kn");
        fs::create_dir_all(&module_dir).expect("module dir");
        fs::write(
            &main_path,
            r#"
use builders::ButtonBuilder

fn main() -> Int:
    let b = ButtonBuilder { label: "Save", key: "" }.key("save")
    return len(b.key)
"#,
        )
        .expect("main source");
        fs::write(
            &module_path,
            r#"
pub struct ButtonBuilder:
    label: String
    key: String

impl ButtonBuilder:
    fn key(_self: Self_, key: String) -> Self_:
        return ButtonBuilder { label: _self.label, key: key }
"#,
        )
        .expect("module source");

        let source = fs::read_to_string(&main_path).expect("read main source");
        let previous_dir = std::env::current_dir().expect("current dir");
        let result = (|| {
            std::env::set_current_dir(temp.path()).expect("set cwd");
            DriverSession::default().compile(&source, CompileTarget::Llvm)
        })();
        std::env::set_current_dir(previous_dir).expect("restore cwd");

        let llvm = result.expect("llvm output");
        assert!(llvm.contains(
            "define internal %ButtonBuilder* @ButtonBuilder_key(%ButtonBuilder* %arg0, i8* %arg1)"
        ));
        assert!(llvm.contains("call %ButtonBuilder* @ButtonBuilder_key(%ButtonBuilder*"));
    }

    #[cfg(feature = "sys")]
    #[test]
    fn compile_llvm_monomorphized_impl_self_copy_preserves_concrete_return() {
        let llvm = DriverSession::default()
            .compile(
                r#"
struct KeywordMeshRecord:
    id: Int
    payload: Int
    tag: String

impl KeywordMeshRecord:
    fn clone_self(_self: Self_) -> Self:
        let copy: Self = _self
        return copy

fn main() -> Int:
    let record = KeywordMeshRecord { id: 1, payload: 20, tag: "mesh" }
    let clone = record.clone_self()
    if clone.id != 1:
        return 1
    if clone.payload != 20:
        return 2
    if clone.tag != "mesh":
        return 3
    return 0
"#,
                CompileTarget::Llvm,
            )
            .expect("llvm output");

        let clone_start = llvm
            .find("define internal %KeywordMeshRecord* @KeywordMeshRecord_clone_self(%KeywordMeshRecord* %arg0)")
            .expect("clone_self must keep concrete struct pointer return after monomorphization");
        let clone_end = llvm[clone_start..]
            .find("ret %KeywordMeshRecord*")
            .expect("clone_self should return the concrete struct pointer")
            + clone_start;
        let window = &llvm[clone_start..clone_end];

        assert!(
            !window.contains("ptrtoint %KeywordMeshRecord*"),
            "monomorphized Self copies must not degrade into i64 pointer bits:\n{}",
            window
        );
        assert!(
            llvm.contains(
                "call %KeywordMeshRecord* @KeywordMeshRecord_clone_self(%KeywordMeshRecord*"
            ),
            "call site must see the concrete clone_self return type:\n{}",
            llvm
        );
    }

    #[cfg(feature = "sys")]
    #[test]
    fn compile_llvm_supports_statement_if_with_ignored_string_result() {
        let llvm = DriverSession::default()
            .compile(
                r#"
fn main() -> Int:
    if 1 == 1:
        "episode" + " two"
    return 0
"#,
                CompileTarget::Llvm,
            )
            .expect("llvm output");

        assert!(llvm.contains("call i8* @str_concat("));
    }

    #[cfg(feature = "sys")]
    #[test]
    fn compile_llvm_monomorphizes_where_bound_trait_method_calls() {
        let llvm = DriverSession::default()
            .compile(
                r#"
trait Metric:
    fn fold_seed(_self: Self_) -> Int:
        return 0

struct Packet:
    id: Int

impl Metric for Packet:
    fn fold_seed(_self: Self_) -> Int:
        return (_self.id * 5) + 13

fn crunch_metric<T>(value: T, salt: Int) -> Int where T: Metric:
    let score = value.fold_seed() + salt
    return score

fn main() -> Int:
    let packet = Packet { id: 7 }
    return crunch_metric(packet, 3)
"#,
                CompileTarget::Llvm,
            )
            .expect("llvm output");

        assert!(llvm.contains("define internal i64 @Packet_fold_seed(%Packet %arg0)"));
        assert!(
            llvm.contains("define internal i64 @crunch_metric_Packet(%Packet %arg0, i64 %arg1)")
        );
        assert!(llvm.contains("call i64 @Packet_fold_seed(%Packet"));
        assert!(llvm.contains("call i64 @crunch_metric_Packet(%Packet"));
    }

    #[cfg(feature = "sys")]
    #[test]
    fn compile_llvm_monomorphizes_generic_struct_literals() {
        let program = DriverSession::default()
            .frontend_to_monomorphized_program(
                r#"
struct Layout:
    stride: Int

struct Wrap<T>:
    value: T
    layout: Layout

struct Packet:
    id: Int

fn wrap_packet(value: Packet) -> Wrap<Packet>:
    return Wrap {
        value: value,
        layout: Layout { stride: 16 }
    }

fn main() -> Int:
    let wrapped: Wrap<Packet> = wrap_packet(Packet { id: 7 })
    return wrapped.value.id
"#,
                CompileTarget::Llvm,
            )
            .expect("monomorphized program");

        assert!(program.items.iter().any(|item| matches!(
            item,
            TypedItem::Struct(struct_item) if struct_item.ast.name == "Wrap_Packet"
        )));

        let wrap_packet_fn = program
            .items
            .iter()
            .find_map(|item| match item {
                TypedItem::Function(function) if function.ast.name == "wrap_packet" => {
                    Some(function)
                }
                _ => None,
            })
            .expect("wrap_packet function");

        assert!(matches!(
            wrap_packet_fn.ast.return_type.as_ref(),
            Some(kain_core::ast::Type::Named { name, generics, .. })
                if name == "Wrap_Packet" && generics.is_empty()
        ));

        let wrapped_return = wrap_packet_fn
            .ast
            .body
            .stmts
            .iter()
            .find_map(|stmt| match stmt {
                kain_core::ast::Stmt::Return(Some(expr), _) => Some(expr),
                _ => None,
            })
            .expect("wrapped return");

        assert!(matches!(
            wrapped_return,
            kain_core::ast::Expr::Struct { name, .. } if name == "Wrap_Packet"
        ));
    }

    #[cfg(all(feature = "gpu", feature = "sys"))]
    #[test]
    fn compile_shader_artifact_bundle_emits_canonical_spirv_bundle() {
        let output = compile_shader_artifact_bundle(
            r#"
shader compute sample_gpu_kernel(id: UVec3) -> Vec4:
    uniform positions: StorageBuffer<Vec4> @0
    uniform center: Vec4 @1
    uniform count: UInt @2
    uniform LOCAL_SIZE_X: UInt @100
    uniform CFG_HIGH_QUALITY: UInt @101

    let idx = id.x
    let pos = positions[idx]
    return vec4(pos.x + center.x, pos.y, pos.z, 1.0)
"#,
        )
        .expect("shader artifact bundle should compile");

        assert_eq!(
            output.bundle.canonical_native_payload,
            ShaderArtifactFormat::Spirv
        );
        assert_eq!(output.bundle.spirv_modules.len(), 1);
        assert!(output.bundle.spirv_modules[0].byte_len > 0);
        assert!(output
            .bundle_json
            .contains("\"canonical_native_payload\": \"spirv\""));
        assert!(output.bundle_json.contains("\"spirv_modules\""));
        assert!(output.derived_hlsl.is_some());
        assert!(output.derived_wgsl.is_some());
        assert!(output.derived_ptx.is_some());
        assert!(output.bundle_json.contains("\"format\": \"wgsl\""));
        assert!(output.bundle_json.contains("\"format\": \"ptx\""));
        assert!(output
            .bundle
            .derived_outputs
            .iter()
            .any(|artifact| artifact.format == ShaderArtifactFormat::Wgsl));
        let ptx_artifacts = output
            .bundle
            .derived_outputs
            .iter()
            .filter(|artifact| artifact.format == ShaderArtifactFormat::Ptx)
            .collect::<Vec<_>>();
        assert!(ptx_artifacts.len() >= 3);
        let ptx_artifact = ptx_artifacts.first().expect("ptx sidecar");
        assert_eq!(
            ptx_artifact
                .ptx
                .as_ref()
                .expect("ptx metadata")
                .required_target_arch,
            "sm_30"
        );
        assert_eq!(
            ptx_artifact.entry_points,
            vec!["sample_gpu_kernel".to_string()]
        );
        assert_eq!(ptx_artifact.binding_slots, vec![0, 1, 2, 100, 101]);
        assert!(ptx_artifacts.iter().any(|artifact| {
            artifact
                .ptx
                .as_ref()
                .is_some_and(|metadata| metadata.required_target_arch == "sm_120")
        }));
        assert!(output.bundle.reflection.notes.iter().any(|note| {
            note.contains("runtime auto-dispatch")
                && note.contains("sm_30")
                && note.contains("sm_120")
        }));
    }

    #[cfg(all(feature = "gpu", feature = "sys"))]
    #[test]
    fn compile_shader_artifact_bundle_after_host_frontend_compile_keeps_scalar_casts() {
        let session = DriverSession::default();
        session
            .frontend_to_typed_program(
                r#"
fn main() -> Int:
    return 7
"#,
                CompileTarget::Interpret,
            )
            .expect("interpret frontend should compile before shader compile");

        let output = session
            .compile_shader_artifact_bundle(
                r#"
shader compute stream_pulse(id: UVec3) -> Vec4:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let i = id.x
    let sample = src[i]
    let pulse = sample + Float(i) * 0.125
    dst[i] = pulse
    return vec4(pulse, pulse * 0.5, 1.0 - pulse, 1.0)
"#,
            )
            .expect("shader compile should not inherit host-only frontend state");

        assert_eq!(
            output.bundle.canonical_native_payload,
            ShaderArtifactFormat::Spirv
        );
        assert_eq!(output.bundle.entry_points.len(), 1);
        assert_eq!(output.bundle.entry_points[0].stage, "compute");
        assert!(output.derived_ptx.is_some());
    }

    #[cfg(all(feature = "gpu", feature = "sys"))]
    #[test]
    fn compile_gpu_artifacts_falls_back_to_ptx_first_bundle_for_cuda_intrinsics() {
        std::thread::Builder::new()
            .stack_size(32 * 1024 * 1024)
            .spawn(|| {
                let output = DriverSession::default()
                    .compile_gpu_artifacts(
                        r#"
use std::cuda

shader compute cuda_lane_probe(id: UVec3) -> Void:
    uniform output: StorageBuffer<UInt> @0
    let idx = id.x
    output[idx] = cuda_lane_id()
    return
"#,
                    )
                    .expect("cuda-native artifact bundle should compile");

                assert_eq!(
                    output.bundle.canonical_native_payload,
                    ShaderArtifactFormat::Ptx
                );
                assert!(output.bundle.spirv_modules.is_empty());
                assert!(output.derived_hlsl.is_none());
                assert!(output.derived_wgsl.is_none());
                assert!(output.derived_ptx.is_some());
                assert!(output
                    .bundle_json
                    .contains("\"canonical_native_payload\": \"ptx\""));
                assert!(output.bundle_json.contains("\"spirv_modules\": []"));
                let ptx_artifacts = output
                    .bundle
                    .derived_outputs
                    .iter()
                    .filter(|artifact| artifact.format == ShaderArtifactFormat::Ptx)
                    .collect::<Vec<_>>();
                assert!(ptx_artifacts.len() >= 3);
                let ptx_artifact = ptx_artifacts.first().expect("ptx sidecar");
                assert_eq!(
                    ptx_artifact.entry_points,
                    vec!["cuda_lane_probe".to_string()]
                );
                assert_eq!(ptx_artifact.binding_slots, vec![0]);
                assert_eq!(
                    ptx_artifact
                        .ptx
                        .as_ref()
                        .expect("ptx metadata")
                        .required_target_arch,
                    "sm_30"
                );
                assert!(ptx_artifacts.iter().any(|artifact| {
                    artifact
                        .ptx
                        .as_ref()
                        .is_some_and(|metadata| metadata.required_target_arch == "sm_90")
                }));
                assert!(output.bundle.reflection.notes.iter().any(|note| {
                    note.contains("runtime auto-dispatch")
                        && note.contains("sm_30")
                        && note.contains("sm_120")
                }));
            })
            .expect("spawn gpu artifact test")
            .join()
            .expect("gpu artifact test thread");
    }
}
