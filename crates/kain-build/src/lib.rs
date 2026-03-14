use std::env;
use std::path::PathBuf;

pub use kain_host::{
    EngineModuleExport, EngineModuleExportConfig, HostResult, HostSession, HostType, KainError,
    KainReflect, NativeParam, TypeSchema,
};

const ENV_OUTPUT_DIR: &str = "KAIN_ENGINE_MODULE_DIR";
const ENV_MODULE_NAME: &str = "KAIN_ENGINE_MODULE_NAME";
const ENV_MODULE_FILE: &str = "KAIN_ENGINE_MODULE_FILE";
const ENV_IMPORT_SHIM_FILE: &str = "KAIN_ENGINE_IMPORT_SHIM_FILE";
const ENV_DISABLE_IMPORT_SHIM: &str = "KAIN_ENGINE_NO_IMPORT_SHIM";
const ENV_DISABLE_BANNER: &str = "KAIN_ENGINE_NO_BANNER";
const ENV_EXPORTED_MODULE_PATH: &str = "KAIN_ENGINE_MODULE_PATH";
const ENV_EXPORTED_IMPORT_PATH: &str = "KAIN_ENGINE_IMPORT_SHIM_PATH";

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EngineModuleBuildConfig {
    pub export: EngineModuleExportConfig,
    pub emit_rustc_env: bool,
    pub emit_rerun_if_env_changed: bool,
}

impl EngineModuleBuildConfig {
    pub fn from_env() -> Self {
        let module_name = env::var(ENV_MODULE_NAME).unwrap_or_else(|_| "engine".to_string());
        let mut export = EngineModuleExportConfig::for_module(module_name);

        if let Ok(output_dir) = env::var(ENV_OUTPUT_DIR) {
            export.output_dir = PathBuf::from(output_dir);
        } else if let Ok(out_dir) = env::var("OUT_DIR") {
            export.output_dir = PathBuf::from(out_dir).join("kain");
        }

        if let Ok(module_file) = env::var(ENV_MODULE_FILE) {
            export.module_file_name = module_file;
        }

        if matches_env_true(ENV_DISABLE_IMPORT_SHIM) {
            export.import_shim_file_name = None;
        } else if let Ok(import_shim) = env::var(ENV_IMPORT_SHIM_FILE) {
            export.import_shim_file_name = Some(import_shim);
        }

        if matches_env_true(ENV_DISABLE_BANNER) {
            export.include_banner = false;
        }

        Self {
            export,
            emit_rustc_env: true,
            emit_rerun_if_env_changed: true,
        }
    }

    pub fn with_export(mut self, export: EngineModuleExportConfig) -> Self {
        self.export = export;
        self
    }

    pub fn with_rustc_env(mut self, emit_rustc_env: bool) -> Self {
        self.emit_rustc_env = emit_rustc_env;
        self
    }

    pub fn with_rerun_if_env_changed(mut self, emit_rerun_if_env_changed: bool) -> Self {
        self.emit_rerun_if_env_changed = emit_rerun_if_env_changed;
        self
    }
}

