use crate::install_layout::{default_kain_install_layout, KAIN_CONFIG_ENV_VAR};
use kain_lattice::{
    normalize_theme_name as normalize_lattice_theme_name,
    supported_theme_names as lattice_supported_theme_names,
};
use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::fs;
use std::io::{self, IsTerminal};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::RwLock;

const CONFIG_SCHEMA_VERSION: u32 = 1;
const DEFAULT_THEME: &str = "slate";
pub const KAIN_DIAG_CAPTURE_ENV_VAR: &str = "KAIN_DIAG_CAPTURE";
pub const KAIN_DIAG_CAPTURE_PATH_ENV_VAR: &str = "KAIN_DIAG_CAPTURE_PATH";
pub const KAIN_DIAG_CAPTURE_ANSI_ENV_VAR: &str = "KAIN_DIAG_CAPTURE_ANSI";

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum KainColorPreference {
    Auto,
    Always,
    Never,
}

impl Default for KainColorPreference {
    fn default() -> Self {
        Self::Auto
    }
}

impl KainColorPreference {
    pub fn parse_str(value: &str) -> Result<Self, String> {
        match value.trim().to_ascii_lowercase().as_str() {
            "auto" => Ok(Self::Auto),
            "always" | "on" | "yes" => Ok(Self::Always),
            "never" | "off" | "no" => Ok(Self::Never),
            other => Err(format!(
                "unknown Kain color mode `{other}`; expected auto, always, or never"
            )),
        }
    }

    pub fn should_color_stdout(self) -> bool {
        self.should_color_for_stream(io::stdout().is_terminal())
    }

    pub fn should_color_stderr(self) -> bool {
        self.should_color_for_stream(io::stderr().is_terminal())
    }

