---
name: lang-systems
description: >-
  Use when authoring, explaining, reviewing, or repairing systems-level Kain code: actors and message pressure, `spawn`/`send`/`ask`, async/future flows, Koka-like effect annotations, `collapse`/`observe`/`decay`, raw pointers, `alloc`/`alloc_zeroed`/`realloc_mem`, `ptr_offset`/`mem_load`/`mem_store`, zero-copy buffers, branchless/bit/cache-line lanes, shatter-friendly data layout, pulse/teleport systems fusion, actor runtime telemetry, and proof/benchmark/attrition expectations for unsafe or low-level authored `.kn` code. Use this for writing IN Kain; use bootstrap/runtime skills when changing parser, typechecker, lowering, scheduler, mailbox, heap, or native ABI internals.
---

# Lang Systems

This is the metal-authoring field manual for Kain. Use it when the authored `.kn` code is not just business logic: it is moving memory, driving actors, declaring effects, using unsafe lanes, shaping cache behavior, proving teardown, or fusing Kain semantics into a systems workload.

## Prime Directive

- Keep systems work authored in Kain when the behavior belongs to Kain semantics.
- Make pressure visible: actors should show turns/messages/backpressure, ownership should show exclusivity/observation/teardown, memory code should show bounds and layout intent.
- Use `lang-semantics` when the systems lane fuses `world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `pulse`, `teleport`, or `shatter`.
- Do not flatten Kain systems code into C cosplay. Kain can say `actor`, `collapse`, `observe`, `decay`, `with Unsafe`, `shatter struct`, and `teleport`; use those when they fit.
- If the desired systems code exposes a compiler/runtime bug, preserve the authored Kain shape and hand off the engine defect to `bootstrap-*` or `runtime-*`.

## Fast Operator Loop

```powershell
rg -n "\b(actor|spawn|send|ask|async|await|with (Pure|IO|Async|GPU|Reactive|Unsafe)|collapse|observe|decay|ptr<|alloc_zeroed|ptr_offset|mem_load|mem_store|shatter struct)\b" library_of_kain blades benchmark smoketest stdlib
rg -n "Effect|EffectSet|parse_effects|parse_actor|Expr::(Spawn|SendMsg|Collapse|Observe|Decay|PtrOffset|MemLoad|MemStore|Alloc|Realloc|Await|AsyncBlock)" crates/kain-core/src
rg -n "kain_actor_|__kain_ownership|kain_machine_shatter|kain_machine_teleport|mailbox|scheduler" runtime/native crates/kain-sys-codegen/src
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade> --target llvm
```

Best first examples:

- `library_of_kain/actor_ownership_backpressure.kn`: actor swarm, ownership region, memory cells, world/entangle, patch/law, converge/orchestrate, teleport, runtime telemetry.
- `benchmark/cases/semantic_singularity_crucible/main.kn`: dirty memory, async/await, actors, teleport, collapse/observe/decay, semantic fusion.
- `benchmark/cases/pulse_teleport_decay_mesh/main.kn`: pulse plus teleport plus actor plus ownership teardown.
- `benchmark/cases/quantumerlang/main.kn`: actor/message/ownership/converge/teleport/world pressure.
- `benchmark/cases/actor_mailbox_erlang/main.kn`: compact actor mailbox/ask pressure.
- `benchmark/cases/ownership_memory/main.kn`: minimal ownership memory lane.
- `benchmark/cases/simd_lane_mix/main.kn`: pointer-backed SIMD-ish lane mixing.
- `benchmark/cases/sim_nbody_gravity/main.kn`: raw pointer arrays and physics-style hot loops.
- `stdlib/actor.kn`: public actor ABI telemetry helpers.
- `stdlib/alloc.kn` and `stdlib/collections.kn`: public stdlib examples of pointer-backed authored data structures.

## Systems Flow

Systems Kain normally crosses these layers:

```text
.kn systems source
-> effect parsing and AST in crates/kain-core/src/parser.rs + ast.rs
-> type/effect/ownership/actor validation in crates/kain-core/src/types.rs
-> portable actor/ownership truth in crates/kain-actor + crates/kain-ownership
-> interpreter behavior in crates/kain-core/src/runtime.rs where supported
-> LLVM/C/Rust lowering in crates/kain-sys-codegen
-> native runtime substrate in runtime/native for actors, ownership, heap, shatter, teleport, pulse
-> benchmark/attrition/Z3 evidence when the claim is performance, safety, or teardown
```

Source anchors:

- Effects: `crates/kain-core/src/effects.rs`, `parser.rs parse_effects`, `types.rs check_effect_call`.
- Actor syntax: `parser.rs parse_actor_with_attrs`, `parse spawn`, `parse send`, `ast.rs Actor`, `Expr::Spawn`, `Expr::SendMsg`.
- Actor validation: `types.rs check_actor`, `crates/kain-actor/src/validation.rs`, `crates/kain-actor/src/native.rs`.
- Actor runtime/interpreter: `crates/kain-core/src/runtime.rs` actor helpers, `stdlib/actor.kn`.
- Actor LLVM/native: `crates/kain-sys-codegen/src/codegen_llvm/mod.rs compile_actor`, `compile_actor_builtin_ask`, `runtime/native/include/actor.h`, `runtime/native/src/core/actor.c`.
- Ownership model: `crates/kain-ownership/src/lib.rs`, `crates/kain-core/src/types.rs` ownership inference, `runtime/native/include/ownership.h`, `runtime/native/src/core/ownership*`.
- Raw memory AST/type/lowering: `ast.rs Type::Ptr`, `Expr::PtrOffset`, `Expr::MemLoad`, `Expr::MemStore`, `Expr::Alloc`, `Expr::Realloc`, `crates/kain-core/src/low_level_memory.rs`, LLVM memory lowering.
- Machine layout/handoff: `runtime/native/include/machine_stones.h`, `runtime/native/src/core/machine_stones.c`, LLVM shatter/teleport/pulse lowering.
- Proofs: `crates/kain-ownership/z3/proofs`, `crates/kain-core/z3/proofs`, `runtime/native/src/core/z3/proofs`, subsystem benchmark cases.

## Feature Index

| Feature | Authoring Use | Syntax Shape | Main Truth |
| --- | --- | --- | --- |
| effects | Declare side-effect boundary | `fn f() -> Int with IO, Unsafe:` | `effects.rs`, `parser.rs parse_effects`, `types.rs check_effect_call` |
| `async fn` | Function that carries Async effect | `async fn ready() -> Int:` | `parser.rs parse_async_function`, `Expr::Await`, `Expr::AsyncBlock` |
| `await` | Resolve future-shaped value | `let x = await ready()` | `ast.rs Expr::Await`, `types.rs infer_expr_type` |
| `actor` | Stateful message turn owner | `actor Relay: state bias: Int = 1` | `parser.rs parse_actor_with_attrs`, `types.rs check_actor` |
| `spawn` | Create actor handle | `spawn Relay(bias = 11)` | `parser.rs spawn expression`, `Expr::Spawn`, LLVM actor spawn |
| `send` | Fire message from handler/user code | `send target.Message(value = x)` | `parser.rs send expression`, `Expr::SendMsg`, LLVM actor send |
| `ask` | Request/reply helper | `ask(actor, "Fold", request)` | `runtime.rs ask`, LLVM `compile_actor_builtin_ask`, `stdlib/actor.kn` |
| actor telemetry | Inspect native actor pressure | `actor_scheduler_queue_depth()` | `stdlib/actor.kn`, `runtime/native/src/core/actor.c` |
| `collapse` | Exclusive scoped mutation | `collapse cells: ...` | `crates/kain-ownership`, `Expr::Collapse` |
| `observe` | Read-only scoped observation | `observe cells: ...` | `crates/kain-ownership`, `Expr::Observe` |
| `decay` | Deterministic teardown | `decay cells` | `crates/kain-ownership`, `Expr::Decay` |
| raw pointer | Explicit memory region | `ptr<Int>`, `ptr<Float>` | `ast.rs Type::Ptr`, `low_level_memory.rs` |
| allocation | Own memory from Kain | `alloc_zeroed(count, "Int")` | `Expr::Alloc`, LLVM/native memory helpers |
| reallocation | Grow memory region | `realloc_mem(ptr, n, "Int", true)` | `Expr::Realloc`, stdlib/runtime helpers |
| pointer offset | Address element lane | `ptr_offset(cells, i, "Int")` | `Expr::PtrOffset`, LLVM pointer lowering |
| memory load/store | Raw cell access | `mem_load(ptr, "Int")`, `mem_store(ptr, v, "Int")` | `Expr::MemLoad`, `Expr::MemStore` |
| shatter layout | SoA layout intent | `shatter struct Particle:` | `lang-semantics`, LLVM shatter lowering, `machine_stones.c` |
| teleport handoff | Zero-copy cross-world move | `teleport value from A to B via bus` | `lang-semantics`, `types.rs`, `machine_stones.c` |
| pulse | Temporal systems tick | `pulse clock every 8ms:` | `lang-semantics`, LLVM/native pulse runtime |
| bit lanes | Branchless/packed systems math | `&`, `|`, `^`, `<<`, `>>` | typechecker, LLVM lowering, Z3 when changing rules |

## Effects: Koka-Like Boundaries

Effects are the authored contract for what a function is allowed to do. They are not decoration; they shape which calls are legal.

```kn
fn mix_pure(value: Int) -> Int with Pure:
    return ((value * 31) + 7) % 1000000007

