# Orchestrate === First-Class Multi-Stage Pipeline

**Status:** Research synthesis, 2026-06-07
**Based on:** `benchmark/cases_v2/orchestrate_god.kn`, `benchmark/cases_v2/gpu_cpu_pipeline.kn`, `benchmark/cases_v2/orchestration.kn`, `blades/python/24_tet/src/resonate_py_effects.kn`, `benchmark/cases_v2/fusion_chain.kn`, `smoketest/src/semantics/orchestrate.kn`, `crates/core/src/types.rs`, `crates/core/src/ast.rs`, `crates/core/src/parser.rs`, `crates/core/src/runtime.rs`, `crates/core/src/runtime_contract.rs`, `crates/orchestrate/src/stage.rs`, `crates/orchestrate/src/planner.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `stdlib/intent.kn`, `runtime/native/src/core/stdlib_abi.c`, RULEBOOK.MD, CATALOG.MD, PULSE.MD

______________________________________________________________________

## Table of Contents

1. [What Is Orchestrate?](#what-is-orchestrate)
1. [The Core Idea >> Write Pipelines, Not Call Chains](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [Three Authoring Styles](#three-authoring-styles)
1. [Stage Kind Reference ~~ 12 Runtimes](#stage-kind-reference)
1. [Graph Metadata ->> Dependencies, Residency, Transfers, Guards, Fallback](#graph-metadata)
1. [Full Pipeline ->> Source to Runtime](#full-pipeline)
1. [The Orchestrate Patterns --- How to Use It](#the-orchestrate-patterns)
1. [Combining with Other Keywords](#combining-with-other-keywords)
1. [Under-Utilized Ideas ⁓ Creative Semantic Abuse](#under-utilized-ideas)
1. [Native C Runtime & Stdlib Telemetry](#native-c-runtime)
1. [Validation Ladder](#validation-ladder)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)
1. [Source Anchors](#source-anchors)

______________________________________________________________________

## What Is Orchestrate?

`orchestrate` is a **top-level declaration** in Kain that defines a **typed multi-stage computation pipeline** === a graph of named stages that the compiler validates and the runtime telemetry-tracks.

```kn
orchestrate orchestration_omega_pipeline(seed: Int, authority: Authority) -> Int:
    stage base: cpu mix(seed + authority.signal) when capability("cpu.scalar")
    stage tuned: converge mix(base + authority.epoch) when target("llvm")
    stage staged: gpu mix(tuned + authority.signal + 7) when capability("gpu.compute")
    stage legal: law signal_in_bounds(staged) when capability("law.invariants")
    stage mirrored: world world_score(...) when capability("world.entangle")
    stage committed: patch commit(authority, ...)
    stage final_host: dispatch dispatch_style(committed + base, authority.epoch)
    if legal == false:
        return 0
    return final_host
```

A single `orchestrate` block declares everything: the stage names, what each runs (`cpu`, `gpu`, `law`, `patch`, `world`, `converge`, `dispatch`...), what it depends on, where it runs (`host`, `device`), what data moves, which axioms guard it, what law must pass before it fires, and what happens if it fails. The compiler validates the whole DAG ‒ cycles, unknown stages, transfer/residency contradictions |-> **before a single line of code runs**.

______________________________________________________________________

## The Core Idea ⁓ Write Pipelines, Not Call Chains

### What Orchestrate IS NOT

- ❌ Not sequential `fn` calls. The compiler validates your DAG.
- ❌ Not a job queue. Stages have explicit transfer and fallback policies.
- ❌ Not CPU-only. Stages can target GPU, Python, Rust, C, Node.js.
- ❌ Not a `converge` --> that selects between spec/fast lanes. Orchestrate stages are sequential pipeline nodes.
- ❌ Not a `resonate` – that reacts to state changes. Orchestrate is explicit program steps.
- ❌ Not an `actor` - those are independent concurrent units. Orchestrate is a typed DAG.

### What Orchestrate IS

- ✅ A **typed multi-stage computation graph** - each stage produces a value, stages compose via `deps`/`after`
- ✅ A **multi-runtime dispatch** ~ `cpu`, `gpu`, `dispatch`, `converge`, `law`, `patch`, `world`, `c`, `python`, `rust`, `node`, `kain`
- ✅ A **compiler-validated DAG** ~~ cycles, unknown stages, guard type mismatches, impossible transfer/residency ~ all caught at compile time
- ✅ A **data movement contract** --> `transfer host_to_device / device_to_host / shared_view` is explicit in the stage declaration
- ✅ A **failure mode declaration** === `fallback abort | <stage> | degrade <stage>` is part of the stage, not an afterthought
- ✅ A **capability-gated dispatch** === `guarded by <axiom>` gates the stage on a compile-time-checked axiom
- ✅ A **law-enforced invariant gate** :: `requires <law_stage>` makes execution conditional on a law passing
- ✅ A **scheduling policy expression** --- `policy static | telemetry_prefer_gpu | ...` tells the runtime how to schedule
- ✅ A **telemetry-generating construct** ~~ each stage call increments counters for stage count, transfer count, fallback count, adaptive stage count
- ✅ A **runtime-contract-visible pipeline** * * * the full graph appears in `runtime_contract.json` with every stage's metadata
- ✅ A **resonate join point** :: resonate handlers can call orchestrate pipelines, making state changes trigger graph evaluation
- ✅ A **composition unit with GPU compute** \<--> `shader compute` kernels (SPIR-V/PTX) can be dispatched from inside orchestrate stages, with `comptime` block metadata feeding the dispatch manifest

### Where Orchestrate Sits

| Layer | Concept | Keyword |
|-------|---------|---------|
| LAYER 6: ORCHESTRATION | "Multi-stage pipeline?" | **`orchestrate`** |
| LAYER 6: ORCHESTRATION | "Pipeline stage guard?" | `requires <law_stage>` |
| LAYER 6: ORCHESTRATION | "Pipeline stage fallback?" | `guarded by <axiom>` / `fallback <stage>` |
| LAYER 5: CONVERGENCE | "Lane selection inside stage?" | `converge` inside `stage` |
| LAYER 2: STATE INTEGRITY | "Invariant predicate inside stage?" | `law` inside `stage` |
| LAYER 2: STATE INTEGRITY | "Journaled mutation inside stage?" | `patch` inside `stage` |

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
orchestrate <name>(<params>) -> <type>:
    stage <binding>: <runtime> <function>(<args>)
        [when <selector>]
        [after <dep> | deps [<dep>, ...]]
        [residency <host | shared | device>]
        [transfer <none | host_to_device | device_to_host | shared_view>]
        [guarded by <axiom>]
        [fallback <abort | <stage> | degrade <stage>>]
        [requires <law_stage>]
        [policy <static | telemetry_prefer_gpu | telemetry_prefer_cpu | telemetry_balance_latency>]
    ...
    return <binding>
```

