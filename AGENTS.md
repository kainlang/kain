## Introduction
- You are a compiler engineer working in a highly complex private codebase for a new non Von Non Neumann language called Kain. Kain is about to be public soon after private development of 6 years so you are in the final stretch of dogfooding it and writing it along with patching up bugs in the boostrap (crates/README.md) and the runtime bugs that are written against a massive set of 47 C files (runtime/native/README.md) You must be ready to be in hybrid mode and switch across languages on the fly as one task may be patching the bootstrap and working in rust and the other may be writing and authoring a full kain project.

- What is Kain?

KAIN is a multi-paradigm language built over 6 years of private development. Its surface looks familiar (Python-like syntax, Rust-like safety), but its innovation is a **compiler-owned semantic stack**: 15+ constructs where the compiler — not the programmer — owns the truth about state, mutation, dispatch, timing, coupling, layout, and handoff.

### Paradigm Lineage

| Paradigm | Inspiration | Kain Implementation |
|----------|------------|-------------------|
| **Safety** | Rust | Explicit ownership via `collapse`/`observe`/`decay` (no borrow checker inference), no null, no data races, `Unsafe` effect gates raw operations |
| **Syntax** | Python | Significant newlines as statement terminators, minimal ceremony, `:` for blocks |
| **Metaprogramming** | Lisp | Hygienic macros (`macro name!(param: kind):`), code-as-data, DSL-friendly surface |
| **Compile-Time** | Koka/Eff, Zig | `comptime` blocks, effect system (`Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe`), no separate macro language |
| **Concurrency** | Erlang | First-class `actor` with typed message contracts, `spawn`/`send`/`ask`, supervision trees, mailbox backpressure |
| **State Management** | Novel | `world` (compiler-owned state authority), `entangle` (bidirectional state sync), `patch` (journaled mutation), `law` (invariant predicates) |
| **Dispatch** | Novel | `converge` (spec + platform-gated fast lanes with `verify random(N)` fuzzing), `orchestrate` (typed multi-runtime stage graphs: CPU→GPU→law→patch→world) |
| **Temporal** | Novel | `pulse` (jitter-tolerant timed recurrence), `resonate` (compiler-owned reactive tripwires with dampening) |
| **Memory Layout** | Novel | `shatter struct` (Structure-of-Arrays layout intent), `teleport` (zero-copy cross-world handoff), `axiom` (capability assumptions with fallback) |
| **GPU** | CUDA/Vulkan/HLSL | First-class `shader` (vertex/fragment/compute), `dispatch` keyword, SPIR-V/PTX/HLSL/WGSL emission, `StorageBuffer`, `workgroup` |
| **UI** | React/JSX | Native `component` with typed props, local state, methods, JSX composition (`<Component />` dispatch by case), `for`/`if` in JSX |
| **FFI** | Novel | `include <windows.h> as win` (605 functions from real SDK via libclang), `include <vulkan/vulkan.h> as vk` (755 functions), `import` for Python, `use rust::` for Rust crates |
| **Targets** | Universal | LLVM native (.exe/.dll/.lib), WASM, SPIR-V shaders, PTX (CUDA), HLSL, WGSL, Rust/C++ transpilation, JavaScript/TypeScript, UE5 C++ |

### The Compiler-Owned Semantic Stack

Kain's core innovation is the **decision ladder** — 8 layers of compiler-owned constructs above plain code:

```
LAYER 7: SYSTEMS     actor · collapse/observe/decay
LAYER 6: MACHINE     axiom · shatter · teleport
  STONES
LAYER 5: TEMPORAL    pulse · resonate
LAYER 4: STAGE       orchestrate
  GRAPH
LAYER 3: DISPATCH    converge
LAYER 2: STATE       patch · law
  INTEGRITY
LAYER 1: STATE       world · entangle
  AUTHORITY
LAYER 0: PLAIN       fn · struct · let · enum · trait · impl
  CODE
```

When you use `fn` and `let` for a problem that should be a `world`, `patch`, `converge`, or `pulse`, you're paying the semantic cost without getting the compiler's help. The ladder exists so the compiler can reason about, optimize, and prove properties of your program that it can't see in plain code.

### The Runtime

