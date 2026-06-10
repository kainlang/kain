# KAIN BY EXAMPLE — The Ultimate Language Reference

> **One compilable snippet per feature. No prose, no theory — just proof that it compiles.**
> Every example is extracted from the canonical codebase (benchmark/cases_v2/, smoketest/, blades/,
> stdlib/).
> Use this to see "how do I write X" in 5 seconds. Copy the pattern. Move on.

______________________________________________________________________

## LAYER 0 — PLAIN CODE

### fn — Function Declaration

```kn
fn add(a: Int, b: Int) -> Int with Pure:
    return a + b

fn generic_sum<T: Numeric>(a: T, b: T) -> T with Pure where T: Default:
    return a + b
```

### let / mut / var — Variable Binding

```kn
let x: Int = 42                 // immutable
let mut counter: Int = 0        // mutable
counter = counter + 1
var acc: Int = 0                // rebindable (older style)
```

### const — Compile-Time Constant

```kn
const MODULUS: Int = 1000000007
const CELL_COUNT: Int = 8
```

### if / elif / else — Conditional (Expression)

```kn
let mode = if val == 0: "zero" elif val == 1: "one" else: "many"

let score = if v < 0: 0 else: v
```

### match — Pattern Matching

```kn
enum Mode: Scalar Vectorized Parallel Hybrid

fn describe(m: Mode) -> String with Pure:
    match m:
        Mode::Scalar     => "scalar"
        Mode::Vectorized => "vectorized"
        Mode::Parallel   => "parallel"
        Mode::Hybrid     => "hybrid"
```

### for — Iteration

```kn
for lane in [1, 2, 3, 4]:
    acc = (acc + lane) % modulus

for i in 0..10:
    result = result * i
```

### while — Conditional Loop

```kn
var i: Int = 0
while i < iterations:
    acc = (acc + i) % modulus
    i = i + 1
```

### loop / break / continue — Loop with Break Value

```kn
let result: Int = loop:
    if done:
        break final_value
    step()

while i < iterations:
    if i % 5 == 0:
        i = i + 1
        continue
    process(i)
```

### return — Early Exit

```kn
fn clamp(v: Int, lo: Int, hi: Int) -> Int:
    if v < lo: return lo
    if v > hi: return hi
    return v
```

### defer — Block-Scoped LIFO Cleanup

```kn
fn process() -> Int:
    let buf: ptr<Int> = alloc_zeroed(N, "Int")
    defer decay buf
    collapse buf:
        mem_store(buf, 42, "Int")
        0
    let v = observe buf:
        mem_load(buf, "Int")
    return v  // decay buf runs here
```

### struct / impl — Product Type with Methods

```kn
struct Packet:
    id: Int
    payload: Int

impl Packet:
    fn score(_self: Self_) -> Int:
        return (_self.id * 13) + (_self.payload * 7)
```

### enum — Sum Type

```kn
enum Option<T>:
    Some(value: T)
    None

enum Result<T, E>:
    Ok(value: T)
    Err(error: E)
```

### trait — Interface

```kn
trait Metric:
    fn score(_self: Self_) -> Int:
        return 0  // default

impl Metric for Packet:
    fn score(_self: Self_) -> Int:
        return ((_self.id * 11) + _self.payload + 17) % 1000000007
```

### type — Type Alias

```kn
type Checksum = Int
type Distance = Float
```

### mod / pub / use — Modules and Visibility

```kn
pub mod util:
    pub fn clamp(v: Int, lo: Int, hi: Int) -> Int:
        if v < lo: return lo
        if v > hi: return hi
        return v

use std::runtime
use std::actor
use std::intent
```

### true / false / none — Literals

```kn
let flag: Bool = true
let done: Bool = false
let nullable: Option<Int> = none
```

### where — Generic Constraints

```kn
fn summary<T: Metric>(p: T, salt: Int) -> Int with Pure where T: Stable:
    let s = p.score()
    let b = p.bias()
    return (s * b + salt) % 1000000007
```

### as — Type Cast / Import Alias

```kn
let clamped: Int = val as Int
include <stdio.h> as libc
import json as py_json
```

______________________________________________________________________

## EFFECTS

### Pure — No Side Effects

```kn
fn pure_compute(v: Int) -> Int with Pure:
    return v * 2
```

### IO — Console / Filesystem / Network

```kn
fn log_signal(value: Int) with IO:
    println("signal=" + str(value))
```

### Async / async / await — Futures

```kn
async fn fetch(id: Int) -> Int with Async:
    return id * 2

fn resolve(v: Int) -> Int:
    let fut = fetch(v)
    return await fut
```