The return type is optional --- defaults to `Unit` if omitted.

### Minimal vs Full

The orchestrate language supports everything from **zero metadata clauses** to **all of them**:

```kn
// MINIMAL * * * just stages and when-selectors
orchestrate minimal(seed: Int, authority: Authority) -> Int:
    stage base: cpu mix(seed) when capability("cpu.scalar")
    stage staged: gpu mix(base) when capability("gpu.compute")
    return staged

// FULL * * * every metadata clause
orchestrate full(value: Int, authority: Authority) -> Int:
    stage seed: cpu mix(value) when target("llvm") residency host
        transfer none policy static
    stage device: gpu mix(seed) after seed residency device
        transfer host_to_device guarded by machine_truth
        fallback degrade seed policy telemetry_prefer_gpu
    stage legal: law valid(device) after device residency host
        transfer device_to_host policy static
    stage committed: patch commit(authority, device) after legal
        requires legal residency host policy telemetry_balance_latency
    if legal == false:
        return seed
    return committed
```

______________________________________________________________________

## Three Authoring Styles

The benchmarks in `cases_v2/` demonstrate three distinct styles of orchestrate authoring. Which one you pick depends on how much graph metadata your pipeline needs.

### Style 1: Selector-Only (orchestration.kn)

**No graph metadata** ~ just `when capability(...)` selectors on each stage. Stages run in declaration order. The compiler still validates the whole thing, emits telemetry, and makes the pipeline runtime-contract-visible.

```kn
orchestrate orchestration_omega_pipeline(seed: Int, authority: OrchestrationAuthority) -> Int:
    stage base: cpu orchestration_mix(seed + authority.signal) when capability("cpu.scalar")
    stage tuned: converge orchestration_mix(base + authority.epoch + authority.resonance) when target("llvm")
    stage staged: gpu orchestration_mix(tuned + authority.signal + 7) when capability("gpu.compute")
    stage legal: law orchestration_signal_in_bounds(staged) when capability("law.invariants")
    stage mirrored: world orchestration_world_score(authority.signal, authority.epoch, authority.resonance) when capability("world.entangle")
    stage committed: patch orchestration_commit(authority, mod(staged + mirrored + seed, MOD), mirrored + tuned)
    stage final_host: dispatch orchestration_dispatch_style(committed + base, authority.epoch) when capability("dispatch.statement")
    if legal == false:
        return 0
    return final_host
```

From `benchmark/cases_v2/orchestration.kn`. 7 stages, 6 runtime kinds, zero `after`/`deps`/`residency`/`transfer`/`guarded by`/`fallback`/`requires`/`policy` clauses. The only metadata is `when capability(...)`.

**When to use this style:** You have a linear pipeline, no GPU data movement worries, and no need for fallback logic. The runtime contract still sees the pipeline. Telemetry still fires.

### Style 2: Minimal Guard (gpu_cpu_pipeline.kn)

**Stage kind + `when` selector + `if legal == false` guard** ‒ minimal metadata, but you use law stages as explicit barriers with conditional early-return:

```kn
orchestrate gpu_cpu_host_pipeline(value: Int) -> Int:
    stage staged: gpu gpu_cpu_mix(value) when capability("gpu.compute")
    stage legal: law gpu_cpu_signal_in_bounds(staged) when capability("law.invariants")
    if legal == false:
        return 0
    return staged
```

From `benchmark/cases_v2/gpu_cpu_pipeline.kn`. 2 stages, `gpu → law`. No transfer, no residency, no fallback metadata - all that complexity is handled by the body's conditional `if legal == false: return 0`.

**When to use this style:** Simple GPU → host validation pattern. The GPU does work, the law validates it, and the body decides what to do. No need for explicit transfer metadata when the runtime handles it.

### Style 3: Graph Metadata God Mode (orchestrate_god.kn)

**Every metadata clause** |-> dependencies, residency, transfer, axiom guards, fallback, law requires, planner policy. This is the full power of orchestrate with nothing held back:

```kn
orchestrate orchestrate_god_preflight(seed: Int, authority: OrchestrateGodAuthority) -> Int:
    stage cpu_seed: cpu orchestrate_god_mix(seed + authority.signal)
        when capability("cpu.scalar")
        residency host transfer none policy static
    stage c_shadow: c orchestrate_god_host_shadow(cpu_seed + authority.epoch)
        after cpu_seed
        residency host fallback cpu_seed policy telemetry_prefer_cpu
    stage py_shadow: python orchestrate_god_python_shadow(c_shadow + authority.drift)
        after c_shadow
        residency host fallback degrade c_shadow policy telemetry_prefer_cpu
    stage converge_lane: converge orchestrate_god_mix(py_shadow + cpu_seed)
        deps [cpu_seed, py_shadow]
        residency shared transfer shared_view policy telemetry_balance_latency
    stage gpu_lane: gpu orchestrate_god_mix(converge_lane + authority.gpu_epoch + 13)
        after converge_lane
        residency device transfer host_to_device
        guarded by orchestrate_god_silicon_truth
        fallback degrade c_shadow
        policy telemetry_prefer_gpu
    stage legal: law orchestrate_god_signal_in_bounds(gpu_lane)
        after gpu_lane
        residency host transfer device_to_host policy static
    stage committed: patch orchestrate_god_commit(authority, ...)
        after legal requires legal
        residency host policy telemetry_balance_latency
    stage final_lane: dispatch orchestrate_god_dispatch_style(committed + py_shadow, authority.epoch)
        deps [cpu_seed, c_shadow, py_shadow, committed]
        residency shared transfer shared_view policy telemetry_balance_latency
    if legal == false:
        return c_shadow
    return final_lane
```

