//! Comprehensive Python Runtime Discovery & Diagnostics for KAIN
//!
//! Provides enterprise-grade, deterministic resolution of Python interpreters,
//! standard library roots, and dynamic libraries across Windows and Unix platforms.
//!
//! Supports:
//! - Explicit environment overrides: KAIN_PYTHON_HOME, KAIN_PYTHON_VENV, KAIN_PYTHON_EXE, KAIN_PYTHON_DLL
//! - Process PYTHONHOME validation (filtering stale/nonexistent drives)
//! - Kain tooling configuration (`kain.toml` [python] section)
//! - Virtualenv auto-detection (.venv, venv)
//! - PATH resolution (including Scoop shims)
//! - Well-known platform install directories (Scoop, official installers, pyenv)
//! - Windows Registry inspection and stale path detection (e.g. decommissioned drive letters)
//! - Environment auto-sanitization before embedded Python initialization

use serde::{Deserialize, Serialize};
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum PythonSourceKind {
    EnvKainHome,
    EnvKainVenv,
    EnvKainExe,
    EnvKainDll,
    EnvPythonHome,
    KainConfig,
    VirtualEnv,
    PathLookup,
    Scoop,
    WindowsStandard,
    UnixStandard,
    WindowsRegistry,
}

impl PythonSourceKind {
    pub fn display_name(self) -> &'static str {
        match self {
            Self::EnvKainHome => "environment (KAIN_PYTHON_HOME)",
            Self::EnvKainVenv => "environment (KAIN_PYTHON_VENV)",
            Self::EnvKainExe => "environment (KAIN_PYTHON_EXE)",
            Self::EnvKainDll => "environment (KAIN_PYTHON_DLL)",
            Self::EnvPythonHome => "environment (PYTHONHOME)",
            Self::KainConfig => "Kain config (kain.toml [python])",
            Self::VirtualEnv => "local virtualenv (.venv)",
            Self::PathLookup => "system PATH",
            Self::Scoop => "Scoop installation",
            Self::WindowsStandard => "Windows standard installation",
            Self::UnixStandard => "Unix standard installation",
            Self::WindowsRegistry => "Windows Registry",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct PythonInstallation {
    pub home: PathBuf,
    pub exe: Option<PathBuf>,
    pub dll: Option<PathBuf>,
    pub stdlib: Option<PathBuf>,
    pub version: Option<String>,
    pub source: PythonSourceKind,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct PythonDiagnostic {
    pub is_available: bool,
    pub installation: Option<PythonInstallation>,
    pub stale_registry_entries: Vec<String>,
    pub warnings: Vec<String>,
    pub search_paths_tried: Vec<PathBuf>,
}

/// Discover and diagnose Python availability without side effects.
pub fn diagnose_python() -> PythonDiagnostic {
    let mut diag = PythonDiagnostic::default();

    // 1. Check Windows registry for stale entries (e.g. pointing to dead drives like F:\)
    if cfg!(windows) {
        check_windows_registry_stale_entries(&mut diag);
    }

    // 2. Try explicit Kain env vars
    if let Some(home) = env::var_os("KAIN_PYTHON_HOME") {
        let p = PathBuf::from(home);
        diag.search_paths_tried.push(p.clone());
        if p.exists() {
            if let Some(install) = inspect_candidate_home(&p, PythonSourceKind::EnvKainHome) {
                diag.installation = Some(install);
                diag.is_available = true;
                return diag;
            }
        } else {
            diag.warnings.push(format!(
                "KAIN_PYTHON_HOME is set to '{}' but the path does not exist on disk.",
                p.display()
            ));
        }
    }

    if let Some(venv) = env::var_os("KAIN_PYTHON_VENV") {
        let p = PathBuf::from(venv);
        diag.search_paths_tried.push(p.clone());
        if p.exists() {
            if let Some(install) = inspect_candidate_home(&p, PythonSourceKind::EnvKainVenv) {
                diag.installation = Some(install);
                diag.is_available = true;
                return diag;
            }
        } else {
            diag.warnings.push(format!(
                "KAIN_PYTHON_VENV is set to '{}' but the path does not exist on disk.",
                p.display()
            ));
        }
    }

    // 3. Try kain.toml tooling config
    let tooling_config = crate::tooling_config::active_kain_tooling_config();
    if let Some(cfg_home) = &tooling_config.python.home {
        diag.search_paths_tried.push(cfg_home.clone());
        if cfg_home.exists() {
            if let Some(install) = inspect_candidate_home(cfg_home, PythonSourceKind::KainConfig) {
                diag.installation = Some(install);
                diag.is_available = true;
                return diag;
            }
        } else {
            diag.warnings.push(format!(
                "Kain config specifies python.home = '{}' but the path does not exist.",
                cfg_home.display()
            ));
        }
    }

    // 4. Try PYTHONHOME from current environment (ONLY if it exists on disk)
    if let Some(pyhome) = env::var_os("PYTHONHOME") {
        let p = PathBuf::from(pyhome);
        diag.search_paths_tried.push(p.clone());
        if p.exists() {
            if let Some(install) = inspect_candidate_home(&p, PythonSourceKind::EnvPythonHome) {
                diag.installation = Some(install);
                diag.is_available = true;
                return diag;
            }
        } else {
            diag.warnings.push(format!(
                "Environment variable PYTHONHOME points to non-existent path: '{}'. Ignoring.",
                p.display()
            ));
        }
    }

    // 5. Try local project .venv or venv
    if let Ok(cwd) = env::current_dir() {
        let mut curr = Some(cwd.as_path());
        let mut depth = 0;
        while let Some(dir) = curr {
            if depth > 5 {
                break;
            }
            for venv_name in &[".venv", "venv"] {
                let venv_path = dir.join(venv_name);
                diag.search_paths_tried.push(venv_path.clone());
                if venv_path.is_dir() && (venv_path.join("pyvenv.cfg").exists() || venv_path.join("Lib").exists()) {
                    if let Some(install) = inspect_candidate_home(&venv_path, PythonSourceKind::VirtualEnv) {
                        diag.installation = Some(install);
                        diag.is_available = true;
                        return diag;
                    }
                }
            }
            curr = dir.parent();
            depth += 1;
        }
    }

    // 6. Try PATH lookup
    for binary_name in &["python", "python3", "python.exe", "python3.exe"] {
        if let Some(exe_path) = find_in_path(binary_name) {
            diag.search_paths_tried.push(exe_path.clone());
            if let Some(install) = inspect_candidate_executable(&exe_path, PythonSourceKind::PathLookup) {
                diag.installation = Some(install);
                diag.is_available = true;
                return diag;
            }
        }
    }

    // 7. Try well-known Scoop directories (Windows)
    if cfg!(windows) {
        let mut scoop_dirs = vec![
            PathBuf::from("C:/scoop/apps/python312/current"),
            PathBuf::from("C:/scoop/apps/python313/current"),
            PathBuf::from("C:/scoop/apps/python311/current"),
            PathBuf::from("C:/scoop/apps/python/current"),
        ];
        if let Ok(userprofile) = env::var("USERPROFILE") {
            let user_scoop = PathBuf::from(userprofile).join("scoop").join("apps");
            scoop_dirs.push(user_scoop.join("python312").join("current"));
            scoop_dirs.push(user_scoop.join("python313").join("current"));
            scoop_dirs.push(user_scoop.join("python311").join("current"));
            scoop_dirs.push(user_scoop.join("python").join("current"));
        }

        for scoop_dir in scoop_dirs {
            diag.search_paths_tried.push(scoop_dir.clone());
            if scoop_dir.is_dir() {
                if let Some(install) = inspect_candidate_home(&scoop_dir, PythonSourceKind::Scoop) {
                    diag.installation = Some(install);
                    diag.is_available = true;
                    return diag;
                }
            }
        }
    }

    // 8. Try standard Windows install roots
    if cfg!(windows) {
        let win_roots = vec![
            PathBuf::from("C:/Python312"),
            PathBuf::from("C:/Python313"),
            PathBuf::from("C:/Python311"),
            PathBuf::from("C:/Python310"),
            PathBuf::from("C:/Program Files/Python312"),
            PathBuf::from("C:/Program Files/Python313"),
            PathBuf::from("C:/Program Files/Python311"),
        ];
        for win_root in win_roots {
            diag.search_paths_tried.push(win_root.clone());
            if win_root.is_dir() {
                if let Some(install) = inspect_candidate_home(&win_root, PythonSourceKind::WindowsStandard) {
                    diag.installation = Some(install);
                    diag.is_available = true;
                    return diag;
                }
            }
        }
    }

    // 9. Try standard Unix paths
    if !cfg!(windows) {
        let unix_roots = vec![
            PathBuf::from("/usr"),
            PathBuf::from("/usr/local"),
            PathBuf::from("/opt/homebrew"),
        ];
        for unix_root in unix_roots {
            diag.search_paths_tried.push(unix_root.clone());
            if unix_root.is_dir() {
                if let Some(install) = inspect_candidate_home(&unix_root, PythonSourceKind::UnixStandard) {
                    diag.installation = Some(install);
                    diag.is_available = true;
                    return diag;
                }
            }
        }
    }

    diag.is_available = false;
    diag
}

/// Sanitize process environment and apply the resolved Python installation
/// so embedded Python (PyO3) and C runtime never query a poisoned Windows registry.
pub fn apply_python_environment(install: &PythonInstallation) {
    // 1. Explicitly set PYTHONHOME to prevent CPython from looking at the registry
    env::set_var("PYTHONHOME", &install.home);
    env::set_var("KAIN_PYTHON_HOME", &install.home);

    if let Some(exe) = &install.exe {
        env::set_var("KAIN_PYTHON_EXE", exe);
    }
    if let Some(dll) = &install.dll {
        env::set_var("KAIN_PYTHON_DLL", dll);
        // On Windows, also add DLL directory to PATH so dynamic loaders succeed
        if cfg!(windows) {
            if let Some(parent) = dll.parent() {
                if let Ok(current_path) = env::var("PATH") {
                    let sep = ";";
                    let new_path = format!("{}{}{}", parent.display(), sep, current_path);
                    env::set_var("PATH", new_path);
                }
            }
        }
    }

    // 2. Sanitize PYTHONPATH: strip any nonexistent directories or dead drive references
    if let Ok(pythonpath) = env::var("PYTHONPATH") {
        let sep = if cfg!(windows) { ';' } else { ':' };
        let valid_paths: Vec<_> = pythonpath
            .split(sep)
            .map(|s| s.trim())
            .filter(|s| !s.is_empty() && Path::new(s).exists())
            .collect();

        if valid_paths.is_empty() {
            env::remove_var("PYTHONPATH");
        } else {
            env::set_var("PYTHONPATH", valid_paths.join(if cfg!(windows) { ";" } else { ":" }));
        }
    }
}

/// Inspect a directory candidate to see if it qualifies as a valid Python home.
fn inspect_candidate_home(home: &Path, source: PythonSourceKind) -> Option<PythonInstallation> {
    if !home.is_dir() {
        return None;
    }

    // Look for standard library indicators
    let stdlib_dir = if home.join("Lib").is_dir() {
        Some(home.join("Lib"))
    } else if home.join("lib").is_dir() {
        // Unix might have lib/python3.12/
        let lib_dir = home.join("lib");
        find_unix_python_lib(&lib_dir).or(Some(lib_dir))
    } else {
        None
    };

    // Verify encodings or os.py exists in stdlib if stdlib directory was found
    let stdlib_valid = if let Some(ref stdlib) = stdlib_dir {
        stdlib.join("encodings").exists()
            || stdlib.join("os.py").exists()
            || stdlib.join("os.pyc").exists()
            || has_python_zip(home)
    } else {
        has_python_zip(home)
    };

    if !stdlib_valid && !home.join("pyvenv.cfg").exists() {
        return None;
    }

    // Locate python executable
    let exe = find_python_exe_in_home(home);

    // Locate python DLL (Windows)
    let dll = if cfg!(windows) {
        find_python_dll_in_home(home)
    } else {
        None
    };

    let version = detect_python_version(exe.as_deref().or(Some(home)));

    Some(PythonInstallation {
        home: home.to_path_buf(),
        exe,
        dll,
        stdlib: stdlib_dir,
        version,
        source,
    })
}

/// Inspect an executable candidate to find its Python home and installation details.
fn inspect_candidate_executable(exe_path: &Path, source: PythonSourceKind) -> Option<PythonInstallation> {
    if !exe_path.is_file() {
        return None;
    }

    // If it's a Scoop shim, read the target path from the .shim file
    let real_exe = if cfg!(windows) {
        resolve_scoop_shim_exe(exe_path).unwrap_or_else(|| exe_path.to_path_buf())
    } else {
        exe_path.to_path_buf()
    };

    // Try parent directory as home
    if let Some(parent) = real_exe.parent() {
        if let Some(install) = inspect_candidate_home(parent, source) {
            return Some(install);
        }
        // If in Scripts/ or bin/, try grandparent
        if let Some(grandparent) = parent.parent() {
            if let Some(install) = inspect_candidate_home(grandparent, source) {
                return Some(install);
            }
        }
    }

    None
}

fn has_python_zip(home: &Path) -> bool {
    if let Ok(entries) = fs::read_dir(home) {
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().to_ascii_lowercase();
            if name.starts_with("python3") && name.ends_with(".zip") {
                return true;
            }
        }
    }
    false
}

fn find_unix_python_lib(lib_dir: &Path) -> Option<PathBuf> {
    if let Ok(entries) = fs::read_dir(lib_dir) {
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().to_ascii_lowercase();
            if name.starts_with("python3.") && entry.path().is_dir() {
                return Some(entry.path());
            }
        }
    }
    None
}

fn find_python_exe_in_home(home: &Path) -> Option<PathBuf> {
    let candidates = [
        home.join("python.exe"),
        home.join("python3.exe"),
        home.join("Scripts").join("python.exe"),
        home.join("bin").join("python"),
        home.join("bin").join("python3"),
    ];
    for cand in candidates {
        if cand.is_file() {
            return Some(cand);
        }
    }
    None
}

fn find_python_dll_in_home(home: &Path) -> Option<PathBuf> {
    // Check root
    if let Some(dll) = find_python_dll_in_dir(home) {
        return Some(dll);
    }
    // Check DLLs/
    let dlls_dir = home.join("DLLs");
    if dlls_dir.is_dir() {
        if let Some(dll) = find_python_dll_in_dir(&dlls_dir) {
            return Some(dll);
        }
    }
    None
}

fn find_python_dll_in_dir(dir: &Path) -> Option<PathBuf> {
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().to_ascii_lowercase();
            if (name.starts_with("python3") || name.starts_with("python31")) && name.ends_with(".dll") {
                return Some(entry.path());
            }
        }
    }
    None
}

