//! Native binary linking for the Kain build pipeline.
//!
//! Extracted from `kain_launcher.rs` Run handler so both `kain build`
//! and `kain run` share one clang invocation path.
//!
//! ## Runtime linking
//!
//! The native C runtime is shipped as a precompiled static library
//! (`libkain_runtime.a` on Linux/macOS, `kain_runtime.lib` on Windows)
//! compiled with `-ffunction-sections -fdata-sections`. When linked with
//! `-Wl,--gc-sections` (Linux) or `/OPT:REF` (MSVC), the linker
//! automatically dead-strips unreferenced runtime functions.
//!
//! Programs that don't use any runtime features skip the runtime entirely
//! and link with `-nostdlib`.
//!
//! ## Emit modes
//!
//! | Emit      | Output     | Clang flags                  |
//! |-----------|-----------|------------------------------|
//! | `Exe`     | .exe      | (default)                    |
//! | `SharedLib`| .dll/.so | `-shared`                    |
//! | `StaticLib`| .lib/.a  | `-c` + `llvm-ar rcs`        |
//! | `Object`  | .obj/.o   | `-c`                         |

use kain_core::install_layout;
use kain_core::CompileTarget;
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
    /// Extra clang arguments appended before the LLVM IR input.
    /// Used by `kain run` to pass toolchain tuning flags (opt level,
    /// target CPU, debug info, section flags, etc.).
    pub extra_args: Vec<String>,
    /// Kain compile target (affects linker flags for bare metal, etc.).
    pub compile_target: CompileTarget,
}

// ── Precompiled runtime archive ──────────────────────────────────────

/// Resolve path to the precompiled native runtime static library.
///
/// Priority:
/// 1. `KAIN_RUNTIME_LIB_PATH` env var (explicit path)
/// 2. `~/.kain/lib/libkain_runtime.a` or `kain_runtime.lib` (toolchain install)
pub fn resolve_precompiled_runtime_archive() -> Option<PathBuf> {
    if let Ok(path) = std::env::var(install_layout::KAIN_RUNTIME_LIB_ENV_VAR) {
        let p = PathBuf::from(path);
        if p.exists() {
            return Some(p);
        }
    }

    if let Some(layout) = install_layout::default_kain_install_layout() {
        let lib_name = if cfg!(windows) {
            "kain_runtime.lib"
        } else {
            "libkain_runtime.a"
        };
        let candidate = layout.lib_dir.join(lib_name);
        if candidate.exists() {
            return Some(candidate);
        }
        let fallback = layout.home_dir.join("lib").join(lib_name);
        if fallback.exists() {
            return Some(fallback);
        }
    }

    None
}

// ── Clang discovery ──────────────────────────────────────────────────

/// Find clang via the canonical discovery function in install_layout.
pub fn find_clang() -> Option<String> {
    install_layout::find_clang().map(|p| p.to_string_lossy().to_string())
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

/// Resolve path to the precompiled freestanding runtime static library.
///
/// Priority:
/// 1. `KAIN_RUNTIME_CORE_LIB_PATH` env var (explicit path)
/// 2. `~/.kain/lib/libkain_runtime_core.a` or `kain_runtime_core.lib`
pub fn resolve_precompiled_freestanding_runtime_archive() -> Option<PathBuf> {
    if let Ok(path) = std::env::var("KAIN_RUNTIME_CORE_LIB_PATH") {
        let p = PathBuf::from(path);
        if p.exists() {
            return Some(p);
        }
    }

    if let Some(layout) = install_layout::default_kain_install_layout() {
        let lib_name = if cfg!(windows) {
            "kain_runtime_core.lib"
        } else {
            "libkain_runtime_core.a"
        };
        let candidate = layout.lib_dir.join(lib_name);
        if candidate.exists() {
            return Some(candidate);
        }
        let fallback = layout.home_dir.join("lib").join(lib_name);
        if fallback.exists() {
            return Some(fallback);
        }
    }

    None
}

/// Platform link libraries for bare-metal targets — always empty.
pub fn bare_metal_link_libs() -> Vec<&'static str> {
    Vec::new()
}