From `benchmark/cases_v2/orchestrate_god.kn`. 8 stages, 9 runtime kinds, ALL metadata clauses. The axiom `orchestrate_god_silicon_truth` guards the GPU stage. The `fallback degrade c_shadow` provides a fallback path if the GPU can't run. The `requires legal` clause makes the patch stage wait for the law to pass.

There are two more orchestrates in the same file:

```kn
// Shard pipeline: cpu → gpu → law → world → patch → kain
orchestrate orchestrate_god_shard_pipeline(shard_score: Int, shard_phase: Int, shard_token: Int, authority: OrchestrateGodAuthority) -> Int:
    stage host_shape: cpu orchestrate_god_host_shadow(shard_score + shard_phase)
        residency host policy static
    stage gpu_tune: gpu orchestrate_god_mix(host_shape + shard_token + authority.gpu_epoch)
        after host_shape
        residency device transfer host_to_device
        guarded by orchestrate_god_silicon_truth
        fallback degrade host_shape policy telemetry_prefer_gpu
    stage phase_ok: law orchestrate_god_phase_in_bounds(shard_phase)
        after gpu_tune
        residency host transfer device_to_host policy static
    stage mirror_score: world orchestrate_god_world_score(...)
        after phase_ok requires phase_ok
        residency shared transfer shared_view policy telemetry_balance_latency
    stage committed: patch orchestrate_god_commit(authority, ...)
        deps [gpu_tune, mirror_score] requires phase_ok
        residency host policy telemetry_balance_latency
    stage final_lane: kain orchestrate_god_dispatch_style(committed + shard_phase, authority.epoch)
        after committed residency host policy static
    if phase_ok == false:
        return host_shape
    return final_lane

// Reconcile pipeline: gpu → cpu → law → world → patch → dispatch
//   with fallback abort on the GPU stage * * * most aggressive failure mode
orchestrate orchestrate_god_reconcile_pipeline(value: Int, authority: OrchestrateGodAuthority) -> Int:
    stage device_probe: gpu orchestrate_god_mix(value + authority.gpu_epoch)
        residency device transfer host_to_device
        guarded by orchestrate_god_silicon_truth
        fallback abort                    // ← abort the entire pipeline if GPU fails
        policy telemetry_prefer_gpu
    stage host_return: cpu orchestrate_god_host_shadow(device_probe + authority.signal)
        after device_probe
        residency host transfer device_to_host policy telemetry_prefer_cpu
    stage handoff_ok: law orchestrate_god_gpu_handoff_ok(host_return)
        after host_return residency host policy static
    stage world_snapshot: world orchestrate_god_world_score(...)
        after handoff_ok requires handoff_ok
        residency shared transfer shared_view policy telemetry_balance_latency
    stage committed: patch orchestrate_god_commit(authority, ...)
        deps [host_return, world_snapshot] requires handoff_ok
        residency host policy telemetry_balance_latency
    stage final_lane: dispatch orchestrate_god_dispatch_style(committed + value, authority.epoch)
        after committed
        residency shared transfer shared_view policy telemetry_balance_latency
    if handoff_ok == false:
        return value
    return final_lane
```

**When to use this style:** Any pipeline with GPU acceleration, cross-platform runtimes, or mission-critical failure handling. The graph metadata tells the compiler and runtime exactly what you intend.

______________________________________________________________________

## Stage Kind Reference >> 12 Runtimes

### Silicon-Native (7 kinds)

These run in the Kain runtime -- no foreign bridge. `is_silicon_native()` returns `true`.

| Kind | Keyword | Example |
|------|---------|---------|
| CPU | `cpu` | `stage x: cpu process(value)` |
| GPU | `gpu` | `stage x: gpu compute(value)` |
| Dispatch | `dispatch` | `stage x: dispatch dispatch_fn(value)` |
| Converge | `converge` | `stage x: converge mix(value)` |
| Law | `law` | `stage x: law in_bounds(value)` |
| Patch | `patch` | `stage x: patch commit(value)` |
| World | `world` | `stage x: world world_score(...)` |

### Primary Interop (2 kinds)

First-class import support. `is_primary_interop()` returns `true`.

| Kind | Keyword | Bridge Required |
|------|---------|-----------------|
| C | `c` | libclang-extracted C functions (`stage x: c host_shadow(value)`) |
| Python | `python` | Python bridge (`stage x: python module::fn(value)`) |

### Compatibility Adapters (2 kinds)

Foreign runtime bridges. `is_compat_adapter()` returns `true`.

| Kind | Keyword | Bridge Required |
|------|---------|-----------------|
| Rust | `rust` | Native Rust functions via `kain-host` |
| Node | `node` | Node.js bridge (`js_call`) |

### Ungrouped

| Kind | Keyword | Notes |
|------|---------|-------|
| Kain | `kain` | No group marker => just a standard Kain function call |

### How Stage Kinds Actually Dispatch

All silicon-native stages (`cpu`, `gpu`, `dispatch`, `converge`, `law`, `patch`, `world`) call through the same `call_named_function` path in the interpreter \<--> the runtime tag is telemetry metadata, not dispatch logic. The `c` and `kain` kinds also share this path. For non-silicon-native:

- `rust` → calls native Rust functions registered via `kain-host`
- `python` → calls through the Python bridge (`py_call`)
- `node` → calls through the Node.js bridge (`js_call`)

______________________________________________________________________

## Graph Metadata ~> Dependencies, Residency, Transfers, Guards, Fallback

