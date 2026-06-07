# Converge — Multi-Strategy Selection with Verification

**Status:** Research synthesis, 2026-06-07
**Based on:** `crates/core/src/parser.rs`, `crates/core/src/ast.rs`, `crates/core/src/types.rs`, `crates/core/src/runtime_contract.rs`, `crates/core/src/runtime.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `runtime/native/include/converge.h`, `runtime/native/src/core/converge.c`, `stdlib/intent.kn`, `stdlib/runtime.kn`, `smoketest/src/semantics/converge.kn`, `blades/experiments/convergence/src/` (6 files, the canonical hack-side proof), `blades/test/converge-autotune-probe/src/main.kn`, `benchmark/cases_v2/fusion_chain.kn`, `benchmark/cases/scalar_mix/main.kn`, `benchmark/cases/semantic_singularity_converge_only/main.kn`, `blades/python/24_tet/src/resonate_py_effects.kn`, `stdlib/random.kn` (stdlib `xoshiro_scramble` converge), CATALOG.MD, RULEBOOK.MD, lang-semantics SKILL

---

## Table of Contents

1. [What is Converge?](#what-is-converge)
2. [The Core Idea — Spec + Alternatives + Verification](#the-core-idea)
3. [Syntax Reference](#syntax-reference)
4. [How It Works — Full Pipeline](#how-it-works)
5. [The Two-Level Architecture](#two-level-architecture)
6. [Selectors — Target and Capability](#selectors)
7. [Verify Random — The Arbitration Mechanism](#verify-random)
8. [The Interpreter Lane Selection Algorithm](#interpreter-lane-selection)
9. [LLVM Codegen — Static and Dynamic Dispatch](#llvm-codegen)
10. [Native C Autotune Cache](#native-c-autotune-cache)
11. [Stdlib Telemetry and Runtime API](#stdlib-telemetry)
12. [The Obvious Use Case — Performance Lane Selection](#obvious-use-case)
13. [The Hack Side — The Convergence Rat Experiment](#the-rat-experiment)
14. [Creative Semantic Abuse — New Under-Utilized Ideas](#creative-abuse)
15. [Combining with Other Keywords](#combining-with-other-keywords)
16. [Real-World Usage Across the Repo](#real-world-usage)
17. [Z3 Proofs](#z3-proofs)
18. [Validation Ladder](#validation-ladder)
19. [Source Anchors](#source-anchors)
20. [Anti-Patterns](#anti-patterns)
21. [Quick Reference](#quick-reference)

---

## What is Converge?

`converge` is a **top-level declaration** in Kain that establishes a **specification with one or more alternative implementations** — a reference (spec) lane plus platform-selected fast lanes — with **verification** that the alternatives produce equivalent results.

```kn
converge mix(value: Int) -> Int:
    spec reference:
        return mix_scalar(value)
    fast llvm_lane when target("llvm"):
        return ((value * 31) + 7) % MODULUS
    fast avx2_lane when capability("cpu.x86.avx2"):
        return ((value * 31) + 7) % MODULUS
    verify random(8)
```

The key insight: **`converge` is not just a performance abstraction.** It is a **multi-strategy selection construct** where the `spec` lane is the ground truth, the `fast` lanes are competing alternatives, and `verify random(N)` is the arbitration mechanism. The selector (`target("llvm")`, `capability("cpu.x86.avx2")`) determines which lane is active. The verify clause proves all active lanes produce the same result for random inputs.

---

## The Core Idea

### What Converge IS NOT

- ❌ Not `if cfg!(target_arch)` inside a function (`converge` is compiler-owned dispatch)
- ❌ Not a virtual method dispatch (that's `impl` + `trait`)
- ❌ Not a GPU dispatcher (that's `dispatch "key" [x, y, z]`)
- ❌ Not a function overload (different signatures)
- ❌ Not a runtime polymorphic dispatch (all lanes share the same signature)

### What Converge IS

- ✅ A **specification-plus-alternatives** declaration — one `spec`, one or more `fast` lanes
- ✅ A **compile-time and runtime selector** — `target("...")` is compile-time, `capability("...")` can be runtime
- ✅ A **verification contract** — `verify random(N)` tests all lanes against the spec
- ✅ An **autotune cache** — the native C runtime caches lane selections per `(converge_key, shape_key, cpu_fingerprint)`
- ✅ A **telemetry surface** — `converge_mismatch_count()` reports lane divergence
- ✅ A **semantic dispatch layer** — spec is truth, fast lanes are optimizations, verify is the proof

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `converge` | "Performance lane selector (SIMD)" | **Strategy selector** — any spec + alternative(s) with verification. Maze-solving strategy picker. Algorithm selector. Quality-of-service lane. A/B test framework. |

---

## Syntax Reference

### Canonical Syntax

```kn
converge <name>(<params>) -> <return_type>:
    spec <lane_name>:
        <body>
    fast <lane_name> when <selector>:
        <body>
    [fast <lane_name> when <selector>: ...]
    [verify random(<count>)]
```

Where:
- `<name>` is a user-defined identifier (becomes the dispatch function)
- `<params>` are function parameters
- `<return_type>` is the return type (all lanes must match)
- `spec` declares exactly one reference lane
- `fast` declares one or more alternative lanes
- `<selector>` is `target("...")` or `capability("...")`
- `verify random(N)` generates N random argument sets and compares all lanes

### Concrete Examples

```kn
// Performance: scalar vs SIMD
converge dot_product(left: ptr<Int>, right: ptr<Int>, cells: Int) -> Int:
    spec reference:
        return scalar_dot(left, right, cells)
    fast avx2_lane when capability("cpu.x86.avx2"):
        return avx2_dot(left, right, cells)
    fast avx512_lane when capability("cpu.x86.avx512"):
        return avx512_dot(left, right, cells)
    verify random(8)

