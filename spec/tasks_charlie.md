# Stream CHARLIE: MarkScript Orchestration

**Stream ID:** CHARLIE
**Role:** Embed the MarkScript VM into the compiler as its orchestration layer — register 9 IVT handlers, load build config from markscript tables, execute build pipelines defined in `buildex.md`
**Effort:** ~3 hours
**Depends On:** none (only needs `std::markscript` from Kain stdlib)
**Requirements Covered:** FR-ORCH.1–19
**Design Reference:** Research 07, Design §§ORCH, MarkScript Embedding API Contract

---

## Context

Markscript is the compiler's orchestration layer — it handles build config, pipeline execution, test running, REPL, and process orchestration. The compiler registers 9 custom IVT handlers (IDs 200–208) into the markscript VM, and markscript dispatches them when the corresponding intent phrases appear in `buildex.md`. This eliminates ~3,000 lines of infrastructure code compared to a custom build system.

The orchestration file `orchestrator.kn` is the ONLY file you create. It uses exactly 20 public functions from `std::markscript` — no direct internal calls (Constraint C-10). The compiler core (lexer, parser, typechecker, codegen) is called through the IVT handlers — but those handlers are STUBS initially. They get wired in Wave 4 (GOLF) when the full pipeline exists.

This stream is COMPLETELY INDEPENDENT. You don't need any compiler types — you only need markscript's VM API.

---

## Files You Own

### Files to Create

| File | Purpose | After This Stream |
|------|---------|-------------------|
| `X:\blades\kain\src\orchestrator.kn` | MarkScript VM embedding + 9 IVT handler stubs (~500 lines) | GOLF reads (as orchestration entry point) |
| `X:\blades\kain\src\build.kn` | Build config scaffolding (markscript table schema) (~50 lines) | GOLF extends with real config |
| `X:\blades\kain\src\buildex.md` | Build pipeline definition (markscript intents + routines) (~100 lines) | GOLF extends with real pipeline stages |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:\blades\kain\src\lexer.kn` | Owned by Stream ALPHA |
| `X:\blades\kain\src\parser.kn` | Owned by Stream DELTA |
| `X:\blades\kain\src\types.kn` | Owned by Stream FOXTROT |
| `X:\blades\kain\src\codegen.kn` | Owned by Stream GOLF |
| `X:\blades\kain\src\compiler.kn` | Owned by Stream GOLF |
| `X:\blades\kain\src\cli.kn` | Owned by Stream GOLF |

---

## Implementation Tasks

---

### CHARLIE-01: BuildConfig Struct + Config Loading (`orchestrator.kn`, part 1)

**Effort:** 0.5h
**Objective:** Define the `BuildConfig` struct and implement `load_build_config()` that reads markscript tables from `build.md`.

**Implementation:**

Create `X:\blades\kain\src\orchestrator.kn`:

```kn
// orchestrator.kn — MarkScript VM embedding for compiler orchestration
// STREAM: CHARLIE
// Consumed by: GOLF (as orchestration entry point)

use std::markscript

// ═════════════════════════════════════════════════════════════════════
// BuildConfig — populated from markscript tables in build.md
// ═════════════════════════════════════════════════════════════════════

pub struct BuildConfig:
    name:         String
    target:       String
    profile:      String
    optimize:     Bool
    lto:          String
    entry:        String
    source_root:  String
    deps:         String
    output:       String
    runtime:      String
    linker:       String
    linker_flags: String
    cc:           String
    cc_flags:     String
    test_root:    String
    doc_root:     String

pub fn build_config_default() -> BuildConfig:
    return BuildConfig {
        name: "kainc",
        target: "llvm",
        profile: "debug",
        optimize: false,
        lto: "none",
        entry: "src/main.kn",
        source_root: "src/",
        deps: "",
        output: "kainc",
        runtime: "kain_runtime",
        linker: "clang",
        linker_flags: "",
        cc: "clang",
        cc_flags: "",
        test_root: "",
        doc_root: "",
    }

