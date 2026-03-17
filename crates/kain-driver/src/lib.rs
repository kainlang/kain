//! Embeddable compiler driver for the KAIN toolchain.
//!
//! This crate owns the "glue" between `kain-core`, language-specific backends,
//! and Rust-hosted applications that want to compile KAIN without going
//! through the CLI binary.

use std::path::PathBuf;
#[cfg(test)]
use std::sync::Mutex;

use kain_core::ast::Program;
use kain_core::error::KainError;
use kain_core::monomorphize::MonomorphizedProgram;
use kain_core::runtime;
use kain_core::{
    comptime, diagnostics, emit_realtime_app_bundle, emit_runtime_contract_bundle, monomorphize,
    realtime_app_bundle_to_json, stdlib, types, CompileTarget, Lexer, Parser, RealtimeAppBundle,
    RuntimeContractBundle, ShaderArtifactBundle, TypedItem, TypedProgram,
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
mod native_app;

#[cfg(feature = "sys")]
use kain_core::Span;

#[cfg(feature = "gpu")]
use gpu;

#[cfg(feature = "sys")]
use kain_sys_codegen as sys;

#[cfg(feature = "sys")]
pub use native_app::{
    compile_native_app_bundle, discover_native_app_root_component, materialize_native_app_bundle,
    NativeAppBundle, NativeAppBundleConfig, NativeAppMaterializationConfig,
    NativeAppMaterializedPaths, NativeAppMetadata, NativeAppRuntimeDependency,
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
        kain_c_ffi::register();
        kain_interop::register();
        kain_node::register();
        kain_python::register();
        kain_crate_ffi::register();

        let source = prepare_c_ffi_source(source, target)?;
        let source = kain_node::prepare_source_for_runtime(&source, target)?;
        let source = prepare_rust_ffi_source(&source, target)?;

        let stdlib_source = stdlib::load_stdlib_for_target(target);
        let full_source = format!("{stdlib_source}\n{source}");

        let tokens = Lexer::new(&full_source).tokenize()?;
        let span_mapper = diagnostics::SpanMapper::new(&full_source);
        let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
        comptime::eval_program(&mut ast)?;
        let typed = types::check(&ast, &span_mapper, "<input>")?;
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
        let resolved_root_component = root_component
            .map(str::to_string)
            .or_else(|| discover_root_component_name(&typed));
        let ui_output = if let Some(root_component) = resolved_root_component.as_deref() {
            Some(kain_core::build_ui_output_from_source(
                source,
                root_component,
            )?)
        } else {
            None
        };
        let bundle = emit_realtime_app_bundle(&typed, ui_output.as_ref(), target);
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

fn discover_root_component_name(program: &TypedProgram) -> Option<String> {
    program.items.iter().find_map(|item| match item {
        TypedItem::Component(component) => Some(component.ast.name.clone()),
        _ => None,
    })
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
}