fn write_runtime_note(value: Int) -> Int with IO:
    println("systems note")
    return mix_pure(value)

async fn ready_signal(seed: Int) -> Int:
    return seed + 29

fn dirty_lane(cells: ptr<Int>, count: Int) -> Int with Unsafe:
    var i: Int = 0
    var acc: Int = 0
    while i < count:
        acc = acc + mem_load(ptr_offset(cells, i, "Int"), "Int")
        i = i + 1
    return acc
```

Effect rules:

- Authored `with` syntax currently parses `Pure`, `IO`, `Async`, `GPU`, `Reactive`, and `Unsafe`.
- `async fn` parses as a normal function and automatically adds `Async`.
- A pure caller can only call pure callees.
- Non-pure callers must contain all callee effects.
- `Unsafe` is the escape hatch: an unsafe caller can call any effectful callee.
- Internal `Effect` also has `Alloc` and `Panic`; do not advertise those as stable authored `with` syntax unless the parser supports them in this checkout.
- Use effects to make intent obvious: IO for filesystem/network/console, Async for awaits/timers/actor waits, GPU for graphics/compute paths, Reactive for UI/event surfaces, Unsafe for raw pointer/ABI/bounds-trusted lanes.

Effect source anchors:

- `crates/kain-core/src/effects.rs`: `Effect`, `EffectSet`, `can_call`, `check_effect_call`.
- `crates/kain-core/src/parser.rs`: `parse_effects`, `parse_async_function`.
- `crates/kain-core/src/types.rs`: function/method call effect checking.
- `crates/kain-core/src/diagnostic_registry.rs`: `effects/violation`.
- `crates/kain-sys-codegen/src/codegen_rust/mod.rs`: effect-to-modifier behavior for Rust output.

## Actors: Message-Turn Systems

Actors are systems authoring because they own state over time, receive pressure, and cross into the native scheduler/mailbox ABI.

```kn
use std::actor