// ── Link ─────────────────────────────────────────────────────────────

/// Link LLVM IR into a native binary.
///
/// Returns the path to the output binary on success.
pub fn link_native_binary(req: &NativeLinkRequest<'_>) -> Result<PathBuf, String> {
    let clang = find_clang().unwrap_or_else(|| "clang".to_string());

    let uses_runtime = source_uses_runtime(req.source_text);
    let has_artifacts = !req.runtime_artifacts.loose_objects.is_empty()
        || !req.runtime_artifacts.static_archives.is_empty();

    let needs_libc = if has_artifacts {
        // Caller supplied explicit runtime artifacts.
        true
    } else if uses_runtime {
        // Source uses runtime features but no artifacts supplied.
        // Try to find the precompiled runtime archive in the toolchain.
        false // Will be resolved inside the emit-specific linker
    } else {
        false // Pure compute, no runtime needed
    };

    match req.emit {
        NativeEmit::Exe => link_exe(&clang, req, uses_runtime, needs_libc),
        NativeEmit::SharedLib => link_shared_lib(&clang, req, uses_runtime, needs_libc),
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

fn link_exe(clang: &str, req: &NativeLinkRequest<'_>, uses_runtime: bool, needs_libc: bool) -> Result<PathBuf, String> {
    let mut cmd = base_clang_command(clang, req);

    // Append caller-supplied extra args (toolchain tuning, etc.)
    for arg in &req.extra_args {
        cmd.arg(arg);
    }

    // Bare metal target-specific flags
    if req.compile_target == CompileTarget::BareMetal {
        cmd.arg("-target").arg("x86_64-unknown-none");
        cmd.arg("-ffreestanding");
        cmd.arg("-nostdlib");
    }

    let has_artifacts = !req.runtime_artifacts.loose_objects.is_empty()
        || !req.runtime_artifacts.static_archives.is_empty();

    if req.compile_target == CompileTarget::BareMetal {
        // Already added -nostdlib -ffreestanding above; skip the normal host-OS branch
    } else if !uses_runtime && !has_artifacts {
        // Pure compute — no C runtime needed
        cmd.arg("-nostdlib");
        #[cfg(windows)]
        cmd.arg("-Wl,/entry:main");
        #[cfg(not(windows))]
        cmd.arg("-Wl,-e,main");
    } else if has_artifacts {
        // Explicit runtime artifacts supplied
        for obj in &req.runtime_artifacts.loose_objects {
            cmd.arg(obj);
        }
        for archive in &req.runtime_artifacts.static_archives {
            cmd.arg(archive);
        }
    } else if uses_runtime {
        // Source uses runtime but no artifacts supplied.
        // Try the precompiled archive from the toolchain.
        if let Some(archive) = resolve_precompiled_runtime_archive() {
            cmd.arg(&archive);
        } else {
            return Err(
                "source uses runtime features but no native runtime library found. "
                .to_string()
                + "Ensure a precompiled runtime archive exists at $KAIN_HOME/lib/kain_runtime.lib "
                + "(or set KAIN_RUNTIME_LIB_PATH). If developing from the monorepo, "
                + "run `python scripts/python/kain_bazel_sync.py sync`."
            );
        }
    }

    if req.compile_target != CompileTarget::BareMetal {
        #[cfg(windows)]
        cmd.arg("-Wl,/subsystem:console");
    }

    cmd.arg(req.llvm_ir_path);
    cmd.arg("-o").arg(req.output_path);

    // Add linker dead-stripping flags
    #[cfg(target_os = "linux")]
    cmd.arg("-Wl,--gc-sections");
    #[cfg(target_os = "macos")]
    cmd.arg("-Wl,-dead_strip");
    #[cfg(windows)]
    cmd.arg("-Wl,/OPT:REF");

    // Default runtime link libraries for the platform
    if req.compile_target != CompileTarget::BareMetal {
        for lib in platform_link_libs() {
            cmd.arg(format!("-l{}", lib));
        }
    }

    // Also pass any explicit link libs from the request
    for lib in &req.runtime_artifacts.link_libs {
        cmd.arg(format!("-l{}", lib));
    }

    run_clang(cmd, req.output_path)
}

fn link_shared_lib(clang: &str, req: &NativeLinkRequest<'_>, uses_runtime: bool, needs_libc: bool) -> Result<PathBuf, String> {
    let mut cmd = base_clang_command(clang, req);
    cmd.arg("-shared");

    // Append caller-supplied extra args (toolchain tuning, etc.)
    for arg in &req.extra_args {
        cmd.arg(arg);
    }

    // Bare metal target-specific flags
    if req.compile_target == CompileTarget::BareMetal {
        cmd.arg("-target").arg("x86_64-unknown-none");
        cmd.arg("-ffreestanding");
        cmd.arg("-nostdlib");
    }

    let has_artifacts = !req.runtime_artifacts.loose_objects.is_empty()
        || !req.runtime_artifacts.static_archives.is_empty();

    if req.compile_target == CompileTarget::BareMetal {
        // Already added -nostdlib above
    } else if !uses_runtime && !has_artifacts {
        cmd.arg("-nostdlib");
        cmd.arg("-Wl,-noentry");
    } else if has_artifacts {
        for obj in &req.runtime_artifacts.loose_objects {
            cmd.arg(obj);
        }
        for archive in &req.runtime_artifacts.static_archives {
            cmd.arg(archive);
        }
    } else if uses_runtime {
        if let Some(archive) = resolve_precompiled_runtime_archive() {
            cmd.arg(&archive);
        } else {
            return Err(
                "source uses runtime features but no native runtime library found. "
                .to_string()
                + "Ensure a precompiled runtime archive exists at $KAIN_HOME/lib/kain_runtime.lib "
                + "(or set KAIN_RUNTIME_LIB_PATH). If developing from the monorepo, "
                + "run `python scripts/python/kain_bazel_sync.py sync`."
            );
        }
    }

    cmd.arg(req.llvm_ir_path);
    cmd.arg("-o").arg(req.output_path);

    // Add linker dead-stripping flags
    #[cfg(target_os = "linux")]
    cmd.arg("-Wl,--gc-sections");
    #[cfg(target_os = "macos")]
    cmd.arg("-Wl,-dead_strip");
    #[cfg(windows)]
    cmd.arg("-Wl,/OPT:REF");

    // Default runtime link libraries for the platform
    if req.compile_target != CompileTarget::BareMetal {
        for lib in platform_link_libs() {
            cmd.arg(format!("-l{}", lib));
        }
    }

    for lib in &req.runtime_artifacts.link_libs {
        cmd.arg(format!("-l{}", lib));
    }

    run_clang(cmd, req.output_path)
}

fn link_static_lib(clang: &str, req: &NativeLinkRequest<'_>) -> Result<PathBuf, String> {
    // Step 1: compile to object
    let obj_path = req.output_path.with_extension("obj");
    let mut compile_cmd = base_clang_command(clang, req);
    // Append caller-supplied extra args (toolchain tuning, etc.)
    for arg in &req.extra_args {
        compile_cmd.arg(arg);
    }
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
    // Append caller-supplied extra args (toolchain tuning, etc.)
    for arg in &req.extra_args {
        cmd.arg(arg);
    }
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

/// Default link libraries for the current platform.
/// These are needed by the precompiled runtime archive.
pub fn platform_link_libs() -> Vec<&'static str> {
    if cfg!(windows) {
        vec![
            "legacy_stdio_definitions",
            "user32",
            "gdi32",
            "shell32",
            "ws2_32",
            "winhttp",
            "advapi32",
        ]
    } else if cfg!(target_os = "macos") {
        Vec::new()
    } else {
        vec!["pthread", "dl", "rt", "m"]
    }
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
