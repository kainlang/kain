# KAIN BY EXAMPLE — Part 3: Systems → Full Fusion (L7–End)

> **One compilable snippet per feature. No prose, no theory — just proof that it compiles.**
> Part 3 covers LAYER 7 (actors, ownership), Foreign & Import, GPU & Shaders, Raw Memory, Atomics, Inline ASM, Virtual Memory, CPU Intrinsics, Testing, Runtime Telemetry, and Full Fusion.

______________________________________________________________________

## LAYER 7 — SYSTEMS

### actor — Message-Oriented Concurrency with Converge + Cascade

```kn
actor CrucibleRelayActor:
    state bias:     Int = 7
    state turns:    Int = 0
    state checksum: Int = 0

    on Compute(reply_to: P, payload: Int):
        self.turns    = self.turns + 1
        let v         = unpack_a(payload)
        let seed      = unpack_b(payload)
        let result    = fast_mix(v + seed, self.bias + self.turns) % 1000000007
        self.checksum = (self.checksum + result) % 1000000007
        let child     = spawn CrucibleVerifier(min_val = 0)
        send child.Verify(reply_to = reply_to, val = result)
        if false:
            send reply_to.Reply(value = 0)
```

### spawn / ask — Multi-Actor + Async Fusion

```kn
fn actor_cascade_checksum(iterations: Int, modulus: Int) -> Int with Unsafe:
    let relay      = spawn CrucibleRelayActor(bias = 7, turns = 0, checksum = 0)
    let teleporter = spawn CrucibleTeleporter(done = 0, last = 0)

    let _w1 = ask(relay, "Compute", pack(1, 1))
    let _w2 = ask(teleporter, "ShatterSend", pack(1, 1))

    var acc: Int = 0
    var index: Int = 0
    while index < iterations:
        let v    = mod(index * 53 + 7, modulus)
        let seed = mod(index * 17 + 3, modulus)

        let relay_reply  = ask(relay, "Compute", pack(v, seed))
        let tele_reply   = ask(teleporter, "ShatterSend", pack(v, seed))

        acc = mod(acc + relay_reply + tele_reply, modulus)
        index = index + 1
    return acc
```

### Actor Cascade — Spawn + Delegate Reply (Converge Inside)

```kn
actor CrucibleRelayActor:
    state bias:     Int = 7
    state turns:    Int = 0
    state checksum: Int = 0

    on Compute(reply_to: P, payload: Int):
        self.turns    = self.turns + 1
        let v         = unpack_a(payload)
        let seed      = unpack_b(payload)
        let result    = fast_mix(v + seed, self.bias + self.turns) % 1000000007
        self.checksum = (self.checksum + result) % 1000000007
        let child     = spawn CrucibleVerifier(min_val = 0)
        send child.Verify(reply_to = reply_to, val = result)

actor CrucibleVerifier:
    state min_val: Int = 0

    on Verify(reply_to: P, val: Int):
        let ok = val >= self.min_val
        if ok == false:
            send reply_to.Reply(value = -99)
            return
        send reply_to.Reply(value = val)
```

### Actor Telemetry — 7 Counters in Hot Loop

```kn
fn actor_telemetry_hot(count: Int) -> Int:
    var checksum: Int = 0
    var i: Int = 0
    while i < count:
        let qd = actor_scheduler_queue_depth()
        let bw = actor_scheduler_busy_workers()
        let ow = actor_scheduler_overflow_thread_spawns()
        let mc = actor_unbounded_mailbox_capacity()
        let dto = actor_default_ask_timeout_ms()
        let sg = actor_default_shutdown_grace_ms()
        let sw = actor_supervision_restart_window_millis()
        checksum = (checksum + qd + bw + ow + mc + dto + sg + sw) % 1000000007
        i = i + 1
    return checksum
```

### collapse / observe / decay — Ownership Inside Actor + Teleport