// Strategy selection: maze solving
converge quantum_maze_run(maze_signature: Int, start: Int, target: Int, width: Int, height: Int) -> Int:
    spec reference:
        return reference_maze_distance(maze_signature, start, target, width, height)
    fast greedy_rat when target("llvm"):
        return greedy_maze_distance(maze_signature, start, target, width, height)
    fast chaos_rat when capability("sim.rat.random_walk"):
        return chaos_maze_distance(maze_signature, start, target, width, height)
    verify random(8)

// Quality-of-service: adaptive quality
converge render_frame(authority: RenderAuth, mode: Bool) -> Int:
    spec quality_path:
        return render_high_quality(authority)
    fast degraded_path when mode:
        return render_low_quality(authority)

// Stdlib: Xoshiro128 PRNG scramble
converge xoshiro_scramble(s1: Int) -> Int:
    spec builtin:
        return xoshiro_scramble_scalar(s1)
    fast llvm_lane when target("llvm"):
        return rotl(s1 * 17, 13) + (s1 >> 17) * 7
```

### Grammar (from parser.rs)

```text
converge <ident>(<params>) [-> <type>]:
    <indent>
    spec <ident>: <block>
    fast <ident> when <selector>: <block>
    ...
    verify random(<int>)
    <dedent>

selector = target(<string>) | capability(<string>)
```

Parser rules:
1. Exactly one `spec` lane required
2. At least one `fast` lane required
3. `verify` is optional
4. Lanes are parsed in order, spec first then fast
5. Selectors are parsed as `target("...")` or `capability("...")`
6. `verify random(N)` requires N >= 0

---

## How It Works — Full Pipeline

### Source to Runtime Flow

```
.kn converge declaration
  → parser.rs: parse_converge()
    → ConvergeDef { name, params, return_type, spec_lane, fast_lanes, verify_random_count }
  → types.rs: check_converge()
    → Resolves dispatcher function signature
    → Type-checks spec lane body
    → Type-checks each fast lane body (must match dispatcher signature)
    → Returns TypedConverge { ast, resolved_type }
  → runtime_contract.rs: collect_converge_contracts()
    → RuntimeConvergeContract { name, spec_lane, fast_lanes, verify_random_count }
    → Emits capability "converge.dispatch"
  → runtime.rs: register_converge_value()
    → Registers converge name in value table for dispatch
  → runtime.rs: select_converge_lane()
    → Scans fast lanes in order, picks first matching selector
    → Falls back to spec if no fast lane matches
  → runtime.rs: verify_converge_selected_against_spec()
    → Calls spec and selected lane with random args
    → Reports mismatch via converge_mismatch_count()
  → codegen_llvm/mod.rs: compile_converge()
    → Emits spec function symbol
    → Emits each fast function symbol
    → Emits dispatch function that probes capabilities at runtime
    → Caches chosen lane for the process lifetime
  → runtime/native: converge.c
    → abi_converge_select_lane_for_key() — probes autotune cache
    → abi_converge_commit_winner() — stores selection in cache
    → abi_converge_record_telemetry() — records timing/lane telemetry
```

### At Compile Time

The typechecker (`types.rs: check_converge`) performs:

1. **Signature derivation** — derives a function signature from the converge declaration (name, params, return_type)
2. **Spec checking** — type-checks the spec lane body as a function with that signature
3. **Fast lane checking** — type-checks each fast lane body
4. **Signature matching** — verifies each fast lane's signature matches the dispatcher signature
5. **Verify type support** — if `verify random(N)` is present, ensures the return type supports equality comparison

```rust
fn check_converge(env: &mut TypeEnv, converge: &ConvergeDef) -> KainResult<TypedConverge> {
    let resolved_type = function_signature(env, &converge_dispatcher_view(converge), None)?;
    let expected_signature = resolved_type.clone();
    // ...
    check_function(env, &converge_lane_function_view(converge, &converge.spec_lane))?;
    for lane in &converge.fast_lanes {
        check_function(env, &converge_lane_function_view(converge, lane))?;
        let lane_signature = function_signature(env, &lane_view, None)?;
        if lane_signature != expected_signature {
            return Err("Converge lane '{}' does not match dispatcher signature");
        }
    }
    Ok(TypedConverge { ast: converge.clone(), resolved_type })
}
```

---

## The Two-Level Architecture

Converge has a **two-level architecture** — an interpreter/runtime path for the Kain interpreter and a native LLVM path for compiled code.

### Level 1: Interpreter Lane Selection

In the interpreter (`runtime.rs`), `select_converge_lane()` implements a simple linear scan:

1. Iterate fast lanes in declaration order
2. For each lane, if a selector is present, evaluate it:
   - `target("llvm")` → compiled target check
   - `capability("cpu.x86.avx2")` → runtime capability probe
3. If no selector or selector matches, use this lane
4. If no fast lane matches, fall back to spec

After selection, `verify_converge_selected_against_spec()` calls both the spec lane and the selected lane with random arguments and checks for divergence. Any mismatch is recorded via `converge_mismatch_count()`.

### Level 2: Native LLVM Dispatch

In native LLVM (`codegen_llvm/mod.rs`), `compile_converge()` emits:

1. **Spec function** — compiled as a normal callable
2. **Fast lane functions** — each compiled as a separate callable
3. **Dispatch function** — the converge name becomes a function that:
   - Statically resolves `target("...")` selectors at compile time
   - Emits runtime CPU capability probes for `capability("...")` selectors
   - Caches the chosen lane in a static variable (once per process)
   - Falls back to the spec lane if no fast lane matches

The LLVM dispatch function caches the lane selection using `kain_converge_cache_key(converge_key, shape_key)` and the native `abi_converge_select_lane_for_key()` function, which probes the autotune cache.

---

## Selectors — Target and Capability

### Target Selector

`target("...")` is a **compile-time** selector. The value is compared against the compilation target name:

```kn
fast llvm_lane when target("llvm")
fast interpret_lane when target("interpret")
fast c_lane when target("cpp")
fast wasm_lane when target("wasm")
```

The parser resolves `target("...")` to `ConvergeSelector::Target(value)`. At the LLVM level, target selectors are resolved statically at compile time — only the matching target's lane is included. At the interpreter level, the current target is checked dynamically.

### Capability Selector

`capability("...")` is a **runtime** selector. The string is compared against runtime CPU/capability masks:

```kn
fast avx2_lane when capability("cpu.x86.avx2")
fast avx512_lane when capability("cpu.x86.avx512f")
fast sse2_lane when capability("cpu.x86.sse2")
fast chaos_rat when capability("sim.rat.random_walk")     // custom capability
```

Capability probing uses `abi_cpu_capability_mask_for_key()` which returns a bitmask. The LLVM codegen emits capability check code that runs once at startup (or first call), caches the result, and then uses the cached lane for all subsequent calls.

### No Selector

If a lane has no selector, it's **always eligible**:

```kn
fast always_eligible_lane:
    return compute(value)