### GPU — GPU Dispatch Access

```kn
fn run_kernel(data: ptr<Float>) -> Int with GPU, Unsafe:
    dispatch "shader::Kernel::compute" [32, 32, 1]
    return 0
```

### Reactive — UI Event Handling

```kn
fn reactive_score(v: Int) -> Int with Reactive:
    return v * 37 + 11
```

### Unsafe — Raw Memory / ASM / ABI Escape Hatch

```kn
fn read_raw(cells: ptr<Int>) -> Int with Unsafe:
    return mem_load(ptr_offset(cells, 0, "Int"), "Int")
```

### and / or — Boolean Operators

```kn
if v >= 0 and v < MODULUS:
    return true

let flag: Bool = a == none or a == 0
```

______________________________________________________________________

## LAYER UI — COMPONENTS

### component — Full React-like UI Widget

```kn
component Counter(initial: Int, label: String):
    state count: Int = initial

    fn label_text(_self: Self_) -> String:
        return _self.label + ": " + str(_self.count)

    render <box>
        <text value={label_text()} />
    </box>
```

### JSX Composition — Components Calling Components

```kn
component Button(label: String, kind: String):
    render <box><text value={label} /></box>

component Toolbar():
    render <stack direction="horizontal">
        <Button label="Save" kind="primary" />
        <Button label="Load" kind="secondary" />
    </stack>
```

### JSX with for / if

```kn
component TodoList(items: [String], selected: Int):
    render <stack direction="vertical">
        for item in items:
            <text value={item} />
        if selected >= 0:
            <text value={"Selected: " + items[selected]} />
        else:
            <text value="Nothing selected" />
    </stack>
```

### world + surface — Wiring World to Component

```kn
component App():
    render <panel title="My App" />

world MyWorld:
    state signal: Int = 1
    surface native_ui => App
```

______________________________________________________________________

## LAYER 1 — STATE AUTHORITY

### world — Compiler-Owned State Container

```kn
world Authority:
    state signal: Int = 1
    state epoch:  Int = 0
    state shadow: Int = 0
    surface native_ui => AppView

world Mirror:
    state signal_copy: Int = 1
    state epoch_copy:  Int = 0
    surface web => AppView
```

### entangle — Compiler-Owned State Coupling

```kn
entangle Authority.signal <-> Mirror.signal_copy with single_writer
entangle Authority.epoch  <-> Mirror.epoch_copy  with single_writer
```

### surface targets — native_ui / web / viewport3d / ue5

```kn
world RenderWorld:
    state frame: Int = 0
    surface native_ui => RenderPanel

world InspectWorld:
    state frame_copy: Int = 0
    surface web => RenderPanel

world SceneWorld:
    state camera: Vec3 = vec3(0, 0, 5)
    surface viewport3d => SceneView

world UnrealWorld:
    state game_state: Int = 0
    surface ue5 => UnrealHUD
```

______________________________________________________________________

## LAYER 2 — STATE INTEGRITY

### patch — Journaled, Tracked World Mutation

```kn
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch  = authority.epoch + 1
    return authority.epoch
```

### law — Invariant Predicate (Returns Bool)

```kn
law signal_valid(v: Int) -> Bool:
    return v >= 0 and v < 1000000007

law epoch_valid(e: Int) -> Bool:
    return e >= 0

law in_range(s: Int, lo: Int, hi: Int) -> Bool:
    return s >= lo and s < hi
```

______________________________________________________________________

## LAYER 3 — DISPATCH

### converge — Spec + Platform-Specific Fast Lanes

```kn
fn mix_scalar(value: Int, seed: Int) -> Int:
    return ((value * 31 + seed) * 17 + 7) % 1000000007

fn mix_closed(value: Int, seed: Int) -> Int:
    return (((value + seed) * 48 + 14) % 1000000007)

converge fast_mix(value: Int, seed: Int) -> Int:
    spec reference:
        return mix_scalar(value, seed)
    fast closed_lane when target("llvm"):
        return mix_closed(value, seed)
    fast avx2_lane when capability("cpu.x86.avx2"):
        return mix_scalar(value, seed)
    verify random(8)
```

______________________________________________________________________

## LAYER 4 — STAGE GRAPH

### orchestrate — Multi-Runtime Typed Pipeline

