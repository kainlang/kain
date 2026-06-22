//! vcpkg setup — one-shot `kain install c-extras` bootstrapper.
//!
//! Clones and bootstraps a portable vcpkg tree at `~/.kain/vcpkg/`.
//! Cross-platform: Windows (bootstrap-vcpkg.bat), Linux/macOS (bootstrap-vcpkg.sh).
//!
//! After successful setup, `kain doctor` will detect vcpkg, and the
//! source-owned manifest pipeline (Strategy 6 in `resolve_library_spec`)
//! will transparently fetch C headers on demand.

use std::path::{Path, PathBuf};
use std::process::Command;

use crate::KainError;

/// Default install root: `~/.kain/vcpkg/`
pub fn default_vcpkg_root() -> PathBuf {
    let home = std::env::var("USERPROFILE")
        .or_else(|_| std::env::var("HOME"))
        .unwrap_or_else(|_| ".".to_string());
    PathBuf::from(home).join(".kain").join("vcpkg")
}

/// Run the full vcpkg setup: clone + bootstrap.
pub fn setup_vcpkg(root: &Path) -> Result<(), KainError> {
    if root.join("vcpkg.exe").exists() || root.join("vcpkg").exists() {
        return Ok(()); // already installed
    }

    std::fs::create_dir_all(root).map_err(|e| {
        KainError::runtime(format!("Failed to create vcpkg root {}: {e}", root.display()))
    })?;

    // Clone vcpkg if the repo doesn't exist yet
    if !root.join(".git").exists() {
        eprint!("Cloning vcpkg (this may take a minute)... ");
        let status = Command::new("git")
            .args([
                "clone",
                "--depth=1",
                "https://github.com/microsoft/vcpkg.git",
                ".",
            ])
            .current_dir(root)
            .status()
            .map_err(|e| {
                KainError::runtime(format!(
                    "Failed to run git clone for vcpkg: {e}. \
                     Is git installed and on your PATH?"
                ))
            })?;
        if !status.success() {
            return Err(KainError::runtime(
                "git clone of vcpkg failed. Check your network connection and try again.",
            ));
        }
        eprintln!("done.");
    }

    // Bootstrap
    eprint!("Bootstrapping vcpkg... ");
    let (bootstrap_cmd, bootstrap_args) = if cfg!(windows) {
        (root.join("bootstrap-vcpkg.bat"), vec![])
    } else {
        (root.join("bootstrap-vcpkg.sh"), vec!["-disableMetrics"])
    };
    let status = Command::new(&bootstrap_cmd)
        .args(&bootstrap_args)
        .current_dir(root)
        .status()
        .map_err(|e| {
            KainError::runtime(format!(
                "Failed to run vcpkg bootstrap script {}: {e}",
                bootstrap_cmd.display()
            ))
        })?;
    if !status.success() {
        return Err(KainError::runtime(
            "vcpkg bootstrap failed. Check the output above for details.",
        ));
    }

    // Verify the binary
    let binary = if cfg!(windows) {
        root.join("vcpkg.exe")
    } else {
        root.join("vcpkg")
    };
    if !binary.exists() {
        return Err(KainError::runtime(format!(
            "vcpkg bootstrap completed but binary not found at {}. \
             Check the bootstrap output above for errors.",
            binary.display()
        )));
    }

    eprintln!("done.");

    // Quick smoke test
    let status = Command::new(&binary)
        .arg("version")
        .status()
        .map_err(|e| {
            KainError::runtime(format!(
                "vcpkg was installed but failed to run: {e}"
            ))
        })?;
    if !status.success() {
        return Err(KainError::runtime(
            "vcpkg binary was built but `vcpkg version` failed.",
        ));
    }

    eprintln!(
        "vcpkg is ready at {}.",
        root.display()
    );
    eprintln!(
        "Add `export VCPKG_ROOT={}` to your shell profile, or set KAIN_VCPKG_EXE={}.",
        root.display(),
        binary.display()
    );
    Ok(())
}

/// Discover the vcpkg binary after setup (reuses `vcpkg.rs` logic).
pub fn check_vcpkg_installed() -> Option<PathBuf> {
    // Try KAIN_VCPKG_EXE first
    if let Ok(exe) = std::env::var("KAIN_VCPKG_EXE") {
        let path = PathBuf::from(&exe);
        if path.exists() {
            return Some(path);
        }
    }
    // Try VCPKG_ROOT
    if let Ok(root) = std::env::var("VCPKG_ROOT") {
        let candidate = if cfg!(windows) {
            PathBuf::from(&root).join("vcpkg.exe")
        } else {
            PathBuf::from(&root).join("vcpkg")
        };
        if candidate.exists() {
            return Some(candidate);
        }
    }
    // Try the default Kain-home tree
    let default = default_vcpkg_root();
    let candidate = if cfg!(windows) {
        default.join("vcpkg.exe")
    } else {
        default.join("vcpkg")
    };
    if candidate.exists() {
        return Some(candidate);
    }
    None
}
