//! Embeddable compiler driver for the KAIN toolchain.
//!
//! This crate owns the "glue" between `kain-core`, language-specific backends,
//! and Rust-hosted applications that want to compile KAIN without going
//! through the CLI binary.

use std::path::PathBuf;

use kain_core::error::KainError;
use kain_core::monomorphize::MonomorphizedProgram;
use kain_core::{
    comptime, diagnostics, monomorphize, stdlib, types, CompileTarget, Lexer, Parser, TypedProgram,
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
pub struct GpuArtifactOutput {
    pub spirv: Vec<u8>,
    pub rust_host: String,
    pub reflection_json: String,
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

    pub fn frontend_to_monomorphized_program(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<MonomorphizedProgram, KainError> {
        let stdlib_source = stdlib::load_stdlib_for_target(target);
        let full_source = format!("{stdlib_source}\n{source}");

        let tokens = Lexer::new(&full_source).tokenize()?;
        let span_mapper = diagnostics::SpanMapper::new(&full_source);
        let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
        comptime::eval_program(&mut ast)?;
        let typed_ast = types::check(&ast, &span_mapper, "<input>")?;
        monomorphize::monomorphize(&typed_ast)
    }

    pub fn frontend_to_typed_program(
        &self,
        source: &str,
        target: CompileTarget,
    ) -> Result<TypedProgram, KainError> {
        let mono_ast = self.frontend_to_monomorphized_program(source, target)?;
        Ok(TypedProgram {
            items: mono_ast.items,
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

                    CompileTarget::Interpret | CompileTarget::Test => Err(KainError::runtime(
                        "Interpret/Test targets not yet implemented in workspace",
                    )),

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
    pub fn compile_gpu_artifacts(&self, source: &str) -> Result<GpuArtifactOutput, KainError> {
        let typed_program = self.frontend_to_typed_program(source, CompileTarget::Spirv)?;
        let spirv = gpu::generate_spirv(&typed_program)?;
        let rust_host = sys::generate_rust_gpu_host_wrappers(&typed_program)?;
        let reflection_json = sys::collect_gpu_artifacts_json(&typed_program).map_err(|err| {
            KainError::runtime(format!("Failed to serialize GPU reflection JSON: {err}"))
        })?;

        Ok(GpuArtifactOutput {
            spirv,
            rust_host,
            reflection_json,
        })
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

pub fn compile_gpu_artifacts(source: &str) -> Result<GpuArtifactOutput, KainError> {
    DriverSession::default().compile_gpu_artifacts(source)
}

#[cfg(feature = "sys")]
pub fn compile_rust_artifact_bundle(
    source: &str,
    include_spirv: bool,
) -> Result<RustBundleOutput, KainError> {
    DriverSession::default().compile_rust_artifact_bundle(source, include_spirv)
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

    #[test]
    fn parse_typescript_aliases() {
        assert_eq!(parse_compile_target("ts"), Some(CompileTarget::Ts));
        assert_eq!(parse_compile_target("typescript"), Some(CompileTarget::Ts));
    }

    #[test]
    fn extension_for_typescript_is_ts() {
        assert_eq!(target_extension(CompileTarget::Ts), "ts");
    }
}