```kn
orchestrate signal_pipeline(value: Int, epoch: Int) -> Int:
    stage host_base: cpu mix(value + epoch)
        when capability("cpu.scalar") residency host transfer none policy telemetry_prefer_cpu

    stage fast_lane: converge fast_mix(host_base, epoch)
        deps [host_base] residency host policy static

    stage law_check: law signal_valid(host_base)
        after fast_lane residency host policy static

    return fast_lane + law_check
```

### orchestrate with GPU / guarded / fallback

```kn
orchestrate gpu_pipeline(data: Int) -> Int:
    stage cpu_stage: cpu mix(data)
        residency host policy static

    stage gpu_stage: gpu fast_mix(cpu_stage, 7)
        after cpu_stage residency device transfer host_to_device
        guarded by silicon_truth fallback degrade fallback_fn

    return gpu_stage
```

______________________________________________________________________

## LAYER 5 — TEMPORAL

### pulse — Timed Recurrence with Jitter

```kn
pulse heartbeat every 16 ms jitter 2 ms:
    Authority.pulse_count = Authority.pulse_count + pulse_tick + 1

pulse fx_tick every 8ms jitter 1ms:
    let dt: Int = pulse_dt_ms
    FxWorld.phase = (FxWorld.phase + advance) % PHASE_MAX
```

> **Duration units:** `ns`, `us`, `ms`, `s`, `tick`, `ticks`
> **Body locals:** `pulse_tick`, `pulse_dt_ms`, `pulse_missed`

### resonate — Reactive State-Change Tripwire

```kn
resonate Authority.signal dampen 16 ms:
    let new_val: Int = resonate_new_i64
    Authority.shadow = (new_val * 53 + Authority.epoch) % 1000000007

resonate FxWorld.drive dampen 32ms:
    let new_drive: Int = resonate_new_i64
    FxWorld.output = if new_drive < 500: 500 else: 600
```

> **Handler locals:** `resonate_new_i64`, `resonate_old_i64`, `resonate_fired`
> **Anti-self-feedback:** cannot write to own trigger field

______________________________________________________________________

## LAYER 6 — MACHINE STONES

### axiom — Capability Assumption with Fallback

```kn
axiom machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "machine supports shatter + teleport + inline asm"
    fallback scalar_fallback
```

### shatter struct — Structure-of-Arrays Layout

```kn
shatter struct Particle:
    position_x: Float
    position_y: Float
    velocity_x: Float
    velocity_y: Float
    alive: Bool

fn use_shattered(particles: [Particle]) -> Int:
    let p = particles[0]
    return (p.position_x + p.velocity_x) as Int
```

### teleport — Zero-Copy Cross-World Handoff

```kn
shatter struct Shard:
    bias: Int
    phase: Int
    checksum: Int
    alive: Bool

let shard = Shard { bias: 42, phase: 13, checksum: 7, alive: true }
let moved = teleport shard from Authority to Mirror via shard_bus
```

______________________________________________________________________

## LAYER 7 — SYSTEMS

### actor — Message-Oriented Concurrency

```kn
actor FoldRelay:
    state bias: Int = 11
    state turns: Int = 0

    on Compute(reply_to: P, payload: Int):
        self.turns = self.turns + 1
        let result = (payload * 17 + self.bias + self.turns) % 1000000007
        send reply_to.Reply(value = result)
```

### spawn / ask — Actor Lifecycle and Request/Reply

```kn
fn actor_lane(rounds: Int) -> Int:
    let relay = spawn FoldRelay(bias = 11)
    let _warm = ask(relay, "Compute", 0)
    var i: Int = 0
    var acc: Int = 0
    while i < rounds:
        acc = (acc + ask(relay, "Compute", acc + i)) % 1000000007
        i = i + 1
    return acc
```

### Actor Cascade — Spawn + Delegate Reply

```kn
actor Worker:
    state multiplier: Int = 3

    on Process(reply_to: P, val: Int):
        let result = (val * self.multiplier) % 1000000007
        let verifier = spawn Verifier(min_val = 0)
        send verifier.VerifyAndReply(reply_to = reply_to, val = result)

actor Verifier:
    state min_val: Int = 0

    on VerifyAndReply(reply_to: P, val: Int):
        if val < self.min_val:
            send reply_to.Reply(value = -99)
            return
        send reply_to.Reply(value = val)
```

### Actor Telemetry

```kn
fn actor_shape_ok() -> Bool:
    return actor_abi_version() >= 3
        and actor_scheduler_queue_depth() >= 0
        and actor_scheduler_total_enqueued() >= actor_scheduler_total_dequeued()
```

### collapse / observe / decay — Ownership Lifecycle