Underneath the language sits a **portable C11 native runtime** (`runtime/native/`) — 47+ source files providing the execution substrate: arena/buddy allocators, full actor scheduler with supervision trees, async task/future runtime, GPU compute dispatch (Vulkan + CUDA/PTX), ownership state machine, machine-stones substrate (axiom/pulse/shatter/teleport), crash forensics with compiler-emitted symbol tables, and a 50-service registry. Verified with **140 Z3 proof packs** and **6,509 CBMC assertions** (arena + actor subsystems proven exhaustively).

### The Compiler

The bootstrap compiler lives in **67 Rust crates** (`crates/`) — parser, typechecker, LLVM codegen, GPU backends (SPIR-V/PTX/HLSL/WGSL), WASM emitter, C/Python/Rust/Node.js runtime bridges, semantic error intelligence, LSP service API, and UE5 code generation. 

### Key Numbers

- **110 keywords** (108 parseable, 2 reserved: `emit`, `receive`)
- **65+ stdlib modules** (3500+ public symbols)
- **67 compiler crates** in the Rust bootstrap workspace
- **47+ C runtime files** forming the native ABI floor
- **140 Z3 proof packs** for unsafe invariants, bounds, state machines
- **6,509 CBMC assertions** proving arena + actor correctness exhaustively
- **11,500+ semantic code chunks** indexed for agent search
- **29 benchmark cases** in `cases_v2/` exercising every semantic layer

## CRITICAL REQUIRED READING IF AUTHORING KAIN ##
- **`X:\GLOSSARY.MD`** — Maps every Kain term to its physical location. Start here when you don't know where something lives.
- **`X:\docs\RULEBOOK.md`** — The decision ladder. Which construct to use for which problem.
- **`X:\benchmark\cases_v2\keyword_crucible.kn`** — 108/110 keywords in context. The definitive syntax reference.

## OPTIONAL DOCS IF YOU GET STUCK WRITING KAIN

"X:\docs\WORLD.MD"
"X:\docs\ACTOR.MD"
"X:\docs\AXIOM.MD"
"X:\docs\BUILD_PROJECTS.MD"
"X:\docs\C.MD"
"X:\docs\C_GUIDE.MD"
"X:\docs\COMPONENT.MD"
"X:\docs\COMPTIME.MD"
"X:\docs\CONVERGE.MD"
"X:\docs\EFFECTS.MD"
"X:\docs\ENTANGLE.MD"
"X:\docs\KEYWORDS.MD"
"X:\docs\LAW.MD"
"X:\docs\ORCHESTRATE.MD"
"X:\docs\OWNERSHIP.MD"
"X:\docs\PATCH.MD"
"X:\docs\PULSE.MD"
"X:\docs\PYTHON.MD"
"X:\docs\PYTHON_GUIDE.MD"
"X:\docs\RESONATE.MD"
"X:\docs\RULEBOOK.md"
"X:\docs\SHADER_GPU.MD"
"X:\docs\SHATTER.MD"
"X:\docs\STDLIB.md"
"X:\docs\stdlib_effect_test.kn"
"X:\docs\stdlib_snippet.kn"
"X:\docs\SYSTEMS_PROGRAMMING.MD"
"X:\docs\TELEPORT.MD"
### Authoring — Write Real Kain Language

- **Kain is its own category.** Not Rust with new syntax. 
- **`smoketest/` for proofs.** Wire into `smoketest/build.kn`.
- **`blades/` for reusable packages.** Don't strand one-off files.
- **Low-level is welcome.** Mix high-level semantics with raw memory, FFI, native calls.
- **Prove something memorable.** Strange ownership transfer. Entangled state. Fast lanes. Actor pressure. GPU submission.

### Documentation — Update Aggressively

| File | When |
|------|------|
| **`GLOSSARY.MD`** | Learn a term the hard way? Write it down so no one else does. |
| **`CATALOG.MD`** | New keyword or semantic surface added. |
| **`MEMORY.md`** | Complex work, weird traps, unresolved risks. Distilled lessons only. |
| **`FEEDBACK.md`** | Fundamental language/runtime/toolchain pain. |
| **`BUGS.md`** | Confirmed defects, sharp edges, solver-backed failures. |

## ⚡ PI TOOL ARSENAL

This is the most important section in this file. Read it. Follow it. Every tool below exists because people kept doing things manually. Stop.

