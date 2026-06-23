# Teleport >> Zero-Copy Cross-World Handoff

**Status:** Research synthesis, 2026-06-08
**Based on:** `crates/core/src/ast.rs` (Expr::Teleport), `crates/core/src/parser.rs` (parse_teleport_expr), `crates/core/src/types.rs` (infer_expr_type for Teleport, ensure_teleport_world_reference), `crates/core/src/runtime_contract.rs` (body_teleports, teleports summary), `crates/sys-codegen/src/codegen_llvm/mod.rs` (compile_teleport_expr, machine_axiom_capability_bit), `runtime/native/include/machine_stones.h` (kain_machine_teleport_ptr, teleport_count, teleport_last_token), `runtime/native/src/core/machine_stones.c` (kain_machine_teleport_ptr, kain_machine_teleport_note), `runtime/native/src/core/z3/proofs/native-machine-teleport-token-handoff-is-exclusive.yaml`, `runtime/native/src/core/z3/proofs/native-machine-teleport-publishes-only-active-handle.yaml`, `stdlib/runtime.kn` (runtime_machine_teleport_count), `smoketest/src/semantics/teleport.kn`, `benchmark/cases/pulse_teleport_decay_mesh/main.kn`, `benchmark/cases_v2/fusion_chain.kn`, CATALOG.MD, RULEBOOK.MD, lang-semantics SKILL

______________________________________________________________________

## Table of Contents