```

No-selector lanes are selected first (they're scanned in declaration order), so the order of fast lanes matters.

---

## Verify Random — The Arbitration Mechanism

### What It Does

`verify random(N)` generates N sets of random arguments and calls **both** the spec lane and each active fast lane with those arguments. If any lane produces a different result than the spec, `converge_mismatch_count()` increments.

### How It Works

In the interpreter (`runtime.rs: verify_converge_selected_against_spec`):

```rust
fn verify_converge_selected_against_spec(env, converge, selected_lane, args, selected_result, label) {
    let spec_result = execute_function_body(env, &converge.spec_lane.body, args)?;
    if spec_result != *selected_result {
        env.converge_mismatch_count = env.converge_mismatch_count.wrapping_add(1);
        // warn about mismatch
    }
}
```

In native LLVM, the verify clause generates startup code that:
1. Generates N random argument sets
2. Calls the spec lane and each fast lane
3. Compares results
4. Calls `abi_converge_record_telemetry()` to record mismatches
5. The mismatch count can be queried via `converge_mismatch_count()`

### The Canonical Usage Pattern

```kn
converge mix(value: Int) -> Int:
    spec reference:
        return ((value * 31) + 7) % MODULUS
    fast llvm_lane when target("llvm"):
        return ((value * 31) + 7) % MODULUS
    verify random(8)
```

The verify clause proves that for 8 random inputs, the selected fast lane produces the same output as the spec. If the fast lane diverges (because of a bug or different behavior), the mismatch is recorded and can be detected at runtime.

---

## The Interpreter Lane Selection Algorithm

```rust
fn select_converge_lane<'a>(env: &Env, converge: &'a ConvergeDef) -> &'a ConvergeLane {
    // 1. Scan fast lanes in declaration order
    for lane in &converge.fast_lanes {
        // 2. Check selector
        let eligible = match &lane.selector {
            Some(ConvergeSelector::Target(target)) => env.target == *target,
            Some(ConvergeSelector::Capability(key)) => {
                env.capabilities.contains(key)
            }
            None => true,  // no selector = always eligible
        };
        if eligible {
            return lane;
        }
    }
    // 3. No fast lane matched → fall back to spec
    &converge.spec_lane
}
```

**Key properties:**
- Lanes are scanned in declaration order
- First matching lane wins (no tie-breaking)
- No-selector lanes are always eligible
- Fallback to spec if no fast lane matches (spec is always eligible)

---

## LLVM Codegen — Static and Dynamic Dispatch

### compile_converge()

The LLVM codegen emits the converge as a function with static caching:

```llvm
; Static lane cache (per converge)
@__kain_converge_<name>_lane = internal global i64 0
@__kain_converge_<name>_initialized = internal global i8 0

; Dispatch function
define i64 @<name>(i64 %arg) {
entry:
  %init = load i8, i8* @__kain_converge_<name>_initialized
  %probe = icmp eq i8 %init, 0
  br i1 %probe, label %select_lane, label %use_cached

select_lane:
  ; Probe capabilities for each fast lane
  %caps = call i64 @abi_cpu_capability_mask_for_key(i8* getelementptr("cpu.x86.avx2"))
  %avx2_mask = and i64 %caps, <bit_for_avx2>
  %has_avx2 = icmp ne i64 %avx2_mask, 0
  br i1 %has_avx2, label %lane_avx2, label %fallback

lane_avx2:
  store i64 1, i64* @__kain_converge_<name>_lane
  store i8 1, i8* @__kain_converge_<name>_initialized
  br label %use_cached

fallback:
  store i64 0, i64* @__kain_converge_<name>_lane  ; spec lane = 0
  store i8 1, i8* @__kain_converge_<name>_initialized
  br label %use_cached

use_cached:
  %lane = load i64, i64* @__kain_converge_<name>_lane
  ; Switch to the right lane
  switch i64 %lane, label %spec_lane [i64 1, label %fast_lane_avx2]

spec_lane:
  %spec_result = call i64 @<name>__spec(i64 %arg)
  ret i64 %spec_result

fast_lane_avx2:
  %fast_result = call i64 @<name>__fast_avx2_lane(i64 %arg)
  ret i64 %fast_result
}
```

**Key properties:**
- Target selectors are resolved statically at compile time
- Capability selectors generate runtime CPU mask probes
- The lane selection is cached after the first call (static variable, once-per-process)
- The dispatch is a direct call, not an indirect function pointer
- Spec and fast lanes are compiled as separate `define` functions

---

## Native C Autotune Cache

### Header (`runtime/native/include/converge.h`)

```c
#define KAIN_CONVERGE_LANE_MAX 8u
#define KAIN_CONVERGE_TELEMETRY_CAP 64u
#define KAIN_CONVERGE_TUNE_CACHE_CAP 64u
#define KAIN_CONVERGE_NO_WINNER UINT64_MAX

