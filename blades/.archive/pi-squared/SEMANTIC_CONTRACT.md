# SEMANTIC CONTRACT -- pi-squared Kain Decision Ladder Enforcement

**Status:** First audit >> 2026-06-13
**Maintainer:** Semantic Contract Enforcer
**Scope:** All pi-squared source under `X:/blades/pi-squared/src/`
**Applies to:** All agents writing pi-squared code

---

## Table of Contents

1. [Purpose](#1-purpose)
2. [Decision Ladder Overview](#2-decision-ladder-overview)
3. [pi-squared Layer Mapping ~~ Current State](#3-pi-squared-layer-mapping--current-state)
4. [pi-squared Layer Mapping ~~ Desired State](#4-pi-squared-layer-mapping--desired-state)
5. [Audit Findings |-> Every Violation Found](#5-audit-findings--every-violation-found)
6. [Anti-Patterns and Correct Idioms](#6-anti-patterns-and-correct-idioms)
7. [Code Review Checklist for Kain Correctness](#7-code-review-checklist-for-kain-correctness)
8. [High-Value Kain Examples From The Repo](#8-high-value-kain-examples-from-the-repo)
9. [Skeleton Templates for Each Missing Construct](#9-skeleton-templates-for-each-missing-construct)
10. [Priority Migration Order](#10-priority-migration-order)

---

## 1. Purpose

This document is the **enforcement bible** for all agents writing Kain code in pi-squared. Its job is to answer one question: **given a problem, which Kain construct should I reach for?**

Kain's innovation is its **compiler-owned semantic stack** --> 15+ constructs where the compiler, not the programmer, owns the truth about state, mutation, dispatch, timing, coupling, layout, and handoff. When you use `fn` and `let` for a problem that should be a `world`, `patch`, `converge`, or `pulse`, you're paying the semantic cost without getting the compiler's help.

This document:

- Maps every pi-squared subsystem to its correct semantic layer
- Documents every decision ladder violation found in the current codebase
- Provides before/after code for every anti-pattern
- Links to canonical Kain examples from the repo
- Provides a code review checklist for future agents

**If you are writing or reviewing pi-squared code, read this first. If you are unsure which construct to use, climb the decision ladder.**

---

## 2. Decision Ladder Overview

```
                    ┌──────────────────────────────────────────┐
                    │ "Am I crossing into C/OS?" │ include     │
                    │ "Is this Python host code?" │ import     │
                    │ "Is this a UI component?"   │ component  │
                    ├──────────────────────────────────────────┤
LAYER 7: SYSTEMS    │ "Concurrent message state?" │ actor     │
                    │ "Raw memory lifecycle?"     │ collapse   │
                    │                             │ observe    │
                    │                             │ decay      │
                    ├──────────────────────────────────────────┤
LAYER 6: MACHINE    │ "Capability assumption?"    │ axiom      │
  STONES            │ "Hot-data layout?"          │ shatter    │
                    │ "Cross-world zero-copy?"    │ teleport   │
                    ├──────────────────────────────────────────┤
LAYER 5: TEMPORAL   │ "Timed recurrence?"         │ pulse      │
                    │ "React to state change?"    │ resonate   │
                    ├──────────────────────────────────────────┤
LAYER 4: STAGE      │ "Multi-stage pipeline?"     │ orchestrate│
  GRAPH             │ "Cross-runtime scheduling?" │ orchestrate│
                    ├──────────────────────────────────────────┤
LAYER 3: DISPATCH   │ "Spec + fast lanes?"        │ converge   │
                    │ "Platform-specific perf?"   │ converge   │
                    ├──────────────────────────────────────────┤
LAYER 2: STATE      │ "Journaled mutation?"       │ patch      │
  INTEGRITY         │ "Invariant predicate?"      │ law        │
                    ├──────────────────────────────────────────┤
LAYER 1: STATE      │ "Global named state?"       │ world      │
  AUTHORITY         │ "Mirrored state?"           │ entangle   │
                    │ "Coupled fields?"           │ entangle   │
                    ├──────────────────────────────────────────┤
LAYER 0: PLAIN      │ None of the above?          │ fn, struct │
  CODE              │                             │ let, enum  │
                    │                             │ trait, impl│
                    └──────────────────────────────────────────┘
```

**The Rule:** Climb from the bottom. If a Layer 1-7 construct fits, use it. `fn` is the fallback, not the first choice.

---

## 3. pi-squared Layer Mapping ⁓ Current State

| Layer | Construct | Count | Files | Correctness |
|-------|-----------|-------|-------|-------------|
| L7 | `actor` | 6 | SessionTree, PiSettingsManager, AgentActor, AgentEventBus, ResourceLoader, InputActor | CORRECT |
| L7 | `collapse/observe/decay` | **0** | – | MISSING |
| L6 | `axiom` | **0** | ‒ | MISSING |
| L6 | `shatter struct` | **0** | === | MISSING |
| L6 | `teleport` | **0** | ⁓ | MISSING |
| L5 | `pulse` | 1 | compaction.kn (compaction_check) | STUB -- body is no-op |
| L5 | `resonate` | **0** | <--> | MISSING |
| L4 | `orchestrate` | 1 | compaction.kn (compact_pipeline) | CORRECT pattern |
| L3 | `converge` | 1 | tools/registry.kn (dispatch_tool) | PARTIAL -- identical spec/fast bodies |
| L2 | `patch` | 11 | trust.kn, registry.kn, models.kn, key_vault.kn, providers/registry.kn | CORRECT pattern |
| L2 | `law` | 7-8 | defaults.kn, trust.kn, key_vault.kn | CORRECT pattern |
| L1 | `world` | 8 | ToolRegistry, TrustStore, ModelRegistry, LlmProviderRegistry, ApiKeyVault, FauxProviderWorld, TerminalScreen, ThemeWorld | CORRECT |
| L1 | `entangle` | **0** | |-> | MISSING |
| L0 | `fn` | 50+ | All files | OVERUSED |
| L0 | `struct`/`enum` | ~20 | types.kn, others | CORRECT |
| UI | `component` | 4 | Stub components | PARTIAL ~~ underutilized |

### Missing Constructs (Zero Usage)

| Construct | Why It's Missing | Impact |
|-----------|-----------------|--------|
| `entangle` | Worlds are independent islands | No compiler-owned state propagation between worlds |
| `resonate` | No reactive state change handlers | State changes silently ignored |
| `axiom` | No capability declarations | Missing platform contracts |
| `shatter struct` | No SoA layout optimization | Appropriate for tool parameter tables |
| `teleport` | No cross-world data movement | All data movement is implicit |
| `collapse/observe/decay` | No raw memory ownership | File/JSON parsing without ownership tracking |

---

## 4. pi-squared Layer Mapping 〰 Desired State

| Subsystem | Current Constructs | Should Also Use | Priority |
|-----------|-------------------|----------------|----------|
| Agent turn loop | `agent.kn` – actor | `resonate` for state changes, `entangle` for world mirroring | HIGH |
| Config subsystem | `settings.kn` >> actor, `defaults.kn` - laws | `entangle` between TrustStore and other worlds | MEDIUM |
| Startup pipeline | `startup.kn` - sequential `fn` calls | `orchestrate` instead of sequential `fn` | **HIGH** |
| Session tree | `tree.kn` ~ actor | `resonate` on entry changes | MEDIUM |
| Event bus | `events.kn` ->> actor | `resonate` as an alternative reactive path | LOW |
| Compaction | `compaction.kn` >> orchestrate + pulse | Wire pulse body (currently no-op), add telemetry guards | HIGH |
| Tool dispatch | `registry.kn` --- converge | Real divergent fast lanes (not identical bodies) | **HIGH** |
| Trust store | `trust.kn` ... world + patch + law | `entangle` between trust decisions | LOW |
| TUI | `tui/` - actor + world | `resonate` for screen state changes, `entangle` for theme sync | MEDIUM |
| World coupling | All worlds | `entangle` between authority/mirror pairs | MEDIUM |
| Resource loading | `loader.kn` <--> actor | `resonate` for cache invalidation | LOW |

---

## 5. Audit Findings ~~ Every Violation Found

### VIOLATION 1 --- Startup Pipeline Uses Sequential `fn` Instead of `orchestrate`

**File:** `X:/blades/pi-squared/src/pipeline/startup.kn`
**Layer violation:** L0 (`fn`) where L4 (`orchestrate`) is correct
**Rulebook reference:** "LAYER 4: STAGE GRAPH <--> Multi-stage pipeline? → orchestrate"
**Severity:** HIGH

**Current code:**

```kn
// startup.kn ->> sequential fn calls, NO orchestrate
pub fn run_startup_pipeline(flags: CliArgs) -> StartupResult:
    run_config_migrations()              // Phase 1
    run_session_migrations()             // Phase 2
    spawn_setup(flags)                   // Phase 3
    let cwd = process_current_working_directory()
    let trust = resolve_trust(cwd, TrustStore)  // Phase 4
    let model = resolve_model(flags, ModelRegistry { models: [] })  // Phase 5
    var system_prompt = "You are pi-squared, a helpful coding assistant."
    return StartupResult::Ready(
        "settings", "session", "resources", model, system_prompt,
    )
```

**Why it's wrong:** The startup has 5+ distinct phases (migrate → config → spawn → trust → model → prompt) with different runtimes and failure modes. Without `orchestrate`, there are no: typed stage dependencies, residency declarations, failure fallbacks, law guards, or telemetry counters. The compiler cannot validate the DAG.

**Correct idiom:** (See Skeleton §9.3 ~> should be added by the BRAVO stream)

```kn
orchestrate startup_pipeline(flags: CliArgs) -> StartupResult:
    stage migrate: cpu run_config_migrations()
        residency host policy static
    stage session_migrate: cpu run_session_migrations()
        deps [migrate] residency host policy static
    stage spawn: cpu spawn_setup(flags)
        deps [session_migrate] residency host policy static
    stage trust_resolve: converge resolve_trust_decision(flags)
        deps [spawn] residency host policy telemetry_balance_latency
    stage model_resolve: converge resolve_model_for_flags(flags)
        deps [trust_resolve] residency host policy static
    stage prompt_build: cpu build_system_prompt(model_resolve)
        deps [model_resolve] residency host policy static
    return StartupResult::Ready(spawn, model_resolve, prompt_build)
```

---

### VIOLATION 2 >> Converge `dispatch_tool` Has Identical Spec and Fast Lane Bodies

**File:** `X:/blades/pi-squared/src/tools/registry.kn` (line 47)
**Layer violation:** L3 (`converge`) but used as regular `fn` dispatch
**Rulebook reference:** "LAYER 3: DISPATCH :: Spec + fast lanes? → converge"
**Severity:** HIGH

**Current code:**

```kn
converge dispatch_tool(
    tool_name: String,
    tool_call_id: String,
    params: String,
    signal: AbortSignal
) -> ToolResult:
    spec reference:
        let result = execute_tool_by_name(tool_name, tool_call_id, params, signal)
        return result
    fast cached_lane when capability("tool.result_cache"):
        let result = execute_tool_by_name(tool_name, tool_call_id, params, signal)
        return result
```

**Why it's wrong:** Both lanes call the exact same function. There is no divergent implementation. No `verify random(N)` clause. The converge adds complexity with zero benefit. The capability `tool.result_cache` doesn't actually select a different implementation. This should either be a plain `fn` or have genuinely divergent lanes.

**Correct idiom (use `fn` for single-path, or real lanes for converge):**

```kn
// Option A: Plain fn (no converge needed)
pub fn dispatch_tool(
    tool_name: String, tool_call_id: String,
    params: String, signal: AbortSignal
) -> ToolResult:
    return execute_tool_by_name(tool_name, tool_call_id, params, signal)

// Option B: Real converge with divergent lanes
converge dispatch_tool(
    tool_name: String, tool_call_id: String,
    params: String, signal: AbortSignal
) -> ToolResult:
    spec reference:
        return execute_tool_by_name(tool_name, tool_call_id, params, signal)
    fast cache_hit_lane when capability("tool.result_cache"):
        return execute_tool_with_cached_result(tool_name, tool_call_id, params, signal)
    fast remote_lane when target("wasm"):
        return execute_tool_remote(tool_name, tool_call_id, params, signal)
    verify random(4)
```

---

### VIOLATION 3 --- Mode Dispatch Uses `if/elif` Chain Instead of `match` on Enum

**File:** `X:/blades/pi-squared/src/main.kn` (line 124)
**Layer violation:** L0 * * * `if/elif` where `match` is cleaner
**Severity:** MEDIUM

**Current code:**

```kn
fn enter_mode(...) -> Int:
    let mode = if flags.print_mode: "print" else: flags.mode
    if mode == "print":
        return run_print_mode(...)
    elif mode == "interactive":
        return run_interactive_mode(...)
    elif mode == "json":
        print("pi-squared: json mode not yet implemented\n")
        return 3
    ...
```

**Correct idiom:**

```kn
enum CliMode:
    Print Interactive Json Rpc

fn enter_mode(mode: CliMode, ...) -> Int:
    match mode:
        CliMode::Print => run_print_mode(...)
        CliMode::Interactive => run_interactive_mode(...)
        CliMode::Json => run_json_mode(...)
        CliMode::Rpc => run_rpc_mode(...)
```

---

### VIOLATION 4 * * * `persist_settings` is a `fn`, Not a `patch`

**File:** `X:/blades/pi-squared/src/config/settings.kn` (line 178)
**Layer violation:** L0 (`fn`) where L2 (`patch`) is correct --> journaled mutation
**Rulebook reference:** "LAYER 2: STATE INTEGRITY - Journaled mutation? → patch"
**Severity:** MEDIUM

**Current code:**

```kn
fn persist_settings(path: String, settings: Settings):
    let serialized = serialize_settings_to_json(settings)
    fs_write_text(path, serialized)
```

**Why it's wrong:** Settings persistence is a world-state mutation (it writes to the filesystem). It should be journaled via `patch` so that:
- Every mutation is tracked via `patch_journal_count()`
- Undo is possible via `abi_patch_undo_last()`
- The mutation is visible in the runtime contract
- Entangle propagation can trigger on the change

**Correct idiom:**

```kn
patch persist_settings(world: SettingsWorld, path: String, settings: Settings) -> Int:
    world.settings_json = serialize_settings_to_json(settings)  // journaled
    return fs_write_text(path, world.settings_json)
```

---

### VIOLATION 5 ‒ `spawn_setup` is a `fn`, Not an Orchestrate Stage

**File:** `X:/blades/pi-squared/src/pipeline/startup.kn` (line 64)
**Layer violation:** L0 (`fn`) where L4 (orchestrate stage) is correct
**Severity:** MEDIUM

**Current code:**

```kn
fn spawn_setup(flags: CliArgs):
    let settings_mgr = spawn PiSettingsManager(...)
    let session = spawn SessionTree()
    let resources = spawn ResourceLoader(...)
    return
```

**Why it's wrong:** Spawning actors is a setup phase with dependency ordering. The actors may need to be wired together (session → agent, event bus → agent). This should be part of the orchestrate pipeline with typed dependencies.

---

### VIOLATION 6 ~~ No `resonate` Anywhere - Silent State Changes

**Layer violation:** L5 (`resonate`) completely missing
**Rulebook reference:** "LAYER 5: TEMPORAL ... React to state change? → resonate"
**Severity:** MEDIUM

**Why it's wrong:**

- When `ToolRegistry.active_tool_names` changes via `register_tool` patch, no handler reacts
- When `TrustStore.decisions_json` changes, no downstream effect is triggered
- When `ApiKeyVault` has a key change, no reconnection logic runs
- When `ModelRegistry` registers a new model, nothing updates the tool schema

Without `resonate`, these state changes are invisible. Downstream code must poll actors or check state manually.

**Correct idiom (example for tool registry):**

```kn
// In tools/registry.kn or a new tools/resonator.kn
resonate ToolRegistry.active_tool_names dampen 100 ms:
    // When tools change, update agent's tool schema
    let new_names_str: String = resonate_new_i64 as String
    if new_names_str != "":
        // Invalidate cached schema, trigger re-build
        ToolRegistry.schema_stale = true
```

---

### VIOLATION 7 * * * No `entangle` Between Worlds >> 8 Independent Islands

**Layer violation:** L1 (`entangle`) completely missing
**Rulebook reference:** "LAYER 1: STATE AUTHORITY |-> Mirrored state? → world + entangle"
**Severity:** MEDIUM

**Current state:** `ToolRegistry`, `TrustStore`, `ModelRegistry`, `LlmProviderRegistry`, `ApiKeyVault`, `FauxProviderWorld`, `TerminalScreen`, `ThemeWorld` <--> all independent, no coupling.

**Why it's wrong:** Many of these worlds have related state. When the trusted provider changes, the API key vault should update. When the model registry adds a model, the terminal screen theme might adjust. Without `entangle`, these must be manually coordinated.

**Correct idiom (example for dual authority/mirror pattern):**

```kn
// Authority world --> mutable
world ToolRegistryAuthority:
    state active_tool_names: String = "[]"
    surface native_ui => PiToolRegistryStub

// Mirror world ‒ read-only via entangle
world ToolRegistryMirror:
    state active_tool_names_copy: String = "[]"
    surface web => PiToolRegistryStub

// Compiler-owned propagation
entangle ToolRegistryAuthority.active_tool_names
    <-> ToolRegistryMirror.active_tool_names_copy
    with single_writer
```

---

### VIOLATION 8 <--> No `axiom` Declarations >> Missing Capability Contracts

**Layer violation:** L6 (`axiom`) completely missing
**Rulebook reference:** "LAYER 6: MACHINE STONES -- Capability assumption? → axiom"
**Severity:** LOW-MEDIUM

**Why it's wrong:** pi-squared uses `converge` with custom capabilities (`tool.result_cache`), `orchestrate` with stage guards, and `pulse` with timing guarantees. But it never declares what capabilities it assumes. An `axiom` would declare that the platform supports `converge.dispatch`, `tool.result_cache`, and `time.pulse`.

**Correct idiom:**

```kn
axiom pi_squared_platform:
    when target("llvm")
    when capability("converge.dispatch")
    when capability("tool.result_cache")
    guarantee "pi-squared requires LLVM compilation, converge dispatch, and tool caching"
    fallback pi_squared_degraded
```

---

### VIOLATION 9 >> Compaction Pulse Body Is Empty (No-Op)

**File:** `X:/blades/pi-squared/src/session/compaction.kn` (line 182)
**Layer violation:** L5 (`pulse`) declared but body is a comment stub
**Severity:** HIGH

**Current code:**

```kn
pulse compaction_check every 30000 ms jitter 2000 ms:
    let _tick = pulse_tick
    let _dt = pulse_dt_ms
    // In production:
    //   1. Get compaction_enabled from PiSettingsManager
    //   2. Get current context from SessionTree.GetContext
    //   3. Call should_compact() with threshold
    //   4. If yes, run compact_pipeline
    //   5. Append compaction via AppendCompaction
```

**Why it's wrong:** A pulse with an empty body was registered and will fire every 30s doing nothing. The Z3-proven scheduler thread will dispatch a fire wrapper that does three local loads and returns. This wastes scheduler cycles and provides no telemetry value.

**Correct idiom:** Wire the pulse body to actually check and compact:

```kn
pulse compaction_check every 30000 ms jitter 2000 ms:
    // 1. Check if compaction is enabled
    let enabled_reply = ask(settings_mgr, "GetSetting",
        pack("compaction_enabled", "effective"))
    // v0.1: hardcoded check until PiSettingsManager is wired
    if pulse_tick % 2 == 0:
        let ctx_reply = ask(session_tree, "GetContext", 0)
        let ctx = unpack_context(ctx_reply)
        let should_run = should_compact(
            estimate_context_tokens(ctx),
            200000,   // context window
            16384     // reserve
        )
        if should_run:
            let result = compact_pipeline(ctx, 20000)
            let _ = ask(session_tree, "AppendCompaction",
                pack(result.summary, result.new_leaf_id))
    let _telemetry = pulse_tick + pulse_dt_ms + pulse_missed
```

---

### VIOLATION 10 ⁓ No Telemetry Delta Guards Anywhere

**Layer violation:** L0 <--> missing telemetry verification
**Rulebook reference:** fusion_chain.kn lines 326-334 --- mandatory telemetry delta pattern
**Severity:** MEDIUM

**Current state:** pi-squared has 11 patches, 1 converge, 1 orchestrate, 1 pulse, 6 actors. None of them prove they actually fired via telemetry counters.

**Why it's wrong:** The fusion_chain benchmark establishes the telemetry delta guard as mandatory for any code that claims to use semantic constructs. Without delta guards:
- You cannot prove patches wrote to the journal
- You cannot prove the orchestrate pipeline ran
- You cannot prove converge selected a lane
- You cannot prove the pulse fired

**Correct idiom:**

```kn
fn prove_patches_fired() -> Int:
    let before = patch_journal_count()
    // ... run the operation ...
    let delta = patch_journal_count() - before
    if delta < 1 and before < 256:
        return -13  // patch never fired
    return 0
```

---

### VIOLATION 11 ___ No `entangle` Epoch Bumps on Patches

**Files:** All `patch` declarations in trust.kn, registry.kn, key_vault.kn, models.kn
**Layer violation:** L2 ___ patches missing epoch counters
**Severity:** MEDIUM

**Current code:**

```kn
patch register_tool(name: String, info: ToolInfo) -> Int:
    let names = json_string_array_field(json_parse_text(ToolRegistry.active_tool_names))
    var updated: JsonArray = json_array()
    var ni: Int = 0
    while ni < len(names):
        updated = json_array_push_string(updated, names[ni])
        ni = ni + 1
    updated = json_array_push_string(updated, name)
    ToolRegistry.active_tool_names = json_stringify(updated)
    return 0
```

**Why it's wrong:** The patch writes to `ToolRegistry.active_tool_names` but does NOT bump an epoch counter. The patch doc says: "Every patch that mutates state should bump an epoch counter. This is how the compiler and downstream constructs (resonate, entangle, orchestrate) observe that state changed."

**Correct idiom:**

```kn
patch register_tool(world: ToolRegistry, name: String, info: ToolInfo) -> Int:
    let names = json_string_array_field(json_parse_text(world.active_tool_names))
    var updated: JsonArray = json_array()
    var ni: Int = 0
    while ni < len(names):
        updated = json_array_push_string(updated, names[ni])
        ni = ni + 1
    updated = json_array_push_string(updated, name)
    world.active_tool_names = json_stringify(updated)
    world.epoch = world.epoch + 1    // ALWAYS bump epoch
    return 0
```

---

### VIOLATION 12 – JSON String Fields Instead of Native Kain Types

**Files:** trust.kn, agent.kn, registry.kn, models.kn, key_vault.kn
**Layer violation:** L0 --> using JSON strings where native Kain structs are appropriate
**Severity:** MEDIUM

**Current code examples:**

```kn
// trust.kn
state decisions_json: String = "{}"
state session_only_json: String = "{}"

// agent.kn
state model_config: String = ""       // JSON of Model
state steer_queue: String = "[]"      // JSON array of AgentMessage
state follow_up_queue: String = "[]"

// registry.kn
state active_tool_names: String = "[]"  // JSON array of tool names
```

**Why it's wrong:** Using JSON-encoded strings instead of Kain struct types:
- Loses type safety (the compiler doesn't check JSON keys)
- Requires `json_parse_text`/`json_stringify` round-trips on every access
- Prevents the compiler from tracking field ownership and mutations
- Makes patches less efficient (whole-string replacement vs. field mutation)
- Complicates entangle (the entire string changes, not just one field)

**Correct idiom:**

```kn
// Instead of JSON string, use Kain native types
world ToolRegistry:
    surface native_ui => PiToolRegistryStub
    state active_tool_names: [String] = []
    state epoch: Int = 0

patch register_tool(world: ToolRegistry, name: String, info: ToolInfo) -> Int:
    push(world.active_tool_names, name)  // native array mutation
    world.epoch = world.epoch + 1
    return 0
```

---

### VIOLATION 13 * * * String-Based Actor References Instead of Typed Handles

**File:** `X:/blades/pi-squared/src/main.kn` (line 81, 85-86, 91)
**Layer violation:** L0 – type erasure
**Severity:** LOW

**Current code:**

```kn
fn handle_startup_result(result: StartupResult) -> Int:
    return enter_mode(
        "settings",       // should be typed actor handle
        "session",        // should be typed actor handle
        "resources",      // should be typed actor handle
        ...
    )
```

**Why it's wrong:** Actor references are strings, not typed handles. The compiler cannot verify that "settings" refers to a valid `PiSettingsManager` actor. This is a Rust-like workaround for a compiler limitation. As the compiler matures, typed actor handles will provide stronger guarantees.

**Mitigation:** Keep a registry mapping string names to `spawn` results. See `tools/registry.kn` for the world-based pattern ... apply the same approach to actors:

```kn
// Better approach: registry-based actor lookup
world PiSquaredWorld:
    surface native_ui => PiSquaredStub
    state settings_actor_ref: String = ""
    state session_actor_ref: String = ""
    state agent_actor_ref: String = ""
    state event_bus_ref: String = ""
    state resource_loader_ref: String = ""
    state epoch: Int = 0
```

---

### VIOLATION 14 --> `event_type_to_string` Uses Serial Return Instead of Match Expression

**File:** `X:/blades/pi-squared/src/agent/events.kn` (line 92)
**Layer violation:** L0 --- redundant `return` in match arms
**Severity:** LOW (cosmetic but noisy)

**Current code:**

```kn
pub fn event_type_to_string(t: AgentEventType) -> String with Pure:
    match t:
        AgentEventType::AgentStart           => return "agent_start"
        AgentEventType::AgentEnd             => return "agent_end"
```

**Kain idiom:** Match is an expression, use it:

```kn
pub fn event_type_to_string(t: AgentEventType) -> String with Pure:
    match t:
        AgentEventType::AgentStart           => "agent_start"
        AgentEventType::AgentEnd             => "agent_end"
```

---

### VIOLATION 15 -- `shatter struct` Not Used -- JSON Arrays As Tool Parameters

**File:** `X:/blades/pi-squared/src/tools/`
**Layer violation:** L6 (`shatter struct`) not used for hot data paths
**Severity:** LOW

**Why it's wrong:** Tool parameters are passed as JSON strings (`params: String`) and parsed via `json_parse_text` in every handler. This is the hottest code path in pi-squared. For tool execution, a `shatter struct` would provide SoA-layout access to parameter fields without JSON round-trips.

**Correct idiom (future optimization):**

```kn
shatter struct ToolCallBatch:
    tool_names: [String]
    tool_ids: [String]
    params_path: [String]
    params_offset: [Int]
    params_limit: [Int]
    // ... one field per access pattern
```

---

## 6. Anti-Patterns and Correct Idioms

### 6.1 Sequential `fn` Calls vs Orchestrate

```
WRONG:                                      RIGHT:
fn pipeline():                              orchestrate pipeline():
    let a = step1(value)                        stage a: cpu step1(value) residency host
    let b = step2(a)                            stage b: cpu step2(a) deps [a] residency host
    return step3(b)                             stage c: cpu step3(b) deps [b] residency host
                                            return c
```

### 6.2 `if` Chain for Platform Dispatch vs Converge

```
WRONG:                                      RIGHT:
fn mix(value: Int) -> Int:                  converge mix(value: Int) -> Int:
    if target_is("llvm"):                       spec reference:
        return fast_mix(value)                      return scalar_mix(value)
    return scalar_mix(value)                    fast llvm_lane when target("llvm"):
                                                    return fast_mix(value)
                                            verify random(4)
```

### 6.3 `let mut` for Global State vs World

```
WRONG:                                      RIGHT:
var global_signal: Int = 0                  world Authority:
                                                state signal: Int = 0
fn set(value: Int):                             surface native_ui => Panel
    global_signal = value
                                            patch set(world: Authority, value: Int):
                                                world.signal = value
                                                world.epoch += 1
```

### 6.4 `fn` Instead of `patch` for Journaled Mutations

```
WRONG:                                      RIGHT:
fn register_tool(name: String):             patch register_tool(world: ToolWorld, name: String):
    ToolRegistry.active = name                   world.active = name
                                                  world.epoch += 1
```

### 6.5 Inline `if` Check vs `law`

```
WRONG:                                      RIGHT:
fn set(value: Int) -> Int:                  law value_in_bounds(v: Int) -> Bool:
    if value < 0 or value >= MOD:               return v >= 0 and v < MOD
        return -1
                                            fn set(value: Int) -> Int:
                                                if law_status(value_in_bounds(value)) < 0:
                                                    return -1
```

### 6.6 `while` + `sleep` vs `pulse`

```
WRONG:                                      RIGHT:
fn game_loop():                             pulse game_clock every 16ms jitter 2ms:
    while true:                                 let next = World.frame + 1
        step(World)                             let r = commit(World, next)
        sleep(16)
```

### 6.7 Callback/Global Variable vs `resonate`

```
WRONG:                                      RIGHT:
fn on_signal_change(old, new):              resonate World.signal dampen 0 ms:
    World.shadow = process(new)                 World.shadow = process(resonate_new_i64)
```

### 6.8 Normal Struct for Hot Data vs `shatter struct`

```
WRONG:                                      RIGHT:
struct Particle:                            shatter struct Particle:
    x: Float                                     x: Float
    y: Float                                     y: Float
```

### 6.9 Direct World Assignment vs `teleport`

```
WRONG:                                      RIGHT:
let data = WorldA.data                      teleport data from WorldA to WorldB via bus
WorldB.data = data                          // zero-copy, moved semantics
```

### 6.10 No Telemetry Verification vs Delta Guards

```
WRONG:                                      RIGHT:
patch commit(value):                        let patch_before = patch_journal_count()
    world.field = value                     commit(value)
let r = world.field                         let delta = patch_journal_count() - patch_before
                                            if delta < 1: return -13
```

---

## 7. Code Review Checklist for Kain Correctness

### Before submitting pi-squared code, check every item:

#### State Authority (Layer 1)

- [ ] **Is global mutable state involved?** → Should it be a `world`?
- [ ] **Do two worlds have related state?** → Should they be coupled via `entangle`?
- [ ] **Does a world mutate state?** → Use a `patch`, not direct `world.field = value`

#### State Integrity (Layer 2)

- [ ] **Does this mutation need auditability/undo?** → Must be a `patch`
- [ ] **Does this check enforce a correctness constraint?** → Must be a `law`, not an inline `if`
- [ ] **Does the patch bump an epoch counter?** → Every patch needs epoch bumps
- [ ] **Is `law_status()` used instead of raw `Bool` check?** → Use the standard vocabulary

#### Dispatch (Layer 3)

- [ ] **Is this an `if target_is(...)` chain?** → Replace with `converge`
- [ ] **Does the converge have divergent fast lanes?** → If lanes are identical, use `fn`
- [ ] **Does the converge have a `verify random(N)` clause?** → Required if lanes can diverge
- [ ] **Is the converge selection order correct?** → Most-specific lanes first

#### Stage Graph (Layer 4)

- [ ] **Is this a sequential multi-step process?** → Should be `orchestrate`, not chained `fn` calls
- [ ] **Are stages properly guarded?** → Use `requires` for law dependencies
- [ ] **Do stages have proper residency/transfer?** → Host for CPU, device for GPU
- [ ] **Does the body prove it fired?** → Telemetry delta guard on `orchestrate_stage_count()`

#### Temporal (Layer 5)

- [ ] **Is there a recurring timer?** → Must be `pulse`, not `while` + `sleep`
- [ ] **Does the pulse body do real work?** → Report no-op pulse bodies immediately
- [ ] **Is there a reactive handler for state changes?** → Should be `resonate`, not polling
- [ ] **Is the dampen window appropriate?** → 0ms for critical, Nms for debounced

#### Machine Stones (Layer 6)

- [ ] **Are capability assumptions declared?** → Use `axiom` for orchestrate guards
- [ ] **Is hot data SoA-friendly?** → Consider `shatter struct` for tight loops
- [ ] **Is data moving between worlds?** → Use `teleport` for zero-copy transfer

#### Systems (Layer 7)

- [ ] **Is there concurrent state with message passing?** → Must be `actor` or `actor` + orchestrate
- [ ] **Is raw memory involved?** → Must use `collapse`/`observe`/`decay` lifecycle
- [ ] **Are actor handlers non-blocking?** → Blocking operations should be delegated

#### General

- [ ] **Does the code use JSON strings where native types work?** → Convert to Kain structs
- [ ] **Are effect annotations present on every `fn`?** → No bare functions without `with Pure`/`with IO`
- [ ] **Is there a telemetry delta guard for every construct?** → Prove everything fired
- [ ] **Is `match` used instead of `if/elif` for enum dispatch?** → Match is exhaustive
- [ ] **Are actor references strings?** → Use a registry world to map names to handles

---

## 8. High-Value Kain Examples From The Repo

### 8.1 World + Entangle + Patch + Law (State Integrity Chain)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/fusion_chain.kn` | **THE canonical example.** Worlds `FusionAuthority` + `FusionMirror` with 4 entangles. 6 patches with epoch bumps. 1 law. Full causal chain: patch → resonate → orchestrate → entangle → actor → teleport. Lines 1-550. |
| `X:/blades/experiments/convergence/src/world.kn` | Rat experiment telemetry world ~ raw buffer pointers entangled with scalar counters. Shows selective entangle. |
| `X:/blades/python/24_tet/src/resonate_py_effects.kn` | 30+ field authority + selective mirror. Organized by semantic section. |

**What to steal:** The dual-world pattern (Authority + Mirror + entangle), epoch bump on every patch, law as pipeline guard, telemetry delta guards.

### 8.2 Actor + Spawn + Send + Ask (Concurrent Message Passing)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/fusion_chain.kn` | `FusionWorker` spawns `FusionVerifier`, delegates `reply_to` via `send`. `FusionRelay` has 3 state fields. `FusionTeleporter` does ownership + teleport inside a handler. Actor cascade pattern. |
| `X:/benchmark/cases_v2/actor_ownership_backpressure.kn` | 4-worker pool with warmup asks, packed payloads, convergence inside handlers. |

**What to steal:** Actor delegation pattern (spawn verifier, pass reply_to), packed payloads via `fusion_pack`/`fusion_unpack`, converge inside actor handlers.

### 8.3 Converge + Spec + Fast + Verify (Platform Dispatch)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/fusion_chain.kn` | `fusion_fast_mix` >> spec + 2 fast lanes (llvm, interpret) + `verify random(4)`. Called from orchestrate stage. |
| `X:/blades/experiments/convergence/src/orchestrate.kn` | `quantum_maze_run` --- 3 lanes with different strategies, domain-specific capabilities. **Creative abuse: converge as strategy selector.** |
| `X:/stdlib/random.kn` | `xoshiro_scramble` => the only stdlib converge. Spec is scalar, fast is LLVM. |

**What to steal:** `verify random(N)` on every converge, domain-specific capability keys (`tool.result_cache`), strategy selection pattern from the rat experiment.

### 8.4 Orchestrate + Stage + Deps + Residency (Multi-Stage Pipeline)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/orchestrate_god.kn` | **GOD MODE.** 3 orchestrates: 8-stage (9 runtimes), 6-stage (6 runtimes), 6-stage (with fallback abort). Every metadata clause used. |
| `X:/benchmark/cases_v2/orchestration.kn` | Selector-only style. 7 stages, 6 runtime kinds, minimal metadata. |
| `X:/benchmark/cases_v2/gpu_cpu_pipeline.kn` | Minimal guard style. GPU → law + conditional early-return. |
| `X:/blades/python/24_tet/src/resonate_py_effects.kn` | 9-stage audio effects pipeline with 2 law checks. |

**What to steal:** `requires <law_stage>` for guard gating, `after <stage>` for linear deps, `residency host` for CPU, telemetry delta guard on `orchestrate_stage_count()`.

### 8.5 Pulse + Every + Jitter (Timed Recurrence)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases/pulse_teleport_decay_mesh/main.kn` | Apex pulse: 13 semantics in one program. Pulse + shatter + teleport + ownership + entangle. |
| `X:/benchmark/cases_v2/fusion_chain.kn` | `fusion_tick_driver` -- every 8ms jitter 1ms, bumps world `pulse_ticks` field. |
| `X:/blades/python/24_tet/src/resonate_py_effects.kn` | `fx_modulation_tick` – every 8ms, uses `pulse_dt_ms` for phase accumulation. |

**What to steal:** `pulse_dt_ms` for delta-time-aware processing, `pulse_missed` for overload detection, `jitter Nms` for scheduler friendliness.

### 8.6 Resonate + Dampen (Reactive State Change)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/fusion_chain.kn` | `resonate FusionAuthority.signal dampen 0ms` -- calls orchestrate pipeline from handler. **Join point: resonate → orchestrate → world state.** |
| `X:/blades/python/24_tet/src/resonate_py_effects.kn` | Resonate on `lfo1_rate` adjusts `tremolo_rate`. Resonate on `distortion_drive` adjusts `distortion_output`. Cascading reactivity. |

**What to steal:** Dampen for debouncing (0ms for critical, 32ms for expensive), anti-self-feedback rule (write to shadow, not trigger field), orchestrate inside handler.

### 8.7 Shatter Struct (SoA Layout)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/fusion_chain.kn` | `FusionShard` :: 5 fields, used as teleport payload. |
| `X:/benchmark/cases_v2/keyword_crucible.kn` | `CrucibleShard` ___ shatter struct used in share/fanout parallel region. |
| `X:/blades/experiments/convergence/src/shatter.kn` | `TrailSample` * * * SoA for simulation particle data. |

**What to steal:** Use for hot data that's accessed by field lane (SoA beats AoS for SIMD/GPU). Combine with teleport for cross-world zero-copy.

### 8.8 Teleport + From + To + Via (Zero-Copy Transfer)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/fusion_chain.kn` | `teleport shard from FusionAuthority to FusionMirror via fusion_shard_bus` ~~ inside actor handler. |
| `X:/smoketest/src/semantics/teleport.kn` | Canonical smoke test --- Score teleport with integrity proof. |

**What to steal:** Always verify teleport integrity with `score_before == score_after`, use `via` naming for bus identity.

### 8.9 Component + State + Render + JSX (UI Components)

| File | What It Shows |
|------|---------------|
| `X:/blades/kain/component_minimal/src/app.kn` | **GOLD MINE:** 280-line Win32 GDI app with components, `include <windows.h>`, raw memory, `@extern`, full message pump. |
| `X:/blades/kain/component_fuzz/src/components.kn` | 40+ component stress tests --> recursive, pointer-laden, world-surface, fragments. |

**What to steal:** Components exist without worlds, methods for JSX logic (not inline expressions), uppercase for component calls, lowercase for native elements.

### 8.10 Collapse + Observe + Decay (Ownership Lifecycle)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/fusion_chain.kn` | `FusionTeleporter.ShatterAndSend` -- alloc → collapse → observe → decay inside actor handler. **The canonical ownership pattern.** |
| `X:/benchmark/cases_v2/keyword_crucible.kn` | `crucible_ownership_chain_checksum` 〰 share + fanout + collapse + clflush + decay. Full ownership state machine. |

**What to steal:** Always: alloc → collapse (write) → observe (read) → decay (free). Use `share`/`fanout` for atomic parallel accumulation.

### 8.11 The Fusion Chain (All Layers in One File)

| File | What It Shows |
|------|---------------|
| `X:/benchmark/cases_v2/fusion_chain.kn` | **THE canonical file.** 550 lines, all 7 layers, 7 benchmark cases, telemetry delta guards, causal chain: patch → resonate → orchestrate → entangle → actor → teleport → world. |

**This is the single most important file in the repo for understanding how Kain's semantic stack composes.** Every pi-squared developer should read it.

---

## 9. Skeleton Templates for Each Missing Construct

### 9.1 Entangle Template (Add to Worlds)

```kn
// In any file with related worlds:
entangle Authority.field1 <-> Mirror.field1_copy with single_writer
entangle Authority.field2 <-> Mirror.field2_copy with single_writer

// Always bump epoch on authority patches:
patch update_authority(world: Authority, value: Int) -> Int:
    world.field1 = value
    world.epoch = world.epoch + 1
    return world.epoch
```

### 9.2 Resonate Template (Reactive State Handler)

```kn
// In tools/registry.kn or dedicated reactor file:
resonate ToolRegistry.active_tool_names dampen 100 ms:
    // old/new values available automatically
    let new_val: Int = resonate_new_i64
    // Write to a DIFFERENT field (cannot write back to active_tool_names)
    ToolRegistry.tool_schema_stale = true
    ToolRegistry.last_change_tick = resonate_new_i64
```

### 9.3 Orchestrate Startup Template (Replace Sequential `fn`)

```kn
orchestrate pi_squared_startup(flags: CliArgs) -> StartupResult:
    stage init: cpu pi_init_runtime()
        residency host policy static
    stage parse: cpu pi_parse_cli(flags)
        deps [init] residency host policy static
    stage migrate: cpu pi_run_migrations()
        deps [parse] residency host policy static
    stage spawn: cpu pi_spawn_actors(flags)
        deps [migrate] residency host policy static
    stage trust: converge pi_resolve_trust(flags, TrustStore)
        deps [spawn] residency host policy telemetry_balance_latency
        guarded by pi_squared_platform
    stage model: converge pi_resolve_model(flags)
        deps [trust] residency host policy static
    stage prompt: cpu pi_build_prompt(model)
        deps [model] residency host policy static
    return StartupResult::Ready(spawn, model, prompt)
```

### 9.4 Axiom Template (Capability Declaration)

```kn
axiom pi_squared_platform_truth:
    when target("llvm")
    when capability("converge.dispatch")
    when capability("tool.result_cache")
    when capability("time.pulse")
    guarantee "pi-squared assumes LLVM compilation, converge dispatch, tool cache, and pulse support"
    fallback pi_squared_degraded_fallback
```

### 9.5 Telemetry Delta Guard Template

```kn
// Copy-paste this pattern into every test and benchmark:
fn prove_constructs_fired() -> Int:
    let patch_before = patch_journal_count()
    let converge_before = runtime_converge_telemetry_count()
    let orchestrate_before = orchestrate_stage_count()
    let actor_before = actor_scheduler_total_enqueued()

    // ... run operations ...

    let patch_delta = patch_journal_count() - patch_before
    let converge_delta = runtime_converge_telemetry_count() - converge_before
    let orchestrate_delta = orchestrate_stage_count() - orchestrate_before
    let actor_delta = actor_scheduler_total_enqueued() - actor_before

    if patch_delta < 1 and patch_before < 256:    return -10
    if converge_delta < 0:                        return -11
    if orchestrate_delta < 1:                     return -12
    if actor_delta < 1:                           return -13
    return 0
```

### 9.6 Ownership Template (For Raw Memory Operations)

```kn
fn process_data(count: Int) -> Int with Unsafe:
    let mut buf: ptr<U8> = alloc_zeroed(count, "U8")
    defer decay buf

    collapse buf:
        var i: Int = 0
        while i < count:
            mem_store(ptr_offset(buf, i, "U8"), U8(i % 256), "U8")
            i = i + 1
        0

    let result: Int = observe buf:
        var acc: Int = 0
        var i: Int = 0
        while i < count:
            acc = acc + Int(mem_load(ptr_offset(buf, i, "U8"), "U8"))
            i = i + 1
        acc

    return result  // defer runs: decay buf
```

### 9.7 Shatter Struct Template (SoA Layout)

```kn
shatter struct ToolExecutionBatch:
    tool_names: [String]
    tool_ids: [String]
    params_path: [String]
    params_offset: [Int]
    params_limit: [Int]
    signals: [Int]
    epochs: [Int]
```

---

## 10. Priority Migration Order

### Phase 1 -- Critical Fixes (This Session)

| # | Fix | File | Effort |
|---|-----|------|--------|
| 1 | Add epoch counters to all patches | `trust.kn`, `registry.kn`, `models.kn`, `key_vault.kn` | Small |
| 2 | Wire compaction pulse body | `session/compaction.kn` | Medium |
| 3 | Fix converge dispatch to use plain `fn` or real divergent lanes | `tools/registry.kn` | Small |
| 4 | Add telemetry delta guards to tests | All test files | Small |

### Phase 2 :: Structural Improvements (Next Stream)

| # | Fix | File | Effort |
|---|-----|------|--------|
| 5 | Convert startup pipeline to orchestrate | `pipeline/startup.kn` | Medium |
| 6 | Add entangle between authority/mirror worlds | All world files | Medium |
| 7 | Add resonate handlers for tool/trust state changes | New files | Small |
| 8 | Convert JSON string world state to native Kain types | `trust.kn`, `registry.kn`, `agent.kn` | Large |

### Phase 3 ~ Semantic Completeness (Future)

| # | Fix | File | Effort |
|---|-----|------|--------|
| 9 | Add axiom declarations | New `axiom.kn` | Small |
| 10 | Add ownership lifecycle for file/tool operations | `tools/` | Medium |
| 11 | Consider shatter struct for tool parameter hot paths | `tools/` | Small |
| 12 | Convert all mode dispatch to match on enum | `main.kn` | Small |

---

## APPENDIX: Quick Reference ~ pi-squared Constructs by File

| File | L0 | L1 | L2 | L3 | L4 | L5 | L6 | L7 | UI |
|------|----|----|----|----|----|----|----|----|----|
| `src/types.kn` | fn, struct, enum | | | | | | | | |
| `src/cli.kn` | fn | | | | | | | | |
| `src/main.kn` | fn, if/elif | | | | | | | | |
| `src/config/defaults.kn` | const, struct | | law x5 | | | | | | |
| `src/config/settings.kn` | fn | | | | | | | actor | |
| `src/config/trust.kn` | fn | world | patch x3, law x2 | | | | | | component |
| `src/config/markscript_loader.kn` | fn | | | | | | | | |
| `src/session/tree.kn` | fn | | | | | | | actor | |
| `src/session/compaction.kn` | fn, struct | | | | orchestrate | pulse | | | |
| `src/session/migrations.kn` | fn | | | | | | | | |
| `src/agent/agent.kn` | fn | | | | | | | actor | |
| `src/agent/events.kn` | fn, enum, struct | | | | | | | actor | |
| `src/agent/queues.kn` | fn, enum | | | | | | | | |
| `src/resources/loader.kn` | | | | | | | | actor | |
| `src/resources/skills.kn` | fn | | | | | | | | |
| `src/resources/prompts.kn` | fn | | | | | | | | |
| `src/resources/context.kn` | fn | | | | | | | | |
| `src/resources/system_prompt.kn` | fn | | | | | | | | |
| `src/tools/trait.kn` | trait, struct | | | | | | | | |
| `src/tools/registry.kn` | fn | world | patch | converge | | | | | component |
| `src/tools/read/write/edit/...` | fn | | | | | | | | |
| `src/pipeline/startup.kn` | fn | | | | | | | | |
| `src/providers/models.kn` | | world | patch | | | | | | |
| `src/providers/registry.kn` | | world | patch | | | | | | |
| `src/providers/key_vault.kn` | | world | patch x3 | | | | | | |
| `src/tui/screen.kn` | | world | | | | | | | |
| `src/tui/theme.kn` | | world | | | | | | | |
| `src/tui/input.kn` | | | | | | | | actor | |

### Legend

- Green cells (✅) = correctly placed
- Yellow cells (⚠️) = partially correct, needs improvement
- Red cells (❌) = wrong layer, needs migration
- Empty = construct not present (check if needed)