actor FoldRelay:
    state bias: Int = 11
    state turns: Int = 0

    on Fold(reply_to: P, request: Int):
        self.turns = self.turns + 1
        let value = ((request * 17) + self.bias + self.turns + 23) % 1000000007
        send reply_to.Reply(value = value)

fn actor_lane(rounds: Int) -> Int:
    let relay = spawn FoldRelay(bias = 11)
    let _warm = ask(relay, "Fold", 0)
    var i: Int = 0
    var acc: Int = 0
    while i < rounds:
        acc = (acc + ask(relay, "Fold", acc + i)) % 1000000007
        i = i + 1
    return acc + actor_scheduler_queue_depth()
```

Actor rules:

- Actor body supports `state`, `var` as state alias, `weak state`, `fn` methods, and `on Message(...)` handlers.
- `spawn Actor(field = value)` requires named init arguments.
- `send target.Message(field = value)` requires named message fields.
- `ask(actor, "Message", payload)` is the public request/reply helper; native LLVM currently expects the message name to be literal and the handler to begin with `reply_to: P` for reply-port lowering.
- Prefer typed-looking named messages over anonymous payload soup.
- Use actors to express long-lived pressure: request/reply, supervision, mailbox depth, fanout, worker pool, throttling, retry, or stateful relay.
- Do not mutate remote actor fields directly. Actor state should change inside handlers or actor methods.

Actor source anchors:

- `parser.rs parse_actor_with_attrs`: accepted body forms.
- `parser.rs` send/spawn expression paths: send parses method-call shape and spawn requires named arguments.
- `ast.rs`: `Actor`, `MessageHandler`, `Expr::Spawn`, `Expr::SendMsg`.
- `types.rs check_actor`: state initializer type checks, handler params, self binding, actor contract validation.
- `crates/kain-actor/src/validation.rs`: duplicate state/handler/method validation and mailbox/restart model.
- `runtime.rs`: interpreter actor registry, `send`, `ask`, `ask_timeout`, actor handler execution.
- `codegen_llvm/mod.rs`: `compile_actor`, `compile_actor_builtin_ask`, native actor declarations.
- `runtime/native/include/actor.h`, `runtime/native/src/core/actor.c`: scheduler, mailbox, reply ports, monitor/link, supervision, telemetry.
- `stdlib/actor.kn`: public actor telemetry helpers.

## Actor Pressure Patterns

Worker pool fanout:

```kn
fn ask_worker(slot: Int, w0: FoldRelay, w1: FoldRelay, w2: FoldRelay, w3: FoldRelay, request: Int) -> Int:
    if slot == 0:
        return ask(w0, "Fold", request)
    elif slot == 1:
        return ask(w1, "Fold", request)
    elif slot == 2:
        return ask(w2, "Fold", request)
    return ask(w3, "Fold", request)
