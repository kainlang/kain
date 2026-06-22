//! vcpkg subprocess wrapper for on-demand port installation.
//!
//! Discovers the vcpkg executable, maps Kain targets to vcpkg triples,
//! and invokes `vcpkg install` with version constraints.

use crate::config::{CLibraryConfig, CInteropTier};
use crate::model::{ManifestContext, ResolvedCLibrary};
use crate::port_overrides::header_to_port;
use crate::CLibraryImportSpec;
use kain_core::CompileTarget;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use kain_core::error::KainError;

/// Map a Kain compile target to a vcpkg triplet string.
fn triple_for_target(_target: CompileTarget) -> &'static str {
    let _ = _target;
    match _target {
        CompileTarget::Llvm | CompileTarget::Interpret | CompileTarget::Test => {
            if cfg!(target_os = "windows") {
                "x64-windows"
            } else if cfg!(target_os = "linux") {
                "x64-linux"
            } else if cfg!(target_os = "macos") {
                "x64-osx"
            } else {
                "x64-windows"
            }
        }
        CompileTarget::C | CompileTarget::Cpp => {
            if cfg!(target_os = "windows") {
                "x64-windows-static"
            } else {
                "x64-linux"
            }
        }
        _ => {
            if cfg!(target_os = "windows") {
                "x64-windows"
            } else if cfg!(target_os = "linux") {
                "x64-linux"
            } else if cfg!(target_os = "macos") {
                "x64-osx"
            } else {
                "x64-windows"
            }
        }
    }
}

/// Discover the vcpkg executable.
///
/// Checks env vars in order:
/// 1. `KAIN_VCPKG_EXE` — explicit path to vcpkg binary
/// 2. `VCPKG_ROOT` + `/vcpkg.exe` (Windows) or `/vcpkg` (Unix)
/// 3. Well-known paths: `~/.kain/vcpkg/vcpkg.exe`, and the system PATH.
fn find_vcpkg_binary() -> Result<PathBuf, KainError> {
    // 1. KAIN_VCPKG_EXE override
    if let Ok(exe) = env::var("KAIN_VCPKG_EXE") {
        let path = PathBuf::from(&exe);
        if path.is_file() {
            return Ok(path);
        }
    }

    // 2. VCPKG_ROOT
    if let Ok(root) = env::var("VCPKG_ROOT") {
        let root = PathBuf::from(&root);
        let candidate = if cfg!(target_os = "windows") {
            root.join("vcpkg.exe")
        } else {
            root.join("vcpkg")
        };
        if candidate.is_file() {
            return Ok(candidate);
        }
    }

    // 3. Well-known paths
    let home = env::var("USERPROFILE")
        .or_else(|_| env::var("HOME"))
        .unwrap_or_default();
    let well_known = if cfg!(target_os = "windows") {
        vec![
            PathBuf::from(&home).join(".kain/vcpkg/vcpkg.exe"),
            PathBuf::from("C:/vcpkg/vcpkg.exe"),
        ]
    } else {
        vec![
            PathBuf::from(&home).join(".kain/vcpkg/vcpkg"),
            PathBuf::from("/usr/local/bin/vcpkg"),
        ]
    };
    for path in &well_known {
        if path.is_file() {
            return Ok(path.clone());
        }
    }

    // 4. Fall back to PATH lookup
    let binary_name = if cfg!(target_os = "windows") {
        "vcpkg.exe"
    } else {
        "vcpkg"
    };
    if let Ok(path) = which_in_path(binary_name) {
        return Ok(path);
    }

    Err(KainError::runtime(
        "vcpkg executable not found. Set KAIN_VCPKG_EXE or VCPKG_ROOT, or install vcpkg to ~/.kain/vcpkg/",
    ))
}

/// Search for a binary in the system PATH.
fn which_in_path(binary: &str) -> Result<PathBuf, ()> {
    if let Ok(path_var) = env::var("PATH") {
        for dir in env::split_paths(&path_var) {
            let candidate = dir.join(binary);
            if candidate.is_file() {
                return Ok(candidate);
            }
        }
    }
    Err(())
}

/// Resolve the vcpkg root directory (where ports are installed).
///
/// Returns `KAIN_VCPKG_ROOT` or defaults to `~/.kain/vcpkg/`.
pub fn resolve_vcpkg_root() -> PathBuf {
    if let Ok(root) = env::var("KAIN_VCPKG_ROOT") {
        return PathBuf::from(root);
    }
    let home = env::var("USERPROFILE")
        .or_else(|_| env::var("HOME"))
        .unwrap_or_default();
    PathBuf::from(home).join(".kain/vcpkg")
}

