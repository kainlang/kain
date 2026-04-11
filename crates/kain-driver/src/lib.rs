//! Embeddable compiler driver for the KAIN toolchain.
//!
//! This crate owns the "glue" between `kain-core`, language-specific backends,
//! and Rust-hosted applications that want to compile KAIN without going
//! through the CLI binary.

use std::path::PathBuf;
#[cfg(test)]
use std::sync::Mutex;

use kain_core::ast::{Expr, Program, WorldSurfaceKind};
use kain_core::error::KainError;
use kain_core::monomorphize::MonomorphizedProgram;
use kain_core::runtime;
use kain_core::{
    comptime, diagnostics, emit_realtime_app_bundle, emit_runtime_contract_bundle, monomorphize,
    realtime_app_bundle_to_json, stdlib, types, CompileTarget, Lexer, Parser, RealtimeAppBundle,
    ResolvedType, RuntimeContractBundle, ShaderArtifactBundle, TypedItem, TypedProgram,
};

#[cfg(all(feature = "gpu", feature = "sys"))]
use kain_core::{
    bytes_to_hex, shader_artifact_bundle_to_json, DerivedShaderArtifact, ShaderArtifactFormat,
    ShaderDebugBundle, ShaderEntryPoint, ShaderIoField, ShaderReflectionShader,
    ShaderReflectionSummary, ShaderResourceLayout, ShaderSourceMapEntry,
    ShaderSpecializationConstant, ShaderStageMetadata, SpirvModuleArtifact,
    SHADER_ARTIFACT_SCHEMA_VERSION,
};

#[cfg(feature = "sys")]
mod compute_residency;
#[cfg(feature = "sys")]
mod native_app;

#[cfg(feature = "sys")]
use kain_core::Span;

#[cfg(feature = "gpu")]
use gpu;

#[cfg(feature = "sys")]
use kain_sys_codegen as sys;

#[cfg(feature = "sys")]
pub use compute_residency::{
    write_compute_residency_sidecars, ComputeResidencyBinding, ComputeResidencyBundle,
    ComputeResidencyEntry, COMPUTE_RESIDENCY_ENV_VAR, COMPUTE_RESIDENCY_FILE_NAME,
};
#[cfg(feature = "sys")]
pub use native_app::{
    compile_native_app_bundle, discover_native_app_root_component, materialize_native_app_bundle,
    NativeAppBundle, NativeAppBundleConfig, NativeAppHostSidecarBinding,
    NativeAppLauncherEntrypoint, NativeAppMaterializationConfig, NativeAppMaterializedPaths,
    NativeAppMetadata, NativeAppRuntimeDependency,
};

#[cfg(feature = "ue5")]
use ue5;

#[cfg(feature = "ue5")]
use ue5_editor;

#[cfg(feature = "ue5")]
use ue5_shaders;

#[cfg(feature = "web")]
use web;

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
        extension: "js",
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

#[derive(Debug, Clone, Default)]
pub struct DriverSession {
    ue5_metadata_dir: Option<PathBuf>,
}

#[derive(Debug, Clone)]
pub struct ShaderArtifactBundleOutput {
    pub bundle: ShaderArtifactBundle,
    pub bundle_json: String,
    pub spirv: Vec<u8>,
    pub rust_host: String,
    pub reflection_json: String,
    pub derived_hlsl: Option<String>,
}

pub type GpuArtifactOutput = ShaderArtifactBundleOutput;

#[derive(Debug, Clone)]
pub struct RealtimeAppBundleOutput {
    pub bundle: RealtimeAppBundle,
    pub bundle_json: String,
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

    pub fn set_ue5_metadata_dir(&mut self, path: impl Into<PathBuf>) {
        self.ue5_metadata_dir = Some(path.into());
    }