int64_t abi_converge_select_lane_for_key(uint64_t converge_key, uint64_t shape_key,
                                          uint64_t eligible_mask, uint64_t fallback_lane);
int64_t abi_converge_commit_winner(uint64_t converge_key, uint64_t shape_key,
                                    uint64_t lane_index, uint64_t eligible_mask);
int64_t abi_converge_record_telemetry(uint64_t converge_key, uint64_t shape_key,
                                       uint64_t lane_index, uint64_t elapsed_ticks, int64_t status);
uint64_t abi_converge_telemetry_count(void);
uint64_t abi_converge_cache_probe_count(void);
uint64_t abi_converge_cache_hit_count(void);
```

### Cache Structure (`runtime/native/src/core/converge.c`)

```c
typedef struct KainConvergeTuneCacheSlot {
    uint64_t key;              // mixed key = hash(converge_key ^ shape_key ^ cpu_fingerprint)
    uint64_t eligible_mask;    // bitmask of eligible lanes
    uint64_t lane_index;       // selected lane
    uint64_t generation;       // version
} KainConvergeTuneCacheSlot;
```

### Lane Selection Algorithm

```
abi_converge_select_lane_for_key(converge_key, shape_key, eligible_mask, fallback_lane):
  1. Compute cache_key = mix64(converge_key ^ (shape_key + constant) ^ cpu_fingerprint) | 1
  2. Probe open-addressed hash table (stride = (key >> 6) | 1)
  3. If cached hit and lane is still in eligible_mask → return cached lane
  4. If cache miss → return lowbit of eligible_mask (first eligible lane)
```

### Cache Commitment

```
abi_converge_commit_winner(converge_key, shape_key, lane_index, eligible_mask):
  1. Validate lane_index < 64 and is in eligible_mask
  2. Find or create cache slot for key
  3. Store lane_index, increment generation
```

### Telemetry

```
abi_converge_record_telemetry(converge_key, shape_key, lane_index, elapsed_ticks, status):
  1. Atomic-fetch-add cursor (circular buffer of KAIN_CONVERGE_TELEMETRY_CAP = 64)
  2. Store sample at slot_index = cursor & 63
```

The telemetry ring buffer supports the silicon autotune pipeline (`proofs-experimental/converge-autotune-selector-ring.smt2`).

---

## Stdlib Telemetry and Runtime API

### From `std::intent`

| Function | Signature | Description |
|----------|-----------|-------------|
| `converge_mismatch_count()` | `() -> Int` | Total number of spec-vs-fast mismatches |
| `converge_choose_int(spec, fast)` | `(Int, Int) -> Int` | Select spec or fast result (telemetry) |
| `converge_choose_bool(spec, fast)` | `(Bool, Bool) -> Bool` | Select spec or fast result (telemetry) |
| `converge_status(spec, fast)` | `(Int, Int) -> Int` | Compare spec vs fast, return status code |

### From `std::runtime`

| Function | Signature | Description |
|----------|-----------|-------------|
| `runtime_converge_select_lane(key, shape_key, mask, fallback)` | `(Int, Int, Int, Int) -> Int` | Probe the autotune cache (native) |
| `runtime_converge_commit_winner(key, shape_key, lane)` | `(Int, Int, Int) -> Int` | Commit to autotune cache |
| `runtime_converge_record_telemetry(key, lane, elapsed, ok, mismatch)` | `(Int, Int, Int, Int, Int) -> Int` | Record telemetry sample |
| `runtime_converge_telemetry_count()` | `() -> Int` | Total telemetry samples |
| `runtime_converge_cache_probe_count()` | `() -> Int` | Total cache probes |
| `runtime_converge_cache_hit_count()` | `() -> Int` | Total cache hits |

### Stdlib Example: `xoshiro_scramble` in `std::random`

```kn
// stdlib/random.kn — converge in the standard library!
fn xoshiro_scramble_scalar(s1: Int) -> Int:
    return rotl(s1 * 17, 13) + (s1 >> 17) * 7

converge xoshiro_scramble(s1: Int) -> Int:
    spec builtin:
        return xoshiro_scramble_scalar(s1)
    fast llvm_lane when target("llvm"):
        return rotl(s1 * 17, 13) + (s1 >> 17) * 7
```

---

## The Obvious Use Case — Performance Lane Selection

### SIMD Dot Product

```kn
fn scalar_dot(left: ptr<Int>, right: ptr<Int>, cells: Int) -> Int:
    var acc: Int = 0
    var i: Int = 0
    while i < cells:
        let a = mem_load(ptr_offset(left, i, "Int"), "Int")
        let b = mem_load(ptr_offset(right, i, "Int"), "Int")
        acc = acc + a * b
        i = i + 1
    return acc

converge dot_i64(left: ptr<Int>, right: ptr<Int>, cells: Int, bias: Int, modulus: Int) -> Int:
    spec reference:
        return scalar_dot(left, right, cells)
    fast avx2_lane when capability("cpu.x86.avx2"):
        return native_simd_i32_domain_dot_avx2_mod(left, right, cells, bias, modulus)
    fast avx512_lane when capability("cpu.x86.avx512"):
        return native_simd_i32_domain_dot_avx512_mod(left, right, cells, bias, modulus)
    verify random(4)
```

This is the **obvious** converge: one scalar spec, two SIMD fast lanes, verify that they agree. The runtime probes AVX2/AVX-512 support at startup and picks the fastest available lane.

### Frame Rendering Quality

```kn
converge render_frame(authority: RenderAuth, mode: Bool) -> Int:
    spec quality_path:
        return render_high_quality(authority)
    fast degraded_path when mode:
        return render_low_quality(authority)
