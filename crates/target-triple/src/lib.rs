//! Host/target platform description for the Kain toolchain.
//!
//! This crate is the single authority for:
//!
//! - [`TargetTriple`] — parsing and describing LLVM-style target triples
//!   (`arch-vendor-os[-env]`, e.g. `x86_64-pc-windows-msvc`).
//! - [`Platform`] — per-OS link/emit conventions (file extensions, default
//!   link libraries, linker GC flags).
//! - [`LlvmTargetId`] / [`LlvmTargetDescriptor`] — the small descriptor the
//!   LLVM codegen backend threads through IR emission (`target triple` and
//!   `target datalayout` strings plus a coarse target identity).
//!
//! It is intentionally dependency-free so every layer of the compiler
//! (build orchestration, CLI, codegen, drivers) can share it.

use std::fmt;

// ── TargetTriple ─────────────────────────────────────────────────────

/// An LLVM-style target triple: `arch-vendor-os[-env]`.
///
/// The four components are kept as plain strings so unknown/future triples
/// (e.g. `aarch64-unknown-linux-musl`, `x86_64-unknown-none`) round-trip
/// losslessly through [`TargetTriple::parse`] / [`TargetTriple::raw`].
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct TargetTriple {
    /// CPU architecture (`x86_64`, `aarch64`, …).
    pub arch: String,
    /// Vendor (`pc`, `unknown`, `apple`, …).
    pub vendor: String,
    /// Operating system (`windows`, `linux`, `darwin`, `none`, …).
    pub os: String,
    /// Environment/ABI (`msvc`, `gnu`, `musl`, …; empty when absent).
    pub env: String,
}

impl TargetTriple {
    /// Detect the triple of the machine compiling/running this code.
    pub fn host() -> Self {
        let arch = std::env::consts::ARCH.to_string();
        match std::env::consts::OS {
            "windows" => Self {
                arch,
                vendor: "pc".to_string(),
                os: "windows".to_string(),
                env: "msvc".to_string(),
            },
            "macos" => Self {
                arch,
                vendor: "apple".to_string(),
                os: "darwin".to_string(),
                env: String::new(),
            },
            os => Self {
                arch,
                vendor: "unknown".to_string(),
                os: os.to_string(),
                // `std::env::consts::OS == "linux"` covers both glibc and musl
                // hosts; default to gnu (callers targeting musl pass --triple).
                env: if os == "linux" {
                    "gnu".to_string()
                } else {
                    String::new()
                },
            },
        }
    }

    /// Parse `arch-vendor-os[-env]` (e.g. `x86_64-pc-windows-msvc`).
    ///
    /// Returns `Err(String)` describing the problem on malformed input.
    pub fn parse(triple: &str) -> Result<Self, String> {
        let parts: Vec<&str> = triple.split('-').collect();
        match parts.as_slice() {
            [arch, vendor, os] => Ok(Self {
                arch: arch.to_string(),
                vendor: vendor.to_string(),
                os: os.to_string(),
                env: String::new(),
            }),
            [arch, vendor, os, env] => {
                if arch.is_empty() || vendor.is_empty() || os.is_empty() || env.is_empty() {
                    return Err(format!("invalid target triple '{}': empty component", triple));
                }
                Ok(Self {
                    arch: arch.to_string(),
                    vendor: vendor.to_string(),
                    os: os.to_string(),
                    env: env.to_string(),
                })
            }
            _ => Err(format!(
                "invalid target triple '{}': expected arch-vendor-os[-env]",
                triple
            )),
        }
    }

    /// Canonical triple string (`arch-vendor-os` or `arch-vendor-os-env`).
    pub fn raw(&self) -> String {
        self.to_string()
    }

    /// Whether this triple targets Windows (any env).
    pub fn is_windows(&self) -> bool {
        self.os == "windows"
    }

    /// Whether this triple targets Linux (any env).
    pub fn is_linux(&self) -> bool {
        self.os == "linux"
    }

    /// Whether this triple targets macOS.
    pub fn is_macos(&self) -> bool {
        self.os == "darwin" || self.os == "macos"
    }

    /// File name of the precompiled native runtime library for this triple.
    pub fn runtime_lib_name(&self) -> &'static str {
        if self.is_windows() {
            "kain_runtime.lib"
        } else {
            "libkain_runtime.a"
        }
    }

    /// Sysroot directory name for well-known cross targets.
    ///
    /// Returns `None` for triples without a canned sysroot; callers fall back
    /// to `{os}-{arch}-{env}`.
    pub fn sysroot_name(&self) -> Option<String> {
        if self.is_linux() && self.env == "musl" {
            Some(format!("linux-{}-musl", self.arch))
        } else {
            None
        }
    }
}