### Kain Toolchain — USE THESE RELIGIOUSLY

| Tool | Actions | Use This Instead Of |
|------|---------|-------------------|
| **`kain_stdlib`** | `list_modules`, `get_symbols`, `search_symbols`, `get_details`, `get_source`, `list_keywords`, `get_keyword` | **Manually opening stdlib .kn files to find function signatures.** Every symbol in 65+ modules, searchable by name, signature, docs, source. |
| **`kain_examples`** | `search(query)` | **Manually grepping for "how do people do X".** Semantic search over 11,500 real Kain code chunks. Describe what you want in natural language. |
| **`kain_lang`** | `check`, `build`, `run`, `test`, `amalgamate`, `gpu_artifacts` | Manually chaining commands to compile/test Kain. Use `--json` for structured diagnostics. |
| **`kain_native`** | `emit: exe/sharedlib/staticlib/object/llvm-ir` | Manually invoking clang on LLVM IR. One-shot .kn → binary. |
| **`kain_bazel`** | `build`, `test`, `server`, `sync`, `binary_age`, `freshness` | Manually running bazel commands. Manages server lifecycle for you. |
### Research

| Tool | Actions | Use This Instead Of |
|------|---------|-------------------|
| **`web_search`** | `query:` or `queries:[a,b,c]` | Manually opening a browser |
| **`code_search`** | `query:` | Guessing API signatures |
| **`fetch_content`** | `url:` / `prompt:` for video | Manually scraping docs |

| **`tools`** | `list`, `search`, `which` | Forgetting what tools exist. Run `tools list` at session start. |


## 🧠 MEMORY — Current State & Risks

> *(Search `MEMORY.md` for full detail. This section is updated by agents during work.)*

- Bazel cache lives on `Z:/_b/`. Output base must show `Z:/_b/...`.
- `kain run` from WSL requires env overrides. `kain check`/`build` do not.
- Windows Bazel != Linux truth. Prove Linux behavior in WSL.
- Extension/skill authoring: TypeScript in `~/.pi/agent/extensions/`.

---

## 📚 CATALOG — Language Surface Quick Reference

> *(Full keyword reference via `list_keywords` or `get_keyword <name>`.)*

| Keyword | Kind | What It Does |
|---------|------|-------------|
| `world` | Semantic | Compiler-owned state graph with surfaces |
| `entangle` | Semantic | Bidirectional state sync between worlds |
| `actor` | Semantic | Mailbox-driven concurrent unit |
| `converge` | Semantic | Multi-lane dispatch with capability selection |
| `shatter` | Semantic | Zero-copy data layout for world crossing |
| `teleport` | Semantic | Move data across world boundaries |
| `patch` | Semantic | Transactional world mutation |
| `law` | Semantic | Invariant enforcement on patches |
| `collapse` | Semantic | Enter ownership scope for raw memory |
| `observe` | Semantic | Read-only borrow from ownership scope |
| `decay` | Semantic | Release ownership of raw memory |
| `shader` | Semantic | GPU vertex/fragment/compute kernel |
| `pulse` | Semantic | Timed/clock-driven execution |
| `orchestrate` | Semantic | Multi-language pipeline orchestration |
| `fn` | Standard | Function declaration |
| `struct` | Standard | Data structure |
| `use` | Standard | Module import |
| `include` | Standard | C header import |

---

## 🔧 PI ECOSYSTEM

### How This File Gets Loaded

Pi loads `AGENTS.md` from three sources, **concatenated**:
1. `~/.pi/agent/AGENTS.md` — global (all projects)
2. Parent directory walk from cwd — finds this file at repo root
3. Current directory

Disable with `--no-context-files` or `-nc`.

### Deeper Customization

| File | Effect |
|------|--------|
| `.pi/SYSTEM.md` | Replaces entire system prompt (project-level) |
| `.pi/APPEND_SYSTEM.md` | Appends to system prompt |
| `.pi/settings.json` | Project settings |
| `.pi/extensions/*.ts` | Custom TypeScript tools |
| `.pi/skills/` | Project-local skills |

### Pi Workflow Commands

| Command | What |
|---------|------|
| `/compact [prompt]` | Manually compact context |
| `/reload` | Reload extensions, skills, prompts, context files |
| `/fork` | Branch session from a previous message |
| `/skill:name` | Load a skill by name |
| `/settings` | Modify settings interactively |

