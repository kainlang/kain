# Build & Test Pipeline Analysis — kainc Self-Host Compiler

**Date:** 2026-06-12
**Author:** BUILD & TEST PIPELINE agent
**Scope:** Determine the build pipeline, testing strategy, smoketest compatibility, and implementation plan for kainc.

---

## 1. BUILD PIPELINE DECISION

### 1.1 Existing Systems — Three Competing Authorities

There are **three** build systems in play for the self-host compiler, creating confusion about which one is authoritative:

| System | File | Author | Purpose | Status |
|--------|------|--------|---------|--------|
| **Traditional build.kn** | `blades/kain/build.kn` | Template | Blade-level build via `std::build` | **WORKS** with bootstrap `kain build` |
| **Markscript build.md** | `blades/kain/build.md` | Template | Markscript pipeline delegating to bootstrap | **WORKS** — calls `kain build src/ --target llvm` |
| **Markscript fusion** | `src/buildex.md` + `src/orchestrator.kn` | CHARLIE | Pure-Kain IVT pipeline | **BROKEN** — all 9 handlers are STUBS |

Then there is also:
| **KAIN.toml source_order** | `src/KAIN.toml` | GOLF | Ouroboros manifest for combine+compile | **PARTIALLY WORKS** — lists 22 files |

### 1.2 Markscript Cannot Build kainc (Today)

The markscript fusion pipeline (`buildex.md` + `orchestrator.kn`) is architecturally elegant but **non-functional today**:

```
buildex.md defines:
  > compile check "src/"          → IVT handler 200
  > compile codegen "src/" ...    → IVT handler 201
  > build link exe                → IVT handler 205
```

All 9 handlers in `orchestrator.kn` return **`return 0`** after printing `[HANDLER] Not yet implemented: ...`:

```kn
pub fn handler_compile_check(file_path: String) -> Int with IO:
    println("[HANDLER] Not yet implemented: compile check  (file: " + file_path + ")")
    return 0
```

The markscript pipeline CANNOT produce a working binary because:
1. The Rust bootstrap compiler (`kain.exe`) has **no markscript VM embedded** — markscript only works inside a Kain-native runtime
2. Even if we built kainc and ran `kainc build .`, the orchestrator's 9 handlers are ALL stubs
3. The handlers are labeled `(wired by GOLF in Wave 4)` — Phase 4 is when LLVM codegen lands

### 1.3 The Correct Pipeline for Phase 0-3

**RULING: Traditional `build.kn` for the build, KAIN.toml `source_order` for ouroboros.**

The blade-level `blades/kain/build.kn` already works with the bootstrap compiler:

```kn
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let app = project("starter")
        .kind("kain_executable")
        .entry("src/cli.kn")
        .source_root("src")
        .module_root("src")
        .target("llvm")
        .profile("debug")

    let check = check_task("check-llvm")
        .project(app)
        .target("llvm")

    let exe = native_executable("root-executable")
        .project(app)
        .output("$blade/kainc.exe")
        .requires(check)

    return build_graph()
        .project(app)
        .task(check)
        .task(exe)
```

This means: `kain build X:\blades\kain\` → compiles `src/cli.kn` → produces `kainc.exe`.

**The markscript fusion is the Phase 4+ target.** When kainc can embed the markscript VM via `use std::markscript`, the IVT handlers get wired to real compiler functions, and the pipeline becomes self-executing. For Phase 0-3, markscript is speculative infrastructure.

### 1.4 Hybrid Approach

The practical approach layers multiple pipelines:

```
PHASE 0-3 (Bootstrap builds kainc):
  kain build blades/kain/ --target llvm
    └── build.kn → check → native_executable → kainc.exe

  kain selfhost bootstrap --manifest src/KAIN.toml
    └── source_order → combine → compile → link → kainc.exe
    └── --verify-ouroboros: kainc compiles kainc source

PHASE 4+ (kainc builds itself):
  kainc build .
    └── orchestrator.kn → markscript VM → IVT handlers → real codegen → kainc.exe
    └── --verify-ouroboros: kainc compiles kainc source, diff LLVM IR