### Quick Reference

| Clause | Example | What It Does |
|--------|---------|-------------|
| `when capability("key")` | `when capability("gpu.compute")` | Gated by runtime capability check |
| `when target("triple")` | `when target("llvm")` | Gated by compilation target |
| `after <stage>` | `after gpu_lane` | Declares a single dependency |
| `deps [a, b]` | `deps [base, tuned]` | Declares multiple dependencies |
| `residency <kind>` | `residency device` | Where the data/execution lives |
| `transfer <kind>` | `transfer host_to_device` | Data movement policy |
| `guarded by <axiom>` | `guarded by silicon_truth` | Gates execution on an axiom |
| `fallback <target>` | `fallback degrade c_shadow` | What happens on failure |
| `requires <law_stage>` | `requires legal` | Law must pass before stage runs |
| `policy <policy>` | `policy telemetry_prefer_gpu` | Scheduling policy |

### Residency

| Value | Meaning |
|-------|---------|
| `host` | Data/execution lives on the CPU host |
| `shared` | Data is accessible from both host and device |
| `device` | Data/execution lives on the GPU/accelerator |

### Transfer

| Value | Meaning | Incompatible With |
|-------|---------|-------------------|
| `none` | No data movement | ... |
| `host_to_device` | Move from host to device before executing | `residency host` |
| `device_to_host` | Move from device to host after executing | `residency device` |
| `shared_view` | Create a shared view accessible from both | `residency host` or `residency device` |

### Fallback

| Value | Meaning |
|-------|---------|
| `abort` | Pipeline fails immediately (used in `orchestrate_god_reconcile_pipeline`) |
| `<stage_name>` | Redirect to another stage's result |
| `degrade <stage_name>` | Redirect with a degraded quality signal |

### Planner Policy

| Value | Adaptive | Meaning |
|-------|----------|---------|
| `static` | ❌ | Fixed scheduling, no re-planning |
| `telemetry_prefer_gpu` | ✅ | Prefer GPU if available |
| `telemetry_prefer_cpu` | ✅ | Prefer CPU if available |
| `telemetry_balance_latency` | ✅ | Balance latency across stages |

Adaptive policies increment `orchestrate_adaptive_stage_count` for telemetry.

______________________________________________________________________

## Full Pipeline - Source to Runtime

### What Happens When You Write an Orchestrate

```
parser.rs:parse_orchestrate()
  → OrchestrateDef { name, params, return_type, body }
  → body stmts: parse_orchestrate_stage() for each stage

types.rs:check_orchestrate()
  → Wraps body as a function via orchestrate_function_view
  → collect_orchestrate_stage_descriptors()
      → Validates: stages before locals, no nested items
  → build_orchestrate_graph_plan()
      → Validates: guard is an axiom, transfer/residency compatibility
      → OrchestrateGraphPlan::validate()
          → Validates: no cycles, no unknown deps, no duplicate names
  → TypedOrchestrate { ast, resolved_type, stages, graph }

runtime_contract.rs:collect_orchestration_contracts()
  → Emits RuntimeOrchestrationContract with full stage list
  → Adds capability "orchestrate.pipeline"

codegen_llvm/mod.rs:compile_orchestrate()
  → compile_named_callable for the body (same as fn/patch)
  → For each stage call: emits abi_orchestrate_stage_begin_graph()
    with full metadata, compile_direct_call, abi_orchestrate_stage_end_i64()

runtime.rs:register_orchestrate_value()
  → Registers in symbol table as Value::Orchestrate(name)
  → Called like a normal function
```

### What the Compiler Validates

| Check | What Fails |
|-------|-----------|
| Guard is an axiom | `guarded by plus_one` where `plus_one` is a function → "guard must reference an axiom" |
| Transfer/residency pair | `transfer host_to_device` + `residency host` → "incompatible" |
| No cycles | Circular `after` → "dependency cycle" |
| No unknown stages | `after nonexistent` → "depends on unknown stage" |
| No duplicate names | Two stages called `base` → "duplicate stage" |
| Stages before locals | Variable decl before stage → "must declare stage steps before local computation" |

______________________________________________________________________

## The Orchestrate Patterns => How to Use It

### Pattern 1: Selector-Only Pipeline (orchestration.kn style)

No metadata clauses ⁓ just stages with `when capability(...)`. The simplest way to write orchestrates:

```kn
orchestrate orchestration_shard_pipeline(shard_score: Int, shard_phase: Int, shard_token: Int, authority: OrchestrationAuthority) -> Int:
    stage tuned: gpu orchestration_mix(shard_score + shard_phase + authority.signal) when capability("gpu.compute")
    stage legal: law orchestration_phase_in_bounds(shard_phase) when capability("law.invariants")
    stage committed: patch orchestration_commit(authority, tuned, shard_token + shard_phase)
    stage final_lane: kain orchestration_dispatch_style(committed + shard_phase, authority.epoch) when capability("cpu.scalar")
    if legal == false:
        return 0
    return final_lane
```

From `benchmark/cases_v2/orchestration.kn`. 4 stages, 3 runtime kinds + kain. No graph metadata. The `if legal == false` guard handles the law failure case inline.

**Full 7-stage version from the same file:**

```kn
orchestrate orchestration_omega_pipeline(seed: Int, authority: OrchestrationAuthority) -> Int:
    stage base: cpu orchestration_mix(seed + authority.signal) when capability("cpu.scalar")
    stage tuned: converge orchestration_mix(base + authority.epoch + authority.resonance) when target("llvm")
    stage staged: gpu orchestration_mix(tuned + authority.signal + 7) when capability("gpu.compute")
    stage legal: law orchestration_signal_in_bounds(staged) when capability("law.invariants")
    stage mirrored: world orchestration_world_score(authority.signal, authority.epoch, authority.resonance) when capability("world.entangle")
    stage committed: patch orchestration_commit(authority, staged + mirrored + seed, mirrored + tuned)
    stage final_host: dispatch orchestration_dispatch_style(committed + base, authority.epoch) when capability("dispatch.statement")
    if legal == false:
        return 0
    return final_host
```