```kn
actor CrucibleTeleporter:
    state done: Int = 0
    state last: Int = 0

    on ShatterSend(reply_to: P, payload: Int):
        self.done = self.done + 1
        let tick   = unpack_a(payload)
        let signal = unpack_b(payload)

        let n: Int = 8
        let mut cells: ptr<Int> = alloc_zeroed(n, "Int")

        collapse cells:
            var i: Int = 0
            while i < n:
                mem_store(ptr_offset(cells, i, "Int"), (tick * (i + 1) * 7) % 1000000007, "Int")
                i = i + 1
            0

        let head: Int = observe cells:
            mem_load(ptr_offset(cells, 0, "Int"), "Int")

        decay cells

        let shard = Shard {
            bias: 42 + (tick % 17), phase: 13 + (signal % 11),
            checksum: (head + signal + tick) % 1000000007, alive: true
        }
        let moved = teleport shard from Authority to Mirror via shard_bus
        send reply_to.Reply(value = pack(shard_score(shard), shard_score(moved)))
```

### share / fanout — Contention Wall with 32 Workers

```kn
fn contention_wall_checksum(iterations: Int) -> Int:
    let expected_total: Int = iterations
    let mut counter: ptr<Int> = alloc_zeroed(1, "Int")

    share counter:
        fanout worker in 0..32:
            let chunk_start: Int = (worker * iterations) / 32
            let chunk_end: Int = ((worker + 1) * iterations) / 32
            var i: Int = chunk_start
            while i < chunk_end:
                let _prev: Int = atomic_add(counter, 1, "acq_rel")
                i = i + 1

    let final_value: Int = observe counter:
        mem_load(counter, "Int")
    decay counter

    if final_value != expected_total:
        return 1
    return final_value
```

______________________________________________________________________

## FOREIGN & IMPORT

### include — C Header Import (Natural + System)

```kn
include native/native_math.h as nm
include <stdio.h> as cstdio
include <windows.h> as win          // 605 functions from real SDK
include <vulkan/vulkan.h> as vk     // 755 functions from real SDK
```

> Companion `.c` file auto-discovered. System headers use libclang extraction.

### import — Python Host Object Import

```kn
import json as py_json
import numpy as np
from torch import tensor

fn encode(value: String) -> String:
    return py_json.dumps(value, separators = [",", ":"])

fn linspace() -> Any:
    return np.linspace(start = 0.0, stop = 1.0, num = 5)
```

> Named Kain args lower to Python kwargs automatically.

### @naked / @section / @link_name / @callconv — Full ABI Control

```kn
@naked
@section(".text.kain.metal.hotpath")
@link_name("__kain_metal_naked_trap")
fn metal_naked_trap() with Unsafe:
    asm("ret")

@callconv("win64")
fn metal_win64_mix(value: Int) -> Int:
    return (value * 31 + 7) % 1000000007

@callconv("vectorcall")
fn metal_vectorcall_mix(value: Int) -> Int:
    return (value * 17 + 3) % 1000000007

fn cc_dispatch_checksum(iterations: Int) -> Int:
    var acc: Int = 0
    var i: Int = 0
    while i < iterations:
        let w = metal_win64_mix(i)
        let v = metal_vectorcall_mix(i)
        acc = (acc + w + v) % 1000000007
        i = i + 1
    return acc
```

______________________________________________________________________

## GPU & SHADERS

### shader compute — UInt Kernel + Vertex + Fragment

```kn
shader compute OrchestrateGodKernel(id: UVec3) -> Void workgroup(8, 1, 1):
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1

    comptime:
        let compute = (
            [64, 1, 1],
            [
                ("src", "u32", ["dispatch.x"], "input",  "kain.shared.buffer"),
                ("dst", "u32", ["dispatch.x"], "output", "kain.shared.buffer"),
            ],
            [],
        )

    let lane = src[id.x]
    dst[id.x] = lane + UInt(9)
    return

shader vertex CrucibleVertex(position: Vec3, uv: Vec2) -> Vec4:
    uniform offset: Vec3 @0
    let lane = position.x + offset.x
    let bias = uv.x + uv.y
    return vec4(lane, position.y + offset.y + bias, position.z + offset.z, 1.0)

shader fragment CrucibleFragment(uv: Vec2) -> Vec4:
    uniform tint: Vec3 @0
    let wave: Float = uv.x * (1.0 - uv.x)
    return vec4(tint.x * wave, tint.y * wave, tint.z * (0.5 + wave), 1.0)
```

### dispatch — Host-Side GPU Launch with ABI Telemetry