```

The `mode` parameter is a `Bool` selector — when `true`, the `degraded_path` lane activates (no `when` clause means always eligible). This is converge as **quality-of-service dispatch** — the calling code controls which lane runs.

---

## The Hack Side — The Convergence Rat Experiment

> *"It effectively places two rats in a maze and pins them against finding the fastest path."*

`blades/experiments/convergence/` is the canonical proof that `converge` is a **general-purpose relationship descriptor**, not a domain-locked feature. The same construct that picks an AVX2 lane over a scalar fallback can pick a maze-solving strategy over competing heuristics.

### The Core Converge

```kn
converge quantum_maze_run(maze_signature: Int, start: Int, target: Int, width: Int, height: Int) -> Int:
    spec reference:
        return reference_maze_distance(maze_signature, start, target, width, height)
    fast greedy_rat when target("llvm"):
        return greedy_maze_distance(maze_signature, start, target, width, height)
    fast chaos_rat when capability("sim.rat.random_walk"):
        return chaos_maze_distance(maze_signature, start, target, width, height)
    verify random(8)
```

### What's Happening

Three pathfinding strategies compete. The `spec` lane is a conservative Manhattan-based heuristic. `greedy_rat` is an optimistic biased heuristic that activates when target is "llvm". `chaos_rat` is a random-walk distance estimate that activates on a custom capability key `sim.rat.random_walk`.

The `verify random(8)` clause ensures the winning strategy is consistent across random inputs — if the greedy rat returns a distance that doesn't match the spec, the mismatch is recorded.

### The Strategy Functions

```kn
// Conservative reference
fn reference_maze_distance(sig: Int, start: Int, target: Int, w: Int, h: Int) -> Int:
    return maze_base_distance(sig, start, target, w, h) + (sig % 3)

// Optimistic greedy (biased lower)
fn greedy_maze_distance(sig: Int, start: Int, target: Int, w: Int, h: Int) -> Int:
    let base = maze_base_distance(sig, start, target, w, h)
    let bias = (sig % 5) - 1                           // bias down by 0-3
    return clamp_int(base - bias, 0, RAT_MODULUS - 1)

// Chaotic (random noise added)
fn chaos_maze_distance(sig: Int, start: Int, target: Int, w: Int, h: Int) -> Int:
    let base = maze_base_distance(sig, start, target, w, h)
    return base + ((sig * 3) % 7) + ((start + target) % 3)
```

Each returns a distance estimate. Converge picks which estimate is the "winner" for this frame. The actual pathfinding (BFS, A*, random walk) runs separately in the `orchestrate` block — converge is only choosing the **strategy outcome**, not the path itself.

### The Orchestrate Frame

The `rat_frame_step` orchestrate runs all three pathfinding algorithms, clears their trails, then calls `quantum_maze_run` to pick the winner:

```kn
orchestrate rat_frame_step(maze: ptr<Int>, start: Int, target: Int, telemetry: RatTelemetry) -> Int:
    let pure_distance = kain run_bfs_trace(maze, start, target, ...)
    let greedy_distance = kain run_astar_trace(maze, start, target, ...)
    let chaos_distance = kain run_chaos_trace(maze, start, target, ...)
    let winner_distance = kain quantum_maze_run(sig, start, target, width, height)
    let committed = kain commit_search(telemetry, frame, ..., pure, greedy, chaos, winner)
    return committed + pure + greedy + chaos + winner + ...
```

### What the Rat Experiment Proves

| Aspect | Proof |
|--------|-------|
| `converge` selects strategies, not instructions | Greedy vs chaos vs reference — three pathfinding heuristics competing |
| `converge` works with custom capabilities | `capability("sim.rat.random_walk")` — a domain-specific capability |
| `converge` integrates with `orchestrate` | `quantum_maze_run` is called inside `rat_frame_step` |
| `converge` integrates with `patch` | The winner distance is committed via `commit_search` (a `patch`) |
| `converge` is verified | `verify random(8)` checks the winner against spec |
| `converge` is strategy selection, not just speed | Each lane implements a fundamentally different strategy |

---

## Creative Semantic Abuse — Under-Utilized Ideas

### 1. Converge as A/B Test Framework

Run two versions of an algorithm in production and compare their outputs:

```kn
converge search_v2(query: String, threshold: Int) -> [Int]:
    spec legacy:
        return legacy_search(query, threshold)
    fast v2_lane when capability("search.v2"):
        return new_search(query, threshold)
    verify random(5)
```

The `verify random(5)` clause checks that the new search produces the same results as the legacy search for random inputs. If a regression ships, `converge_mismatch_count()` catches it immediately.

### 2. Converge as Adaptive Quality-of-Service

Different cost/quality tradeoffs gated by runtime metrics:

```kn
converge compress_image(image: Image, target_bytes: Int) -> Image:
    spec lossless:
        return compress_png(image)
    fast lossy_high when capability("qos.high_quality"):
        return compress_jpeg(image, 95)
    fast lossy_medium when capability("qos.medium_quality"):
        return compress_jpeg(image, 75)
    fast lossy_low when capability("qos.low_quality"):
        return compress_jpeg(image, 50)
```

The capability keys could be set dynamically based on frame time budget, battery level, or network conditions.

### 3. Converge as Model/Heuristic Selector

Different ML models or heuristics for the same input:

```kn
converge predict_click_rate(features: [Float]) -> Float:
    spec logistic_regression:
        return logistic_regression_predict(features)
    fast decision_tree when capability("model.decision_tree"):
        return dt_predict(features)
    fast neural_net when capability("model.neural_net"):
        return nn_predict(features)
    verify random(10)
```

### 4. Converge as Numeric Stability Checker

Use a high-precision spec and a fast approximate lane — the verify clause tells you how much error the approximation introduces:

```kn
converge fast_sin(angle: Float) -> Float:
    spec stdlib_math:
        return sin(angle)
    fast approx_lane when target("llvm"):
        return cheat_sin(angle)    // polynomial approx
    verify random(100)