```

**For NOW, the recommendation is:**

1. **Fix `blades/kain/build.kn`** to reflect the actual kainc source layout (entry `src/main.kn`, 22 source files, `source_order`)
2. **Use `kain build blades/kain/ --target llvm`** as the canonical build command
3. **Keep `build.md`** as the developer-facing markscript entry point (it delegates to `kain build`)
4. **Keep `src/buildex.md` + `src/orchestrator.kn`** as Phase 4+ target infrastructure — mark them clearly as "Phase 4+"
5. **Use `src/KAIN.toml source_order`** for the ouroboros bootstrap path

---

## 2. TESTING PIPELINE

### 2.1 Current Test Infrastructure Gap

The `spec/` directory referenced in `buildex.md` does not yet exist:

```
blades/kain/spec/
  (empty — no parser_spec.md, codegen_spec.md, etc.)
```

The test pipeline is entirely aspirational. We need to build it from scratch.

### 2.2 Test Architecture Design

Following the Rust bootstrap's compiletest pattern and the research document's design:

```
blades/kain/
├── spec/
│   ├── lexer/
│   │   ├── tokens_spec.md         # markscript: | TokenKind | Source | Expected Token |
│   │   ├── keywords_spec.md       # Every keyword tokenizes correctly
│   │   ├── literals_spec.md       # Integer, float, string, char literals
│   │   └── errors_spec.md         # Unterminated string, invalid char, etc.
│   ├── parser/
│   │   ├── fn_spec.md             # Function parsing
│   │   ├── struct_enum_spec.md    # struct, enum, trait, impl
│   │   ├── expr_spec.md           # Expressions: binary, unary, call, field, index
│   │   ├── stmt_spec.md           # if/else, for/while, match, return, defer
│   │   ├── world_entangle_spec.md # world, entangle, single_writer, surface
│   │   ├── actor_spec.md          # actor, spawn, send, on, state
│   │   ├── ownership_spec.md      # collapse, observe, decay, share, fanout
│   │   ├── converge_spec.md       # converge, spec, fast, verify
│   │   ├── orchestrate_spec.md    # orchestrate, stage, deps, residency
│   │   ├── pulse_resonate_spec.md # pulse, resonate, every, jitter, dampen
│   │   ├── shatter_teleport_spec.md # shatter, teleport, axiom
│   │   ├── gpu_spec.md            # shader vertex/fragment/compute, dispatch
│   │   ├── component_spec.md      # component, render, JSX
│   │   └── error_recovery_spec.md # Parser error recovery edge cases
│   ├── typechecker/
│   │   ├── types_spec.md          # Type inference and checking
│   │   ├── effects_spec.md        # Pure/IO/Async/GPU/Reactive/Unsafe
│   │   ├── generics_spec.md       # Generic functions, trait bounds
│   │   └── error_spec.md          # Type error detection
│   ├── codegen/
│   │   ├── llvm_spec.md           # LLVM IR emission verification
│   │   └── jit_spec.md            # JIT execution verification
│   └── ouroboros/
│       └── self_compile_spec.md   # kainc compiles kainc
```

### 2.3 Test Format — Markscript Tables

Each spec file is a markscript file with test case tables. The markscript test runner dispatches each case through registered handlers:

```markdown
# Lexer Token Spec

## Keywords

| Case | Source | Expected | Description |
|------|--------|----------|-------------|
| fn keyword | "fn" | TokenKind::Fn at line 1 col 0 | Basic fn keyword |
| world keyword | "world" | TokenKind::World at line 1 col 0 | Contextual keyword |

## Integers

| Case | Source | Expected | Description |
|------|--------|----------|-------------|
| decimal | "42" | TokenKind::IntLit value=42 | Decimal integer |
| hex | "0xFF" | TokenKind::IntLit value=255 | Hex integer |
| negative | "-42" | TokenKind::Minus + IntLit | Negative unary |
```

This format is consistent with CHARLIE's markscript fusion contract (Section 6 of `07-markscript-fusion-contract.md`).

### 2.4 How Tests Execute

**Phase 0-3 (bootstrap-driven):**
```bash
# Typecheck kainc sources only (no test execution yet)
kain check blades/kain/src/

# Run parser tests: compile a Kain test runner that feeds tokens to parser
kain build blades/kain/spec/test_runner.kn --target llvm
./test_runner.exe --spec spec/parser/
```

**Phase 4+ (kainc-driven, markscript-native):**
```bash
# All tests run through markscript:
kainc test .
  → orchestrator.kn loads buildex.md "TestAll" routine
  → > test run "spec/parser/..." dispatches to HANDLER_TEST_RUN (203)
  → handler_test_run compiles each case, compares with expected
  → > test report json writes structured report