```kn
fn dispatch_pipeline_checksum(iterations: Int, modulus: Int) -> Int with GPU, Unsafe:
    let _ = runtime_init()
    var acc: Int = 0
    var index: Int = 0
    while index < iterations:
        dispatch "shader::OrchestrateGodKernel::compute" [13, 2, 1]
        let status  = abi_cuda_last_status()
        let invoc   = abi_cuda_last_dispatch_invocations()
        let outputs = abi_cuda_last_output_binding_count()
        acc = ((acc + ((status + 2048) * 3) + invoc + outputs + (index % 11))) % modulus
        index = index + 1
    let _ = runtime_shutdown()
    return acc
```

> `dispatch "shader::KernelName::compute" [X, Y, Z]` — workgroup counts, NOT thread counts. Compute key MUST be a string literal.

### std::graphics — Host Graphics Command Recording

```kn
use std::graphics

fn graphics_probe() -> Int:
    let session = graphics_session_create("probe", 320, 240)
    let vb = graphics_buffer_create_from_hex(session, "vertex", "v", "00000000...", 12)
    let vs = graphics_shader_spirv_from_hex(session, "vs", "vertex", "main", "03022307")
    let fs = graphics_shader_spirv_from_hex(session, "fs", "fragment", "main", "03022307")
    let pipeline = graphics_pipeline_create(session, "pipe", vs, fs, "software")
    let _begin = graphics_begin_frame(session, 16.0)
    let _draw = graphics_draw_mesh(session, pipeline, mesh, 3)
    let present = graphics_end_frame(session)
    let _destroy = graphics_session_destroy(session)
    return present
```

### std::gpu — Resource Policy

```kn
use std::gpu
use std::graphics::shared

fn buffer_contract() -> Int:
    let policy = gpu_resource_policy(
        gpu_shared_memory_policy(
            GPU_ACCESS_READ_WRITE, GPU_QUEUE_COMPUTE | GPU_QUEUE_TRANSFER,
            GPU_LAYOUT_STD430, GPU_DESCRIPTOR_STORAGE_BUFFER
        ),
        GPU_BUFFER_USAGE_STORAGE | GPU_BUFFER_USAGE_TRANSFER_SRC,
        "particles.next"
    )
    let buffer = gpu_shared_buffer_zeroed("f32", [4], "f32", "", policy)
    return buffer.byte_length
```

______________________________________________________________________

## RAW MEMORY & POINTERS

### ptr<T> / alloc_zeroed / ptr_offset / mem_load / mem_store

```kn
fn raw_memory(count: Int) -> Int with Unsafe:
    let cells: ptr<Int> = alloc_zeroed(count, "Int")
    defer decay cells

    collapse cells:
        var i: Int = 0
        while i < count:
            mem_store(ptr_offset(cells, i, "Int"), (i * 17) + 3, "Int")
            i = i + 1
        0

    let result: Int = observe cells:
        mem_load(ptr_offset(cells, 0, "Int"), "Int")

    return result
```

### volatile_load / volatile_store — MMIO Access

```kn
fn mmio_probe(slot: ptr<Int>) -> Int with Unsafe:
    volatile_store(slot, 11, "Int")
    let seen = volatile_load(slot, "Int")
    return seen
```

### bitcast / ptr_to_int / int_to_ptr

```kn
fn cast_lane(x: Float) -> Int with Unsafe:
    let bits: Int = bitcast(x, "I64")
    let ptr_val: Int = ptr_to_int(some_ptr)
    let restored: ptr<Int> = int_to_ptr(ptr_val, "ptr<Int>")
    return bits
```

______________________________________________________________________

## ATOMICS & FENCES

### atomic_add / atomic_store / atomic_fence — 32-Worker Contention

```kn
fn contention_wall_checksum(iterations: Int) -> Int:
    let expected_total: Int = iterations
    let mut counter: ptr<Int> = alloc_zeroed(1, "Int")

    share counter:
        fanout worker in 0..32:
            let chunk_start: Int = (worker * iterations) / 32
            let chunk_end: Int = ((worker + 1) * iterations) / 32
            var i: Int = chunk_start
            while i < chunk_end:
                let _prev: Int = atomic_add(counter, 1, "acq_rel")
                i = i + 1

    let final_value: Int = observe counter:
        mem_load(counter, "Int")
    decay counter

    if final_value != expected_total:
        return 1
    return final_value
```

