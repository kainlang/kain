//! KAIN CLI Library
//!
//! Re-exports core compiler functionality and keeps the CLI surface thin over
//! the embeddable `kain-driver` crate.

extern crate self as cli;

// Re-export core compiler
pub use kain_core::*;

// CLI-specific modules
pub mod amalgamate;
pub mod blade_launcher;
pub mod blades;
pub mod bridge;
pub mod clean;
pub mod cli_boot;
pub mod codebase;
pub mod error;
pub mod fabric;
#[cfg(all(feature = "gpu", feature = "sys"))]
pub mod gpu_artifacts;
pub mod import_asm;
pub mod import_c;
pub mod import_crate;
pub mod import_platform;
pub mod import_rust;
#[cfg(feature = "typescript-import")]
pub mod import_typescript;
#[cfg(not(feature = "typescript-import"))]
pub mod import_typescript {
    use std::path::{Path, PathBuf};

    use crate::error::{KainError, KainResult};

    #[derive(Debug, Clone)]
    pub struct ImportTypeScriptBatchOptions {
        pub recursive: bool,
        pub flat: bool,
        pub include_filters: Vec<String>,
        pub exclude_filters: Vec<String>,
        pub fail_fast: bool,
        pub strict_generated_output: bool,
        pub report_json: Option<PathBuf>,
    }

    impl Default for ImportTypeScriptBatchOptions {
        fn default() -> Self {
            Self {
                recursive: true,
                flat: false,
                include_filters: Vec::new(),
                exclude_filters: Vec::new(),
                fail_fast: false,
                strict_generated_output: false,
                report_json: None,
            }
        }
    }

    pub fn import_typescript_with_batch(
        _input: &Path,
        _output: Option<&Path>,
        _target: Option<&str>,
        _batch: &ImportTypeScriptBatchOptions,
    ) -> KainResult<()> {
        Err(KainError::runtime(
            "TypeScript import support is disabled in this Kain build. Rebuild cli with the `typescript-import` feature to enable `kain import-ts`.".to_string(),
        ))
    }
}
pub mod kain_launcher;
pub mod llvm_native_stage;
pub mod lsp;
pub mod native_ui_build;
pub mod native_ui_dev;
pub mod omni;
pub mod packager;
pub mod packages;
pub mod repair;
pub mod run;
pub mod runtime_tools;
pub mod rust_build;
pub mod selfhost;
pub mod selfhost_bootstrap;
pub mod selfhost_profile;
pub mod selfhost_report;

// Constants
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
pub const LANGUAGE_NAME: &str = "KAIN";
pub const BUILD_NUMBER: &str = match option_env!("KAIN_BUILD_NUMBER") {
    Some(v) => v,
    None => "dev",
};
pub const BUILD_UNIX_TIME: &str = match option_env!("KAIN_BUILD_UNIX_TIME") {
    Some(v) => v,
    None => "0",
};
pub const BUILD_TRACKING_MODE: &str = match option_env!("KAIN_BUILD_TRACKING_MODE") {
    Some(v) => v,
    None => "unmanaged",
};
pub const BUILD_PROFILE: &str = match option_env!("KAIN_BUILD_PROFILE") {
    Some(v) => v,
    None => "unknown",
};
pub const BUILD_TARGET_TRIPLE: &str = match option_env!("KAIN_BUILD_TARGET_TRIPLE") {
    Some(v) => v,
    None => "unknown",
};
pub const BUILD_HOST_TRIPLE: &str = match option_env!("KAIN_BUILD_HOST_TRIPLE") {
    Some(v) => v,
    None => "unknown",
};
pub const BUILD_GIT_SHA: &str = match option_env!("KAIN_GIT_SHA") {
    Some(v) => v,
    None => "unknown",
};

pub type HybridArtifactOutput = kain_driver::HybridArtifactOutput;
pub const BUILD_GIT_COMMIT_COUNT: &str = match option_env!("KAIN_GIT_COMMIT_COUNT") {
    Some(v) => v,
    None => "0",
};
pub const BUILD_GIT_DIRTY: &str = match option_env!("KAIN_GIT_DIRTY") {
    Some(v) => v,
    None => "unknown",
};

pub use kain_commands::shared::{
    detect_launcher_from_path, render_launcher_menu, resolve_legacy_target_alias,
    should_show_launcher_menu, LauncherKind,
};

fn default_driver_session() -> kain_driver::DriverSession {
    kain_driver::DriverSession::default()
}

pub fn parse_compile_target(alias: &str) -> Option<CompileTarget> {
    kain_driver::parse_compile_target(alias)
}

pub fn target_extension(target: CompileTarget) -> &'static str {
    kain_driver::target_extension(target)
}

pub fn supported_targets_csv() -> String {
    kain_driver::supported_targets_csv()
}