```kn
fn ownership_lane(count: Int) -> Int with Unsafe:
    let mut cells: ptr<Int> = alloc_zeroed(count, "Int")

    collapse cells:
        var i: Int = 0
        while i < count:
            mem_store(ptr_offset(cells, i, "Int"), (i * 17) + 3, "Int")
            i = i + 1
        0

    let observed: Int = observe cells:
        var w: Int = 0
        var sum: Int = 0
        while w < count:
            sum = (sum + mem_load(ptr_offset(cells, w, "Int"), "Int")) % 1000000007
            w = w + 1
        sum

    decay cells
    return observed
```

### share / fanout — Parallel Disjoint Writes

```kn
fn parallel_lane(partials: ptr<Int>, workers: Int, rounds: Int) -> Int with Unsafe:
    share partials:
        fanout worker in 0..workers:
            let slot: ptr<Int> = ptr_offset(partials, worker, "Int")
            var local: Int = 0
            var step: Int = 0
            while step < rounds:
                local = (local + worker + step) % 1000000007
                step = step + 1
            atomic_store(slot, local)
    return 0
```

______________________________________________________________________

## FOREIGN & IMPORT

### include — C Header Import (Natural)

```kn
include native/native_math.h as nm
include <stdio.h> as cstdio
include <windows.h> as win          // 605 functions from real SDK
include <vulkan/vulkan.h> as vk     // 755 functions from real SDK

fn native_probe() -> Int:
    return nm_mix(7, 11) + cstdio_printf("hello\n")
```

> Companion `.c` file auto-discovered for local headers.
> System headers (angle brackets) use libclang extraction.

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

### @extern / @link_name — Direct DLL/ABI Binding

```kn
@link_name("MessageBoxA")
fn message_box(hwnd: Int, text: ptr<Int>, caption: ptr<Int>, flags: Int) -> Int
```

### @callconv — Calling Convention Control

```kn
@callconv("win64")
fn win64_mix(value: Int) -> Int:
    return (value * 31 + 7) % 1000000007

@callconv("vectorcall")
fn vectorcall_mix(value: Int) -> Int:
    return (value * 17 + 3) % 1000000007
```

### @naked / @section — Naked Function with Custom Section

```kn
@naked
@section(".text.kain.metal.hotpath")
@link_name("__kain_metal_naked_trap")
fn naked_trap() with Unsafe:
    asm("ret")
```

______________________________________________________________________

## GPU & SHADERS

### shader compute — GPU Compute Kernel

```kn
shader compute MyKernel(id: UVec3) -> Void workgroup(8, 8, 1):
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1

    comptime:
        let compute = (
            [32, 32, 1],
            [
                ("src", "f32", ["grid"], "input",  "kain.shared.buffer"),
                ("dst", "f32", ["grid"], "output", "kain.shared.buffer"),
            ],
            [],
        )

    let i = id.x + id.y * UInt(256)
    if i > UInt(254) * UInt(256) + UInt(254):
        return
    dst[i] = src[i] + 1.0
    return
```

### shader vertex / fragment — Graphics Pipeline

```kn
shader vertex MyVertex(position: Vec3, uv: Vec2) -> Vec4:
    uniform offset: Vec3 @0
    return vec4(position.x + offset.x, position.y + offset.y + uv.x, position.z + offset.z, 1.0)

shader fragment MyFragment(uv: Vec2) -> Vec4:
    uniform tint: Vec3 @0
    let wave: Float = uv.x * (1.0 - uv.x)
    return vec4(tint.x * wave, tint.y * wave, tint.z * (0.5 + wave), 1.0)
```

### dispatch — Host-Side GPU Launch

```kn
fn main() -> Int with GPU, Unsafe:
    let init = runtime_init()
    dispatch "shader::MyKernel::compute" [32, 32, 1]
    let shutdown = runtime_shutdown()
    return 0
```

> `dispatch "shader::KernelName::compute" [X, Y, Z]`
> Dimensions are workgroup counts, NOT thread counts.
> Compute key MUST be a string literal.

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

### atomic_add / atomic_store / atomic_fence

```kn
fn atomic_lane(slot: ptr<Int>) -> Int with Unsafe:
    let prior = atomic_add(slot, 1, "acq_rel")
    atomic_fence("seq_cst")
    return prior
```

### lfence / sfence / mfence — x86 ISA Barriers

```kn
fn fence_storm() -> Int with Unsafe:
    lfence()
    let val = 42
    sfence()
    mfence()
    return val
```

### clflush — Cache Line Flush