```

If the approximation drifts beyond acceptable error, `converge_mismatch_count()` reports it. This turns converge into a **numeric stability regression detector**.

### 5. Converge as Power Management Lane

Gated by battery or power capabilities:

```kn
converge render_frame(auth: RenderAuth) -> Int:
    spec full_quality:
        return render_max_quality(auth)
    fast balanced when capability("power.balanced"):
        return render_balanced(auth)
    fast powersave when capability("power.low"):
        return render_low_power(auth)
```

The capability keys are set by a power management system. When the device is on battery, the power-saving lane activates automatically.

### 6. Converge as Debug/Release Dispatch

```kn
converge process_metrics(data: [Int]) -> Int:
    spec release_mode:
        return compute_summary(data)
    fast debug_mode when capability("debug.assertions_enabled"):
        return compute_summary_with_assertions(data)
    verify random(3)
```

The `debug.assertions_enabled` capability is only present in debug builds. In release builds, the debug lane is never eligible.

### 7. Converge + Pulse + Adaptive Rate

Combine with pulse for adaptive frame rate:

```kn
pulse render_clock every 16ms jitter 2ms:
    let overloaded = pulse_missed > 0
    let _result = converge render_frame(Authority, overloaded)
```

The converge selects between high-quality and degraded rendering based on overload detection.

---

## Combining with Other Keywords

### Converge + Orchestrate

Converge is called inside orchestrate stages as a `converge` stage runtime:

```kn
orchestrate pipeline(value: Int) -> Int:
    stage selected: converge quantum_maze_run(sig, start, target, w, h)
    stage committed: kain commit_search(telemetry, ..., selected)
    return committed
```

Or converge can be called as a `kain` stage call that references a converge name:

```kn
orchestrate pipeline(value: Int) -> Int:
    let result: Int = kain quantum_maze_run(sig, start, target, w, h)
    stage committed: kain commit_search(telemetry, ..., result)
    return result
```

### Converge + Patch

The winner of a converge can feed directly into a patch:

```kn
let winner = quantum_maze_run(sig, start, target, w, h)
let committed = commit_search(telemetry, frame, ..., winner)
```

### Converge + Pulse

Pulse is the temporal driver; converge selects the algorithm per-beat:

```kn
pulse tick every 16ms:
    let quality = pulse_missed > 0
    let _ = converge render(Authority, quality)
```

### Converge + World

Converge lanes can read and write world state (through called functions):

```kn
converge process_frame(auth: RenderAuthority) -> Int:
    spec full:
        return render_full(auth)
    fast partial when capability("qos.low"):
        return render_partial(auth)
```

### Converge + Actor

Converge can select which actor to call or which message to send:

```kn
converge dispatch_work(payload: Int) -> Int:
    spec queue_to_worker:
        return ask(WorkerPool[0], "Process", payload)
    fast fast_path when capability("routing.fast_lane"):
        return ask(WorkerPool[1], "Process", payload)
```

### Converge + Entangle

Converge can select which world state to read:

```kn
converge read_signal(authority: Authority, mirror: Mirror) -> Int:
    spec from_authority:
        return authority.signal
    fast from_mirror when capability("read.mirror"):
        return mirror.signal_copy
```

---

## Real-World Usage Across the Repo

### stdlib/random.kn — The Only Stdlib Converge

```kn
fn xoshiro_scramble_scalar(s1: Int) -> Int:
    return rotl(s1 * 17, 13) + (s1 >> 17) * 7

converge xoshiro_scramble(s1: Int) -> Int:
    spec builtin:
        return xoshiro_scramble_scalar(s1)
    fast llvm_lane when target("llvm"):
        return rotl(s1 * 17, 13) + (s1 >> 17) * 7
```

This is the **only converge in the entire standard library**. It's used by the Xoshiro128 PRNG for the core scramble operation. The spec is a Kain scalar implementation; the fast lane is an LLVM-compiled expression that may select vectorized codegen.

### smoketest/src/semantics/converge.kn

```kn
converge smoke_mix(value: Int) -> Int:
    spec reference:
        return smoke_scalar_mix(value)
    fast llvm_lane when target("llvm"):
        return ((value * 31) + 7) % 1000000007
    fast interpret_lane when target("interpret"):
        return ((value * 31) + 7) % 1000000007
    verify random(8)

pub fn smoke_converge_lane() -> Int:
    let result = smoke_mix(100)
    let expected = smoke_scalar_mix(100)
    if result != expected: return 1
    if converge_mismatch_count() != 0: return 2
    return 0
```

The smoketest verifies that converge returns the correct value AND reports zero mismatches.

### blades/test/converge-autotune-probe/src/main.kn

```kn
converge accelerate_probe(value: Int) -> Int:
    spec reference:
        return ((value * 13) + 5) % PROBE_MODULUS
    fast scalar_lane when target("llvm"):
        return ((value * 13) + 5) % PROBE_MODULUS
    fast avx2_lane when capability("cpu.x86.avx2"):
        return ((value * 13) + 5) % PROBE_MODULUS
    verify random(2)

fn selector_probe() -> Int:
    let avx2_available = runtime_cpu_has_capability("cpu.x86.avx2")
    let selected_lane = runtime_converge_select_lane(...)
    // ... verify cache, telemetry all work ...
```

This blade directly probes the native autotune cache — `selected_lane`, cache counts, telemetry counts — to prove the C runtime's converge infrastructure works.

### benchmark/cases_v2/fusion_chain.kn

```kn
converge fusion_fast_mix(value: Int, tick: Int) -> Int:
    spec reference:
        return fusion_mix(value + tick)
    fast llvm_lane when target("llvm"):
        return ((value + tick) * 17 + 31) % FUSION_MODULUS
    fast interpret_lane when target("interpret"):
        return ((value + tick) * 17 + 31) % FUSION_MODULUS