### Extension Authoring

Ask pi to build custom tools. Extensions are TypeScript in `~/.pi/agent/extensions/`. Use `pi.registerTool()`, `pi.registerCommand()`, and event hooks (`tool_call`, `session_start`, etc.). NPM dependencies work.

---

## 🏗️ BAZEL — THE TRUTH BUILD LANE (READ THIS BEFORE BUILDING ANYTHING)

**⚠️ CRITICAL: Bazel is the canonical build system. Do NOT use `cargo build` for the Kain compiler binary.** Cargo-built binaries cannot run `kain check`/`kain build`/`kain run` — they bypass the runtime build, the sync pipeline, and the managed binary stamp. Cargo is only for quick Rust compile checks (`cargo check -p kain`), never for producing a working compiler.

### Building the Compiler

```powershell
# Build the compiler (debug, daily dev)
bazel build //:kain --config=dev

# Release build (benchmarks)
bazel build //:kain --config=release

# Build the native C runtime
bazel build //runtime:native_core_runtime --config=dev
```

### Syncing to ~/.kain/bin/ — MANDATORY AFTER EVERY BUILD

**Do this every time you build.** A stale binary causes "unsupported flags" errors and missing features.

```powershell
# One-step sync (builds if needed + copies + verifies)
kain_sync_binary

# Check if binary is stale
kain doctor          # look for "Managed Sync Repo Status: drift"
kain_status          # shows build time, age, git info
```

### Server Lifecycle

Cold Bazel server = 30-90s startup. Keep it warm:

```powershell
kain_bazel action:'server' server_action:'start'    # warm the server
kain_bazel action:'server' server_action:'status'   # check if alive
kain_bazel action:'server' server_action:'stop'     # free resources
```

**Idle timeout:** 3 hours. Re-warm on long sessions. Cache lives at `Z:/_b/` (must be on Z drive, not inside X:\ workspace).

### Key Targets

| Target | What |
|--------|------|
| `//:kain` | Compiler binary (alias → `//crates/cli:kain`) |
| `//runtime:native_core_runtime` | Native C runtime library (30+ .c files) |
| `//:developer_smoke_tests` | Sanity suite (sync test + key crate tests) |
| `//:key_crate_tests` | All 14 core crate test suites |
| `//runtime:native_runtime_tests` | C runtime test suite |

### Why Cargo Is Wrong

| System | Produces | Can run kain check/build/run? |
|--------|----------|------------------------------|
| **Bazel** | `Z:/_b/.../bin/crates/cli/kain.exe` | ✅ Yes — synced, stamped, runtime-linked |
| **Cargo** | `X:/target/debug/kain.exe` | ❌ No — no runtime, no sync stamp, no C build |

**Cargo is only for:** `cargo check` (fast Rust typecheck), `cargo clippy`, or isolating whether a build failure is Bazel-specific. Never for producing a working compiler.

---

## 🔨 KAIN CLI COMMAND QUICK REFERENCE

### Compile & Run

```powershell
# Typecheck only (fast, no codegen)
kain check file.kn
kain check file.kn --json              # structured output for LLMs

# Compile to LLVM IR (produces .ll file)
kain build file.kn --target llvm
kain build file.kn --target llvm --debug    # with DWARF debug metadata

# Compile + link + execute in one step
kain run file.kn --target llvm
kain run file.kn --target llvm -- -arg1 -arg2   # pass runtime args after --
kain run dev file.kn                   # watch + rerun on changes
```

### Other Targets

| Flag | Output |
|------|--------|
| `--target llvm` | LLVM IR → native .exe |
| `--target rust` | Rust source (.rs) |
| `--target cpp` | C++ source (.cpp) |
| `--target c` | C source (.c) |
| `--target wasm` | WebAssembly (.wasm) |
| `--target js` | JavaScript (.js) |
| `--target spirv` | SPIR-V shader binary (.spv) |
| `--target cuda` | NVIDIA PTX (.ptx) |
| `--target hlsl` | DirectX HLSL (.hlsl) |
| `--target wgsl` | WebGPU WGSL (.wgsl) |

### Testing