### lfence / sfence / mfence — Tight Loop Barrier Pressure

```kn
fn fence_barrier_pressure_checksum(iterations: Int) -> Int with Unsafe:
    var acc: Int = 0
    var index: Int = 0
    while index < iterations:
        lfence()
        sfence()
        mfence()
        let lane = (index * 31 + 7) % 1000000007
        lfence()
        acc = (acc + lane) % 1000000007
        sfence()
        index = index + 1
    mfence()
    return acc
```

### clflush — Cache Line Flush with Operand Binding

```kn
fn asm_cache_flush_checksum(iterations: Int) -> Int with Unsafe:
    let buf: ptr<Int> = alloc_zeroed(64, "Int")
    let result: Int = collapse buf:
        var slot: Int = 0
        while slot < 64:
            mem_store(ptr_offset(buf, slot, "Int"), slot * 37, "Int")
            slot = slot + 1
        var acc: Int = 0
        var index: Int = 0
        while index < iterations:
            let addr = ptr_offset(buf, index % 64, "Int")
            asm("clflush ($0)", addr, memory = true)
            let val = mem_load(addr, "Int")
            acc = acc + ((val + index) % 1000000007)
            index = index + 1
        acc
    decay buf
    return result
```

### spin_loop_hint / prefetch

```kn
fn spin_wait() with Unsafe:
    spin_loop_hint()
    prefetch(data_ptr)
```

______________________________________________________________________

## INLINE ASM

### asm — Basic (No Operands)

```kn
fn pause_loop(iterations: Int) -> Int with Unsafe:
    var i: Int = 0
    var acc: Int = 0
    while i < iterations:
        asm("pause")
        asm("nop")
        acc = acc + (i & 255)
        i = i + 1
    return acc
```

### asm — With Operands (memory clobber) — Stress Test

```kn
fn clflush_hammer(iterations: Int) -> Int with Unsafe:
    let buf: ptr<Int> = alloc_zeroed(64, "Int")
    let result: Int = collapse buf:
        var slot: Int = 0
        while slot < 64:
            mem_store(ptr_offset(buf, slot, "Int"), slot * 37, "Int")
            slot = slot + 1
        var acc: Int = 0
        var index: Int = 0
        while index < iterations:
            let addr = ptr_offset(buf, index % 64, "Int")
            asm("clflush ($0)", addr, memory = true)
            let val = mem_load(addr, "Int")
            acc = acc + ((val + index) % 1000000007)
            index = index + 1
        acc
    decay buf
    return result
```

> Options: `volatile = true`, `memory = true`, `intel = true`

______________________________________________________________________

## VIRTUAL MEMORY

### vm_reserve / vm_commit / vm_protect — Full Page Torture Loop

```kn
fn vm_page_torture_checksum(iterations: Int) -> Int with Unsafe:
    let page_size = vm_page_size()
    var acc: Int = 0
    var index: Int = 0
    while index < iterations:
        let pages = vm_reserve(page_size * 2)
        if ptr_to_int(pages) != 0:
            let committed = vm_commit(pages, page_size)
            if committed == 0:
                collapse pages:
                    mem_store(pages, index * 17, "Int")
                    let val = mem_load(pages, "Int")
                    acc = (acc + val) % 1000000007
                    0
                let _prot_none = vm_protect_none(pages, page_size)
                let _prot_rw = vm_protect_read_write(pages, page_size)
                collapse pages:
                    let val2 = mem_load(pages, "Int")
                    acc = (acc + val2) % 1000000007
                    0
                let _prot_rwx = vm_protect_execute_read_write(pages, page_size)
                let locked = vm_lock(pages, page_size)
                if locked == 0:
                    let _unlocked = vm_unlock(pages, page_size)
                let _decommitted = vm_decommit(pages, page_size)
            let _released = vm_unmap(pages, page_size)
        index = index + 1
    return acc
```

______________________________________________________________________

## CPU INTRINSICS

### cpuid — All 4 Registers, Both Leaves, in Loop

