# Kain VPS / Fresh Machine Setup Guide

**Date**: 2026-07-08  
**Purpose**: Bootstrap a hermetic Kain development environment on a fresh Windows machine
**Target**: Move from hardcoded `X:/`/`Z:/` paths to repo-relative hermetic setup

> **⚠️ First-Time Build Time**: ~30-60 minutes for cold Bazel cache (11,458 actions).
> Subsequent builds take minutes.

---

## 1. Prerequisites — Install System Dependencies

```powershell
# Windows SDK (needed for kernel32.lib and other system libs)
winget install "Microsoft.WindowsSDK.10.0.18362"

# LLVM/Clang (provides clang.exe, libclang.dll, lld-link.exe)
winget install LLVM.LLVM

# Python 3.x (needed for PyO3 build scripts and Python interop)
winget install Python.Python.3.12

# VS Build Tools (provives MSVC linker, lib.exe, headers)
winget install Microsoft.VisualStudio.2022.BuildTools

# Bazelisk (manages Bazel versions)
winget install Bazel.Bazelisk
```

### Verify Installations

```powershell
# LLVM
& "C:\Program Files\LLVM\bin\clang.exe" --version

# Python
& "C:\Users\$env:USERNAME\AppData\Local\Programs\Python\Python312\python.exe" --version

# Bazel
bazel version

# VS Build Tools
dir "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
```

---

## 2. Clone & Prepare the Repo

```powershell
git clone <repo-url> F:\Kain-Lang
cd F:\Kain-Lang
```

---

## 3. Configure Bazel for Hermetic Paths

### 3.1 Update `.bazelrc`

Replace all `Z:/_b/` and `X:/` references with paths outside the repo tree
(Bazel forbids caches inside the workspace):

```ini
# .bazelrc — key changes:
startup --output_user_root=../.bazel-cache/kain/output-user-root
common --repository_cache=../.bazel-cache/kain/repo
common --repo_env=TMP=F:/.bazel-cache/kain/tmp
common --action_env=TMP=F:/.bazel-cache/kain/tmp
build --disk_cache=../.bazel-cache/kain/disk
```

> **Why absolute TMP paths?** Bazel's `cargo_build_script` runner resolves relative
> paths against the exec root (deep in external/...). Absolute paths avoid
> spurious "path not found" errors.

### 3.2 Update `.bazeliskrc`

```ini
BAZELISK_HOME=../.bazel-cache/kain/bazelisk
```

### 3.3 Create `.bazelrc.local` (VPS-specific)

```ini
# Python path for PyO3 build scripts
common --action_env=PYO3_PYTHON=C:/Users/ephemara/AppData/Local/Programs/Python/Python312/python.exe
build --action_env=PYO3_PYTHON=C:/Users/ephemara/AppData/Local/Programs/Python/Python312/python.exe

# LLVM libclang path (no quotes, no spaces — use 8.3 short path)
common --action_env=LIBCLANG_PATH=C:/PROGRA~1/LLVM/bin
```

### 3.4 Create cache directories

```powershell
mkdir -p ../.bazel-cache/kain/{output-user-root,repo,disk,tmp,bazelisk}
```

### 3.5 Fix `MODULE.bazel`

Remove hardcoded `F:/Scoop/...` Python paths from `crate.annotation_select` blocks.
PyO3 needs `PYO3_PYTHON` set via `.bazelrc.local` or shell environment.

```python
# BEFORE (hardcoded — breaks on any machine without Scoop):
crate.annotation_select(
    crate = "pyo3-build-config",
    triples = ["x86_64-pc-windows-msvc"],
    build_script_env = {
        "PYO3_PYTHON": "F:/Scoop/apps/python312/current/python.exe",
    },
)

# AFTER — set via .bazelrc.local or MODULE.bazel with machine-appropriate path:
crate.annotation_select(
    crate = "pyo3-build-config",
    triples = ["x86_64-pc-windows-msvc"],
    build_script_env = {
        "PYO3_PYTHON": "C:/Users/ephemara/AppData/Local/Programs/Python/Python312/python.exe",
    },
)
```

> **Note**: On this VPS, PyO3 `[for tool]` build scripts don't inherit
> `--action_env` from `.bazelrc.local`. They need `PYO3_PYTHON` in the
> `build_script_env` of the `crate.annotation_select` in `MODULE.bazel`.

---

## 4. Fix Missing Crates

The `crates/target-triple/` directory may be missing from the repo checkout.
Create it with a stub:

```powershell
mkdir crates/target-triple/src
```