// Load build config from markscript tables in build.md
pub fn load_build_config(vm: markscript.VmHandle, config_path: String) -> BuildConfig with IO:
    let status: Int = markscript.mks_run_file(config_path)
    if status != 0:
        // Config file not found or parse error — return defaults
        return build_config_default()

    // Find the Metadata table
    let meta_handle: Int = markscript.mks_find_table(vm, "Metadata")
    if meta_handle < 0:
        return build_config_default()

    let mut config: BuildConfig = build_config_default()

    // Read each field from the markscript table
    // Table layout: row 0 = project name, row 1 = target, row 2 = profile, ...
    let name_val: String = markscript.mks_table_get_string(vm, meta_handle, 0, 1, "kainc")
    let target_val: String = markscript.mks_table_get_string(vm, meta_handle, 1, 1, "llvm")
    let profile_val: String = markscript.mks_table_get_string(vm, meta_handle, 2, 1, "debug")
    let opt_val: Int = markscript.mks_table_get_int(vm, meta_handle, 3, 1, 0)
    let lto_val: String = markscript.mks_table_get_string(vm, meta_handle, 4, 1, "none")
    let entry_val: String = markscript.mks_table_get_string(vm, meta_handle, 5, 1, "src/main.kn")
    let src_root_val: String = markscript.mks_table_get_string(vm, meta_handle, 6, 1, "src/")

    config.name = name_val
    config.target = target_val
    config.profile = profile_val
    config.optimize = opt_val == 1
    config.lto = lto_val
    config.entry = entry_val
    config.source_root = src_root_val

    return config
```

**Acceptance Criteria:**
- [ ] `BuildConfig` struct has all 16 fields with correct types
- [ ] `build_config_default()` returns sensible defaults
- [ ] `load_build_config()` reads from markscript tables using the 6 API functions
- [ ] Falls back to defaults when config file is missing

---

### CHARLIE-02: IVT Handler Constants + Handler Stubs (`orchestrator.kn`, part 2)

**Effort:** 1h
**Objective:** Define the 9 IVT handler ID constants and implement the stub handler functions that bridge markscript intents to compiler core operations.

**Implementation (append to `orchestrator.kn`):**

```kn
// ═════════════════════════════════════════════════════════════════════
// IVT Handler IDs (registered into markscript VM)
// ═════════════════════════════════════════════════════════════════════

pub const HANDLER_COMPILE_CHECK:   Int = 200
pub const HANDLER_COMPILE_CODEGEN: Int = 201
pub const HANDLER_COMPILE_JIT:     Int = 202
pub const HANDLER_TEST_RUN:        Int = 203
pub const HANDLER_TEST_REPORT:     Int = 204
pub const HANDLER_BUILD_LINK:      Int = 205
pub const HANDLER_BUILD_PACKAGE:   Int = 206
pub const HANDLER_SELFHOST_PHASE1: Int = 207
pub const HANDLER_SELFHOST_PHASE2: Int = 208

// ═════════════════════════════════════════════════════════════════════
// Handler Functions — STUBS (wired by GOLF in Wave 4)
// ═════════════════════════════════════════════════════════════════════

// Handler 200: "compile check" — lex + parse + typecheck only
pub fn handler_compile_check(file_path: String) -> Int with IO:
    // STUB: GOLF wires this to DriverSession::check()
    // let session = driver_session_new()
    // let result = driver_session_check(session, read_file(file_path), file_path)
    // return if result.has_errors then 1 else 0
    let _ = file_path
    return 0  // stub: always succeeds

// Handler 201: "compile codegen" — full compilation (lex → parse → typecheck → codegen)
pub fn handler_compile_codegen(file_path: String, target: String, profile: String) -> Int with IO:
    // STUB: GOLF wires this to DriverSession::compile()
    let _ = file_path
    let _ = target
    let _ = profile
    return 0

// Handler 202: "compile jit" — JIT in-memory execution
pub fn handler_compile_jit(file_path: String) -> Int with IO:
    // STUB: GOLF wires this to JIT path
    let _ = file_path
    return 0

// Handler 203: "test run" — execute test specification
pub fn handler_test_run(spec_path: String) -> Int with IO:
    // STUB: GOLF wires test discovery + execution
    let _ = spec_path
    return 0

// Handler 204: "test report" — generate formatted report
pub fn handler_test_report(format: String) -> String with IO:
    // STUB
    let _ = format
    return ""

// Handler 205: "build link" — link object files into final binary
pub fn handler_build_link(target: String) -> Int with IO:
    // STUB: GOLF wires clang invocation
    let _ = target
    return 0

// Handler 206: "build package" — full end-to-end build
pub fn handler_build_package(package_name: String) -> Int with IO:
    // STUB
    let _ = package_name
    return 0

// Handler 207: "selfhost phase1" — Rust DLL bridge
pub fn handler_selfhost_phase1(crate_name: String) -> Int with IO:
    // STUB
    let _ = crate_name
    return 0