fn detect_python_version(target: Option<&Path>) -> Option<String> {
    let target = target?;
    let exe = if target.is_file() {
        target.to_path_buf()
    } else {
        find_python_exe_in_home(target)?
    };

    let output = Command::new(&exe)
        .arg("--version")
        .output()
        .ok()?;

    if output.status.success() {
        let text = String::from_utf8_lossy(&output.stdout);
        let trimmed = text.trim();
        if !trimmed.is_empty() {
            return Some(trimmed.to_string());
        }
        let text_err = String::from_utf8_lossy(&output.stderr);
        let trimmed_err = text_err.trim();
        if !trimmed_err.is_empty() {
            return Some(trimmed_err.to_string());
        }
    }
    None
}

fn find_in_path(binary_name: &str) -> Option<PathBuf> {
    let path_var = env::var_os("PATH")?;
    for dir in env::split_paths(&path_var) {
        let full = dir.join(binary_name);
        if full.is_file() {
            return Some(full);
        }
        if cfg!(windows) && !binary_name.ends_with(".exe") {
            let with_exe = dir.join(format!("{binary_name}.exe"));
            if with_exe.is_file() {
                return Some(with_exe);
            }
        }
    }
    None
}

fn resolve_scoop_shim_exe(shim_exe: &Path) -> Option<PathBuf> {
    let shim_file = shim_exe.with_extension("shim");
    if shim_file.is_file() {
        if let Ok(content) = fs::read_to_string(&shim_file) {
            for line in content.lines() {
                let trimmed = line.trim();
                if trimmed.starts_with("path =") {
                    let parts: Vec<&str> = trimmed.split('=').collect();
                    if parts.len() >= 2 {
                        let target = parts[1].trim().trim_matches('"');
                        let p = PathBuf::from(target);
                        if p.exists() {
                            return Some(p);
                        }
                    }
                }
            }
        }
    }
    None
}

