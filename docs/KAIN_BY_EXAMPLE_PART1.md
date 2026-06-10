# KAIN BY EXAMPLE — Part 1: Plain Code → State Integrity (L0–L2)

> **One compilable snippet per feature. No prose, no theory — just proof that it compiles.**
> Part 1 covers LAYER 0 (plain code, effects), LAYER UI (components), LAYER 1 (world, entangle), LAYER 2 (patch, law).

______________________________________________________________________

## LAYER 0 — PLAIN CODE

### fn — Function Declaration

```kn
fn add(a: Int, b: Int) -> Int with Pure:
    return a + b

fn generic_sum<T: Numeric>(a: T, b: T) -> T with Pure where T: Default:
    return a + b

fn maze_snapshot(maze: ptr<Int>, cell_count: Int) -> [Int] with Unsafe:
    var snapshot: [Int] = []
    var i: Int = 0
    while i < cell_count:
        push(snapshot, mem_load(ptr_offset(maze, i, "Int"), "Int"))
        i = i + 1
    return snapshot
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
enum NativeSubsystem:
    RuntimeCore Filesystem Input Networking Process
    UserInterface Graphics IntentRuntime LowLevelMemory OwnershipMemory

fn subsystem_label(s: NativeSubsystem) -> String:
    match s:
        NativeSubsystem::RuntimeCore    => "runtime-core"
        NativeSubsystem::Filesystem     => "filesystem"
        NativeSubsystem::Input          => "input"
        NativeSubsystem::Networking     => "networking"
        NativeSubsystem::Process        => "process"
        NativeSubsystem::UserInterface  => "user-interface"
        NativeSubsystem::Graphics       => "graphics"
        NativeSubsystem::IntentRuntime  => "intent-runtime"
        NativeSubsystem::LowLevelMemory => "low-level-memory"
        NativeSubsystem::OwnershipMemory => "ownership-memory"
        _ => "unknown"
```

### for — Iteration

```kn
for lane in [1, 2, 3, 4]:
    acc = (acc + lane) % modulus

for i in 0..10:
    result = result * i

for range_value in range(0, 4):
    range_sum = range_sum + range_value
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
var odd_sum = 0
var step = 0
loop:
    step = step + 1
    if step == 2:
        continue
    if step > 5:
        break
    odd_sum = odd_sum + step

let result: Int = loop:
    if done:
        break final_value
    step()
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

### Effects Chained — All 5 in One Expression

```kn
fn pure_effect_score(value: Int) -> Int with Pure:
    return value + 1

fn io_effect_score(value: Int) -> Int with IO:
    return value + 2

fn gpu_effect_score(value: Int) -> Int with GPU:
    return value + 3

fn reactive_effect_score(value: Int) -> Int with Reactive:
    return value + 4

fn unsafe_effect_score(value: Int) -> Int with Unsafe:
    return value + 5

// All 5 effects chained in one expression
let chained = unsafe_effect_score(reactive_effect_score(
    gpu_effect_score(io_effect_score(pure_effect_score(0)))))
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

### component — Full Widget with Raw Memory

```kn
component MemoryWidget(cell_count: Int) with Unsafe:
    state buffer: ptr<Int> = int_to_ptr(0, "Int")
    state initialized: Bool = false
    state checksum: Int = 0

    fn alloc_buffer(_self: Self_) -> Int:
        if _self.initialized:
            return _self.checksum
        _self.buffer = alloc_zeroed(_self.cell_count, "Int")
        collapse _self.buffer:
            var i: Int = 0
            while i < _self.cell_count:
                mem_store(ptr_offset(_self.buffer, i, "Int"),
                          (i * 31 + 7) % 1000000007, "Int")
                i = i + 1
            0
        let obs = observe _self.buffer:
            var acc: Int = 0
            var j: Int = 0
            while j < _self.cell_count:
                acc = (acc + mem_load(ptr_offset(_self.buffer, j, "Int"), "Int")) % 1000000007
                j = j + 1
            acc
        _self.checksum = obs
        _self.initialized = true
        return obs

    fn display_checksum(_self: Self_) -> String:
        return "mem[" + str(_self.cell_count) + "] checksum: " + str(_self.checksum)

    render <box>
        <text value={display_checksum()} />
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

### JSX with Recursive Components

```kn
component RecursiveTree(depth: Int, label: String):
    state expanded: Bool = false

    fn has_children(_self: Self_) -> Bool:
        return _self.depth > 0

    fn display_label(_self: Self_) -> String:
        return _self.label + " (depth " + str(_self.depth) + ")"

    render <stack direction="vertical">
        <text value={display_label()} />
        if has_children():
            <RecursiveTree depth={depth - 1} label={label + ".L"} />
            <RecursiveTree depth={depth - 1} label={label + ".R"} />
        else:
            <text value="LEAF" />
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