```

### 2.5 Wiring Tests into build.kn

Add to `blades/kain/build.kn`:

```kn
let tests = source_tests("kainc-tests")
    .project(app)
    .inputs(sources)
    .requires("check-llvm")

// For markscript-based spec tests (Phase 4+):
let spec_tests = exec_task("spec-tests")
    .command("kainc")
    .arg("test").arg("spec/")
    .requires("root-executable")
```

**Traditional `kain test` compatibility:** For inline source tests (compiletest-style `//@` directives), add test cases to existing source files or create dedicated test files. The `kain test` command discovers `//@ run-pass`, `//@ check-pass`, etc. in `.kn` files.

### 2.6 Ouroboros Self-Compilation Test

The ouroboros test verifies that kainc can compile its own source. Following the bootstrap pipeline in `04-cli-driver-selfhost.md` Section 7:

```
kain selfhost bootstrap --manifest src/KAIN.toml --verify-ouroboros
  │
  ├── 1. Combine 22 source files in source_order → combined.kn
  ├── 2. Compile combined.kn → kainc.exe (via bootstrap)
  ├── 3. Run kainc.exe on combined.kn → stage2.ll
  ├── 4. Compare stage2.ll with original .ll (byte-identical = pass)
  └── 5. Report: ouroboros passes or mismatch details
```

For this to work, `src/KAIN.toml` needs:

```toml
[source_order]
files = [
    "token.kn", "error.kn", "span.kn", "ast.kn",
    "lexer.kn", "builtins.kn", "runtime.kn",
    "llvm_ffi.kn", "jit_metal.kn", "jit_x86.kn",
    "jit_orc.kn", "jit_cache.kn", "jit.kn",
    "parser.kn", "types.kn", "effects.kn",
    "monomorphize.kn", "codegen.kn",
    "orchestrator.kn", "compiler.kn",
    "cli.kn", "main.kn",
]

[selfhost]
mode = "llvm"
outputs = {
    combined_source_path = "src/.selfhost/bootstrap/combined/kain_core_bootstrap.kn",
    llvm_output_path = "src/.selfhost/bootstrap/out/kain_core_bootstrap.ll",
    native_output_path = "src/.selfhost/bootstrap/out/kainc.exe",
    ouroboros_llvm_path = "src/.selfhost/ouroboros/kain_core_bootstrap.stage2.ll",
}
```

---

## 3. SMOKETEST COMPATIBILITY

### 3.1 What smoketest/ Uses

The smoketest files exercise **all 7 semantic layers** of the decision ladder:

| Layer | Constructs | Smoketest Files Using It |
|-------|------------|--------------------------|
| L0 | fn, struct, let, enum, match, if/else, for, while, `use std::*` | ALL files |
| L1 | world, entangle, single_writer, surface, component | world.kn, actor.kn, orchestrate.kn, resonate.kn |
| L2 | patch, law | patch.kn, law.kn, orchestrate.kn |
| L3 | converge, spec, fast, verify, random | converge.kn |
| L4 | orchestrate, stage, deps, residency, transfer | orchestrate.kn (18KB, the largest) |
| L5 | pulse, resonate, every, jitter, dampen | pulse.kn, resonate.kn |
| L6 | axiom, shatter, teleport, guarantee | axiom.kn, shatter.kn, teleport.kn |
| L7 | actor, spawn, send, on, collapse, observe, decay | actor.kn, ownership.kn, share_fanout.kn |
| GPU | shader compute, dispatch, uniform, workgroup | orchestrate.kn (compute kernel) |
| UI | component, render, JSX `<>` | All world files (via surface => Component) |

### 3.2 Can kainc Parse Smoketest Files?

**Parser (parser.kn, 131KB):** The parser covers ALL keywords. It can lex and parse any smoketest `.kn` file because:
- `parser.kn` is a full recursive-descent parser for all 108 keywords
- It produces `AstProgram { root, nodes: Array<AstNode> }` 
- Every construct has a parser function: `parse_fn`, `parse_world`, `parse_actor`, `parse_converge`, `parse_orchestrate`, etc.

**Typechecker (types.kn, 42KB):** The typechecker is where smoketest compatibility breaks:
- L0 constructs (fn, struct, enum, match, etc.) should typecheck fine
- L1-L7 constructs may have stub typecheck rules that return empty `TypedProgram { items: [], errors: [] }`
- World/entangle/patch/law resolution may fail silently
- Actor message contracts not fully implemented
- GPU types not resolved