    pub fn cargo_term_color(self) -> &'static str {
        match self {
            Self::Auto => "auto",
            Self::Always => "always",
            Self::Never => "never",
        }
    }

    fn should_color_for_stream(self, is_terminal: bool) -> bool {
        match self {
            Self::Always => true,
            Self::Never => false,
            Self::Auto => {
                if std::env::var_os("NO_COLOR").is_some() {
                    return false;
                }
                if let Some(force) = std::env::var_os("CLICOLOR_FORCE") {
                    let normalized = force.to_string_lossy().trim().to_ascii_lowercase();
                    if normalized == "1" || normalized == "true" || normalized == "yes" {
                        return true;
                    }
                }
                is_terminal
            }
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum KainDiagnosticCaptureMode {
    Off,
    Failures,
}

impl Default for KainDiagnosticCaptureMode {
    fn default() -> Self {
        Self::Off
    }
}

impl KainDiagnosticCaptureMode {
    pub fn parse_str(value: &str) -> Result<Self, String> {
        match value.trim().to_ascii_lowercase().as_str() {
            "off" | "none" | "disabled" => Ok(Self::Off),
            "failures" | "errors" | "on" | "enabled" => Ok(Self::Failures),
            other => Err(format!(
                "unknown Kain diagnostics capture mode `{other}`; expected off or failures"
            )),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum KainParallelismSetting {
    Count(usize),
    Preset(String),
}

impl KainParallelismSetting {
    fn resolve(&self, available: usize) -> Result<usize, String> {
        let available = available.max(1);
        match self {
            Self::Count(count) => Ok((*count).max(1)),
            Self::Preset(name) => match name.trim().to_ascii_lowercase().as_str() {
                "smart" | "balanced" => Ok(available.saturating_sub(1).max(1)),
                "all" | "max" | "full" => Ok(available),
                "half" => Ok((available / 2).max(1)),
                "efficiency" | "eco" => Ok((available / 3).max(1)),
                other => Err(format!(
                    "unknown Kain parallelism preset `{other}`; expected smart, all, half, or efficiency"
                )),
            },
        }
    }
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct KainBuildConfigFile {
    pub jobs: Option<KainParallelismSetting>,
    pub cargo_jobs: Option<KainParallelismSetting>,
    pub native_jobs: Option<KainParallelismSetting>,
    pub native_profile: Option<String>,
    pub native_opt_level: Option<String>,
    pub native_target_cpu: Option<String>,
    pub native_debug_info: Option<bool>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct KainUiConfigFile {
    pub color: Option<KainColorPreference>,
    pub theme: Option<String>,
    pub experimental_help: Option<bool>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct KainDiagnosticsConfigFile {
    pub capture: Option<KainDiagnosticCaptureMode>,
    pub path: Option<PathBuf>,
    pub store_ansi: Option<bool>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct KainToolingConfigFile {
    pub schema: u32,
    pub build: KainBuildConfigFile,
    pub ui: KainUiConfigFile,
    pub diagnostics: KainDiagnosticsConfigFile,
}

impl Default for KainToolingConfigFile {
    fn default() -> Self {
        Self {
            schema: CONFIG_SCHEMA_VERSION,
            build: KainBuildConfigFile::default(),
            ui: KainUiConfigFile::default(),
            diagnostics: KainDiagnosticsConfigFile::default(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ResolvedKainBuildConfig {
    pub available_parallelism: usize,
    pub cargo_jobs: usize,
    pub native_jobs: usize,
    pub native_profile: Option<String>,
    pub native_opt_level: Option<String>,
    pub native_target_cpu: Option<String>,
    pub native_debug_info: Option<bool>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ResolvedKainUiConfig {
    pub color: KainColorPreference,
    pub theme: String,
    pub experimental_help: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ResolvedKainDiagnosticsConfig {
    pub capture: KainDiagnosticCaptureMode,
    pub path: PathBuf,
    pub store_ansi: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct ResolvedKainToolingConfig {
    pub source_path: Option<PathBuf>,
    pub loaded_from_disk: bool,
    pub build: ResolvedKainBuildConfig,
    pub ui: ResolvedKainUiConfig,
    pub diagnostics: ResolvedKainDiagnosticsConfig,
}

impl Default for ResolvedKainToolingConfig {
    fn default() -> Self {
        let available_parallelism = std::thread::available_parallelism()
            .map(|value| value.get())
            .unwrap_or(1)
            .max(1);
        let balanced_jobs = available_parallelism.saturating_sub(1).max(1);
        let source_path = default_kain_install_layout().map(|layout| layout.config_path);
        Self {
            source_path: source_path.clone(),
            loaded_from_disk: false,
            build: ResolvedKainBuildConfig {
                available_parallelism,
                cargo_jobs: balanced_jobs,
                native_jobs: balanced_jobs,
                native_profile: None,
                native_opt_level: None,
                native_target_cpu: None,
                native_debug_info: None,
            },
            ui: ResolvedKainUiConfig {
                color: KainColorPreference::Auto,
                theme: DEFAULT_THEME.to_string(),
                experimental_help: true,
            },
            diagnostics: ResolvedKainDiagnosticsConfig {
                capture: KainDiagnosticCaptureMode::Off,
                path: default_diagnostics_capture_path(source_path.as_deref()),
                store_ansi: true,
            },
        }
    }
}

static ACTIVE_TOOLING_CONFIG: Lazy<RwLock<ResolvedKainToolingConfig>> =
    Lazy::new(|| RwLock::new(ResolvedKainToolingConfig::default()));

pub fn supported_theme_names() -> &'static [&'static str] {
    lattice_supported_theme_names()
}

pub fn normalize_ui_theme_name(raw: &str) -> Result<String, String> {
    normalize_theme_name(raw)
}

pub fn resolve_kain_config_path(explicit: Option<&Path>) -> Option<PathBuf> {
    if let Some(explicit) = explicit {
        return Some(explicit.to_path_buf());
    }
    if let Some(raw) = std::env::var_os(KAIN_CONFIG_ENV_VAR) {
        let path = PathBuf::from(raw);
        if !path.as_os_str().is_empty() {
            return Some(path);
        }
    }
    default_kain_install_layout().map(|layout| layout.config_path)
}

pub fn load_kain_tooling_config(
    explicit: Option<&Path>,
) -> Result<ResolvedKainToolingConfig, String> {
    let mut resolved = ResolvedKainToolingConfig::default();
    let config_path = resolve_kain_config_path(explicit);
    resolved.source_path = config_path.clone();

    let is_explicit = explicit.is_some() || std::env::var_os(KAIN_CONFIG_ENV_VAR).is_some();
    let Some(config_path) = config_path else {
        apply_diagnostics_env_overrides(&mut resolved)?;
        return Ok(resolved);
    };

    if !config_path.exists() {
        if is_explicit {
            return Err(format!(
                "Kain config '{}' does not exist",
                config_path.display()
            ));
        }
        resolved.diagnostics.path = default_diagnostics_capture_path(Some(&config_path));
        apply_diagnostics_env_overrides(&mut resolved)?;
        return Ok(resolved);
    }

    let source = fs::read_to_string(&config_path).map_err(|err| {
        format!(
            "failed to read Kain config '{}': {err}",
            config_path.display()
        )
    })?;
    let decoded = toml::from_str::<KainToolingConfigFile>(&source).map_err(|err| {
        format!(
            "failed to parse Kain config '{}': {err}",
            config_path.display()
        )
    })?;
    if decoded.schema != CONFIG_SCHEMA_VERSION {
        return Err(format!(
            "Kain config '{}' uses schema {} but this build expects {}",
            config_path.display(),
            decoded.schema,
            CONFIG_SCHEMA_VERSION
        ));
    }

    let available_parallelism = resolved.build.available_parallelism;
    resolved.loaded_from_disk = true;
    resolved.build.cargo_jobs = decoded
        .build
        .cargo_jobs
        .as_ref()
        .or(decoded.build.jobs.as_ref())
        .map(|setting| setting.resolve(available_parallelism))
        .transpose()?
        .unwrap_or(resolved.build.cargo_jobs);
    resolved.build.native_jobs = decoded
        .build
        .native_jobs
        .as_ref()
        .or(decoded.build.jobs.as_ref())
        .map(|setting| setting.resolve(available_parallelism))
        .transpose()?
        .unwrap_or(resolved.build.native_jobs);
    resolved.build.native_profile = decoded
        .build
        .native_profile
        .as_ref()
        .map(|value| value.trim().to_ascii_lowercase())
        .filter(|value| !value.is_empty());
    resolved.build.native_opt_level = decoded
        .build
        .native_opt_level
        .as_ref()
        .map(|value| value.trim().to_ascii_lowercase())
        .filter(|value| !value.is_empty());
    resolved.build.native_target_cpu = decoded
        .build
        .native_target_cpu
        .as_ref()
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty());
    resolved.build.native_debug_info = decoded.build.native_debug_info;
    resolved.ui.color = decoded.ui.color.unwrap_or(resolved.ui.color);
    resolved.ui.theme = decoded
        .ui
        .theme
        .as_deref()
        .map(normalize_theme_name)
        .transpose()?
        .unwrap_or_else(|| resolved.ui.theme.clone());
    resolved.ui.experimental_help = decoded
        .ui
        .experimental_help
        .unwrap_or(resolved.ui.experimental_help);
    resolved.diagnostics.capture = decoded
        .diagnostics
        .capture
        .unwrap_or(resolved.diagnostics.capture);
    resolved.diagnostics.path = decoded
        .diagnostics
        .path
        .as_deref()
        .map(|path| normalize_capture_path(path, Some(config_path.as_path())))
        .unwrap_or_else(|| default_diagnostics_capture_path(Some(config_path.as_path())));
    resolved.diagnostics.store_ansi = decoded
        .diagnostics
        .store_ansi
        .unwrap_or(resolved.diagnostics.store_ansi);
    apply_diagnostics_env_overrides(&mut resolved)?;

    Ok(resolved)
}

pub fn install_active_kain_tooling_config(config: ResolvedKainToolingConfig) {
    if let Ok(mut active) = ACTIVE_TOOLING_CONFIG.write() {
        *active = config;
    }
}

pub fn active_kain_tooling_config() -> ResolvedKainToolingConfig {
    ACTIVE_TOOLING_CONFIG
        .read()
        .map(|value| value.clone())
        .unwrap_or_else(|_| ResolvedKainToolingConfig::default())
}

pub fn apply_cargo_command_defaults(command: &mut Command) {
    let config = active_kain_tooling_config();
    command
        .arg("--jobs")
        .arg(config.build.cargo_jobs.to_string());
    command.env("CARGO_BUILD_JOBS", config.build.cargo_jobs.to_string());
    command.env("CARGO_TERM_COLOR", config.ui.color.cargo_term_color());
    if matches!(config.ui.color, KainColorPreference::Always) {
        command.env("CLICOLOR_FORCE", "1");
    }
}

pub fn active_native_parallelism() -> usize {
    active_kain_tooling_config().build.native_jobs.max(1)
}

pub fn active_ui_theme_name() -> String {
    active_kain_tooling_config().ui.theme
}

pub fn active_color_preference() -> KainColorPreference {
    active_kain_tooling_config().ui.color
}

fn normalize_theme_name(raw: &str) -> Result<String, String> {
    if raw.trim().is_empty() {
        return Ok(DEFAULT_THEME.to_string());
    }
    normalize_lattice_theme_name(raw)
}

fn default_diagnostics_capture_path(config_path: Option<&Path>) -> PathBuf {
    if let Some(parent) = config_path.and_then(Path::parent) {
        return parent.join("diagnostics").join("errors.jsonl");
    }
    std::env::current_dir()
        .unwrap_or_else(|_| PathBuf::from("."))
        .join(".kain")
        .join("diagnostics")
        .join("errors.jsonl")
}

fn normalize_capture_path(raw: &Path, config_path: Option<&Path>) -> PathBuf {
    if raw.is_absolute() {
        return raw.to_path_buf();
    }
    if let Some(parent) = config_path.and_then(Path::parent) {
        return parent.join(raw);
    }
    std::env::current_dir()
        .unwrap_or_else(|_| PathBuf::from("."))
        .join(raw)
}

fn apply_diagnostics_env_overrides(resolved: &mut ResolvedKainToolingConfig) -> Result<(), String> {
    if let Some(raw) = std::env::var_os(KAIN_DIAG_CAPTURE_ENV_VAR) {
        resolved.diagnostics.capture =
            KainDiagnosticCaptureMode::parse_str(raw.to_string_lossy().as_ref())?;
    }
    if let Some(raw) = std::env::var_os(KAIN_DIAG_CAPTURE_PATH_ENV_VAR) {
        let path = PathBuf::from(raw);
        if !path.as_os_str().is_empty() {
            resolved.diagnostics.path = if path.is_absolute() {
                path
            } else {
                std::env::current_dir()
                    .unwrap_or_else(|_| PathBuf::from("."))
                    .join(path)
            };
        }
    }
    if let Some(raw) = std::env::var_os(KAIN_DIAG_CAPTURE_ANSI_ENV_VAR) {
        resolved.diagnostics.store_ansi = parse_env_bool(
            KAIN_DIAG_CAPTURE_ANSI_ENV_VAR,
            raw.to_string_lossy().as_ref(),
        )?;
    }
    Ok(())
}

fn parse_env_bool(name: &str, value: &str) -> Result<bool, String> {
    match value.trim().to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => Ok(true),
        "0" | "false" | "no" | "off" => Ok(false),
        other => Err(format!(
            "invalid boolean for {name}: `{other}`; expected true/false, on/off, yes/no, or 1/0"
        )),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use once_cell::sync::Lazy;
    use std::env;
    use std::fs;
    use std::sync::Mutex;
    use std::sync::MutexGuard;
    use tempfile::TempDir;

    static TOOLING_CONFIG_TEST_LOCK: Lazy<Mutex<()>> = Lazy::new(|| Mutex::new(()));

    fn lock_tooling_config_test() -> MutexGuard<'static, ()> {
        TOOLING_CONFIG_TEST_LOCK
            .lock()
            .unwrap_or_else(|poisoned| poisoned.into_inner())
    }

    #[test]
    fn resolves_parallelism_presets() {
        let _guard = lock_tooling_config_test();
        assert_eq!(
            KainParallelismSetting::Preset("all".to_string())
                .resolve(8)
                .expect("all"),
            8
        );
        assert_eq!(
            KainParallelismSetting::Preset("half".to_string())
                .resolve(8)
                .expect("half"),
            4
        );
        assert_eq!(
            KainParallelismSetting::Preset("smart".to_string())
                .resolve(8)
                .expect("smart"),
            7
        );
    }

    #[test]
    fn loads_config_file_and_resolves_overrides() {
        let _guard = lock_tooling_config_test();
        let temp_dir = TempDir::new().expect("temp dir");
        let config_path = temp_dir.path().join("config.toml");
        fs::write(
            &config_path,
            r#"
schema = 1

[build]
jobs = "half"
cargo_jobs = 3
native_profile = "benchmark-release"
native_target_cpu = "native"

[ui]
color = "always"
theme = "ember"
experimental_help = false

[diagnostics]
capture = "failures"
path = "logs/errors.jsonl"
store_ansi = false
"#,
        )
        .expect("write config");

        let config = load_kain_tooling_config(Some(&config_path)).expect("config loads");

        assert!(config.loaded_from_disk);
        assert_eq!(config.source_path, Some(config_path.clone()));
        assert_eq!(config.build.cargo_jobs, 3);
        assert_eq!(
            config.build.native_profile.as_deref(),
            Some("benchmark-release")
        );
        assert_eq!(config.build.native_target_cpu.as_deref(), Some("native"));
        assert_eq!(config.ui.color, KainColorPreference::Always);
        assert_eq!(config.ui.theme, "sandstone");
        assert!(!config.ui.experimental_help);
        assert_eq!(
            config.diagnostics.capture,
            KainDiagnosticCaptureMode::Failures
        );
        assert_eq!(
            config.diagnostics.path,
            config_path
                .parent()
                .expect("config parent")
                .join("logs")
                .join("errors.jsonl")
        );
        assert!(!config.diagnostics.store_ansi);
    }

    #[test]
    fn legacy_theme_aliases_normalize_to_canonical_names() {
        let _guard = lock_tooling_config_test();
        assert_eq!(
            normalize_ui_theme_name("lattice").expect("lattice canonical theme"),
            "lattice"
        );
        assert_eq!(
            normalize_ui_theme_name("hyperpop").expect("hyperpop alias"),
            "slate"
        );
        assert_eq!(
            normalize_ui_theme_name("oxide").expect("oxide alias"),
            "graphite"
        );
        assert_eq!(
            normalize_ui_theme_name("glacier").expect("glacier alias"),
            "arctic"
        );
        assert_eq!(
            normalize_ui_theme_name("ember").expect("ember alias"),
            "sandstone"
        );
    }

    #[test]
    fn explicit_missing_config_is_an_error() {
        let _guard = lock_tooling_config_test();
        let temp_dir = TempDir::new().expect("temp dir");
        let config_path = temp_dir.path().join("missing.toml");
        let error = load_kain_tooling_config(Some(&config_path)).expect_err("missing config");
        assert!(error.contains("does not exist"));
    }

    #[test]
    fn env_override_config_path_is_respected() {
        let _guard = lock_tooling_config_test();
        let temp_dir = TempDir::new().expect("temp dir");
        let config_path = temp_dir.path().join("config.toml");
        fs::write(&config_path, "schema = 1\n").expect("write config");

        let previous = env::var_os(KAIN_CONFIG_ENV_VAR);
        env::set_var(KAIN_CONFIG_ENV_VAR, &config_path);

        let resolved = load_kain_tooling_config(None).expect("env config");

        match previous {
            Some(value) => env::set_var(KAIN_CONFIG_ENV_VAR, value),
            None => env::remove_var(KAIN_CONFIG_ENV_VAR),
        }

        assert_eq!(resolved.source_path, Some(config_path));
        assert!(resolved.loaded_from_disk);
    }

    #[test]
    fn diagnostics_env_overrides_capture_path_and_ansi() {
        let _guard = lock_tooling_config_test();
        let temp_dir = TempDir::new().expect("temp dir");
        let config_path = temp_dir.path().join("config.toml");
        fs::write(
            &config_path,
            r#"
schema = 1

[diagnostics]
capture = "off"
"#,
        )
        .expect("write config");

        let previous_config = env::var_os(KAIN_CONFIG_ENV_VAR);
        let previous_capture = env::var_os(KAIN_DIAG_CAPTURE_ENV_VAR);
        let previous_path = env::var_os(KAIN_DIAG_CAPTURE_PATH_ENV_VAR);
        let previous_ansi = env::var_os(KAIN_DIAG_CAPTURE_ANSI_ENV_VAR);
        env::set_var(KAIN_CONFIG_ENV_VAR, &config_path);
        env::set_var(KAIN_DIAG_CAPTURE_ENV_VAR, "failures");
        env::set_var(KAIN_DIAG_CAPTURE_PATH_ENV_VAR, "capture\\errors.jsonl");
        env::set_var(KAIN_DIAG_CAPTURE_ANSI_ENV_VAR, "false");

        let resolved = load_kain_tooling_config(None).expect("env override config");

        match previous_config {
            Some(value) => env::set_var(KAIN_CONFIG_ENV_VAR, value),
            None => env::remove_var(KAIN_CONFIG_ENV_VAR),
        }
        match previous_capture {
            Some(value) => env::set_var(KAIN_DIAG_CAPTURE_ENV_VAR, value),
            None => env::remove_var(KAIN_DIAG_CAPTURE_ENV_VAR),
        }
        match previous_path {
            Some(value) => env::set_var(KAIN_DIAG_CAPTURE_PATH_ENV_VAR, value),
            None => env::remove_var(KAIN_DIAG_CAPTURE_PATH_ENV_VAR),
        }
        match previous_ansi {
            Some(value) => env::set_var(KAIN_DIAG_CAPTURE_ANSI_ENV_VAR, value),
            None => env::remove_var(KAIN_DIAG_CAPTURE_ANSI_ENV_VAR),
        }

        assert_eq!(
            resolved.diagnostics.capture,
            KainDiagnosticCaptureMode::Failures
        );
        assert_eq!(
            resolved.diagnostics.path,
            env::current_dir()
                .expect("cwd")
                .join("capture")
                .join("errors.jsonl")
        );
        assert!(!resolved.diagnostics.store_ansi);
    }

    #[test]
    fn repo_local_kain_home_config_is_discovered_from_current_directory() {
        let _guard = lock_tooling_config_test();
        let temp_dir = TempDir::new().expect("temp dir");
        let repo_root = temp_dir.path().join("repo");
        let nested = repo_root.join("smoketest").join("src");
        let config_path = repo_root.join(".kain").join("config.toml");
        fs::create_dir_all(config_path.parent().expect("config parent")).expect("kain home dir");
        fs::create_dir_all(&nested).expect("nested dirs");
        fs::write(
            &config_path,
            r#"
schema = 1

[ui]
theme = "graphite"
"#,
        )
        .expect("write config");

        let previous_cwd = env::current_dir().expect("cwd");
        let previous_config = env::var_os(KAIN_CONFIG_ENV_VAR);
        let previous_home = env::var_os(crate::install_layout::KAIN_HOME_ENV_VAR);
        let previous_repo_root = env::var_os(crate::install_layout::KAIN_REPO_ROOT_ENV_VAR);
        env::set_current_dir(&nested).expect("set cwd");
        env::remove_var(KAIN_CONFIG_ENV_VAR);
        env::remove_var(crate::install_layout::KAIN_HOME_ENV_VAR);
        env::remove_var(crate::install_layout::KAIN_REPO_ROOT_ENV_VAR);

        let resolved = load_kain_tooling_config(None).expect("repo local config");

        env::set_current_dir(previous_cwd).expect("restore cwd");
        match previous_config {
            Some(value) => env::set_var(KAIN_CONFIG_ENV_VAR, value),
            None => env::remove_var(KAIN_CONFIG_ENV_VAR),
        }
        match previous_home {
            Some(value) => env::set_var(crate::install_layout::KAIN_HOME_ENV_VAR, value),
            None => env::remove_var(crate::install_layout::KAIN_HOME_ENV_VAR),
        }
        match previous_repo_root {
            Some(value) => env::set_var(crate::install_layout::KAIN_REPO_ROOT_ENV_VAR, value),
            None => env::remove_var(crate::install_layout::KAIN_REPO_ROOT_ENV_VAR),
        }

        assert_eq!(resolved.source_path, Some(config_path));
        assert!(resolved.loaded_from_disk);
        assert_eq!(resolved.ui.theme, "graphite");
    }
}
