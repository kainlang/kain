//! KAIN CLI Library
//!
//! Re-exports core compiler functionality and keeps the CLI surface thin over
//! the embeddable `kain-driver` crate.

// Re-export core compiler
pub use kain_core::*;

// CLI-specific modules
pub mod error;
pub mod fabric;
#[cfg(all(feature = "gpu", feature = "sys"))]
pub mod gpu_artifacts;
pub mod import_asm;
pub mod import_c;
pub mod import_crate;
pub mod import_rust;
pub mod import_typescript;
pub mod llvm_native_stage;
pub mod lsp;
pub mod native_ui_build;
pub mod omni;
pub mod packager;
pub mod repair;
pub mod rust_build;
pub mod selfhost;
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
pub const BUILD_GIT_COMMIT_COUNT: &str = match option_env!("KAIN_GIT_COMMIT_COUNT") {
    Some(v) => v,
    None => "0",
};
pub const BUILD_GIT_DIRTY: &str = match option_env!("KAIN_GIT_DIRTY") {
    Some(v) => v,
    None => "unknown",
};

const KN_SHORTCUTS: &[&str] = &[
    "kn <file.kn>                Run a Kain file immediately",
    "kn -c \"fn main(): ...\"      Run inline Kain code",
    "Get-Content script.kn | kn   Run piped Kain source",
    "kn <file.kn> --watch        Re-run on save for fast authoring",
    "kn run <file.kn>            Explicit interpret mode",
    "kn build <file.kn> -t rust  Generate Rust output",
    "kn doctor                   Inspect PATH + runtime wiring",
    "kn doctor --repair <file>    Repair a source file in place or dry-run",
    "kn doctor --repair <file> --profile aggressive",
];

const KN_PYTHON_FFI_MODULES: &[&str] = &[
    "std::python::bridge",
    "std::python::numpy",
    "std::python::pygame",
    "std::python::trimesh",
];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LauncherKind {
    Kain,
    Kn,
    Unknown,
}

impl LauncherKind {
    pub fn display_name(self) -> &'static str {
        match self {
            Self::Kn => "kn",
            Self::Kain | Self::Unknown => "kain",
        }
    }

    pub fn prefers_interpret_default(self) -> bool {
        matches!(self, Self::Kn)
    }
}

pub fn detect_launcher_from_path(path: Option<&std::path::Path>) -> LauncherKind {
    let stem = path
        .and_then(|value| value.file_stem())
        .and_then(|value| value.to_str())
        .map(|value| value.to_ascii_lowercase());
    match stem.as_deref() {
        Some("kn") => LauncherKind::Kn,
        Some("kain") => LauncherKind::Kain,
        Some(_) | None => LauncherKind::Unknown,
    }
}

pub fn should_show_launcher_menu(
    launcher: LauncherKind,
    has_command: bool,
    has_input: bool,
) -> bool {
    launcher == LauncherKind::Kn && !has_command && !has_input
}

pub fn resolve_legacy_target_alias(
    launcher: LauncherKind,
    requested_target: &str,
    has_output: bool,
) -> String {
    if launcher.prefers_interpret_default()
        && requested_target.eq_ignore_ascii_case("wasm")
        && !has_output
    {
        "run".to_string()
    } else {
        requested_target.to_string()
    }
}

pub fn render_launcher_menu(launcher: LauncherKind) -> Option<String> {
    if launcher != LauncherKind::Kn {
        return None;
    }

    let mut menu = String::from(" kn Quick Start\n");
    menu.push_str(" Run-first authoring is active for this launcher.\n\n");
    for line in KN_SHORTCUTS {
        menu.push_str(" ");
        menu.push_str(line);
        menu.push('\n');
    }
    menu.push('\n');
    menu.push_str(" Python FFI is already wired in:\n");
    for module in KN_PYTHON_FFI_MODULES {
        menu.push_str("   - use ");
        menu.push_str(module);
        menu.push('\n');
    }
    menu.push('\n');
    menu.push_str(" Example:\n");
    menu.push_str("   kn smoketest/python/numpy_supernova/smoke.kn\n");
    Some(menu)
}

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
    fn detects_kn_launcher_from_path() {
        let path = std::path::Path::new("C:/Users/Admin/.cargo/bin/kn.exe");
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