This crate (`kain-target`) provides `LlvmTargetId`, `LlvmTargetDescriptor`,
`Platform`, `TargetTriple`, and related types. It must export:

| Export | Kind | Used By |
|--------|------|---------|
| `LlvmTargetId` | Enum | `kain-sys-codegen` |
| `LlvmTargetDescriptor` | Struct (with `datalayout` field) | `kain-sys-codegen` |
| `Platform` | Struct (with `exe_extension`, `subsystem_flag`, `debug_none_flag`, `icf_flag`, `dead_strip_flag`, `link_libs`, `cpp_link_libs`, etc.) | `kain-cli`, `kain-build` |
| `TargetTriple` | Struct (with `os`, `arch`, `env`, `vendor` fields) | `kain-cli`, `kain-build`, `kain-driver` |
| `TargetTriple::parse()` | `Result<TargetTriple, String>` (NOT `Vec<TargetTriple>`) | `kain-build` |

See `crates/target-triple/src/lib.rs` for the full stub.

Also create `crates/target-triple/Cargo.toml` and `crates/target-triple/BUILD.bazel`.

---

## 5. Build the Compiler

### 5.1 Build with VS environment

```batch
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set PYO3_PYTHON=C:\Users\ephemara\AppData\Local\Programs\Python\Python312\python.exe
set LIBCLANG_PATH=C:\Program Files\LLVM\bin
bazel build //:kain --config=dev
```

> `vcvars64.bat` sets up the MSVC environment (PATH, INCLUDE, LIB) needed
> for the Rust compiler to find `link.exe` and the Windows SDK.

### 5.2 Handle Cargo Bazel Repin

If `MODULE.bazel` was modified, the lockfile digest will mismatch:

```text
Error: Digests do not match: Current Digest(...) != Expected Digest(...)
```

Fix by setting `CARGO_BAZEL_REPIN=true`:

```batch
set CARGO_BAZEL_REPIN=true
bazel build //:kain --config=dev
```

This re-runs the workspace splice + lockfile generation. It only needs to
happen once — the updated lockfile is checked in.

### 5.3 First Build Time

| Phase | Time | Notes |
|-------|------|-------|
| Cargo Bazel Bootstrap | ~7 min | Compiles `cargo-bazel` from source |
| Lockfile Generation | ~2 min | Splicing + dependency resolution |
| External Crate Compilation | ~5 min | Downloads and compiles ~200+ crates |
| Rust Crate Compilation | ~15 min | All 67 crates + 11,458 actions |
| C Runtime Compilation | ~5 min | 47 C files (handle.c is slowest) |
| **Total (cold)** | **~35-60 min** | Server startup + downloads + compilation |

---

## 6. Build & Sync the Native C Runtime

### 6.1 Build the Runtime

```powershell
# Using the Python script
C:\Users\ephemara\AppData\Local\Programs\Python\Python312\python.exe scripts/python/update_runtime.py
```

This handles:
1. `bazel build //runtime:native_core_runtime --config=dev`
2. Finding the Bazel output directory
3. Archiving `.obj` files into `kain_runtime.lib` using MSVC `lib.exe`
4. Copying to `.kain/lib/kain_runtime.lib`
5. Running `kain doctor` to verify

### 6.2 Manual Archive (if script fails)

```batch
@echo off
set LIB_DIR=F:\Kain-Lang\.kain\lib
set OBJ_DIR=F:\.bazel-cache\kain\output-user-root\{hash}\execroot\_main\bazel-out\x64_windows-opt\bin\runtime\_objs\native_core_runtime_c
mkdir %LIB_DIR% 2>nul
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\lib.exe" /NOLOGO /OUT:%LIB_DIR%\kain_runtime.lib %OBJ_DIR%\*.obj
```

---

## 7. Run `kain doctor`

After building both the compiler and runtime, run the diagnostic:

```batch
@echo off
set PATH=C:\Program Files\LLVM\bin;C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python312;%PATH%
F:\Kain-Lang\.kain\bin\kain.exe doctor
```