```

Backpressure loop shape:

```kn
fn pressure_lane(rounds: Int) -> Int:
    let w0 = spawn FoldRelay(bias = 5)
    let w1 = spawn FoldRelay(bias = 7)
    let w2 = spawn FoldRelay(bias = 11)
    let w3 = spawn FoldRelay(bias = 13)
    let _warm = ask(w0, "Fold", 0) + ask(w1, "Fold", 0) + ask(w2, "Fold", 0) + ask(w3, "Fold", 0)
    var i: Int = 0
    var acc: Int = 0
    while i < rounds:
        let lane: Int = i & 3
        let burst: Int = ((i / 9) % 3) + 1
        var b: Int = 0
        while b < burst:
            acc = (acc + ask_worker(lane, w0, w1, w2, w3, acc + i + b)) % 1000000007
            b = b + 1
        i = i + 1
    return acc
```

Actor telemetry to prove the lane is real:

```kn
fn actor_runtime_shape_ok() -> Bool:
    return actor_abi_version() >= 3 and actor_scheduler_total_enqueued() >= actor_scheduler_total_dequeued()
```

Telemetry helpers include:

- `actor_abi_version`, `actor_default_mailbox_capacity`, `actor_default_ask_timeout_ms`.
- `actor_scheduler_queue_depth`, `actor_scheduler_max_queue_depth`, `actor_scheduler_total_enqueued`, `actor_scheduler_total_dequeued`.
- `actor_scheduler_worker_count`, `actor_scheduler_active_workers`, `actor_scheduler_busy_workers`.
- `actor_scheduler_overflow_thread_spawns`.
- `actor_registry_lookup/register/unregister/has`.
- `actor_monitor`, `actor_link`, supervision counters, restart/escalation counters.

## Ownership: Collapse, Observe, Decay

Ownership constructs are the authored shape of aliasing and lifetime.

```kn
fn fold_cells(cells: ptr<Int>, count: Int) -> Int:
    var i: Int = 0
    var acc: Int = 0
    while i < count:
        acc = (acc + mem_load(ptr_offset(cells, i, "Int"), "Int")) % 1000000007
        i = i + 1
    return acc

fn ownership_lane(count: Int) -> Int with Unsafe:
    let mut cells: ptr<Int> = alloc_zeroed(count, "Int")

    collapse cells:
        var i: Int = 0
        while i < count:
            mem_store(ptr_offset(cells, i, "Int"), (i * 17) + 3, "Int")
            i = i + 1
        0

    let observed: Int = observe cells:
        fold_cells(cells, count)

    decay cells
    return observed