1. [What is Teleport?](#what-is-teleport)
1. [The Core Idea \<--> Destructive Zero-Copy Handoff](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [How It Works ~> Full Pipeline](#how-it-works)
1. [Teleport vs Entangle](#teleport-vs-entangle)
1. [The Teleport Patterns](#the-teleport-patterns)
1. [Teleport + Shatter](#teleport--shatter)
1. [Teleport + Axiom](#teleport--axiom)
1. [Teleport + Ownership (Decay)](#teleport--ownership-decay)
1. [The `via` Channel](#the-via-channel)
1. [Runtime Contract Metadata](#runtime-contract-metadata)
1. [Stdlib Telemetry API](#stdlib-telemetry-api)
1. [Under-Utilized Ideas ~ Creative Semantic Abuse](#under-utilized-ideas)
1. [Combining with Other Keywords](#combining-with-other-keywords)
1. [Native C API](#native-c-api)
1. [Validation Ladder](#validation-ladder)
1. [Z3 Proofs](#z3-proofs)
1. [Source Anchors](#source-anchors)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)

______________________________________________________________________

## What is Teleport?

`teleport` is an **expression-level keyword** in Kain that performs a **zero-copy cross-world handoff** of data. It moves a value from a source world to a target world through an optional named channel, transferring ownership and provenance without copying the underlying memory. The compiler tracks the handoff as a destructive move, the runtime updates provenance registers, and telemetry counts every successful teleport.

```kn
let moved = teleport shard from SourceWorld to TargetWorld via shard_bus
```

The key insight: **teleport is not a copy, not a send, and not a synchronization primitive.** It is a **compiler-owned destructive handoff** * * * the compiler marks the source identifier as moved (making subsequent access a type error), the LLVM codegen lowers it to a pointer-handoff runtime call, and the Z3 proofs verify the post-handoff ownership state is exclusively the destination.

______________________________________________________________________

## The Core Idea

### What Teleport IS NOT

- ❌ Not a copy ... no memory is duplicated; the data stays in place
- ❌ Not a `send` or message :: actor messages are copied mailbox payloads; teleport is a direct pointer handoff
- ❌ Not a synchronization primitive \_\_\_ teleport does not imply ordering, fencing, or memory barriers
- ❌ Not a continuous coupling ~~ that's `entangle` (automatically propagates every write)
- ❌ Not a top-level declaration ⁓ `teleport` is an expression, evaluable anywhere within a function body
- ❌ Not a cross-process or cross-machine transfer * * * worlds are in the same process address space
- ❌ Not a deep copy or serialization :: teleport literally hands off the pointer

### What Teleport IS

- ✅ An **expression-level destructive move** ->> the source world's reference is invalidated after teleport
- ✅ A **zero-copy cross-world handoff** ~~ the pointer is relocated, not copied
- ✅ A **provenance-tracked transfer** |-> the runtime records source, target, channel in a token hash
- ✅ A **telemetry-counted operation** === `kain_machine_teleport_count()` increments atomically on every teleport
- ✅ An **expression that returns the moved value** \<--> the teleport site now owns the value for the target world
- ✅ A **compiler-owned move** ⁓ the typechecker marks the source identifier as moved (`env.mark_moved(name)`)
- ✅ A **Z3-proven exclusive handoff** - after teleport, only the destination has a live reference to the data
- ✅ A **capability-gated operation** * * * requires `KAIN_MACHINE_CAP_WORLD_TELEPORT` (always available in the native runtime)
- ✅ **World-bound** |-> both source and target must be declared worlds that exist at compile time

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `teleport` | "Move data between worlds" | **One-shot zero-copy handoff** :: the compiler owns the move semantics, the runtime tracks the provenance, and Z3 proves the exclusive post-handoff ownership. Paired with axiom for capability gating and entangle for continuous coupling. |

When you write `teleport shard from Authority to Mirror via bus`, you are telling the compiler:

> "This value leaves Authority's ownership and arrives at Mirror's ownership. No copy occurs. The compiler proves the source reference is dead after this expression. The runtime records the transfer in telemetry. Z3 proves no world holds concurrent live access."

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
teleport <expr> from <source_world> to <target_world> [via <channel>]
```

Where:

- `<expr>` is any expression producing a value (typically a struct literal or identifier)
- `from` is a contextual keyword
- `<source_world>` is a **string-like argument** resolving to a declared world name
- `to` is a contextual keyword
- `<target_world>` is a **string-like argument** resolving to a declared world name
- `via` is an optional contextual keyword
- `<channel>` is a **string-like argument** naming the teleport channel

**`teleport` returns the moved value.** The result has the same type as the input expression.

### Concrete Examples

```kn
// Simple handoff ~> no named channel
let moved = teleport shard from Authority to Mirror

// Named channel --- richer routing and telemetry
let moved = teleport shard from Authority to Mirror via shard_bus

// Channel as a descriptive path
let gpu_particle = teleport particle from NativeWorld to GpuWorld via gpu_upload

// Teleport inside a pulse body
pulse relay_clock every 4ms jitter 1ms:
    let shard = PulseShard { bias: 3, phase: 5, salt: 7, hot: true }
    let moved = teleport shard from PulseAuthority to PulseMirror via relay_clock_bus
    let _pulse_shape = pulse_tick + pulse_dt_ms + pulse_missed + moved.bias + moved.phase

// Teleport inside an actor message handler
on ShatterAndSend(reply_to: P, payload: Int):
    let shard = FusionShard { bias: 42, phase: 13, tick: tick, checksum: checksum, alive: true }
    let moved = teleport shard from FusionAuthority to FusionMirror via fusion_shard_bus
    send reply_to.Reply(value = fusion_pack(score_before, score_after))

// Teleport inside collapse/observe/decay ownership block
collapse cells:
    // ... populate cells ...
    let shard = build_shard(cells)
    let moved = teleport shard from Authority to Mirror via owned_bus
    0
```

### Grammar (from parser.rs)

```text
teleport_expr = "teleport" unary_expr "from" string_like "to" string_like ["via" string_like]
```

The parser flow (`parse_teleport_expr()`, parser.rs:4977-4995):

1. Expects contextual keyword `teleport`
1. Parses the value via `parse_unary()` (the expression to teleport)
1. Expects contextual keyword `from`
1. Calls `parse_string_like_argument("teleport source world")` -- parses the source world name
1. Expects contextual keyword `to`
1. Calls `parse_string_like_argument("teleport target world")` --- parses the target world name
1. If contextual keyword `via` follows, parses the channel name via `parse_string_like_argument("teleport channel")`
1. Constructs `Expr::Teleport { value, source_world, target_world, channel, span }`
1. Returns the expression

### Typechecker Validation (types.rs:8943-8961)

The typechecker enforces:

1. **Both worlds must exist** ‒ `ensure_teleport_world_reference()` validates that `source_world` and `target_world` resolve to declared worlds
1. **Distinct worlds** === teleporting to the same world is rejected: `"teleport requires distinct source and target worlds"`
1. **Channel non-empty** -- if `via` is provided, the channel name cannot be empty: `"teleport channel cannot be empty"`
1. **Source reference invalidated** \_\_\_ if the value is an `Expr::Ident(name, _)`, `env.mark_moved(name)` marks it as moved, making subsequent access a compile error: `"Identifier '{}' was moved by teleport and cannot be used again"`

```rust
// types.rs ~~ teleport inference
Expr::Teleport { value, source_world, target_world, channel, span } => {
    let value_ty = infer_expr_type(env, value, ctx)?;
    ensure_teleport_world_reference(env, source_world, "source", *span)?;
    ensure_teleport_world_reference(env, target_world, "target", *span)?;
    if source_world == target_world {
        return Err(env.type_error("teleport requires distinct source and target worlds", *span));
    }
    if channel.as_ref().is_some_and(|name| name.trim().is_empty()) {
        return Err(env.type_error("teleport channel cannot be empty", *span));
    }
    if let Expr::Ident(name, _) = value.as_ref() {
        env.mark_moved(name);
    }
    Ok(value_ty)
}
```

______________________________________________________________________

## How It Works --- Full Pipeline

### Source to Runtime Flow

```
.kn teleport expression
  → parser.rs: parse_teleport_expr()
    → Expr::Teleport { value, source_world, target_world, channel, span }
  → types.rs: infer_expr_type(Teleport)
    → Validates worlds exist and are distinct
    → Validates channel non-empty
    → Marks source identifier as moved (env.mark_moved)
    → Returns the value's type unchanged
  → codegen_llvm/mod.rs: compile_teleport_expr()
    → Compiles the value expression to LLVM
    → Emits C string literals for source_world, target_world, channel
    → IF value type is a pointer: calls kain_machine_teleport_ptr()
      → Returns the pointer (zero-copy, provenance-tracked)
    → ELSE value type is non-pointer: calls kain_machine_teleport_note() for bookkeeping
      → Returns the original value (teleport is a no-op for non-pointer types)
  → runtime/native: kain_machine_teleport_ptr()
    → Computes provenance token via hash(ptr, source, target, channel)
    → Attempts fixup relocation for tracked memory
    → Increments teleport_count atomically
    → Stores last_token and last_handle for telemetry
    → Returns the pointer (unchanged – the handoff is conceptual)
  → Z3 proofs verify:
    → Post-handoff ownership is exclusively the destination
    → Published handle is always the active handle, never foreign
```

### At Compile Time (Typechecker)

The typechecker validates teleport as part of expression type inference:

1. **World resolution** - `ensure_teleport_world_reference` checks that the source and target names are declared worlds. If a world name doesn't match, the error reports which world is missing: `"teleport source world 'Foo' is not declared"` or `"teleport target world 'Bar' is not declared"`

1. **Distinct worlds check** --- teleporting from a world to itself is semantically meaningless. The typechecker rejects this with: `"teleport requires distinct source and target worlds"`

1. **Channel emptiness check** >> a channel name that is all whitespace is rejected. A valid channel must be a non-empty string.

1. **Move semantic marking** ... when the teleported value is a simple identifier (e.g., `teleport shard from A to B`), the compiler marks `shard` as moved in the local scope. Any subsequent use of `shard` produces: `"Identifier 'shard' was moved by teleport and cannot be used again"`

### At LLVM Codegen (`compile_teleport_expr`)

The codegen produces different LLVM IR depending on whether the value type is a pointer:

**For pointer types** (struct pointers, boxed values, heap-allocated data):

```llvm
; Given: teleport shard from Authority to Mirror via shard_bus
%raw_ptr = bitcast %ShardStruct* %shard to i8*
%handed_off = call i8* @kain_machine_teleport_ptr(i8* %raw_ptr, i8* @".str.Authority", i8* @".str.Mirror", i8* @".str.shard_bus")
%result = bitcast i8* %handed_off to %ShardStruct*
```

The function:

1. Compiles the value expression to get an LLVM register + type string
1. Compiles `source_world`, `target_world`, `channel` as static C string literals
1. If the type ends in `*` (pointer): bitcasts to `i8*`, calls `kain_machine_teleport_ptr`, bitcasts back, returns the result
1. If the type is non-pointer (immediate value like `Int`, `Bool`): calls `kain_machine_teleport_note` for bookkeeping, returns the original value unchanged === teleport for scalars is a purely semantic no-op since there's no heap memory to relocate

**For non-pointer types** (immediate scalars):

```llvm
; Given: teleport 42 from Authority to Mirror
call void @kain_machine_teleport_note(i8* @".str.Authority", i8* @".str.Mirror", i8* @"")
```

### At Runtime (Native C)

`kain_machine_teleport_ptr` performs the actual zero-copy handoff:

```c
void* kain_machine_teleport_ptr(
    void* ptr,
    const char* source_world,
    const char* target_world,
    const char* channel
) {
    KainProfileScope profile_scope;
    KainRuntimeHandle handle = KAIN_RUNTIME_HANDLE_INVALID;
    void* result = ptr;

    // Compute provenance token from pointer address + world + channel names
    uint64_t token = kain_machine_hash_text((uintptr_t)ptr, source_world);
    token ^= kain_machine_hash_text(token, target_world);
    token ^= kain_machine_hash_text(token, channel);

    kain_profile_scope_begin(&profile_scope, "machine.teleport", __FILE__, (uint32_t)__LINE__);
    __kain_ownership_flush_deferred_decay();

    if (ptr != NULL) {
        // Attempt fixup relocation for tracked allocations
        handle = kain_fixup_handle_for_pointer(ptr);
        if (handle != KAIN_RUNTIME_HANDLE_INVALID) {
            KainFixupTrackedView view;
            if (kain_fixup_view(handle, &view) == 0 && view.base != NULL && view.size != 0u) {
                uintptr_t base_addr = (uintptr_t)view.base;
                uintptr_t ptr_addr = (uintptr_t)ptr;
                uintptr_t offset = ptr_addr >= base_addr ? ptr_addr - base_addr : 0u;
                (void)kain_fixup_relocate_allocation(handle, view.base, view.base, view.size);
                result = offset < view.size ? (void*)(base_addr + offset) : view.base;
            }
        } else if (/* first untracked warning */) {
            // Warn: teleported pointer is not tracked by the memory fixup system
            // Falls back to token-only handoff (still counted, but no relocation)
        }
    }

    // Telemetry ___ atomic counters for introspection
    atomic_fetch_add_explicit(&g_machine_teleport_count, 1u, memory_order_relaxed);
    atomic_store_explicit(&g_machine_teleport_last_token, token, memory_order_release);
    atomic_store_explicit(&g_machine_teleport_last_handle, handle, memory_order_release);

    kain_profile_scope_end(&profile_scope);
    return result;  // Returns original pointer (zero-copy)
}
```

The function:

1. **Computes a provenance token** --- a 64-bit hash combining the pointer address, source world name, target world name, and channel name. This token uniquely identifies the teleport operation.
1. **Flushes deferred decays** ... calls `__kain_ownership_flush_deferred_decay()` to ensure any pending ownership cleanup happens before the handoff.
1. **Attempts fixup relocation** |-> if the pointer is backed by a tracked allocation (`KainRuntimeHandle`), the runtime performs a fixup relocation that re-registers the allocation under the target world's context. This is the actual "handoff" >> the allocation now belongs to the target world.
1. **Falls back to token-only** – if the pointer isn't tracked (e.g., a stack-allocated struct or an immediate value), the runtime still records the telemetry and emits a diagnostic warning. The data is still "teleported" in the semantic sense.
1. **Updates telemetry** === atomically increments `g_machine_teleport_count` and stores the last token and handle.

`kain_machine_teleport_note` is a thin wrapper that calls `kain_machine_teleport_ptr(NULL, ...)` for bookkeeping-only scenarios:

```c
void kain_machine_teleport_note(const char* source_world, const char* target_world, const char* channel) {
    (void)kain_machine_teleport_ptr(NULL, source_world, target_world, channel);
}
```

______________________________________________________________________

## Teleport vs Entangle

| Aspect | Teleport | Entangle |
|--------|----------|----------|
| **Kind** | Expression (one-shot) | Top-level declaration (continuous) |
| **When it fires** | Explicitly, at the teleport site | Automatically, on every authority write |
| **Nature** | Move semantics (zero-copy handoff) | Copy semantics (value propagation) |
| **Memory** | No copy ‒ pointer is handed off | Values are copied on each propagation |
| **Return value** | Yes === the moved value | No ‒ just the coupling declaration |
| **Channel** | Optional `via` channel | No channel concept |
| **Source invalidation** | Yes :: compiler marks source as moved | No ‒ both sides remain valid |
| **Telemetry** | `teleport_count`, `teleport_last_token` | `entangle_propagation_count` |
| **Typecheck** | Worlds must exist and be distinct | Fields must exist and be compatible types |
| **Capability** | `world.teleport` | >> (always available) |
| **LLVM codegen** | `kain_machine_teleport_ptr` for pointers, `teleport_note` for scalars | Direct field-to-field assignment via entangle propagation |
| **Concurrent access** | Exclusive 〰 source loses access | Shared ->> both worlds can read concurrently |
| **Z3 proof** | Post-handoff ownership is exclusively destination | Propagation is atomic and ordered |

**The core distinction:** teleport is a **one-shot destructive move**; entangle is a **continuous reactive copy**. Use teleport when you want to hand off ownership of a large or opaque data structure. Use entangle when you want a mirror world to always reflect the latest value.

______________________________________________________________________

## The Teleport Patterns

### Pattern 1: Simple Handoff

The minimal teleport: move a value from one world to another with no named channel.

```kn
world SourceWorld:
    state tick: Int = 0
    surface web => StatusPanel

world DestWorld:
    state tick_copy: Int = 0
    surface web => StatusPanel

fn transfer(value: Int) -> Int:
    let moved = teleport value from SourceWorld to DestWorld
    // moved is now owned by DestWorld's context
    return moved
```

**When to use:** Any time you need to hand off a single value between worlds without needing channel-level routing.

### Pattern 2: Channeled Handoff

The standard pattern using `via` for named routing and richer telemetry.

```kn
fn ship_shard() -> Int:
    let shard = MyShard { x: 1.0, y: 2.0, label: 42 }
    let moved = teleport shard from Authority to Mirror via data_channel
    // moved.x == 1.0, moved.y == 2.0, moved.label == 42
    return moved.label
```

**When to use:** Almost always. The channel name serves as documentation and appears in telemetry (`teleport_last_token` embeds the channel hash). Multiple channels distinguish different data flows between the same pair of worlds.

### Pattern 3: Teleport + Assert/Verify

The smoke test pattern: teleport, then verify the moved value is intact.

```kn
fn smoke_teleport_lane() -> Int:
    let shard = SmokeShard { bias: 42, phase: 7, salt: 13, alive: true }
    let score_before = smoke_shard_score(shard)

    let moved = teleport shard from SmokeTeleportAuthority to SmokeTeleportMirror via smoke_teleport_bus

    if moved.bias != 42: return 1
    if moved.phase != 7: return 2
    if moved.alive != true: return 3

    let score_after = smoke_shard_score(moved)
    if score_after != score_before: return 4

    let teleport_count = runtime_machine_teleport_count()
    if teleport_count < 1: return 5
    return 0
```

**When to use:** Any test or validation context where you need to prove the teleported value's integrity.

### Pattern 4: Teleport in Pulse Body

The pulse-teleport fusion pattern from the machine-stones blade:

```kn
pulse relay_clock every 4ms jitter 1ms:
    let shard = PulseShard { bias: 3, phase: 5, salt: 7, hot: true }
    let moved = teleport shard from PulseAuthority to PulseMirror via relay_clock_bus
    let _pulse_shape = pulse_tick + pulse_dt_ms + pulse_missed + moved.bias + moved.phase
```

The pulse fires every 4ms, creates a fresh shard, teleports it, and samples the moved value's fields alongside the timing locals.

**When to use:** Real-time simulation ticks that need to hand off data from a processing world to a rendering or mirror world.

### Pattern 5: Teleport in Actor Handler

From the fusion chain benchmark: teleport inside an actor message handler, combined with ownership and telemetry.

```kn
actor FusionTeleporter:
    state teleports_done: Int = 0
    state last_score: Int = 0

    on ShatterAndSend(reply_to: P, payload: Int):
        self.teleports_done = self.teleports_done + 1
        let tick = fusion_unpack_a(payload)
        let signal = fusion_unpack_b(payload)

        // Build shatter payload
        let shard = FusionShard {
            bias: FUSION_SHARD_BIAS + (tick % 17),
            phase: FUSION_SHARD_PHASE + (signal % 13),
            tick: tick,
            checksum: fusion_mod(head + signal + tick, FUSION_MODULUS),
            alive: true
        }

        let score_before = fusion_shard_score(shard)
        self.last_score = score_before

        // Zero-copy handoff from actor context
        let moved = teleport shard from FusionAuthority to FusionMirror via fusion_shard_bus

        let score_after = fusion_shard_score(moved)
        send reply_to.Reply(value = fusion_pack(score_before, score_after))
```

**When to use:** Actors that process data and need to hand it off to a different world's memory domain.

### Pattern 6: Chained Teleports

Multiple teleports in sequence, each moving data through a pipeline of worlds.

```kn
world StageA:
    surface ... => PanelA
world StageB:
    surface ... => PanelB
world StageC:
    surface ... => PanelC

fn pipeline(value: MyData) -> MyData:
    let a = teleport value from StageA to StageB
    let b = teleport a from StageB to StageC
    return b
```

Note: teleport returns the value, so you can chain them. But the compiler marks `value` as moved after the first teleport \<--> you cannot refer to the original binding after the handoff.

______________________________________________________________________

## Teleport + Shatter

Shatter structs are the natural payload for teleport. The SoA (Structure of Arrays) layout that the compiler generates for `shatter struct` is preserved across the world boundary ... the runtime relocates the entire allocation, not individual fields.

```kn
shatter struct AgentParticle:
    x: Float
    y: Float
    vx: Float
    vy: Float
    alive: Bool

world NativeWorld:
    state particles: Int = 0
    surface native_ui => SimulationPanel

world GpuWorld:
    state particles_copy: Int = 0
    surface viewport3d => "gpu-view"

pulse agent_sinus every 16ms jitter 1ms:
    let particle = AgentParticle {
        x: 1.0, y: 2.0,
        vx: 0.5, vy: 0.25,
        alive: true
    }
    let gpu_particle = teleport particle from NativeWorld to GpuWorld via gpu_upload
    let _pulse_budget = pulse_tick + pulse_dt_ms
    let _alive_after_handoff = gpu_particle.alive
```

**What happens under the hood:**

1. `AgentParticle` is a `shatter struct` ~> the compiler lays out `x`, `y`, `vx`, `vy`, `alive` as separate SoA lanes
1. When teleported, the entire allocation (all lanes) is handed off via `kain_machine_teleport_ptr`
1. The pointer is the same -- no data is copied, only the ownership/provenance is transferred
1. The target world reads the same SoA layout with the same access pattern

**Teleport + shatter is a zero-copy SoA transfer.** No serialization, no lane repacking, no per-element copy loop.

______________________________________________________________________

## Teleport + Axiom

Teleport pairs with `axiom` through the `world.teleport` capability. An axiom can declare `capability("world.teleport")` to gate teleport-using code on machines that support it (which is all machines running the native runtime).

```kn
axiom teleport_truth:
    when target("llvm")
    when capability("world.teleport")
    guarantee "world teleport is available on this machine profile"
    fallback copy_fallback

world Authority:
    state signal: Int = 0

world Mirror:
    state signal_copy: Int = 0

fn teleport_safe(value: Int) -> Int:
    // The axiom's capability bit is always set in the native runtime,
    // but this pattern documents the assumption in the runtime contract
    return teleport value from Authority to Mirror via safe_bus
```

The `world.teleport` capability is always available in the native runtime (`KAIN_MACHINE_CAP_WORLD_TELEPORT = 0x00000008`), so the axiom check always passes on native LLVM targets. The purpose of the axiom is documentation === it declares the dependency in the runtime contract.

The LLVM codegen maps `world.teleport` to bit 3 in the capability mask:

```rust
// codegen_llvm/mod.rs
"world.teleport" | "interop.zero-copy-handoff" => 1u64 << 3,
```

And the native C runtime automatically sets bit 3 in every machine's capability mask:

```c
uint64_t caps = KAIN_MACHINE_CAP_ATOMIC_BITMASK |       // bit 0
                KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER |   // bit 1
                KAIN_MACHINE_CAP_MEMORY_SHATTER |        // bit 2
                KAIN_MACHINE_CAP_WORLD_TELEPORT;          // bit 3 (always set)
```

______________________________________________________________________

## Teleport + Ownership (Decay)

Teleport integrates with Kain's ownership model (`collapse`, `observe`, `decay`). A teleported value can originate from or be consumed within an ownership scope.

From the pulse_teleport_decay_mesh benchmark ~~ teleport inside a `collapse` block:

```kn
collapse cells:
    var i: Int = 0
    while i < rounds:
        let lane: Int = i % 8
        let slot: Int = ((i * 5) + lane) % cell_count
        let shard = PulseShard {
            bias: shards[lane].bias,
            phase: shards[lane].phase,
            salt: shards[lane].salt,
            hot: shards[lane].hot
        }
        let moved = teleport shard from PulseAuthority to PulseMirror via pulse_hot_bus
        let old_cell: Int = mem_load(ptr_offset(cells, slot, "Int"), "Int")
        // ... process moved value alongside owned cells ...
        i = i + 1
    0  // collapse block returns 0, cells decay after the block
```

After the `collapse` block exits:

- The raw pointer `cells` is decayed
- But the teleported `moved` values now live in `PulseMirror`'s domain
- The ownership scope and the teleport scope are independent

The native runtime ensures this with `__kain_ownership_flush_deferred_decay()` at the top of `kain_machine_teleport_ptr` – any pending deferred decays are processed before the handoff, preventing race conditions between ownership cleanup and teleport relocation.

**Key rule:** teleport moves ownership from the expression's current scope to the target world. The source variable is marked moved and cannot be used. But the collapsed pointer's lifecycle is orthogonal – the `collapse` block still owns the pointer until the block ends.

______________________________________________________________________

## The `via` Channel

The optional `via` clause names a channel for the teleport. Channels serve three purposes:

### 1. Routing / Documentation

A channel name describes the data flow:

```kn
let gpu = teleport particle from NativeWorld to GpuWorld via gpu_upload
let audio = teleport buffer from EngineWorld to FxWorld via audio_feed
let vis = teleport mesh from LoadWorld to RenderWorld via geometry_bus
```

Each named channel is a distinct teleport flow, documented by its name.

### 2. Telemetry Distinction

The channel name is hashed into the provenance token:

```c
uint64_t token = kain_machine_hash_text((uintptr_t)ptr, source_world);
token ^= kain_machine_hash_text(token, target_world);
token ^= kain_machine_hash_text(token, channel);
```

Different channels between the same pair of worlds produce different tokens. You can inspect `kain_machine_teleport_last_token()` to identify which channel was last used.

### 3. Compiler Validation

The typechecker validates that a channel, if provided, is non-empty:

```rust
if channel.as_ref().is_some_and(|name| name.trim().is_empty()) {
    return Err(env.type_error("teleport channel cannot be empty", *span));
}
```

This prevents accidentally writing `teleport x from A to B via ""` (empty channel string).

______________________________________________________________________

## Runtime Contract Metadata

When a program contains teleport expressions, the runtime contract includes teleport metadata:

### Capabilities Emitted

| Capability Key | Provider | Description |
|---|---|---|
| `world.teleport` | `kain-core.runtime` | "Program uses destructive zero-copy ownership handoff across worlds." |
| `interop.zero-copy-handoff` | `kain-core.runtime` | "Teleport expressions require no-copy destination ownership materialization." |

These are emitted when `summary.teleports > 0`:

```rust
if summary.teleports > 0 {
    capabilities.push(runtime_capability(
        "world.teleport",
        "kain-core.runtime",
        Some("Program uses destructive zero-copy ownership handoff across worlds."),
    ));
    // ...
    capabilities.push(runtime_capability(
        "interop.zero-copy-handoff",
        "kain-core.runtime",
        Some("Teleport expressions require no-copy destination ownership materialization."),
    ));
}
```

### Pulse Body Teleport Flag

Pulse contracts include a `body_teleports` boolean flag, set to `true` when a pulse body contains any teleport expression:

```rust
pub struct RuntimePulseContract {
    pub name: String,
    pub interval: String,
    pub interval_ms: u64,
    pub jitter: Option<String>,
    pub body_ownership_ops: bool,
    pub body_teleports: bool,   // ← true when pulse body contains teleport
}
```

The detection walks the pulse body's statement tree:

```rust
fn block_contains_teleport_expr(block: &Block) -> bool {
    block.stmts.iter().any(stmt_contains_teleport_expr)
}
fn stmt_contains_teleport_expr(stmt: &Stmt) -> bool {
    match stmt {
        Stmt::Let { value, .. } => value.as_ref().is_some_and(expr_contains_teleport_expr),
        Stmt::Expr(expr) => expr_contains_teleport_expr(expr),
        Stmt::Defer { expr, .. } => expr_contains_teleport_expr(expr),
        Stmt::Return(value, _) | Stmt::Break(value, _) => {
            value.as_ref().is_some_and(expr_contains_teleport_expr)
        }
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => { /* ... */ }
        // etc.
    }
}
fn expr_contains_teleport_expr(expr: &Expr) -> bool {
    matches!(expr, Expr::Teleport { .. })
}
```

### Telemetry Summary

The runtime contract summary tracks `teleports` as a scalar count:

```rust
pub struct RuntimeContractSummary {
    // ...
    pub teleports: usize,
    // ...
}
```

______________________________________________________________________

## Stdlib Telemetry API

The stdlib exposes three native functions for teleport telemetry, all in `std::runtime`:

```kn
// std::runtime >> teleport telemetry

/// Returns the total number of teleport operations performed.
/// Monotonic, never decrements, atomic.
fn kain_machine_teleport_count() -> Int

/// Returns the provenance token of the most recent teleport.
/// The token is a 64-bit hash of (ptr, source_world, target_world, channel).
fn kain_machine_teleport_last_token() -> Int

/// Returns the runtime handle of the most recent teleport's allocation.
/// Zero if the most recent teleport was a non-tracked pointer.
fn kain_machine_teleport_last_handle() -> Int
```

The public API through `std::runtime`:

```kn
// In stdlib/runtime.kn:

pub fn runtime_machine_teleport_count() -> Int:
    return native_machine_teleport_count()

pub fn runtime_machine_teleport_last_token() -> Int:
    return native_machine_teleport_last_token()
```

### Usage in Tests

```kn
use std::runtime

fn verify_teleport_fired() -> Int:
    let count = runtime_machine_teleport_count()
    if count < 1:
        return -1   // teleport never fired
    if count < expected_teleports:
        return -2   // not enough teleports
    return 0
```

From the fusion chain benchmark ~ checking teleport telemetry across all layers:

```kn
fn fusion_full_causal_chain_checksum(iterations: Int, modulus: Int) -> Int with Unsafe:
    // Snapshot telemetry – every layer must increment
    let teleport_before = runtime_machine_teleport_count()
    // ... run pipeline ...
    let teleport_delta = runtime_machine_teleport_count() - teleport_before
    if teleport_delta < 1:
        return -12   // teleport layer never fired
    // ...
```

______________________________________________________________________

## Under-Utilized Ideas – Creative Semantic Abuse

### 1. Teleport as Zero-Copy Parameter Passing

Currently, teleport moves data between worlds. But teleport is an expression – it could be used inside function calls to hand off ownership of a parameter without copying:

```kn
fn process_and_sink(data: SomeBigStruct) -> Int:
    // Process data locally, then teleport to the archive world
    let score = score_data(data)
    let archived = teleport data from WorkingWorld to ArchiveWorld via archived_bus
    return score + archived.checksum
```

The data is not copied from caller to callee => it moves from the calling context's world to the function's world via teleport.

### 2. Teleport as Hot-Data Migration

In a game or simulation, teleport can migrate a hot particle or entity from CPU to GPU world without copying:

```kn
pulse migration_clock every 8ms jitter 1ms:
    let hot_idx = find_hottest_particle(SimWorld)
    let particle = SimWorld.particles[hot_idx]
    let gpu_copy = teleport particle from SimWorld to GpuWorld via hot_migration
    GpuWorld.active_particles = GpuWorld.active_particles + 1
```

**Under-explored:** Teleport as a migration primitive, moving hot data to the world where it will be processed next, without the cost of serialization, copying, or IPC.

### 3. Teleport + Resonate: Trigger on Handoff

A resonate handler on a world field could fire when a teleported value arrives:

```kn
resonate MirrorWorld.last_arrival dampen 0ms:
    let arrived = resonate_new_i64
    MirrorWorld.processed = process(arrived)
```

The teleport writes to the destination world's field, triggering the resonate handler. This creates a one-shot reactive handoff => teleport as a directed pulse of state change.

### 4. Teleport as Builder-Pipeline Terminator

A builder function builds a value, then teleports it to a result world:

```kn
fn build_and_deliver(config: Config) -> Int:
    let obj = ComplexObject {
        field_a: build_a(config),
        field_b: build_b(config),
        field_c: build_c(config),
    }
    let delivered = teleport obj from BuilderWorld to ClientWorld via delivery
    return delivered.id
```

**Under-explored:** Teleport as a composable builder-terminator. The builder owns exclusive access during construction, then teleports the fully-built value to the consumer world.

### 5. Teleport as World-to-World Mailbox

Multiple teleports from different sources into the same target world create a mailbox-like pattern:

```kn
// Source A sends
let msg_a = teleport payload_a from WorldA to MailboxWorld via channel_a

// Source B sends
let msg_b = teleport payload_b from WorldB to MailboxWorld via channel_b
```

The MailboxWorld accumulates teleported values from multiple sources, each via a distinct channel. The `teleport_count` and `teleport_last_token` provide arrival-order telemetry.

### 6. Teleport for Ownership Debugging

If you're tracking ownership bugs, instrument teleport count:

```kn
fn debug_teleport_flow(value: SomeData) -> SomeData:
    let before = runtime_machine_teleport_count()
    let moved = teleport value from DebugSource to DebugTarget via debug_bus
    let after = runtime_machine_teleport_count()
    let _delta = after - before
    DebugWorld.last_teleport_token = runtime_machine_teleport_last_token()
    return moved
```

______________________________________________________________________

## Combining with Other Keywords

### Teleport + World

Teleport is meaningless without worlds ~ both source and target must be declared worlds:

```kn
world Authority:
    state signal: Int = 0

world Mirror:
    state signal_copy: Int = 0

fn handoff(value: Int) -> Int:
    return teleport value from Authority to Mirror via bus
```

### Teleport + Shatter

The canonical payload shape. SoA layout preserved across the handoff. See [Teleport + Shatter](#teleport--shatter) above.

### Teleport + Axiom

Capability gating via `world.teleport`. See [Teleport + Axiom](#teleport--axiom) above.

### Teleport + Pulse

Timed teleport beats. The body creates a fresh value and teleports it on every tick:

```kn
pulse teleport_clock every 16ms jitter 2ms:
    let shard = MyShard { x: pulse_tick, y: pulse_dt_ms, alive: true }
    let moved = teleport shard from Authority to Mirror via clock_bus
```

### Teleport + Actor

Teleport inside actor message handlers. The actor builds or receives data and teleports it:

```kn
actor Producer:
    on Emit(reply_to: P, value: Int):
        let shard = MyShard { x: value, y: value * 2, alive: true }
        let moved = teleport shard from WorldA to WorldB via producer_bus
        send reply_to.Ack(ok = true)
```

### Teleport + Entangle

Teleport and entangle are complementary ~~ teleport is one-shot handoff; entangle is continuous coupling. Both move data between worlds; teleport moves ownership, entangle copies values.

### Teleport + Collapse/Observe/Decay

Teleport can originate from or land in ownership blocks. See [Teleport + Ownership](#teleport--ownership-decay) above.

### Teleport + Orchestrate

Teleport is an expression that can appear inside orchestrate stages (which are function bodies):

```kn
orchestrate pipeline(value: Int) -> Int:
    stage process: cpu process(value) residency host
    stage deliver: cpu teleport(process) from Authority to Mirror residency host
    return deliver
```

But note: the orchestrate stage's `residency` and `transfer` clauses are about device memory management (host ↔ device). Teleport is about world-to-world handoff within the same address space. They are orthogonal concerns.

### Teleport + Law

Laws can validate teleported values:

```kn
law teleported_value_valid(value: MyData) -> Bool:
    return value.checksum > 0 and value.alive == true

fn handoff(value: MyData) -> MyData:
    if law_status(teleported_value_valid(value)) == false:
        return value  // reject the handoff
    let moved = teleport value from Authority to Mirror via validated_bus
    return moved
```

### Teleport + Converge

Teleport can appear inside converge lanes:

```kn
converge deliver(value: MyData) -> MyData:
    spec reference:
        // Copy-based reference implementation
        return copy_to_world(value, Authority, Mirror)
    fast teleport_lane when capability("world.teleport"):
        return teleport value from Authority to Mirror via fast_bus
    verify random(4)
```

______________________________________________________________________

## Native C API

### Header (`runtime/native/include/machine_stones.h`)

```c
// Public teleport API

/// Perform a zero-copy teleport handoff.
/// Returns the pointer (unchanged - handoff is conceptual).
/// Counts the operation atomically and stores provenance metadata.
void* kain_machine_teleport_ptr(
    void* ptr,
    const char* source_world,
    const char* target_world,
    const char* channel
);

/// Record a teleport note without moving data.
/// Used for non-pointer (scalar) teleport expressions.
void kain_machine_teleport_note(
    const char* source_world,
    const char* target_world,
    const char* channel
);

/// Return the total number of teleport operations since runtime start.
uint64_t kain_machine_teleport_count(void);

/// Return the provenance token of the most recent teleport.
/// 64-bit hash of (ptr_address, source_world, target_world, channel).
uint64_t kain_machine_teleport_last_token(void);

/// Return the runtime handle of the most recent teleport's allocation.
/// 0 if the most recent teleport used a non-tracked pointer.
uint64_t kain_machine_teleport_last_handle(void);
```

### Capability Constant

```c
#define KAIN_MACHINE_CAP_WORLD_TELEPORT UINT64_C(0x0000000000000008)  // bit 3
```

This bit is always set in `kain_machine_current_capabilities()` => the native runtime always supports teleport.

### Global State

```c
static atomic_uint_fast64_t g_machine_teleport_count;       // atomic monotonic counter
static atomic_uint_fast64_t g_machine_teleport_last_token;   // last provenance token
static atomic_uint_fast64_t g_machine_teleport_last_handle;  // last fixup handle
static atomic_flag g_machine_teleport_untracked_warned;      // one-shot warning flag
```

All telemetry variables use relaxed memory ordering for the count (performance) and release/acquire for token/handle (visibility).

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck teleport expression
kain check <file.kn> --target llvm

# 2. Compile and run (teleport executes in native LLVM)
kain run <file.kn> --target llvm

# 3. Verify teleport fired (check telemetry)
kain run <file.kn> --target llvm
# Add a test that checks runtime_machine_teleport_count() > 0

# 4. Smoketest semantics
kain run smoketest/src/semantics/teleport.kn --target llvm

# 5. Machine stones blade (axiom + pulse + shatter + teleport fusion)
kain run blades/test/machine-stones/src/main.kn --target llvm

# 6. Pulse-teleport-decay-mesh benchmark (54K iterations, 13+ semantics)
python benchmark/run.py --case pulse_teleport_decay_mesh --languages kain --runs 1 --warmups 0

# 7. Fusion chain (teleport in full 7-layer causal chain)
python benchmark/run.py --case fusion_chain --languages kain --runs 1 --warmups 0

# 8. Check runtime contract
# The output runtime_contract.json will show "world.teleport" and "interop.zero-copy-handoff" capabilities
# Pulse contracts will have body_teleports=true when pulses contain teleport
```

### Smoketest

```kn
// smoketest/src/semantics/teleport.kn
use std::runtime
use std::machine
use shatter::SmokeShard
use shatter::smoke_shard_score

world SmokeTeleportAuthority:
    state signal: Int = 1
    surface web => SmokeTeleportPanel

world SmokeTeleportMirror:
    state signal_copy: Int = 1
    surface web => SmokeTeleportPanel

pub fn smoke_teleport_lane() -> Int:
    let shard = SmokeShard { bias: 42, phase: 7, salt: 13, alive: true }
    let score_before = smoke_shard_score(shard)

    let moved = teleport shard from SmokeTeleportAuthority to SmokeTeleportMirror via smoke_teleport_bus

    // Verify field integrity after handoff
    if moved.bias != 42:  return 1
    if moved.phase != 7:  return 2
    if moved.alive != true: return 3

    // Verify structural integrity (scoring function produces same result)
    let score_after = smoke_shard_score(moved)
    if score_after != score_before: return 4

    // Verify telemetry fired
    let teleport_count = runtime_machine_teleport_count()
    if teleport_count < 1: return 5
    return 0
```

______________________________________________________________________

## Z3 Proofs

### Proof 1: Post-Handoff Ownership Exclusivity

**File:** `runtime/native/src/core/z3/proofs/native-machine-teleport-token-handoff-is-exclusive.yaml`

**Proves:** After a teleport handoff, the destination owns the data exclusively. The source and destination cannot both be live simultaneously >> the handoff is destructive, not a shared copy.

```smt2
(set-logic QF_BV)
(declare-const owner_after (_ BitVec 2))
(define-fun SOURCE () (_ BitVec 2) #b00)
(define-fun DESTINATION () (_ BitVec 2) #b01)
(assert (= owner_after DESTINATION))
(define-fun source_live () Bool (= owner_after SOURCE))
(define-fun destination_live () Bool (= owner_after DESTINATION))
(assert (and source_live destination_live))
(check-sat)
```

The assertion `(and source_live destination_live)` checks whether both can be true simultaneously. `check-sat` returns `UNSAT` ~ it is impossible for both source and destination to own the data after teleport. The post-handoff owner is exclusively the destination.

**Invariants verified:**

- `owner_after = DESTINATION` * * * the destination receives exclusive ownership
- `source_live = false` => the source does not retain ownership
- `destination_live = true` ~ the destination holds the only live reference

### Proof 2: Published Handle is Active

**File:** `runtime/native/src/core/z3/proofs/native-machine-teleport-publishes-only-active-handle.yaml`

**Proves:** When teleport publishes a fixup-backed handle for telemetry (`kain_machine_teleport_last_handle()`), the published value is always the actual handle of the teleported pointer === never an unrelated foreign handle.

```smt2
(set-logic QF_LIA)
(declare-const teleported_handle Int)
(declare-const foreign_handle Int)
(assert (> teleported_handle 0))
(assert (> foreign_handle 0))
(assert (distinct teleported_handle foreign_handle))
(define-fun published_last_handle () Int teleported_handle)
(assert (= published_last_handle foreign_handle))
(check-sat)
```

The proof asserts that the published handle equals a foreign handle - this is `UNSAT`, proving the published handle is always the teleported pointer's handle (or zero when no handle exists). The telemetry cannot report a handle from an unrelated allocation.

### Proof 3: Capability Token Collision Freedom (Adjacent)

The capability token collision proof (`native-machine-capability-token-signatures-are-collision-free.yaml`) also covers `world.teleport` ~~ its 14-byte token signature `(0x0e, 'w', 'o', 't')` is demonstrably distinct from all other capabilities in the runtime's hash switch.

______________________________________________________________________

## Source Anchors

### Parser & AST (`crates/core/`)

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 77 | `"teleport"` keyword constant |
| `crates/core/src/ast.rs` | 1866 | `Expr::Teleport { value, source_world, target_world, channel, span }` |
| `crates/core/src/ast.rs` | 1994 | Teleport span-handling in expression span merge |
| `crates/core/src/ast.rs` | 3400 | Teleport in expression visitor |
| `crates/core/src/parser.rs` | 4790 | `parse_teleport_expr()` dispatch from postfix |
| `crates/core/src/parser.rs` | 4977-4995 | `parse_teleport_expr()` ~> full implementation |

### Typechecker

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/types.rs` | 7869-7873 | Moved-identifier error message |
| `crates/core/src/types.rs` | 8943-8961 | `infer_expr_type` for `Expr::Teleport` |
| `crates/core/src/types.rs` | 9076-9092 | `ensure_teleport_world_reference()` => world resolution |
| `crates/core/src/types.rs` | 5219-5222 | Teleport in object expression walk |
| `crates/core/src/types.rs` | 6494-6498 | Teleport in pointer expression walk |

### Runtime Contract

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime_contract.rs` | 166-168 | `body_teleports` flag in pulse contract |
| `crates/core/src/runtime_contract.rs` | 625-636 | `world.teleport` + `interop.zero-copy-handoff` capability emission |
| `crates/core/src/runtime_contract.rs` | 1767 | `teleports` count in contract summary |
| `crates/core/src/runtime_contract.rs` | 1826-1828 | `teleport` in ownership expression detection |
| `crates/core/src/runtime_contract.rs` | 2341-2356 | `block_contains_teleport_expr`, `stmt_contains_teleport_expr`, `expr_contains_teleport_expr` |

### LLVM Codegen

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 1427 | Teleport pointer-let type collection |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 1849 | `world.teleport` → bit 3 capability mapping |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 3858-3860 | Teleport addressable-local check |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 4734-4735 | Teleport identifier-name collection |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 5374 | Teleport ephemeral-local safety check |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11574-11633 | `compile_teleport_expr()` ... full LLVM codegen for teleport |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 18669-18678 | Teleport dispatch in compile_expr |

### Native C Runtime

| File | Lines | Content |
|------|-------|---------|
| `runtime/native/include/machine_stones.h` | 13 | `KAIN_MACHINE_CAP_WORLD_TELEPORT` capability constant (bit 3) |
| `runtime/native/include/machine_stones.h` | 40-49 | `kain_machine_teleport_ptr`, `kain_machine_teleport_note` declarations |
| `runtime/native/include/machine_stones.h` | 55-57 | `kain_machine_teleport_count`, `kain_machine_teleport_last_token`, `kain_machine_teleport_last_handle` declarations |
| `runtime/native/src/core/machine_stones.c` | 75-78 | Teleport global state: `g_machine_teleport_count`, `g_machine_teleport_last_token`, `g_machine_teleport_last_handle`, `g_machine_teleport_untracked_warned` |
| `runtime/native/src/core/machine_stones.c` | 487-524 | `kain_machine_teleport_ptr()` ~> full implementation |
| `runtime/native/src/core/machine_stones.c` | 526-530 | `kain_machine_teleport_note()` ->> bookkeeping wrapper |
| `runtime/native/src/core/machine_stones.c` | 532-544 | `kain_machine_teleport_count()`, `kain_machine_teleport_last_token()`, `kain_machine_teleport_last_handle()` |

### Z3 Proofs

| File | Content |
|------|---------|
| `runtime/native/src/core/z3/proofs/native-machine-teleport-token-handoff-is-exclusive.yaml` | Proves post-teleport ownership is exclusively the destination |
| `runtime/native/src/core/z3/proofs/native-machine-teleport-publishes-only-active-handle.yaml` | Proves teleported handle is always the active one |
| `runtime/native/src/core/z3/proofs/native-machine-capability-token-signatures-are-collision-free.yaml` | Proves `world.teleport` token signature is collision-free |

### Stdlib

| File | Content |
|------|---------|
| `stdlib/runtime.kn` | `kain_machine_teleport_count()`, `kain_machine_teleport_last_token()` --> extern decls |
| `stdlib/runtime.kn` | `runtime_machine_teleport_count()`, `runtime_machine_teleport_last_token()` - pub wrappers |

### Examples

| File | Usage |
|------|-------|
| `smoketest/src/semantics/teleport.kn` | Minimal smoke test: shatter struct + teleport + field verification + score integrity + telemetry check |
| `smoketest/src/semantics/pulse.kn` | Pulse body with teleport + shatter + timing locals (identical pattern) |
| `library_of_kain/teleport.kn` | Same pattern as smoketest (library copy) |
| `blades/test/machine-stones/src/main.kn` | Full machine-stones dogfood: axiom + pulse + shatter + teleport + telemetry |
| `benchmark/cases/pulse_teleport_decay_mesh/main.kn` | **Apex:** 54K iterations, pulse + teleport + world + entangle + patch + law + converge + orchestrate + actor + collapse/observe/decay, 13+ semantics |
| `benchmark/cases_v2/fusion_chain.kn` | Teleport inside actor handler (FusionTeleporter.ShatterAndSend) in 7-layer causal chain |
| `benchmark/cases/semantic_singularity/main.kn` | Teleport in semantic singularity stress test |

______________________________________________________________________

## Anti-Patterns

### ❌ Using Teleport For Simple Copy Between Worlds

```kn
// WRONG: teleport for a read-only copy
fn read_signal() -> Int:
    let moved = teleport Authority.signal from Authority to Mirror
    // Authority.signal is now moved ~ can't read it anymore!
    return moved + Authority.signal   // COMPILE ERROR: moved
```

```kn
// RIGHT: use entangle for continuous copying
entangle Authority.signal <-> Mirror.signal_copy with single_writer

fn read_signal() -> Int:
    return Mirror.signal_copy   // always reflects Authority's value
```

**Why:** Teleport is destructive ‒ the source binding is invalidated. If you want a copy that preserves both sides, use `entangle` for continuous propagation or just read the target field directly.

### ❌ Teleporting Without Checking Telemetry

```kn
// WRONG: no proof the teleport actually happened
fn handoff(value: Int) -> Int:
    let moved = teleport value from A to B via bus
    return moved
```

```kn
// RIGHT: verify telemetry in test/validation code
fn handoff(value: Int) -> Int:
    let count_before = runtime_machine_teleport_count()
    let moved = teleport value from A to B via bus
    let count_after = runtime_machine_teleport_count()
    if count_after - count_before < 1:
        return -1   // teleport didn't fire
    return moved
```

**Why:** Every teleport should increment the counter. If your test expects teleport to happen, check the count. The smoke test and fusion chain benchmarks both do this.

### ❌ Teleporting Between Same World

```kn
// WRONG: teleport requires distinct source and target worlds
fn noop(value: Int) -> Int:
    let moved = teleport value from A to A via bus   // COMPILE ERROR
    return moved
```

The typechecker rejects this: `"teleport requires distinct source and target worlds"`.

### ❌ Using a Moved Identifier After Teleport

```kn
// WRONG: source identifier is moved
fn double_use(value: Int) -> Int:
    let moved = teleport value from A to B via bus
    return value + moved   // COMPILE ERROR: 'value' was moved by teleport
```

```kn
// RIGHT: use the return value
fn correct_use(value: Int) -> Int:
    let moved = teleport value from A to B via bus
    return moved   // OK => using the moved value
```

**Why:** The typechecker marks the source identifier as moved (`env.mark_moved(name)`). Any subsequent access produces: `"Identifier 'value' was moved by teleport and cannot be used again"`.

### ❌ Empty Channel Name

```kn
// WRONG: channel cannot be empty
let moved = teleport value from A to B via ""   // COMPILE ERROR
```

The typechecker rejects: `"teleport channel cannot be empty"`. Either omit `via` entirely or provide a non-empty channel name.

### ❌ Using Teleport for Cross-Process / Cross-Machine Data Transfer

```kn
// WRONG: teleport is in-process only
let moved = teleport data from LocalWorld to RemoteWorld     // RemoteWorld outside process space
```

Teleport operates within a single process address space. Worlds are in-process memory domains, not network endpoints. For cross-process or cross-machine transfer, use actor messages, IPC, or networking |-> not teleport.

### ❌ Expecting Teleport to Be Concurrently Safe

```kn
// WRONG: teleport is not a concurrency primitive
// Thread A:
let moved = teleport value from A to B
// Thread B (same time):
let moved2 = teleport other from A to B
```

Teleport operates at the expression level with relaxed memory ordering for the counter and release for the token/handle. It does not provide mutual exclusion or synchronization between concurrent teleports. Use actors or atomics for concurrent access patterns.

### ❌ Forgetting That Teleport Returns the Value

```kn
// WRONG: ignoring the return value
teleport shard from A to B via bus   // value is lost
```

```kn
// RIGHT: capture the moved value
let moved = teleport shard from A to B via bus   // moved is now in B's domain
```

**Why:** Teleport is an expression, not a statement. It returns the moved value. If you don't capture the return value, the moved value is silently dropped >> which may be intentional (if you only care about the telemetry) but is usually a bug.

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `teleport <expr> from <world> to <world> [via <channel>]` |
| **Category** | Expression-level keyword (not a declaration) |
| **Return value** | The moved value (same type as input) |
| **Memory model** | Zero-copy ->> pointer is handed off, not copied |
| **Source invalidation** | ✅ Compiler marks identifier as moved |
| **Source/target** | String-like arguments resolving to declared `world` names |
| **Channel** | Optional `via <string>` - non-empty, routed into provenance token |
| **Distinct worlds** | Required ‒ compiling rejects same-world teleport |
| **Typecheck** | World resolution, move marking, distinct check, non-empty channel |
| **LLVM codegen** | Pointer types → `kain_machine_teleport_ptr()`; scalars → `kain_machine_teleport_note()` |
| **Native API** | `kain_machine_teleport_ptr(void*, src, tgt, ch)` returns `void*` |
| **Capability** | `KAIN_MACHINE_CAP_WORLD_TELEPORT` (bit 3) ~ always available |
| **Telemetry** | `runtime_machine_teleport_count()`, `runtime_machine_teleport_last_token()` |
| **Provenance token** | 64-bit hash: `mix(ptr, source_world, target_world, channel)` |
| **Runtime contract** | `world.teleport` + `interop.zero-copy-handoff` capabilities emitted when `teleports > 0` |
| **Pulse contract flag** | `body_teleports: bool` ~> true when pulse body contains teleport |
| **Z3 proofs** | Exclusive post-handoff ownership + published handle is always active handle |
| **Canonical example** | `smoketest/src/semantics/teleport.kn` :: full smoke test with verification |
| **Apex benchmark** | `benchmark/cases/pulse_teleport_decay_mesh/main.kn` 〰 54K iterations, 13+ semantics |
| **Fusion example** | `benchmark/cases_v2/fusion_chain.kn` |-> teleport in actor handler (FusionTeleporter) |
| **Pair with** | `world` (source/target), `shatter` (SoA payload), `pulse` (timed handoff), `actor` (handler context), `axiom` (capability gate), `entangle` (continuous alternative) |