```

Called inside the `fusion_signal_pipeline` orchestrate, which is called from a resonate handler triggered by a patch to an entangled world — **converge as one node in a 7-layer causal chain**.

### benchmark/cases/semantic_singularity_converge_only/main.kn

A dedicated benchmark that stresses converge as a standalone semantic — multiple converge dispatches in tight loops, measuring lane selection overhead and verify clause execution time.

### benchmark/cases/scalar_mix/main.kn

The simplest converge — a scalar mix with spec + one LLVM fast lane, used as a baseline benchmark for converge dispatch overhead.

### blades/python/24_tet/src/resonate_py_effects.kn

Four converges used inside orchestrate stages for audio DSP:

```kn
converge fx_lfo_sin_scalar(phase: Int, depth: Int) -> Int: ...
converge fx_wet_dry_mix(phase: Int, mix: Int, lfo: Int) -> Int: ...
converge fx_waveshape(value: Int, drive: Int) -> Int: ...
converge fx_param_clamp(value: Int, lo: Int, hi: Int) -> Int: ...
```

Each converge selects between a scalar reference and an LLVM-compiled fast lane, all wired into a 9-stage `orchestrate` pipeline driven by an 8ms `pulse`.

---

## Z3 Proofs

### Experimental: Converge Autotune Selector Ring

**File:** `runtime/native/src/core/z3/proofs-experimental/converge-autotune-selector-ring.smt2`

This experimental proof verifies the circular telemetry buffer's safety properties:
- The cursor (atomic-fetch-add) never outruns the slot indices
- The ring buffer masking (`cursor & (CAP - 1)`) is correct for power-of-two capacity
- No concurrent write to the same slot from different threads corrupts the structure

### Capability Token Collision

**File:** `runtime/native/src/core/z3/proofs/native-machine-capability-token-signatures-are-collision-free.yaml`

This proof covers the stable hash tokens used by converge keys (alongside axiom/pulse/shatter/teleport tokens). It verifies that capability name → token mappings are collision-free within the declared set.

---

## Validation Ladder

```powershell
# 1. Typecheck
kain check <file.kn> --target llvm

# 2. Compile + run
kain run <file.kn> --target llvm

# 3. Verify converge results (check mismatch count)
kain run <file.kn> --target llvm
# Add test: converge_mismatch_count() == 0

# 4. For the rat experiment
kain run blades/experiments/convergence --target llvm

# 5. For benchmark proof
python benchmark/run.py --case semantic_singularity_converge_only --languages kain --runs 1
python benchmark/run.py --case scalar_mix --languages kain --runs 1

# 6. For autotune probe
kain run blades/test/converge-autotune-probe --target llvm

# 7. Check runtime contract
# The output runtime_contract.json shows converges[] with spec/fast lanes and verify config

# 8. For stdlib converge
kain run <using std::random::xoshiro128_next>
```

### Smoketest

```kn
pub fn smoke_converge_lane() -> Int:
    let result = smoke_mix(100)
    let expected = smoke_scalar_mix(100)
    if result != expected:
        return 1
    if converge_mismatch_count() != 0:
        return 2
    let pair = smoke_mix_pair(17, 31)
    if pair < 0:
        return 3
    return 0
```

---

## Source Anchors

### Parser & AST

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 356-387 | `ConvergeDef`, `ConvergeLaneKind`, `ConvergeSelector`, `ConvergeLane` |
| `crates/core/src/parser.rs` | 1017, 1954-2034 | `parse_converge()` — full function |
| `crates/core/src/parser.rs` | 2079-2130 | `parse_converge_lane()`, `parse_converge_selector()`, `parse_converge_verify_random_count()` |

### Typechecker

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/types.rs` | 198-202 | `TypedConverge` |
| `crates/core/src/types.rs` | 4659 | `check_converge()` dispatch |
| `crates/core/src/types.rs` | 5912-5944 | `check_converge()` — type checks spec and all fast lanes, validates matching signatures |
| `crates/core/src/types.rs` | 5888-5910 | `validate_pulse_duration()` (shared with pulse/resonate) |

### Runtime Contract

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime_contract.rs` | 202-215 | `RuntimeConvergeContract` |
| `crates/core/src/runtime_contract.rs` | 361, 639, 887 | Collection, `converge.dispatch` capability emission |
| `crates/core/src/runtime_contract.rs` | 1411-1461 | `collect_converge_contracts()`, `runtime_converge_contract()` |

### Runtime (Interpreter)

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime.rs` | 3967, 4175, 4297 | `register_converge_value()` |
| `crates/core/src/runtime.rs` | 7256-7435 | `select_converge_lane()`, `verify_converge_selected_against_spec()` |

### LLVM Codegen

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13186 | `compile_converge()` dispatch |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15278+ | `compile_converge()` — emits spec, fast lanes, and dispatch function |

### Native C Runtime

| File | Lines | Content |
|------|-------|---------|
| `runtime/native/include/converge.h` | Full header | API: select, commit, record telemetry, cache probes |
| `runtime/native/src/core/converge.c` | Full file | Autotune cache, telemetry ring, lane selection |

### Stdlib

| File | Lines | Content |
|------|-------|---------|
| `stdlib/intent.kn` | 59, 163-164, 227-237, 311-312, 371-378 | `converge_mismatch_count()`, `converge_choose_*()`, `converge_status()` |
| `stdlib/runtime.kn` | 130-145 | `native_converge_select_lane()`, `native_converge_commit_winner()`, `native_converge_record_telemetry()`, etc. |
| `stdlib/random.kn` | 37 | `converge xoshiro_scramble()` — the only stdlib converge |

### Convergence Rat Experiment