```kn
fn cache_flush(addr: ptr<Int>) with Unsafe:
    asm("clflush ($0)", addr, memory = true)
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

### asm — With Operands (memory clobber)

```kn
fn clflush_line(buf: ptr<Int>, index: Int) with Unsafe:
    let addr = ptr_offset(buf, index, "Int")
    asm("clflush ($0)", addr, memory = true)
```

> Options: `volatile = true`, `memory = true`, `intel = true`

______________________________________________________________________

## VIRTUAL MEMORY

### vm_reserve / vm_commit / vm_protect / vm_unmap

```kn
fn vm_lane(iterations: Int) -> Int with Unsafe:
    let page_size = vm_page_size()
    let pages = vm_reserve(page_size * 2)
    if ptr_to_int(pages) != 0:
        let _committed = vm_commit(pages, page_size)
        collapse pages:
            mem_store(pages, 42, "Int")
            0
        let _prot = vm_protect_read_write(pages, page_size)
        let _rwx = vm_protect_execute_read_write(pages, page_size)
        let _locked = vm_lock(pages, page_size)
        let _unlocked = vm_unlock(pages, page_size)
        let _decommitted = vm_decommit(pages, page_size)
    let _released = vm_unmap(pages, page_size)
    return 0
```

______________________________________________________________________

## CPU INTRINSICS

### cpuid_eax / cpuid_ebx / cpuid_ecx / cpuid_edx

```kn
fn topology_probe() -> Int with Unsafe:
    let cores = cpu_core_count()
    let logical = cpu_logical_count()
    let cache_line = cpu_cache_line_bytes()
    let sig = cpuid_eax(0, 0)
    let features = cpuid_eax(1, 0)
    let ext = cpuid_ebx(7, 0)
    let tid = current_thread_id()
    let affinity = current_thread_affinity_mask()
    let numa = numa_current_node()
    return cores + logical + cache_line + sig + features + ext + tid + affinity + numa
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

### test — Compiletest-Style Test Case

```kn
test fn test_addition():
    assert(add(2, 2) == 4)

test fn test_bounds():
    let result = signal_valid(42)
    assert(result == true)
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

### Semantic Telemetry

```kn
use std::intent

fn verify_semantics_fired() -> Int:
    let resonate_delta = resonate_fire_count() - resonate_before
    let entangle_delta = entangle_propagation_count() - entangle_before
    let teleport_delta = runtime_machine_teleport_count() - teleport_before
    let patch_delta = patch_journal_count() - patch_before
    let orchestrate_delta = orchestrate_stage_count() - orchestrate_before

    if resonate_delta < 1:    return -10
    if entangle_delta < 1:    return -11
    if teleport_delta < 1:    return -12
    if patch_delta < 1:       return -13
    if orchestrate_delta < 1: return -14
    return 0
```

### Actor Telemetry

```kn
fn actor_telemetry() -> Int:
    return actor_scheduler_queue_depth()
        + actor_scheduler_total_enqueued()
        + actor_scheduler_total_dequeued()
        + actor_scheduler_worker_count()
        + actor_scheduler_overflow_thread_spawns()
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

### All 7 Layers in One Loop Body

```kn
// Each iteration crosses: world → patch → resonate → orchestrate → entangle → actor → teleport
use std::runtime
use std::actor
use std::intent

fn full_fusion_checksum(iterations: Int) -> Int with Unsafe:
    let relay = spawn FusionRelay(bias = 7)
    let teleporter = spawn FusionTeleporter(done = 0, last = 0)
    var i: Int = 0
    var acc: Int = 0

    while i < iterations:
        // LAYER 1+2: Patch world state (triggers resonate)
        let tick = commit_signal(FusionAuthority, i + acc)

        // LAYER 3: Converge fast lane
        let mixed = fast_mix(tick, i)

        // LAYER 4: Orchestrate pipeline
        let piped = signal_pipeline(mixed, tick)

        // LAYER 5: Read entangle-propagated mirror value
        let mirror_val = FusionMirror.signal_copy

        // LAYER 6: Teleport cross-world
        let shard = Shard { bias: 42, phase: 13, checksum: piped, alive: true }
        let moved = teleport shard from FusionAuthority to FusionMirror via shard_bus

        // LAYER 7: Actor ask (packed payload)
        let ack = ask(relay, "Compute", fusion_pack(mirror_val, i))

        acc = (acc + ack + teleport_score(moved)) % 1000000007
        i = i + 1

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

*End of Kain By Example · Every feature has exactly one compilable snippet · Extracted from the
canonical codebase (fusion_chain.kn, metal.kn, keyword_crucible.kn, CRUSHER.kn, blades/,
smoketest/)*