impl fmt::Display for TargetTriple {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.env.is_empty() {
            write!(f, "{}-{}-{}", self.arch, self.vendor, self.os)
        } else {
            write!(f, "{}-{}-{}-{}", self.arch, self.vendor, self.os, self.env)
        }
    }
}

// ── Platform ─────────────────────────────────────────────────────────

/// Default C/C++ link libraries for Windows MSVC targets.
///
/// Covers the APIs referenced by the precompiled native runtime archive
/// (verified via `llvm-nm --undefined-only` on kain_runtime.lib): window
/// management (user32), GDI snapshots (gdi32), process/registry (advapi32),
/// argv parsing (shell32), COM/propsys (ole32), Winsock (ws2_32), WinHTTP
/// client (winhttp) and MIDI input (winmm).
static WINDOWS_LINK_LIBS: &[&str] = &[
    "kernel32", "user32", "gdi32", "advapi32", "shell32", "ole32", "uuid", "winhttp",
    "winmm", "ws2_32", "ntdll",
];

/// Default C/C++ link libraries for Linux targets.
static LINUX_LINK_LIBS: &[&str] = &["m", "pthread", "dl", "rt"];

/// Default C++ standard libraries for Linux targets.
static LINUX_CPP_LINK_LIBS: &[&str] = &["stdc++"];

/// Default C++ standard libraries for macOS targets.
static MACOS_CPP_LINK_LIBS: &[&str] = &["c++"];

/// Per-OS link/emit conventions: file extensions, default libraries and
/// linker flags shared by `kain build` and `kain run`.
///
/// All strings are `'static` so `Platform` is `Copy` and freely threading
/// through link planning is cheap.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Platform {
    /// Executable extension without dot (`exe` on Windows, `` elsewhere).
    pub exe_extension: &'static str,
    /// Shared library extension without dot (`dll` / `so` / `dylib`).
    pub shared_lib_extension: &'static str,
    /// Static archive extension without dot (`lib` / `a`).
    pub static_lib_extension: &'static str,
    /// Object file extension without dot (`obj` / `o`).
    pub object_extension: &'static str,
    /// Default system libraries linked with the runtime archive.
    pub link_libs: &'static [&'static str],
    /// Default C++ standard libraries for mixed C++ links.
    pub cpp_link_libs: &'static [&'static str],
    /// Windows subsystem flag passed as `-Wl,{flag}` (e.g. `subsystem:console`).
    pub subsystem_flag: Option<&'static str>,
    /// Dead-strip/GC-sections flag passed as `-Wl,{flag}`.
    pub dead_strip_flag: &'static str,
    /// Debug-suppression flag passed as `-Wl,{flag}` when debug info is off.
    pub debug_none_flag: Option<&'static str>,
    /// Identical-code-folding flag passed as `-Wl,{flag}` in release builds.
    pub icf_flag: Option<&'static str>,
}

impl Platform {
    /// Platform conventions of the machine running this code.
    pub fn host() -> Self {
        Self::for_triple(&TargetTriple::host())
    }

    /// Platform conventions for a (possibly cross-compiled) target triple.
    pub fn for_triple(triple: &TargetTriple) -> Self {
        if triple.is_windows() {
            Self {
                exe_extension: "exe",
                shared_lib_extension: "dll",
                static_lib_extension: "lib",
                object_extension: "obj",
                link_libs: WINDOWS_LINK_LIBS,
                cpp_link_libs: &[],
                subsystem_flag: Some("/subsystem:console"),
                dead_strip_flag: "/OPT:REF",
                debug_none_flag: Some("/DEBUG:NONE"),
                icf_flag: Some("/OPT:ICF"),
            }
        } else if triple.is_macos() {
            Self {
                exe_extension: "",
                shared_lib_extension: "dylib",
                static_lib_extension: "a",
                object_extension: "o",
                link_libs: &[],
                cpp_link_libs: MACOS_CPP_LINK_LIBS,
                subsystem_flag: None,
                dead_strip_flag: "-dead_strip",
                debug_none_flag: None,
                icf_flag: None,
            }
        } else {
            // Linux and other Unix-like / bare-metal-ish targets.
            Self {
                exe_extension: "",
                shared_lib_extension: "so",
                static_lib_extension: "a",
                object_extension: "o",
                link_libs: LINUX_LINK_LIBS,
                cpp_link_libs: LINUX_CPP_LINK_LIBS,
                subsystem_flag: None,
                dead_strip_flag: "--gc-sections",
                debug_none_flag: Some("--strip-debug"),
                icf_flag: Some("--icf=all"),
            }
        }
    }
}