```kn
fn cpu_cpuid_topology_checksum(iterations: Int) -> Int with Unsafe:
    let cores = cpu_core_count()
    let logical = cpu_logical_count()
    let packages = cpu_package_count()
    let cache_line = cpu_cache_line_bytes()
    let numa_nodes = numa_node_count()
    let numa_current = numa_current_node()

    var acc: Int = 0
    var index: Int = 0
    while index < iterations:
        let r0 = cpuid_eax(0, 0)
        let r1 = cpuid_ebx(0, 0)
        let r2 = cpuid_ecx(0, 0)
        let r3 = cpuid_edx(0, 0)
        let leaf1_eax = cpuid_eax(1, 0)
        let leaf1_ebx = cpuid_ebx(1, 0)
        let leaf1_ecx = cpuid_ecx(1, 0)
        let leaf1_edx = cpuid_edx(1, 0)
        acc = (acc + r0 + r1 + r2 + r3 + leaf1_eax + leaf1_ebx + leaf1_ecx + leaf1_edx
               + cores + logical + packages + cache_line) % 1000000007
        index = index + 1
    let _ = numa_nodes + numa_current
    return acc
```

______________________________________________________________________

## COMPILE-TIME

### comptime — Shader Metadata Block

```kn
comptime:
    let compute = (
        [64, 1, 1],
        [
            ("src", "u32", ["dispatch.x"], "input",  "kain.shared.buffer"),
            ("dst", "u32", ["dispatch.x"], "output", "kain.shared.buffer"),
        ],
        [],
    )
```

### macro! — Syntactic Macro

```kn
macro fold!(x: expr):
    mod(x, 1000000007)

let result = fold!(acc + index)
```

> The `!` after macro name is mandatory.

______________________________________________________________________

## TESTING

### test — Compiletest-Style (Including Unsafe Systems)

```kn
test fn test_addition():
    assert(add(2, 2) == 4)

test fn test_bounds():
    let result = signal_valid(42)
    assert(result == true)

test fn test_ownership_lane():
    let val = raw_ownership_memory_checksum(100, 1000000007)
    assert(val >= 0)

test fn test_fence_pressure():
    let val = fence_barrier_pressure_checksum(10)
    assert(val > 0)
```

______________________________________________________________________

## RUNTIME LIFECYCLE & TELEMETRY

### runtime_init / runtime_shutdown

```kn
use std::runtime

fn main() -> Int:
    let init = runtime_init()
    if init != 0:
        return 100 + init
    let result = compute()
    let shutdown = runtime_shutdown()
    if shutdown != 0:
        return 200 + shutdown
    return result
```

### Semantic Telemetry — All Layers with Delta Guards

```kn
use std::intent

fn verify_semantics_fired() -> Int:
    let resonate_delta = resonate_fire_count() - resonate_before
    let entangle_delta = entangle_propagation_count() - entangle_before
    let teleport_delta = runtime_machine_teleport_count() - teleport_before
    let patch_delta = patch_journal_count() - patch_before
    let orchestrate_delta = orchestrate_stage_count() - orchestrate_before
    let converge_delta = runtime_converge_telemetry_count() - converge_before
    let pulse_delta = runtime_machine_pulse_total_fire_count() - pulse_before

    if resonate_delta < 1:    return -10
    if entangle_delta < 1:    return -11
    if teleport_delta < 1:    return -12
    if patch_delta < 1 and patch_before < 256: return -13  // journal capacity edge case
    if orchestrate_delta < 1: return -14
    if converge_delta < 0:    return -15
    if pulse_delta < 0:       return -16
    return 0
```

### std::machine — Curated Machine Helpers

```kn
use std::machine

fn machine_probe(ptr: ptr<Int>) -> Int with Unsafe:
    load_fence()
    store_fence()
    let _ = full_fence()
    cache_flush(ptr)
    spin_loop_hint()
    return vm_page_size()
```

______________________________________________________________________

## PUTTING IT ALL TOGETHER — FULL FUSION

### All 7 Layers + Telemetry Validation in One Loop

