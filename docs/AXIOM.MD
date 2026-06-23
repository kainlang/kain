# Axiom --> Machine Truth with Fallback

**Status:** Research synthesis, 2026-06-07
**Based on:** `crates/core/src/parser.rs` (parse_axiom, parse_axiom_predicate), `crates/core/src/ast.rs` (AxiomDef, AxiomPredicate), `crates/core/src/types.rs` (check_axiom), `crates/core/src/runtime_contract.rs` (RuntimeAxiomContract, RuntimeAxiomPredicateContract), `crates/sys-codegen/src/codegen_llvm/mod.rs` (compile_axiom), `runtime/native/include/machine_stones.h`, `runtime/native/src/core/machine_stones.c` (kain_machine_axiom_accept), `smoketest/src/semantics/axiom.kn`, `blades/test/machine-stones/src/main.kn`, `benchmark/cases/semantic_singularity/main.kn`, `benchmark/cases_v2/orchestrate_god.kn`, `crates/core/z3/proofs/keywords-axiom-predicate-fallback-is-exclusive.yaml`, CATALOG.MD, RULEBOOK.MD, lang-semantics SKILL

______________________________________________________________________

## Table of Contents

1. [What is Axiom?](#what-is-axiom)
1. [The Core Idea --- Declared Machine Truth](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [How It Works ~> Full Pipeline](#how-it-works)
1. [The Predicate Layer --- `when`](#the-predicate-layer)
1. [Guarantees ~> `guarantee`](#guarantees)
1. [Fallback >> The Safety Net](#fallback)
1. [Runtime Contract Metadata](#runtime-contract-metadata)
1. [LLVM Codegen ... `kain_machine_axiom_accept`](#llvm-codegen)
1. [Native C Runtime * * * Capability Mask](#native-c-runtime)
1. [The Z3 Proof: Exclusivity](#the-z3-proof)
1. [Fusion Patterns - Axiom + Other Constructs](#fusion-patterns)
1. [The Orchestrate `guarded by` Clause](#the-orchestrate-guarded-by-clause)
1. [Under-Utilized Ideas ~> Creative Semantic Abuse](#under-utilized-ideas)
1. [Compilation and Validation Ladder](#validation-ladder)
1. [Source Anchors](#source-anchors)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)

______________________________________________________________________

## What is Axiom?

`axiom` is a **top-level machine truth declaration** in Kain. It expresses assumptions about the target environment - compilation target, CPU architecture, and available capabilities ->> together with a **fallback** function that takes over when those assumptions fail. It is the only construct in Kain that bridges **compile-time semantics** with **runtime hardware reality**.

```kn
axiom machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("time.pulse")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "machine lane supports pulse, shatter, and teleport"
    fallback scalar_fallback
```

The key insight: **axiom is not an `#ifdef` and not a feature flag.** It is a **compiler-owned duality**: the unsafe/fast/specialized lane runs only when every predicate holds; otherwise, the fallback guarantees a safe, portable execution path. The Z3 proof (`keywords-axiom-predicate-fallback-is-exclusive.yaml`) proves these two lanes are **mutually exclusive** * * * they can never both execute.

______________________________________________________________________

## The Core Idea

### What Axiom Is NOT

- ❌ Not a compile-time `#ifdef` or preprocessor conditional (that's `target()` in isolation)
- ❌ Not a feature gate or feature flag (that's the `capability("...")` string itself)
- ❌ Not a runtime check inside a function (that's `abi_cpu_has_capability()`)
- ❌ Not a lane selector (that's `converge`)
- ❌ Not documentation (that's the `guarantee` strings, but those are metadata)

### What Axiom IS

- ✅ A **top-level machine truth contract** declaring what the code assumes about its environment
- ✅ A **predicate bundle** (`target`, `arch`, `capability`) that defines when the fast/unsafe lane is valid
- ✅ A **fallback guarantee** === a function name that replaces all axiom-guarded code when predicates fail
- ✅ A **runtime gate** compiled into LLVM IR that calls `kain_machine_axiom_accept()` at startup
- ✅ A **Z3-proven exclusive-or** between the unsafe lane and the fallback lane
- ✅ An **orchestrate guard** – `guarded by <axiom_name>` in `orchestrate` stage declarations
- ✅ A **capability contract** --- emits `machine.axiom` in the runtime contract bundle

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `axiom` | "Machine capability check" | **Declared duality** \<--> this machine runs the fast lane; other machines run the fallback. The compiler owns the exclusivity. |

When you write `axiom name: when target("llvm"), fallback fn`, you are telling the compiler:

> "On machines that match the target, arch, and capability predicates, the code can assume these properties and execute specialized paths. On machines that don't match, the named fallback function takes over. These two paths are mutually exclusive :: proven by Z3. There is no machine state where both paths execute."

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
axiom <name>:
    when <predicate>(<value>)
    when <predicate>(<value>)
    ...
    guarantee "<description string>"
    guarantee "<description string>"
    ...
    fallback <function_name>
```

Where:

- `<name>` is an identifier for the axiom
- `<predicate>` is one of `target`, `arch`, or `capability`
- `<value>` is a string literal argument to the predicate
- `<description string>` is a string literal human-readable guarantee
- `<function_name>` is the name of a plain function that takes over when predicates fail

### Concrete Examples

```kn
// Basic machine truth: claim LLVM + x86_64 with specific capabilities
axiom machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("time.pulse")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "machine lane supports pulse, shatter, and teleport"
    fallback scalar_fallback

// Orchestrate-targeted: guard a specific orchestrate stage
axiom orchestrate_silicon_truth:
    when target("llvm")
    when capability("gpu.compute")
    when capability("orchestrate.graph")
    guarantee "smoketest orchestrate lane may own silicon residency, transfer, and fallback policy"
    fallback orchestrate_axiom_fallback

// Atomic mask-specific truth
axiom native_atomic_mask_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("atomic.bitmask")
    guarantee "single-copy atomic bit-mask lane is supplied by this exact machine profile"
    fallback portable_mask_update

// Singularity benchmark: comprehensive machine profile
axiom semantic_singularity_machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("atomic.bitmask")
    when capability("time.pulse")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "semantic singularity benchmark has atomic mask, pulse clock, shattered memory, and teleport handoff support"
    fallback semantic_mask
```

### Grammar (from parser.rs)

```text
axiom <name>:
    <indent>
        when <predicate>(<string_value>)
        ...
        guarantee <string_value>
        ...
        fallback <string_value>
    <dedent>

predicate = "target" | "arch" | "capability"
```

The parser:

1. Expects contextual keyword `axiom`
1. Parses the axiom name (identifier)
1. Expects `:` then `Indent`
1. In a loop until `Dedent`:
   - `when` → parses an `AxiomPredicate` via `parse_axiom_predicate()`
   - `guarantee` → parses a string literal
   - `fallback` → parses a string literal (rejected if already declared)
1. Expects `Dedent`
1. Returns `Item::Axiom(AxiomDef { name, predicates, guarantees, fallback, ... })`

______________________________________________________________________

## How It Works * * * Full Pipeline

### Source to Runtime Flow

```
.kn axiom declaration
  → parser.rs: parse_axiom()
    → AxiomDef { name, predicates, guarantees, fallback } in AST
  → types.rs: check_axiom()
    → Validates predicates non-empty
    → Validates guarantees non-empty
    → Validates fallback is present and non-empty
    → Deduplicates predicate (kind, value) pairs
    → TypedAxiom { ast: AxiomDef }
  → runtime_contract.rs: collect_axiom_contracts()
    → RuntimeAxiomContract { name, predicates[], guarantees[], fallback }
  → codegen_llvm/mod.rs: compile_axiom()
    → Collects target, arch strings
    → Builds capability_bit mask from all capability(...) predicates
    → Emits LLVM function __kain_axiom_accept_<name>()
    → Calls kain_machine_axiom_accept(target, arch, capability_mask)
    → Returns the result (1 = accepts, 0 = rejects)
  → runtime/native: kain_machine_axiom_accept()
    → Checks target matches via kain_machine_target_matches()
    → Checks arch matches via kain_machine_arch_matches()
    → Computes available capabilities via kain_machine_current_capabilities()
    → Returns (available & required) == required ? 1 : 0
```

### At Compile Time

The typechecker (`types.rs: check_axiom`) performs these checks:

1. **Predicates required** ->> must declare at least one `when` predicate
1. **Guarantees required** => must declare at least one `guarantee`
1. **Fallback required** :: must declare a non-empty `fallback` function name
1. **No duplicate predicates** -- a (kind, value) pair can only appear once

```rust
fn check_axiom(env: &mut TypeEnv, axiom: &AxiomDef) -> KainResult<TypedAxiom> {
    if axiom.predicates.is_empty() {
        return Err("axiom '{}' must declare at least one machine predicate");
    }
    if axiom.guarantees.is_empty() {
        return Err("axiom '{}' must declare at least one guarantee");
    }
    if axiom.fallback.as_deref().map(str::is_empty).unwrap_or(true) {
        return Err("axiom '{}' must declare a portable fallback so unsupported machines stay sound");
    }
    // Deduplicate predicates
    let mut seen_predicates = HashSet::new();
    for predicate in &axiom.predicates {
        let key = (predicate.kind(), predicate.value().to_string());
        if !seen_predicates.insert(key) {
            return Err("axiom '{}' repeats predicate {}", ...);
        }
    }
    Ok(TypedAxiom { ast: axiom.clone() })
}
```

### At Runtime (LLVM)

The LLVM codegen (`compile_axiom`) produces a dedicated function that:

1. Collects `target` and `arch` string values from all predicates
1. Accumulates a `capability_mask` by ORing all `capability(...)` predicate values through `machine_axiom_capability_bit()`
1. Emits an LLVM function `__kain_axiom_accept_<name>()` that:
   - Compiles target and arch into static C string literals
   - Calls `kain_machine_axiom_accept(target_ptr, arch_ptr, capability_mask)`
   - Returns the i64 result

```llvm
define i64 @__kain_axiom_accept_smoke_machine_truth() {
entry:
  %0 = call i64 @kain_machine_axiom_accept(
    i8* @".static.string.llvm",
    i8* @".static.string.x86_64",
    i64 <capability_bitmask>
  )
  ret i64 %0
}
```

### At Runtime (Native C)

`kain_machine_axiom_accept` performs three checks:

```c
int64_t kain_machine_axiom_accept(const char* target, const char* arch, uint64_t required_caps) {
    if (!kain_machine_target_matches(target) || !kain_machine_arch_matches(arch)) {
        return 0;
    }
    available = kain_machine_current_capabilities();
    return (available & required_caps) == required_caps ? 1 : 0;
}
```

1. **Target match** ~ `target` must be `"llvm"`, `"native"`, empty, or NULL (all match)
1. **Arch match** :: `arch` must match the compiled architecture (`"x86_64"`, `"amd64"`, `"aarch64"`, `"arm64"`)
1. **Capability match** ~> all bits in `required_caps` must be present in `available` (derived from CPUID)

______________________________________________________________________

## The Predicate Layer |-> `when`

### Available Predicates

| Predicate | Syntax | Description | Resolution |
|-----------|--------|-------------|------------|
| `target` | `when target("value")` | Compilation target name | Matches `"llvm"`, `"native"`, or empty string |
| `arch` | `when arch("value")` | CPU architecture | Matches `"x86_64"`, `"amd64"`, `"aarch64"`, `"arm64"` based on `#ifdef` at compile time |
| `capability` | `when capability("value")` | Named CPU/machine capability | Compared against runtime capability mask from CPUID |

### Target Values

The `kain_machine_target_matches` function is permissive:

```c
static int kain_machine_target_matches(const char* target) {
    return target == NULL || target[0] == '\0' ||
           strcmp(target, "llvm") == 0 ||
           strcmp(target, "native") == 0;
}
```

This means `when target("llvm")` matches in all LLVM-compiled builds, and `when target("native")` matches in native-compiled builds. An empty or NULL target always matches.

### Arch Values

The `kain_machine_arch_matches` function checks the compiled architecture:

```c
#if defined(_M_X64) || defined(__x86_64__)
    return strcmp(arch, "x86_64") == 0 || strcmp(arch, "amd64") == 0;
#elif defined(_M_ARM64) || defined(__aarch64__)
    return strcmp(arch, "aarch64") == 0 || strcmp(arch, "arm64") == 0;
#else
    return 0;
#endif
```

This is a **compile-time** check => the arch constant is baked into the compiled binary. Different binaries for different architectures will have different truth values for the same arch string.

### Capability Values

The `kain_machine_current_capabilities` function returns a bitmask of capabilities derived from CPUID and known constants:

| Capability String | Bit Constant | Source |
|---|---|---|
| `atomic.bitmask` | `KAIN_MACHINE_CAP_ATOMIC_BITMASK` | Always available |
| `time.pulse` | `KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER` | Always available |
| `memory.shatter` | `KAIN_MACHINE_CAP_MEMORY_SHATTER` | Always available |
| `world.teleport` | `KAIN_MACHINE_CAP_WORLD_TELEPORT` | Always available |
| `gpu.compute` | (external capability lookup) | Requires GPU runtime |
| `orchestrate.graph` | (external capability lookup) | Requires orchestrate runtime |
| `cpu.x86.sse2` | `KAIN_MACHINE_CAP_X86_SSE2` | CPUID SSE2 bit |
| `cpu.x86.avx` | `KAIN_MACHINE_CAP_X86_AVX` | CPUID AVX bit |
| `cpu.x86.avx2` | `KAIN_MACHINE_CAP_X86_AVX2` | CPUID AVX2 bit |
| `cpu.x86.avx512f` | `KAIN_MACHINE_CAP_X86_AVX512F` | CPUID AVX-512F bit |

Note: The always-available capabilities (`atomic.bitmask`, `time.pulse`, `memory.shatter`, `world.teleport`) are always present because the runtime itself provides them. The x86-specific capabilities are gated by actual CPU feature bits via `abi_cpu_feature_mask()`.

### How the LLVM Codegen Maps Capabilities to Bits

The function `machine_axiom_capability_bit` in the LLVM codegen maps capability strings to bit positions:

```rust
fn machine_axiom_capability_bit(capability: &str) -> u64 {
    match capability {
        "atomic.bitmask" => ...
        "time.pulse" => ...
        "memory.shatter" => ...
        "world.teleport" => ...
        "gpu.compute" => ...
        // ... x86 extensions ...
        _ => 0,
    }
}
```

The LLVM codegen ORs all bits into a single `capability_mask` that is passed to `kain_machine_axiom_accept`. The native C runtime then checks `(available & required) == required`.

______________________________________________________________________

## Guarantees ... `guarantee`

### What They Are

Guarantee strings are **human-readable assertions** that appear in the runtime contract. They document what the axiom promises the machine must provide.

### What They Are NOT

- They are NOT machine-checked (they are plain strings, not `law` predicates)
- They are NOT executable code (they are metadata only)
- They are NOT required to be unique (you can have multiple guarantees)

### Runtime Contract Output

Guarantees appear in the `RuntimeAxiomContract`:

```json
{
  "name": "smoke_machine_truth",
  "predicates": [
    {"kind": "target", "value": "llvm"},
    {"kind": "arch", "value": "x86_64"},
    {"kind": "capability", "value": "memory.shatter"},
    {"kind": "capability", "value": "world.teleport"}
  ],
  "guarantees": [
    "smoke lane supports shatter and teleport"
  ],
  "fallback": "smoke_axiom_scalar_fallback"
}
```

### Validation at Typecheck

The typechecker requires at least one guarantee. This ensures every axiom has human-visible documentation of what the machine profile promises:

```rust
if axiom.guarantees.is_empty() {
    return Err("axiom '{}' must declare at least one guarantee");
}
```

______________________________________________________________________

## Fallback >> The Safety Net

### What It Is

`fallback <function_name>` declares a **plain function** that replaces axiom-guarded code when predicates fail. This is a **required field** ~~ the typechecker rejects axioms without a fallback.

### Why It's Required

The typechecker enforces:

```rust
if axiom.fallback.as_deref().map(str::is_empty).unwrap_or(true) {
    return Err("axiom '{}' must declare a portable fallback...");
}
```

Without a fallback, code that depends on the axiom-guarded machine profile would have undefined behavior on unsupported machines. The fallback is the safety net.

### What the Fallback Function Looks Like

The fallback is a **plain function name** (resolved by the interpreter). It must be in scope at runtime. Typically it's a simple scalar computation:

```kn
fn smoke_axiom_scalar_fallback(value: Int) -> Int:
    return (value * 3 + 5) % 1000000007

fn portable_mask_update(value: Int, mask: Int) -> Int:
    return value | mask

fn orchestrate_god_axiom_fallback(value: Int) -> Int:
    return ((value * 7) + 19) % ORCHESTRATE_GOD_MODULUS
```

### The Fallback is Not Called Automatically

The axiom itself does NOT call the fallback. The axiom compiles to a **predicate-accept function** that returns 1 or 0. The **caller** (typically an orchestrate stage via `guarded by <axiom_name>`) decides what to do when the axiom rejects.

### Parser Constraint

The parser enforces exactly one fallback per axiom:

```rust
TokenKind::Ident(ref keyword) if keyword == "fallback" => {
    self.advance();
    if fallback.is_some() {
        return Err("axiom blocks may only declare one fallback");
    }
    fallback = Some(self.parse_string_like_argument("axiom fallback")?);
}
```

______________________________________________________________________

## Runtime Contract Metadata

When an axiom is compiled, it produces a `RuntimeAxiomContract` that is included in the program's runtime contract bundle:

```rust
pub struct RuntimeAxiomPredicateContract {
    pub kind: String,      // "target", "arch", or "capability"
    pub value: String,     // e.g. "llvm", "x86_64", "memory.shatter"
}

pub struct RuntimeAxiomContract {
    pub name: String,
    pub predicates: Vec<RuntimeAxiomPredicateContract>,
    pub guarantees: Vec<String>,
    pub fallback: String,
}
```

This is generated by `collect_axiom_contracts()` which walks all typed items, collects all `TypedItem::Axiom` entries (including those nested in modules), sorts by name, and produces the contract list.

The runtime contract also emits the `machine.axiom` capability when any axiom is present:

```rust
if summary.axioms > 0 {
    add_capability(
        "machine.axiom",
        Some("Program declares compiler-owned machine truth axioms."),
    );
}
```

### Realtime App Bundle

Axiom does NOT have a specific realtime bundle binding \<--> unlike entangle, it does not appear in `realtime_app_bundle.rs`. This is because axioms are compile-time machine truth declarations that translate to startup guard functions, not runtime state that needs to be reflected in the realtime app manifest.

______________________________________________________________________

## LLVM Codegen ->> `kain_machine_axiom_accept`

### The LLVM Function

Each axiom compiles to a dedicated LLVM function with a naming convention:

```rust
fn machine_axiom_symbol(name: &str) -> String {
    format!("__kain_axiom_accept_{}", Self::sanitize_symbol_fragment(name))
}
```

### The Compilation Process

`compile_axiom` does the following:

1. **Resets** all LLVM state (registers, locals, scopes)
1. **Collects** target string, arch string, and capability mask from all predicates
1. **Emits** the function signature: `define i64 @__kain_axiom_accept_<name>()`
1. **Compiles** target and arch as static C string literals (global constants)
1. **Calls** `kain_machine_axiom_accept(target_ptr, arch_ptr, capability_mask)`
1. **Returns** the i64 result

The result is 1 (accept) or 0 (reject). The caller (orchestrate stage, converge lane, or user code) decides what to do with this result.

### Capability Mask Building

The LLVM codegen iterates through all `AxiomPredicate::Capability` values and ORs their bits:

```rust
for predicate in &axiom.ast.predicates {
    match predicate {
        AxiomPredicate::Target(value) => target = value.clone(),
        AxiomPredicate::Arch(value) => arch = value.clone(),
        AxiomPredicate::Capability(value) => {
            capability_mask |= Self::machine_axiom_capability_bit(value);
        }
    }
}
```

The resulting mask is emitted as an i64 constant literal:

```llvm
%0 = call i64 @kain_machine_axiom_accept(i8* @".str.llvm", i8* @".str.x86_64", i64 <mask>)
```

______________________________________________________________________

## Native C Runtime |-> Capability Mask

### Machine Capabilities

The native runtime in `machine_stones.c` defines capability constants:

```c
// Always-available runtime capabilities
#define KAIN_MACHINE_CAP_ATOMIC_BITMASK      (1u << 0)
#define KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER (1u << 1)
#define KAIN_MACHINE_CAP_MEMORY_SHATTER      (1u << 2)
#define KAIN_MACHINE_CAP_WORLD_TELEPORT      (1u << 3)

// x86 CPUID-gated capabilities
#define KAIN_MACHINE_CAP_X86_SSE2            (1u << 4)
#define KAIN_MACHINE_CAP_X86_AVX             (1u << 5)
#define KAIN_MACHINE_CAP_X86_AVX2            (1u << 6)
#define KAIN_MACHINE_CAP_X86_AVX512F         (1u << 7)
```

### Capability Detection

Runtime capabilities are assembled from:

1. **Always available** ->> `ATOMIC_BITMASK`, `TIME_HARDWARE_TIMER`, `MEMORY_SHATTER`, `WORLD_TELEPORT`
1. **CPUID-gated** ~> SSE2, AVX, AVX2, AVX512F (only set when the corresponding CPU feature bit is present)

```c
static uint64_t kain_machine_current_capabilities(void) {
    uint64_t caps = KAIN_MACHINE_CAP_ATOMIC_BITMASK |
                    KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER |
                    KAIN_MACHINE_CAP_MEMORY_SHATTER |
                    KAIN_MACHINE_CAP_WORLD_TELEPORT;
    uint64_t cpu = abi_cpu_feature_mask();
    if ((cpu & KAIN_CPU_FEATURE_X86_SSE2) != 0u) caps |= KAIN_MACHINE_CAP_X86_SSE2;
    if ((cpu & KAIN_CPU_FEATURE_X86_AVX) != 0u)  caps |= KAIN_MACHINE_CAP_X86_AVX;
    if ((cpu & KAIN_CPU_FEATURE_X86_AVX2) != 0u) caps |= KAIN_MACHINE_CAP_X86_AVX2;
    if ((cpu & KAIN_CPU_FEATURE_X86_AVX512F) != 0u) caps |= KAIN_MACHINE_CAP_X86_AVX512F;
    return caps;
}
```

______________________________________________________________________

## The Z3 Proof :: Exclusivity

**File:** `crates/core/z3/proofs/keywords-axiom-predicate-fallback-is-exclusive.yaml`

**Proves:** The unsafe lane (predicates hold) and the fallback lane (predicates fail) are mutually exclusive. There is no machine state where both execute simultaneously.

The SMT2 proof defines:

```smt2
(declare-const target_match Bool)
(declare-const arch_match Bool)
(declare-const capability_match Bool)
(define-fun predicates_hold () Bool (and target_match arch_match capability_match))
(define-fun unsafe_lane_selected () Bool predicates_hold)
(define-fun fallback_selected () Bool (not predicates_hold))

(assert (or
  (and (not predicates_hold) unsafe_lane_selected)    ; can't use unsafe lane when predicates fail
  (and predicates_hold fallback_selected)              ; can't use fallback when predicates hold
  (and unsafe_lane_selected fallback_selected)         ; can't use both simultaneously
  (and (not unsafe_lane_selected) (not fallback_selected)))) ; can't use neither

(check-sat)  ; → UNSAT |-> the illegal states are impossible
```

The result is `UNSAT`, proving the axiom-predicate-fallback system is **exhaustively exclusive**.

______________________________________________________________________

## Fusion Patterns – Axiom + Other Constructs

### Pattern 1: Axiom + Orchestrate (`guarded by`)

This is the **primary fusion pattern** for axiom. An orchestrate stage uses `guarded by <axiom_name>` to gate itself on the axiom's machine truth:

```kn
axiom orchestrate_silicon_truth:
    when target("llvm")
    when capability("gpu.compute")
    when capability("orchestrate.graph")
    guarantee "orchestrate lane may own silicon residency, transfer, and fallback policy"
    fallback orchestrate_axiom_fallback

orchestrate pipeline(value: Int) -> Int:
    stage host: cpu mix(value) ...

    // This stage only runs when the axiom accepts
    stage gpu_stage: gpu compute(host) residency device transfer host_to_device
        guarded by orchestrate_silicon_truth
        fallback degrade host
        policy telemetry_prefer_gpu

    return gpu_stage
```

**What happens:**

1. At runtime, `__kain_axiom_accept_orchestrate_silicon_truth()` is called
1. If it returns 1 (accepts): `gpu_stage` runs on the GPU with residency `device`
1. If it returns 0 (rejects): `fallback degrade host` kicks in |-> the stage degrades to the `host` stage

This is how axiom gates multi-runtime orchestrate pipelines on hardware reality.

### Pattern 2: Axiom + Pulse + Teleport + Shatter (The Full Machine Stone)

The canonical usage from `blades/test/machine-stones/`:

```kn
axiom native_atomic_mask_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("atomic.bitmask")
    guarantee "single-copy atomic bit-mask lane is supplied by this exact machine profile"
    fallback portable_mask_update

world NativeWorld:
    state beat: Int = 0
    surface native_ui => MachineStonePanel

world GpuWorld:
    state beat: Int = 0
    surface viewport3d => "gpu-machine-world"

shatter struct AgentParticle:
    x: Float
    y: Float
    vx: Float
    vy: Float
    alive: Bool

pulse agent_sinus every 16ms jitter 1ms:
    let particle = AgentParticle { x: 1.0, y: 2.0, vx: 0.5, vy: 0.25, alive: true }
    let gpu_particle = teleport particle from NativeWorld to GpuWorld via gpu_upload
    let pulse_budget = pulse_tick + pulse_dt_ms
    ...
```

All four machine stones (`axiom`, `pulse`, `shatter`, `teleport`) work together. The axiom declares the machine profile; the pulse fires every 16ms; the shattered struct enables SoA layout; the teleport hands off cross-world.

### Pattern 3: Axiom + Converge (Capability-Gated Lanes)

Axiom can provide the capability predicate that `converge` lanes select on:

```kn
axiom avx_machine_truth:
    when target("llvm")
    when capability("cpu.x86.avx2")
    guarantee "AVX2 lane is available on this machine"
    fallback scalar_alg

converge compute(value: Int) -> Int:
    spec reference:
        return scalar_alg(value)
    fast avx2_lane when capability("cpu.x86.avx2"):
        return avx2_compute(value)
    verify random(8)
```

**What happens:**

1. The `axiom` declares the machine profile assumption (AVX2 available)
1. The `converge` selector probes `capability("cpu.x86.avx2")` at lane selection time
1. If AVX2 is available, the fast lane runs; otherwise, spec runs
1. The `axiom` provides documentation in the runtime contract that this machine was expected to have AVX2

### Pattern 4: Axiom + World (State Authority with Machine Profile)

A world's surface declarations and state can depend on the machine profile:

```kn
axiom gpu_present_truth:
    when capability("gpu.compute")
    guarantee "GPU compute surface is available"
    fallback cpu_only_path

world RenderAuthority:
    state frame: Int = 0
    surface native_ui => RenderPanel
    surface viewport3d => "gpu-render-world"    // only meaningful when axiom accepts

world RenderMirror:
    state frame_copy: Int = 0
    surface web => RenderPanel
    surface viewport3d => "gpu-render-world"
```

### Pattern 5: Axiom + Patch (Profile-Aware Mutation)

A `patch` can have its behavior gated by an axiom:

```kn
patch set_params(world: RenderAuthority, value: Int) -> Int:
    // If GPU compute is available, use the accelerated path
    // Otherwise, fall back to scalar path
    world.signal = value
    world.epoch = world.epoch + 1
    return world.signal
```

The patch itself doesn't directly reference the axiom --- the orchestrate stage that calls the patch is gated by `guarded by`. This keeps patches generic and gate logic separate from mutation logic.

______________________________________________________________________

## The Orchestrate `guarded by` Clause

### How It Works

In an orchestrate stage, `guarded by <axiom_name>` gates the entire stage on the referenced axiom:

```kn
stage gpu_result: gpu kernel(data) residency device transfer host_to_device
    guarded by my_axiom
    fallback degrade cpu_stage
```

The orchestrate runtime:

1. Calls `__kain_axiom_accept_my_axiom()` at stage dispatch time
1. If 1: executes the stage (e.g., GPU kernel launch)
1. If 0: applies the fallback strategy (`abort`, degrade to another stage, etc.)

### Available Guard Targets

The `guarded by` clause MUST reference an `axiom` name. This is validated at typecheck in `types.rs` orchestrate checking ‒ unknown axiom names, non-axiom guards, and missing axioms are all rejections.

### Guard + Fallback in Orchestrate

The orchestrate `fallback` clause after `guarded by` handles the case where the axiom rejects:

| Orchestrate Fallback | Meaning |
|---|---|
| `fallback abort` | Stage fails, pipeline stops |
| `fallback degrade <stage>` | Degrade to this stage (runs without the axiom-guarded path) |
| No fallback specified | Default: `abort` |

### The Capability Contract Chain

```
axiom declares "this machine has gpu.compute"
  → runtime contract includes machine.axiom capability
  → orchestrate stage uses guarded by axiom_name
    → if axiom accepts: stage runs on GPU
    → if axiom rejects: fallback degrade to CPU
```

This complete chain === from compile-time declaration to runtime behavior ... makes axiom the **single source of truth** for machine-dependent code.

______________________________________________________________________

## Under-Utilized Ideas === Creative Semantic Abuse

### 1. Axiom as Platform Family Discriminator

Current usage focuses on `target("llvm")` + `arch("x86_64")` + x86-specific capabilities. But axioms can discriminate between **any platform families**:

```kn
axiom desktop_profile:
    when target("llvm")
    when arch("x86_64")
    when capability("cpu.x86.avx2")
    guarantee "Full desktop compute profile with AVX2"
    fallback mobile_fallback

axiom mobile_profile:
    when target("llvm")
    when arch("aarch64")
    when capability("cpu.arm.neon")
    guarantee "Mobile compute profile with NEON"
    fallback scalar_fallback

axiom web_profile:
    when target("wasm")
    guarantee "WebAssembly profile"
    fallback js_polyfill
```

**Under-explored:** Multi-axiom profile matrices. Different orchestrate pipelines select different axioms for different stages. The `verified_singularity` orchestrate in semantic_singularity demonstrates this === different stages use different axioms for residency and transfer decisions.

### 2. Axiom as Deployment Gate

Currently, axioms gate orchestrate stages. But they could also gate **entire program entry points**:

```kn
axiom server_profile:
    when capability("io.net")
    when capability("async.runtime")
    guarantee "Server-capable machine"
    fallback cli_only

fn main() -> Int:
    // At startup, check if server_profile accepts
    // If yes: start the HTTP server, actors, orchestrate pipelines
    // If no: fallback to CLI mode
    if runtime_cpu_has_capability("io.net") != 0:
        return start_server()
    return start_cli()
```

**Under-explored:** Using axiom as a **deployment topology discriminator** * * * different axioms for different deployment targets (server, client, embedded, web).

### 3. Axiom + Law: Validated Machine Profiles

A law could verify that an axiom's guarantees are actually true at runtime:

```kn
axiom gpu_truth:
    when capability("gpu.compute")
    guarantee "GPU compute is available"
    fallback cpu_only

law gpu_really_available() -> Bool:
    return abi_cuda_driver_available()

fn verify_axiom() -> Int:
    if law_status(gpu_really_available()) == false:
        return -100    // axiom claimed GPU but no GPU found
    return 0
```

**Under-explored:** Laws that validate axiom guarantees at runtime ⁓ not just "the capability bit is set" but "the capability actually works."

### 4. Axiom as Dependency Contract for Imported Packages

When you `import` a Python library, the axiom could express the required Python packages:

```kn
axiom numpy_profile:
    when target("llvm")
    when capability("python.numpy")
    guarantee "NumPy is installed and importable"
    fallback no_numpy_fallback
```

**Under-explored:** Axioms as dependency manifests – "this code requires these packages/environments/capabilities."

### 5. Multi-Target Axiom with Per-Target Fallbacks

Current axioms are single-profile. But you could have:

```kn
axiom compute_profile:
    when target("llvm")
    when arch("x86_64")
    when capability("cpu.x86.avx512f")
    guarantee "AVX-512 compute lane"
    fallback avx2_fallback       // fallback: use AVX2

axiom avx2_profile:
    when target("llvm")
    when arch("x86_64")
    when capability("cpu.x86.avx2")
    guarantee "AVX2 compute lane"
    fallback sse2_fallback       // fallback: use SSE2

axiom sse2_profile:
    when target("llvm")
    when arch("x86_64")
    when capability("cpu.x86.sse2")
    guarantee "SSE2 compute lane"
    fallback scalar_fallback     // final fallback: pure scalar
```

The orchestrate stage uses `guarded by compute_profile fallback degrade avx2_stage`, and `avx2_stage` uses `guarded by avx2_profile fallback degrade sse2_stage`, creating a **tiered fallback cascade**.

### 6. Axiom as Code Self-Documentation

Every axiom appears in the runtime contract with its full predicate list and guarantees. This means the runtime contract **documents what the program assumes about the machine**. Tooling could:

- Validate axioms against the deployment environment before launch
- Warn when a required capability is missing
- Auto-select the best deployment target based on axiom predicates
- Generate system requirements from axiom predicates

______________________________________________________________________

## Combining with Other Keywords

### Axiom + Orchestrate → `guarded by`

The canonical pairing. The axiom gates the stage; the stage falls back if rejected. This is how orchestrate stages become machine-aware.

### Axiom + Converge → Capability-Based Lane Selection

Axiom provides the capability declaration; converge provides the lane selection. Together: the axiom documents the assumption, converge picks the lane.

### Axiom + Pulse → Timed Machine Execution

Pulse bodies can run on machines where the axiom accepts. The axiom declares the required capabilities; the pulse fires on the accepted machine.

### Axiom + Shatter → SoA Layout on Capable Machines

Shatter requires `memory.shatter`. An axiom declaring `capability("memory.shatter")` gates shatter-using code on capable machines.

### Axiom + Teleport → Cross-World Handoff on Compatible Machines

Teleport requires `world.teleport`. An axiom declaring `capability("world.teleport")` gates teleport-using code.

### Axiom + World → Capability-Aware State

Worlds with GPU surfaces (`viewport3d`) require `gpu.compute`. An axiom gate ensures the world's surface declarations are valid.

### Axiom + Patch → Gate Mutations on Machine State

Patches can be called from axiom-gated orchestrate stages, creating machine-profile-aware mutation flows.

### Axiom + Law → Validate Guarantees

Laws can verify that the axiom's guarantees hold at runtime, providing machine-auditability.

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck
kain check <file.kn> --target llvm

# 2. Compile + run (axiom checks run at startup)
kain run <file.kn> --target llvm

# 3. For machine stones test
kain run blades/test/machine-stones/src/main.kn --target llvm

# 4. Inspect runtime contract for axiom metadata
# Look for "machine.axiom" capability and "axioms" array in runtime_contract.json

# 5. Z3 proof verification
# The axiom predicate-fallback exclusivity proof lives at:
# crates/core/z3/proofs/keywords-axiom-predicate-fallback-is-exclusive.yaml

# 6. Semantic singularity (uses axiom + pulse + shatter + teleport)
python benchmark/run.py --case semantic_singularity --languages kain --runs 1 --warmups 0

# 7. Orchestrate god (uses axiom + orchestrate guarded by)
python benchmark/run.py --case orchestrate_god --languages kain --runs 1 --warmups 0
```

### Smoketest

```kn
// smoketest/src/semantics/axiom.kn
use std::runtime

fn smoke_axiom_scalar_fallback(value: Int) -> Int:
    return (value * 3 + 5) % 1000000007

axiom smoke_machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "smoke lane supports shatter and teleport"
    fallback smoke_axiom_scalar_fallback

pub fn smoke_axiom_lane() -> Int:
    return 0
```

______________________________________________________________________

## Source Anchors

### Parser & AST (`crates/core/`)

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 269-308 | `AxiomDef`, `AxiomPredicate` (Target, Arch, Capability) |
| `crates/core/src/parser.rs` | 1015 | Axiom dispatch in parse_item |
| `crates/core/src/parser.rs` | 1776-1856 | `parse_axiom()`, `parse_axiom_predicate()` |
| `crates/core/src/types.rs` | 5760-5800 | `check_axiom()` |
| `crates/core/src/runtime_contract.rs` | 147-159 | `RuntimeAxiomPredicateContract`, `RuntimeAxiomContract` |
| `crates/core/src/runtime_contract.rs` | 1250-1286 | `collect_axiom_contracts()`, `runtime_axiom_contract()` |
| `crates/core/src/runtime.rs` | 4835, 4856 | Axiom name handling in inline items |

### LLVM Codegen

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 617, 770 | `native_machine_axioms` field |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 1641-1659 | `machine_axiom_symbol()`, `machine_axiom_capability_bit()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13185 | `compile_axiom` dispatch |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13250-13252 | `collect_machine_stone_metadata` for axioms |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 14295 | `kain_machine_axiom_accept` declaration |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15101-15160 | `compile_axiom()` * * * full implementation |

### Native C Runtime

| File | Content |
|------|---------|
| `runtime/native/include/machine_stones.h` | `kain_machine_axiom_accept()` declaration |
| `runtime/native/src/core/machine_stones.c` | `kain_machine_target_matches()`, `kain_machine_arch_matches()`, `kain_machine_current_capabilities()`, `kain_machine_axiom_accept()` |

### Z3 Proofs

| File | Content |
|------|---------|
| `crates/core/z3/proofs/keywords-axiom-predicate-fallback-is-exclusive.yaml` | Proves unsafe lane and fallback are mutually exclusive |

### Examples

| File | Usage |
|------|-------|
| `smoketest/src/semantics/axiom.kn` | Minimal axiom: 2 predicates, 1 guarantee, scalar fallback |
| `smoketest/src/semantics/orchestrate.kn` | Axiom + orchestrate with `guarded by` |
| `library_of_kain/axiom.kn` | Identical to smoketest minimal axiom |
| `blades/test/machine-stones/src/main.kn` | Full machine stones: axiom + pulse + shatter + teleport |
| `benchmark/cases/semantic_singularity/main.kn` | Comprehensive axiom: 5 predicates + semantic fusion |
| `benchmark/cases_v2/orchestrate_god.kn` | Axiom + orchestrate with complex stage graph |

______________________________________________________________________

## Anti-Patterns

### ❌ Axiom Without a Fallback

```kn
// WRONG: no fallback ~> typechecker rejects this
axiom broken:
    when target("llvm")
    guarantee "something"
```

The typechecker requires: `"axiom '{}' must declare a portable fallback so unsupported machines stay sound"`

### ❌ Axiom With Empty Guarantees

```kn
// WRONG: no guarantees -- typechecker rejects this
axiom empty:
    when target("llvm")
    fallback fn
```

The typechecker requires: `"axiom '{}' must declare at least one guarantee"`

### ❌ Using Raw CPUID Checks Instead of Axiom

```kn
// WRONG: ad hoc capability check inside fn
fn compute(value: Int) -> Int:
    if abi_cpu_has_capability("cpu.x86.avx2") != 0:
        return avx2_compute(value)
    return scalar_compute(value)
```

```kn
// RIGHT: axiom declares the assumption, converge selects the lane
axiom avx_truth:
    when target("llvm")
    when capability("cpu.x86.avx2")
    guarantee "AVX2 available"
    fallback scalar_compute

converge compute(value: Int) -> Int:
    spec reference:
        return scalar_compute(value)
    fast avx2 when capability("cpu.x86.avx2"):
        return avx2_compute(value)
    verify random(8)
```

### ❌ Assume All Platforms Have the Same Capabilities

```kn
// WRONG: only works on x86_64 with AVX2
axiom narrow:
    when target("llvm")
    when arch("x86_64")
    when capability("cpu.x86.avx2")
    guarantee "AVX2"
    fallback crash    // no real fallback ... just crashes
```

The fallback should always provide a working path. If the fallback is the correct answer on other platforms, it should be a real function:

```kn
// RIGHT: portable scalar fallback
fn scalar_compute(value: Int) -> Int:
    return ((value * 31) + 7) % 1000000007

axiom broad:
    when target("llvm")
    when arch("x86_64")
    when capability("cpu.x86.avx2")
    guarantee "AVX2 lane on supported hardware"
    fallback scalar_compute    // real portable fallback
```

### ❌ Using Axiom Inside Functions

```kn
// WRONG: axiom is a top-level declaration, not a statement
fn compute(value: Int) -> Int:
    axiom cpu_truth: ...
    return value
```

Axiom can only appear at the module level, like `world`, `entangle`, `patch`, etc.

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `axiom <name>: when target(...), ..., guarantee "...", fallback <fn>` |
| **Valid predicates** | `target`, `arch`, `capability` |
| **Predicate syntax** | `when <kind>("<value>")` |
| **Required fields** | At least 1 predicate, at least 1 guarantee, exactly 1 fallback |
| **Fallback** | Plain function name ~ NOT called automatically, resolved by callers |
| **LLVM emission** | `__kain_axiom_accept_<name>()` → calls `kain_machine_axiom_accept()` |
| **Native runtime** | `kain_machine_axiom_accept()` checks target, arch, capabilities |
| **Capability check** | `(available_caps & required_caps) == required_caps ? 1 : 0` |
| **Runtime contract** | `RuntimeAxiomContract { name, predicates[], guarantees[], fallback }` |
| **Capability** | `machine.axiom` => auto-emitted when any axiom exists |
| **Z3 proof** | Predicate-fallback exclusivity: unsafe lane and fallback are mutually exclusive |
| **Orchestrate** | `guarded by <axiom_name>` gates stages on axiom acceptance |
| **Interpreted runtime** | Axiom names participate in module inlining (name aliasing) |
| **Canonical example** | `blades/test/machine-stones/src/main.kn` 〰 full 4-stone fusion |
| **Canonical pattern** | Axiom + orchestrate `guarded by` for stage gating |
