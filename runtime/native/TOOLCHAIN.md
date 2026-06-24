# Kain C Runtime — Build Toolchain

> This document describes every compiler, build system, and tool involved in
> building the Kain native C runtime (`kain_runtime.lib` / `libkain_runtime.a`)
> and linking it into compiled Kain executables.
>
> **Target audience:** Developers who need to rebuild the runtime, understand
> why a particular compiler is used, diagnose link errors, or add new C source
> files to the runtime.
>
> **Related:** `TROUBLESHOOTING_DEV.md` (symptom-to-fix reference), `README.md`
> (runtime architecture), `Makefile` (local dev), `../../scripts/kain_toolchain.py`
> (toolchain discovery)

---

## Table of Contents

1. [Compilers Used](#1-compilers-used)
2. [Pipeline Flow](#2-pipeline-flow)
3. [Where MSVC Is vs Clang](#3-where-msvc-is-vs-clang)
4. [How the Kain Compiler Finds and Invokes the Toolchain](#4-how-the-kain-compiler-finds-and-invokes-the-toolchain)
5. [Build Systems](#5-build-systems)
6. [Environment Variables](#6-environment-variables)
7. [The Freestanding Runtime](#7-the-freestanding-runtime)
8. [Verification Toolchain](#8-verification-toolchain)
9. [Common Issues](#9-common-issues)
10. [How to Rebuild the Runtime](#10-how-to-rebuild-the-runtime)
11. [Appendix: Key Files and Paths](#11-appendix-key-files-and-paths)

---

## 1. Compilers Used

### Who compiles the C runtime (.c → .obj → .lib)?

There are **two independent build systems** for the C runtime, and they use
different compilers:

| Build System | Windows | Linux | macOS |
|---|---|---|---|
| **Makefile** (local dev) | Clang (MSYS2/MinGW) | Clang or GCC | Apple Clang |
| **Bazel** (production) | MSVC (`cl.exe`) via `@rules_cc` | GCC or Clang (system default) | Apple Clang (Xcode) |

### Who links the final Kain executable (.ll → .exe)?

The **Kain compiler** (`kain build --target llvm`) always uses **Clang** to
compile LLVM IR into a native binary. On Windows, this clang acts as a
frontend to the **MSVC linker** (`lld-link.exe` or `link.exe`) via clang-cl
mode. The exact command is constructed by `crates/build/src/native_link.rs`.

### Summary table

| Step | Windows Build Tool | Linux Build Tool | macOS Build Tool |
|------|---|---|---|
| C compilation (Makefile) | clang | clang/gcc | Apple clang |
| C compilation (Bazel) | cl.exe (MSVC) | gcc/clang | Apple clang |
| Static archiving (Makefile) | ar | ar | ar |
| Static archiving (Bazel) | lib.exe (MSVC) | ar (built-in) | ar (built-in) |
| LLVM IR → .exe (kain build) | clang → lld-link | clang → ld.lld | clang → ld64 |
| Formal verification | CBMC (WSL or native) | CBMC | CBMC |

---

## 2. Pipeline Flow

### Runtime library build (C sources → static library)

```
Linux/macOS (Makefile):
  arena.c → clang -std=c11 ... -c → arena.o
  actor.c → clang -std=c11 ... -c → actor.o
  ...
  ar rcs libkain_runtime.a *.o

Linux/macOS (Bazel):
  arena.c → gcc/clang -std=c11 ... -c → arena.o
  actor.c → gcc/clang -std=c11 ... -c → actor.o
  ...
  ar rcs libnative_core_runtime.a *.o

Windows (Makefile via MSYS2):
  arena.c → clang -std=c11 ... -c → arena.o
  actor.c → clang -std=c11 ... -c → actor.o
  ...
  ar rcs libkain_runtime.lib *.o

Windows (Bazel / MSVC):
  arena.c → cl.exe /std:c11 /c → arena.obj
  actor.c → cl.exe /std:c11 /c → actor.obj
  ...
  lib.exe /OUT:kain_runtime.lib *.obj
```

### Kain program build (.kn → .exe)

The Kain compiler pipeline is:

```
your_file.kn
  → [Rust compiler frontend]
  → LLVM IR (.ll)
  → [clang + lld-link (Windows) / clang + ld.lld (Linux) / clang + ld64 (macOS)]
  → executable (.exe / no extension)
```

The linker receives:

1. The compiled LLVM IR (with `kain_runtime_init`/`kain_runtime_shutdown` calls)
2. The precompiled `kain_runtime.lib` / `libkain_runtime.a`
3. Platform link libraries (e.g., `user32.lib`, `ws2_32.lib`, `pthread`, `dl`)

Dead-stripping (`/OPT:REF` on Windows, `--gc-sections` on Linux, `-dead_strip`
on macOS) removes any runtime functions the program doesn't actually call.

### Example: what `kain build hello.kn --target llvm` actually runs

On **Windows**, the effective command is approximately:

```
clang -O2 -Wno-override-module ^
    "X:\.kain\lib\kain_runtime.lib" ^
    -Wl,/subsystem:console ^
    hello.ll ^
    -o hello.exe ^
    -Wl,/OPT:REF ^
    -llegacy_stdio_definitions -luser32 -lgdi32 -lshell32 ^
    -lws2_32 -lwinhttp -ladvapi32
```

On **Linux**:

```
clang -O2 -Wno-override-module \
    ~/.kain/lib/libkain_runtime.a \
    hello.ll \
    -o hello \
    -Wl,--gc-sections \
    -lpthread -ldl -lrt -lm
```

---

## 3. Where MSVC Is vs Clang

### Where MSVC is used

| Location | Why |
|---|---|
| **Bazel C runtime build** (Windows) | Bazel's `@rules_cc` on Windows detects VS Build Tools and uses `cl.exe` as the C compiler. Windows SDK and MSVC CRT headers are found automatically. |
| **`kain_toolchain.py --env`** | Discovers MSVC lib/include paths (`LIB` / `INCLUDE` env vars) so that clang/lld-link can find CRT libraries (`libcmt.lib`, `oldnames.lib`, etc.) when linking Kain executables. |
| **`lib.exe` archiving** (Windows) | The MSVC librarian (`lib.exe`) is always used to archive `.obj` files into `.lib` — both by Bazel and by the `sync_runtime_library()` Python script. |

### Where Clang is used

| Location | Why |
|---|---|
| **Makefile local dev** (all platforms) | Clang is the default (`CC ?= clang`). Faster, supports ASan/UBSan/TSan uniformly, and has `__builtin_fmax`/`__builtin_fmin` which the runtime uses for branchless math. |
| **`kain build --target llvm`** (all platforms) | The compiler emits LLVM IR, and Clang is the canonical LLVM IR → native binary compiler. On Windows, clang-cl mode delegates to the MSVC linker. |
| **Conformance tests** (all platforms) | All `compile_test.sh` scripts prefer clang, falling back to gcc. |

### How to switch

- **Makefile**: Override `CC` and `AR` at invocation:
  ```bash
  make CC=gcc AR=gcc-ar lib
  make CC=clang-cl AR=llvm-lib lib   # Windows: use clang frontend + llvm-lib
  ```
- **Bazel**: The MSVC toolchain is auto-detected. To force clang on Windows,
  add `--config=clangcl` to your Bazel command (requires a `clangcl` config
  in `.bazelrc`; not currently present).
- **`kain build`**: Set `KAIN_CLANG_PATH` to point at a specific clang binary.

### Why this split exists

- **MSVC** is required on Windows because the Kain runtime uses Win32 API
  (`CRITICAL_SECTION`, `HANDLE`, `VirtualAlloc`, `WS2_32`, `D3D12`) that
  requires MSVC-compatible CRT linkage. The platform link libraries
  (`user32.lib`, `ws2_32.lib`, etc.) are MSVC-format .lib files.
- **Clang** is the universal LLVM IR compiler. Using clang for local dev
  gives consistent sanitizer support and the `__builtin_fmax`/`__builtin_fmin`
  that the runtime's branchless math depends on.

---

## 4. How the Kain Compiler Finds and Invokes the Toolchain

### Clang discovery (`crates/core/src/install_layout.rs`)

Priority order:

1. **`KAIN_CLANG_PATH`** environment variable (explicit path to clang)
2. **Bundled toolchain** — looks for `toolchain/llvm/bin/clang.exe` relative
   to the Kain home directory, repo root, or executable directory
3. **`PATH`** — runs `clang --version` to check
4. **System install** — Windows: `C:\Program Files\LLVM\bin\clang.exe`

### MSVC link environment (`apply_windows_msvc_link_env`)

On Windows, before invoking clang, the compiler sets the `LIB` environment
variable so `lld-link.exe` can find MSVC CRT libraries:

1. `VCToolsInstallDir` → `lib/x64` (MSVC toolchain libs)
2. `WindowsSdkDir` + `WindowsSDKLibVersion` → SDK `lib/um/x64`, `lib/ucrt/x64`
3. Falls back to scanning VS 2022/2019 install paths

### Runtime library resolution

The linker finds `kain_runtime.lib` / `libkain_runtime.a` via:

1. **`KAIN_RUNTIME_LIB_PATH`** environment variable (explicit path)
2. **`~/.kain/lib/kain_runtime.lib`** (Windows) or **`~/.kain/lib/libkain_runtime.a`** (POSIX)

### Runtime manifest resolution

The compiler can optionally read a `native_core_runtime.toml` manifest for
multi-file C compilation (when compiling C alongside Kain):

1. `KAIN_RUNTIME_MANIFEST_PATH` or `KAIN_RUNTIME_MANIFEST` env var
2. `runtime/native_core_runtime.toml` relative to repo root / Kain home
3. `runtime/native_runtime.toml` (legacy fallback)

---

## 5. Build Systems

### 5a. Local Dev (Makefile)

**Location:** `runtime/native/Makefile`

**Purpose:** Fast iteration on C runtime code. Seconds to compile.

| Target | What It Does |
|---|---|
| `make` (default) | Compiles all `.c` → `.o` (no linking) |
| `make lib` | Builds static library `_build/lib/libkain_runtime.a` / `.lib` |
| `make shared` | Builds shared library `_build/lib/libkain_runtime.so` / `.dylib` / `.dll` |
| `make test` | Builds + runs smoke + property tests (ASan+UBSan) |
| `make fuzz` | Builds libFuzzer harnesses |
| `make stress` | Builds + runs stress tests (TSan) |
| `make clean` | Removes `_build/` |

**Default compiler:** `CC ?= clang` (overridable)

**Key flags:**
- `-std=c11 -Wall -Wextra -g`
- On Windows: `-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS`
- Sanitizers: `-fsanitize=address,undefined` (Linux/macOS only — not MinGW)

**Source selection:**
- Compiles all `src/core/*.c` **except** `*_benchmark.c`, `python_runtime*.c`,
  `cuda_runtime*.c` (filtered out for speed)
- Platform layer: `src/platform/platform.c` + win32 or linux shared helpers

**Limitations:** No GPU backend ABI libraries, no python_runtime or cuda_runtime.

### 5b. Production (Bazel)

**Location:** `runtime/BUILD.bazel` + `runtime/native_runtime_rules.bzl`

**Purpose:** Authoritative build used for shipping `kain_runtime.lib`.

**Key targets:**

| Target | Description |
|---|---|
| `//runtime:native_core_runtime` | Full production runtime (60+ C files) |
| `//runtime:native_runtime` | Alias for `native_core_runtime` |
| `//runtime:native_core_freestanding` | Bare-metal subset (18 files, `-ffreestanding -nostdlib`) |
| `//runtime:runtime_headers_only` | Headers only (no .c compilation) |
| `//runtime:native_runtime_tests` | Test suite (10 cc_test targets) |

**Config presets (`--config=`):**

| Config | Mode | Effect |
|---|---|---|
| `dev` (default) | `--compilation_mode=opt` | Optimized dev build |
| `debug` | `--compilation_mode=dbg` | Debug symbols |
| `release` | `--compilation_mode=opt` | Release (same as dev currently) |
| `speed` | opt + LTO | ThinLTO, codegen-units=1 |
| `maxperf` | More parallel jobs | Full CPU/memory utilization |

#### How `platform_select()` works

Defined in `runtime/native_runtime_rules.bzl`:

```python
def platform_select(windows = [], linux = [], macos = [], default = []):
    return select({
        ":windows": windows,
        ":linux": linux,
        ":macos": macos,
        "//conditions:default": default,
    })
```

The `:windows`/`:linux`/`:macos` config_settings are defined in
`runtime/BUILD.bazel` using `@platforms//os:*` constraints:

```python
config_setting(
    name = "windows",
    constraint_values = ["@platforms//os:windows"],
)
```

This is used to select platform-specific:
- **C source files** (e.g., `crash_handler_win32.c` only on Windows)
- **Compiler flags** (`/W3 /std:c11` on Windows vs `-Wall -std=c11` on POSIX)
- **Defines** (`WIN32_LEAN_AND_MEAN` on Windows, `_GNU_SOURCE` on Linux)
- **Link libraries** (`user32.lib` on Windows, `-lpthread` on Linux)

#### Compiler flags per platform

| Platform | C flags | C++ flags |
|---|---|---|
| **Windows** (MSVC) | `/W3 /std:c11 /experimental:c11atomics /Gy` | `/W3 /std:c++20` |
| **Linux** (GCC/clang) | `-Wall -Wextra -std=c11 -ffunction-sections -fdata-sections` | `-Wall -Wextra -std=c++20 -ffunction-sections -fdata-sections` |
| **macOS** (Apple clang) | Same as Linux | Same as Linux |

#### Link libraries per platform

| Platform | Link Libraries |
|---|---|
| **Windows** | `legacy_stdio_definitions`, `user32`, `gdi32`, `shell32`, `ws2_32`, `winhttp`, `advapi32`, `ole32`, `winmm` |
| **Linux** | `pthread`, `dl`, `rt`, `m`, `asound` |
| **macOS** | `AudioToolbox`, `CoreAudio`, `CoreMIDI`, `CoreFoundation` |

#### Platform defines

| Platform | Defines |
|---|---|
| **Windows** | `WIN32`, `_WINDOWS`, `WIN32_LEAN_AND_MEAN`, `_WIN32_WINNT=0x0A00`, `_CRT_DECLARE_NONSTDC_NAMES=0`, `KAIN_RUNTIME_HAS_VULKAN_LOADER` |
| **Linux** | `_GNU_SOURCE`, `_POSIX_C_SOURCE=200112`, `_FILE_OFFSET_BITS=64`, `KAIN_RUNTIME_HAS_VULKAN_LOADER` |
| **macOS** | (none additional) |

#### The manifest pipeline (TOML → .bzl)

```
runtime/native_core_runtime.toml     ← You edit this to add/remove source files
         ↓
tools/bazel/sync_native_runtime_builds.py     ← Run this to regenerate
         ↓
runtime/runtime_manifest_data.bzl     ← Auto-generated, consumed by BUILD.bazel
         ↓
declare_runtime_bundle("native_core_runtime", NATIVE_CORE_RUNTIME) in BUILD.bazel
         ↓
Bazel cc_library with sources, copts, defines, linkopts
```

**Important:** After editing `native_core_runtime.toml`, you MUST run
`python tools/bazel/sync_native_runtime_builds.py` to regenerate
`runtime_manifest_data.bzl`. The BUILD.bazel loads data from the `.bzl`
file, not directly from the `.toml`.

### 5c. Python Sync Script (Legacy)

**Location:** `scripts/python/kain_bazel_sync.py`

**Purpose:** One-shot build + sync of the runtime library to `~/.kain/lib/`.

```bash
# Full rebuild + sync
python scripts/python/kain_bazel_sync.py sync

# Re-archive existing .obj files without rebuilding
python scripts/python/kain_bazel_sync.py sync --skip-build
```

This script:
1. Runs `bazel build //runtime:native_core_runtime`
2. On Windows: finds `lib.exe`, archives `.obj` files → `kain_runtime.lib`
3. On POSIX: copies `libnative_core_runtime.a` → `libkain_runtime.a`
4. Copies to `~/.kain/lib/`

---

## 6. Environment Variables

### Toolchain selection

| Variable | Purpose | Used By |
|---|---|---|
| `KAIN_CLANG_PATH` | Explicit path to clang binary | `install_layout.rs`, `native_link.rs` |
| `KAIN_HOME` | Root of Kain toolchain install (default: `~/.kain`) | `install_layout.rs` |
| `CC` | C compiler for Makefile (default: `clang`) | `Makefile` |
| `AR` | Archiver for Makefile (default: `ar`) | `Makefile` |

### Runtime library resolution

| Variable | Purpose | Used By |
|---|---|---|
| `KAIN_RUNTIME_LIB_PATH` | Explicit path to `kain_runtime.lib` / `libkain_runtime.a` | `native_link.rs` |
| `KAIN_RUNTIME_CORE_LIB_PATH` | Explicit path to freestanding runtime lib | `native_link.rs` |
| `KAIN_RUNTIME_MANIFEST_PATH` | Explicit path to `native_core_runtime.toml` | `install_layout.rs` |
| `KAIN_RUNTIME_MANIFEST` | Alias for above | `install_layout.rs` |
| `KAIN_RUNTIME_C_PATH` | Explicit path to runtime C source | `install_layout.rs` |

### MSVC discovery (Windows only)

| Variable | Purpose | Set By |
|---|---|---|
| `VCToolsInstallDir` | VS toolchain root | VS Developer Command Prompt |
| `WindowsSdkDir` | Windows SDK root | VS Developer Command Prompt |
| `WindowsSDKLibVersion` | SDK lib version | VS Developer Command Prompt |
| `LIB` | MSVC linker search paths | `apply_windows_msvc_link_env()` |
| `INCLUDE` | MSVC compiler include paths | VS Developer Command Prompt |

### Bazel

| Variable | Purpose | Used By |
|---|---|---|
| `LIBCLANG_PATH` | Path to `libclang.dll`/`.so`/`.dylib` for Rust bindings | `.bazelrc` |
| `BAZEL_SH` | Bash shell for Bazel on Windows | `kain_bazel_sync.py` |

### Repository and project

| Variable | Purpose |
|---|---|
| `KAIN_REPO_ROOT` | Root of the Kain monorepo (for discovering resources) |
| `KAIN_CONFIG` | Path to Kain config file |
| `KAIN_STDLIB_PATH` | Path to stdlib directory |

---

## 7. The Freestanding Runtime

The freestanding runtime (`native_core_freestanding`) is a **bare-metal subset**
that compiles with `-ffreestanding -nostdlib`. It provides the minimal symbols
that Kain-compiled code emits (`string_new`, `KAIN_alloc`, `print_i64`, etc.)
without any OS or libc dependencies.

### Build

```bash
bazel build //runtime:native_core_freestanding --config=dev
```

### What's included (18 source files)

**Memory & layout:** `arena.c`, `buddy.c`, `bitfield.c`, `union.c`,
`deferred_free.c`, `handle.c`, `fixup.c`

**Compiler semantic runtime:** `entangle.c`, `wire.c`, `event.c`, `batch_queue.c`

**Machine stones (core):** `ownership.c`, `converge.c`, `profile.c`

**Infrastructure:** `version.c`, `services.c`, `crash_handler.c`,
`freestanding_stubs.c`

### What `freestanding_stubs.c` provides

Stub implementations of LLVM IR symbols that the Kain compiler always emits:

- `string_new()` → returns pointer unchanged (strings live in `.rodata`)
- `print_i64()`/`print_f64()`/`print_bool()`/`print_str()` → no-ops
- `KAIN_alloc()` → returns NULL (kernel provides real allocator)
- `str_concat*()` → returns first non-null arg
- `rc_retain()`/`rc_release()` → no-ops
- `qemu_debug_putc()`/`qemu_debug_puts()` → I/O port 0xE9 (QEMU debug console)

### Target

```bash
clang -target x86_64-unknown-none -ffreestanding -nostdlib -c freestanding_stubs.c
```

This mirrors Rust's `core` vs `std` split: the freestanding runtime is the
`core` equivalent — platform-independent, no OS assumptions.

---

## 8. Verification Toolchain

The native runtime has a three-layer verification pipeline:

### Layer 1: Sanitizer tests (Makefile)

```
make test      → ASan + UBSan     (memory errors, UB)
make stress    → TSan              (data races)
make fuzz      → libFuzzer + ASan  (coverage-guided edge cases)
```

### Layer 2: CBMC formal verification

CBMC (C Bounded Model Checker) converts C code into SAT/SMT formulas and
proves that no assertion violation, pointer dereference, integer overflow, or
undefined behavior is possible within bounded loop unwinding.

```bash
python test/scripts/run_pipeline.py cbmc --harness check_arena    # 833 assertions
python test/scripts/run_pipeline.py cbmc --harness check_actor    # 5,676 assertions
```

- **6,509 CBMC assertions** total across all harnesses
- **140 Z3 proof packs** in `src/core/z3/proofs/` for mathematical invariants
  that CBMC's bounded approach cannot reach

### Layer 3: Z3 SMT proofs

Stored in `src/core/z3/`, the Z3 pipeline proves unbounded mathematical
properties: mailbox capacity never exceeded, arena regions never overlap,
ownership state machine transitions are valid, etc.

### CBMC toolchain on Windows

CBMC runs through **WSL Ubuntu** by default (Linux headers are clean; MinGW/MSVC
headers contain constructs that choke CBMC's parser). If WSL is unavailable,
the pipeline falls back to **GCC preprocessing + native CBMC**.

The toolchain discovery script (`kain_toolchain.py --env`) is used to find MSVC
include paths for CBMC preprocessing.

---

## 9. Common Issues

### 9.1 `__builtin_fmax`/`__builtin_fmin` on MSVC

The runtime uses `__builtin_fmax`/`__builtin_fmin` for branchless double clamping
(`kain_clampd` in `core.c`, line 479):

```c
return __builtin_fmax(__builtin_fmin(value, max_value), min_value);
```

**Problem:** MSVC does not provide `__builtin_fmax`. This is fine because:
- The **Makefile** always uses clang, which has these builtins.
- The **Bazel build** on Windows uses MSVC, but `kain_clampd` is compiled by
  clang in the Makefile path and by MSVC in the Bazel path. **However,** MSVC
  has `__max`/`__min` intrinsics defined in `<intrin.h>`, which provides the
  same behavior. If the MSVC build fails with `__builtin_fmax` undefined, add:
  ```c
  #ifdef _MSC_VER
  #define __builtin_fmax __max
  #define __builtin_fmin __min
  #endif
  ```
  This shim is not currently in the runtime — it's only needed if the Bazel
  MSVC build encounters this symbol.

### 9.2 Runtime .lib freshness

**Symptom:** Changes to `runtime/native/src/*.c` don't affect compiled `.exe`.

**Root cause:** The Kain compiler (`kain.exe`) links against a **precompiled
static library** (`kain_runtime.lib`). Modifying `.c` files does NOT
automatically rebuild the `.lib`. You must explicitly:

1. Build with Bazel
2. Archive into `.lib`
3. Copy to `~/.kain/lib/`

See [Section 10](#10-how-to-rebuild-the-runtime) for the exact sequence.

### 9.3 ABI mismatches

**Symptom:** Linker errors about `__kain_crash_table`, `kain_runtime_init`,
or other undefined symbols.

**Root cause:** The runtime `.lib` was compiled with different flags or a
different compiler than what the Kain compiler's clang invocation expects.

**Fixes:**
- Rebuild the runtime with the **same Bazel config** (`dev` vs `release`
  changes the output directory)
- Ensure the runtime was built with `-ffunction-sections -fdata-sections`
  (so dead-stripping works correctly)
- Check that `KAIN_RUNTIME_LIB_PATH` points at the correct `.lib`

### 9.4 Bazel cache returning stale objects

**Symptom:** `bazel build //runtime:native_core_runtime` says "nothing to
build" even after changing `.c` files.

**Fix:** Disable the disk cache for one invocation:
```bash
bazel build //runtime:native_core_runtime --config=dev --disk_cache=
```

### 9.5 File lock preventing .lib sync

**Symptom:** `Permission denied: 'kain_runtime.lib'`

**Fix:** Kill any running Kain executables:
```powershell
taskkill /F /IM kain.exe
taskkill /F /IM ui_test.exe
```

### 9.6 `lib.exe` not found

**Symptom:** `'lib.exe' is not recognized as an internal or external command`

**Fixes:**
- Run from a **Visual Studio Developer Command Prompt** (x64)
- Or use the **`kain_toolchain.py`** script to set up the environment:
  ```powershell
  eval $(python scripts/kain_toolchain.py)
  ```
- Or install **Visual Studio 2022 Build Tools** with the "Desktop development
  with C++" workload

### 9.7 Missing C source files in the runtime manifest

**Symptom:** `undefined symbol: ui_render_frame` (or similar) when linking.

**Fix:** Add the new `.c` file to `runtime/native_core_runtime.toml`, then
regenerate the Bazel manifest:

```bash
python tools/bazel/sync_native_runtime_builds.py
bazel build //runtime:native_core_runtime --config=dev --disk_cache=
```

---

## 10. How to Rebuild the Runtime

### Quick rebuild (full automation)

```powershell
# Option A: Use the legacy Python sync script (recommended)
python scripts/python/kain_bazel_sync.py sync

# Option B: Manual step-by-step
bazel build //runtime:native_core_runtime --config=dev --disk_cache=
for /f %i in ('bazel info bazel-bin --config=dev') do set BB=%i
lib.exe /NOLOGO /OUT:%USERPROFILE%\.kain\lib\kain_runtime.lib ^
    "%BB%/runtime/_objs/native_core_runtime_c/*.obj"
```

### Step-by-step (Windows)

#### Step 1: Update the manifest (only if adding/removing .c files)

```powershell
# Edit runtime/native_core_runtime.toml to add/remove sources
# Then regenerate the Bazel data:
python tools/bazel/sync_native_runtime_builds.py
```

#### Step 2: Build with Bazel

```powershell
bazel build //runtime:native_core_runtime --config=dev --disk_cache=
```

The `--disk_cache=` flag forces re-evaluation of all C source files.

#### Step 3: Archive .obj files into .lib

```powershell
for /f %i in ('bazel info bazel-bin --config=dev') do set BB=%i
lib.exe /NOLOGO /OUT:%USERPROFILE%\.kain\lib\kain_runtime.lib ^
    "%BB%/runtime/_objs/native_core_runtime_c/*.obj"
```

#### Step 4: Verify

```powershell
# Check the timestamp
dir %USERPROFILE%\.kain\lib\kain_runtime.lib

# Build a test Kain file
kain build hello.kn --target llvm
```

### Step-by-step (Linux/macOS)

```bash
# Build with Bazel
bazel build //runtime:native_core_runtime --config=dev --disk_cache=

# POSIX: cc_library produces .a directly
cp bazel-bin/runtime/libnative_core_runtime.a ~/.kain/lib/libkain_runtime.a
```

### Step-by-step (Makefile, local dev only)

```bash
# Quick compile + static library — does NOT install to ~/.kain/lib/
cd runtime/native
make lib

# The .a/.lib lives at:
#   runtime/native/_build/lib/libkain_runtime.a   (Linux/macOS)
#   runtime/native/_build/lib/libkain_runtime.lib (Windows/MinGW)

# Copy manually if you want to use it:
cp _build/lib/libkain_runtime.a ~/.kain/lib/
```

### Rebuilding the freestanding runtime

```bash
bazel build //runtime:native_core_freestanding --config=dev
# Output: bazel-bin/runtime/libnative_core_freestanding.a
```

### Verifying freshness

```bash
# Check when the runtime .lib was last built
kain_status

# Check if a rebuild is needed
python scripts/python/kain_bazel_sync.py check-stamp
```

---

## 11. Appendix: Key Files and Paths

### Runtime source and build

| What | Path |
|---|---|
| C source files | `runtime/native/src/core/*.c`, `src/ui/*.c`, `src/platform/*.c` |
| Platform sources (Windows) | `runtime/native/src/platform/win32/*.c` |
| Platform sources (Linux) | `runtime/native/src/platform/linux/*.c` |
| Platform sources (macOS) | `runtime/native/src/platform/macos/*.c` |
| Public headers | `runtime/native/include/*.h` |
| Canonical manifest | `runtime/native_core_runtime.toml` |
| Freestanding manifest | `runtime/native_core_freestanding.toml` |
| Generated Bazel data | `runtime/runtime_manifest_data.bzl` |
| Bazel rules macros | `runtime/native_runtime_rules.bzl` |
| Bazel BUILD file | `runtime/BUILD.bazel` |
| Freestanding BUILD | `runtime/BUILD.freestanding.bzl` |
| Makefile (local dev) | `runtime/native/Makefile` |

### Compiler toolchain integration

| What | Path |
|---|---|
| Clang discovery | `crates/core/src/install_layout.rs` |
| Linker invocation | `crates/build/src/native_link.rs` |
| Toolchain discovery script | `scripts/kain_toolchain.py` |
| Bazel sync script | `scripts/python/kain_bazel_sync.py` |
| Manifest generator | `tools/bazel/sync_native_runtime_builds.py` |
| Bazel config | `.bazelrc` |

### Installed runtime

| What | Path (Windows) | Path (POSIX) |
|---|---|---|
| Static library | `~/.kain/lib/kain_runtime.lib` | `~/.kain/lib/libkain_runtime.a` |
| Freestanding lib | `~/.kain/lib/kain_runtime_core.lib` | `~/.kain/lib/libkain_runtime_core.a` |
| Compiler binary | `~/.kain/bin/kain.exe` | `~/.kain/bin/kain` |
| Config | `~/.kain/config.toml` | `~/.kain/config.toml` |

### Verification

| What | Path |
|---|---|
| CBMC harnesses | `runtime/native/test/cbmc/check_*.c` |
| CBMC pipeline | `runtime/native/test/scripts/run_pipeline.py` |
| Z3 proof packs | `runtime/native/src/core/z3/proofs/*.yaml` |
| Test README | `runtime/native/test/README.md` |
| GCC for CBMC preprocessing | Any `gcc` on PATH (Windows: MinGW or WSL) |

---

> **Last updated:** 2026-06-24  
> **Questions?** Check `TROUBLESHOOTING_DEV.md` for symptom-based fixes,
> or grep for the relevant symbol in `crates/core/src/install_layout.rs`
> and `crates/build/src/native_link.rs`.