/// Check Windows Registry for stale Python paths pointing to non-existent drives/paths
fn check_windows_registry_stale_entries(diag: &mut PythonDiagnostic) {
    if !cfg!(windows) {
        return;
    }

    let reg_keys = [
        "HKCU\\Software\\Python\\PythonCore",
        "HKLM\\Software\\Python\\PythonCore",
    ];

    for root_key in reg_keys {
        let output = Command::new("reg")
            .args(["query", root_key, "/s"])
            .output();

        if let Ok(out) = output {
            if out.status.success() {
                let text = String::from_utf8_lossy(&out.stdout);
                let mut current_sub_key = String::new();
                for line in text.lines() {
                    let trimmed = line.trim();
                    if trimmed.starts_with("HKEY_") {
                        current_sub_key = trimmed.to_string();
                    } else if (trimmed.contains("REG_SZ") || trimmed.contains("REG_EXPAND_SZ"))
                        && (current_sub_key.ends_with("\\InstallPath") || current_sub_key.ends_with("\\PythonPath"))
                    {
                        let parts: Vec<&str> = trimmed.split_whitespace().collect();
                        if parts.len() >= 3 {
                            let val = parts[2..].join(" ");
                            for candidate in val.split(';') {
                                let cand = candidate.trim();
                                if !cand.is_empty() && (cand.contains(":\\") || cand.contains(":/")) {
                                    let cand_path = Path::new(cand);
                                    if !cand_path.exists() {
                                        let warning = format!(
                                            "{} -> '{}' (path does not exist on disk)",
                                            current_sub_key, cand
                                        );
                                        if !diag.stale_registry_entries.contains(&warning) {
                                            diag.stale_registry_entries.push(warning);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/// Save a preferred Python home directory into the user's active kain.toml tooling configuration.
pub fn save_python_home_to_config(home_path: &Path) -> Result<PathBuf, String> {
    let layout = crate::install_layout::default_kain_install_layout()
        .ok_or_else(|| "Failed to resolve Kain install layout".to_string())?;

    let config_path = layout.config_path;
    if let Some(parent) = config_path.parent() {
        fs::create_dir_all(parent)
            .map_err(|err| format!("Failed to create config directory {}: {err}", parent.display()))?;
    }

    let mut config_file: crate::tooling_config::KainToolingConfigFile = if config_path.is_file() {
        let content = fs::read_to_string(&config_path)
            .map_err(|err| format!("Failed to read {}: {err}", config_path.display()))?;
        toml::from_str(&content)
            .map_err(|err| format!("Failed to parse {}: {err}", config_path.display()))?
    } else {
        crate::tooling_config::KainToolingConfigFile::default()
    };

    config_file.python.home = Some(home_path.to_path_buf());
    if let Some(exe) = find_python_exe_in_home(home_path) {
        config_file.python.exe = Some(exe);
    }
    if cfg!(windows) {
        if let Some(dll) = find_python_dll_in_home(home_path) {
            config_file.python.dll = Some(dll);
        }
    }

    let encoded = toml::to_string_pretty(&config_file)
        .map_err(|err| format!("Failed to serialize config: {err}"))?;
    fs::write(&config_path, encoded)
        .map_err(|err| format!("Failed to write {}: {err}", config_path.display()))?;

    // Reload active config
    let reloaded = crate::tooling_config::load_kain_tooling_config(Some(&config_path))?;
    crate::tooling_config::install_active_kain_tooling_config(reloaded);

    Ok(config_path)
}

/// Clean up obsolete/stale Windows registry keys for PythonCore that point to dead drives.
pub fn clean_stale_windows_registry_entries() -> Result<Vec<String>, String> {
    if !cfg!(windows) {
        return Ok(Vec::new());
    }

    let diag = diagnose_python();
    let mut cleaned = Vec::new();

    // Look for HKCU\Software\Python\PythonCore\3.12 etc that point to non-existent paths
    for entry in &diag.stale_registry_entries {
        if let Some((key_part, _)) = entry.split_once(" -> ") {
            // Find base version key e.g. HKCU\Software\Python\PythonCore\3.12
            let parts: Vec<&str> = key_part.split('\\').collect();
            if parts.len() >= 4 {
                // Truncate to the version key e.g. HKCU\Software\Python\PythonCore\3.12
                let target_key = parts[0..4].join("\\");
                let output = Command::new("reg")
                    .args(["delete", &target_key, "/f"])
                    .output();
                if let Ok(out) = output {
                    if out.status.success() {
                        cleaned.push(target_key);
                    }
                }
            }
        }
    }

    Ok(cleaned)
}