    pub fn frontend_to_checked_program(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<CheckedFrontend, KainError> {
        self.frontend_to_checked_program_with_extra_globals(
            source,
            target,
            std::iter::empty::<(String, ResolvedType)>(),
        )
    }

    pub fn frontend_to_checked_program_with_extra_globals<I>(
        &self,
        source: &str,
        target: CompileTarget,
        extra_globals: I,
    ) -> Result<CheckedFrontend, KainError>
    where
        I: IntoIterator<Item = (String, ResolvedType)>,
    {
        register_frontend_extensions_for_target(target);
        let source = prepare_frontend_source_for_target(source, target)?;

        let stdlib_source = stdlib::load_stdlib_for_target(target);
        let full_source = format!("{stdlib_source}\n{source}");

        let tokens = Lexer::new(&full_source).tokenize()?;
        let span_mapper = diagnostics::SpanMapper::new(&full_source);
        let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
        comptime::eval_program(&mut ast)?;
        let typed = types::check_with_extra_globals(&ast, &span_mapper, "<input>", extra_globals)?;
        Ok(CheckedFrontend { ast, typed })
    }

    pub fn frontend_to_monomorphized_program(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<MonomorphizedProgram, KainError> {
        let checked = self.frontend_to_checked_program(source, target)?;
        monomorphize::monomorphize(&checked.typed)
    }

    pub fn frontend_to_typed_program(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<TypedProgram, KainError> {
        Ok(self.frontend_to_checked_program(source, target)?.typed)
    }

    pub fn frontend_to_typed_program_with_extra_globals<I>(
        &self,
        source: &str,
        target: CompileTarget,
        extra_globals: I,
    ) -> Result<TypedProgram, KainError>
    where
        I: IntoIterator<Item = (String, ResolvedType)>,
    {
        Ok(self
            .frontend_to_checked_program_with_extra_globals(source, target, extra_globals)?
            .typed)
    }

    pub fn compile_runtime_contract_bundle(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<RuntimeContractBundle, KainError> {
        let typed = self.frontend_to_typed_program(source, target)?;
        Ok(emit_runtime_contract_bundle(&typed, target))
    }

    pub fn compile_realtime_app_bundle(
        &self,
        source: &str,
        target: CompileTarget,
        root_component: Option<&str>,
    ) -> Result<RealtimeAppBundleOutput, KainError> {
        let typed = self.frontend_to_typed_program(source, target)?;
        let prepared_source =
            prepare_c_ffi_source(source, target).unwrap_or_else(|_| source.to_string());
        let resolved_world = resolve_world_selection(&typed, target, root_component)?;
        let ui_output = if let Some(root_component) = resolved_world.root_component.as_deref() {
            Some(kain_core::build_ui_output_from_source(
                &prepared_source,
                root_component,
            )?)
        } else {
            None
        };
        let mut bundle = emit_realtime_app_bundle(&typed, ui_output.as_ref(), target);
        apply_active_world_selection_to_realtime_bundle(
            &mut bundle,
            resolved_world.active_world_name.as_deref(),
        )?;
        let bundle_json = realtime_app_bundle_to_json(&bundle).map_err(|err| {
            KainError::runtime(format!(
                "Failed to serialize realtime app bundle JSON: {err}"
            ))
        })?;
        Ok(RealtimeAppBundleOutput {
            bundle,
            bundle_json,
        })
    }

    pub fn compile(&self, source: &str, target: CompileTarget) -> Result<String, KainError> {
        match target {
            #[cfg(feature = "ue5")]
            CompileTarget::Ue5 => {
                let mono_for_codegen = self.frontend_to_monomorphized_program(source, target)?;
                let output = ue5::generate(&mono_for_codegen, None, None)?;
                Ok(format!("{}\n{}", output.header, output.source))
            }
            _ => {
                #[allow(unused_variables)]
                let typed_for_codegen = self.frontend_to_typed_program(source, target)?;

                match target {
                    #[cfg(feature = "ue5")]
                    CompileTarget::Usf => ue5_shaders::generate_usf(&typed_for_codegen),

                    #[cfg(feature = "gpu")]
                    CompileTarget::Spirv => gpu::generate_spirv(&typed_for_codegen)
                        .map(|bytes| format!("{} bytes", bytes.len())),

                    #[cfg(feature = "gpu")]
                    CompileTarget::Hlsl => gpu::generate_hlsl(&typed_for_codegen),

                    #[cfg(feature = "web")]
                    CompileTarget::Wasm => web::generate_wasm(&typed_for_codegen)
                        .map(|bytes| format!("{} bytes", bytes.len())),

                    #[cfg(feature = "web")]
                    CompileTarget::Js => web::generate_js(&typed_for_codegen),

                    #[cfg(feature = "web")]
                    CompileTarget::Ts => web::generate_ts(&typed_for_codegen),

                    #[cfg(feature = "web")]
                    CompileTarget::Ks => web::generate_ks(&typed_for_codegen),

                    #[cfg(feature = "web")]
                    CompileTarget::Hybrid => {
                        let output = web::generate_hybrid(&typed_for_codegen)?;
                        Ok(output.js)
                    }

                    #[cfg(feature = "sys")]
                    CompileTarget::Llvm => {
                        sys::generate_llvm(&typed_for_codegen).and_then(|bytes| {
                            String::from_utf8(bytes).map_err(|err| {
                                KainError::codegen(
                                    format!("LLVM output was not valid UTF-8: {err}"),
                                    Span::default(),
                                )
                            })
                        })
                    }

                    #[cfg(feature = "sys")]
                    CompileTarget::Rust => sys::generate_rust(&typed_for_codegen),

                    #[cfg(feature = "sys")]
                    CompileTarget::Cpp => sys::generate_cpp(&typed_for_codegen),

                    CompileTarget::Interpret => {
                        let value = runtime::interpret(&typed_for_codegen)?;
                        Ok(value.to_string())
                    }

                    CompileTarget::Test => {
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
        let typed_for_codegen = self.frontend_to_typed_program(source, CompileTarget::Spirv)?;
        gpu::generate_spirv(&typed_for_codegen)
    }

    #[cfg(not(feature = "gpu"))]
    pub fn compile_spirv_binary(&self, _source: &str) -> Result<Vec<u8>, KainError> {
        Err(KainError::runtime("SPIR-V target requires gpu feature"))
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
        let derived_hlsl = Some(gpu::generate_hlsl(&typed_program)?);
        let bundle =
            build_shader_artifact_bundle(&reflection, &spirv, derived_hlsl.as_deref(), "<input>");
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
        self.compile_shader_artifact_bundle(source)
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
        let source = prepare_rust_ffi_source(source, CompileTarget::Ue5)?;
        let stdlib_source = stdlib::load_stdlib_for_target(CompileTarget::Ue5);
        let full_source = format!("{stdlib_source}\n{source}");

        let tokens = Lexer::new(&full_source).tokenize()?;
        let span_mapper = diagnostics::SpanMapper::new(&full_source);
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

fn prepare_rust_ffi_source(source: &str, target: CompileTarget) -> Result<String, KainError> {
    let prepare = kain_crate_ffi::PrepareContext {
        current_dir: std::env::current_dir().ok(),
        manifest_path: None,
    };
    kain_crate_ffi::augment_source_for_runtime(source, target, &prepare)
}

fn prepare_c_ffi_source(source: &str, target: CompileTarget) -> Result<String, KainError> {
    let prepare = kain_c_ffi::PrepareContext {
        current_dir: std::env::current_dir().ok(),
        manifest_path: None,
    };
    kain_c_ffi::augment_source_for_runtime(source, target, &prepare)
}

fn register_frontend_extensions_for_target(target: CompileTarget) {
    match target {
        CompileTarget::Interpret | CompileTarget::Test => {
            kain_interop::register();
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
    target: CompileTarget,
) -> Result<String, KainError> {
    match target {
        CompileTarget::Interpret | CompileTarget::Test => {
            let source = prepare_c_ffi_source(source, target)?;
            let source = kain_node::prepare_source_for_runtime(&source, target)?;
            prepare_rust_ffi_source(&source, target)
        }
        CompileTarget::Rust => prepare_c_ffi_source(source, target),
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

pub fn compile(source: &str, target: CompileTarget) -> Result<String, KainError> {
    DriverSession::default().compile(source, target)
}

pub fn compile_spirv_binary(source: &str) -> Result<Vec<u8>, KainError> {
    DriverSession::default().compile_spirv_binary(source)
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
        CompileTarget::Rust | CompileTarget::Llvm | CompileTarget::Cpp => {
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

#[cfg(all(feature = "gpu", feature = "sys"))]
fn build_shader_artifact_bundle(
    reflection: &sys::RustGpuArtifactOutput,
    spirv: &[u8],
    derived_hlsl: Option<&str>,
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

    let mut derived_outputs = Vec::new();
    if let Some(hlsl) = derived_hlsl {
        derived_outputs.push(DerivedShaderArtifact {
            format: ShaderArtifactFormat::Hlsl,
            module_name: module_name.clone(),
            contents: hlsl.to_string(),
        });
    }

    ShaderArtifactBundle {
        schema_version: SHADER_ARTIFACT_SCHEMA_VERSION,
        canonical_native_payload: ShaderArtifactFormat::Spirv,
        spirv_modules: vec![SpirvModuleArtifact {
            module_name: module_name.clone(),
            byte_len: spirv.len(),
            bytes_hex: bytes_to_hex(spirv),
            entry_points: entry_points
                .iter()
                .map(|entry| entry.entry_point.clone())
                .collect(),
            stage_hints,
        }],
        reflection: ShaderReflectionSummary {
            emitted: !reflection_shaders.is_empty(),
            shaders: reflection_shaders,
            notes: vec![
                "Compiler-owned shader artifact bundle emitted from kain-driver.".to_string(),
                "SPIR-V is the canonical native GPU payload; backend text shaders are derived outputs.".to_string(),
            ],
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

    #[test]
    fn parse_typescript_aliases() {
        assert_eq!(parse_compile_target("ts"), Some(CompileTarget::Ts));
        assert_eq!(parse_compile_target("typescript"), Some(CompileTarget::Ts));
    }

    #[test]
    fn extension_for_typescript_is_ts() {
        assert_eq!(target_extension(CompileTarget::Ts), "ts");
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

    #[cfg(all(feature = "gpu", feature = "sys"))]
    #[test]
    fn compile_shader_artifact_bundle_emits_canonical_spirv_bundle() {
        let output = compile_shader_artifact_bundle(
            r#"
shader compute sample_gpu_kernel(id: UVec3) -> Vec4:
    uniform positions: StorageBuffer<Vec4> @0
    uniform center: Vec4 @1
    uniform brush_alpha: Sampler2D @2
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
    }
}
