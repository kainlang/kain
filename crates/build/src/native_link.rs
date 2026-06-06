//! Native binary linking for the Kain build pipeline.
//!
//! Extracted from `kain_launcher.rs` Run handler so both `kain build`
//! and `kain run` share one clang invocation path.
//!
//! ## Emit modes
//!
//! | Emit      | Output     | Clang flags                  |
//! |-----------|-----------|------------------------------|
//! | `Exe`     | .exe      | (default)                    |
//! | `SharedLib`| .dll/.so | `-shared`                    |
//! | `StaticLib`| .lib/.a  | `-c` + `llvm-ar rcs`        |
//! | `Object`  | .obj/.o   | `-c`                         |
//!
//! ## libc detection
//!
//! If the Kain source uses runtime functions (`use std::runtime`,
//! `use std::process`, etc.), the linker pulls in the native runtime
//! bundle. Pure-compute programs use `-nostdlib` and link directly.

use kain_core::install_layout;
use std::path::{Path, PathBuf};
use std::process::Command;

// ── Emit mode ────────────────────────────────────────────────────────

/// What kind of native artifact to produce.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NativeEmit {
    /// Standalone executable (.exe on Windows, no extension on Unix)
    Exe,
    /// Shared library / DLL (.dll on Windows, .so on Unix)
    SharedLib,
    /// Static library (.lib on Windows, .a on Unix)
    StaticLib,
    /// Object file (.obj on Windows, .o on Unix)
    Object,
}

impl Default for NativeEmit {
    fn default() -> Self { Self::Exe }
}

impl NativeEmit {
    /// File extension for this emit mode on the current platform.
    pub fn extension(&self) -> &'static str {
        match self {
            Self::Exe => {
                if cfg!(windows) { "exe" } else { "" }
            }
            Self::SharedLib => {
                if cfg!(windows) { "dll" } else { "so" }
            }
            Self::StaticLib => {
                if cfg!(windows) { "lib" } else { "a" }
            }
            Self::Object => {
                if cfg!(windows) { "obj" } else { "o" }
            }
        }
    }
}

// ── Compiled runtime artifacts ───────────────────────────────────────

/// Pre-compiled native runtime objects/archives passed to the linker.
#[derive(Default, Clone)]
pub struct NativeRuntimeArtifacts {
    /// Individual object files to link.
    pub loose_objects: Vec<PathBuf>,
    /// Static archives to link.
    pub static_archives: Vec<PathBuf>,
    /// Link library names (e.g. `["user32", "kernel32"]`).
    pub link_libs: Vec<String>,
}

// ── Link request ─────────────────────────────────────────────────────

/// Everything the linker needs to produce a native binary.
pub struct NativeLinkRequest<'a> {
    /// What kind of artifact to emit.
    pub emit: NativeEmit,
    /// Path to the LLVM IR (.ll) file.
    pub llvm_ir_path: &'a Path,
    /// Where to write the output binary.
    pub output_path: &'a Path,
    /// The original Kain source (used to detect runtime dependencies).
    pub source_text: &'a str,
    /// Pre-compiled runtime artifacts (empty = pure compute, uses `-nostdlib`).
    pub runtime_artifacts: NativeRuntimeArtifacts,
}

// ── Clang discovery ──────────────────────────────────────────────────

/// Find clang: bundled toolchain > `KAIN_CLANG_PATH` env > PATH > system install.
pub fn find_clang() -> Option<String> {
    // 1. Explicit env var
    if let Some(path) = install_layout::resolve_bundled_clang_path() {
        return Some(path.to_string_lossy().to_string());
    }

    // 2. PATH
    if std::process::Command::new("clang")
        .arg("--version")
        .output()
        .is_ok()
    {
        return Some("clang".to_string());
    }

    // 3. Standard system installs
    #[cfg(windows)]
    {
        let default_path = r"C:\Program Files\LLVM\bin\clang.exe";
        if Path::new(default_path).exists() {
            return Some(default_path.to_string());
        }
    }

    None
}

// ── libc detection ───────────────────────────────────────────────────

/// Heuristic: does this Kain source use the native runtime?
fn source_uses_runtime(source: &str) -> bool {
    let markers = [
        "use std::runtime",
        "use std::process",
        "use std::actor",
        "use std::fs",
        "use std::os",
        "use std::net",
        "use std::time",
        "use std::input",
        "use std::machine",
        "use std::thread",
        "use std::intent",
        "use std::gpu",
    ];
    markers.iter().any(|m| source.contains(m))
}

// ── Link ─────────────────────────────────────────────────────────────

/// Link LLVM IR into a native binary.
///
/// Returns the path to the output binary on success.
pub fn link_native_binary(req: &NativeLinkRequest<'_>) -> Result<PathBuf, String> {
    let clang = find_clang().unwrap_or_else(|| "clang".to_string());

    let needs_libc = source_uses_runtime(req.source_text) && !req.runtime_artifacts.loose_objects.is_empty();

    match req.emit {
        NativeEmit::Exe => link_exe(&clang, req, needs_libc),
        NativeEmit::SharedLib => link_shared_lib(&clang, req, needs_libc),
        NativeEmit::StaticLib => link_static_lib(&clang, req),
        NativeEmit::Object => link_object(&clang, req),
    }
}

// ── Emit-specific linkers ────────────────────────────────────────────

