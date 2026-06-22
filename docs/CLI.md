# Kain CLI — Complete Reference

> **Synthesized from:** `research/cli/clipart1.md` (commands/flags), `clipart2.md` (targets/backends), `clipart3.md` (manifests/config)
> **Generated:** 2026-06-22
> **Scope:** Every CLI command, compilation target, manifest format, config key, output artifact, and env var

---

## Table of Contents

[Quick Reference: Every CLI Command](#quick-reference-every-cli-command)
[Global Flags](#global-flags)
3. [Core Commands](#3-core-commands)
4. [Package Commands](#4-package-commands)
5. [Import Commands](#5-import-commands)
6. [Tooling Commands](#6-tooling-commands)
7. [Runtime & Platform Commands](#7-runtime--platform-commands)
8. [Specialized Commands](#8-specialized-commands)
9. [Compilation Targets (Complete)](#9-compilation-targets-complete)
10. [Compiler Pipeline & Backends](#10-compiler-pipeline--backends)
11. [Native Build Pipeline](#11-native-build-pipeline)
12. [GPU Artifact Pipeline](#12-gpu-artifact-pipeline)
13. [Sidecar Artifacts](#13-sidecar-artifacts)
14. [Manifest Formats](#14-manifest-formats)
15. [Tooling Config (config.toml)](#15-tooling-config-configtoml)
16. [Install Layout (KAIN_HOME)](#16-install-layout-kain_home)
17. [Package System](#17-package-system)
18. [Capsule Format](#18-capsule-format)
19. [Fabric Polyglot System](#19-fabric-polyglot-system)
20. [Environment Variables](#20-environment-variables)
21. [Output File Extensions Quick Reference](#21-output-file-extensions-quick-reference)
22. [Error Handling & Exit Codes](#22-error-handling--exit-codes)

---

## Quick Reference: Every CLI Command

### Core

| Command | Aliases | What It Does |
|---------|---------|-------------|
| `kain check <input>` | `c` | Typecheck source w/o emitting artifacts. `--json`, `--pedantic`, `--audit` |
| `kain build [input]` | `b` | Build to 13+ targets: `--target llvm,wasm,rust,c,cpp,ts,js,spirv,cuda,hlsl,wgsl,hybrid,baremetal`. Multi-target with `--targets`. `--debug`, `--clean`, `--lane`, `--emit exe|sharedlib|staticlib|object`. Legacy UE5 targets also available. |
| `kain run [input]` | `r` | Compile + execute through unified pipeline. `--target auto`, `--debug`, `--dry-run` |
| `kain test <input>` | `t` | Run compiletest-style tests. `--mode check-pass/run-fail/kain-test`, `--json` |
| `kain doctor` | `d` | Show binary/build diagnostics, env wiring. `--repair <file>` |
| `kain clean [path]` | `cl` | Clean build/run/amalgamate artifacts. `--scope build|run|all` |
| `kain format <inputs...>` | `fmt`, `f` | Canonical source formatting. `--check`, `--write` |
| `kain repl` | — | Interactive REPL session |
| `kain watch <input>` | — | Watch file changes + re-run automatically |
| `kain init [path]` | `i` | Scaffold a new Kain project |

### Build Targets (`--target` / `--targets`)

| Target | CLI Alias | Produces |
|--------|-----------|----------|
| LLVM native | `llvm`, `native`, `n` | `.ll` → clang → `.exe`/ELF. Primary native backend. |
| WebAssembly | `wasm`, `w` | `.wasm` binary |
| Bare metal | `baremetal`, `kernel` | `.ll` for `x86_64-unknown-none`, no libc/OS |
| Rust | `rust`, `rs` | `.rs` source + GPU host wrappers + reflection JSON |
| C | `c` | `.c` source |
| C++ | `cpp`, `c++` | `.cpp` source |
| JavaScript | `js`, `javascript`, `j` | `.js` source |
| TypeScript | `ts`, `typescript` | `.ts` source |
| KainScript | `ks`, `kainscript` | `.ks` (JS + JSDoc types) |
| SPIR-V | `spirv`, `gpu`, `s` | `.spv` binary (canonical GPU format) |
| PTX/CUDA | `cuda`, `ptx`, `nvptx` | `.ptx` with multi-variant modules |
| HLSL | `hlsl`, `h` | `.hlsl` shader text |
| WGSL | `wgsl`, `webgpu` | `.wgsl` shader text (WebGPU) |
| Hybrid web | `hybrid`, `web` | `.hybrid` descriptor + `.wasm` + `.js` + `.ts` |
| Interpret | `run`, `interpret`, `i` | Execute in Rust interpreter, no file output |
| Test | `test`, `t` | Test harness mode, runs inline `test` blocks |

Use `kain build file.kn --target rust` or `kain build --targets llvm,wasm,rust file.kn` for multi-target. `kain build native-ui <input>` also builds native desktop apps via the Qt/Tauri backend.

#### Native Emit Modes (`--emit`)

Controls the native linker output for `--target llvm`:

| `--emit` | Windows | Linux/macOS | Description |
|----------|---------|-------------|-------------|
| `exe` | `.exe` | (no ext) | Standalone executable (default) |
| `sharedlib` | `.dll` | `.so` / `.dylib` | Shared / dynamic library |
| `staticlib` | `.lib` | `.a` | Static library |
| `object` | `.obj` | `.o` | Object file (no linking) |

```bash
kain build file.kn --emit sharedlib    # → .dll / .so / .dylib
kain build file.kn --emit staticlib    # → .lib / .a
kain build file.kn --emit object       # → .obj / .o
kain build file.kn                     # → .exe (default)
```

---

### Run Subcommands

| Command | What It Does |
|---------|-------------|
| `kain run <file>` | **Interpreted execution** — runs in the Rust interpreter. ⚠ **Experimental:** does not support `shatter`, `collapse`/`observe`/`decay`, `teleport`, `include` C FFI, or GPU `dispatch`. Use `--target llvm` for full native support. |
| `kain run <file> --target llvm` | **Compile to native + execute** — the primary run path. Full semantic stack support (LLVM IR → clang → native binary). `--debug` for DWARF, `-- <args>` for program args. |
| `kain run dev <input>` | Dev loop — watch + re-run on changes |
| `kain run plan <input>` | Print resolved run plan w/o executing. `--json` |

### Package

| Command | Aliases | What It Does |
|---------|---------|-------------|
| `kain add <package>` | — | Record capsule-backed dependency in KAIN.lock |
| `kain install <package>` | — | Install package into global Kain package store |
| `kain publish <input>` | — | Publish portable source capsule(s). `--artifacts`, `--evidence` |
| `kain amalgamate [input]` | `a` | **Capsule packager** — pack source trees into portable `.kn` files. `-o`, `--name`, `--version`, `--author`, `--tag`, `--contents source|snapshot|artifacts|evidence`, `--archive`, `--capsule-set`, `--header`, `--compression`, `--api-index`, `--module-index`, `--preview-symbols`. Sub: `inspect`, `unpack`. |

### Import

| Command | What It Does |
|---------|-------------|
| `kain import platform <pkg>` | Import native SDK (Vulkan, etc.) via libclang. `--sdk`, `--header` |
| `kain import crates [path]` | Bundle Rust workspace crates into Kain. `--blades`, `--flat` |
| `kain import-c <input>` | Import C source via libclang. `-I`, `-D` for preprocessor |
| `kain import-rust <input>` | Import Rust source (Ouroboros). `--flat`, `--fail-fast` |
| `kain import-crate <name>` | Import a Rust crate via FFI layer. `--features`, `--all-features` |
| `kain import-asm <input>` | Import legacy assembly (6502, Z80, LR35902). `--format` |
| `kain import-ts <input>` | Import TypeScript source. Requires `typescript-import` feature |

### Tooling

| Command | What It Does |
|---------|-------------|
| `kain lsp` | Language Server (stub — use kain-service-api instead) |
| `kain config show/set/init` | Config control plane. `show --json`, `set key value`, `init` |
| `kain selfhost bootstrap/phase1/phase2` | Self-host bootstrap workflows (ouroboros pipeline) |
| `kain stdlib-map` | Generate/check the stdlib symbol atlas. `--write`, `--check`, `--json` |
| `kain commands list/export/packs/help` | Inspect the command registry |

### Runtime & Platform

| Command | What It Does |
|---------|-------------|
| `kain runtime build` | Build manifest-driven native runtime bundle |
| `kain runtime validate` | Run native runtime validation lane |
| `kain native-ui dev <input>` | Launch native desktop dev loop with hot reload |
| `kain bridge serve --entry <file>` | Run resident Kain JSON-lines bridge process |
| `kain codebase inspect/run` | Trusted workspace codebase operations |

### Amalgamate (Detailed)

Kain's **capsule pipeline** — packs any number of modules into a single portable `.kn` file. Compiler resolves imports directly from capsules. No unpack, no install, no network.

| Subcommand | What It Does | Key Flags |
|-----------|-------------|-----------|
| `amalgamate <input> -o <out>` | **Pack** — create capsule | `--name`, `--version`, `--author`, `--tag`, `--contents`, `--archive`, `--capsule-set`, `--header`, `--compression`, `--api-index`, `--module-index` |
| `amalgamate inspect <capsule>` | **Inspect** — metadata + file inventory | `--json` |
| `amalgamate unpack <capsule>` | **Unpack** — extract to directory | `-o` |

```bash
kain amalgamate src/ -o mylib.kn                                    # pack
kain amalgamate . -o mylib.kn --name MyLib --tag math                # with metadata
kain amalgamate src/ -o mylib.kn --archive --compression zstd        # compressed
kain amalgamate inspect mylib.kn                                     # inspect
kain amalgamate unpack mylib.kn -o ./vendor/                          # unpack
```

### Specialized

| Command | Aliases | What It Does |
|---------|---------|-------------|
| `kain gpu-artifacts <input>` | — | Generate SPIR-V/PTX/HLSL/WGSL sidecars with reflection |
| `kain inject <inputs...>` | — | Inject Kain source into existing UE5 plugin **⚠ Legacy** |
| `kain omni init/build` | — | Omni polyglot project management |
| `kain fabric init/validate/run` | — | Multi-runtime step DAG pipeline |

### Legacy UE5 Targets (deprecated, no longer tested)

| Target | CLI Alias | Produces |
|--------|-----------|----------|
| UE5 C++ | `ue5`, `unreal`, `u` | `.h` + `.cpp` for Unreal Engine. **⚠ Legacy — not actively tested.** |
| UE5 Editor | `ue5editor`, `editor` | `.h` + `.cpp` for Slate editor UI. **⚠ Legacy — not actively tested.** |
| USF shader | `usf` | `.usf` + C++ reflection header/impl. **⚠ Legacy — not actively tested.** |

---

## Global Flags

Available **before any subcommand**:

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--config <PATH>` | `PathBuf` | None | Config path resolution: arg → `KAIN_CONFIG` → nearest `.kain/config.toml` → `KAIN_HOME/config.toml` |
| `--color auto\|always\|never` | Enum | None | Force color policy |
| `--theme <NAME>` | String | None | Theme: `plain`, `lattice`, `slate`, `graphite`, `arctic`, `sandstone` |
| `-c` / `--code <CODE>` | String | None | Inline source (`python -c` style). Conflicts with `input`. |
| `-o` / `--output <FILE>` | `PathBuf` | None | Output file path |
| `-t` / `--target <TARGET>` | String | `"wasm"` | Compile target alias |
| `-r` / `--run` | bool | false | Run immediately after compile |
| `-w` / `--watch` | bool | false | Watch + recompile on changes |
| `-g` / `--debug` | bool | false | DWARF debug metadata in LLVM IR. Available on `build`, `run` |
| `--emit-ast` | bool | false | Debug: emit AST |
| `--emit-typed` | bool | false | Debug: emit typed AST |
| `-v` / `--verbose` | bool | false | Verbose output |
| `--dry-run` | bool | false | Plan w/o executing |
| `--strict` | bool | false | Transpiler warnings → errors |

*UE5-only flags (legacy, not actively tested): `--plugin <NAME>`, `--plugins-dir <DIR>`, `--analyze` (USF shader)*

### Default Behavior (No Subcommand)

1. **File + native target** → compile + run as native script (`kn` launcher)
2. **File + other target** → compile to target
3. **Inline `-c` code** → run or compile by target
4. **Piped stdin** → run or compile by target
5. **Nothing** → REPL (`kn`), or error `"No input file provided"` (`kain`)
6. **Legacy `blades`/`equip`** → blocked with migration hint, exit 2

---

### Launcher Architecture (brief)

| Launcher | Binary | Behavior |
|----------|--------|----------|
| `kain` | `kain.exe` | Full compiler CLI — explicit commands, no menu |
| `kn` | `kn.exe` | Interpret-first — quick-start menu, REPL, stdin, native script mode |
| `blade` | `blade.exe` | Standalone blade workspace tool |

Detected by binary filename. `main_entry()` → config load → clap parse → dispatch.



## 3. Core Commands

### 3.1 `check` — Typecheck Without Emitting Artifacts

**Alias:** `kain c`

```
kain check [OPTIONS] <INPUT>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Source file/directory, `-` for stdin, or capsule path |
| `-t` / `--target` | String | `"run"` | Target profile to typecheck against |
| `--fail-fast` | bool | false | Stop after first failed file |
| `--json` | bool | false | Structured JSON to stdout (mutually exclusive with `--json-out`) |
| `--json-out <FILE>` | `PathBuf` | None | Write JSON report to file |
| `--pedantic` | bool | false | Run ALL validators including expensive/speculative ones (ETA-B) |
| `--audit` | bool | false | Check then build, report errors build caught that check missed (ETA-C) |

**JSON output** includes per-file diagnostic envelopes with: severity, kind, code, title, message, file, location, labels, notes, help, phase.

---

### 3.2 `build` — Build File, Project, or Build Authority

**Alias:** `kain b`

```
kain build [OPTIONS] [INPUT]
kain build native-ui [OPTIONS] <INPUT>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `[INPUT]` | `PathBuf` | None | Input file or project path. Omit for current project. |
| `-o` / `--output` | `PathBuf` | None | Output file path |
| `-t` / `--target` | String | None | Single target override |
| `--targets <T1,T2>` | Comma-sep | None | Multiple target override |
| `--lane <LANE>` | String | None | Build lane: `bootstrap`, `dev`, `release`, `dist`, `selfhost` |
| `--clean` | bool | false | Clean `.kain` roots before building |
| `-g` / `--debug` | bool | false | DWARF debug metadata in LLVM IR |
| `--ue5` | bool | false | Build UE5 plugin **⚠ Legacy — not actively tested** |
| `--rust` | bool | false | Build Rust target |
| `--embed` | bool | false | Embed original Kain source as comments in generated C++ |
| `--emit <MODE>` | Enum | `exe` | Output artifact type for native (LLVM) builds: `exe`, `sharedlib`, `staticlib`, `object`. Implies `--target llvm`. |

#### `build native-ui` Subcommand

```
kain build native-ui [OPTIONS] <INPUT>
```

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Kain UI source file |
| `--root <COMPONENT>` | String | None | Root component override |
| `--app-name <NAME>` | String | None | App name / Cargo package name |
| `--window-title <TITLE>` | String | None | Window title |
| `-o` / `--out <DIR>` | `PathBuf` | None | Project output directory |
| `--artifact-dir <DIR>` | `PathBuf` | None | Artifact directory inside project |
| `--bundle-only` | bool | false | Generate project but skip cargo build |
| `--clean` | bool | false | Clean generated roots |
| `--release` | bool | false | Release mode |
| `--runtime-crate <NAME>` | String | `"kain-ui-native"` | Runtime crate name |
| `--runtime-path <PATH>` | `PathBuf` | None | Path dependency (conflicts with `--runtime-version`) |
| `--runtime-version <VER>` | String | None | Published version dependency |
| `--host <HOST>` | String | `"qt"` | Desktop host: `qt` or `tauri` |
| `--tauri-bundle-id <ID>` | String | None | Tauri bundle ID |
| `--tauri-window-label <LABEL>` | String | None | Tauri window label |

---

### 3.3 `run` — Run Through Unified Pipeline

**Alias:** `kain r`

```
kain run [OPTIONS] [INPUT]
kain run dev [OPTIONS] [INPUT] [-- <ARGS>...]
kain run plan [OPTIONS] [INPUT]
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `[INPUT]` | `PathBuf` | None | Entry file, Cargo/Fabric manifest, blade root, or workspace path |
| `--target` | String | `"auto"` | Run target override |
| `-g` / `--debug` | bool | false | DWARF debug metadata in LLVM IR |
| `--json` | bool | false | Emit run report JSON to stdout |
| `--trace` | bool | false | Include trace-oriented report detail |
| `--keep-artifacts` | bool | false | Keep cached/generated run artifacts |
| `--dry-run` | bool | false | Print resolved run plan without executing |
| `-- <ARGS>...` | `Vec<String>` | None | Runtime args (after `--`) |

#### `run dev` Subcommand

Launches in `RunMode::Dev` — watches inputs and re-runs on change.

Additional flags: `--dry-run` plans without executing the first run.

#### `run plan` Subcommand

Prints the resolved run plan without executing.

Additional flags: `--json` for JSON output.

---

### 3.4 `test` — Run Kain Source Tests

**Alias:** `kain t`

```
kain test [OPTIONS] <INPUT>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Source file or directory |
| `--mode` | String | None | Override test mode: `check-pass`, `check-fail`, `run-pass`, `run-fail`, `kain-test` |
| `-t` / `--target` | String | `"run"` | Target profile for check modes |
| `--fail-fast` | bool | false | Stop after first failed case |
| `--ignored` | bool | false | Run `//@ ignore` cases instead of skipping |
| `--json` | bool | false | Structured JSON to stdout |
| `--json-out <FILE>` | `PathBuf` | None | Write JSON report to file |

Uses compiletest-style directives (`//@ check-pass`, `//@ run-fail`, etc.).

---

### 3.5 `doctor` — Binary/Diagnostics/Environment Inspection

**Alias:** `kain d`

```
kain doctor [OPTIONS]
```

| Arg/Flag | Type | Description |
|----------|------|-------------|
| `--repair <FILE>` | `PathBuf` | Repair a source file in place or dry-run |
| `--repair-tree <DIR>` | `PathBuf` | Repair every `.kn` file under a tree |
| `--repair-preview` | bool | Preview repair changes |
| `--profile <PROFILE>` | String | Repair profile: `safe`, `aggressive`, etc. |

Shows: binary version, build info, git state, sync status, runtime path, environment diagnostics.

---

### 3.6 `clean` — Clean Build Artifacts

**Alias:** `kain cl`

```
kain clean [OPTIONS] [PATH]
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `[PATH]` | `PathBuf` | `"."` | Path inside workspace |
| `--scope` | String | `"all"` | Clean scope: `build`, `run`, `amalgamate`, or `all` |
| `--dry-run` | bool | false | Print clean plan without removing |
| `--json` | bool | false | JSON output |

---

### 3.7 `format` — Canonical Source Formatting

**Alias:** `kain fmt`, `kain f`

```
kain format [OPTIONS] [INPUTS]...
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<INPUTS>...` | `Vec<PathBuf>` | Required | Files or directories. Use `-` for stdin. |
| `--check` | bool | false | Check if already formatted (conflicts with `--write`) |
| `-w` / `--write` | bool | false | Rewrite files in place (conflicts with `--check`) |

Without `--check` or `--write`, prints formatted source to stdout.

---

### 3.8 `repl` — Interactive REPL

```
kain repl
```

Launches the terminal REPL. No arguments.

---

### 3.9 `watch` — Watch & Rerun

```
kain watch [OPTIONS] [INPUT] [-- <ARGS>...]
```

Same flags as `run` (see [Section 5.3](#53-run)). Watches inputs for changes and re-runs automatically.

---

### 3.10 `init` — Initialize a New Project

**Alias:** `kain i`

```
kain init [OPTIONS] [PATH]
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `[PATH]` | `PathBuf` | `"."` | Project path |
| `--name <NAME>` | String | None | Explicit project name |

Creates a starter directory structure with a minimal `KAIN.toml` or `build.kn`.

---

## 4. Package Commands

### 4.1 `add` — Add Capsule-Backed Dependency

```
kain add [OPTIONS] <PACKAGE>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<PACKAGE>` | String | Required | Package name, local root, or source capsule path |
| `--version <VER>` | String | None | Version override |
| `--manifest <FILE>` | `PathBuf` | None | Project root or KAIN.toml to update |

Records the dependency in `KAIN.lock`.

---

### 4.2 `install` — Install Global Package

```
kain install [OPTIONS] <PACKAGE>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<PACKAGE>` | String | Required | Package name, local root, or capsule path |
| `--version <VER>` | String | None | Version override |

Installs into `{KAIN_HOME}/packages/`.

---

### 4.3 `publish` — Publish Source Capsules

```
kain publish [OPTIONS] <INPUT>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Package/project/workspace root or entry file |
| `-o` / `--output` | `PathBuf` | None | Output capsule path |
| `--name <NAME>` | String | None | Override published package name |
| `--version <VER>` | String | None | Override version |
| `--artifacts` | bool | false | Emit artifacts companion capsule |
| `--evidence` | bool | false | Emit evidence companion capsule |
| `--archive` | bool | false | Store as compressed archive |

Default output: `<input>/.kain/publish/<name>-<version>.kn`

---

### 4.4 `amalgamate` — Pack/Inspect/Unpack Capsules

**Alias:** `kain a`

```
kain amalgamate [OPTIONS] [INPUT] [-o OUTPUT]          # Pack
kain amalgamate inspect [OPTIONS] <INPUT>               # Inspect
kain amalgamate unpack [OPTIONS] <INPUT>                # Unpack
```

**Pack flags (no subcommand):**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `[INPUT]` | `PathBuf` | None | File or directory to pack |
| `-o` / `--output` | `PathBuf` | None | Output capsule path |
| `--name <NAME>` | String | None | Capsule display name |
| `--version <VER>` | String | None | Version label |
| `--author <AUTHOR>` | Repeatable | — | Author field |
| `--note <NOTE>` | Repeatable | — | Free-form notes |
| `--tag <TAG>` | Repeatable | — | Tags |
| `--meta <KEY=VALUE>` | Repeatable | — | Arbitrary metadata |
| `--contents <TYPE>` | String | `"source"` | Content policy: `source`, `snapshot`, `assets`, `artifacts`, `evidence` |
| `--capsule-set <NAME>` | String | None | Sibling capsule set name |
| `--archive` | bool | false | Compressed archive payload |
| `--header <MODE>` | String | `"rich"` | Header: `minimal`, `rich`, `off` |
| `--preview-symbols <N>` | usize | 40 | Max preview symbols in header |
| `--compression <MODE>` | String | `"zstd"` | Payload compression: `zstd` or `none` |
| `--api-index <MODE>` | String | `"auto"` | Public API index: `auto` or `off` |
| `--module-index <MODE>` | String | `"auto"` | Module index: `auto` or `off` |

**`inspect` subcommand:**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Capsule artifact path |
| `--json` | bool | false | JSON metadata output |

**`unpack` subcommand:**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Capsule artifact path |
| `-o` / `--output` | `PathBuf` | None | Output directory (default: `<capsule>.unpacked`) |

---

## 5. Import Commands

### 5.1 `import platform` — Native SDK/Platform Import

```
kain import platform [OPTIONS] <PACKAGE>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<PACKAGE>` | String | Required | Package name or SDK root path |
| `--package-name <NAME>` | String | None | Override module name when arg is a path |
| `--provider <NAME>` | String | `"system"` | Provider label for lockfile |
| `--sdk <PATH>` | `PathBuf` | None | Explicit SDK root to scan |
| `-o` / `--output <DIR>` | `PathBuf` | None | Output directory |
| `--target-triple <TRIPLE>` | String | None | Target triple to lock against |
| `--dry-run` | bool | false | Print without writing |
| `--report-json <FILE>` | `PathBuf` | None | Write lock/report JSON |
| `--registry <FILE>` | `PathBuf` | None | Registry metadata (e.g. Vulkan vk.xml) |
| `--header <FILE>` | `PathBuf` | None | Explicit C header entrypoint |

Default output: `.kain/platform/<package>/<target-triple>/`

---

### 5.2 `import crates` — Rust Crate Bundle Import

```
kain import crates [OPTIONS] [PATH]
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `[PATH]` | `PathBuf` | current dir | Workspace root |
| `--source-root <DIR>` | `PathBuf` | None | Rust source root (auto-detected) |
| `-o` / `--output` | `PathBuf` | None | Output `.kn` file or directory |
| `--blades` | bool | false | Mirror into blades-style tree |
| `-t` / `--target` | String | None | Compilation target (conflicts with `--blades`) |
| `--flat` | bool | false | Flatten to global scope |
| `--include <FILTER>` | Comma-sep | — | Include paths containing filter |
| `--exclude <FILTER>` | Comma-sep | — | Exclude paths containing filter |
| `--fail-fast` | bool | false | Stop on first failure |

---

### 5.3 `import-c` — C Source Import (libclang)

```
kain import-c [OPTIONS] <INPUT>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | C source file or directory |
| `-o` / `--output` | `PathBuf` | None | Output `.kn` file |
| `-t` / `--target` | String | None | Compile target (compile directly, no .kn) |
| `-I` / `--include-paths` | Repeatable | — | C preprocessor include paths |
| `-D` / `--defines` | Repeatable | — | Preprocessor defines |
| `--flat` | bool | false | Flatten to global scope |
| `--include <FILTER>` | Comma-sep | — | Include filter |
| `--exclude <FILTER>` | Comma-sep | — | Exclude filter |
| `--fail-fast` | bool | false | Stop on first failure |
| `--report-json <FILE>` | `PathBuf` | None | Import report JSON |

Uses libclang for parsing. Supports `include <windows.h> as win`, `include <vulkan/vulkan.h> as vk`.

---

### 5.4 `import-rust` — Rust Source Import (Ouroboros)

```
kain import-rust [OPTIONS] <INPUT>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Rust source file or directory |
| `-o` / `--output` | `PathBuf` | None | Output `.kn` file |
| `-t` / `--target` | String | None | Compile target |
| `--flat` | bool | false | Flatten to global scope |
| `--include <FILTER>` | Comma-sep | — | Include filter |
| `--exclude <FILTER>` | Comma-sep | — | Exclude filter |
| `--fail-fast` | bool | false | Stop on first failure |
| `--report-json <FILE>` | `PathBuf` | None | Import report JSON |

---

### 5.5 `import-crate` — Rust Crate FFI Import

```
kain import-crate [OPTIONS] <CRATE_NAME>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<CRATE_NAME>` | String | Required | Crate name for `use rust::<name>` |
| `--manifest-path <FILE>` | `PathBuf` | None | Cargo manifest for workspace resolution |
| `--crate-path <DIR>` | `PathBuf` | None | Local crate folder or Cargo.toml |
| `--mode <MODE>` | String | `"both"` | `live`, `generate`, or `both` |
| `-o` / `--output <DIR>` | `PathBuf` | None | Output directory |
| `--report-json <FILE>` | `PathBuf` | None | Report JSON path |
| `--features <F1,F2>` | Comma-sep | — | Cargo features |
| `--all-features` | bool | false | Enable all |
| `--no-default-features` | bool | false | Disable defaults |

---

### 5.6 `import-asm` — Legacy Assembly Import

```
kain import-asm [OPTIONS] <INPUT>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Assembly source file |
| `--format` | String | `"6502-furby"` | Dialect format |
| `--out <FILE>` | `PathBuf` | None | Output `.kn` file |
| `--validate-only` | bool | false | Parse only, skip writing |

Supports: `6502/Furby`, `LR35902/Game Boy`, `Z80/Spectrum/MSX`.

---

### 5.7 `import-ts` — TypeScript Import

```
kain import-ts [OPTIONS] <INPUT>
```

| Arg/Flag | Type | Default | Description |
|----------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | TypeScript file or directory |
| `-o` / `--output` | `PathBuf` | None | Output `.kn` file |
| `-t` / `--target` | String | None | Compile target |
| `--flat` | bool | false | Flatten to global scope |
| `--include <FILTER>` | Comma-sep | — | Include filter |
| `--exclude <FILTER>` | Comma-sep | — | Exclude filter |
| `--fail-fast` | bool | false | Stop on first failure |
| `--report-json <FILE>` | `PathBuf` | None | Import report JSON |

**Requires:** `typescript-import` feature. Falls back to error message if disabled.

---

## 6. Tooling Commands

### 6.1 `lsp` — Language Server Protocol

```
kain lsp
```

**Status:** Stub. Prints deprecation notice:
> "KAIN's historical Rust LSP is deprecated... Use kain-service-api as the compiler service layer."

---

### 6.2 `config` — Config Control Plane

```
kain config show [--json]
kain config set <KEY> <VALUE>
kain config init [--path <FILE>] [--force]
```

**`config show`** prints: path, UI settings, build parallelism, native defaults, diagnostics settings.

**`config set`** with dotted key path:

| Key | Value Type | Description |
|-----|-----------|-------------|
| `build.jobs` | `smart\|all\|half\|efficiency\|<N>` | Global parallelism |
| `build.cargo-jobs` | Same | Cargo jobs |
| `build.native-jobs` | Same | Native compilation jobs |
| `build.native-profile` | `release\|debug\|<empty>` | Native toolchain profile |
| `build.native-opt-level` | `0\|1\|2\|3\|s\|z` | Optimization level |
| `build.native-target-cpu` | `native\|default\|<cpu>` | Target CPU |
| `build.native-debug-info` | `true\|false` | Debug info |
| `ui.color` | `auto\|always\|never` | Color preference |
| `ui.theme` | `plain\|lattice\|slate\|graphite\|arctic\|sandstone` | CLI theme |
| `ui.experimental-help` | `true\|false` | Experimental help rendering |
| `diagnostics.capture` | `off\|failures` | Diagnostic capture mode |
| `diagnostics.path` | Path string | Capture output path |
| `diagnostics.store-ansi` | `true\|false` | Store ANSI in diagnostics |

**`config init`** writes a starter config file.

---

### 6.3 `selfhost` — Self-Host Bootstrap Workflows

```
kain selfhost bootstrap [OPTIONS]
kain selfhost phase1 [OPTIONS]
kain selfhost phase2 [OPTIONS]
```

**`selfhost bootstrap`:**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--manifest-path <FILE>` | `PathBuf` | None | Manifest path |
| `--backend` | String | `"llvm"` | Backend target |
| `--combine-only` | bool | false | Only combine sources |
| `--emit-llvm-only` | bool | false | Only emit LLVM IR |
| `--link-native` | bool | false | Link native executable |
| `--verify-ouroboros` | bool | false | Verify ouroboros pipeline |

**`selfhost phase1`:**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--inventory-dir <DIR>` | `PathBuf` | None | Inventory directory |
| `--output-dir <DIR>` | `PathBuf` | None | Output directory |
| `--profile-path <FILE>` | `PathBuf` | None | Profile path |
| `--emit-bundles` | bool | true | Emit bundles |
| `--all-crates` | bool | false | Process all crates |
| `--force` | bool | false | Force regeneration |

**`selfhost phase2`:**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--inventory-dir <DIR>` | `PathBuf` | None | Inventory directory |
| `--output-dir <DIR>` | `PathBuf` | None | Output directory |
| `--profile-path <FILE>` | `PathBuf` | None | Profile path |
| `--emit-bundles` | bool | true | Emit bundles |
| `--emit-roundtrip-rust` | bool | true | Emit roundtrip Rust |
| `--assemble-stage2` | bool | true | Assemble stage 2 |
| `--build-stage2` | bool | true | Build stage 2 |
| `--all-crates` | bool | false | All crates |
| `--force` | bool | false | Force regeneration |

---

### 6.4 `stdlib-map` — Stdlib Symbol Atlas

```
kain stdlib-map [OPTIONS]
```

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--repo-root <DIR>` | `PathBuf` | Auto-discovered | Repo root |
| `--stdlib-root <DIR>` | `PathBuf` | `<repo>/stdlib` | Stdlib source root |
| `--native-manifest <FILE>` | Repeatable | — | Runtime manifest to include |
| `--json-out <FILE>` | `PathBuf` | None | JSON output path |
| `--llm-out <FILE>` | `PathBuf` | None | LLM markdown output path |
| `--write` | bool | false | Rewrite checked-in generated files |
| `--check` | bool | false | Fail if generated files are stale |
| `--json` | bool | false | Print JSON instead of LLM markdown |

---

### 6.5 `commands` — Command Registry Inspection

```
kain commands list [--bin <NAME>] [--runtime] [--json]
kain commands export [--bin <NAME>] [--runtime]
kain commands packs [--json]
kain commands help [--bin <NAME>] [--runtime]
```

| Command | Purpose |
|---------|---------|
| `list` | List registry entries for a launcher view |
| `export` | Export registry metadata as JSON |
| `packs` | List command packs loaded into registry |
| `help` | Render help from dynamic Clap builder |

---

## 7. Runtime & Platform Commands

### 7.1 `runtime` — Native Runtime Build & Validate

```
kain runtime build [OPTIONS]
kain runtime validate [OPTIONS]
```

**`runtime build`:**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--release` | bool | false | Release mode |
| `--verbose` | bool | false | Verbose output |

**`runtime validate`:**

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--release` | bool | false | Release mode |
| `--verbose` | bool | false | Verbose output |
| `--skip-cli-build` | bool | false | Skip CLI build |
| `--skip-runtime-build` | bool | false | Skip runtime build |
| `--skip-fixtures` | bool | false | Skip native fixture suite |
| `--skip-conformance` | bool | false | Skip conformance suite |

---

### 7.2 `native-ui` — Native Desktop App Dev Loop

```
kain native-ui dev [OPTIONS] <INPUT>
```

Same flags as `build native-ui` (see [Section 5.2](#52-build)). Launches with watch + hot reload.

---

### 7.3 `bridge` — Resident Kain Bridge

```
kain bridge serve --entry <FILE> [--dispatch-function <NAME>]
```

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `--entry <FILE>` | `PathBuf` | Required | Entry `.kn` file |
| `--dispatch-function <NAME>` | String | `"kain_bridge_dispatch"` | Dispatch function name |

Runs a JSON-lines bridge process for programmatic Kain integration.

---

### 7.4 `codebase` — Workspace Codebase Control

```
kain codebase inspect [OPTIONS]
kain codebase run [OPTIONS]
```

Operators on the current workspace for trusted local codebase inspection and package/runtime operations.

---

## 8. Specialized Commands

### 8.1 `gpu-artifacts` — Generate GPU Shader Artifacts

```
kain gpu-artifacts [OPTIONS] <INPUT>
```

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUT>` | `PathBuf` | Required | Shader source file |
| `-o` / `--output` | `PathBuf` | None | Output base path |
| `--target` | String | `"all"` | Target: `all`, `spirv`, `cuda`, `hlsl`, `wgsl` |
| `--no-residency` | bool | false | Skip compute residency sidecars |
| `--no-derived` | bool | false | Skip derived cross-target artifacts |

Generates: `.spv`, `.gpu.rs`, `.reflect.json`, `.shader_bundle.json` + optional `.derived.hlsl`, `.derived.wgsl`, `.derived.ptx`.

---

### 8.2 `inject` — Inject Kain into Existing Plugin

```
kain inject [OPTIONS] <INPUTS>...
```

| Flag | Type | Default | Description |
|------|------|---------|-------------|
| `<INPUTS>...` | `Vec<PathBuf>` | Required | Input `.kn` files |
| `--plugin-dir <DIR>` | `PathBuf` | Auto-detect | Target plugin directory |
| `--plugin <NAME>` | String | Auto-detect | Plugin name |
| `--force` | bool | false | Force overwrite |
| `--dry-run` | bool | false | Show what would happen |
| `--ue5` | bool | false | Use UE5 codegen **⚠ Legacy** |

---

### 8.3 `omni` — Omni Polyglot Project Management

```
kain omni init [OPTIONS] [PATH]
kain omni build [OPTIONS] [PATH]
```

**`omni init`** creates an Omni project from template. **`omni build`** stages imports and compiles to all declared targets.

---

### 8.4 `fabric` — Fabric Multi-Runtime Pipeline

```
kain fabric init [OPTIONS] [PATH]
kain fabric validate [OPTIONS]
kain fabric run [OPTIONS]
```

**`fabric init`** creates a Fabric manifest from template (`local` or `polyglot`). **`fabric validate`** validates the manifest. **`fabric run`** executes the step DAG.

---

## 9. Compilation Targets (Complete)

### 9.1 CompileTarget Enum

Defined in `crates/core/src/lib.rs`. 19 variants:

```rust
pub enum CompileTarget {
    Wasm,          // WebAssembly binary
    Js,            // JavaScript
    Ts,            // TypeScript
    Hybrid,        // WASM + JS/TS hybrid bundle
    C,             // C source
    Llvm,          // LLVM IR → native executable
    Rust,          // Rust source
    Cpp,           // C++ source
    // ⚠ Legacy UE5 targets (no longer actively tested):
    // Ue5,           // UE5 C++ code
    // Ue5Editor,     // UE5 Editor Slate code
    // Usf,           // Unreal Shader Format
    Spirv,         // SPIR-V binary (canonical GPU format)
    Hlsl,          // HLSL shader text
    Wgsl,          // WGSL shader text
    Cuda,          // NVIDIA PTX
    Interpret,     // Interpreted execution
    Test,          // Test harness mode
    Ks,            // KainScript (JS + JSDoc types)
    BareMetal,     // Freestanding LLVM IR (no OS, no libc)
}
```

### 9.2 Target Spec Table

| Target | Extension | CLI Aliases | Backend Crate | Feature Gate |
|--------|-----------|-------------|---------------|-------------|
| Wasm | `.wasm` | `wasm`, `w` | `kain-wasm` via `kain-web` | `web` |
| Llvm | `.ll` | `llvm`, `native`, `n` | `kain-sys-codegen` (LLVM) | `sys` |
| BareMetal | `.ll` | `baremetal`, `bare-metal`, `bare`, `kernel` | `kain-sys-codegen` (LLVM, no-std) | `sys` |
| C | `.c` | `c` | `kain-sys-codegen` (C) | `sys` |
| Rust | `.rs` | `rust`, `rs` | `kain-sys-codegen` (Rust) | `sys` |
| Cpp | `.cpp` | `cpp`, `c++` | `kain-sys-codegen` (C++) | `sys` |
| Spirv | `.spv` | `spirv`, `gpu`, `shader`, `s` | `kain-gpu` (SPIR-V) | `gpu` |
| Hlsl | `.hlsl` | `hlsl`, `h` | `kain-gpu` → `kain-shader-text` | `gpu` |
| Wgsl | `.wgsl` | `wgsl`, `webgpu` | `kain-gpu` → `kain-shader-text` | `gpu` |
| Cuda | `.ptx` | `cuda`, `ptx`, `nvptx` | `kain-gpu` (PTX) | `gpu` |
| Js | `.js` | `js`, `javascript`, `j` | `kain-script` via `kain-web` | `web` |
| Ts | `.ts` | `ts`, `typescript` | `kain-script` via `kain-web` | `web` |
| Ks | `.ks` | `ks`, `kainscript`, `kscript` | `kain-script` via `kain-web` | `web` |
| Hybrid | `.hybrid` | `hybrid`, `web` | `kain-web` (hybrid) | `web` |
| Interpret | `.txt` (not written) | `run`, `r`, `interpret`, `i` | `kain-core` (interpreter) | always |
| Test | `.txt` (not written) | `test`, `t` | `kain-core` (test runner) | always |
| Ue5 ⚠ | `.h` + `.cpp` | `ue5`, `unreal`, `u` | `kain-ue5` (legacy) | `ue5` |
| Ue5Editor ⚠ | `.h` + `.cpp` | `ue5editor`, `ue5-editor`, `editor`, `slate` | `kain-ue5-editor` (legacy) | `ue5` |
| Usf ⚠ | `.usf` | `usf` | `kain-ue5-shaders` (legacy) | `ue5` |

> **⚠ UE5 targets are legacy** — Kain began as a UE5 codegen language. Those codegen backends are preserved in source but no longer actively tested. See [Legacy UE5 Targets](#legacy-ue5-targets-deprecated-no-longer-tested) for context.

### 9.2a Native Emit Modes

For `--target llvm`, the `--emit` flag controls the final artifact produced by the native linker:

| `--emit` | Windows | Linux | macOS | Description |
|----------|---------|-------|-------|-------------|
| `exe` (default) | `.exe` | (no ext) | (no ext) | Standalone executable |
| `sharedlib` | `.dll` | `.so` | `.dylib` | Shared library / dynamic library |
| `staticlib` | `.lib` | `.a` | `.a` | Static library |
| `object` | `.obj` | `.o` | `.o` | Object file (no linking) |

### 9.3 LLVM Target Descriptors

| Descriptor | Triple | Platform |
|------------|--------|----------|
| `LLVM_TARGET_WINDOWS_X64_MSVC` | `x86_64-pc-windows-msvc` | Windows x64 |
| `LLVM_TARGET_LINUX_X64_GNU` | `x86_64-unknown-linux-gnu` | Linux x64 |
| `LLVM_TARGET_MACOS_ARM64` | `arm64-apple-darwin` | macOS Apple Silicon |
| `LLVM_TARGET_BAREMETAL_X64` | `x86_64-unknown-none` | Freestanding x64 |

---

## 10. Compiler Pipeline & Backends

### Pipeline Stages

```
Resolve → Parse → Comptime → Typecheck → Monomorphize → Codegen → Interpret
```

| Phase | What Happens | Output |
|-------|-------------|--------|
| **Resolve** | Module resolution, stdlib loading, FFI discovery | `FrontendSourceBundle` |
| **Parse** | Lexer + Parser | AST (`Program`) |
| **Comptime** | Compile-time evaluation of `comptime` blocks | Mutated AST |
| **Typecheck** | Type checking + effect checking | `TypedProgram` |
| **Monomorphize** | Generic instantiation, memory lowering | `MonomorphizedProgram` |
| **Codegen** | Target-specific code generation | String / bytes / bundle |
| **Interpret** | Rust interpreter execution (Interpret/Test targets) | Console output |

### Backend Summary

| Backend | Entry Point | What It Generates |
|---------|------------|-------------------|
| **LLVM IR** | `kain-sys-codegen::codegen_llvm::generate()` | Textual `.ll` with DWARF support |
| **C** | `kain-sys-codegen::codegen_c::generate()` | `.c` source with smaller explicit subset |
| **C++** | `kain-sys-codegen::codegen_cpp::generate()` | `.cpp` source |
| **Rust** | `kain-sys-codegen::codegen_rust::generate()` | `.rs` library + GPU host wrappers + reflection |
| **SPIR-V** | `kain-gpu::codegen_spirv::generate()` | Binary `.spv` (canonical GPU format) |
| **PTX** | `kain-gpu::codegen_ptx::generate()` | `.ptx` text with variant modules |
| **HLSL** | `kain-shader-text` (derived from SPIR-V) | `.hlsl` text |
| **WGSL** | `kain-shader-text` (derived from SPIR-V) | `.wgsl` text |
| **WASM** | `kain-wasm::codegen_wasm::generate()` | Binary `.wasm` |
| **JS/TS/KS** | `kain-script::codegen_{js,ts,ks}::generate()` | `.js`/`.ts`/`.ks` text |
| **Hybrid** | `kain-web::generate_hybrid()` | WASM + JS + TS bundle |
| **USF ⚠ Legacy** | `kain-ue5-shaders` | `.usf` + C++ header + implementation |

---

## 11. Native Build Pipeline

For `Llvm`, `C`, and `BareMetal` targets:

```
.kn source
  ↓
1. LLVM IR / C emission
  ↓
2. LLVM IR slicing (dead function elimination at IR text level)
  ↓
3. Backend artifact written (.ll/.c)
  ↓
4. Sidecar staging (runtime_contract.json, realtime_app.json,
   shader_bundle.json, compute_residency.json)
  ↓
5. Clang compilation (.ll/.c → .o → link → .exe/executable)
  ↓
6. GPU runtime DLL staged if needed
```

### LLVM IR Slicing

Controlled by `KAIN_NATIVE_LLVM_IR_SLICING` (default: `true`).

Algorithm:
1. Parse all `define`/`declare` functions in LLVM IR
2. Start reachability from `main` + top-level references
3. Transitively follow `call`/`invoke` targets
4. Keep only reachable functions + required declarations

### Native Toolchain Tuning Profiles

| Profile | Opt Level | Target CPU | Debug Info | Link GC |
|---------|-----------|------------|------------|---------|
| Debug | `-O0` | Host default | `-g` | No |
| Release | `-O2` | Host default | `-g0` | Yes |
| BenchmarkRelease | `-O3` | `native` | `-g0` | Yes |

### Native Runtime Elision

If no reachable functions make external calls, the runtime library can be elided entirely. Controlled by `KAIN_NATIVE_RUNTIME_ELISION` (default: `true`).

Runtime lib search order:
1. `KAIN_RUNTIME_LIB_PATH` env var
2. `~/.kain/lib/libkain_runtime.a` (or `kain_runtime.lib` on Windows)
3. `{KAIN_HOME}/lib/`
4. Compile runtime from source if none found

### Native Executable Cache

Cache key = SHA-256(fingerprint + source). Controlled by `KAIN_NATIVE_EXEC_CACHE` (default: `true`).

Cache directory: `{cache_root}/kain/native-exec/{key}/` containing:
- `fingerprint.txt`, `source.kn`, `artifact.{ll,c}`, executable, `sidecars/`

---

## 12. GPU Artifact Pipeline

For GPU shader source via `kain gpu-artifacts`:

```
.kn shader source
  ↓
1. Type check
  ↓
2. Primary codegen (SPIR-V binary or PTX modules)
  ↓
3. Rust GPU host wrappers (.gpu.rs)
4. GPU reflection JSON (.reflect.json)
5. Shader artifact bundle (.shader_bundle.json)
  ↓
6. Derived outputs (optional): HLSL, WGSL, PTX variant modules
  ↓
7. Compute residency sidecars (optional)
```

### Target Filtering

| Target | Outputs |
|--------|---------|
| `all` | `.spv`, `.gpu.rs`, `.reflect.json`, `.shader_bundle.json`, `.derived.hlsl`, `.derived.wgsl`, `.derived.ptx` |
| `spirv` | `.spv`, `.gpu.rs`, `.reflect.json`, `.shader_bundle.json` (+ derived unless `--no-derived`) |
| `cuda` | `.gpu.rs`, `.reflect.json`, `.shader_bundle.json`, `.derived.ptx` |
| `hlsl` | `.spv`, `.gpu.rs`, `.reflect.json`, `.shader_bundle.json`, `.derived.hlsl` |
| `wgsl` | `.spv`, `.gpu.rs`, `.reflect.json`, `.shader_bundle.json`, `.derived.wgsl` |

---

## 13. Sidecar Artifacts

For native and GPU builds, the compiler generates sidecar JSON artifacts.

### Runtime Contract Bundle (`*.runtime_contract.json`)

Contains: version record, platform compatibility, runtime reflection, service/asset bindings, world contracts.

### Realtime App Bundle (`*.realtime_app.json`)

Contains: UI component tree, world selections, surface definitions, shader bundle refs, compute residency, window config, resource bindings.

Used at runtime for hot-reloading, world configuration, GPU resource binding.

### Shader Bundle (`*.shader_bundle.json` or `kain_shader_bundle.json`)

Contains: full shader artifact metadata — entry points, reflection summaries, resource layouts, derived outputs, specialization constants, source maps.

### Compute Residency Bundle (`kain_compute_residency.json`)

Contains: per-shader compute info — workgroup/dispatch sizes, shared memory, CUDA stream/graph policy, PTX variant modules.

### Other Sidecars

| File Name | Content |
|-----------|---------|
| `native_app_bundle.json` | Full native app bundle |
| `kain_runtime_compatibility.json` | Platform compatibility |
| `kain_runtime_version.json` | Runtime version metadata |
| `kain_reflection_payload.json` | Runtime reflection payload |
| `kain_c_host_bridges.json` | C FFI host bridge manifest |
| `app_manifest.json` | App manifest |
| `runtime_snapshot.json` | Runtime snapshot |

---

## 14. Manifest Formats

Kain has **6 manifest formats** plus a config file:

| Format | File Name | Format | Purpose | Crate |
|--------|-----------|--------|---------|-------|
| Project/Package | `KAIN.toml` / `kain.toml` | TOML | Package metadata, workspace, build config | `crates/blades` |
| Build Authority | `build.kn` | Kain source | Declarative build DAG with typed tasks | (scan by blade scanner) |
| Platform Reqs | `platform.kn` | Kain source | Platform capability requirements | (scan by blade scanner) |
| Omni Project | `KAIN.omni.toml` | TOML | Polyglot compile targets | `crates/omni` |
| Fabric Manifest | `KAIN.fabric.toml` | TOML | Multi-runtime step DAG | `crates/omni` (fabric) |
| Package Lock | `KAIN.lock` | TOML | Pinned dependency versions | `crates/blades` |
| Tooling Config | `config.toml` (in `.kain/`) | TOML | User/workspace settings | `crates/core` |

### Manifest Discovery

A directory is recognized as a workspace marker if it contains any of:
- `KAIN.toml` or `kain.toml`
- `build.kn` or `platform.kn`
- `Cargo.toml`
- `.git`

### 14.1 KAIN.toml — Project/Package Manifest

```toml
[package]
name = "my-package"
version = "0.1.0"
description = "..."

[workspace]
blades = ["blades/*", "apps/*", "crates/*"]

[build]
entry = "src/main.kn"
targets = ["llvm", "wasm"]
artifact_root = ".kain/out"

[run]
entry = "src/main.kn"
target = "llvm"

[blade]
name = "my-blade"
kind = "kain_app"
dependencies = [{name = "kaintana", version = "0.3.0"}]
build_targets = ["llvm"]

[c_ffi]
include_paths = ["vendor/include"]
link_libs = ["user32", "gdi32"]

[rust_ffi]
manifest_path = "Cargo.toml"

[manifests]
fabric = "KAIN.fabric.toml"
```

#### Section Reference

**`[package]`**: `name`, `version`, `description`

**`[workspace]`**: `blades` (glob patterns), `members` (explicit paths), `search_roots`, `stdlib_root`, `generated_root`

**`[build]`**: `entry`, `source_root`, `targets`, `artifact_root`, `cache_root`, `profile`, `tasks` (deprecated)

**`[run]`**: `entry`, `blade`, `target`, `args`, `env`, `cwd`, `watch`

**`[blade]`**: `name`, `version`, `kind` (inferred: kain/kain_library/kain_app/rust_crate/c_ffi/fabric/mixed), `entry`, `source_roots`, `dependencies`, `cargo_manifest`, `fabric_manifest`, `gpu`, `rust`, `c_ffi`, `fabric`

**`[c_ffi]`**: `include_paths`, `defines`, `link_libs`, `libraries[]` (each with: `name`, `header`, `sources`, `shared_lib`, `symbols`)

**`[rust_ffi]`**: `manifest_path`, `path_crates[]`, `registry_crates[]`

#### Workspace Inference

Blade kind is inferred when not explicitly set:

| Has | Inferred Kind |
|-----|---------------|
| `Cargo.toml` + entry | `"mixed"` |
| `Cargo.toml` only | `"rust_crate"` |
| C FFI libraries | `"c_ffi"` |
| Fabric manifest | `"fabric"` |
| Default | `"kain"` |

### 14.2 build.kn — Build Authority (Kain Source)

Text-scanned by the blade scanner. Recognizes:

```kain
build_task(name: "check-main", kind: "check", entry: "src/main.kn")
native_executable(name: "app", entry: "src/main.kn", targets: ["llvm"])
test_suite(name: "unit", kind: "kain-test", entry: "tests/unit.kn")
proof_obligation(name: "safety", entry: "proofs/safety.kn")
certify_gate(name: "release", depends_on: ["check-main", "test-unit"])
```

Task kinds: `build`, `check`, `exec`, `native_executable`, `amalgamate`, `test_suite`, `proof_obligation`, `benchmark`, `attrition`, `certify_gate`

### 14.3 KAIN.omni.toml — Omni Project Manifest

```toml
[workspace]
root = "."
search_roots = ["src"]

[build]
entry = "src/main.kn"
output_dir = "omni_out"
inline_kain_imports = true

[[build.targets]]
kind = "Rust"
output = "gen/rust/"

[[build.targets]]
kind = "TypeScript"
output = "gen/ts/"

[[imports]]
kind = "Rust"
source = "./local_crate"
output = "src/imports/local_crate.kn"
```

Target kinds: `Rust`, `JavaScript`, `TypeScript`, `C++`, `HLSL`, `SPIR-V`, `GpuArtifacts`, `RustBundle`
> **UE5** and **USF** targets also available but legacy (no longer actively tested).

### 14.4 KAIN.fabric.toml — Fabric Polyglot Manifest

```toml
version = 1

[workspace]
root = "."

[[requires]]
key = "runtime.python"
version = 1
optional = false

[[steps]]
id = "process"
runtime = "python"
entry = "scripts/process.py"
depends_on = []
outputs = [{name = "result", kind = "value"}]

[[steps]]
id = "render"
runtime = "kain"
blade = "app"
depends_on = ["process"]
outputs = [{name = "image", kind = "shared_image"}]

[reports]
directory = "fabric_reports"
emit_jsonl_events = true
```

Runtimes: `kain`, `python`, `rust_crate`, `c_abi`, `node`, `gpu_compute`
Contracts: `value`, `shared_buffer`, `shared_image`, `compute_plan`

---

## 15. Tooling Config (config.toml)

### Config Load Order

1. `--config <PATH>` CLI flag
2. `KAIN_CONFIG` environment variable
3. `{KAIN_HOME}/config.toml`
4. `{cwd}/.kain/config.toml` (walking up from CWD)
5. Defaults (no file)

### Schema

```toml
schema = 1

[build]
jobs = "smart"                  # smart, all, half, efficiency, or integer
# cargo_jobs = "smart"
# native_jobs = "smart"
# native_profile = "release"
# native_opt_level = "2"
# native_target_cpu = "native"
# native_debug_info = false

[ui]
color = "auto"                  # auto, always, never
theme = "slate"                 # plain, lattice, slate, graphite, arctic, sandstone
experimental_help = false

[diagnostics]
capture = "failures"            # off, failures
path = ".kain/diagnostics/errors.jsonl"
store_ansi = false
```

### Parallelism Settings

| Preset | Behavior |
|--------|----------|
| `smart` / `balanced` | `available - 1` (min 1) |
| `all` / `max` / `full` | `available` |
| `half` | `available / 2` (min 1) |
| `efficiency` / `eco` | `available / 3` (min 1) |

### Theme Aliases

| Input | Resolved To |
|-------|-------------|
| `"slate"` | `"slate"` (default) |
| `"lattice"` | `"lattice"` |
| `"hyperpop"` | `"slate"` |
| `"oxide"` | `"graphite"` |
| `"glacier"` | `"arctic"` |
| `"ember"` | `"sandstone"` |

---

## 16. Install Layout (KAIN_HOME)

### Resolution Order

1. `KAIN_HOME` env var
2. Nearest `.kain/` ancestor (checks for `config.toml` or `install_manifest.json`)
3. `$HOME/.kain/` (Unix) / `$USERPROFILE/.kain/` (Windows)

### Layout

```
{KAIN_HOME}/
├── bin/                        # Executables (kain.exe, kn.exe)
├── lib/                        # Libraries (kain_runtime.lib, libkain_runtime.a)
├── stdlib/                     # Standard library source
├── runtime/                    # Runtime C source
├── toolchain/                  # Toolchain
│   └── llvm/bin/               # Bundled LLVM (clang.exe, libclang.dll)
├── packages/                   # Installed package store
│   └── {name}/
│       ├── package-index.json
│       └── versions/{version}/workspace/
├── .kain/                      # Workspace-local artifacts
│   ├── out/                    # Build output artifacts
│   ├── cache/                  # Build caches
│   │   ├── build/
│   │   ├── native-exec/
│   │   └── amalgamate/
│   ├── reports/                # Build/test reports
│   ├── platform/{pkg}/{triple} # Platform SDK imports
│   └── publish/                # Published capsules
```

---

## 17. Package System

### `kain add` Flow

1. Resolves the package name/version
2. If a capsule path or local root, materializes it
3. Creates/updates `KAIN.lock` with pinned version
4. Stores workspace in `{KAIN_HOME}/packages/{name}/versions/{version}/workspace/`

### `kain publish` Flow

1. Scans workspace for source files
2. Creates a capsule with metadata (name, version, authors)
3. Optionally creates companion capsules for artifacts and evidence
4. Writes to `.kain/publish/<name>-<version>.kn`

### Package Store

```
{KAIN_HOME}/packages/
└── {package-name}/
    ├── package-index.json      # { "versions": ["0.1.0", "0.2.0"], "latest": "0.2.0" }
    └── versions/
        └── {version}/
            ├── workspace/      # Unpacked workspace files
            └── package-install.json  # Install metadata
```

---

## 18. Capsule Format

Kain capsules are **self-describing `.kn` files** embedding file archives.

### Storage Modes

| Mode | Stored As | Use Case |
|------|-----------|----------|
| **Editable** | Inline `//!kain-file` blocks | Version control, diffing |
| **Archive** | Base64-encoded compressed JSON | Distribution |

### File Structure

```
//!kain-capsule ----------------------------------------------------------
//! name:       my-package
//! version:    0.1.0
//! kind:       blade
//! storage:    editable
//! contents:   source
//! digest:     sha256:abc...
//! structure:  15 files | 3 modules
//!
//! -- PUBLIC INTERFACE --
//! [math]  pub fn add(a: Int, b: Int) -> Int
//! -----------------------------------------------------------------------

//!kain-capsule
// schema = 2
// kind = "blade"
// storage = "editable"
// contents = "source"
// digest = "sha256:abc..."
// ... TOML metadata ...
//!end-kain-capsule

//!kain-file
// path = "src/main.kn"
// kind = "text"
// bytes = 42
// sha256 = "sha256:def..."
//!kain-file-content
fn main() -> Int:
    return 42
//!end-kain-file
```

### Sentinels

| Sentinel | Purpose |
|----------|---------|
| `//!kain-capsule` | Start of metadata section / header |
| `//!end-kain-capsule` | End of metadata section |
| `//!kain-capsule-payload` | Start of archive payload (base64) |
| `//!end-kain-capsule-payload` | End of archive payload |
| `//!kain-file` | Start of editable file metadata |
| `//!kain-file-content` | Start of file content |
| `//!end-kain-file` | End of file block |

### Capsule Contents Classification

| Policy | What's Included |
|--------|----------------|
| `source` | `.kn`, `.c`, `.h`, manifests, shaders |
| `snapshot` | Everything (default) |
| `assets` | Binary non-source (images, fonts, audio, 3D) |
| `artifacts` | Build outputs (`.exe`, `.dll`, `.spv`, `.ll`, etc.) |
| `evidence` | Test/proof/benchmark outputs |

---

## 19. Fabric Polyglot System

The Fabric system executes a **multi-runtime step DAG** with typed contract bindings.

### Execution Lifecycle

1. **Validate** — Load and validate manifest
2. **Resolve** — Resolve entries and manifests from workspace
3. **Plan** — Topological sort of steps (respects `depends_on`)
4. **Execute** — Run each step, passing outputs as inputs to dependents
5. **Report** — Write execution report + optional JSONL event stream

### Contract Kinds

| Kind | Description | Snapshot Fields |
|------|-------------|-----------------|
| `value` | Scalar/string | `summary`, `json` |
| `shared_buffer` | Binary buffer | `byte_length`, `element_type`, `shape`, `format`, `ownership` |
| `shared_image` | Image buffer | `width`, `height`, `channels`, `pixel_format`, `color_space` |
| `compute_plan` | GPU dispatch | `compute_key`, `dispatch_invocations`, `tensor_binding_count` |

---

## 20. Environment Variables

### Install Layout & Paths

| Variable | Purpose |
|----------|---------|
| `KAIN_HOME` | Override home directory (default: `$HOME/.kain/`) |
| `KAIN_CONFIG` | Override tooling config path |
| `KAIN_STDLIB_PATH` | Override stdlib search path |
| `KAIN_RUNTIME_C_PATH` | Override runtime C source path |
| `KAIN_RUNTIME_MANIFEST_PATH` | Override runtime TOML manifest path |
| `KAIN_RUNTIME_LIB_PATH` | Override runtime library path |
| `KAIN_CLANG_PATH` | Override clang compiler path |
| `KAIN_REPO_ROOT` | Set repo root for `.kain` ancestor discovery |

### Native Build Tuning

| Variable | Default | Description |
|----------|---------|-------------|
| `KAIN_NATIVE_PROFILE` | — | `debug`, `release`, or `benchmark-release` |
| `KAIN_NATIVE_OPT_LEVEL` | — | `0`, `1`, `2`, `3`, `s`, `z` |
| `KAIN_NATIVE_TARGET_CPU` | — | `native` or `<cpu>` string |
| `KAIN_NATIVE_DEBUG_INFO` | — | `true`/`false` |

### Native Build Caching

| Variable | Default | Description |
|----------|---------|-------------|
| `KAIN_NATIVE_EXEC_CACHE` | `true` | Enable native executable cache |
| `KAIN_NATIVE_EXEC_CACHE_DIR` | — | Override cache directory |
| `KAIN_NATIVE_RUNTIME_ELISION` | `true` | Enable runtime elision |
| `KAIN_NATIVE_LLVM_IR_SLICING` | `true` | Enable IR dead function elimination |

### Diagnostics

| Variable | Description |
|----------|-------------|
| `KAIN_DIAG_CAPTURE` | Override capture mode (`off` or `failures`) |
| `KAIN_DIAG_CAPTURE_PATH` | Override capture path |
| `KAIN_DIAG_CAPTURE_ANSI` | Override ANSI storage |

### Banner & UI

| Variable | Effect |
|----------|--------|
| `KAIN_NO_BANNER` | Suppress CLI banner |
| `KAIN_ENGINE_NO_BANNER` | Also suppress banner |
| `NO_COLOR` | Disable color (when mode is `auto`) |
| `CLICOLOR_FORCE` | Force color output |

### GPU Runtime

| Variable | Effect |
|----------|--------|
| `KAIN_GPU_RUNTIME_LIBRARY` | GPU runtime DLL path |
| `KAIN_GPU_RUNTIME_ALLOW_CARGO_BUILD` | Allow cargo build for GPU runtime |

### Engine Module (build.rs)

| Variable | Description |
|----------|-------------|
| `KAIN_ENGINE_MODULE_DIR` | Output directory |
| `KAIN_ENGINE_MODULE_NAME` | Module name (default: `"engine"`) |
| `KAIN_ENGINE_MODULE_FILE` | Module file name |
| `KAIN_ENGINE_IMPORT_SHIM_FILE` | Import shim file name |

### Sync

| Variable | Effect |
|----------|--------|
| `KAIN_SYNC_ROOT` | Sync state root |
| `KAIN_SYNC_STAMP_PATH` | Override sync stamp JSON |

---

## 21. Output File Extensions Quick Reference

### Primary Compilation Outputs

| Extension | Target | Format |
|-----------|--------|--------|
| `.ll` | Llvm, BareMetal | LLVM IR text |
| `.c` | C | C source text |
| `.cpp` | Cpp, Ue5, Ue5Editor | C++ source text |
| `.h` | Ue5, Ue5Editor, Usf | C++ header text |
| `.rs` | Rust | Rust source text |
| `.js` | Js, Hybrid | JavaScript text |
| `.ts` | Ts, Hybrid | TypeScript text |
| `.ks` | Ks | KainScript text |
| `.wasm` | Wasm, Hybrid | WebAssembly binary |
| `.spv` | Spirv | SPIR-V binary |
| `.ptx` | Cuda | PTX assembly text |
| `.hlsl` | Hlsl | HLSL shader text |
| `.wgsl` | Wgsl | WGSL shader text |
| `.usf` | Usf | Unreal Shader Format text |
| `.hybrid` | Hybrid | JSON descriptor |
| `.exe` | Llvm/C/BareMetal (Win) | PE executable |
| (no ext) | Llvm/C/BareMetal (Unix) | ELF/Mach-O executable |

### GPU Artifact Outputs

| Extension | Content |
|-----------|---------|
| `.gpu.rs` | Rust GPU host wrapper |
| `.reflect.json` | GPU reflection metadata |
| `.shader_bundle.json` | Full shader artifact bundle |
| `.derived.hlsl` | Derived HLSL |
| `.derived.wgsl` | Derived WGSL |
| `.derived.ptx` | Derived PTX primary |
| `.derived.{arch}.ptx` | PTX variant (e.g. `.sm_80.ptx`) |

### Native Build Sidecars

| Name | Content |
|------|---------|
| `*.runtime_contract.json` | Runtime contract bundle |
| `*.realtime_app.json` | Realtime app bundle |
| `kain_shader_bundle.json` | Shader artifact bundle |
| `kain_compute_residency.json` | Compute residency |
| `native_app_bundle.json` | Full native app bundle |
| `kain_runtime_compatibility.json` | Platform compatibility |
| `kain_runtime_version.json` | Runtime version metadata |
| `kain_reflection_payload.json` | Runtime reflection |
| `kain_c_host_bridges.json` | C FFI host bridge manifest |
| `app_manifest.json` | App manifest |
| `runtime_snapshot.json` | Runtime snapshot |

### Capsule

| Sentinel | Content |
|----------|---------|
| `//!kain-capsule` ... `//!end-kain-capsule` | Self-describing `.kn` capsule |

---

## 22. Error Handling & Exit Codes

| Condition | Exit Code |
|-----------|-----------|
| Success | 0 |
| Config failure | 1 |
| Command execution failure | 1 |
| Legacy removed command (`blades`, `equip`) | 2 |

### Diagnostic Capture

When `[diagnostics].capture = "failures"`, compiler errors are appended to a JSONL file with:

- Event kind, command, argv, cwd
- Launcher, target, source name/path
- Rendered output + structured diagnostic JSON
- Phase: `lexer`, `parser`, `type`, `effect`, `borrow`, `codegen`, `runtime`, `io`, `enhanced`, `rich`, `multi`
- Tags + context

First capture triggers a one-time notification: "Diagnostic capture active — events written to: {path}"

---

## Appendix: Complete Command Index

| Command | Subcommands | Aliases | Category |
|---------|-------------|---------|----------|
| `check` | — | `c` | Core |
| `build` | `native-ui` | `b` | Core |
| `run` | `dev`, `plan` | `r` | Core |
| `test` | — | `t` | Core |
| `doctor` | — | `d` | Core |
| `clean` | — | `cl` | Core |
| `format` | — | `fmt`, `f` | Core |
| `repl` | — | — | Core |
| `watch` | — | — | Core |
| `init` | — | `i` | Core |
| `add` | — | — | Package |
| `install` | — | — | Package |
| `publish` | — | — | Package |
| `amalgamate` | `inspect`, `unpack` | `a` | Package |
| `import` | `platform`, `crates` | — | Import |
| `import-c` | — | — | Import |
| `import-rust` | — | — | Import |
| `import-crate` | — | — | Import |
| `import-asm` | — | — | Import |
| `import-ts` | — | — | Import |
| `lsp` | — | — | Tooling |
| `config` | `show`, `set`, `init` | — | Tooling |
| `selfhost` | `bootstrap`, `phase1`, `phase2` | — | Tooling |
| `stdlib-map` | — | — | Tooling |
| `commands` | `list`, `export`, `packs`, `help` | — | Tooling |
| `runtime` | `build`, `validate` | — | Runtime |
| `native-ui` | `dev` | — | Runtime |
| `bridge` | `serve` | — | Runtime |
| `codebase` | `inspect`, `run` | — | Runtime |
| `gpu-artifacts` | — | — | Specialized |
| `inject` | — | — | Specialized |
| `omni` | `init`, `build` | — | Specialized |
| `fabric` | `init`, `validate`, `run` | — | Specialized |
| *(external)* | — | — | Fallback |

---

*End of document. Generated 2026-06-22 from source code analysis of `crates/cli/`, `crates/commands/`, `crates/driver/`, `crates/blades/`, `crates/omni/`, `crates/amalgamate/`, `crates/core/`, and related crates.*
