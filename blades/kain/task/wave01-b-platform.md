# Task: platform.kn — OS / Architecture Detection

## Agent: kain-writer
## Wave: 1
## File to write: X:/blades/kain/src/platform.kn
## Target lines: ~100
## Dependencies: None
## Parallel: Yes

---

## What to Build

Lightweight platform detection. Tells the compiler what OS and CPU architecture it's running on so it can set the correct LLVM target triple, linker flags, and runtime paths.

## Public API Contract

```kain
/// Known operating systems
pub enum PlatformOs:
    Windows
    Linux
    Macos
    Unknown

/// Known CPU architectures
pub enum PlatformArch:
    X86_64
    Aarch64
    Unknown

/// Detect the current OS
pub fn detect_os() -> PlatformOs

/// Detect the current CPU architecture
pub fn detect_arch() -> PlatformArch

/// Get the LLVM target triple for the current platform
pub fn get_target_triple() -> String

/// Convenience checks
pub fn is_windows() -> Bool
pub fn is_linux() -> Bool
pub fn is_macos() -> Bool

/// Get the default shared library extension (.dll, .so, .dylib)
pub fn shared_lib_extension() -> String

/// Get the default executable extension (.exe, "")
pub fn exe_extension() -> String

/// Path separator for the current platform
pub fn path_separator() -> String
```

## Internal Implementation Strategy

Use Kain compile-time constants or runtime detection. Since the self-host compiler is built for a specific platform, lean toward compile-time:

Option A (preferred): Use `const` values set at build time via build.kn configuration
Option B: Use platform-specific `asm` or syscall probes

For the initial implementation, hardcode the target triple for the build platform. The Kain build system (build.kn) can inject platform values as compile-time constants.

## Research to Read

- X:/blades/kain/research/03-llvm-codegen-jit.md — Section on target triples and platform detection
- X:/blades/kain/research/SELFHOST-KN.MD — Section 9 (LLVM-C FFI Contract) for target triple format

## Reference Files

- crates/cli/src/kain_launcher.rs — search for "target_triple" and "platform" patterns

## Neighboring Files

| File | What it needs from platform.kn |
|------|-------------------------------|
| target.kn | `get_target_triple()` for LLVM target init |
| driver.kn | `is_windows()` for path handling |
| jit.kn | `get_target_triple()` for OrcJIT config |

## Test Expectations

- `kain check src/platform.kn` passes
- `get_target_triple()` returns a valid triple like "x86_64-pc-windows-msvc"
- `is_windows()` returns true on Windows build machines
- `exe_extension()` returns ".exe" on Windows, "" on Linux