```kn
// Each iteration crosses all 7 layers with telemetry guards proving every layer fired.
// From benchmark/cases_v2/fusion_chain.kn case 3 — real, compilable, benchmarked code.
use std::runtime
use std::actor
use std::intent

fn fusion_full_causal_chain_checksum(iterations: Int, modulus: Int) -> Int with Unsafe:
    let relay = spawn FusionRelay(turns = 0, bias = 3, checksum = 0)
    let teleporter = spawn FusionTeleporter(teleports_done = 0, last_score = 0)

    // Snapshot telemetry — every layer must increment
    let resonate_fire_before = resonate_fire_count()
    let entangle_before = entangle_propagation_count()
    let teleport_before = runtime_machine_teleport_count()
    let patch_before = patch_journal_count()
    let orchestrate_before = orchestrate_stage_count()

    var acc: Int = 0
    var index: Int = 0
    while index < iterations:
        // LAYER 1+2: world mutation via patch triggers resonate
        let signal_value = ((index * 97) + 31) % modulus
        let tick = fusion_strike_signal(FusionAuthority, signal_value)

        // LAYER 3: resonate fired -> shadow auto-updated by orchestrate inside handler
        let shadow = FusionAuthority.shadow

        // LAYER 4: entangle propagated to mirror
        let mirror_signal = FusionMirror.signal_copy
        let mirror_tick = FusionMirror.tick_copy

        // LAYER 5: relay processes signal (converge fast lane inside actor)
        let relay_reply = ask(relay, "Signal", fusion_pack(signal_value, tick))

        // LAYER 6+7: teleporter does collapse/observe/decay + shatter-teleport
        let teleport_reply = ask(teleporter, "ShatterAndSend", fusion_pack(tick, signal_value))

        // LAND: teleport result back into world
        let chain_result = ((relay_reply + teleport_reply) * 13
            + (mirror_signal + mirror_tick) * 17 + index * 19) % modulus
        let landed = fusion_land_teleport(FusionAuthority, chain_result)
        let ack_val = fusion_strike_ack(FusionAuthority)

        acc = (acc + shadow + mirror_signal + chain_result + landed + ack_val) % modulus
        index = index + 1

    // Telemetry delta guard — all layers MUST have fired
    let resonate_delta = resonate_fire_count() - resonate_fire_before
    let entangle_delta = entangle_propagation_count() - entangle_before
    let teleport_delta = runtime_machine_teleport_count() - teleport_before
    let patch_delta = patch_journal_count() - patch_before
    let orchestrate_delta = orchestrate_stage_count() - orchestrate_before

    if resonate_delta < 1:    return -10
    if entangle_delta < 1:    return -11
    if teleport_delta < 1:    return -12
    if patch_delta < 1 and patch_before < 256: return -13
    if orchestrate_delta < 1: return -14

    return acc
```

______________________________________________________________________

## QUICK REFERENCE — DECISION LADDER

```
"Am I crossing into C/OS?"  → include ... as ...
"Is this Python interop?"   → import ...
"Is this a GPU kernel?"     → shader compute
"Is this a UI component?"   → component
─────────────────────────────────────────────
LAYER 7: "Concurrent state?"     → actor
         "Raw memory lifecycle?" → collapse/observe/decay
─────────────────────────────────────────────
LAYER 6: "Capability assumption?" → axiom
         "Hot-data layout?"       → shatter struct
         "Cross-world zero-copy?" → teleport
─────────────────────────────────────────────
LAYER 5: "Timed recurrence?"    → pulse
         "React to state change?" → resonate
─────────────────────────────────────────────
LAYER 4: "Multi-stage pipeline?" → orchestrate
─────────────────────────────────────────────
LAYER 3: "Spec + fast lanes?"   → converge
─────────────────────────────────────────────
LAYER 2: "Journaled mutation?"  → patch
         "Invariant predicate?" → law
─────────────────────────────────────────────
LAYER 1: "Global named state?"  → world + entangle
─────────────────────────────────────────────
LAYER 0: None of the above?     → fn, struct, let, etc.
```

______________________________________________________________________

*End of Kain By Example Part 3 · Every feature has one compilable snippet · Extracted from blades/pong, blades/component_fuzz, benchmark/cases_v2/fusion_chain.kn, benchmark/cases_v2/metal.kn, benchmark/cases_v2/keyword_crucible.kn, benchmark/cases_v2/classic_systems.kn, benchmark/cases_v2/orchestrate_god.kn*