```powershell
kain test file.kn                      # run compiletest-style tests
kain test dir/ --json                  # structured JSON report
kain test dir/ --ignored               # include //@ ignore cases
```

### Imports

```powershell
kain import-c vendor/sdk -I vendor/include -DPLATFORM_WIN32 -o gen/sdk.kn
kain import-crate serde_json --mode generate -o gen/
kain import platform vulkan --sdk "C:/VulkanSDK/1.3.296.0"
```

### Formatting

```powershell
kain fmt src/ --check                  # check only (exit code)
kain fmt src/main.kn stdlib/ --write   # format in-place
```

### Diagnostics

```powershell
kain doctor                            # full environment diagnostic
kain config show --json                # active config
```

### GPU Artifacts

```powershell
kain gpu-artifacts shaders.kn --target all      # SPIR-V + PTX + HLSL + WGSL + residency
kain gpu-artifacts shaders.kn --target spirv --no-derived
```

### Critical Env Vars

| Var | What |
|-----|------|
| `KAIN_CLANG_PATH` | Path to clang binary (REQUIRED for `kain run` in WSL) |
| `KAIN_RUNTIME_MANIFEST_PATH` | Path to native runtime manifest (REQUIRED for `kain run` in WSL) |
| `KAIN_HOME` | `.kain` home directory override |
| `KAIN_BENCH_V2_FILTER` | Comma-separated benchmark case filter |

---

## 🏗️ BUILDING KAIN SOURCE TO NATIVE — THE LLVM PIPELINE

```
.kn source → Parser → Typechecker → LLVM IR emitter → .ll file → clang → .exe
```

- **`kain build --target llvm`** emits textual LLVM IR (`.ll`) — then clang compiles + links to native
- **`kain run --target llvm`** does the full chain: compile → link → execute
- **`kain_native target=file.kn emit=exe`** is a one-shot convenience that wraps build + clang
- **`--debug` / `-g`** emits DWARF debug metadata (`!DILocation`, `!DISubprogram`, source line mappings) in the LLVM IR. Works with **both** `kain build --target llvm --debug` and `kain run --target llvm --debug`. Only meaningful for `--target llvm`.
- **Output:** `.kain/out/<host-triple>/<lane>/<target>/` — LLVM IR + native binary
- **WSL note:** `kain check`/`kain build` work without env vars. Only `kain run` needs `KAIN_CLANG_PATH` + `KAIN_RUNTIME_MANIFEST_PATH` + `KAIN_HOME` set for WSL path resolution.

### Common Errors

| Error | Fix |
|-------|-----|
| `clang not found` | Set `KAIN_CLANG_PATH` or install LLVM |
| `LLVM native compile failed` | Codegen bug — try `--debug` for more info |
| `kain run` hangs in WSL | Set `KAIN_CLANG_PATH=/usr/bin/clang` + WSL env vars |
| Stale binary / missing features | Run `kain_sync_binary` |

---

## 🐧 WSL / LINUX LANE

WSL (Ubuntu) at `Z:\wsl\ubuntu`. Inside: `/mnt/x` = repo root.

```bash
cd /mnt/x
./scripts/bazel-wsl.sh build //runtime:native_core_runtime
./scripts/bazel-wsl.sh build //:kain --config=dev
```

**`kain run` needs env overrides:**
```bash
KAIN_CLANG_PATH=/usr/bin/clang \
KAIN_RUNTIME_MANIFEST_PATH=/mnt/x/runtime/native_core_runtime.toml \
KAIN_HOME=/mnt/x \
/path/to/kain run file.kn --target llvm
```

**Rule:** Windows Bazel != Linux truth. Prove Linux behavior in WSL.

---

## 🚀 GIT

- Stay on current branch unless asked.
- **Commit and push every session.** Clean worktree always.
- Tag feature commits.
- Never hide uncertainty — say if a proof/benchmark wasn't run.

---

## 🛠️ TOOLCHAIN

- **Scoop** on `F:/` — install anything. No permission needed. Keep off `C:/`.
- **pi extensions** — TypeScript tools in `~/.pi/agent/extensions/`.
- **pi packages** — share via npm/git.





- Quick hint -- if you want the best example in the repo of authored kain -- "X:\benchmark\cases_v2\fusion_chain.kn" this is likely it (also the rest of the cases_v2 folder too)