/// Ensure a vcpkg port is installed at the given version constraint.
///
/// Returns the installed tree root (e.g., `<vcpkg_root>/installed/<triple>/`).
///
/// Uses a sentinel file (`.kain-fetch-marker`) to skip re-installation
/// when the package is already present at the required version.
pub fn ensure_installed(
    package: &str,
    triple: &str,
    version: &str,
) -> Result<PathBuf, KainError> {
    let root = resolve_vcpkg_root();
    let installed_tree = root.join("installed").join(triple);

    // Sentinel file for idempotent installs
    let sentinel_name = format!(".kain-fetch-marker-{}-{}", package, version);
    let sentinel = installed_tree.join(&sentinel_name);

    if sentinel.is_file() {
        return Ok(installed_tree);
    }

    // Ensure the root directory exists
    fs::create_dir_all(&root).map_err(|err| {
        KainError::runtime(format!("failed to create vcpkg root {}: {err}", root.display()))
    })?;

    let vcpkg_bin = find_vcpkg_binary()?;
    let _timeout_secs = env::var("KAIN_VCPKG_TIMEOUT")
        .ok()
        .and_then(|v| v.parse().ok())
        .unwrap_or(600u64);

    let mut cmd = Command::new(&vcpkg_bin);
    cmd.arg("install")
        .arg(format!("{}:{}", package, triple))
        .arg(format!("--version={}", version))
        .arg("--vcpkg-root")
        .arg(&root)
        .arg("--x-install-root")
        .arg(&installed_tree);

    // Use a subprocess timeout if available (via external crate or manual)
    let output = cmd.output().map_err(|err| {
        KainError::runtime(format!(
            "failed to launch vcpkg for '{}': {err}",
            package
        ))
    })?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(KainError::runtime(format!(
            "vcpkg install failed for '{}' version '{}': {}",
            package,
            version,
            stderr.trim()
        )));
    }

    // Write sentinel file to mark successful installation
    fs::write(&sentinel, "").map_err(|err| {
        KainError::runtime(format!(
            "failed to write vcpkg sentinel {}: {err}",
            sentinel.display()
        ))
    })?;

    Ok(installed_tree)
}

/// Attempt to resolve a versioned include via vcpkg on-demand fetch.
///
/// Called as Strategy 6 in `resolve_library_spec`. Returns `Ok(None)` if
/// the header doesn't map to a known vcpkg port (so other strategies can try).
pub fn resolve_vcpkg_fetch(
    spec: &CLibraryImportSpec,
    _start_dir: &Path,
    version: &str,
    target: CompileTarget,
) -> Result<Option<(ResolvedCLibrary, ManifestContext)>, KainError> {
    let include_target = match &spec.include_target {
        Some(t) => t.clone(),
        None => return Ok(None),
    };

    // Map the header to a vcpkg port name
    let port = header_to_port(&include_target);

    let triple = triple_for_target(target);

    // Check if vcpkg binary exists before attempting install.
    // If vcpkg is not installed, fall through to let other strategies try.
    if find_vcpkg_binary().is_err() {
        return Ok(None);
    }

    // Install the port via vcpkg. Propagate errors (network, permissions, etc.)
    // so the user sees actionable diagnostics.
    let installed_tree = ensure_installed(&port, triple, version)?;

    // Resolve header path within the installed tree
    let header_path = installed_tree.join("include").join(&include_target);
    if !header_path.is_file() {
        // Try without subdirectory: some ports install directly into include/
        let alt_header = installed_tree.join("include").join(
            Path::new(&include_target)
                .file_name()
                .unwrap_or_default(),
        );
        if alt_header.is_file() {
            return build_resolved(include_target, &port, version, &installed_tree, &alt_header);
        }
        return Ok(None);
    }

    build_resolved(include_target, &port, version, &installed_tree, &header_path)
}

/// Build a ResolvedCLibrary from vcpkg-installed paths.
fn build_resolved(
    _include_target: String,
    port: &str,
    version: &str,
    installed_tree: &Path,
    header_path: &Path,
) -> Result<Option<(ResolvedCLibrary, ManifestContext)>, KainError> {
    let lib_dir = installed_tree.join("lib");

    // Discover static libraries
    let mut static_lib_paths = Vec::new();
    if lib_dir.is_dir() {
        if let Ok(entries) = fs::read_dir(&lib_dir) {
            for entry in entries.flatten() {
                let path = entry.path();
                let ext = path
                    .extension()
                    .and_then(|e| e.to_str())
                    .unwrap_or("");
                if ext == "lib" || ext == "a" {
                    static_lib_paths.push(path);
                }
            }
        }
    }

    let import_name = port.to_string();
    let resolved = ResolvedCLibrary {
        import_name,
        manifest_root: installed_tree.to_path_buf(),
        header_path: header_path.to_path_buf(),
        shared_lib_path: None,
        source_paths: Vec::new(),
        object_paths: Vec::new(),
        static_lib_paths,
        bitcode_paths: Vec::new(),
        config: CLibraryConfig::default(),
        global_config: Default::default(),
        tier: CInteropTier::Static,
        runtime_owned: false,
        version: Some(version.to_string()),
        vcpkg_lock_sha256: None,
    };

    let manifest_ctx = ManifestContext {
        root_dir: Some(installed_tree.to_path_buf()),
        config: None,
    };

    Ok(Some((resolved, manifest_ctx)))
}