fn base_clang_command(clang: &str, _req: &NativeLinkRequest<'_>) -> Command {
    let mut cmd = Command::new(clang);
    cmd.arg("-O2");
    cmd.arg("-Wno-override-module");
    install_layout::apply_windows_msvc_link_env(&mut cmd);
    cmd
}

fn link_exe(clang: &str, req: &NativeLinkRequest<'_>, needs_libc: bool) -> Result<PathBuf, String> {
    let mut cmd = base_clang_command(clang, req);

    if !needs_libc {
        // Pure compute — no C runtime needed
        cmd.arg("-nostdlib");
        #[cfg(windows)]
        cmd.arg("-Wl,/entry:main");
        #[cfg(not(windows))]
        cmd.arg("-Wl,-e,main");
    } else {
        for obj in &req.runtime_artifacts.loose_objects {
            cmd.arg(obj);
        }
        for archive in &req.runtime_artifacts.static_archives {
            cmd.arg(archive);
        }
    }

    #[cfg(windows)]
    cmd.arg("-Wl,/subsystem:console");

    cmd.arg(req.llvm_ir_path);
    cmd.arg("-o").arg(req.output_path);

    for lib in &req.runtime_artifacts.link_libs {
        cmd.arg(format!("-l{}", lib));
    }

    run_clang(cmd, req.output_path)
}

fn link_shared_lib(clang: &str, req: &NativeLinkRequest<'_>, needs_libc: bool) -> Result<PathBuf, String> {
    let mut cmd = base_clang_command(clang, req);
    cmd.arg("-shared");

    if !needs_libc {
        cmd.arg("-nostdlib");
        cmd.arg("-Wl,-noentry");
    } else {
        for obj in &req.runtime_artifacts.loose_objects {
            cmd.arg(obj);
        }
        for archive in &req.runtime_artifacts.static_archives {
            cmd.arg(archive);
        }
    }

    cmd.arg(req.llvm_ir_path);
    cmd.arg("-o").arg(req.output_path);

    for lib in &req.runtime_artifacts.link_libs {
        cmd.arg(format!("-l{}", lib));
    }

    run_clang(cmd, req.output_path)
}

fn link_static_lib(clang: &str, req: &NativeLinkRequest<'_>) -> Result<PathBuf, String> {
    // Step 1: compile to object
    let obj_path = req.output_path.with_extension("obj");
    let mut compile_cmd = base_clang_command(clang, req);
    compile_cmd.arg("-c");
    compile_cmd.arg(req.llvm_ir_path);
    compile_cmd.arg("-o").arg(&obj_path);

    let status = compile_cmd.status().map_err(|e| format!("clang -c failed: {e}"))?;
    if !status.success() {
        return Err(format!("clang -c exited with {status}"));
    }

    // Step 2: archive
    let ar = find_ar();
    let ar_status = Command::new(&ar)
        .args(["rcs"])
        .arg(req.output_path)
        .arg(&obj_path)
        .status()
        .map_err(|e| format!("{ar} failed: {e}"))?;

    // Cleanup
    let _ = std::fs::remove_file(&obj_path);

    if !ar_status.success() {
        return Err(format!("{ar} exited with {ar_status}"));
    }

    Ok(req.output_path.to_path_buf())
}

fn link_object(clang: &str, req: &NativeLinkRequest<'_>) -> Result<PathBuf, String> {
    let mut cmd = base_clang_command(clang, req);
    cmd.arg("-c");
    cmd.arg(req.llvm_ir_path);
    cmd.arg("-o").arg(req.output_path);

    run_clang(cmd, req.output_path)
}

fn run_clang(mut cmd: Command, output: &Path) -> Result<PathBuf, String> {
    let status = cmd.status().map_err(|e| format!("clang invocation failed: {e}"))?;
    if !status.success() {
        return Err(format!("clang exited with {status}"));
    }
    Ok(output.to_path_buf())
}

fn find_ar() -> String {
    for name in &["llvm-ar", "llvm-ar.exe", "ar", "ar.exe"] {
        if std::process::Command::new(name)
            .arg("--version")
            .output()
            .is_ok()
        {
            return name.to_string();
        }
    }
    "llvm-ar".to_string()
}

// ── Tests ─────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn emit_extensions_are_platform_appropriate() {
        if cfg!(windows) {
            assert_eq!(NativeEmit::Exe.extension(), "exe");
            assert_eq!(NativeEmit::SharedLib.extension(), "dll");
            assert_eq!(NativeEmit::StaticLib.extension(), "lib");
            assert_eq!(NativeEmit::Object.extension(), "obj");
        } else {
            assert_eq!(NativeEmit::Exe.extension(), "");
            assert_eq!(NativeEmit::SharedLib.extension(), "so");
            assert_eq!(NativeEmit::StaticLib.extension(), "a");
            assert_eq!(NativeEmit::Object.extension(), "o");
        }
    }

    #[test]
    fn default_emit_is_exe() {
        assert_eq!(NativeEmit::default(), NativeEmit::Exe);
    }

    #[test]
    fn runtime_markers_are_detected() {
        assert!(source_uses_runtime("use std::runtime\nfn main() -> Int: return 0"));
        assert!(source_uses_runtime("use std::process\nuse std::fs"));
        assert!(!source_uses_runtime("fn main() -> Int:\n    return 0"));
        assert!(!source_uses_runtime("fn is_prime(n: Int) -> Bool:\n    return true"));
    }
}