| File | Content |
|------|---------|
| `blades/experiments/convergence/README.md` | Full experiment documentation |
| `blades/experiments/convergence/src/orchestrate.kn` | `quantum_maze_run` converge + `rat_frame_step` orchestrate |
| `blades/experiments/convergence/src/main.kn` | Main loop: oracle → orchestrate → converge → archivist → visualize |
| `blades/experiments/convergence/src/patch.kn` | `commit_search` — records converge winner in journal |
| `blades/experiments/convergence/src/world.kn` | `RatTelemetry` — best_lane telemetry |
| `blades/experiments/convergence/src/actors.kn` | Simulation agents that consume converge results |
| `blades/experiments/convergence/src/shatter.kn` | TrailSample struct (SoA) |
| `blades/experiments/convergence/src/laws.kn` | 9 domain laws including `rat_lane_kind_valid` (0,1,2) |

### Other Examples

| File | Usage |
|------|-------|
| `smoketest/src/semantics/converge.kn` | Basic smoke: spec + 2 fast lanes + verify |
| `blades/test/converge-autotune-probe/src/main.kn` | Autotune cache → orchestrate → probe |
| `benchmark/cases_v2/fusion_chain.kn` | `fusion_fast_mix` in orchestrate stage |
| `benchmark/cases/scalar_mix/main.kn` | Minimal perf converge |
| `benchmark/cases/semantic_singularity_converge_only/main.kn` | Converge stress test |
| `blades/python/24_tet/src/resonate_py_effects.kn` | 4 converges in audio DSP orchestrate pipeline |
| `benchmark/cases/actor_ownership_backpressure/main.kn` | Converge in actor ownership stress test |

---

## Anti-Patterns

### ❌ Single Implementation in Converge

```kn
// WRONG: converge with only one lane
converge mix(value: Int) -> Int:
    spec reference:
        return mix_scalar(value)
```

`converge` requires at least one `fast` lane. If you only have one implementation, use `fn`.

### ❌ Different Signatures Across Lanes

```kn
// WRONG: different param counts
converge mix(value: Int) -> Int:
    spec reference:
        return mix_scalar(value)
    fast avx2_lane when capability("cpu.x86.avx2"):
        return avx2_mix(value, extra_param)    // type error!
```

All lanes must have the same signature (same params, same return type).

### ❌ Using `if target == "llvm"` Inside a Function

```kn
// WRONG: manual target check
fn mix(value: Int) -> Int:
    if target_is("llvm"):
        return fast_mix(value)
    return scalar_mix(value)
```

```kn
// RIGHT: compiler-owned dispatch
converge mix(value: Int) -> Int:
    spec reference:
        return scalar_mix(value)
    fast llvm_lane when target("llvm"):
        return fast_mix(value)
```

### ❌ No Verify Clause for Divergent Lanes

If fast lanes can produce different results, always add `verify random(N)`. Without verify, diverge silently and `converge_mismatch_count()` never increments because there's no spec comparison.

### ❌ Using Converge When Orchestrate Is Better

If lanes have different dependencies or runtimes (CPU vs GPU), use `orchestrate` instead:

```kn
// WRONG: converge for cross-runtime dispatch
converge process(data: [Int]) -> Int:
    spec cpu_path:
        return cpu_process(data)
    fast gpu_path when capability("gpu.compute"):
        return gpu_process(data)    // converge doesn't handle GPU dispatch
```

```kn
// RIGHT: orchestrate for cross-runtime
orchestrate process(data: [Int]) -> Int:
    stage result: cpu process_cpu(data) residency host
    return result
```

### ❌ Not Checking Telemetry

After running converge, check `converge_mismatch_count()`. If it's non-zero, lanes diverged:

```kn
let result = converge_mismatch_count()
if result != 0:
    // lanes diverged — log, alert, or fall back to spec
```

### ❌ Assuming Lane Order Doesn't Matter

Fast lanes are scanned in declaration order. The first matching lane wins. If you have:

```kn
fast any_lane:                                // no selector = always matches
fast avx2_lane when capability("cpu.x86.avx2")
```

Then `any_lane` will be selected even when AVX2 is available, because it's declared first. Order lanes from most-specific to least-specific.

---

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `converge name(params) -> Type: spec lane: body [fast lane when selector: body]... [verify random(N)]` |
| **Spec lanes** | Exactly 1 required |
| **Fast lanes** | At least 1 required |
| **Selectors** | `target("llvm"|"interpret"|"cpp"|"wasm")` — compile-time; `capability("...")` — runtime |
| **No selector** | Lane is always eligible |
| **Selection order** | Fast lanes scanned in declaration order, first match wins |
| **Fallback** | Spec lane is always eligible (fallback) |
| **Verify** | `verify random(N)` generates N random arg sets, compares all lanes against spec |
| **Mismatch telemetry** | `converge_mismatch_count()` — increments on spec-vs-fast divergence |
| **Interpreter** | Linear scan of lanes, direct body execution |
| **LLVM** | Static target resolution, runtime capability probes, cached lane selection |
| **Autotune cache** | 64-slot open-addressed hash table keyed by `hash(converge_key ^ shape_key ^ cpu)` |
| **Telemetry ring** | 64-slot circular buffer for lane selection timing data |
| **Capabilities** | `converge.dispatch` — auto-emitted when any converge exists |
| **Max lanes** | `KAIN_CONVERGE_LANE_MAX = 8` |
| **Stdlib** | `std::random::xoshiro_scramble` — the only stdlib converge |
| **Canonical example** | `blades/experiments/convergence/src/orchestrate.kn` — maze strategy selection |
| **Canonical perf example** | `benchmark/cases/scalar_mix/main.kn` — SIMD lane selection |
| **Source** | `crates/core/src/parser.rs` (parse), `crates/core/src/types.rs` (check), `runtime/native/src/core/converge.c` (autotune) |