### world — Full Game State (18 fields)

```kn
world PongAuthority:
    state left_paddle_y: Int = 228
    state right_paddle_y: Int = 228
    state ball_x: Int = 443
    state ball_y: Int = 273
    state ball_dx: Int = 7
    state ball_dy: Int = 5
    state left_score: Int = 0
    state right_score: Int = 0
    state frame_clock: Int = 0
    state logical_swarm_count: Int = 100000
    state render_swarm_sample_count: Int = 192
    state collisions_total: Int = 0
    state last_goal: Int = 0
    state chaos_mode: Int = 0
    state left_bias: Int = 0
    state right_bias: Int = 14
    state swarm_energy: Int = 100000
    state drift_total: Int = 0
    surface native_ui => App

world PongMirror:
    state mirrored_left_paddle_y: Int = 228
    state mirrored_right_paddle_y: Int = 228
    state mirrored_ball_x: Int = 443
    state mirrored_ball_y: Int = 273
    state mirrored_ball_dx: Int = 7
    state mirrored_ball_dy: Int = 5
    state mirrored_left_score: Int = 0
    state mirrored_right_score: Int = 0
    state mirrored_frame_clock: Int = 0
    state mirrored_logical_swarm_count: Int = 100000
    state mirrored_render_swarm_sample_count: Int = 192
    state mirrored_collisions_total: Int = 0
    state mirrored_last_goal: Int = 0
    state mirrored_chaos_mode: Int = 0
    state mirrored_left_bias: Int = 0
    state mirrored_right_bias: Int = 14
    state mirrored_swarm_energy: Int = 100000
    state mirrored_drift_total: Int = 0
    surface web => App
```

### entangle — 18 Couplings

```kn
entangle PongAuthority.left_paddle_y <-> PongMirror.mirrored_left_paddle_y with single_writer
entangle PongAuthority.right_paddle_y <-> PongMirror.mirrored_right_paddle_y with single_writer
entangle PongAuthority.ball_x <-> PongMirror.mirrored_ball_x with single_writer
entangle PongAuthority.ball_y <-> PongMirror.mirrored_ball_y with single_writer
entangle PongAuthority.ball_dx <-> PongMirror.mirrored_ball_dx with single_writer
entangle PongAuthority.ball_dy <-> PongMirror.mirrored_ball_dy with single_writer
entangle PongAuthority.left_score <-> PongMirror.mirrored_left_score with single_writer
entangle PongAuthority.right_score <-> PongMirror.mirrored_right_score with single_writer
entangle PongAuthority.frame_clock <-> PongMirror.mirrored_frame_clock with single_writer
entangle PongAuthority.collisions_total <-> PongMirror.mirrored_collisions_total with single_writer
entangle PongAuthority.chaos_mode <-> PongMirror.mirrored_chaos_mode with single_writer
entangle PongAuthority.swarm_energy <-> PongMirror.mirrored_swarm_energy with single_writer
entangle PongAuthority.drift_total <-> PongMirror.mirrored_drift_total with single_writer
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

### patch — 18-Field Journaled Mutation

```kn
patch apply_frame(
    authority: PongAuthority,
    left_paddle_y: Int, right_paddle_y: Int,
    ball_x: Int, ball_y: Int,
    ball_dx: Int, ball_dy: Int,
    left_score: Int, right_score: Int,
    frame_clock: Int,
    logical_swarm_count: Int, collisions_total: Int,
    chaos_mode: Int, left_bias: Int,
    right_bias: Int, swarm_energy: Int, drift_total: Int
) -> Int:
    authority.left_paddle_y = left_paddle_y
    authority.right_paddle_y = right_paddle_y
    authority.ball_x = ball_x
    authority.ball_y = ball_y
    authority.ball_dx = ball_dx
    authority.ball_dy = ball_dy
    authority.left_score = left_score
    authority.right_score = right_score
    authority.frame_clock = frame_clock
    authority.logical_swarm_count = logical_swarm_count
    authority.collisions_total = collisions_total
    authority.chaos_mode = chaos_mode
    authority.left_bias = left_bias
    authority.right_bias = right_bias
    authority.swarm_energy = swarm_energy
    authority.drift_total = drift_total
    return authority.frame_clock
```

### law — Compound Invariants (Laws Calling Laws)

```kn
law cell_in_bounds(index: Int, cell_count: Int) -> Bool:
    return index >= 0 and index < cell_count

law coordinate_in_bounds(x: Int, y: Int, width: Int, height: Int) -> Bool:
    return x >= 0 and y >= 0 and x < width and y < height

law start_target_distinct(start: Int, target: Int, cell_count: Int) -> Bool:
    return cell_in_bounds(start, cell_count)
       and cell_in_bounds(target, cell_count)
       and start != target
```
