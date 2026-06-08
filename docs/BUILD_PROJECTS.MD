# Kain Build System & Projects

> **Date**: 2026-06-08
> **Read after**: RULEBOOK.md
> **See also**: C_GUIDE.MD (C interop), PYTHON_GUIDE.MD (Python interop), GLOSSARY.MD (term map)

---

## Table of Contents

1. [The One File — build.kn is the Authority](#1-the-one-file--buildkn-is-the-authority)
2. [The Evidence DAG — Build Graph Concepts](#2-the-evidence-dag--build-graph-concepts)
3. [Project Layout — Standard Structure](#3-project-layout--standard-structure)
4. [CLI Command Reference](#4-cli-command-reference)
5. [Build Targets — Full Target Table](#5-build-targets--full-target-table)
6. [DWARF Debug Metadata](#6-dwarf-debug-metadata)
7. [Environment Variables](#7-environment-variables)
8. [Module & Import System](#8-module--import-system)
9. [Real build.kn Examples — Annotated Walkthroughs](#9-real-buildkn-examples--annotated-walkthroughs)
10. [build.kn DSL Reference](#10-buildkn-dsl-reference)
11. [Quick Reference](#11-quick-reference)

---

## 1. The One File — build.kn is the Authority

Every Kain project has exactly **one build authority**: `build.kn`. This file is a Kain source file (not YAML, not TOML, not JSON) that exports a `fn build(ctx: BuildContext) -> BuildGraph` function using the `std::build` DSL. It declares packages, blades, tasks, dependencies, and artifacts in a declarative DAG.

**KAIN.toml files exist throughout the repo** as compatibility metadata for C FFI and blade discovery. They are read by the toolchain for legacy metadata (package name, version) but **all build logic lives in build.kn**. Think of KAIN.toml as a living README — informative, but not authoritative.

### Canonical Minimal build.kn

```kn
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let pkg = package("my-project")
        .version("0.1.0")
        .description("A minimal Kain project")

    let blade_spec = blade("my-project")
        .kind("kain_library")
        .entry("src/main.kn")
        .source_root("src")
        .module_root("src")
        .build_target("llvm")

    let defaults = build_defaults()
        .entry("src/main.kn")
        .artifact_root(".kain/out")
        .cache_root(".kain/cache/build")
        .profile("debug")
        .target("llvm")

    let check = build_check("check-llvm")
        .entry("src/main.kn")
        .target("llvm")
        .axis("target", "llvm")
        .input("src/main.kn")
        .input("build.kn")

    let exe = native_executable("root-executable")
        .entry("src/main.kn")
        .root_output("$blade/my-project.exe")
        .requires("check-llvm")

    return build_graph()
        .package(pkg)
        .blade(blade_spec)
        .defaults(defaults)
        .task(check)
        .task(exe)
```

### Key Points

- **`use std::build`** — imports the build DSL. Every build.kn starts here.
- **`BuildContext`** — injected by the compiler at build time. Contains `workspace_root`, `blade_root`, `host`, `target`, `lane`, `profile`.
- **`build_graph()`** — the return value. A collection of packages, blades, and tasks.
- **`$blade/` and `$root/` prefixes** — path interpolation. `$blade` resolves to the blade's output root, `$root` to the workspace root.
- **Builder pattern** — every spec struct uses fluent `.method()` chaining that returns a new copy. Nothing is mutated in place.

---

## 2. The Evidence DAG — Build Graph Concepts

Every `kain build`, `kain run`, or `kain test` invocation resolves a **BuildGraph** — a directed acyclic graph of tasks with dependency edges. The compiler walks this graph, executes each task in order, and produces evidence (artifacts, reports, telemetry).

### Core Types

| Type | Purpose |
|------|---------|
| `BuildContext` | Injected context: workspace, blade, host, target, lane, profile |
| `PackageSpec` | A named package (name, version, description) |
| `BladeSpec` | A blade: entry file, source/module roots, build targets |
| `ProjectSpec` | Combined package+blade shorthand for simple projects |
| `SourceSetSpec` | A named set of source files, globs, and excludes |
| `BuildTaskSpec` | A single task in the DAG with id, kind, inputs, outputs, deps |
| `BuildGraphSpec` | The full graph: packages + blades + tasks |

### Task Kinds

| Kind Constant | Task Function | What It Does |
|---------------|---------------|-------------|
| `BUILD_KIND_CHECK` | `build_check(id)`, `check_task(id)` | Typecheck only, no codegen |
| `BUILD_KIND_NATIVE_EXECUTABLE` | `native_executable(id)`, `root_executable(id)` | Compile to native .exe |
| `BUILD_KIND_TEST` | `source_tests(id)` | Run inline source tests |
| `BUILD_KIND_PROOF` | `proof(id)` | Run formal proof cases |
| `BUILD_KIND_BENCHMARK` | `benchmark(id)` | Run performance benchmarks |
| `BUILD_KIND_ATTRITION` | `attrition(id)` | Run attrition/sabotage tests |
| `BUILD_KIND_CERTIFY` | `certify(subject)` | Meta-task: certify that dependencies passed |
| `BUILD_KIND_EXEC` | `exec_task(id)`, `command_task(id)` | Run an arbitrary command |
| `BUILD_KIND_AMALGAMATE` | `amalgamate_capsule(id)`, `capsule_set(id)` | Pack sources into portable capsule |

### Dependency Edges

Tasks declare dependencies via `.requires(task_id)` or `.after(task_id)`. The graph is topologically sorted at build time:

```kn
let cert = certify("my-project.local")
    .requires(check)         # requires passes first
    .requires(exe)           # then exe
    .requires(tests)         # then tests
```

The compiler **validates the DAG** — cycles, missing task references, and dangling outputs all produce errors before any work begins.

### Path Interpolation

Tasks understand these path prefixes:

| Prefix | Resolves To |
|--------|-------------|
| `$root` | Workspace root (where build.kn lives) |
| `$blade` | Blade output root (usually `.kain/out/`) |
| `$task` | Task cache root (usually `.kain/cache/build/<task-id>/`) |

---

## 3. Project Layout — Standard Structure

```
my-project/
├── build.kn                    # Mandatory — build authority
├── KAIN.toml                   # Vestigial — compatibility metadata
├── src/                        # Source root
│   ├── main.kn                 # Entry point
│   ├── lib/                    # Internal modules
│   │   └── helpers.kn
│   └── core/                   # Core module root (alternative)
│       └── my_project.kn
├── native/                     # Optional C/C++ companions
│   ├── bridge.h
│   └── bridge.c
├── .kain/                      # Auto-generated — NEVER check in
│   ├── out/                    # Build artifacts
│   ├── cache/build/            # Task cache
│   ├── cache/run/              # Run cache
│   ├── reports/                # JSON report artifacts
│   └── platform/               # Imported platform SDKs
├── telemetry/                  # Optional runner scripts
│   └── run_tests.ps1
└── generated/                  # Optional generated sources
    └── native_runtime/
```

### Starter Template (`blades/templates/starter/`)

The minimal scaffold from `blades/templates/starter/` contains exactly:

```
build.kn        # build authority (17 lines)
src/
  main.kn       # "hello world" entry point
```

```kn
// blades/templates/starter/src/main.kn
use std::io

fn main() -> Int:
    println("hello world")
    return 0
```

Use `kain init my-project` to scaffold — it creates the standard layout, a starter `build.kn`, and a `src/main.kn`.

---

## 4. CLI Command Reference

Every command is available as `kain <subcommand> [options] [input]`. Most also work as `kn <subcommand>`.

### Typecheck — `kain check`

**Purpose**: Fast typecheck-only pass — no codegen, no output files. Ideal for CI gates and IDE-level feedback.

```bash
# Basic typecheck
kain check src/main.kn

# Typecheck with structured JSON output (for CI/LLM consumption)
kain check src/main.kn --json

# Write JSON report to file
kain check src/main.kn --json-out report.json

# Typecheck everything under a directory
kain check blades/kain/asm/src/main.kn

# Fail fast on first error
kain check src/ --fail-fast

# Read from stdin
kain check -
```

| Flag | Effect |
|------|--------|
| `--json` | Emit structured JSON to stdout |
| `--json-out <FILE>` | Write JSON report to file |
| `--target <TARGET>` | Target profile to typecheck against (default: `run`) |
| `--fail-fast` | Stop after first failed file |
| `--config <CONFIG>` | Explicit config path |

**What it produces**: Exit code 0 (success) or non-zero (failure). JSON report if requested. Diagnostics formatted to stderr.

---

### Build — `kain build`

**Purpose**: Compile sources to a target (LLVM, WASM, etc.) using the build graph from `build.kn`.

```bash
# Build using the nearest build.kn (current or parent directory)
kain build

# Build a specific file directly
kain build src/main.kn --target llvm

# Build with DWARF debug metadata
kain build src/main.kn --target llvm --debug

# Build to WASM
kain build src/main.kn --target wasm

# Build a specific blade
kain build blades/c/sqlite/main.kn --target llvm

# Build in release lane
kain build --lane release

# Clean then build
kain build --clean

# Build with multiple targets
kain build --targets llvm,wasm,js

# Build with output path override
kain build -o dist/app.exe --target llvm
```

| Flag | Effect |
|------|--------|
| `-t, --target <TARGET>` | Single target override |
| `--targets <TARGETS>` | Comma-separated targets |
| `--lane <LANE>` | Build lane: bootstrap, dev, release, dist, selfhost |
| `-g, --debug` | Emit DWARF debug metadata |
| `--clean` | Clean `.kain/` roots before building |
| `--embed` | Embed original Kain source in generated C++ |
| `--ue5` | Build Unreal Engine 5 plugin |
| `--config <CONFIG>` | Explicit config path |

**What it produces**: Depends on target — `.exe` (llvm), `.wasm` (wasm), `.c` (c), etc.

---

### Run — `kain run`

**Purpose**: Build (if needed) and execute the result. The unified run pipeline resolves the entry point and handles compilation + execution in one step.

```bash
# Run the default project
kain run

# Run a specific file
kain run src/main.kn --target llvm

# Run with DWARF debug metadata
kain run src/main.kn --target llvm --debug

# Pass runtime arguments to the executable (after --)
kain run src/main.kn --target llvm -- -arg1 -arg2 --flag

# Watch mode — re-run on file changes
kain run dev src/main.kn

# Dry-run: print the resolved run plan without executing
kain run plan src/main.kn

# Emit JSON run report
kain run src/main.kn --json

# Keep intermediate artifacts
kain run src/main.kn --keep-artifacts
```

| Command | Purpose |
|---------|---------|
| `kain run` | Single build+execute |
| `kain run dev` | Watch mode — re-runs on changes |
| `kain run plan` | Print resolved plan without executing |

| Flag | Effect |
|------|--------|
| `--target <TARGET>` | Run target override (default: auto) |
| `-g, --debug` | Emit DWARF debug metadata |
| `--json` | Emit JSON run report to stdout |
| `--trace` | Include trace-oriented report detail |
| `--keep-artifacts` | Keep cached/generated artifacts |
| `--dry-run` | Print resolved plan without executing |

---

### Test — `kain test`

**Purpose**: Run Kain source tests using compiletest-style pass/fail directives. Supports `//@` annotations for expected behavior.

```bash
# Run all tests in a directory (recursively)
kain test src/

# Test a single file
kain test src/tests/test_math.kn

# Run with JSON output
kain test src/ --json

# Write JSON report to file
kain test src/ --json-out test_report.json

# Run only ignored tests
kain test src/ --ignored

# Override test mode
kain test src/ --mode run-pass

# Fail fast on first failure
kain test src/ --fail-fast
```

| Flag | Effect |
|------|--------|
| `--mode <MODE>` | Override mode: check-pass, check-fail, run-pass, run-fail, kain-test |
| `--target <TARGET>` | Default target for check modes (default: run) |
| `--ignored` | Run cases marked `//@ ignore` |
| `--fail-fast` | Stop after first failed case |
| `--json` | Emit structured JSON to stdout |
| `--json-out <FILE>` | Write JSON report to file |

**Test directives** (in source comments):

```
//@ run-pass        — expects successful execution
//@ run-fail        — expects runtime failure
//@ check-pass      — expects successful typecheck
//@ check-fail      — expects typecheck error
//@ ignore          — skip this test
//@ needs-target:wasm  — skip unless wasm target is available
```

---

### Format — `kain fmt`

**Purpose**: Canonical source formatting — compiler-owned, no config files, one style.

```bash
# Check formatting (no changes)
kain fmt src/ --check

# Format in place
kain fmt src/ --write

# Format multiple files/directories
kain fmt src/main.kn src/lib/ --write

# Read from stdin
kain fmt -
```

| Flag | Effect |
|------|--------|
| `--check` | Only check, don't write |
| `-w, --write` | Rewrite files in place |

---

### Doctor — `kain doctor`

**Purpose**: Full environment diagnostic — binary info, targets, PATH, stdlib, toolchain.

```bash
# Full diagnostic
kain doctor

# Repair a file (parser recovery)
kain doctor --repair src/main.kn --write

# Repair all files under a tree
kain doctor --repair-tree src/ --write

# Preview repairs without writing
kain doctor --repair src/main.kn --dry-run

# Suggest repairs (safe mode)
kain doctor --repair src/main.kn --suggest

# Aggressive repair profile
kain doctor --repair src/main.kn --profile aggressive
```

**What it reports**: Binary version, build SHA, profile, target triple, Kain HOME, config path, stdlib roots, runtime C path, LLVM Clang path, PATH matches, supported targets, sync status.

---

### Config — `kain config`

**Purpose**: View and modify the active Kain control plane.

```bash
# Show active config
kain config show

# Show config as JSON
kain config show --json

# Set a config key
kain config set build.native-debug-info true
kain config set ui.theme arctic
kain config set build.jobs 8

# Initialize a fresh config
kain config init
```

**Config keys**:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `build.jobs` | int/string | `"smart"` | Parallelism for builds |
| `build.native-profile` | string | `"debug"` | Native compiler profile |
| `build.native-opt-level` | string | `"0"` | Optimization level |
| `build.native-target-cpu` | string | `"default"` | Target CPU for native |
| `build.native-debug-info` | bool | `true` | Emit DWARF debug info |
| `ui.theme` | string | `"slate"` | CLI theme |
| `ui.color` | string | `"auto"` | Color policy |
| `diagnostics.capture` | bool | `false` | Capture diagnostics to file |

---

### Clean — `kain clean`

**Purpose**: Remove generated `.kain/` artifacts.

```bash
# Clean everything
kain clean

# Clean only build artifacts
kain clean --scope build

# Clean only run artifacts
kain clean --scope run

# Dry-run preview
kain clean --dry-run

# JSON output
kain clean --json

# Clean a specific workspace
kain clean blades/kain/asm/
```

| Flag | Effect |
|------|--------|
| `--scope <SCOPE>` | Clean scope: build, run, amalgamate, or all (default) |
| `--dry-run` | Print plan without removing |
| `--json` | Emit JSON |

---

### Amalgamate — `kain amalgamate`

**Purpose**: Pack sources into a portable Kain capsule — a single-file distribution format.

```bash
# Pack a project into a capsule
kain amalgamate src/main.kn -o my-project.kn

# Inspect a capsule without unpacking
kain amalgamate inspect my-project.kn

# Unpack a capsule to directory
kain amalgamate unpack my-project.kn -o my-project/

# Pack with metadata
kain amalgamate src/ -o lib.kn --name my-lib --version 0.1.0 --author "Me"

# Full archive (compressed, not editable inline)
kain amalgamate src/ -o lib.kn --archive

# Sidecar capsule set
kain amalgamate src/ -o lib.kn --capsule-set my-lib
```

| Flag | Effect |
|------|--------|
| `-o, --output <PATH>` | Output capsule path |
| `--name <NAME>` | Capsule display name |
| `--version <VERSION>` | Version label |
| `--author <AUTHOR>` | Repeatable author field |
| `--note <NOTE>` | Repeatable note |
| `--tag <TAG>` | Repeatable tag |
| `--meta <KEY=VALUE>` | Arbitrary metadata |
| `--contents <TYPE>` | source, snapshot, assets, artifacts, or evidence |
| `--archive` | Store as compressed archive |
| `--compression <TYPE>` | zstd (default) or none |
| `--capsule-set <NAME>` | Sidecar set name |
| `--header <MODE>` | minimal, rich, or off |

---

### GPU Artifacts — `kain gpu-artifacts`

**Purpose**: Compile GPU shaders (SPIR-V, PTX/CUDA, HLSL) from Kain compute/fragment shader sources.

```bash
# Generate all GPU artifacts
kain gpu-artifacts shaders.kn

# Generate only SPIR-V
kain gpu-artifacts shaders.kn --target spirv

# Generate only CUDA PTX
kain gpu-artifacts shaders.kn --target cuda

# Specify output directory
kain gpu-artifacts shaders.kn -o out/shaders/

# Skip residency sidecar generation
kain gpu-artifacts shaders.kn --no-residency

# Skip derived cross-target artifacts
kain gpu-artifacts shaders.kn --no-derived
```

| Flag | Effect |
|------|--------|
| `--target <TARGET>` | spirv, cuda, hlsl, or all (default) |
| `-o, --output <PATH>` | Output directory |
| `--no-residency` | Skip compute residency sidecars |
| `--no-derived` | Skip derived cross-target artifacts |

---

### Import C — `kain import-c`

**Purpose**: Import C headers into Kain via libclang extraction.

```bash
# Import a single C header
kain import-c vendor/sdk/include/sdk.h -o gen/sdk.kn

# Import with include paths and defines
kain import-c vendor/sdk -I vendor/include -DPLATFORM_WIN32 -o gen/sdk.kn

# Import and compile directly to LLVM (skip .kn output)
kain import-c vendor/header.h --target llvm

# Flatten all symbols into global scope
kain import-c vendor/sdk -o gen/sdk.kn --flat

# Filter by include/exclude paths
kain import-c vendor/sdk --include "core/" --exclude "test/"

# Write failure report
kain import-c vendor/sdk --report-json import-report.json

# Fail fast
kain import-c vendor/sdk --fail-fast
```

| Flag | Effect |
|------|--------|
| `-o, --output <PATH>` | Output .kn file |
| `-I, --include-paths <DIRS>` | Preprocessor include paths |
| `-D, --defines <DEFINES>` | Preprocessor defines |
| `--flat` | Flatten into global scope |
| `--include <FILTER>` | Only files containing filter |
| `--exclude <FILTER>` | Exclude files containing filter |
| `--report-json <FILE>` | Failure report |
| `--fail-fast` | Stop on first failure |

---

### Import Platform — `kain import platform`

**Purpose**: Import a native platform SDK/package into target-aware thunks.

```bash
# Import Vulkan SDK
kain import platform vulkan --sdk "C:/VulkanSDK/1.3.296.0"

# Import with explicit registry
kain import platform vulkan --registry vk.xml

# Import to a specific target triple
kain import platform vulkan --target-triple x86_64-pc-windows-msvc

# Dry-run (print lock, write nothing)
kain import platform vulkan --dry-run

# Specify output directory
kain import platform vulkan -o gen/vulkan/
```

| Flag | Effect |
|------|--------|
| `--sdk <PATH>` | Explicit SDK root |
| `--provider <NAME>` | Provider label for lockfile |
| `--target-triple <TRIPLE>` | Target triple |
| `--registry <PATH>` | Explicit registry metadata file |
| `--header <PATH>` | Explicit C header entrypoint |
| `-o, --output <PATH>` | Output directory |
| `--dry-run` | Preview without writing |
| `--report-json <FILE>` | Write lock/report JSON |

---

### Language Server — `kain lsp`

**Purpose**: Start the Kain Language Server for IDE integration.

```bash
# Start LSP (stdio protocol)
kain lsp
```

Supports LSP-compatible editors (VS Code, VSCodium, Neovim, etc.).

---

### Init — `kain init`

**Purpose**: Scaffold a new Kain project with standard layout and starter build.kn.

```bash
# Init current directory
kain init

# Init with explicit project name
kain init --name my-project

# Init at specific path
kain init ./my-project
```

Creates: `build.kn`, `src/main.kn`, `KAIN.toml`.

---

## 5. Build Targets — Full Target Table

`kain build` and `kain check` accept `--target`/`--targets` flags. Here is every supported target:

| Flag | Output | Extension | Use Case |
|------|--------|-----------|----------|
| `--target llvm` | LLVM IR → native | `.exe` / `.dll` | **Standard native executable** |
| `--target c` | C source | `.c` | C interop, embedding |
| `--target cpp` | C++ source | `.cpp` | C++ interop, Unreal |
| `--target rust` | Rust source | `.rs` | Rust interop, cargo integration |
| `--target wasm` | WebAssembly | `.wasm` | Web, WASM runtimes |
| `--target spirv` | SPIR-V shader | `.spv` | Vulkan, GPU compute |
| `--target cuda` | CUDA PTX | `.ptx` | NVIDIA GPU compute |
| `--target hlsl` | HLSL shader | `.hlsl` | DirectX, GPU |
| `--target wgsl` | WGSL shader | `.wgsl` | WebGPU |
| `--target js` | JavaScript | `.js` | JS runtimes |
| `--target typescript` | TypeScript | `.ts` | TS projects |
| `--target ue5` | Unreal plugin | `.h/.cpp` | Unreal Engine 5 |

**Current supported targets** (from `kain doctor`): `wasm`, `llvm`, `c`, `spirv`, `hlsl`, `wgsl`, `cuda`, `usf`, `js`, `ts`, `rust`, `hybrid`, `cpp`, `ue5`, `ue5editor`, `run`, `test`, `ks`.

**Default target** for `kain build` without `--target`: `llvm`.

---

## 6. DWARF Debug Metadata

DWARF debug info is a **first-class feature** in Kain, not an afterthought. The compiler emits `!DILocation`, `!DISubprogram`, `!DISubroutineType`, and full source-line mappings into the LLVM IR so that native debuggers (LLDB, WinDbg, Visual Studio) can show **Kain source lines**, not the generated IR.

### Enabling DWARF

```bash
# Build with DWARF
kain build src/main.kn --target llvm --debug

# Run with DWARF
kain run src/main.kn --target llvm --debug

# Persistent config
kain config set build.native-debug-info true
```

The `-g` / `--debug` flag is equivalent to `-g` in clang/GCC. Without it, the compiler emits no debug metadata.

### Profile Behavior

| Profile | Debug Info | Optimization | Use Case |
|---------|-----------|-------------|----------|
| `debug` (default) | DWARF (`-g`) | `-O0` | Development, breakpoints |
| `release` | None (`-g0`) | `-O2` | Shipping, performance |
| `benchmark-release` | None (`-g0`) | `-O3 -march=native` | Benchmarking |

Profiles are set in `build.kn` via `.profile("debug")` on `build_defaults()` or `ProjectSpec`, or via `kain build --lane release`.

### Debugging in LLDB

```bash
# Build with debug info
kain build blades/c/platform/windows/src/main.kn --target llvm --debug

# Debug the resulting exe
lldb ./blades/c/platform/windows/src/main.exe
(lldb) breakpoint set --file main.kn --line 7
(lldb) run
(lldb) frame variable
(lldb) source list
```

**What you get**:
- Kain source file paths in backtraces
- Kain line numbers mapped to IR instructions
- Local variable names (when the optimizer doesn't eliminate them)
- Function names matching your Kain source

### Config Keys

```bash
kain config set build.native-debug-info true    # Enable DWARF by default
kain config set build.native-opt-level 0         # -O0 for best debug experience
kain config set build.native-profile debug       # Use debug profile
```

**Important**: DWARF is only meaningful for `--target llvm`. Other targets (wasm, js, spirv) don't support native debug metadata.

---

## 7. Environment Variables

The Kain toolchain respects these environment variables:

| Variable | Purpose | Default |
|----------|---------|---------|
| `KAIN_HOME` | Root directory for Kain installation | `~/.kain` |
| `KAIN_CONFIG` | Path to active config.toml | `$KAIN_HOME/config.toml` |
| `KAIN_STDLIB_PATH` | Path to stdlib root | `$KAIN_HOME/stdlib` or repo-relative |
| `KAIN_CLANG_PATH` | Path to clang executable for C interop | Auto-detected |
| `KAIN_RUNTIME_MANIFEST_PATH` | Path to native runtime manifest | `$KAIN_HOME/runtime.toml` |
| `KAIN_RUNTIME_C_PATH` | Path to runtime C source | Auto-detected |
| `KAIN_REPO_ROOT` | Root of the Kain repo checkout | Auto-detected |
| `KAIN_SYNC_ROOT` | Sync state directory | `$KAIN_HOME/state` |
| `KAIN_SYNC_STAMP_PATH` | Sync stamp JSON file | `$KAIN_HOME/state/state/sync_stamp.json` |
| `KAIN_SYNC_LOCK_PATH` | Sync lock file | `$KAIN_HOME/state/locks/sync.lock` |
| `KAIN_BAZEL_CONFIG` | Bazel config for internal builds | `dev` |
| `KAIN_BAZEL_LAUNCHER_DIR` | Bazel launcher directory | `$KAIN_HOME/bin` |
| `KAIN_BAZEL_PYTHON` | Python executable for Bazel scripts | Auto-detected |
| `KAIN_BENCH_V2_EXE` | Benchmark v2 executable path | Set by build.kn |
| `KAIN_BENCH_V2_FILTER` | Benchmark filter pattern | (none) |
| `KAIN_C_ABI_FLAVOR` | C ABI policy override | `default` |
| `KAIN_CACHE_DIR` | Global cache directory | `$KAIN_HOME/cache` |
| `KAIN_GENERATED_DIR` | Generated artifacts root | `$KAIN_HOME/generated` |
| `KAIN_PACKAGES_DIR` | Installed packages directory | `$KAIN_HOME/packages` |
| `KAIN_TOOLING_DIR` | Tooling directory | `$KAIN_HOME/tooling` |

**Config resolution order**:
1. `--config <PATH>` flag (explicit override)
2. `KAIN_CONFIG` env var
3. Search parent directories for `.kain/config.toml`
4. `$KAIN_HOME/config.toml`

### Bench-Specific Variables

These are set by build.kn's `exec_task` with `.env()`:

```kn
let telemetry = exec_task("telemetry-v2")
    .env("KAIN_BENCH_V2_EXE", "$root/kain-benchmark-v2.exe")
    .env("KAIN_BENCH_V2_MARKDOWN", "$root/latest_v2.md")
    .env("KAIN_BENCH_V2_JSON", "$root/out/reports/latest_v2.json")
    .env("KAIN_BENCH_V2_TRACK_ROOT", "$root/out/reports/v2_tracks")
```

---

## 8. Module & Import System

Kain's module resolution is **filesystem-native**: module paths map directly to file paths. There is no package index or registry — `use math` means "find `math.kn` in the resolution path."

### Module Import Syntax

```kn
use std::io                            # Stdlib module
use std::collections                   # Nested stdlib module
use math                               # Local module
use feature::panel                     # Nested local module
use c::windows::win32                  # C interop module
use rust::serde                        # Rust interop module
```

### Resolution Order

When the compiler encounters `use feature::panel`, it searches in this order:

1. **Local filesystem** (relative to the importing file and its parents):
   - `<importing_dir>/feature/panel.kn`
   - `<importing_dir>/src/feature/panel.kn`
   - `<importing_dir>/src/core/feature.kn`
   - `<importing_dir>/feature/panel.kn`
   - Walks UP parent directories checking the same patterns

2. **Blade module roots** (from build.kn's `module_root()` or `module_roots()`):
   - `<module_root>/feature/panel.kn`

3. **Stdlib roots** (from `KAIN_STDLIB_PATH`):
   - `<stdlib_root>/feature/panel.kn`

4. **Installed packages** (from `KAIN_HOME/packages/`):
   - `<package_root>/<module_root>/feature/panel.kn`

### Stdlib Module Aliases

Some nested stdlib modules have flat aliases for convenience:

```
std::graphics::shared  →  graphics/shared.kn  (alias: graphics_shared.kn)
std::native::<module>  →  strips the "native/" prefix
```

### Module Organization Rules

- One concept = one `.kn` file in `stdlib/`
- Nested modules use filesystem nesting: `std::os::path` → `stdlib/os_path.kn`
- The `pub` keyword controls visibility: `pub fn`, `pub struct`, `pub const`
- Private items are accessible only within their module
- `mod` declarations create inline submodules

### Module Discovery Example

A `use my_app::utils` in `src/main.kn` inside a workspace with `module_root("src")` resolves:

1. `<project>/src/my_app/utils.kn` ← **found here**
2. `<project>/src/main/my_app/utils.kn` (importer-relative)
3. `<project>/my_app/utils.kn` (workspace root)
4. `<stdlib>/my_app/utils.kn` (stdlib fallback)

---

## 9. Real build.kn Examples — Annotated Walkthroughs

### 9.1 Minimal Blade — `blades/kain/asm/build.kn`

The simplest possible build.kn. A single blade, a single native executable.

```kn
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let pkg = package("asm")
        .version("1.0.0")
        .description("Space Invaders — inline asm + Win32, zero C bridge, pure metal Kain")

    let blade_spec = blade("asm")
        .kind("kain_library")           # Blade kind (kain_library, kain_binary, kain_executable)
        .entry("src/main.kn")           # Entry point file
        .source_root("src")             # Where to find .kn sources
        .module_root("src")             # Module resolution root
        .build_target("llvm")           # What to build for

    let defaults = build_defaults()
        .entry("src/main.kn")
        .artifact_root(".kain/out")     # Where build artifacts go
        .cache_root(".kain/cache/build")# Task cache
        .profile("debug")               # debug or release
        .target("llvm")                 # Default target

    let check = build_check("check-llvm")
        .entry("src/main.kn")
        .target("llvm")
        .axis("target", "llvm")         # Matrix axis for CI telemetry
        .input("src/main.kn")
        .input("build.kn")
        .input("KAIN.toml")

    let root_exe = native_executable("root-executable")
        .entry("src/main.kn")
        .root_output("$blade/asm_space_invaders.exe")  # $blade = blade output root
        .requires("check-llvm")                         # Dependency edge
        .input("src/main.kn")
        .input("build.kn")
        .input("KAIN.toml")

    return build_graph()
        .package(pkg)
        .blade(blade_spec)
        .defaults(defaults)
        .task(check)
        .task(root_exe)
```

**Pattern**: `package()` + `blade()` + `build_defaults()` + `build_check()` + `native_executable()` → `build_graph()`. Every project follows this core pattern.

---

### 9.2 C Interop Blade — `blades/c/sqlite/` pattern

For projects that use C FFI via `include ... as ...`, the build.kn adds C companion sources and the KAIN.toml records the blade metadata.

```kn
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let app = project("sqlite-natural-include")
        .kind("kain_executable")
        .version("0.1.0")
        .description("SQLite amalgamation smoke for natural C header imports")
        .entry("main.kn")                    # Entry at root level
        .source_root(".")                    # Source root is .
        .module_root(".")                    # Module root is .
        .target("llvm")
        .artifact_root(".kain/out")
        .cache_root(".kain/cache/build")
        .profile("debug")

    let sources = source_set("sqlite-sources")
        .glob("**/*.kn")
        .glob("**/*.{h,c}")                  # Include C sources
        .file("build.kn")
        .file("KAIN.toml")

    let check = check_task("check-llvm")
        .project(app)
        .target("llvm")
        .inputs(sources)

    let exe = native_executable("root-executable")
        .project(app)
        .output("$blade/sqlite_test.exe")
        .requires(check)
        .inputs(sources)

    return build_graph(app)
        .sources(sources)
        .tasks(check, exe)
```

**Key differences from minimal**:
- Uses `project()` shorthand instead of separate `package()` + `blade()`
- `source_set()` groups sources with glob patterns
- `.inputs(sources)` passes the entire source set (not individual files)
- C files (`*.{h,c}`) are discovered but **not compiled** — the compiler includes them in the build edge so changes trigger recompilation

KAIN.toml companion:

```toml
[package]
name = "sqlite-natural-include"
version = "0.1.0"
description = "SQLite amalgamation smoke for natural C header imports in Kain."

[blade]
name = "sqlite-natural-include"
kind = "kain_binary"
entry = "main.kn"
source_roots = ["."]
module_roots = ["."]
build_targets = ["llvm"]

[run]
entry = "main.kn"
target = "llvm"

[build]
entry = "main.kn"
artifact_root = ".kain/out"
cache_root = ".kain/cache/build"
profile = "debug"
```

---

### 9.3 Full Workspace — `smoketest/build.kn`

The largest build graph in the repo: album-edition workspace with 9 module roots, GPU artifacts, WASM, telemetry runners, and certification.

```kn
use std::build

const ALBUM_ROOTS = [
    "src/semantics",
    "src/systems",
    "src/gpu",
    "src/stdlib",
    "src/interop",
    "src/wasm",
    "src/telemetry",
    "src/ui",
]

const VISUAL_RUNNER_INPUTS = [
    "telemetry/invoke_kain.ps1",
    "telemetry/python_bridge.kn",
    "telemetry/run_smoketest_mode.kn",
    "native",
]

fn smoke_mode(name: String, args: Array<String>) -> BuildTask:
    return album_mode("album-" + name)
        .mode(name)
        .runner("telemetry/run_smoketest_mode.kn")
        .executable("$root/smoketest.exe")
        .output_dir("$root/telemetry/" + name)
        .inputs(VISUAL_RUNNER_INPUTS)
        .args(args)
        .requires("root-executable")
        .requires("check-telemetry-runner")
        .telemetry("smoketest." + name)

fn build(ctx: BuildContext) -> BuildGraph:
    let album = project("smoketest")
        .kind("kain_executable")
        .version("0.1.0")
        .description("Album-edition workspace covering every Kain feature.")
        .entry("src/main.kn")
        .source_roots(ALBUM_ROOTS)           # Multiple source roots
        .module_roots(ALBUM_ROOTS)           # Multiple module roots
        .generated_root("telemetry")
        .targets("llvm", "spirv", "wasm")    # Multi-target
        .artifact_root(".kain/out")
        .cache_root(".kain/cache/build")

    let sources = source_set("album-sources")
        .root("src")
        .glob("src/**/*.kn")
        .glob("telemetry/**/*.kn")
        .glob("native/**/*.{h,c}")
        .file("KAIN.toml")
        .file("vendor/sqlite-src/manifest.uuid")
        .file("build-smoketest-visualizer-bridge.ps1")
        .file("run-visual-smoketest.ps1")

    let check = check_task("check-llvm")
        .project(album)
        .target("llvm")
        .inputs(sources)
        .telemetry("llm.evidence", "smoketest.album")

    let gpu = gpu_suite("gpu-artifacts")
        .fragment("src/gpu/fragment.kn")
        .compute("src/gpu/compute.kn")
        .targets("spirv", "cuda", "hlsl")
        .artifact_root(".kain/out/gpu")
        .requires("check-llvm")

    let wasm = check_task("check-wasm")
        .entry("src/wasm/wasm_main.kn")
        .target("wasm")
        .telemetry("llm.wasm")

    let tests = source_tests("source-tests")
        .project(album)
        .inputs(sources)
        .requires("check-llvm")

    let runner = kain_runner("check-telemetry-runner")
        .entry("telemetry/run_smoketest_mode.kn")
        .target("interpret")
        .inputs(VISUAL_RUNNER_INPUTS)
        .telemetry("smoketest.python-ffi")

    let exe = native_executable("root-executable")
        .project(album)
        .output("$blade/smoketest.exe")
        .requires(check, gpu, wasm, tests)   # Multiple dependencies

    let full = smoke_mode("full", [])         # Custom task factories
    let bench = smoke_mode("benchmark",
        ["--bench-rounds", "256", "--bench-passes", "6"])
    let abuse = smoke_mode("attrition",
        ["--attrition-ops", "20", "--attrition-rounds", "96"])

    let cert = certify("smoketest.local")
        .requires(check, runner, gpu, wasm, tests, exe, full, bench, abuse)

    let capsules = capsule_set("smoketest")
        .after(cert)
        .source("$root/smoketest.kn")
        .artifacts("$root/smoketest.artifacts.kn")
        .evidence("$root/smoketest.evidence.kn")
        .tag("portable")
        .tag("smoketest")
        .telemetry("smoketest.capsule")

    return build_graph(album)
        .sources(sources)
        .tasks(check, gpu, wasm, tests, runner, exe, full, bench, abuse, cert, capsules)
```

**Key patterns**:
- **Custom task factory** — `smoke_mode()` is a Kain function that returns a `BuildTask`. This keeps the build graph DRY.
- **Multi-target** — one project produces LLVM + SPIR-V + WASM artifacts.
- **Certification** — `certify()` is a meta-task that runs after everything else and attests the whole pipeline passed.
- **Capsule set** — final portable artifact with source, artifacts, and evidence.
- **Telemetry** — `.telemetry()` attaches evidence channels to each task for CI observability.

---

### 9.4 Benchmark Project — `benchmark/build.kn`

Shows `exec_task()` for running external commands, environment variables, and workspace defaults.

```kn
use std::build
use std::certify

fn build(ctx: BuildContext) -> BuildGraph:
    let ws = workspace_defaults()
        .search_root(".")
        .generated_root(".kain/generated")

    let pkg = package("benchmark")
        .version("0.2.0")
        .description("Benchmark v2 router lane for compact Kain-native telemetry-driven benchmark growth.")

    let spec = blade("benchmark-v2")
        .kind("kain_executable")
        .entry("cases_v2/.telemetryrouter/router.kn")
        .source_root("cases_v2")
        .module_root("cases_v2")
        .build_target("llvm")

    let defaults = build_defaults()
        .entry("cases_v2/.telemetryrouter/router.kn")
        .artifact_root(".kain/out/llvm")
        .cache_root(".kain/cache/build")
        .profile("debug")
        .target("llvm")

    let run = run_defaults()
        .entry("cases_v2/.telemetryrouter/router.kn")
        .target("llvm")
        .watch("cases_v2")          # Watch these dirs in dev mode
        .watch("build.kn")

    let check = build_check("check-llvm")
        .entry("cases_v2/.telemetryrouter/router.kn")
        .target("llvm")
        .axis("target", "llvm")
        .telemetry("benchmark.v2.check")
        .input("cases_v2/.telemetryrouter/router.kn")
        .input("cases_v2/classic_core.kn")
        # ... (all case input files)

    let root_exe = native_executable("root-executable")
        .entry("cases_v2/.telemetryrouter/router.kn")
        .root_output("$root/kain-benchmark-v2.exe")
        .requires("check-llvm")
        # ... (all case input files)

    let telemetry = exec_task("telemetry-v2")
        .command("powershell")                  # Run an external command
        .cwd("$root")
        .requires("root-executable")
        .input("cases_v2/.telemetryrouter/router.kn")
        # ... (all case input files)
        .arg("-NoProfile")
        .arg("-ExecutionPolicy").arg("Bypass")
        .arg("-Command").arg("& $env:KAIN_BENCH_V2_EXE")
        .env("KAIN_BENCH_V2_EXE", "$root/kain-benchmark-v2.exe")
        .env("KAIN_BENCH_V2_MARKDOWN", "$root/latest_v2.md")
        .env("KAIN_BENCH_V2_JSON", "$root/out/reports/latest_v2.json")
        .env("KAIN_BENCH_V2_TRACK_ROOT", "$root/out/reports/v2_tracks")
        .stdout("$task/router.stdout.txt")
        .stderr("$task/router.stderr.txt")
        .output("$root/latest_v2.md")
        .output("$root/out/reports/latest_v2.json")
        .timeout_ms(120000)
        .always_run()                            # Run even if cached

    let certify = certify_gate("certify-v2")
        .requires("check-llvm")
        .requires("root-executable")
        .requires("telemetry-v2")
        .certifies("benchmark.v2.local")

    return build_graph()
        .workspace(ws)
        .package(pkg)
        .blade(spec)
        .defaults(defaults)
        .run(run)
        .task(check)
        .task(root_exe)
        .task(telemetry)
        .task(certify)
```

**Key patterns**:
- `exec_task()` runs arbitrary commands (PowerShell, bash, python, etc.)
- `.env()` sets environment variables for the spawned process
- `.stdout()` / `.stderr()` redirect output to files
- `.always_run()` prevents task caching
- `.timeout_ms()` sets a timeout for the task
- `run_defaults()` configures watch mode paths
- `workspace_defaults()` sets workspace-wide defaults

---

### 9.5 GPU + C FFI Blade — `blades/c/vulkan_v2/build.kn`

A more complex project combining GPU compute shaders, C FFI, and `source_set()` glob patterns.

```kn
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let app = project("fluid-sim")
        .kind("kain_executable")
        .version("0.1.0")
        .description("GPU fluid simulator — libclang C-FFI + SPIR-V compute")
        .entry("src/main.kn")
        .source_root("src")
        .module_root("src")
        .targets("llvm")
        .artifact_root(".kain/out")
        .cache_root(".kain/cache/build")
        .profile("debug")

    let sources = source_set("fluid-sources")
        .glob("src/**/*.kn")
        .file("build.kn")
        .file("KAIN.toml")

    let check = check_task("check-llvm")
        .project(app)
        .target("llvm")
        .axis("target", "llvm")
        .inputs(sources)

    let gpu = gpu_suite("gpu-artifacts")
        .compute("src/fluid.comp.kn")             # SPIR-V compute shader
        .targets("spirv")                         # Generate SPIR-V only
        .artifact_root(".kain/out/gpu")
        .requires("check-llvm")

    let exe = native_executable("fluid-sim-exe")
        .project(app)
        .output("$blade/fluid_sim.exe")
        .requires(check)
        .requires(gpu)                            # GPU artifacts built first
        .inputs(sources)

    return build_graph(app)
        .sources(sources)
        .tasks(check, gpu, exe)
```

**Key patterns**:
- GPU artifacts are a **separate task** with its own dependency edge
- The native executable depends on GPU artifacts — ensures shaders are compiled first
- `.compute()` specifies the compute shader source file

---

### 9.6 UI Framework Blade — `blades/ui/kaintana/build.kn`

A larger library project with multiple source/module roots, certification, and capsule sets.

```kn
use std::build
use std::test
use std::proof
use std::certify

const KAINTANA_SOURCE_ROOTS = [
    "src", "src/api", "src/core",
    "src/platform/desktop", "src/platform/vulkan", "src/platform/winit",
    "examples",
]

fn build(ctx: BuildContext) -> BuildGraph:
    let pkg = project("kaintana")
        .kind("kain_library")
        .version("0.1.0")
        .description("Blade-owned Kain UI framework ...")
        .entry("src/kaintana.kn")
        .source_roots(KAINTANA_SOURCE_ROOTS)
        .module_roots(KAINTANA_SOURCE_ROOTS)
        .targets("llvm")
        .artifact_root(".kain/out")
        .cache_root(".kain/cache/build")
        .profile("debug")

    let sources = source_set("kaintana-sources")
        .glob("src/**/*.kn")
        .glob("examples/**/*.kn")
        .glob("native/**/*.{h,c}")
        .file("build.kn")
        .file("KAIN.toml")

    let defaults = build_defaults()
        .entry("src/main.kn")
        .artifact_root(".kain/out")
        .cache_root(".kain/cache/build")
        .profile("debug")
        .target("llvm")

    let run = run_defaults()
        .entry("src/main.kn")
        .target("llvm")

    let surface_check = check_task("surface-check-llvm")
        .project(pkg).entry("src/kaintana.kn").target("llvm")
        .axis("target", "llvm").telemetry("llm.surface")

    let main_check = check_task("check-llvm")
        .project(pkg).entry("src/main.kn").target("llvm")
        .axis("target", "llvm").telemetry("llm.evidence", "kaintana.main")

    let root_exe = native_executable("root-executable")
        .entry("src/main.kn")
        .root_output("$root/kaintana.exe")
        .arg("--no-verify-llvm")
        .requires("surface-check-llvm", "check-llvm")
        .inputs(sources)

    let cert = certify("kaintana.local")
        .requires(surface_check, main_check, root_exe)

    let capsule = capsule_set("kaintana")
        .after(cert)
        .source("$root/kaintana.kn")
        .tag("portable").tag("kaintana")
        .telemetry("kaintana.capsule")

    return build_graph(pkg)
        .defaults(defaults).run(run)
        .sources(sources)
        .tasks(surface_check, main_check, root_exe, cert, capsule)
```

---

## 10. build.kn DSL Reference

Every builder function in `std::build`:

### Package & Blade Builders

| Function | Returns | Purpose |
|----------|---------|---------|
| `package(name)` | `PackageSpec` | Declare a named package |
| `blade(name)` | `BladeSpec` | Declare a blade (buildable unit) |
| `project(name)` | `ProjectSpec` | Combined package+blade shorthand |
| `build_defaults()` | `BuildDefaultsSpec` | Default build settings |
| `workspace_defaults()` | `WorkspaceDefaultsSpec` | Workspace-wide settings |
| `run_defaults()` | `RunDefaultsSpec` | Run-time settings |
| `platform_package(name)` | `PlatformPackageSpec` | External platform package |
| `source_set(name)` | `SourceSetSpec` | Named source file collection |
| `build_graph()` | `BuildGraphSpec` | The root build graph |

### Task Builders

| Function | Kind | Purpose |
|----------|------|---------|
| `build_check(id)` | `check` | Typecheck task |
| `check_task(id)` | `check` | Alias for build_check |
| `native_executable(id)` | `native-executable` | Build native exe |
| `root_executable(id)` | `native-executable` | Alias (output at root) |
| `build_native_executable(id)` | `native-executable` | Alias |
| `source_tests(id)` | `test` | Run inline source tests |
| `exec_task(id)` | `exec` | Run arbitrary command |
| `command_task(id)` | `exec` | Alias for exec_task |
| `amalgamate_capsule(id)` | `amalgamate` | Pack into capsule |
| `capsule_set(id)` | `amalgamate` | Pack capsule set |
| `gpu_suite(id)` | `gpu` | Compile GPU artifacts |
| `cuda_artifacts(id)` | `exec` | Compile CUDA PTX |
| `kain_runner(id)` | `exec` | Run kain as subprocess |
| `album_mode(id)` | `exec` | Run album smoke mode |
| `certify(subject)` | `certify` | Meta-task: certify pipeline |
| `build_task(id)` | (generic) | Generic task with explicit kind |
| `build_task_of_kind(id, kind)` | (explicit) | Task with explicit kind string |

### PackageSpec Methods

```kn
package("name")
    .version("0.1.0")           # Version string
    .description("...")         # Description
```

### BladeSpec Methods

```kn
blade("name")
    .kind("kain_library")       # Blade kind
    .entry("src/main.kn")       # Entry point
    .source_root("src")         # Source root (repeatable)
    .module_root("src")         # Module root (repeatable)
    .build_target("llvm")       # Target (repeatable)
    .dependency("other-blade")  # Blade dependency
```

### ProjectSpec Methods

```kn
project("name")
    .kind("kain_executable")    # Project kind
    .version("0.1.0")           # Version
    .description("...")         # Description
    .entry("src/main.kn")       # Entry point
    .source_root("src")         # Source root (repeatable)
    .module_root("src")         # Module root (repeatable)
    .target("llvm")             # Build target (repeatable)
    .artifact_root(".kain/out") # Artifact output root
    .cache_root(".kain/cache")  # Cache root
    .profile("debug")           # Build profile
    .generated_root("gen")      # Generated files root
    .run_arg("--flag")          # Default run args
    .watch("src/")              # Watch paths (dev mode)
```

### BuildDefaultsSpec Methods

```kn
build_defaults()
    .entry("src/main.kn")       # Default entry
    .artifact_root(".kain/out") # Artifacts root
    .cache_root(".kain/cache")  # Cache root
    .profile("debug")           # Profile
    .target("llvm")             # Default target
```

### WorkspaceDefaultsSpec Methods

```kn
workspace_defaults()
    .search_root(".")           # Workspace search root
    .generated_root(".kain/gen")# Generated root
    .stdlib_root("stdlib")      # Stdlib path override
    .manifest_root(".")         # Manifest root
    .blade_root("blades/")      # Blade root (repeatable)
    .member("path")             # Workspace member (repeatable)
```

### SourceSetSpec Methods

```kn
source_set("name")
    .root("src")                # Source root (repeatable)
    .file("path/to/file.kn")    # Explicit file (repeatable)
    .glob("src/**/*.kn")        # Glob pattern (repeatable)
    .dir("path/to/dir")         # Directory (repeatable)
    .exclude("*.test.kn")       # Exclude pattern (repeatable)
```

### BuildTaskSpec Methods

**Identity & Kind**:

| Method | Purpose |
|--------|---------|
| `.kind("check")` | Override task kind |
| `.entry("src/main.kn")` | Entry file |
| `.source("src/main.kn")` | Alias for `.entry()` |
| `.path("src/main.kn")` | Alias for `.entry()` |
| `.blade("blade-name")` | Associate with blade |
| `.project(project_spec)` | Inherit from project |
| `.manifest("path.toml")` | Manifest path |

**Command (for exec tasks)**:

| Method | Purpose |
|--------|---------|
| `.command("powershell")` | Command to run |
| `.arg("--flag")` | Argument (repeatable) |
| `.cwd("$root")` | Working directory |
| `.env("KEY", "value")` | Env variable (repeatable) |
| `.stdout("$task/out.txt")` | Stdout redirect |
| `.stderr("$task/err.txt")` | Stderr redirect |

**Target & Profile**:

| Method | Purpose |
|--------|---------|
| `.target("llvm")` | Build target |
| `.profile("debug")` | Build profile |
| `.axis("key", "value")` | Matrix axis for telemetry |

**Input/Output**:

| Method | Purpose |
|--------|---------|
| `.input("src/file.kn")` | Input file (repeatable) |
| `.inputs(source_set)` | Input from source set |
| `.output("$blade/out.exe")` | Output artifact (repeatable) |
| `.root_output("$root/out.exe")` | Output at workspace root |
| `.artifact(".kain/out/...")` | Alias for `.output()` |

**Dependencies**:

| Method | Purpose |
|--------|---------|
| `.requires("task-id")` | Dependency (repeatable) |
| `.after("task-id")` | Alias for `.requires()` |
| `.depends_on("task-id")` | Alias for `.requires()` |
| `.requires_capability("gpu")` | Capability gate |
| `.when_capability("gpu")` | Alias |

**GPU-Specific**:

| Method | Purpose |
|--------|---------|
| `.fragment("src/frag.kn")` | Fragment shader source |
| `.compute("src/comp.kn")` | Compute shader source |
| `.targets("spirv", "cuda")` | GPU target formats |

**Telemetry & Metadata**:

| Method | Purpose |
|--------|---------|
| `.telemetry("channel")` | Evidence channel (repeatable) |
| `.certifies("subject")` | Certification subject |
| `.tag("portable")` | Tag (repeatable) |
| `.note("note text")` | Note (repeatable) |
| `.meta("key", "value")` | Arbitrary metadata |
| `.option("key", "value")` | Generic option |

**Execution Policy**:

| Method | Purpose |
|--------|---------|
| `.timeout_ms(30000)` | Timeout in milliseconds |
| `.always_run()` | Skip caching; always execute |
| `.mode("full")` | Mode argument for album/runner |
| `.runner("script.kn")` | Runner entry point |
| `.executable("$root/exe")` | Executable path for runner |

**Capsule-Specific**:

| Method | Purpose |
|--------|---------|
| `.name("capsule-name")` | Display name |
| `.version("0.1.0")` | Version |
| `.author("Name")` | Author (repeatable) |
| `.header("rich")` | Header rendering mode |
| `.archive(true)` | Store as compressed archive |
| `.compression("zstd")` | Compression method |
| `.capsule_set("name")` | Sidecar set name |
| `.contents("source")` | Content policy |
| `.storage("editable")` | Storage format |
| `.stem("lib")` | Output stem |

### BuildGraphSpec Methods

```kn
build_graph()
    .project(project_spec)      # Add project (creates package + blade)
    .package(package_spec)      # Add package
    .blade(blade_spec)          # Add blade
    .task(task_spec)            # Add task (repeatable)
    .tasks(task1, task2, ...)   # Add multiple tasks
    .sources(source_set)        # Register source set
    .require(platform_pkg)      # Require platform package
    .defaults(build_defaults)   # Register defaults
    .workspace(ws_defaults)     # Register workspace defaults
    .run(run_defaults)          # Register run defaults
```

---

## 11. Quick Reference

### 30-Second Summary

```
                    ┌──────────────────────────────┐
                    │  1. Write build.kn            │
                    │  2. kain build                │
                    │  3. kain run                  │
                    │  4. kain check --json          │
                    └──────────────────────────────┘
```

### Cheat Sheet

```bash
# Typecheck
kain check src/main.kn
kain check src/ --json

# Build & Run
kain build src/main.kn --target llvm
kain run src/main.kn --target llvm
kain run dev src/                          # Watch mode

# Debug
kain run src/main.kn --target llvm --debug
kain build --lane release

# Test
kain test src/
kain test src/ --json

# Format
kain fmt src/ --check
kain fmt src/ --write

# Diagnostics
kain doctor
kain config show --json

# C Import
kain import-c vendor/sdk -I vendor/include -o gen/sdk.kn
kain import platform vulkan --sdk "C:/VulkanSDK/1.3.296.0"

# GPU
kain gpu-artifacts shaders.kn --target spirv

# Distribution
kain amalgamate src/ -o project.kn --archive

# Clean
kain clean
kain clean --dry-run

# Scaffold
kain init my-project
```

### Build Task Types

| Task | Builder | Command |
|------|---------|---------|
| Check | `build_check()`, `check_task()` | `kain check` |
| Native exe | `native_executable()`, `root_executable()` | `kain build --target llvm` |
| Test | `source_tests()` | `kain test` |
| Exec | `exec_task()`, `command_task()` | `kain build` (runs command) |
| GPU | `gpu_suite()` | `kain gpu-artifacts` |
| Capsule | `amalgamate_capsule()`, `capsule_set()` | `kain amalgamate` |
| Certify | `certify()` | `kain build` (meta-task) |

### Path Prefixes

| Prefix | Resolution |
|--------|-----------|
| `$root` | Workspace root (where build.kn lives) |
| `$blade` | Blade output root |
| `$task` | Task cache root |

### Config File Locations

| File | Purpose | Authority |
|------|---------|-----------|
| `build.kn` | Build authority | **YES** — all build logic |
| `KAIN.toml` | Compatibility metadata | No — vestigial |
| `.kain/config.toml` | Local config overrides | User preferences |
| `$KAIN_HOME/config.toml` | Global config | User preferences |

### Quick Module Resolution

```
use std::io      → <stdlib>/io.kn
use math         → <cwd>/math.kn, <cwd>/src/math.kn, <module_root>/math.kn
use a::b::c      → <cwd>/a/b/c.kn, <module_root>/a/b/c.kn
use c::win32     → (C interop module)
use rust::serde  → (Rust interop module)
```

---

> **Pro tip**: When you're unsure how a build graph works, run `kain doctor` first — it tells you exactly what environment you're in, which targets are available, and where everything lives. Then run `kain run plan src/main.kn` to see exactly what the build graph will do before it does it.