> **CRITICAL**: The binary needs `python312.dll` (from Python install dir) and
> `libclang.dll` (from LLVM bin dir) on `PATH` at runtime. These are NOT
> needed at build time (they're found via `LIBCLANG_PATH` / `PYO3_PYTHON`),
> but the Windows loader needs them when the process starts.

### What `kain doctor` Checks

| Field | What It Means |
|-------|---------------|
| Binary Path | Compiler location |
| Kain Home | Config/stdlib/lib root |
| Resolved Stdlib | Where `.kn` stdlib files are found |
| Resolved Runtime C | C runtime source location |
| Resolved Runtime Manifest | TOML listing all `.c` files |
| Resolved LLVM Clang | `clang.exe` path |
| Managed Sync Stamp | Whether binary was synced via `kain_sync_binary` |
| Supported Targets | Codegen backends available |

---

## 8. Common Pitfalls & Fixes

### `link.exe not found`
The MSVC environment isn't loaded. Run `vcvars64.bat` first:
```
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
```

### `kernel32.lib not found` (LNK1181)
Windows SDK not installed:
```powershell
winget install "Microsoft.WindowsSDK.10.0.18362"
```

### `LNK1104: cannot open file '...\lnk{...}.tmp'`
TMP/TEMP paths must be **absolute**, not relative. Cargo build scripts run
in deeply nested directories and resolve relative paths against the wrong
root. Use `F:/.bazel-cache/kain/tmp` (absolute).

### `Python 3.x interpreter not found` (pyo3-build-config)
Two possible causes:
1. `PYO3_PYTHON` not set in the shell environment AND not in `.bazelrc.local`
2. `PYO3_PYTHON` set in `.bazelrc.local` but not inherited by `[for tool]` build scripts

**Fix**: Set `PYO3_PYTHON` in both places:
- The shell env: `set PYO3_PYTHON=C:\Users\...\python.exe`
- The `crate.annotation_select` in `MODULE.bazel` (for tool build scripts)

### `no field 'datalayout' on type LlvmTargetDescriptor`
The `kain-target` crate is missing this field. Add to struct:
```rust
pub struct LlvmTargetDescriptor {
    pub datalayout: String,  // required!
    // ...
}
```

### `STATUS_DLL_NOT_FOUND` (exit code -1073741515)
At runtime, `kain.exe` needs:
- `python312.dll` → add Python install dir to `PATH`
- `libclang.dll` → add LLVM bin dir to `PATH`
- `VCRUNTIME140.dll` → add VS Build Tools VC/Redist to `PATH`

### `.bazelrc` cache paths rejected ("inside main repo")
Bazel won't let you put `repository_cache`, `output_user_root`, or
`disk_cache` inside the workspace tree. Use sibling directory:
```ini
common --repository_cache=../.bazel-cache/kain/repo
```

### `Digests do not match` (lockfile stale)
After changing `MODULE.bazel`, the `Cargo.Bazel.lock` needs regeneration:
```batch
set CARGO_BAZEL_REPIN=true
bazel build //:kain --config=dev
```

### `bazel build` hangs / times out (cold server)
Bazel server is cold. Warm it up:
```powershell
bazel info --config=dev
bazel build //:kain --config=dev
```

---

## 9. Final Hermetic Path Layout

```
F:/Kain-Lang/                         # Repo root (portable — any drive/any machine)
├── .bazelrc                          # All paths use ../.bazel-cache/kain/ or absolute
├── .bazelrc.local                    # VPS-specific: Python + LLVM paths
├── .bazeliskrc                       # Points to sibling cache
├── .bazelrc.wsl                      # Uses sibling cache pattern
├── MODULE.bazel                      # PYO3_PYTHON set per-machine
├── crates/target-triple/             # Created stub for kain-target crate
├── .kain/
│   ├── bin/kain.exe                  # Synced compiler binary
│   ├── lib/kain_runtime.lib          # Native C runtime library
│   └── config.toml                   # Tooling configuration
└── (sibling) .bazel-cache/kain/      # All Bazel mutable state
    ├── output-user-root/
    ├── repo/
    ├── disk/
    ├── tmp/
    └── bazelisk/
```

---

## 10. Quick Reference — One-Time Setup

```powershell
# 1. Install deps
winget install "Microsoft.WindowsSDK.10.0.18362"
winget install LLVM.LLVM
winget install Python.Python.3.12
winget install Microsoft.VisualStudio.2022.BuildTools
winget install Bazel.Bazelisk

# 2. Clone repo
cd F:\
git clone <repo-url> F:\Kain-Lang
cd F:\Kain-Lang

# 3. Create cache dirs
mkdir -p ../.bazel-cache/kain/{output-user-root,repo,disk,tmp,bazelisk}

# 4. Create missing target-triple crate
mkdir crates/target-triple/src
# (write Cargo.toml, BUILD.bazel, src/lib.rs — see section 4)

# 5. Configure .bazelrc.local
# (add PYO3_PYTHON and LIBCLANG_PATH — see section 3.3)

# 6. Copy binary to .kain/bin/
# (run build_kain.bat from section 5.1)

# 7. Wire up PATH for runtime deps
# (add Python and LLVM bin dirs to PATH — see section 7)

# 8. Setup complete!
kain doctor
```