// Handler 208: "selfhost phase2" — pure Kain self-compilation
pub fn handler_selfhost_phase2(crate_name: String) -> Int with IO:
    // STUB
    let _ = crate_name
    return 0
```

**Acceptance Criteria:**
- [ ] All 9 handler IDs defined as `pub const` Int constants (200–208)
- [ ] All 9 handler functions have correct signatures matching the IVT contract
- [ ] All stubs are compilable (no type errors)
- [ ] Each stub has a TODO comment describing what GOLF wires

---

### CHARLIE-03: VM Initialization + Handler Registration (`orchestrator.kn`, part 3)

**Effort:** 0.75h
**Objective:** Implement `orchestrator_init()` that creates the markscript VM and registers all 9 handlers.

**Implementation (append to `orchestrator.kn`):**

```kn
// ═════════════════════════════════════════════════════════════════════
// OrchestratorState — VM handle + config
// ═════════════════════════════════════════════════════════════════════

pub struct OrchestratorState:
    vm:      markscript.VmHandle
    config:  BuildConfig

pub fn orchestrator_init(config_path: String) -> OrchestratorState with IO:
    let vm: markscript.VmHandle = markscript.mks_new_vm()

    // Register all 9 compiler-specific IVT handlers
    markscript.mks_register(vm, "compile check", HANDLER_COMPILE_CHECK)
    markscript.mks_register(vm, "compile codegen", HANDLER_COMPILE_CODEGEN)
    markscript.mks_register(vm, "compile jit", HANDLER_COMPILE_JIT)
    markscript.mks_register(vm, "test run", HANDLER_TEST_RUN)
    markscript.mks_register(vm, "test report", HANDLER_TEST_REPORT)
    markscript.mks_register(vm, "build link", HANDLER_BUILD_LINK)
    markscript.mks_register(vm, "build package", HANDLER_BUILD_PACKAGE)
    markscript.mks_register(vm, "selfhost phase1", HANDLER_SELFHOST_PHASE1)
    markscript.mks_register(vm, "selfhost phase2", HANDLER_SELFHOST_PHASE2)

    let config: BuildConfig = load_build_config(vm, config_path)

    return OrchestratorState {
        vm: vm,
        config: config,
    }
```

**Acceptance Criteria:**
- [ ] `orchestrator_init()` creates VM, registers 9 handlers, loads config
- [ ] Handler IDs match the constants defined in CHARLIE-02
- [ ] Config loaded from the provided path

---

### CHARLIE-04: Build Pipeline Execution + CLI Integration (`orchestrator.kn`, part 4)

**Effort:** 0.75h
**Objective:** Implement pipeline dispatch functions that load and execute `buildex.md` routines.

**Implementation (append to `orchestrator.kn`):**

```kn
// ═════════════════════════════════════════════════════════════════════
// Pipeline Execution
// ═════════════════════════════════════════════════════════════════════

pub fn orchestrator_build(state: OrchestratorState, stage: String) -> Int with IO:
    // Load and execute buildex.md pipeline
    let status: Int = markscript.mks_run_file("buildex.md")
    if status != 0:
        return status

    // Dispatch the requested build stage (or "BuildAll" by default)
    let routine: String = if stage == "":
        "BuildAll"
    else:
        stage

    // Execute the routine through markscript
    let result: Int = markscript.mks_run_with_vm(state.vm, routine)
    return result

pub fn orchestrator_check(state: OrchestratorState, path: String) -> Int with IO:
    // Quick check: lex + parse + typecheck only
    let result: Int = handler_compile_check(path)  // STUB pending GOLF
    return result

pub fn orchestrator_test(state: OrchestratorState, spec_path: String) -> Int with IO:
    let result: Int = handler_test_run(spec_path)  // STUB pending GOLF
    return result

pub fn orchestrator_selfhost(state: OrchestratorState, verify: Bool) -> Int with IO:
    // Self-host pipeline
    let result: Int = 0
    // Phase 1: Rust bridge (optional)
    // result = handler_selfhost_phase1("kainc")
    // if result != 0: return result

    // Phase 2: Pure Kain self-compilation
    result = handler_selfhost_phase2("kainc")
    if result != 0:
        return result

    // Ouroboros verification
    if verify:
        // TODO: GOLF wires byte-comparison logic
        pass

    return result

// ── CLI entry points (called by GOLF's cli.kn / main.kn) ──