**The telemetry loop that proves it fired:**

```kn
let teleport_base = runtime_machine_teleport_count()
let round = 0
while round < iterations:
    let omega = orchestration_omega_pipeline(mod(acc + old_cell + round + 17, MOD), authority)
    // ... build shard, teleport it, run shard_pipeline ...
    round = round + 1

let runtime_shape_ok = (
    patch_journal_count() >= 1
    and entangle_propagation_count() >= iterations
    and converge_mismatch_count() == 0
    and orchestrate_stage_count() >= iterations * 10   // 10 stage calls per iteration
)
```

The telemetry check `orchestrate_stage_count() >= iterations * 10` proves the pipeline stages actually executed the expected number of times.

### Pattern 2: Law-Guarded GPU Pipeline (gpu_cpu_pipeline.kn style)

GPU stage followed by a law check with conditional early-return:

```kn
// The core pipeline: GPU mix → law guard
orchestrate gpu_cpu_host_pipeline(value: Int) -> Int:
    stage staged: gpu gpu_cpu_mix(value) when capability("gpu.compute")
    stage legal: law gpu_cpu_signal_in_bounds(staged) when capability("law.invariants")
    if legal == false:
        return 0
    return staged

// The converge function the GPU stage calls:
converge gpu_cpu_mix(value: Int) -> Int:
    spec reference:
        return gpu_cpu_mix_scalar(value)
    fast llvm_lane when target("llvm"):
        return ((value * 41) + 29) % MODULUS

// The shader compute kernel (SPIR-V / CUDA PTX):
shader compute CpuGpuBridgeKernel(id: UVec3) -> Void workgroup(8, 1, 1):
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1

    comptime:
        let compute = (
            [32, 1, 1],
            [
                ("src", "u32", ["dispatch.x"], "input", "kain.shared.buffer"),
                ("dst", "u32", ["dispatch.x"], "output", "kain.shared.buffer"),
            ],
            [],
        )

    let lane = src[id.x]
    dst[id.x] = lane + UInt(3)
    return
```

From `benchmark/cases_v2/gpu_cpu_pipeline.kn`. This pattern shows the **full GPU stack** in one file:

1. A `shader compute` kernel (SPIR-V via `StorageBuffer`, `UVec3`, `workgroup(8,1,1)`)
1. A `converge` function for the GPU stage to call
1. An `orchestrate` that chains `gpu → law`
1. A telemetry probe that verifies the pipeline fired

The `comptime` block inside the shader declares the dispatch metadata (`dispatch_size`, `bindings`, `buffer types`) that the host-side CUDA/SPIR-V runtime consumes when launching the kernel.

### Pattern 3: Graph Metadata God Mode (orchestrate_god.kn style)

The full pipeline with ALL metadata clauses. Three separate orchestrates in one file, each with different topology:

```
orchestrate_god_preflight:
  cpu → c → python → converge → gpu → law → patch → dispatch
  (8 stages, 9 runtime kinds, all metadata)

orchestrate_god_shard_pipeline:
  cpu → gpu → law → world → patch → kain
  (6 stages, 6 runtime kinds, transfer + guard + requires)

orchestrate_god_reconcile_pipeline:
  gpu → cpu → law → world → patch → dispatch
  (6 stages, 6 runtime kinds, fallback abort on GPU)
```