// ── LLVM target descriptors ──────────────────────────────────────────

/// Coarse identity of an LLVM codegen target.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum LlvmTargetId {
    /// `x86_64-pc-windows-msvc`.
    WindowsX64Msvc,
    /// `x86_64-unknown-linux-gnu`.
    LinuxX64Gnu,
    /// `x86_64-unknown-none` (freestanding).
    BareMetalX64,
    /// Anything else (cross/musl/aarch64/…); see [`LlvmTargetDescriptor::triple`].
    Other,
}

/// Minimal LLVM target description threaded through IR emission.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LlvmTargetDescriptor {
    /// Coarse target identity.
    pub id: LlvmTargetId,
    /// LLVM `target triple` string emitted into the IR.
    pub triple: String,
    /// LLVM `target datalayout` string emitted into the IR.
    pub datalayout: String,
}

impl LlvmTargetDescriptor {
    /// Descriptor for the machine running this code.
    pub fn host() -> Self {
        Self::for_triple(&TargetTriple::host().to_string())
    }

    /// Descriptor for an explicit triple string.
    ///
    /// Unknown triples map to [`LlvmTargetId::Other`] with a generic
    /// little-endian x86-64 datalayout; known triples carry their canonical
    /// LLVM datalayout strings.
    pub fn for_triple(triple: &str) -> Self {
        match triple {
            "x86_64-pc-windows-msvc" => Self {
                id: LlvmTargetId::WindowsX64Msvc,
                triple: triple.to_string(),
                datalayout: "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
                    .to_string(),
            },
            "x86_64-unknown-linux-gnu" | "x86_64-unknown-linux-musl" => Self {
                id: LlvmTargetId::LinuxX64Gnu,
                triple: triple.to_string(),
                datalayout: "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
                    .to_string(),
            },
            "x86_64-unknown-none" => Self {
                id: LlvmTargetId::BareMetalX64,
                triple: triple.to_string(),
                datalayout: "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
                    .to_string(),
            },
            other => Self {
                id: LlvmTargetId::Other,
                triple: other.to_string(),
                datalayout: "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
                    .to_string(),
            },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_round_trips() {
        for triple in [
            "x86_64-pc-windows-msvc",
            "x86_64-unknown-linux-gnu",
            "x86_64-unknown-linux-musl",
            "aarch64-unknown-linux-musl",
            "x86_64-unknown-none",
        ] {
            let parsed = TargetTriple::parse(triple).unwrap();
            assert_eq!(parsed.raw(), triple);
            assert_eq!(parsed.to_string(), triple);
        }
    }

    #[test]
    fn parse_rejects_garbage() {
        assert!(TargetTriple::parse("windows").is_err());
        assert!(TargetTriple::parse("").is_err());
        assert!(TargetTriple::parse("a-b-c-d-e").is_err());
    }

    #[test]
    fn windows_platform_conventions() {
        let triple = TargetTriple::parse("x86_64-pc-windows-msvc").unwrap();
        assert!(triple.is_windows());
        let platform = Platform::for_triple(&triple);
        assert_eq!(platform.exe_extension, "exe");
        assert_eq!(platform.shared_lib_extension, "dll");
        assert_eq!(platform.static_lib_extension, "lib");
        assert_eq!(platform.object_extension, "obj");
        assert!(platform.subsystem_flag.is_some());
        assert_eq!(triple.runtime_lib_name(), "kain_runtime.lib");
    }

    #[test]
    fn linux_platform_conventions() {
        let triple = TargetTriple::parse("x86_64-unknown-linux-gnu").unwrap();
        assert!(!triple.is_windows());
        let platform = Platform::for_triple(&triple);
        assert_eq!(platform.exe_extension, "");
        assert_eq!(platform.shared_lib_extension, "so");
        assert_eq!(platform.dead_strip_flag, "--gc-sections");
        assert_eq!(triple.runtime_lib_name(), "libkain_runtime.a");
    }

    #[test]
    fn llvm_descriptors_match_codegen_expectations() {
        let win = LlvmTargetDescriptor::for_triple("x86_64-pc-windows-msvc");
        assert_eq!(win.id, LlvmTargetId::WindowsX64Msvc);
        assert!(!win.datalayout.is_empty());
        let bare = LlvmTargetDescriptor::for_triple("x86_64-unknown-none");
        assert_eq!(bare.id, LlvmTargetId::BareMetalX64);
        // host() must always produce a usable descriptor.
        let host = LlvmTargetDescriptor::host();
        assert!(!host.triple.is_empty());
        assert!(!host.datalayout.is_empty());
    }
}