```

Ownership rules:

- `collapse target:` means exclusive scoped mutation.
- `observe target:` means read-only scoped observation; nested observations are modeled by observer count.
- `decay target` is terminal teardown; do it once after all live observations/collapses end.
- The portable model has states `Idle`, `Observed(n)`, `Collapsed`, and `Decayed`.
- Collapse is only legal from idle. Observe is legal from idle or observed. Decay is only legal from idle.
- World state, entangled authority/mirror, heap allocations, local allocas, RC objects, and imported pointers can have different ownership region policies.
- If a demo only allocates then immediately decays without a real exclusive/observe region, it is not proving systems ownership.

Ownership source anchors:

- `crates/kain-ownership/src/lib.rs`: `OwnershipState`, `OwnershipTransition`, region kinds, transition errors.
- `ast.rs`: `Expr::Collapse`, `Expr::Observe`, `Expr::Decay`.
- `types.rs`: ownership expression traversal, early-exit rejection, target type checks.
- `runtime/native/tests/test_ownership_memory.c`: native guard behavior.
- `runtime/native/include/ownership.h`, `runtime/native/src/core/*ownership*`.
- `crates/kain-ownership/z3/proofs`, `runtime/native/src/core/z3/proofs/native-ownership-*.yaml`.

## Raw Memory And Pointer Lanes

Raw memory is allowed, but it should look intentionally dangerous and be bounded by shape.

```kn
fn pack_header(seq: Int, kind: Int, flags: Int, version: Int) -> Int with Unsafe:
    let seq_lane: Int = (seq & 1048575) << 12
    let kind_lane: Int = (kind & 15) << 8
    let flag_lane: Int = (flags & 15) << 4
    let version_lane: Int = version & 15
    return seq_lane | kind_lane | flag_lane | version_lane

fn branchless_select(mask: Int, hot_value: Int, cold_value: Int) -> Int with Unsafe:
    let all_bits: Int = 0 - (mask & 1)
    return (hot_value & all_bits) | (cold_value & (all_bits ^ -1))

fn store_packet(buffer: ptr<Int>, packet: Int, salt: Int) -> Int with Unsafe:
    let header = pack_header(packet, packet & 15, branchless_select(packet & 1, 9, 3), 1)
    let payload = ((header * 2246822519) ^ salt) & 4294967295
    let base: Int = packet * 4
    mem_store(ptr_offset(buffer, base + 0, "Int"), header, "Int")
    mem_store(ptr_offset(buffer, base + 1, "Int"), payload & 4095, "Int")
    mem_store(ptr_offset(buffer, base + 2, "Int"), (payload >> 16) & 65535, "Int")
    mem_store(ptr_offset(buffer, base + 3, "Int"), (header + payload + salt) % 1000003, "Int")
    return payload
```

Raw memory rules:

- Use `ptr<T>` for explicit pointer-bearing data.
- Allocate with `alloc`, `alloc_zeroed`, or stdlib allocator helpers when you need owned memory.
- Use `realloc_mem(ptr, count, "Type", zeroed_new)` when growing in authored Kain if examples in this checkout use that helper.
- Always carry element type strings in pointer helpers: `ptr_offset(cells, i, "Int")`, `mem_load(ptr, "Int")`, `mem_store(ptr, value, "Int")`.
- Prefer one allocation shape per hot lane: SoA pointer arrays, packed `Int` cells, or `shatter struct`, not accidental mixed abstractions.
- Avoid pointer arithmetic with hidden units. Name constants like `WORDS_PER_PACKET`, `CACHE_LINE_WORDS`, `CELL_COUNT`.
- Do not silently ignore bounds. If the lane depends on `base + width <= count`, prove it or keep the loop shape obviously bounded.

Raw memory source anchors:

- `ast.rs`: `Type::Ptr`, `PointerProvenance`, `Expr::PtrOffset`, `Expr::MemLoad`, `Expr::MemStore`, `Expr::Alloc`, `Expr::Realloc`.
- `crates/kain-core/src/low_level_memory.rs`: AST-to-low-level helper handling and memory metadata.
- `types.rs infer_expr_type`: pointer/memload/memstore type behavior.
- `codegen_llvm/mod.rs`: pointer collect/forwarding, memory load/store, ownership provenance, ephemeral local/shatter handling.
- `runtime/native/include/memory.h`, `runtime/native/src/core/*memory*`.
- Z3 helpers: use `ptr_offset_ok`, `size_add_ok`, `size_mul_ok`, `buffer_growth_ok`, `range_check` when editing underlying rules.

## Shatter, Cache Geometry, And Layout Intent

Use `shatter struct` when the system wants a structure-of-arrays layout or lane-wise hot data.

```kn
const WORDS_PER_PACKET: Int = 4
const CACHE_LINE_WORDS: Int = 8

shatter struct PacketLane:
    bias: Int
    phase: Int
    salt: Int
    hot: Bool

fn tile_base(packet: Int) -> Int with Unsafe:
    let cache_line: Int = packet / CACHE_LINE_WORDS
    let lane: Int = packet % CACHE_LINE_WORDS
    return (cache_line * CACHE_LINE_WORDS) + lane

fn packet_word_base(packet: Int) -> Int with Unsafe:
    return tile_base(packet) * WORDS_PER_PACKET
```

Layout rules:

- `shatter struct` is authored layout intent, not just a normal struct with a cooler name.
- Use it when field lanes are hot independently, when arrays should behave like SoA, or when teleport/pulse/actors shuttle compact lane descriptors.
- For raw buffers, explicitly define element counts, words per packet, cache-line words, packet count, and field packing.
- For shatter arrays/literals, LLVM has special stack/closed-field-projection paths and native `kain_machine_shatter_alloc` paths.
- If shatter field pointer math or lane offsets change, prove lane count/element bounds with Z3.

Layout source anchors:

- `ast.rs SHATTER_ATTRIBUTE_NAME`, `Struct::is_shattered`.
- `parser.rs parse_shatter_struct`.
- `runtime_contract.rs RuntimeShatterContract`, capability `memory.shatter`.
- `codegen_llvm/mod.rs`: `compile_shattered_array_literal`, `compile_shattered_field_ptr`, stack shatter candidate analysis.
- `runtime/native/include/machine_stones.h`, `runtime/native/src/core/machine_stones.c`.
- Z3: `crates/kain-core/z3/proofs/keywords-shatter-field-lane-offset-stays-in-bounds.yaml`, native machine proofs.

## Async And Future Systems

Use async where waiting is part of the system shape, not as decorative syntax.

```kn
fn ready_value(seed: Int) -> impl Future<Int>:
    return async seed + 29

async fn fold_ready(seed: Int) -> Int:
    let value = await ready_value(seed)
    return value + 1
```

Async rules:

- `async fn` adds the `Async` effect automatically.
- `async expr`/`async: block` creates a future-shaped value.
- `await expr` returns the future payload type.
- Actor `ask` is conceptually a waiting systems boundary; native LLVM uses reply ports and timeout helpers.
- For timer/pulse-driven systems, combine with `pulse` under `lang-semantics` and prove runtime shape with `runtime_machine_pulse_total_fire_count()`.

Source anchors:

- `parser.rs parse_async_function`, `Expr::Await`, `Expr::AsyncBlock`.
- `types.rs infer_expr_type` for futures/await.
- `runtime.rs` future helpers and actor ask/ask_timeout.
- `runtime/native/src/core/stdlib_abi.c` future support.

## Systems Fusion Pattern

A high-value systems Kain lane usually combines at least three of these:

- Actor pressure: `spawn`, `ask`, `send`, worker pools, reply ports, telemetry.
- Ownership pressure: `collapse`, `observe`, `decay`.
- Raw memory: `ptr<T>`, allocation, pointer offsets, loads/stores.
- Layout intent: `shatter struct`, packed words, cache-line constants.
- Semantic state: `world`, `entangle`, `patch`, `law`.
- Machine time/handoff: `pulse`, `teleport`.
- Performance proof: benchmark case with expected checksum and runtime shape checks.

Canonical shape:

```kn
fn systems_hot_loop(cells: ptr<Int>, relay: FoldRelay, rounds: Int) -> Int:
    var checksum: Int = 0
    collapse cells:
        var i: Int = 0
        while i < rounds:
            let slot: Int = i % 64
            let old_cell: Int = mem_load(ptr_offset(cells, slot, "Int"), "Int")
            let reply: Int = ask(relay, "Fold", old_cell + i + checksum)
            let next_cell: Int = (reply + old_cell + slot) % 1000000007
            mem_store(ptr_offset(cells, slot, "Int"), next_cell, "Int")
            checksum = (checksum + next_cell) % 1000000007
            i = i + 1
        0
    return checksum
```

Turn that into a proof lane by adding:

- `runtime_init()` / `runtime_shutdown()` when native runtime helpers are involved.
- Warmup `ask` calls before timing pressure.
- Expected checksum to reject silent behavior drift.
- Runtime shape checks: actor ABI, scheduler counters, patch/entangle/teleport/pulse/converge telemetry if used.
- Benchmark case under `benchmark/cases/<name>/main.kn` if performance is the claim.
- Attrition case if teardown/scheduler/heap long-run health is the claim.

## Validation Ladders

For loose authored Kain:

```powershell
kain check <entry.kn> --target llvm
kain run <entry.kn> --target llvm
```

For examples:

```powershell
rg -n "actor|collapse|observe|decay|mem_load|mem_store|ptr_offset" library_of_kain benchmark/cases
```

For benchmark proof:

```powershell
python benchmark/run.py --case actor_mailbox_erlang,ownership_memory,semantic_singularity_crucible,pulse_teleport_decay_mesh --languages kain --runs 1 --warmups 0 --timeout 900
```

For runtime cleanliness:

```powershell
python attrition/run.py --help
rg -n "actor|ownership|mailbox|decay|teleport|pulse" attrition runtime/native
```

For Z3:

- Use Z3 when changing underlying pointer arithmetic, allocation growth, content-length/buffer rules, actor state-machine invariants, mailbox capacity, ownership transitions, ABI layout, shatter offsets, teleport handoff, or branchless bit tricks.
- Good proof primitives include `ptr_offset_ok`, `size_add_ok`, `size_mul_ok`, `buffer_growth_ok`, `range_check`, `state_machine_check`, and `bitvec_equiv`.
- Existing proof packs are evidence. Do not claim a new unsafe invariant because a nearby test passed.

## Handoff Boundaries

- Use `lang-semantics` when the systems lane depends heavily on world/entangle/patch/law/converge/orchestrate/axiom/pulse/teleport/shatter meaning.
- Use `bootstrap-core` when parser, AST, typechecker, effect rules, ownership syntax, actor syntax, or generic lowering must change.
- Use `bootstrap-actors` when compiler/frontend actor truth or actor reflection/lowering contracts change.
- Use `bootstrap-ownership` when `collapse`/`observe`/`decay` type rules, parser wiring, lowering metadata, or proof packs change.
- Use `runtime-core` when native scheduler, mailbox, reply port, ownership guard, heap, teleport, shatter, pulse, or native runtime core behavior changes.
- Use `test-bench` when performance claims are central.
- Use `test-attrition` when teardown, scheduler, mailbox, heap, or ownership long-run health is central.
- Use `test-crash-forensics` when native executable hangs/crashes or actor/memory behavior becomes frame-count-sensitive.
- Use `tool-z3-black-magic` for solver-discovered constants, branchless replacements, perfect masks, or bit tricks.

## Anti-Patterns

- Do not write actor-shaped workloads as shared mutable globals.
- Do not use `ask` with dynamic message-name strings in native LLVM lanes unless the lowering supports it.
- Do not put raw memory in a function that pretends to be `Pure`.
- Do not allocate raw memory and skip `decay` unless ownership intentionally transfers elsewhere.
- Do not hide pointer units. Say whether an offset is elements, bytes, words, packets, cache lines, or lanes.
- Do not replace `collapse` with comments about exclusivity.
- Do not replace `observe` with casual reads when read-only scope is the point.
- Do not use `Unsafe` as a trash can. Use it to mark a deliberate proof/ABI/pointer boundary.
- Do not leave systems demos without expected checksums or runtime shape checks.
- Do not fix authored systems failures by deleting Kain semantics. Route engine bugs to the owning bootstrap/runtime skill.