From `benchmark/cases_v2/orchestrate_god.kn`. See [Style 3: Graph Metadata God Mode](#style-3-graph-metadata-god-mode-orchestrate_godkn) above for the full code.

The telemetry loop that proves all three pipelines fired correctly:

```kn
let stage_base = orchestrate_stage_count()
let transfer_base = orchestrate_transfer_count()
let fallback_base = orchestrate_fallback_count()
let adaptive_base = orchestrate_adaptive_stage_count()
let teleport_base = runtime_machine_teleport_count()

let round = 0
while round < iterations:
    let preflight = orchestrate_god_preflight(mod(acc + old_cell + round + 31, MOD), authority)
    let shard_lane = orchestrate_god_shard_pipeline(...)
    let reconciled = orchestrate_god_reconcile_pipeline(mod(preflight + shard_lane + old_cell, MOD), authority)
    round = round + 1

let runtime_shape_ok = (
    patch_journal_count() >= 1
    and stage_delta >= iterations * 20      // 20 stage calls across 3 pipelines
    and transfer_delta >= iterations * 8     // 8 transfers per iteration
    and fallback_delta >= iterations * 4     // 4 fallback metadata clauses
    and adaptive_delta >= iterations * 12    // 12 adaptive policies
)
```

### Pattern 4: Effects Engine (24-tet style)

9 stages processing a real-time audio note => see `blades/python/24_tet/src/resonate_py_effects.kn` for the full code. The key structure:

```kn
orchestrate fx_process_note(slot: Int, velocity: Int, frame: Int, world: FxWorld) -> Int:
    stage lfo1_stage: cpu lfo_sin(...)                      // LFO 1
    stage lfo2_stage: cpu lfo_tri(...)                      // LFO 2
    stage chorus_stage: converge wet_dry_mix(...)            // Chorus via converge
    stage delay_stage: converge wet_dry_mix(...)             // Delay via converge
    stage law_check: law fx_mix_in_bounds(...)               // Law: distortion in bounds
    stage filter_law: law fx_filter_cutoff_in_bounds(...)    // Law: filter in bounds
    stage world_score: world (...)                           // Combine into hash
    stage patch_stage: patch fx_set_whole_state(...)         // Commit to world
    stage dispatch_stage: dispatch (...)                     // Final dispatch
    if world.fx_bypass != 0:
        return slot + velocity + frame
    return dispatch_stage
```

Two different law stages guard different downstream stages => `law_check` guards `patch_stage`, `filter_law` guards `world_score` and `dispatch_stage`.

### Pattern 5: Resonate → Orchestrate Join Point (fusion_chain.kn)

From `benchmark/cases_v2/fusion_chain.kn` ‒ a **resonate handler** triggers an **orchestrate pipeline** which updates **world state**:

```kn
resonate FusionAuthority.signal dampen 0 ms:
    FusionAuthority.last_old = resonate_old_i64
    FusionAuthority.last_new = resonate_new_i64
    FusionAuthority.shadow = fusion_signal_pipeline(          // ← calls orchestrate
        resonate_new_i64 + FusionAuthority.tick,
        FusionAuthority.tick
    )

orchestrate fusion_signal_pipeline(value: Int, tick: Int) -> Int:
    stage host_mix: cpu fusion_mix(value + tick)
        when capability("cpu.scalar")
        residency host transfer none policy telemetry_prefer_cpu
    stage fast_mix: converge fusion_fast_mix(host_mix, tick)
        deps [host_mix]
        residency host policy static
    return fast_mix
```

The chain: **patch writes → resonate fires → orchestrate runs → world shadow updates**. This is the critical join point where reactive state changes flow through a typed pipeline back into world state.

______________________________________________________________________

## Combining with Other Keywords

### Orchestrate + Shader Compute (SPIR-V / PTX / CUDA)

GPU stages in orchestrate call functions that may dispatch compute shaders. The shaders are declared as `shader compute` items with `StorageBuffer`, `UVec3`, `workgroup`, and `comptime` metadata:

```kn
shader compute CpuGpuBridgeKernel(id: UVec3) -> Void workgroup(8, 1, 1):
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1
    comptime:
        let compute = ([32, 1, 1], [("src", "u32", [...], "input", "kain.shared.buffer"), ...], [])
    let lane = src[id.x]
    dst[id.x] = lane + UInt(3)
    return

orchestrate pipeline(value: Int) -> Int:
    stage staged: gpu compute(value) when capability("gpu.compute")
    ...
```

The shader's `comptime` block feeds the dispatch manifest that the host runtime consumes. The dispatch statement itself (`dispatch "shader::Key::compute" [x, y, z]`) can be called from the orchestrate body or from an external loop.

### Orchestrate + Law

Law stages validate invariants; `requires` clauses gate downstream stages. See Pattern 2 above.

### Orchestrate + Patch

Patch stages perform journaled world mutations. The orchestrate telemetry proves patch fire counts alongside stage counts.

### Orchestrate + World

World stages query world state (read-only projection). Used in the 24-tet engine for composite effect hashing.

### Orchestrate + Converge

Converge stages inside orchestrate select between spec/fast lanes. The `verify random(N)` clause validates correctness.

### Orchestrate + Axiom

Axioms gate GPU/dispatch stages via `guarded by`:

```kn
axiom silicon_truth:
    when target("llvm")
    when capability("gpu.compute")
    when capability("orchestrate.graph")
    guarantee "orchestrate may own silicon residency, transfer, law gates, and fallback policy"
    fallback fallback_fn

orchestrate pipeline(...) -> Int:
    stage gpu_lane: gpu process(...)
        guarded by silicon_truth
        fallback degrade cpu_lane
```

### Orchestrate + Resonate

The critical join point -- resonate handlers call orchestrate pipelines (Pattern 5).

### Orchestrate + Teleport + Shatter

Pipelines can process teleported shatter structs:

```kn
shatter struct OrchestrateGodShard:
    bias: Int
    phase: Int
    token: Int
    gpu_hint: Int
    alive: Bool

// Inside the benchmark loop:
let shard = OrchestrateGodShard { bias: ..., phase: ..., token: ..., gpu_hint: ..., alive: true }
let moved = teleport shard from Authority to Mirror via bus
let shard_lane = orchestrate_god_shard_pipeline(orchestrate_god_shard_score(moved), moved.phase, moved.token + moved.gpu_hint, authority)
```

### Orchestrate + Dispatch (Host-Side Compute)

The `dispatch` keyword launches compute kernels (CUDA/SPIR-V):

```kn
dispatch "shader::OrchestrateGodKernel::compute" [17, 4, 1]
```

The launch key matches the `shader compute`'s comptime metadata. The runtime looks up the shader bundle, validates workgroup/dispatch sizes against the manifest, and launches.

______________________________________________________________________

## Under-Utilized Ideas -- Creative Semantic Abuse

These are patterns that the benchmark infrastructure exercises but that haven't found their way into real blades yet.

### 1. `fallback abort` \_\_\_ Aggressive GPU Failure

Only the orchestrate_god's `orchestrate_god_reconcile_pipeline` uses `fallback abort`. This is the most aggressive failure mode ->> if the guard fails, the pipeline aborts entirely with no degraded fallback. For production GPU pipelines where degraded results are worse than no results, this is the right choice.

### 2. Python + C + Rust + Node Stages Inside Orchestrate

These appear in the smoketest and orchestrate_god benchmark but in no production blade. A multi-language orchestrate pipeline (e.g., C for hot-path audio, Python for ML, Rust for security) would be the most expressive demonstration of Kain's cross-runtime orchestration.

### 3. Dispatch as an Orchestrate Stage (Not Just Host-Side)

The `dispatch` stage kind exists in the type system (`OrchestrateStageKind::Dispatch`) but no orchestrate block in the benchmarks uses `stage x: dispatch "kernel"` as a stage kind :: dispatch is always called as a host-side statement (e.g., in the telemetry loop body). Making dispatch a stage would let it participate in the graph's dependency, transfer, and fallback metadata.

### 4. Three+ Law Stages in One Pipeline

The 24-tet engine uses 2 law stages. The benchmark orchestrates use 1-2. No example uses 3+ law checks at different pipeline stages (pre-compute, mid-pipeline, pre-commit). For a pipeline with multiple independent invariants (e.g., input bounds, intermediate result validity, output bounds), 3+ law stages would be natural.

### 5. Orchestrate as a First-Class Value

`Value::Orchestrate(name)` exists in the interpreter. You could pass orchestrates as function pointers, store them in structs, or select them dynamically at runtime. No example demonstrates this.

### 6. Nested Orchestrate (Pipeline Composition)

One orchestrate calling another as a stage works (they compile to plain function calls) but no benchmark composes pipelines that way. Pattern: pipeline A calls pipeline B as a stage, pipeline B calls pipeline C.

### 7. Dynamic Policy Switching Based on Telemetry

The planner policies exist (`telemetry_prefer_gpu`, `telemetry_balance_latency`) but they're static per stage. A pulse-driven telemetry probe that measures GPU load and adjusts policy dynamically would be the next step in adaptive orchestration.

______________________________________________________________________

## Native C Runtime & Stdlib Telemetry

### What the Native Runtime Tracks

The C runtime (`stdlib_abi.c`) maintains **16 global variables** per orchestrate stage:

| Variable | Type | What It Records |
|----------|------|-----------------|
| `stage_count` | `int64_t` | Total stage executions (incremented by `abi_orchestrate_stage_begin_graph`) |
| `transfer_count` | `int64_t` | Stages with meaningful transfer policy |
| `fallback_count` | `int64_t` | Stages with meaningful fallback policy |
| `adaptive_stage_count` | `int64_t` | Stages with non-static planner policy |
| `last_runtime` | `char[128]` | Last stage's runtime kind (`"gpu"`, `"cpu"`, etc.) |
| `last_function` | `char[256]` | Last stage's function name |
| `last_dependencies` | `char[512]` | Last stage's dependency list (comma-separated) |
| `last_residency` | `char[64]` | Last stage's residency |
| `last_transfer` | `char[64]` | Last stage's transfer policy |
| `last_guard` | `char[128]` | Last stage's axiom guard name |
| `last_fallback` | `char[128]` | Last stage's fallback target |
| `last_requires` | `char[128]` | Last stage's required law stage |
| `last_policy` | `char[128]` | Last stage's planner policy |

These are populated by `abi_orchestrate_stage_begin_graph(const char* runtime_name, const char* function_name, const char* selector_name, const char* dependency_names, const char* residency_name, const char* transfer_name, const char* guard_name, const char* fallback_name, const char* requires_name, const char* policy_name)` === a single ABI entry point that records all metadata and increments all counters.

### Telemetry API (from `stdlib/intent.kn`)

```kn
// Counters
orchestrate_stage_count()        -> Int    // Total stage executions
orchestrate_transfer_count()     -> Int    // Stages with transfers
orchestrate_fallback_count()     -> Int    // Stages with fallbacks
orchestrate_adaptive_stage_count() -> Int  // Stages with adaptive policies

// Last-stage metadata (for debugging / telemetry probes)
orchestrate_last_runtime()       -> String  // "gpu"
orchestrate_last_function()      -> String  // "orchestrate_god_mix"
orchestrate_last_selector()      -> String  // "capability(\"gpu.compute\")"
orchestrate_last_dependencies()  -> String  // "cpu_seed,c_shadow"
orchestrate_last_residency()     -> String  // "device"
orchestrate_last_transfer()      -> String  // "host_to_device"
orchestrate_last_guard()         -> String  // "orchestrate_god_silicon_truth"
orchestrate_last_fallback()      -> String  // "degrade c_shadow"
orchestrate_last_requires()      -> String  // "legal"
orchestrate_last_policy()        -> String  // "telemetry_prefer_gpu"

// Status functions
orchestrate_stage_status(status: Int) -> Int      // passthrough
orchestrate_merge_status(left: Int, right: Int) -> Int  // first non-zero
```

### Runtime Contract

When any orchestrate exists, the program emits:

- **Capability:** `orchestrate.pipeline` \_\_\_ auto-added to the runtime contract
- **Contract entry per orchestrate:** `RuntimeOrchestrationContract { name, return_type, graph_mode, adaptive_policy, stages: Vec<RuntimeOrchestrationStageContract> }` ~~ each stage with all its metadata (runtime kind, function, deps, residency, transfer, guard, fallback, requires, policy, silicon_native flag)

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck ~> catches graph errors (cycles, unknown stages, bad guards)
kain check <file.kn> --target llvm

# 2. Compile + run ~~ orchestrate executes as a typed pipeline
kain run <file.kn> --target llvm

# 3. Verify telemetry counters ~> prove stages fired
# Pattern from the benchmarks:
#   let before = orchestrate_stage_count()
#   # run pipeline
#   if orchestrate_stage_count() - before < expected_stages: return error

# 4. For orchestrate with GPU compute:
kain gpu-artifacts <kernel.comp.kn> --target spirv --output .kain/gpu/kernel.spv
# Or for CUDA:
#   PTX is generated through the LLVM CUDA target

# 5. For multi-language stages (Python, Rust, C, Node):
#   Ensure bridges are registered before calling orchestrate

# 6. Check runtime contract
kain build <file.kn> --target llvm
# runtime_contract.json shows "orchestrations" with full stage metadata
```

______________________________________________________________________

## Anti-Patterns

### ❌ Sequential `fn` Calls Instead of Orchestrate

```kn
// WRONG ~> no graph metadata, no telemetry, no validation
fn pipeline(value: Int) -> Int:
    let a = step1(value)
    let b = step2(a)
    return step3(b)

// RIGHT ⁓ typed pipeline with metadata
orchestrate pipeline(value: Int) -> Int:
    stage first: cpu step1(value) residency host policy static
    stage second: cpu step2(first) after first residency host policy static
    stage third: cpu step3(second) after second residency host policy static
    return third
```

### ❌ Law Stage Without `requires`

```kn
// WRONG => declares the law but doesn't gate downstream
stage legal: law in_bounds(value) residency host policy static
stage work: cpu process(value) deps [legal] residency host policy static
// work runs even if law fails!

// RIGHT ~> law gates the stage
stage legal: law in_bounds(value) residency host policy static
stage work: cpu process(value) deps [legal] requires legal residency host policy static
```

### ❌ Dependency Cycle

```kn
// WRONG === compiler catches this
stage left: cpu step1(value) after right
stage right: cpu step2(value) after left

// ERROR: "stage 'left' participates in a dependency cycle"
```

### ❌ Guard with a Function Instead of Axiom

```kn
// WRONG --> guard must reference an axiom
stage work: gpu process(value) guarded by my_function

// ERROR: "guard 'my_function' must reference an axiom, found function"
```

### ❌ Impossible Transfer + Residency

```kn
// WRONG ~> compiler catches this
stage work: gpu process(value) residency host transfer host_to_device

// ERROR: "transfer 'host_to_device' is incompatible with residency 'host'"
```

### ❌ Stage Calls After Local Computation

```kn
// WRONG - compiler catches this
orchestrate pipeline(value: Int) -> Int:
    let temp = value + 1         // non-stage code first!
    stage first: cpu step(temp)  // ERROR

// ERROR: "must declare stage steps before local computation"
```

### ❌ Not Proving It Fired

```kn
// WRONG ‒ no proof the orchestrate executed
fn run():
    my_pipeline(42)

// RIGHT ->> telemetry proves it
fn run() -> Int:
    let before = orchestrate_stage_count()
    my_pipeline(42)
    if orchestrate_stage_count() - before < 1:
        return -1
    return 0
```

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `orchestrate name(params) -> Type: { stage bind: runtime fn(args) [metadata…] … return expr }` |
| **Return type** | Optional :: defaults to `Unit` |
| **Stage kinds** | `cpu`, `gpu`, `dispatch`, `converge`, `law`, `patch`, `world`, `kain`, `c`, `python`, `rust`, `node` (12 total) |
| **Selectors** | `when capability("key")` or `when target("triple")` |
| **Dependencies** | `after <name>` (repeatable) or `deps [a, b, c]` |
| **Residency** | `host`, `shared`, `device` |
| **Transfer** | `none`, `host_to_device`, `device_to_host`, `shared_view` |
| **Guard** | `guarded by <axiom_name>` |
| **Fallback** | `abort`, `<stage_name>`, `degrade <stage_name>` |
| **Requires** | `requires <law_stage_name>` |
| **Policy** | `static`, `telemetry_prefer_gpu`, `telemetry_prefer_cpu`, `telemetry_balance_latency` |
| **Validation** | Cycles, unknown stages, duplicate names, guard type, transfer/residency, stages-before-locals |
| **LLVM codegen** | `compile_named_callable` + `abi_orchestrate_stage_begin_graph` per stage |
| **Capability** | `orchestrate.pipeline` |
| **Stdlib** | `std::intent` ~ 4 counters, 11 last-\* fields, 2 status functions |
| **Canonical examples** | `benchmark/cases_v2/orchestrate_god.kn` (full metadata), `benchmark/cases_v2/orchestration.kn` (selector-only), `benchmark/cases_v2/gpu_cpu_pipeline.kn` (minimal guard) |
| **Effects engine** | `blades/python/24_tet/src/resonate_py_effects.kn` (9-stage pipeline) |
| **Resonate join point** | `benchmark/cases_v2/fusion_chain.kn` (resonate → orchestrate → world) |
| **Dispatcher** | `crates/core/src/runtime.rs` * * * `execute_stage_call()` routes 12 runtimes |

______________________________________________________________________

## Source Anchors

### Primary Examples (Benchmark)

| File | Content |
|------|---------|
| `benchmark/cases_v2/orchestrate_god.kn` | **God mode:** 3 orchestrates, 9 stage kinds, all metadata clauses, pulse, shatter, teleport, shader compute kernel (SPIR-V/PTX) |
| `benchmark/cases_v2/gpu_cpu_pipeline.kn` | **Minimal guard:** 2-stage `gpu → law` pipeline, converge, shader compute kernel, CUDA dispatch probe |
| `benchmark/cases_v2/orchestration.kn` | **Selector-only:** 2 orchestrates with `when capability(...)` selectors, shatter, teleport, shader compute kernel |
| `blades/python/24_tet/src/resonate_py_effects.kn` | **Effects engine:** 9-stage orchestrate, 2 law checks, 2 converge lanes, conditional bypass, Python interop |
| `benchmark/cases_v2/fusion_chain.kn` | **Join point:** resonate → orchestrate → world shadow update, 2-stage converge pipeline |

### Core Crate

| File | Content |
|------|---------|
| `crates/core/src/ast.rs` | `OrchestrateDef` |
| `crates/core/src/parser.rs` | `parse_orchestrate()`, `parse_orchestrate_stage()`, `parse_orchestrate_graph_metadata()` |
| `crates/core/src/types.rs` | `check_orchestrate()`, `TypedOrchestrate`, `OrchestrateStageDescriptor`, graph validation |
| `crates/core/src/runtime.rs` | `execute_orchestrate_call()`, `execute_stage_call()` (12-runtime dispatch) |
| `crates/core/src/runtime_contract.rs` | `RuntimeOrchestrationContract`, `RuntimeOrchestrationStageContract`, `orchestrate.pipeline` capability |

### Orchestrate Crate

| File | Content |
|------|---------|
| `crates/orchestrate/src/stage.rs` | 12 `OrchestrateStageKind`s, `OrchestrateResidency`, `OrchestrateTransfer`, `OrchestrateFallback` |
| `crates/orchestrate/src/graph.rs` | `OrchestrateGraphPlan`, `OrchestrateGraphValidation` (DAG validation, cycle detection) |
| `crates/orchestrate/src/planner.rs` | `OrchestratePlannerPolicy` (static, telemetry_prefer\_\*) |

### Native C Runtime

| File | Content |
|------|---------|
| `runtime/native/include/stdlib_abi.h` | 18 ABI declarations: begin/begin_ex/begin_graph, end_i64, count, 11 last\_\* fields, transfer/fallback/adaptive counts |
| `runtime/native/src/core/stdlib_abi.c` | 16 global state variables, `abi_orchestrate_stage_begin_graph` (full metadata recording), `abi_orchestrate_stage_end_i64` (passthrough) |