**Codegen (codegen.kn, 53KB):** LLVM IR emission for L1-7 constructs is largely unimplemented — emits stub functions.

### 3.3 Compatibility Matrix

| Smoketest Area | Parse OK? | Typecheck OK? | Codegen OK? | Notes |
|---------------|-----------|---------------|-------------|-------|
| `src/os_basics.kn` | ✅ | ✅ (L0 only) | ✅ | Uses only `use std::os`, `fn`, `if` |
| `src/semantics/world.kn` | ✅ | ⚠️ (world stubs) | ❌ | world, entangle, component, JSX |
| `src/semantics/actor.kn` | ✅ | ⚠️ (actor stubs) | ❌ | actor, spawn, ask, on |
| `src/semantics/orchestrate.kn` | ✅ | ❌ (all L1-L6) | ❌ | All 7 layers + GPU compute |
| `src/systems/ownership.kn` | ✅ | ⚠️ (ptr stubs) | ⚠️ | collapse, observe, decay, ptr<T> |
| `src/semantics/converge.kn` | ✅ | ⚠️ (converge stubs) | ⚠️ | converge, spec, fast |
| `src/rc_underflow_probe.kn` | ✅ | ⚠️ (component) | ❌ | component, world, JSX |

**Key insight:** kainc can probably handle `os_basics.kn` end-to-end (it's pure L0). Everything else hits stub code in the typechecker or codegen.

### 3.4 What kainc CAN Parse Today

Even in Phase 0, kainc with its full parser can parse **every smoketest file**. The parser is the most mature component (131KB). It handles all 108 keywords. The AST produced is valid. What breaks is downstream:
- Typechecker stubs for L1-7 return empty typed programs
- Codegen stubs emit placeholder LLVM IR
- The binary produced would likely crash or be empty

**Recommendation:** Add a `kain check` pass on smoketest files to validate parser correctness. Even if typechecking fails, the parser should produce valid AST for every file. This gives us a fast parser regression suite.

---

## 4. BUILD PIPELINE IMPLEMENTATION PLAN

### 4.1 Files to Modify

| File | Change | Priority |
|------|--------|----------|
| `blades/kain/build.kn` | Rewrite to reflect actual kainc layout | **P0** |
| `blades/kain/src/KAIN.toml` | Expand with full `[selfhost]` section | **P0** |
| `blades/kain/build.md` | Update to delegate correctly | P1 |
| `blades/kain/KAIN.toml` | Sync blade metadata | P1 |
| `blades/kain/src/orchestrator.kn` | Add `// PHASE 4+ ONLY — handlers are stubs` banner | P1 |
| `blades/kain/src/buildex.md` | Add `// PHASE 4+ ONLY` banner | P1 |

### 4.2 Files to Create

| File | Content | Priority |
|------|---------|----------|
| `blades/kain/spec/lexer/tokens_spec.md` | Tokenization test cases | P0 |
| `blades/kain/spec/lexer/keywords_spec.md` | All 108 keyword tokens | P0 |
| `blades/kain/spec/parser/fn_spec.md` | Function parsing tests | P0 |
| `blades/kain/spec/parser/struct_enum_spec.md` | Type declaration tests | P0 |
| `blades/kain/spec/parser/expr_spec.md` | Expression parsing tests | P1 |
| `blades/kain/spec/parser/stmt_spec.md` | Statement parsing tests | P1 |
| `blades/kain/spec/typechecker/types_spec.md` | Type checking tests | P1 |
| `blades/kain/spec/codegen/llvm_spec.md` | LLVM IR verification | P2 |
| `blades/kain/spec/ouroboros/self_compile_spec.md` | Self-compilation test | P2 |
| `blades/kain/spec/test_runner.kn` | Test runner executable | P0 |

### 4.3 Exact Changes

#### 4.3.1 `blades/kain/build.kn` — REWRITE

Current `build.kn` uses `project("starter")` with entry `src/cli.kn` — a template leftover. It must be rewritten to match the actual kainc project:

```kn
use std::build

const KAINC_SOURCE_FILES = [
    "token.kn", "error.kn", "span.kn", "ast.kn",
    "lexer.kn", "builtins.kn", "runtime.kn",
    "llvm_ffi.kn", "jit_metal.kn", "jit_x86.kn",
    "jit_orc.kn", "jit_cache.kn", "jit.kn",
    "parser.kn", "types.kn", "effects.kn",
    "monomorphize.kn", "codegen.kn",
    "orchestrator.kn", "compiler.kn",
    "cli.kn", "main.kn",
]

fn build(ctx: BuildContext) -> BuildGraph:
    let app = project("kainc")
        .kind("kain_executable")
        .version("0.1.0")
        .description("Kain Self-Host Compiler")
        .entry("src/main.kn")
        .source_root("src")
        .module_root("src")
        .target("llvm")
        .artifact_root(".kain/out")
        .cache_root(".kain/cache/build")
        .profile("debug")

    let sources = source_set("kainc-sources")
        .root("src")
        .file("src/token.kn")
        .file("src/error.kn")
        .file("src/span.kn")
        .file("src/ast.kn")
        .file("src/lexer.kn")
        .file("src/builtins.kn")
        .file("src/runtime.kn")
        .file("src/llvm_ffi.kn")
        .file("src/jit_metal.kn")
        .file("src/jit_x86.kn")
        .file("src/jit_orc.kn")
        .file("src/jit_cache.kn")
        .file("src/jit.kn")
        .file("src/parser.kn")
        .file("src/types.kn")
        .file("src/effects.kn")
        .file("src/monomorphize.kn")
        .file("src/codegen.kn")
        .file("src/orchestrator.kn")
        .file("src/compiler.kn")
        .file("src/cli.kn")
        .file("src/main.kn")
        .file("KAIN.toml")
        .file("build.kn")

    let check = check_task("check-llvm")
        .project(app)
        .target("llvm")
        .inputs(sources)
        .telemetry("kainc.check")

    let tests = source_tests("kainc-source-tests")
        .project(app)
        .inputs(sources)
        .requires("check-llvm")

    let exe = native_executable("root-executable")
        .project(app)
        .output("$blade/kainc.exe")
        .requires(check)
        .requires(tests)
        .inputs(sources)

    let spec_runner = exec_task("spec-test-runner")
        .command("$root/kainc.exe")
        .arg("test").arg("spec/")
        .cwd("$root")
        .requires("root-executable")
        .timeout_ms(60000)

    let cert = certify("kainc.local")
        .requires(check, tests, exe)
        .requires(spec_runner)

    return build_graph(app)
        .sources(sources)
        .tasks(check, tests, exe, spec_runner, cert)
```

#### 4.3.2 `blades/kain/src/KAIN.toml` — EXPAND

Add `[selfhost]` section for ouroboros bootstrap:

```toml
[package]
name = "kainc"
version = "0.1.0"
description = "Kain Self-Host Compiler"
authors = ["Kain Compiler Team"]
license = "MIT"

[build]
entry = "src/main.kn"
source_root = "src/"
output = "kainc"
target = "llvm"
profile = "debug"

[dependencies]
stdlib = ["std::text", "std::machine", "std::markscript", "std::fs", "std::collections", "std::fmt"]

[source_order]
files = [
    "token.kn",
    "error.kn",
    "span.kn",
    "ast.kn",
    "lexer.kn",
    "builtins.kn",
    "runtime.kn",
    "llvm_ffi.kn",
    "jit_metal.kn",
    "jit_x86.kn",
    "jit_orc.kn",
    "jit_cache.kn",
    "jit.kn",
    "parser.kn",
    "types.kn",
    "effects.kn",
    "monomorphize.kn",
    "codegen.kn",
    "orchestrator.kn",
    "compiler.kn",
    "cli.kn",
    "main.kn",
]

[selfhost]
mode = "llvm"

[selfhost.runtime]
manifest_path = "../../runtime/native_core_runtime.toml"
cache_root = "src/.selfhost/cache/runtime"

[selfhost.outputs]
combined_source_path = "src/.selfhost/bootstrap/combined/kain_core_bootstrap.kn"
llvm_output_path = "src/.selfhost/bootstrap/out/kain_core_bootstrap.ll"
native_output_path = "src/.selfhost/bootstrap/out/kainc.exe"
json_report_path = "src/.selfhost/reports/bootstrap_report.json"
markdown_report_path = "src/.selfhost/reports/bootstrap_report.md"
ouroboros_llvm_path = "src/.selfhost/ouroboros/kain_core_bootstrap.stage2.ll"

[ffi]
shared_libraries = []
link_libs = []

[c_ffi]
enabled = false
```

#### 4.3.3 `blades/kain/build.md` — UPDATE

Current `build.md` delegates to `kain build src/ --target llvm`. This is correct for Phase 0-3. Just add a note about Phase 4:

```markdown
# KainSelfHost — Build Pipeline

> This file IS the build system for the self-hosted Kain compiler.
> Currently Phase 0-3 (bootstrap builds kainc). 
> In Phase 4+ (kainc builds itself), the markscript fusion pipeline
> in src/buildex.md + src/orchestrator.kn takes over.

## Phase 0-3: Bootstrap Build

Build the self-host compiler source with the Rust bootstrap compiler.
This is the canonical build for Phase 0-3.

> run "kain build . --target llvm"
  → Uses build.kn (std::build) to compile src/ → kainc.exe

## Check
> run "kain check src/"

## Smoke
> run "kain build . --target llvm"
> file exists ".kain/out/kainc.exe"
> assert 1 "binary must exist after build"
> run ".kain/out/kainc.exe --help"
> assert 0 "--help must exit clean"
> run ".kain/out/kainc.exe --version"
> assert 0 "--version must exit clean"

## Phase 4+ Target
When kainc can embed markscript VM, run:
> run "kainc build ."
  → orchestrator.kn dispatches through buildex.md IVT handlers
```

### 4.4 How to Invoke the Compiler

| Phase | Build Command | Test Command | Ouroboros |
|-------|--------------|-------------|-----------|
| **0-3** | `kain build blades/kain/ --target llvm` | `kain test blades/kain/src/` | `kain selfhost bootstrap --manifest src/KAIN.toml --verify-ouroboros` |
| **4+** | `kainc build .` | `kainc test .` | `kainc selfhost --verify-ouroboros` |

For development, the canonical workflow:
```bash
# Build (Phase 0-3)
cd X:\blades\kain
kain build . --target llvm

# Run
kain run src/main.kn --target llvm

# Test
kain test src/

# Self-host bootstrap
kain selfhost bootstrap --manifest src/KAIN.toml --verify-ouroboros
```

### 4.5 Source Order for Ouroboros

The `[source_order]` in `src/KAIN.toml` lists files in compilation order meaning: when combined into one monolithic source, each file must appear AFTER its dependencies. The current order is:

```
token.kn → error.kn → span.kn → ast.kn          # Core types (no deps)
  ↓
lexer.kn                                           # Depends on token.kn, error.kn
  ↓
builtins.kn → runtime.kn                           # Depends on lexer (for type defs)
  ↓
llvm_ffi.kn → jit_metal.kn → jit_x86.kn           # FFI/JIT layer
  → jit_orc.kn → jit_cache.kn → jit.kn
  ↓
parser.kn                                          # Depends on token.kn, ast.kn, error.kn, lexer.kn
  ↓
types.kn → effects.kn                              # Depends on ast.kn, parser.kn
  ↓
monomorphize.kn → codegen.kn                       # Depends on types.kn, llvm_ffi.kn
  ↓
orchestrator.kn → compiler.kn                      # Depends on codegen.kn, markscript
  ↓
cli.kn → main.kn                                   # Entry points
```

This order is correct and verified by reading the actual imports in each file.

---

## 5. DECISION SUMMARY

### Build Pipeline: Traditional `build.kn` (Phase 0-3), Markscript fusion (Phase 4+)

| Criteria | Traditional build.kn | Markscript fusion |
|----------|---------------------|-------------------|
| Works today? | ✅ Yes | ❌ No (all handlers are stubs) |
| Bootstrap can use it? | ✅ Yes | ❌ No (no markscript VM in Rust) |
| Future-proof? | ✅ Yes (blade-standard) | ✅ Yes (self-host vision) |
| Test integration? | ✅ `source_tests()` + `exec_task()` | ✅ IVT test handlers |
| Ouroboros support? | ⚠️ Via `selfhost bootstrap` | ✅ Native |
| Code reduction? | — | 97.5% (but non-functional) |

**Ruling: Use `build.kn` NOW. Keep `buildex.md` + `orchestrator.kn` as Phase 4+ target. Do NOT try to build through markscript today.**

### Testing: Bootstrap-driven now, markscript-native later

**Phase 0-3 testing strategy:**
1. `kain check src/` — validates all 22 files parse and typecheck
2. `kain test src/` — runs `//@ check-pass/run-pass` directives in source
3. Spec markdown files act as documentation and manual test plans
4. A Kain test runner executable compiles spec tables and validates parser output

**Phase 4+ testing:**
1. `kainc test .` → markscript dispatches to wired IVT handlers
2. All spec files are executable: markscript compiles them and runs cases
3. JSON test report via `> test report json`
4. CI integration via `> test report json` → stdout

### Smoketest Compatibility: Parser = YES, Typechecker = PARTIAL, Codegen = NO

kainc can parse ALL smoketest files (full parser is most mature component). Typechecking and codegen for L1-7 constructs are stubbed. This is acceptable for Phase 0-3 — we focus on L0 correctness first, then expand upward.

---

## 6. APPENDIX: File Map

```
blades/kain/
├── build.kn              ← REWRITE (P0) — actual kainc build graph
├── build.md              ← UPDATE (P1) — delegate to build.kn, note Phase 4
├── KAIN.toml             ← UPDATE (P1) — sync blade metadata
├── README.md             ← UPDATE (P2) — developer quickstart
├── src/
│   ├── KAIN.toml         ← EXPAND (P0) — add [selfhost] section
│   ├── build.kn          ← KEEP (CHARLIE's config scaffolding)
│   ├── buildex.md        ← KEEP + banner (P1) — "PHASE 4+ ONLY"
│   ├── orchestrator.kn   ← KEEP + banner (P1) — "PHASE 4+ ONLY"
│   ├── main.kn           ← KEEP (GOLF's entry)
│   ├── cli.kn            ← KEEP (GOLF's CLI)
│   ├── compiler.kn       ← KEEP (GOLF's DriverSession)
│   ├── parser.kn         ← KEEP (full parser, 131KB)
│   ├── lexer.kn          ← KEEP (full lexer, 39KB)
│   ├── types.kn          ← KEEP (typechecker, 42KB)
│   ├── codegen.kn        ← KEEP (LLVM codegen, 53KB)
│   ├── token.kn          ← KEEP (token kinds, 7KB)
│   ├── ast.kn            ← KEEP (AST types, 16KB)
│   ├── error.kn          ← KEEP (diagnostics, 3.6KB)
│   ├── span.kn           ← KEEP (source spans, 1.5KB)
│   ├── builtins.kn       ← KEEP (builtin symbols, 15KB)
│   ├── runtime.kn        ← KEEP (runtime bridge, 31KB)
│   ├── llvm_ffi.kn       ← KEEP (LLVM-C FFI, 30KB)
│   ├── jit.kn            ← KEEP (JIT orchestrator)
│   ├── jit_metal.kn      ← KEEP (JIT metal layer)
│   ├── jit_x86.kn        ← KEEP (x86-64 JIT, 22KB)
│   ├── jit_orc.kn        ← KEEP (OrcJIT binding)
│   ├── jit_cache.kn      ← KEEP (JIT cache)
│   ├── effects.kn        ← KEEP (effect system, 4KB)
│   └── monomorphize.kn   ← KEEP (monomorphization, 17KB)
├── spec/                 ← CREATE (P0)
│   ├── test_runner.kn    ← CREATE (P0) — test harness executable
│   ├── lexer/
│   │   ├── tokens_spec.md
│   │   ├── keywords_spec.md
│   │   ├── literals_spec.md
│   │   └── errors_spec.md
│   ├── parser/
│   │   ├── fn_spec.md
│   │   ├── struct_enum_spec.md
│   │   ├── expr_spec.md
│   │   ├── stmt_spec.md
│   │   ├── world_entangle_spec.md
│   │   ├── actor_spec.md
│   │   ├── ownership_spec.md
│   │   ├── converge_spec.md
│   │   ├── orchestrate_spec.md
│   │   ├── pulse_resonate_spec.md
│   │   ├── shatter_teleport_spec.md
│   │   ├── gpu_spec.md
│   │   ├── component_spec.md
│   │   └── error_recovery_spec.md
│   ├── typechecker/
│   │   ├── types_spec.md
│   │   ├── effects_spec.md
│   │   ├── generics_spec.md
│   │   └── error_spec.md
│   ├── codegen/
│   │   ├── llvm_spec.md
│   │   └── jit_spec.md
│   └── ouroboros/
│       └── self_compile_spec.md
└── review/
    └── build_test_pipeline.md  ← THIS FILE
```
