# Pulse — First-Class Temporal Beat

**Status:** Research synthesis, 2026-06-07
**Based on:** `crates/core/src/parser.rs`, `crates/core/src/ast.rs`, `crates/core/src/types.rs`, `crates/core/src/runtime_contract.rs`, `crates/core/src/runtime.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `runtime/native/include/machine_stones.h`, `runtime/native/src/core/machine_stones.c`, `libclang` ABI extraction, `stdlib/runtime.kn`, `stdlib/intent.kn`, `smoketest/src/semantics/pulse.kn`, `library_of_kain/pulse.kn`, `blades/test/machine-stones/src/main.kn`, `benchmark/cases/pulse_teleport_decay_mesh/main.kn`, `blades/python/24_tet/src/resonate_py_effects.kn`, `blades/sims/fluid-studio/src/*.kn`, Z3 proof surfaces, CATALOG.MD, RULEBOOK.MD, lang-semantics SKILL, lang-systems SKILL

______________________________________________________________________

## Table of Contents

1. [What is Pulse?](#what-is-pulse)
1. [The Core Idea — Compiler-Owned Temporal Semantics](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [How It Works — Full Pipeline](#how-it-works)
1. [The Body Environment — pulse_tick, pulse_dt_ms, pulse_missed](#the-body-environment)
1. [Duration Unit Reference](#duration-unit-reference)
1. [Effect Model](#effect-model)
1. [Runtime Contract Metadata](#runtime-contract-metadata)
1. [LLVM Codegen — Two-Function Fire Wrapper](#llvm-codegen)
1. [Native C Runtime — The Scheduler Thread](#native-c-runtime)
1. [Stdlib Telemetry API](#stdlib-telemetry-api)
1. [The Pulset Clock Patterns](#the-pulse-clock-patterns)
1. [Under-Utilized Ideas — Creative Semantic Abuse](#under-utilized-ideas)
1. [Combining with Other Keywords](#combining-with-other-keywords)
1. [Validation Ladder](#validation-ladder)
1. [Z3 Proofs](#z3-proofs)
1. [Source Anchors](#source-anchors)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)

______________________________________________________________________

## What is Pulse?

`pulse` is a **top-level declaration** in Kain that establishes a **compiler-owned temporal beat** — a block of code that the runtime executes on a recurring schedule with jitter tolerance, missed-beat tracking, and first-class timing locals. It is **not** a hand-rolled `while` + `sleep` loop, an event callback, or an OS timer abstraction. It is a **first-class temporal semantic** where the compiler and runtime own the scheduling, not the programmer.

```kn
pulse render_clock every 16ms jitter 2ms:
    let next = Authority.frame + 1
    let committed = commit_frame(Authority, next)
    let _shape = frame_score(committed) + pulse_tick + pulse_dt_ms + pulse_missed
```

The key insight: **pulse replaces `while true { sleep(N); body() }` with a compiler-owned temporal construct.** The runtime manages the scheduler thread, the timing arithmetic, the missed-beat counter, and the jitter window — all proven correct by Z3 for the missed-beat bounds invariant. Your code gets `pulse_tick`, `pulse_dt_ms`, and `pulse_missed` as guaranteed locals bound to the actual wall-clock timing.

______________________________________________________________________

## The Core Idea

### What Pulse IS NOT

- ❌ Not a hand-rolled `while` + `sleep` loop (no jitter tolerance, no missed-beat tracking)
- ❌ Not an OS timer callback (no compiler-owned scheduling, no semantic locals)
- ❌ Not an async/await timer (no state-machine overhead, no wake/poll cycle)
- ❌ Not a frame clock owned by the graphics layer (that's `std::graphics` frame loop)
- ❌ Not an event handler (that's `resonate` or actor `on` handlers)
- ❌ Not a CPU spin loop (the scheduler thread sleeps between beats)

### What Pulse IS

- ✅ A **compiler-owned temporal beat** — the runtime owns the scheduler thread
- ✅ A **jitter-tolerant periodic execution** — `jitter Nms` tolerates minor timing variance
- ✅ A **first-class timing environment** — `pulse_tick`, `pulse_dt_ms`, `pulse_missed` are guaranteed locals
- ✅ A **Z3-proven missed-beat bound** — `pulse_missed` never exceeds elapsed beats, never negative
- ✅ A **thread-once lifetime** — the scheduler thread starts on the first registered pulse
- ✅ A **native OS thread** — on Windows via `CreateThread`, on POSIX via `pthread_create`
- ✅ **Immediate first fire** — every pulse fires once immediately on registration, then on schedule
- ✅ **Broad effect permissions** — IO, Async, GPU, Reactive, Unsafe, Alloc, Panic are all allowed
- ✅ **Direct world access** — pulse bodies can read and write world state directly
- ✅ **64-slot bounded slot table** — open-addressed hash, power-of-two, lock-protected

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `pulse` | "Frame clock / game loop" | **Temporal driver** — any recurring beat that owns scheduling, jitter tolerance, and timing telemetry, with Z3-proven arithmetic and compiler-emitted fire wrappers |

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
pulse <name> every <duration> [jitter <duration>]:
    <body block>
```

Where:

- `<name>` is a user-defined identifier
- `every` is a contextual keyword
- `<duration>` is `Integer Unit` (e.g., `16ms`, `250us`, `1s`, `8tick`)
- `jitter` is an optional contextual keyword
- `<body block>` is a standard block `{ ... }`

### Concrete Examples

```kn
// Frame clock — 60 FPS (~16.67ms) with 2ms jitter tolerance
pulse render_clock every 16ms jitter 2ms:
    let next = Authority.frame + 1
    let committed = commit_frame(Authority, next)
    let _shape = frame_score(committed) + pulse_tick + pulse_dt_ms + pulse_missed

// LFO modulation tick — 120 Hz (~8.33ms)
pulse fx_modulation_tick every 8ms jitter 1ms:
    let dt: Int = pulse_dt_ms
    if dt < 1:
        dt = 8
    let advance1: Int = FxWorld.lfo1_rate * dt / 10
    FxWorld.lfo1_phase = (FxWorld.lfo1_phase + advance1) % (PHASE_MAX + 1)

// High-speed pulse — 250 microseconds
pulse fast_tick every 250us:
    let _now = pulse_tick

// Low-speed pulse — 1 second
pulse heartbeat every 1s:
    let status = check_health()
    HealthAuthority.heartbeat_epoch = HealthAuthority.heartbeat_epoch + 1

// Tick-based (platform-relative)
pulse platform_tick every 4tick:
    let _clock = pulse_tick
```

### Grammar (from parser.rs)

```text
pulse <name> every <integer> <unit> [jitter <integer> <unit>]: <block>

Integer = literal Int token
unit    = ident ("ns" | "us" | "ms" | "s" | "tick" | "ticks")
```

The parser flow:

1. Expects contextual keyword `pulse`
1. Parses name ident
1. Expects contextual keyword `every`
1. Calls `parse_pulse_duration()` — reads integer token + ident token → `PulseDuration{value, unit}`
1. If `jitter` contextual keyword follows, parses another pulse duration
1. Expects colon
1. Parses block
1. Returns `Item::Pulse(PulseDef { name, interval, jitter, body, ... })`

______________________________________________________________________

## How It Works — Full Pipeline

### Source to Runtime Flow

```
.kn pulse declaration
  → parser.rs: parse_pulse()
    → PulseDef { name, interval, jitter, body } in AST
  → types.rs: check_pulse()
    → Validates duration values > 0, valid units
    → Creates scope with pulse_tick (I64), pulse_dt_ms (I64), pulse_missed (I64)
    → Validates body with broad effect set
    → TypedPulse { ast }
  → runtime_contract.rs: collect_pulse_contracts()
    → RuntimePulseContract { name, interval, interval_ms, jitter, body_ownership_ops, body_teleports }
    → Emits capability "time.pulse" + "time.hardware-timer"
  → runtime.rs: register_pulse_value()
    → Pulse items are registered but no-op in the interpreter (native-only construct)
  → codegen_llvm/mod.rs: compile_pulse()
    → Emits two LLVM functions:
       1. __kain_pulse_body_{name}(pulse_tick, pulse_dt_ms, pulse_missed)
       2. __kain_pulse_fire_{name}() — calls pulse_snapshot, loads locals, invokes body
  → codegen_llvm/mod.rs: emit_machine_stones_entry_preamble()
    → Emits call to kain_machine_pulse_start() for each pulse (after world init)
  → runtime/native: machine_stones.c
    → kain_machine_pulse_start() — registers pulse in 64-slot table, starts scheduler thread
    → Scheduler thread loop — checks slots, fires due pulses, sleeps idle
    → kain_machine_pulse_snapshot() — computes tick/dt/missed timing values
    → kain_machine_pulse_stop_all() — stops scheduler, joins thread, registered at atexit()
```

### At Compile Time

The typechecker (`types.rs: check_pulse`) performs:

1. **Duration validation** — interval value must be > 0
1. **Unit validation** — unit must be `ns`, `us`, `ms`, `s`, `tick`, or `ticks`
1. **Jitter validation** — if present, jitter value must be > 0 and unit must be valid
1. **Body typechecking** — body is compiled as a unit-returning block with broad effects
1. **Local injection** — `pulse_tick`, `pulse_dt_ms`, `pulse_missed` are defined as `Int(I64)` in the body scope

```rust
fn check_pulse(env: &mut TypeEnv, pulse: &PulseDef) -> KainResult<TypedPulse> {
    validate_pulse_duration(env, &pulse.interval, "pulse interval")?;
    if let Some(jitter) = &pulse.jitter {
        validate_pulse_duration(env, jitter, "pulse jitter")?;
    }

    let ctx = SemanticContext {
        function_name: pulse.name.clone(),
        return_type: ResolvedType::Unit,
        effects: EffectSet::new()
            .with(Effect::IO)
            .with(Effect::Async)
            .with(Effect::GPU)
            .with(Effect::Reactive)
            .with(Effect::Unsafe)
            .with(Effect::Alloc)
            .with(Effect::Panic),
    };
    env.with_scope(|env| {
        env.define("pulse_tick", ResolvedType::Int(IntSize::I64));
        env.define("pulse_dt_ms", ResolvedType::Int(IntSize::I64));
        env.define("pulse_missed", ResolvedType::Int(IntSize::I64));
        check_block_semantics(env, &pulse.body, &ctx)
    })?;

    Ok(TypedPulse { ast: pulse.clone() })
}
```

### At Runtime (Interpreter)

The interpreter handles `Item::Pulse(_)` as a no-op — pulse is a **native-only semantic**. This means pulse bodies do not execute in `kain check` or interpreter mode. They only execute when compiled to native LLVM and run through the machine stones runtime.

```rust
// runtime.rs — pulse is registered but not interpreted
Item::Pulse(_) => {}
TypedItem::Pulse(_) => {}
```

This is intentional. Pulse **IS** the native temporal lane. The interpreter would need a real OS timer thread to faithfully implement pulse — and at that point you're running native code anyway.

______________________________________________________________________

## The Body Environment — pulse_tick, pulse_dt_ms, pulse_missed

Every pulse body has exactly three auto-injected locals:

### `pulse_tick: Int` (I64)

- **Monotonic counter** — increments on every scheduled beat
- **Starts at 0** on first fire
- **Never decrements or resets** for the life of the pulse
- Use for: frame counters, animation indices, phase accumulation, beat-division logic

### `pulse_dt_ms: Int` (I64)

- **Actual elapsed time** since last beat (or first beat) in milliseconds
- **Approximately equals the interval** under normal conditions
- **Differs from interval** when the scheduler is delayed (OS scheduling, heavy load)
- Use for: time-based animation, physics integration, LFO phase advance, anything that needs delta time

### `pulse_missed: Int` (I64)

- **Number of missed beats** since the last fire
- **Zero under normal conditions** — at least two full intervals must have elapsed
- **Non-zero when overloaded** — the Z3 proof guarantees `pulse_missed <= advanced` and never negative
- Use for: overload detection, adaptive quality scaling, degraded-mode fallback

### The Jitter Window

`pulse_missed` uses the jitter tolerance. If the elapsed time exceeds `interval + jitter`, the beat is considered delayed. If more than one full interval has elapsed (regardless of jitter), the excess beats are counted as missed.

```c
// From machine_stones.c: kain_machine_pulse_snapshot()
uint64_t tolerated = interval_ns + jitter_ns;
*out_missed = (advanced > 1u && elapsed_ns > tolerated) ? advanced - 1u : 0u;
```

The Z3 proof (`native-machine-pulse-missed-beat-stays-bounded.yaml`) verifies:

- `missed >= 0` (never negative)
- `missed <= advanced` (never exceeds total elapsed beats)
- `missed == 0` when `advanced <= 1` (single beat never counts as missed)

### Canonical Body Usage

```kn
pulse fx_modulation_tick every 8ms jitter 1ms:
    // Safe delta-time — guaranteed positive
    let dt: Int = pulse_dt_ms
    if dt < 1:
        dt = 8                       // fallback if timing is anomalous

    // Phase advance from rate * dt
    let advance: Int = FxWorld.lfo1_rate * dt / 10
    FxWorld.lfo1_phase = (FxWorld.lfo1_phase + advance) % (PHASE_MAX + 1)

    // Slow-cycle work at 1/32th frequency using tick counter
    if pulse_tick % 32 == 0:
        FxWorld.fx_epoch = FxWorld.fx_epoch + 1
```

______________________________________________________________________

## Duration Unit Reference

### Supported Units

| Unit | Alias | Description | Canonical Example |
|------|-------|-------------|-------------------|
| `ns` | — | Nanoseconds | `pulse fast every 500ns:` |
| `us` | — | Microseconds | `pulse tick every 250us:` |
| `ms` | — | Milliseconds | `pulse frame every 16ms:` |
| `s` | — | Seconds | `pulse heartbeat every 1s:` |
| `tick` | `ticks` | Platform-relative tick | `pulse clock every 4tick:` |

### Unit Conversion Table (to ns)

| Unit | 1 unit in ns | LLVM codegen formula |
|------|-------------|----------------------|
| `ns` | 1 | `value` (as-is) |
| `us` | 1,000 | `value * 1_000` |
| `ms` | 1,000,000 | `value * 1_000_000` |
| `s` | 1,000,000,000 | `value * 1_000_000_000` |
| `tick` / `ticks` | 1 | `value` (as-is) |

### Unit Conversion Table (to ms, for runtime contract)

| Unit | 1 unit in ms | Runtime contract formula |
|------|-------------|--------------------------|
| `ns` | 1/1,000,000 | `value / 1_000_000` (min 1) |
| `us` | 1/1,000 | `value / 1_000` (min 1) |
| `ms` | 1 | `value` (as-is) |
| `s` | 1,000 | `value * 1_000` |
| `tick` / `ticks` | 1 | `value` (as-is) |

______________________________________________________________________

## Effect Model

Pulse bodies are permitted the broadest effects of any Kain construct:

| Effect | Why Allowed |
|--------|-------------|
| `IO` | Console output, logging, file I/O during pulses |
| `Async` | Spawning async tasks from a timed beat |
| `GPU` | Dispatch calls from timed rendering/animation ticks |
| `Reactive` | Writing to world fields that have resonate handlers |
| `Unsafe` | Raw memory operations, pointer arithmetic, atomics |
| `Alloc` | Heap allocation within pulse bodies |
| `Panic` | Fatal error handling within pulse bodies |

**Pulse bodies accept ALL effects** — they are the most permissive semantic construct in Kain, on par with `resonate`. This is because code that runs on a timed schedule needs full access to the program's capabilities.

______________________________________________________________________

## Runtime Contract Metadata

When a pulse is compiled, it produces a `RuntimePulseContract`:

```rust
pub struct RuntimePulseContract {
    pub name: String,              // "fx_modulation_tick"
    pub interval: String,          // "8ms"
    pub interval_ms: u64,          // 8
    pub jitter: Option<String>,    // Some("1ms")
    pub body_ownership_ops: bool,  // true if body uses collapse/observe/decay
    pub body_teleports: bool,      // true if body uses teleport
}
```

### Capabilities Emitted

When any pulse is present in the program:

| Capability Key | Provider | Description |
|----------------|----------|-------------|
| `time.pulse` | `kain-core` | "Program declares first-class temporal pulse execution beats." |
| `time.hardware-timer` | `runtime/native` | "Pulse contracts can lower to native timer-backed scheduling lanes." |

### Contract Collection

All pulses (including those nested in modules) are collected, sorted by name, and included in the program's runtime contract manifest:

```rust
fn runtime_pulse_contract(pulse: &TypedPulse) -> RuntimePulseContract {
    RuntimePulseContract {
        name: pulse.ast.name.clone(),
        interval: pulse.ast.interval.as_authored(),
        interval_ms: pulse_duration_to_millis(&pulse.ast.interval),
        jitter: pulse.ast.jitter.as_ref().map(PulseDuration::as_authored),
        body_ownership_ops: block_contains_ownership_expr(&pulse.ast.body),
        body_teleports: block_contains_teleport_expr(&pulse.ast.body),
    }
}
```

______________________________________________________________________

## LLVM Codegen — Two-Function Fire Wrapper

The LLVM codegen emits **two functions** per pulse:

### Function 1: Body Function

```llvm
define void @__kain_pulse_body_render_clock(i64 %pulse_tick_arg, i64 %pulse_dt_ms_arg, i64 %pulse_missed_arg) {
entry:
  %pulse_tick.addr = alloca i64
  store i64 %pulse_tick_arg, i64* %pulse_tick.addr
  %pulse_dt_ms.addr = alloca i64
  store i64 %pulse_dt_ms_arg, i64* %pulse_dt_ms.addr
  %pulse_missed.addr = alloca i64
  store i64 %pulse_missed_arg, i64* %pulse_missed.addr
  ; ... compiled pulse body ...
  ret void
}
```

This is the user's pulse body, compiled as a `void(i64, i64, i64)` function. The three arguments are stored as alloca'd locals, allowing the body to reference `pulse_tick`, `pulse_dt_ms`, and `pulse_missed` as if they were normal variables.

### Function 2: Fire Wrapper

```llvm
define void @__kain_pulse_fire_render_clock() {
entry:
  %pulse.tick.out = alloca i64
  %pulse.dt.out = alloca i64
  %pulse.missed.out = alloca i64
  call void @kain_machine_pulse_snapshot(i64 <token>, i64 <interval_ns>, i64 <jitter_ns>,
                                          i64* %pulse.tick.out, i64* %pulse.dt.out, i64* %pulse.missed.out)
  %tick = load i64, i64* %pulse.tick.out
  %dt = load i64, i64* %pulse.dt.out
  %missed = load i64, i64* %pulse.missed.out
  call void @__kain_pulse_body_render_clock(i64 %tick, i64 %dt, i64 %missed)
  ret void
}
```

This is the function pointer passed to the native C runtime's `kain_machine_pulse_start()`. It:

1. Allocates three output slots
1. Calls `kain_machine_pulse_snapshot()` which fills the slots with timing values
1. Loads the values
1. Calls the body function

### Pulse Token (Stable Hash)

Each pulse gets a stable 64-bit hash token from its name:

```rust
let token = Self::stable_runtime_hash64(&pulse.ast.name);
```

This token is used to identify the pulse in the native slot table. The LLVM codegen also uses it when emitting the entry preamble:

```llvm
; Emitted in main() entry preamble (after world init, before any logic)
call i64 @kain_machine_pulse_start(i64 <token>, i64 <interval_ns>, i64 <jitter_ns>,
                                    void ()* @__kain_pulse_fire_render_clock)
```

### Pulse Start Ordering

The LLVM codegen emits `kain_machine_pulse_start()` calls in the entry preamble **after** all world initialization but **before** any user code. Each pulse registers itself:

1. The runtime finds/creates a slot via the token
1. Sets the slot's interval, jitter, fire function pointer
1. Starts the scheduler thread (first pulse only)
1. **Fires immediately once** (the initial fire outside the scheduler loop)

______________________________________________________________________

## Native C Runtime — The Scheduler Thread

### Slot Table

```c
#define KAIN_MACHINE_PULSE_SLOT_COUNT 64u
#define KAIN_MACHINE_PULSE_SLOT_MASK (KAIN_MACHINE_PULSE_SLOT_COUNT - 1u)

typedef struct KainMachinePulseSlot {
    uint64_t token;           // stable hash token from name
    uint64_t last_ns;         // last fire timestamp (nanoseconds)
    uint64_t tick;            // monotonic tick counter
    uint64_t interval_ns;     // scheduled interval in nanoseconds
    uint64_t jitter_ns;       // jitter tolerance window
    uint64_t next_due_ns;     // next scheduled fire time
    uint64_t fire_count;      // total fires (for telemetry)
    KainMachinePulseFireFn fire;  // function pointer to fire wrapper
    uint8_t occupied;         // slot is in use
    uint8_t scheduled;        // slot is actively scheduled
} KainMachinePulseSlot;

static KainMachinePulseSlot g_machine_pulse_slots[KAIN_MACHINE_PULSE_SLOT_COUNT];
```

**Design:** 64 open-addressed slots, power-of-two for fast masking. Slot lookup is by token hash with linear probing.

### Scheduler Thread Lifecycle

```
First pulse_registered
  → kain_machine_pulse_ensure_scheduler_started()
    → CreateThread (Windows) / pthread_create (POSIX)
    → Thread enters scheduler loop

Scheduler loop:
  while (!stop_requested):
    scan all 64 slots
    for each scheduled slot where now >= next_due:
      add to fire queue
      advance next_due by interval
    release lock
    for each job in fire queue:
      record fire (atomic increment)
      call fire()  →  __kain_pulse_fire_{name}()  →  snapshot + body
    if no jobs:
      sleep(min(next_due - now), idle_sleep_ns)

Stop:
  → stop_requested = 1
  → WaitForSingleObject / pthread_join
  → clear all slots
```

The scheduler thread is **started once** — on the first `kain_machine_pulse_start()` call — and stopped at `atexit()` via `kain_machine_pulse_stop_all()`.

### The Fire Queue

Each scheduler iteration:

1. Holds the spinlock
1. Scans all 64 slots
1. Queues any slot whose `next_due_ns <= now_ns`
1. Advances `next_due_ns` by `interval_ns` for each queued slot
1. Releases the spinlock
1. Fires each queued job **outside the lock**

This is a batched fire pattern — the lock is held only during scan+Scheduling, not during body execution.

### Sleep Calculation

When no jobs are due:

- Compute `sleep_ns = min(all next_due - now, KAIN_MACHINE_PULSE_SCHEDULER_IDLE_SLEEP_NS)`
- `KAIN_MACHINE_PULSE_SCHEDULER_IDLE_SLEEP_NS = 1_000_000` (1ms)
- Sleep via `kain_machine_sleep_ns()` (platform-specific `Sleep`/`nanosleep`/`usleep`)

### The Snapshot Function

```c
void kain_machine_pulse_snapshot(uint64_t pulse_token, uint64_t interval_ns,
                                   uint64_t jitter_ns, uint64_t* out_tick,
                                   int64_t* out_dt_ms, uint64_t* out_missed) {
    uint64_t now_ns = kain_machine_now_ns();
    // Find slot, compute elapsed/advanced
    if (slot->last_ns == 0u) {     // first fire
        slot->last_ns = now_ns;
        slot->tick = 0u;
        elapsed_ns = interval_ns;   // treat first beat as full interval
        advanced = 1u;
    } else {
        elapsed_ns = now_ns >= slot->last_ns ? now_ns - slot->last_ns : 0u;
        advanced = elapsed_ns / interval_ns;
        if (advanced > 0u) {
            slot->tick += advanced;
            slot->last_ns = now_ns;
        }
    }
    *out_tick = slot->tick;
    *out_dt_ms = elapsed_ns / 1_000_000;  // ns → ms
    // Z3-proven missed beat bound:
    *out_missed = (advanced > 1u && elapsed_ns > tolerated) ? advanced - 1u : 0u;
}
```

______________________________________________________________________

## Stdlib Telemetry API

### Total Fire Count

The telemetry counter is accessed through `std::runtime`:

```kn
use std::runtime

fn prove_pulse_fired() -> Int:
    let fires = runtime_machine_pulse_total_fire_count()
    if fires < 1:
        return 7    // pulse never fired
    return 0
```

The native C function is:

```c
uint64_t kain_machine_pulse_total_fire_count(void) {
    return atomic_load_explicit(&g_machine_pulse_total_fire_count, memory_order_acquire);
}
```

### No Dedicated `std::pulse` Module

Unlike `entangle` (which has `std::intent` wrappers), pulse has **no dedicated stdlib module**. The pulse telemetry counter lives in `std::runtime` alongside other machine-stone counters (`kain_machine_teleport_count`, `kain_machine_pulse_total_fire_count`). This is because pulse is primarily a **native runtime construct**, not a data structure or algorithm that warrants a helper module.

______________________________________________________________________

## The Pulse Clock Patterns

### Pattern 1: Frame Clock (Render Loop)

```kn
pulse render_clock every 16ms jitter 2ms:
    let next_frame = EngineAuthority.frame + 1
    let committed = commit_frame(EngineAuthority, next_frame)
    let _health = frame_health_check(committed, pulse_tick, pulse_missed)
```

**Why pulse not `while`:** The runtime owns the scheduler, handles jitter, tracks missed frames via `pulse_missed`, and the Z3 proof guarantees the missed-beat arithmetic is correct.

### Pattern 2: LFO / Modulation Generator

From the 24-tet effects engine:

```kn
pulse fx_modulation_tick every 8ms jitter 1ms:
    let dt: Int = pulse_dt_ms
    if dt < 1:
        dt = 8

    // Phase accumulation: rate * dt
    let advance1: Int = FxWorld.lfo1_rate * dt / 10
    FxWorld.lfo1_phase = (FxWorld.lfo1_phase + advance1) % (PHASE_MAX + 1)

    let advance2: Int = FxWorld.lfo2_rate * dt / 10
    FxWorld.lfo2_phase = (FxWorld.lfo2_phase + advance2) % (PHASE_MAX + 1)

    if pulse_tick % 32 == 0:
        FxWorld.fx_epoch = FxWorld.fx_epoch + 1
```

**Why pulse not a timer callback:** The body has full access to world state, can compute phase from actual elapsed time (`pulse_dt_ms` is wall-clock), and uses `pulse_tick` for epoch divisions.

### Pattern 3: Agent Simulation Beat

```kn
pulse agent_sinus every 16ms jitter 1ms:
    let particle = AgentParticle { x: 1.0, y: 2.0, vx: 0.5, vy: 0.25, alive: true }
    let gpu_particle = teleport particle from NativeWorld to GpuWorld via gpu_upload
    let pulse_budget = pulse_tick + pulse_dt_ms
    let _alive_after_handoff = gpu_particle.alive
    let _missed_beats = pulse_missed
```

**Why pulse:** The body creates a particle, teleports it to GPU world, and uses `pulse_tick` + `pulse_dt_ms` as a timing budget. The `pulse_missed` local detects overload.

### Pattern 4: Fusion Chain (13 Semantics in One Body)

From the pulse_teleport_decay_mesh benchmark:

```kn
pulse relay_clock every 4ms jitter 1ms:
    let shard = PulseShard { bias: 3, phase: 5, salt: 7, hot: true }
    let moved = teleport shard from PulseAuthority to PulseMirror via relay_clock_bus
    let _pulse_shape = pulse_tick + pulse_dt_ms + pulse_missed + moved.bias + moved.phase
```

This body combines: `pulse` (temporal) + `shatter struct` (data layout) + `teleport` (cross-world handoff) + world fields in a single tick — 13+ semantic constructs in one program. The pulse provides the beat; the other semantics provide the payload.

______________________________________________________________________

## Under-Utilized Ideas — Creative Semantic Abuse

### 1. Pulse as Hardware Watchdog / Health Beat

Pulse is the natural construct for health-check beats. Create a simple health authority world and let a pulse heartbeat increment its epoch:

```kn
world HealthAuthority:
    state heartbeat_epoch: Int = 0
    state last_alert: Int = 0
    surface web => HealthDashboard

pulse health_heartbeat every 1s:
    HealthAuthority.heartbeat_epoch = HealthAuthority.heartbeat_epoch + 1
    if pulse_missed > 0:
        // We're overloaded — log or escalate
        HealthAuthority.last_alert = pulse_tick
```

A separate actor or orchestrate stage can observe the heartbeat epoch and raising alerts when it stalls.

### 2. Pulse + Converge: Adaptive Frame Rate

Use `pulse_missed` to detect overload and switch to a degraded quality path:

```kn
pulse adaptive_render every 8ms jitter 1ms:
    let quality_path = pulse_missed > 0   // overloaded — use fast path
    let _frame = converge render_frame(RenderAuthority, quality_path)
```

Where `converge render_frame(quality: Bool)` has a `fast degraded_lane when quality` that skips expensive passes.

### 3. Pulse + Orchestrate: Timed Pipeline Frames

Pulse can fire an entire orchestrate pipeline on each beat:

```kn
pulse pipeline_beat every 100ms:
    let result = process_pipeline(
        TelemetryWorld.raw_signal,
        TelemetryWorld.tick
    )
    TelemetryWorld.last_result = result
```

Where `process_pipeline` is an `orchestrate` with CPU → GPU → law → patch stages. The pulse drives the pipeline; the pipeline drives the computation.

### 4. Pulse as Physics Integration Step

For simulations, pulse provides wall-clock dt:

```kn
pulse physics_tick every 4ms jitter 1ms:
    let dt_seconds: Int = pulse_dt_ms
    // dt_seconds ≈ 4 (but accurate to wall clock)
    let gravity_step: Int = GRAVITY * dt_seconds / 1000
    SimWorld.velocity_y = SimWorld.velocity_y + gravity_step
    SimWorld.position_y = SimWorld.position_y + SimWorld.velocity_y * dt_seconds / 1000
```

Integrate against actual elapsed time, not assumed time. If the scheduler is delayed, `pulse_dt_ms` reflects it and the physics stay stable.

### 5. Pulse as Beat-Division Conductor

`pulse_tick` is monotonic and never resets. Use it for polyrhythm:

```kn
pulse conductor every 4ms jitter 1ms:
    // 4ms pulse = 250 Hz base rate

    // Quarter note (every 64 ticks = ~4 Hz)
    if pulse_tick % 64 == 0:
        MusicWorld.quarter_beat = MusicWorld.quarter_beat + 1

    // Eighth note (every 32 ticks)
    if pulse_tick % 32 == 0:
        MusicWorld.eighth_beat = MusicWorld.eighth_beat + 1

    // Sixteenth note (every 16 ticks)
    if pulse_tick % 16 == 0:
        MusicWorld.sixteenth_beat = MusicWorld.sixteenth_beat + 1
```

### 6. Pulse as CPU-Bound Work Scheduler

Pulse can perform work in timed chunks, yielding between chunks:

```kn
pulse work_chunker every 1ms:
    if WorkQueue.pending == 0:
        return   // nothing to do — skip this beat entirely
    // Process one work item per beat
    let item = WorkQueue.next()
    process_work_item(item)
```

______________________________________________________________________

## Combining with Other Keywords

### Pulse + World

Direct world state mutation within pulse bodies:

```kn
world FxWorld:
    state lfo1_phase: Int = 0
    state tick_count: Int = 0
    surface native_ui => FxPanel

pulse fx_tick every 8ms:
    FxWorld.lfo1_phase = (FxWorld.lfo1_phase + 1) % MAX_PHASE
    FxWorld.tick_count = pulse_tick
```

### Pulse + Entangle

Pulse writes propagate through entangle automatically:

```kn
world Authority:
    state signal: Int = 1
world Mirror:
    state signal_copy: Int = 1
entangle Authority.signal <-> Mirror.signal_copy with single_writer

pulse signal_beat every 16ms:
    Authority.signal = Authority.signal + 1   // propagates to Mirror.signal_copy
```

### Pulse + Patch

Use `patch` for journaled mutations within pulses:

```kn
pulse frame_beat every 16ms:
    let next = Authority.frame + 1
    let committed = commit_frame(Authority, next)   // patch
    let _score = frame_score(committed)
```

### Pulse + Resonate

Pulse writes can trigger resonate handlers:

```kn
pulse param_beat every 8ms:
    FxWorld.distortion_drive = compute_drive(pulse_tick)

resonate FxWorld.distortion_drive dampen 16ms:
    let new_drive = resonate_new_i64
    FxWorld.distortion_output = clamp_output(new_drive)
```

### Pulse + Teleport

The canonical machine-stones pattern: pulse + teleport in the same body:

```kn
pulse agent_sinus every 16ms jitter 1ms:
    let particle = AgentParticle { x: 1.0, y: 2.0, ... }
    let gpu_particle = teleport particle from NativeWorld to GpuWorld via gpu_upload
```

### Pulse + Converge

Use converge for adaptive quality based on missed beats:

```kn
pulse adaptive_render every 8ms jitter 2ms:
    let mode = pulse_missed > 0    // overload → fast lane
    let _frame = converge render(Auth, mode)     // spec = quality, fast = degraded
```

### Pulse + Orchestrate

Pulse fires orchestrate pipelines:

```kn
pulse pipeline_beat every 100ms:
    let result = process_telemetry(PulseMirror.raw_signal)
    PulseAuthority.processed = result
```

### Pulse + Actor

Pulse can send messages to actors:

```kn
pulse actor_beat every 100ms:
    let reply = ask(Relay, "Process", pulse_tick)
    Authority.last_reply = reply
```

### Pulse + Shatter

Pulse + shatter struct + teleport = the machine-stones trifecta:

```kn
shatter struct AgentParticle:
    x: Float
    y: Float
    vx: Float
    vy: Float
    alive: Bool

pulse agent_tick every 16ms:
    let particle = AgentParticle { x: 1.0, y: 2.0, vx: 0.5, vy: 0.25, alive: true }
    let moved = teleport particle from NativeWorld to GpuWorld via upload
```

### Pulse + Ownership (collapse/observe/decay)

Pulse bodies have the `Alloc` effect, allowing full ownership lifecycle:

```kn
pulse memory_beat every 1s:
    let mut cells: ptr<Int> = alloc_zeroed(16, "Int")
    collapse cells:
        mem_store(cells, pulse_tick, "Int")
        0
    let val: Int = observe cells:
        mem_load(cells, "Int")
    decay cells
    MemoryWorld.last_val = val
```

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck (pulse passes typecheck but does NOT execute in interpreter)
kain check <file.kn> --target llvm

# 2. Compile + run (pulse executes in native LLVM)
kain run <file.kn> --target llvm

# 3. Verify pulse fired (check telemetry)
kain run <file.kn> --target llvm
# Add a test that checks runtime_machine_pulse_total_fire_count() > 0

# 4. For benchmark proof
python benchmark/run.py --case pulse_teleport_decay_mesh --languages kain --runs 1 --warmups 0

# 5. For machine stones blade
kain run blades/test/machine-stones --target llvm

# 6. Check runtime contract
# The output runtime_contract.json will show pulses with interval_ms and capabilities
```

### Smoketest

```kn
// smoketest/src/semantics/pulse.kn
use std::runtime
use std::intent
use shatter::SmokeShard

world SmokePulseAuthority:
    state signal: Int = 1
    surface web => SmokePulsePanel

world SmokePulseMirror:
    state signal_copy: Int = 1
    surface web => SmokePulsePanel

pulse smoke_clock every 8ms jitter 1ms:
    let shard = SmokeShard { bias: 1, phase: 2, salt: 3, alive: true }
    let moved = teleport shard from SmokePulseAuthority to SmokePulseMirror via smoke_pulse_bus
    let _tick_shape = pulse_tick + pulse_dt_ms + pulse_missed + moved.bias

pub fn smoke_pulse_lane() -> Int:
    return 0
```

______________________________________________________________________

## Z3 Proofs

### 1. Pulse Missed Beat Stays Bounded

**File:** `runtime/native/src/core/z3/proofs/native-machine-pulse-missed-beat-stays-bounded.yaml`

**Proves:** The `pulse_missed` counter never exceeds the number of elapsed beats and never goes negative. This is a safety proof for the missed-beat arithmetic in `kain_machine_pulse_snapshot()`.

```smt2
(set-logic QF_NIA)
(declare-const elapsed Int)
(declare-const interval Int)
(declare-const jitter Int)
(assert (>= elapsed 0))
(assert (> interval 0))
(assert (>= jitter 0))
(define-fun advanced () Int (div elapsed interval))
(define-fun tolerated () Int (+ interval jitter))
(define-fun missed () Int (ite (and (> advanced 1) (> elapsed tolerated)) (- advanced 1) 0))
(assert (or (< missed 0) (> missed advanced)))
(check-sat)
```

**Invariants verified:**

- `missed >= 0` (never underflows)
- `missed <= advanced` (never exceeds total beats)
- When `advanced == 1` (single beat), `missed == 0` (no false positives)
- The jitter window only affects the tolerance check, not the ceiling

### 2. Z3 Audit Findings

The Z3 data catalog (`runtime/native/src/core/z3/data/`) flags two findings for the pulse code:

| Finding | Risk | Details |
|---------|------|---------|
| `kain_machine_pulse_snapshot` | 30 | Multiple arithmetic sites: elapsed/interval division, advanced subtraction. The proof above addresses exactly this risk. |
| `g_machine_pulse_total_fire_count` increment | 55 | "count-like state mutates without lock or atomic primitive" — **Note:** This is incorrect; the actual code uses `atomic_fetch_add_explicit` with `memory_order_relaxed`, which IS an atomic primitive. The audit flag is a false positive for the relaxed ordering choice. |

______________________________________________________________________

## Source Anchors

### Parser & AST

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 148, 309-326 | `Item::Pulse(PulseDef)`, `PulseDef`, `PulseDuration` |
| `crates/core/src/parser.rs` | 1026 | `parse_pulse()` dispatch |
| `crates/core/src/parser.rs` | 1854-1913 | `parse_pulse()` full function |
| `crates/core/src/parser.rs` | 1880-1908 | `parse_pulse_duration()` |

### Typechecker

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/types.rs` | 4665 | `check_pulse()` dispatch |
| `crates/core/src/types.rs` | 5804-5830 | `check_pulse()` full function |
| `crates/core/src/types.rs` | 5888-5910 | `validate_pulse_duration()`, `pulse_duration_unit_is_valid()` |

### Runtime Contract

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime_contract.rs` | 161-168 | `RuntimePulseContract` struct |
| `crates/core/src/runtime_contract.rs` | 358, 404 | Collection and emission |
| `crates/core/src/runtime_contract.rs` | 592-602 | `time.pulse` + `time.hardware-timer` capabilities |
| `crates/core/src/runtime_contract.rs` | 1288-1311 | `collect_pulse_contracts()`, `runtime_pulse_contract()` |
| `crates/core/src/runtime_contract.rs` | 1383-1392 | `pulse_duration_to_millis()` |

### Runtime (Interpreter)

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime.rs` | 4186, 4308 | `Item::Pulse(_)` → no-op (native-only) |
| `crates/core/src/runtime.rs` | 4836, 4857 | Pulse name resolution for aliasing |

### LLVM Codegen

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 1633-1638 | Symbol name helpers |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 1648-1658 | `machine_pulse_duration_ns()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13187 | `compile_pulse()` dispatch |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15161-15281 | `compile_pulse()` full function (body + fire wrapper) |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13258-13265 | `NativePulseInfo` collection for entry preamble |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 14296-14297 | `kain_machine_pulse_snapshot` / `kain_machine_pulse_start` declarations |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15699-15706 | Entry preamble `kain_machine_pulse_start()` call |

### Native C Runtime

| File | Lines | Content |
|------|-------|---------|
| `runtime/native/include/machine_stones.h` | 19-42 | Full public API: snapshot, start, stop, fire count |
| `runtime/native/src/core/machine_stones.c` | 1-43 | Includes, constants, slot definitions |
| `runtime/native/src/core/machine_stones.c` | 44-58 | `KainMachinePulseSlot`, `KainMachinePulseFireJob` structs |
| `runtime/native/src/core/machine_stones.c` | 68-86 | Global state: slots, lock, scheduler running, total fire count |
| `runtime/native/src/core/machine_stones.c` | 287-491 | Full pulse implementation: snapshot, start, stop, fire count |

### Z3 Proofs

| File | Content |
|------|---------|
| `runtime/native/src/core/z3/proofs/native-machine-pulse-missed-beat-stays-bounded.yaml` | SMT2 proof: missed >= 0, missed \<= advanced |
| `runtime/native/src/core/z3/proofs/native-machine-capability-token-signatures-are-collision-free.yaml` | Token collision proof for the stable hash |

### Stdlib

| File | Content |
|------|---------|
| `stdlib/runtime.kn` | `kain_machine_pulse_total_fire_count()` — extern telemetry function |

### Examples

| File | Usage |
|------|-------|
| `smoketest/src/semantics/pulse.kn` | Minimal pulse + shatter + teleport smoke test |
| `library_of_kain/pulse.kn` | Same pattern as smoketest |
| `blades/test/machine-stones/src/main.kn` | Axiom + pulse + shatter + teleport dogfood blade with telemetry checks |
| `benchmark/cases/pulse_teleport_decay_mesh/main.kn` | **Apex:** pulse + world + entangle + patch + law + converge + orchestrate + actor + shatter + teleport + ownership — 54,000 iterations, 13+ semantics |
| `blades/python/24_tet/src/resonate_py_effects.kn` | Pulse for LFO modulation timing in real-time audio effects engine |
| `blades/sims/fluid-studio/src/*.kn` | Pulse in fluid simulation UI/main loop |
| `benchmark/cases/semantic_singularity_crucible/main.kn` | Pulse in LLVM semantic torture lane |
| `benchmark/cases/semantic_singularity/main.kn` | Pulse in standard semantic stress test |
| `benchmark/cases/semantic_singularity_no_actor/main.kn` | Pulse without actors (control test) |
| `benchmark/cases/semantic_singularity_no_entangle/main.kn` | Pulse without entangle (control test) |
| `benchmark/cases/semantic_singularity_no_patch/main.kn` | Pulse without patch (control test) |

______________________________________________________________________

## Anti-Patterns

### ❌ Hand-Rolled `while` + `sleep` Instead of Pulse

```kn
// WRONG: manual timing loop
fn main_loop():
    var frame: Int = 0
    while true:
        frame = frame + 1
        render_frame(frame)
        sleep_millis(16)
```

```kn
// RIGHT: compiler-owned temporal beat
pulse render_clock every 16ms jitter 2ms:
    let next_frame = EngineAuthority.frame + 1
    let committed = commit_frame(EngineAuthority, next_frame)
    let _health = frame_score(committed) + pulse_tick + pulse_dt_ms + pulse_missed
```

### ❌ Assuming Fixed dt Instead of Using pulse_dt_ms

```kn
// WRONG: hardcoded dt
pulse physics_tick every 4ms:
    let dt = 4    // assumes perfect timing — wrong when overloaded
    velocity = velocity + g * dt
```

```kn
// RIGHT: use actual elapsed time
pulse physics_tick every 4ms jitter 1ms:
    let dt = pulse_dt_ms
    if dt < 1: dt = 4    // safeguard
    velocity = velocity + g * dt / 1000
    position = position + velocity * dt / 1000
```

### ❌ Using Pulse for One-Shot Timers

```kn
// WRONG: pulse as a one-shot timer
pulse delayed_action every 5s:
    do_once()         // fires every 5s forever
```

```kn
// RIGHT: use async timer or ask_timeout for one-shots
async fn delayed():
    sleep_millis(5000)
    do_once()
```

### ❌ Unbounded Pulse Bodies

```kn
// WRONG: pulse body that takes longer than the interval
pulse heavy_work every 16ms:
    process_all_data()   // if this takes 50ms, you miss 2 beats
```

```kn
// RIGHT: monitor pulse_missed, adapt quality
pulse adaptive_work every 16ms jitter 2ms:
    let quality = pulse_missed > 1    // we're behind
    let _result = converge process_work(data, quality)
```

### ❌ Not Checking Telemetry

Without `runtime_machine_pulse_total_fire_count()` > 0 checks, you have no proof the pulse actually fired. The machine-stones test blade uses this as a gate:

```kn
if runtime_machine_pulse_total_fire_count() < 1:
    return 7    // pulse never fired — test failure
```

### ❌ Multiple Fast Pulses (< 1ms)

The scheduler has a 1ms idle sleep floor. Pulses faster than 1ms will burn scheduler CPU. For sub-ms timing, use `pulse_dt_ms` accumulation or a physics integrator that batches sub-step math per beat.

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `pulse <name> every <value><unit> [jitter <value><unit>]: <body>` |
| **Units** | `ns`, `us`, `ms`, `s`, `tick`, `ticks` |
| **Body locals** | `pulse_tick: Int` (monotonic counter), `pulse_dt_ms: Int` (actual elapsed ms), `pulse_missed: Int` (overloaded beat count) |
| **Effect permissions** | IO, Async, GPU, Reactive, Unsafe, Alloc, Panic — **all effects allowed** |
| **Interpreter** | No-op — pulse is native-only, only executes under LLVM |
| **LLVM output** | Two functions per pulse: `__kain_pulse_body_{name}` + `__kain_pulse_fire_{name}` |
| **Native scheduler** | Dedicated OS thread, 64-slot open-addressed table, spinlock-protected, 1ms idle floor |
| **First fire** | Immediate — pulses fire once on registration, then on schedule |
| **Lifecycle** | Scheduler starts on first pulse, stops at `atexit()` |
| **Telemetry** | `runtime_machine_pulse_total_fire_count()` — atomic, monotonic, never resets |
| **Capabilities** | `time.pulse` + `time.hardware-timer` — auto-emitted when any pulse exists |
| **Runtime contract** | `RuntimePulseContract { name, interval, interval_ms, jitter, body_ownership_ops, body_teleports }` |
| **Z3 proofs** | Missed-beat bound: `missed >= 0`, `missed <= advanced`, never negative, bounded by elapsed |
| **Token** | Stable 64-bit hash of pulse name |
| **Slot count** | 64 (power of two, open-addressed) |
| **Canonical example** | `benchmark/cases/pulse_teleport_decay_mesh/main.kn` — 13+ semantics in one program |
| **Canonical pattern** | Frame clock, LFO modulation, agent simulation beat, health heartbeat |
| **Stdlib** | `std::runtime` — `kain_machine_pulse_total_fire_count()` |
| **Source** | `crates/core/src/ast.rs` (PulseDef), `crates/sys-codegen/src/codegen_llvm/mod.rs` (compile_pulse), `runtime/native/src/core/machine_stones.c` (scheduler) |