impl Default for EngineModuleBuildConfig {
    fn default() -> Self {
        Self::from_env()
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EngineModuleBuildResult {
    pub export: EngineModuleExport,
}

pub struct EngineModuleBuild {
    host: HostSession,
    config: EngineModuleBuildConfig,
}

impl EngineModuleBuild {
    pub fn new() -> Self {
        Self {
            host: HostSession::new(),
            config: EngineModuleBuildConfig::default(),
        }
    }

    pub fn with_config(config: EngineModuleBuildConfig) -> Self {
        Self {
            host: HostSession::new(),
            config,
        }
    }

    pub fn config(&self) -> &EngineModuleBuildConfig {
        &self.config
    }

    pub fn config_mut(&mut self) -> &mut EngineModuleBuildConfig {
        &mut self.config
    }

    pub fn host(&self) -> &HostSession {
        &self.host
    }

    pub fn host_mut(&mut self) -> &mut HostSession {
        &mut self.host
    }

    pub fn register_type<T>(&mut self) -> &mut Self
    where
        T: KainReflect,
    {
        self.host.register_type::<T>();
        self
    }

    pub fn register_schema(&mut self, schema: TypeSchema) -> &mut Self {
        self.host.register_schema(schema);
        self
    }

    pub fn declare_native_fn(
        &mut self,
        name: impl Into<String>,
        params: Vec<NativeParam>,
        return_type: HostType,
    ) -> &mut Self {
        self.host.declare_native_fn(name, params, return_type);
        self
    }

    pub fn build(&self) -> HostResult<EngineModuleBuildResult> {
        if self.config.emit_rerun_if_env_changed {
            emit_rerun_env_directives();
        }

        let export = self.host.export_engine_module(&self.config.export)?;
        println!(
            "cargo:warning=Exported KAIN engine module to {}",
            export.module_path.display()
        );

        if self.config.emit_rustc_env {
            println!(
                "cargo:rustc-env={}={}",
                ENV_EXPORTED_MODULE_PATH,
                export.module_path.display()
            );
            if let Some(import_path) = &export.import_shim_path {
                println!(
                    "cargo:rustc-env={}={}",
                    ENV_EXPORTED_IMPORT_PATH,
                    import_path.display()
                );
            }
        }

        Ok(EngineModuleBuildResult { export })
    }
}

impl Default for EngineModuleBuild {
    fn default() -> Self {
        Self::new()
    }
}

pub fn export_engine_module<F>(configure: F) -> HostResult<EngineModuleBuildResult>
where
    F: FnOnce(&mut EngineModuleBuild),
{
    let mut build = EngineModuleBuild::new();
    configure(&mut build);
    build.build()
}

fn emit_rerun_env_directives() {
    for key in [
        ENV_OUTPUT_DIR,
        ENV_MODULE_NAME,
        ENV_MODULE_FILE,
        ENV_IMPORT_SHIM_FILE,
        ENV_DISABLE_IMPORT_SHIM,
        ENV_DISABLE_BANNER,
    ] {
        println!("cargo:rerun-if-env-changed={key}");
    }
}

fn matches_env_true(key: &str) -> bool {
    env::var(key)
        .map(|value| matches!(value.as_str(), "1" | "true" | "TRUE" | "yes" | "YES"))
        .unwrap_or(false)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[derive(Clone)]
    struct LocalVec3;

    impl KainReflect for LocalVec3 {
        fn schema() -> TypeSchema {
            TypeSchema::new(
                "Vec3",
                "LocalVec3",
                kain_host::reflect::TypeKind::Struct {
                    fields: vec![
                        kain_host::reflect::FieldSchema::new(
                            "x",
                            kain_host::reflect::TypeRef::Primitive(
                                kain_host::reflect::PrimitiveType::Float,
                            ),
                        ),
                        kain_host::reflect::FieldSchema::new(
                            "y",
                            kain_host::reflect::TypeRef::Primitive(
                                kain_host::reflect::PrimitiveType::Float,
                            ),
                        ),
                    ],
                },
            )
        }
    }

    #[test]
    fn build_exports_engine_module_into_configured_directory() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system time")
            .as_nanos();
        let export_dir = env::temp_dir().join(format!("kain_build_export_{unique}"));

        let mut build = EngineModuleBuild::with_config(EngineModuleBuildConfig {
            export: EngineModuleExportConfig::for_module("engine")
                .with_output_dir(&export_dir)
                .with_banner(false),
            emit_rustc_env: false,
            emit_rerun_if_env_changed: false,
        });
        build.register_type::<LocalVec3>();
        build.declare_native_fn(
            "host_double",
            vec![NativeParam::new("value", HostType::Int)],
            HostType::Int,
        );

        let result = build.build().expect("export build");
        let module_text = std::fs::read_to_string(&result.export.module_path).expect("module text");

        assert!(module_text.contains("struct Vec3:"));
        assert!(module_text.contains("fn host_double(value: Int) -> Int:"));

        std::fs::remove_dir_all(export_dir).expect("cleanup export dir");
    }
}