pub fn orch_build_cli(input_path: String, target: String, profile: String, stage: String) -> Int with IO:
    let state: OrchestratorState = orchestrator_init("build.md")
    state.config.target = target
    state.config.profile = profile
    return orchestrator_build(state, stage)

pub fn orch_check_cli(input_path: String) -> Int with IO:
    let state: OrchestratorState = orchestrator_init("build.md")
    return orchestrator_check(state, input_path)

pub fn orch_run_cli(input_path: String) -> Int with IO:
    let state: OrchestratorState = orchestrator_init("build.md")
    return handler_compile_jit(input_path)  // STUB pending GOLF

pub fn orch_test_cli(input_path: String) -> Int with IO:
    let state: OrchestratorState = orchestrator_init("build.md")
    return orchestrator_test(state, input_path)

pub fn orch_selfhost_cli(verify: Bool) -> Int with IO:
    let state: OrchestratorState = orchestrator_init("build.md")
    return orchestrator_selfhost(state, verify)
```

**Acceptance Criteria:**
- [ ] `orchestrator_build()` loads `buildex.md` and dispatches the named routine
- [ ] `orchestrator_check()` calls the compile check handler
- [ ] CLI entry points (`orch_build_cli`, `orch_check_cli`, etc.) provide clean interface for GOLF
- [ ] All functions compile without errors

---

## Build Pipeline Definition (`buildex.md`)

Create `X:\blades\kain\src\buildex.md`:

```markdown
# kainc Build Pipeline

@schema (Metadata: string[6], ...)

## Metadata
| name | target | profile | optimize | lto | entry | source_root | deps | output | runtime | linker | linker_flags | cc | cc_flags | test_root | doc_root |
|------|--------|---------|----------|-----|-------|-------------|------|--------|---------|--------|--------------|----|---------|-----------|----------|
| kainc| llvm   | debug   | 0        | none| src/main.kn | src/   |      | kainc  | kain_runtime | clang |          | clang |     | spec/  | docs/    |

## Routines

### BuildAll
> compile check "src/"
> compile codegen "src/" --llvm --debug
> build link exe
> assert 0

### QuickCheck
> compile check "src/"
> assert 0

### JitRun
> compile jit "src/main.kn"
> assert 0

### TestAll
> compile check "src/"
> test run "spec/"
> assert 0

### CleanAll
> spawn "rm -rf out/"
> await 0
```

**Acceptance Criteria:**
- [ ] `buildex.md` has correct markscript format with `@schema` directive
- [ ] Metadata table has all required columns
- [ ] BuildAll routine chains compile check → compile codegen → build link
- [ ] QuickCheck does compile check only
- [ ] TestAll chains check + test run

---

## Stream Conventions

- **Language:** Pure Kain with IO effect for all markscript API calls
- **Naming:** snake_case for functions; PascalCase for structs; `HANDLER_*` for IVT IDs
- **Imports:** `use std::markscript` — ONLY these 20 public API functions are permitted
- **Handler stubs:** Each stub must have a clear TODO comment describing what GOLF will wire
- **Testing:** Verify markscript VM creation and handler registration in isolation

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT implement any compiler pipeline logic (lexer, parser, typechecker, codegen) — those are stubs only
- ❌ Do NOT call markscript internal functions — use only the 20 public API functions
- ❌ Do NOT import from `src::lexer`, `src::parser`, `src::types`, `src::codegen` — the orchestrator is independent
- ❌ Do NOT wire the handler stubs to real implementations — that's GOLF's job in Wave 4
- ❌ Do NOT use Layer 1–7 constructs (world, actor, converge, etc.) — exception: `world` MAY be used for OrchestratorState if helpful (Constraint C-3)

---

## Verification (After This Stream)

```bash
# Typecheck the orchestrator
kain check X:\blades\kain\src\orchestrator.kn
```

**Self-check:**
- [ ] `orchestrator.kn` created with all sections
- [ ] 9 IVT handler IDs defined (200–208)
- [ ] 9 handler stubs with correct signatures
- [ ] `orchestrator_init()` creates VM and registers all handlers
- [ ] `orchestrator_build()` loads buildex.md and dispatches routines
- [ ] `build.kn` and `buildex.md` created
- [ ] Compiles cleanly

---

## Completion Report

When done, report:
- Files created: orchestrator.kn, build.kn, buildex.md — with line counts
- Handlers registered: 9 (IDs 200–208)
- Handler stubs: all 9 compilable, awaiting GOLF wiring
- Any issues encountered