fn frontend_to_typed_program(
    source: &str,
    target: CompileTarget,
) -> Result<TypedProgram, KainError> {
    kain_driver::frontend_to_typed_program(source, target)
}

/// Compile with backend selection.
pub fn compile(source: &str, target: CompileTarget) -> Result<String, KainError> {
    kain_driver::compile(source, target)
}

pub fn format_source(source: &str) -> Result<String, KainError> {
    kain_driver::format_source(source)
}

pub fn compile_runtime_contract_bundle(
    source: &str,
    target: CompileTarget,
) -> Result<RuntimeContractBundle, KainError> {
    kain_driver::compile_runtime_contract_bundle(source, target)
}

pub fn compile_realtime_app_bundle(
    source: &str,
    target: CompileTarget,
    root_component: Option<&str>,
) -> Result<kain_driver::RealtimeAppBundleOutput, KainError> {
    kain_driver::compile_realtime_app_bundle(source, target, root_component)
}

pub fn compile_spirv_binary(source: &str) -> Result<Vec<u8>, KainError> {
    kain_driver::compile_spirv_binary(source)
}

pub fn compile_ptx_source(source: &str) -> Result<String, KainError> {
    kain_driver::compile_ptx_source(source)
}

pub fn compile_wasm_binary(source: &str) -> Result<Vec<u8>, KainError> {
    kain_driver::compile_wasm_binary(source)
}

pub fn compile_hybrid_artifacts(
    source: &str,
) -> Result<kain_driver::HybridArtifactOutput, KainError> {
    kain_driver::compile_hybrid_artifacts(source)
}

pub fn compile_shader_artifact_bundle(
    source: &str,
) -> Result<kain_driver::ShaderArtifactBundleOutput, KainError> {
    kain_driver::compile_shader_artifact_bundle(source)
}

// Helper functions for main.rs

#[cfg(feature = "ue5")]
pub fn compile_ue5(
    source: &str,
    output_name: Option<&str>,
    copyright: Option<&str>,
) -> Result<ue5::Ue5Output, KainError> {
    default_driver_session().compile_ue5(source, output_name, copyright)
}

#[cfg(feature = "ue5")]
pub fn compile_ue5_with_context(
    source: &str,
    output_name: Option<&str>,
    copyright: Option<&str>,
    metadata_dir: Option<std::path::PathBuf>,
) -> Result<ue5::Ue5Output, KainError> {
    default_driver_session().compile_ue5_with_context(source, output_name, copyright, metadata_dir)
}

#[cfg(feature = "ue5")]
pub fn generate_usf_header(source: &str, shader_name: &str) -> Result<String, KainError> {
    default_driver_session().generate_usf_header(source, shader_name)
}

#[cfg(feature = "ue5")]
pub fn generate_usf_implementation(
    source: &str,
    shader_name: &str,
    plugin_name: &str,
) -> Result<String, KainError> {
    default_driver_session().generate_usf_implementation(source, shader_name, plugin_name)
}

#[cfg(feature = "ue5")]
pub fn compile_ue5editor(
    source: &str,
    plugin_name: &str,
    copyright: Option<&str>,
) -> Result<ue5_editor::Ue5EditorOutput, KainError> {
    default_driver_session().compile_ue5editor(source, plugin_name, copyright)
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

    #[test]
    fn parse_cuda_ptx_aliases() {
        assert_eq!(parse_compile_target("cuda"), Some(CompileTarget::Cuda));
        assert_eq!(parse_compile_target("ptx"), Some(CompileTarget::Cuda));
        assert_eq!(parse_compile_target("nvptx"), Some(CompileTarget::Cuda));
    }

    #[test]
    fn extension_for_cuda_target_is_ptx() {
        assert_eq!(target_extension(CompileTarget::Cuda), "ptx");
    }

    #[test]
    fn detects_kn_launcher_from_path() {
        let path = std::path::Path::new("C:/Users/Admin/.kain/bin/kn.exe");
        assert_eq!(detect_launcher_from_path(Some(path)), LauncherKind::Kn);
    }

    #[test]
    fn kn_legacy_mode_defaults_to_run_without_output() {
        assert_eq!(
            resolve_legacy_target_alias(LauncherKind::Kn, "wasm", false),
            "run"
        );
        assert_eq!(
            resolve_legacy_target_alias(LauncherKind::Kn, "rust", false),
            "rust"
        );
        assert_eq!(
            resolve_legacy_target_alias(LauncherKind::Kn, "wasm", true),
            "wasm"
        );
    }

    #[test]
    fn only_kn_without_args_shows_launcher_menu() {
        assert!(should_show_launcher_menu(LauncherKind::Kn, false, false));
        assert!(!should_show_launcher_menu(LauncherKind::Kn, true, false));
        assert!(!should_show_launcher_menu(LauncherKind::Kn, false, true));
        assert!(!should_show_launcher_menu(LauncherKind::Kain, false, false));
    }
}
