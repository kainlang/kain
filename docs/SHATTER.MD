# Shatter 〰 Structure-of-Arrays Layout Intent

**Status:** Research synthesis, 2026-06-08
**Based on:** `crates/core/src/parser.rs` (parse_shatter_struct, "shatter" contextual keyword), `crates/core/src/ast.rs` (SHATTER_ATTRIBUTE_NAME, is_shattered), `crates/core/src/types.rs` (shatter struct typechecking), `crates/core/src/runtime_contract.rs` (RuntimeShatterContract, collect_shatter_contracts), `crates/sys-codegen/src/codegen_llvm/mod.rs` (ShatteredArrayBacking, ShatteredArrayLocal, compile_shattered_array_literal, compile_shattered_field_ptr, emit_shatter_lane_bases, emit_stack_shatter_lane_bases), `runtime/native/include/machine_stones.h` (KAIN_MACHINE_CAP_MEMORY_SHATTER, kain_machine_shatter_alloc, kain_machine_shatter_lane_ptr, kain_machine_shatter_lane_base, kain_machine_shatter_free), `runtime/native/src/core/machine_stones.c` (KainMachineShatterBuffer, shatter allocation implementation), `runtime/native/src/core/z3/proofs/native-machine-shatter-lane-offset-stays-in-payload.yaml`, `runtime/native/src/core/z3/proofs/native-machine-shatter-lane-base-shift-offset-stays-in-payload.yaml`, `runtime/native/src/core/z3/proofs-experimental/machine-shatter-shift-offset-equivalence.smt2`, `smoketest/src/semantics/shatter.kn`, `benchmark/cases/machine_stones_shatter_loop/main.kn`, `benchmark/cases/semantic_singularity_shatter_only/main.kn`, `benchmark/cases/pulse_teleport_decay_mesh/main.kn`, `benchmark/cases_v2/keyword_crucible.kn`, `benchmark/cases_v2/fusion_chain.kn`, RULEBOOK.MD, lang-semantics SKILL

______________________________________________________________________

## Table of Contents

1. [What is Shatter?](#what-is-shatter)
1. [The Core Idea ‒ Compiler-Owned SoA](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [How It Works ->> Full Pipeline](#how-it-works)
1. [SoA vs AoS => Layout Comparison](#soa-vs-aos)
1. [Heap vs Stack Allocation](#heap-vs-stack-allocation)
1. [Shatter + Teleport](#shatter--teleport)
1. [Shatter + Axiom](#shatter--axiom)
1. [Under-Utilized Ideas](#under-utilized-ideas)
1. [Combining with Other Keywords](#combining-with-other-keywords)
1. [Native C API](#native-c-api)
1. [Z3 Proofs](#z3-proofs)
1. [Validation Ladder](#validation-ladder)
1. [Source Anchors](#source-anchors)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)

______________________________________________________________________

## What is Shatter?

`shatter struct` is a **struct declaration modifier** in Kain that marks a struct for **Structure-of-Arrays (SoA) layout** ⁓ the compiler owns the memory layout transformation. Instead of Array-of-Structs (AoS) where `[N]Particle` stores each particle's fields contiguously, shatter tells the compiler to lay out each field as its own contiguous array -- the `x` lane, `y` lane, `vx` lane, `vy` lane, etc.

```kn
// Without shatter: Array-of-Structs (AoS)
// Memory: [x0,y0,vx0,vy0,alive0, x1,y1,vx1,vy1,alive1, ...]
struct Particle:
    x: Float
    y: Float
    vx: Float
    vy: Float
    alive: Bool

// With shatter: Structure-of-Arrays (SoA)
// Memory: [x0,x1,x2,...] [y0,y1,y2,...] [vx0,vx1,...] [vy0,vy1,...] [alive0,alive1,...]
shatter struct Particle:
    x: Float
    y: Float
    vx: Float
    vy: Float
    alive: Bool
```

This is critical for **SIMD-friendly, cache-line-optimized hot loops**. When iterating over a shattered array, consecutive accesses hit contiguous memory within the same lane ‒ perfect for vectorized loads and stores.

The key insight: **shatter is not a separate keyword** ‒ it is a modifier before `struct` (parsed as `shatter struct Name:`). The parser pushes a `#[shatter]` attribute and delegates to the normal struct parser. The compiler owns all layout decisions.

______________________________________________________________________

## The Core Idea --> Compiler-Owned SoA

### What Shatter Is NOT

- ❌ Not a programmer-specified layout --> the compiler decides SoA vs AoS details, not the programmer
- ❌ Not a separate data type \<--> a shattered struct is still a struct, accessed with `.field` syntax
- ❌ Not a SIMD intrinsic -- it enables SIMD-friendly layout, but doesn't emit SIMD instructions itself
- ❌ Not a runtime type ~ the transformation is purely at the memory layout level
- ❌ Not a database column store ~> it's an SoA memory layout for hot loops
- ❌ Not a packing directive --- `shatter` is about lane organization, not padding minimization
- ❌ Not a GPU-only feature --- shatter works on CPU too; it is runtime-neutral

### What Shatter IS

- ✅ A **struct declaration modifier** that instructs the compiler to use Structure-of-Arrays layout
- ✅ A **compiler-owned transformation** \<--> the parser pushes a `#[shatter]` attribute, and the codegen layer decides the exact SoA format
- ✅ A **memory lane separator** ~~ each field becomes its own contiguous lane in memory
- ✅ A **SIMD enabler** \_\_\_ SoA layout means N consecutive `x` values are adjacent in memory, ready for vectorized loads
- ✅ A **cache-line optimizer** === hot loops that touch only 1–2 fields per iteration only load those cache lines, not entire structs
- ✅ A **runtime-contract citizen** – appears as `RuntimeShatterContract` with name, layout string, and field lane list
- ✅ A **capability-gated feature** => requires `KAIN_MACHINE_CAP_MEMORY_SHATTER` (bit 0x00000004)
- ✅ A **Z3-proven memory safety target** – two proofs guarantee lane pointer arithmetic stays inside the SoA payload
- ✅ A **first-class teleport partner** \_\_\_ shattered structs can be teleported across worlds, preserving the SoA layout

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `shatter struct` | "Struct with SoA layout" | **Compiler-owned memory transformation** -- the parser adds a `#[shatter]` attribute, the codegen produces lane-based allocation and access. The runtime provides the contiguous SoA buffer. |

When you write `shatter struct Particle:`, you are telling the compiler:

> "This struct will be used in hot loops with array access. Arrange the fields in separate contiguous lanes so that iterating over N elements accesses consecutive memory within each lane. You own the layout ⁓ I own the `.field` access syntax."

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
shatter struct <Name>:
    <field1>: <Type1>
    <field2>: <Type2>
    ...
```

Where:

- `shatter` is the contextual keyword (must appear immediately before `struct`)
- `<Name>` is an identifier for the struct
- `<field>` is a field name
- `<Type>` is a field type (typically `Int`, `Float`, `Bool`, or other scalar types)

### Grammar (from parser.rs)

```
shatter_struct = "shatter" "struct" ident ":" indent field_list dedent
```

The parser:

1. Expects contextual keyword `shatter`
1. Pushes `Attribute { name: SHATTER_ATTRIBUTE_NAME, args: Vec::new(), span: start }`
1. Delegates to `parse_struct_with_attrs(vis, attrs)` ->> the normal struct parser
1. The struct body is parsed identically to a regular struct

```rust
fn parse_shatter_struct(&mut self, vis: Visibility, mut attrs: Vec<Attribute>) -> KainResult<Item> {
    let start = self.current_span();
    self.expect_contextual_ident("shatter")?;
    attrs.push(Attribute {
        name: SHATTER_ATTRIBUTE_NAME.to_string(),
        args: Vec::new(),
        span: start,
    });
    self.parse_struct_with_attrs(vis, attrs)
}
```

### Concrete Examples

```kn
// Minimal: single shatter struct
shatter struct Shard:
    bias: Int
    phase: Int
    salt: Int
    alive: Bool

// Particle system ->> the canonical use case
shatter struct Particle:
    x: Int
    y: Int
    vx: Int
    vy: Int
    alive: Bool

// Mesh vertex data
shatter struct Vertex:
    position_x: Float
    position_y: Float
    position_z: Float
    normal_x: Float
    normal_y: Float
    normal_z: Float
    u: Float
    v: Float

// Semantic benchmark shard
shatter struct SemanticShard:
    x: Int
    y: Int
    drift: Int
    alive: Bool

// Pulse-actor-shatter fusion (from fusion_chain.kn)
shatter struct PulseShard:
    bias: Int
    phase: Int
    salt: Int
    hot: Bool
```

______________________________________________________________________

## How It Works >> Full Pipeline

### Source to Runtime Flow

```
.kn shatter struct declaration
  → parser.rs: parse_shatter_struct()
    → pushes Attribute { name: "shatter" }
    → parse_struct_with_attrs() -- normal struct parsing
    → Item::Struct(Struct { attributes: ["shatter"], ... })

  → ast.rs: Struct.is_shattered()
    → self.attributes.iter().any(|attr| attr.name == SHATTER_ATTRIBUTE_NAME)
    → returns true when "shatter" attribute is present

  → types.rs: (typecheck ... no special validation for shatter structs)
    → Struct is typechecked as a regular struct with its attribute preserved

  → runtime_contract.rs: collect_shatter_contracts()
    → RuntimeShatterContract { name, layout: "structure-of-arrays", field_lanes: [...] }
    → Emits "memory.shatter" capability when any shatter struct exists

  → codegen_llvm/mod.rs: struct registration (Item::Struct)
    → Inserts struct fields into self.struct_defs
    → Registers name in self.shattered_structs

  → codegen_llvm/mod.rs: local variable lowering
    → When an array of shattered struct is declared:
      → compile_shattered_array_literal()
        → Calls @kain_machine_shatter_alloc(lane_count, element_count)
        → Call @kain_machine_shatter_lane_base() for each lane
        → Populates lanes with struct literal values
      → OR stack-backed: emit_stack_shatter_lane_bases()
        → Allocas per lane, bitcast to i8*

  → codegen_llvm/mod.rs: field access (compile_shattered_field_ptr)
    → particles[lane].x
    → Lookup: is this array a shattered array local?
    → If yes: lane_base + (element_index * 8) via GEP or shl
    → If runtime-backed: @kain_machine_shatter_lane_ptr(handle, field_lane, element_index)

  → runtime/native: kain_machine_shatter_alloc / _lane_ptr / _lane_base / _free
    → Contiguous buffer: [lane_data[0..N], lane_data[1..N], ...]
    → Element slot: KAIN_MACHINE_SHATTER_SLOT_BYTES = 8 bytes
```

### At Parse Time

The contextual keyword `shatter` is recognized in `parse_item`:

```rust
// In parse_item, among the contextual keyword checks:
TokenKind::Ident(ref name) if name == "shatter" => {
    self.parse_shatter_struct(vis, attributes)
}
```

The keyword table (line 76) lists `"shatter"` alongside `"axiom"`, `"pulse"`, `"teleport"`, `"entangle"`, etc. as contextual identifiers - they are not reserved, only recognized in specific contexts.

The `parse_shatter_struct` function is minimal => it adds an attribute and delegates:

```rust
fn parse_shatter_struct(&mut self, vis: Visibility, mut attrs: Vec<Attribute>) -> KainResult<Item> {
    let start = self.current_span();
    self.expect_contextual_ident("shatter")?;
    attrs.push(Attribute {
        name: SHATTER_ATTRIBUTE_NAME.to_string(),
        args: Vec::new(),
        span: start,
    });
    self.parse_struct_with_attrs(vis, attrs)
}
```

### At AST Level

The canonical attribute marker is defined in `ast.rs`:

```rust
pub const SHATTER_ATTRIBUTE_NAME: &str = "shatter";
```

And the check on the `Struct` type:

```rust
impl Struct {
    pub fn is_shattered(&self) -> bool {
        self.attributes
            .iter()
            .any(|attribute| attribute.name == SHATTER_ATTRIBUTE_NAME)
    }
}
```

This is the single source of truth for "is this struct shattered?" >> used by the runtime contract collector, LLVM codegen, and any downstream tooling.

### At Typecheck Time

The typechecker (`types.rs`) has **no special validation** for shatter structs. A `shatter struct` is typechecked as a regular struct. The `#[shatter]` attribute is preserved through typecheck and passed to codegen.

This means:

- Field types are checked normally
- Generics work normally
- Visibility modifiers work normally
- No extra shatter-specific validation is enforced at typecheck time

### At Runtime Contract Collection

Every shattered struct produces a `RuntimeShatterContract`:

```rust
pub struct RuntimeShatterContract {
    pub name: String,
    pub layout: String,        // always "structure-of-arrays"
    pub field_lanes: Vec<String>,  // ordered list of field names
}
```

Collected by `collect_shatter_contracts()`, which walks all typed items, finds `TypedItem::Struct` where `is_shattered()`, and emits a contract.

When any shattered struct exists, the `memory.shatter` capability is emitted:

```rust
if summary.shatters > 0 {
    add_capability(
        "memory.shatter",
        Some("Program declares shattered SoA structs."),
    );
}
```

### At LLVM Codegen Time

The LLVM codegen is where shatter comes alive. There are three critical data structures:

**`ShatteredArrayBacking`** |-> the allocation strategy:

```rust
enum ShatteredArrayBacking {
    RuntimeHandle,       // Heap-allocated via kain_machine_shatter_alloc
    StackLaneBuffers,    // Stack-allocated per-lane alloca buffers
}
```

**`ShatteredArrayLocal`** ~> the tracked local variable:

```rust
struct ShatteredArrayLocal {
    struct_name: String,
    element_count: usize,
    lane_base_values: Vec<String>,
    backing: ShatteredArrayBacking,
}
```

**Registration:**

When a `TypedItem::Struct` is processed and `is_shattered()` returns true, the struct name is inserted into `self.shattered_structs`:

```rust
TypedItem::Struct(struct_def) if struct_def.ast.is_shattered() => {
    self.shattered_structs.insert(struct_def.ast.name.clone());
}
```

**Array literal lowering:**

When an array literal of shattered structs is encountered (`compile_shattered_array_literal`):

```llvm
; Allocate the SoA buffer
%handle = call i8* @kain_machine_shatter_alloc(i64 <lane_count>, i64 <element_count>)

; Get base pointer for each lane
%lane0 = call i8* @kain_machine_shatter_lane_base(i8* %handle, i64 0)
%lane1 = call i8* @kain_machine_shatter_lane_base(i8* %handle, i64 1)
...

; Populate each element across lanes
%elem0_lane0_ptr = getelementptr inbounds i8, i8* %lane0, i64 0
%elem0_lane0_typed = bitcast i8* %elem0_lane0_ptr to <field_ty>*
store <field_ty> <value>, <field_ty>* %elem0_lane0_typed
```

**Field access lowering:**

When accessing `particles[lane].x` (`compile_shattered_field_ptr`):

1. Checks if the array is in `self.shattered_array_locals`
1. Finds the field index in the struct's fields
1. If a lane base is available and index is proven in-bounds:
   - Computes `byte_offset = element_index * 8` (or `element_index << 3`)
   - Uses `getelementptr inbounds` on the lane base pointer
   - Bitcasts to the field type
1. If runtime-backed fallback:
   - Calls `@kain_machine_shatter_lane_ptr(handle, field_lane, element_index)`

```llvm
; Fast path: lane_base + (element_index * 8)
%idx = shl i64 <element_index>, 3
%raw = getelementptr inbounds i8, i8* %lane_base, i64 %idx
%typed = bitcast i8* %raw to <field_ty>*
%val = load <field_ty>, <field_ty>* %typed

; Runtime fallback path:
%ptr = call i8* @kain_machine_shatter_lane_ptr(i8* %handle, i64 <field_lane>, i64 <element_index>)
```

**The `shattered_index_is_proven_in_bounds`** check is key === when the index is a compile-time literal (`i % 8` where `i` is proven in `0..8`), the compiler can use the fast `lane_base + offset` path instead of the runtime function call.

### At Runtime (Native C)

The native runtime provides a simple contiguous SoA buffer. See [Native C API](#native-c-api) below for full details.

______________________________________________________________________

## SoA vs AoS – Layout Comparison

### Memory Layout

Consider `Particle` with fields `{x: Int, y: Int, vx: Int, vy: Int, alive: Bool}`.

**Array-of-Structs (AoS)** ->> the default without shatter:

```
Memory: [x0,y0,vx0,vy0,alive0, x1,y1,vx1,vy1,alive1, x2,y2,vx2,vy2,alive2, ...]
         ^-- struct 0 --^  ^-- struct 1 --^  ^-- struct 2 --^
```

- Each struct is packed contiguously
- Field `x` of element 0 and field `x` of element 1 are separated by the entire struct size
- Iterating over all `x` values causes cache-line waste (loading `y`, `vx`, `vy`, `alive` too)

**Structure-of-Arrays (SoA)** * * * with shatter:

```
Memory: [x0,x1,x2,...,xN] [y0,y1,y2,...,yN] [vx0,vx1,...,vxN] [vy0,vy1,...,vyN] [alive0,alive1,...,aliveN]
         ^-- x lane --^  ^-- y lane --^  ^-- vx lane --^  ^-- vy lane --^  ^-- alive lane --^
```

- Each field is its own contiguous lane
- Field `x` of element 0 and field `x` of element N-1 are adjacent in memory
- Iterating over all `x` values is a linear scan over one contiguous block
- Zero waste -- no unused fields loaded into cache

### When Shatter Wins

| Scenario | AoS | SoA (shatter) |
|----------|-----|---------------|
| **SIMD loops** | Must gather/scatter per-iteration | Contiguous lanes → vector load/store |
| **Hot path touches 1 field** | Full struct loaded to cache | Only that field's lane loaded |
| **Hot path touches 2 fields** | 2 struct fields loaded, rest wasted | Only those 2 lanes loaded |
| **Transform all x,y** | Scattered loads across structs | Linear scan over x lane, y lane |
| **Particle death sweep** | Loads all fields to check alive flag | Linear scan over alive lane only |
| **Narrow struct (< 128 bytes)** | Cache-friendly, one struct per line | Still beneficial for SIMD |
| **Random access pattern** | Predictable offset per struct | Must compute lane offset ~ slightly more arithmetic |
| **Small arrays (< 4 elements)** | Better cache utilization | Overhead may not pay off |

### The Compiler's Decision

The compiler decides the exact SoA layout. Current implementation:

- Each slot is `KAIN_MACHINE_SHATTER_SLOT_BYTES` = 8 bytes (`i64` width)
- Fields smaller than 8 bytes use 1 slot
- All lanes are the same length (`element_count`)
- Layout: `lane0[0..N]`, `lane1[0..N]`, ..., `laneM[0..N]`

The LLVM codegen chooses between **heap-backed** (runtime allocate) and **stack-backed** (alloca per lane) based on:

- Whether the array is a local that never escapes field-projection or len-only use
- If stack-safe (`expr_is_safe_stack_shatter_use` proves no escape), stack lane buffers are used
- Otherwise, heap allocation via `kain_machine_shatter_alloc`

______________________________________________________________________

## Heap vs Stack Allocation

The LLVM codegen makes an optimization decision when lowering shattered array literals. There are two backing strategies:

### Runtime-Handle Backing

```llvm
; Heap-allocate one contiguous SoA buffer
%handle = call i8* @kain_machine_shatter_alloc(i64 <lane_count>, i64 <element_count>)
; Get per-lane base pointers
%lane0 = call i8* @kain_machine_shatter_lane_base(i8* %handle, i64 0)
%lane1 = call i8* @kain_machine_shatter_lane_base(i8* %handle, i64 1)
...
```

Used when:

- The array escapes its scope
- The array is passed to functions
- The array is stored in a struct or returned
- Cannot prove safe for stack allocation

### Stack-Lane-Buffer Backing

```llvm
; Stack-allocate per-lane buffers
%shatter_lane0_1 = alloca [N x i64]
%lane0 = bitcast [N x i64]* %shatter_lane0_1 to i8*
%shatter_lane1_2 = alloca [N x i64]
%lane1 = bitcast [N x i64]* %shatter_lane1_2 to i8*
...
```

Used when:

- The local variable is a stack-local array of shattered structs
- Every use is a closed field-projection (`arr[lane].field`) or len() call
- The `expr_is_safe_stack_shatter_use` analysis proves no escaping access
- The scope is tracked in `stack_shatter_candidate_scopes`

The safety analysis (`expr_is_safe_stack_shatter_use`) recursively checks:

- Direct field projections: `arr[i].field` ✓
- Assignments: `arr[i].field = value` ✓
- Function calls with the array as argument: X (escapes)
- Pointer operations: X (escapes)
- Return of the array: X (escapes)

### Free Behavior

For runtime-handle-backed allocations, `kain_machine_shatter_free` is called when the local goes out of scope:

```rust
// Cleanup in scope exit
if let Some(local) = self.shattered_array_locals.remove(var_name) {
    if matches!(local.backing, ShatteredArrayBacking::RuntimeHandle) {
        emit("  call void @kain_machine_shatter_free(i8* %handle)");
    }
}
```

Stack-backed allocations don't need explicit free ~ they're alloca'd and cleaned up by the standard stack frame teardown.

______________________________________________________________________

## Shatter + Teleport

Shattered structs are **first-class teleport citizens**. The SoA layout is preserved when teleporting across worlds.

### How It Works

```kn
world NativeWorld:
    state beat: Int = 0
    surface native_ui => PulsePanel

world MirrorWorld:
    state beat_copy: Int = 0
    surface web => PulsePanel

shatter struct PulseShard:
    bias: Int
    phase: Int
    salt: Int
    hot: Bool

pulse relay_clock every 4ms jitter 1ms:
    let shard = PulseShard { bias: 3, phase: 5, salt: 7, hot: true }
    let moved = teleport shard from NativeWorld to MirrorWorld via relay_clock_bus
    let _pulse_shape = pulse_tick + pulse_dt_ms + pulse_missed + moved.bias + moved.phase
```

When a shattered struct is teleported:

1. The entire struct value is serialized/lifted from the source world
1. Transported via the named bus/channel
1. Materialized in the target world
1. The SoA layout is **preserved** in both source and target => the struct still uses `.field` syntax

### From the Semantic Singularity Benchmark

```kn
// Shatter structs are teleported across worlds in hot loops
let shard = PulseShard {
    bias: shards[lane].bias,
    phase: shards[lane].phase,
    salt: shards[lane].salt,
    hot: shards[lane].hot
}
let moved = teleport shard from PulseAuthority to PulseMirror via pulse_hot_bus
let staged = pulse_pipeline((checksum + old_cell + moved.bias + moved.phase + ...))
```

The teleport happens on every iteration of a 54,000-round loop in the full fusion chain benchmark (`pulse_teleport_decay_mesh/main.kn`).

### Runtime Telemetry

```kn
// Query teleport count across the program
let teleports = runtime_machine_teleport_count()
```

The `runtime_machine_teleport_count()` fuction tracks all teleports, including shattered struct transfers. The `fusion_pulse_actor_teleport` benchmark logs 512 teleport events across 8 shattered shards.

______________________________________________________________________

## Shatter + Axiom

Shatter pairs with `axiom` as part of the **Machine Stones** layer (LAYER 6). The axiom declares the machine capability assumptions, including shatter support.

### Capability Contract

The `memory.shatter` capability is checked via axiom:

```kn
axiom machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "machine lane supports shatter and teleport"
    fallback scalar_fallback
```

### Capability Bit

```c
#define KAIN_MACHINE_CAP_MEMORY_SHATTER UINT64_C(0x0000000000000004)  // bit 2
```

This is an **always-available** capability in the current runtime :: it is unconditionally included in `kain_machine_current_capabilities()`:

```c
static uint64_t kain_machine_current_capabilities(void) {
    uint64_t caps = KAIN_MACHINE_CAP_ATOMIC_BITMASK |
                    KAIN_MACHINE_CAP_TIME_HARDWARE_TIMER |
                    KAIN_MACHINE_CAP_MEMORY_SHATTER |
                    KAIN_MACHINE_CAP_WORLD_TELEPORT;
    // ... plus CPUID-gated caps
    return caps;
}
```

### Axiom + Shatter in Orchestrate

```kn
axiom orchestrate_silicon_truth:
    when target("llvm")
    when capability("gpu.compute")
    when capability("memory.shatter")
    guarantee "orchestrate lane may own silicon residency, transfer, and fallback policy"
    fallback orchestrate_axiom_fallback

orchestrate pipeline(value: Int) -> Int:
    stage host: cpu mix(value) ...
    stage gpu_result: gpu compute(host) residency device transfer host_to_device
        guarded by orchestrate_silicon_truth
        fallback degrade host
    return gpu_result
```

The axiom declares the capability, the orchestrate `guarded by` clause gates the stage.

### Runtime Contract Emission

When any shattered struct is present in the program, the `memory.shatter` capability is auto-emitted:

```json
{
  "capabilities": ["memory.shatter", ...],
  "shatters": [
    {
      "name": "PulseShard",
      "layout": "structure-of-arrays",
      "field_lanes": ["bias", "phase", "salt", "hot"]
    }
  ]
}
```

This means: **if you declare `capability("memory.shatter")` in an axiom, the runtime contract will confirm shatter is available when your program uses it.**

______________________________________________________________________

## Under-Utilized Ideas ⁓ Creative Semantic Abuse

### 1. Shatter for GPU Compute Kernels

Current usage focuses on CPU particle loops. But SoA layout is even more natural for GPU compute --- lanes become GPU threads, and each thread processes one element across fields:

```kn
shatter struct GPUParticle:
    pos_x: Float
    pos_y: Float
    vel_x: Float
    vel_y: Float
    mass: Float
    charge: Float

shader compute particle_shader:
    dispatch { x: 256, y: 1, z: 1 }
    workgroup 64
    fn main(@builtin(global_invocation_id) id: vec3<u32>):
        let i = id.x
        // Each thread processes one particle
        // pos_x[i], pos_y[i], vel_x[i], vel_y[i] are contiguous in memory
        // GPU coalesced access is optimal
```

**Under-explored:** Explicit shatter-to-GPU memory binding. Currently shatter is runtime-neutral \<--> it provides SoA layout for both CPU and GPU. A GPU-specific shatter could bind shader `StorageBuffer` lanes directly to shatter fields.

### 2. Shatter + Resizeable Lanes

Current shatter buffers are fixed-size (`element_count` is set at allocation). A resizeable shatter variant could grow lanes dynamically:

```kn
shatter struct DynamicGrid:
    cells: Float     // automatically resizes

// Dynamic shatter could grow all lanes atomically
```

**Under-explored:** Dynamic shatter buffers that support push/append semantics while keeping all lanes in sync.

### 3. Multi-Field Lane Sweeps

When only some fields are hot, shatter lets you operate on a subset:

```kn
// Only touch x and y ~> vx, vy, alive lanes stay cold in cache
fn sweep_positions(particles: ptr<Particle>, count: Int) -> Int:
    collapse particles:
        var i: Int = 0
        while i < count:
            let px = particles[i].x
            let py = particles[i].y
            // Only x and y lanes are loaded into cache
            i = i + 1
        0
```

**Under-explored:** Automatic field-prefetching hints * * * "only lanes x, y needed" could prefetch only those lanes into L1.

### 4. Shatter for Hash Tables / Lookup Structures

SoA layout can benefit hash table entries too ->> keys in one lane, values in another:

```kn
shatter struct HashEntry:
    key: Int
    value: Int
    occupied: Bool
```

A linear probe scan over only the `occupied` lane avoids loading key/value until a slot is found.

**Under-explored:** Shatter as a general-purpose associative-data-structure layout.

### 5. Nested Shatter

Current shatter is flat >> all fields are peers. A nested shatter could compose:

```kn
shatter struct Transform:
    position: Float[3]    // could be its own SoA
    rotation: Float[4]
    scale: Float[3]

shatter struct SceneNode:
    transform: Transform  // nested shatter?
    parent: Int
    child_count: Int
```

**Under-explored:** Recursive/hierarchical SoA layout where nested shattered structs produce sub-lanes.

### 6. Shatter + Law: Invariant Checking on Lanes

A `law` could verify invariants across lanes:

```kn
law particles_in_bounds(particles: ptr<Particle>, count: Int) -> Bool:
    var ok: Bool = true
    var i: Int = 0
    while i < count:
        if particles[i].x < 0 or particles[i].x > 1024:
            ok = false
        if particles[i].y < 0 or particles[i].y > 768:
            ok = false
        i = i + 1
    return ok
```

**Under-explored:** Law-level lane analysis – the compiler could prove a law only touches specific lanes and emit specialized lane-scan loops.

### 7. Shatter + Resonate: Reactive Per-Field Updates

A `resonate` on a world field that holds a shattered struct could detect per-lane changes:

```kn
shatter struct Config:
    gamma: Float
    brightness: Float
    contrast: Float

world Settings:
    state config: Config = Config { gamma: 1.0, brightness: 0.5, contrast: 1.0 }
    surface settings_ui => SettingsPanel

resonate Settings.config dampen 16ms:
    // The whole config changed - but which lane?
    // Future: per-lane resonate triggers
```

**Under-explored:** Per-lane resonate triggers that fire only when a specific field lane changes.

______________________________________________________________________

## Combining with Other Keywords

### Shatter + Teleport → Cross-World SoA Transfer

Shattered structs teleported across worlds preserve their SoA layout. The teleport bus serializes the struct value; the target world receives it with the same field access semantics.

### Shatter + Axiom → Capability-Gated SoA

The axiom declares `capability("memory.shatter")`. The runtime gate ensures shatter-backed code only runs on machines that support it.

### Shatter + Pulse → Hot-Loop Particle Simulation

Pulse bodies iterate over shattered arrays for particle physics, mesh updates, or signal processing. The SoA layout ensures cache-line-efficient access.

### Shatter + Orchestrate → Stage-Graph SoA Processing

Orchestrate stages can process shattered arrays in CPU or GPU stages. The SoA layout is natural for both residency models.

### Shatter + Collapse/Observe → Ownership Over Shattered Memory

Shatter allocations can be managed through ownership:

```kn
let particles = [
    Particle { x: 1, y: 2, vx: 0, vy: 0, alive: true },
    Particle { x: 3, y: 4, vx: 1, vy: 1, alive: false },
]

collapse particles:
    var acc: Int = 0
    for lane in range(0, 2):
        acc = acc + particles[lane].x
    0

// particles is freed when scope exits
```

### Shatter + Law → Lane-Invariant Checking

Laws verify invariants across lanes without touching the whole struct.

### Shatter + World → SoA State in Worlds

A world can hold a shattered struct as state (via `entangle`, teleport, or direct assignment).

### Shatter + Entangle → SoA State Mirroring

When a world field holds a shattered struct, `entangle` propagates changes to the mirror world.

### Shatter + Resonate → Reactive SoA Updates

Changes to shattered struct fields in worlds trigger resonate handlers.

### Shatter + Converge → Lane-Specialized Algorithms

Converge lanes can select different SoA-processing algorithms based on capabilities or data size.

______________________________________________________________________

## Native C API

### Data Structure (`machine_stones.h` / `machine_stones.c`)

The native runtime defines the shatter buffer as a contiguous allocation:

```c
typedef struct KainMachineShatterBuffer {
    uint64_t lane_count;       // number of fields (lanes)
    uint64_t element_count;    // number of elements per lane
    uint64_t payload_bytes;    // total data bytes: lane_count * element_count * 8
    unsigned char data[];      // flexible array member – contiguous SoA payload
} KainMachineShatterBuffer;
```

Layout in memory:

```
data: [lane0_elem0] [lane0_elem1] ... [lane0_elemN]
      [lane1_elem0] [lane1_elem1] ... [lane1_elemN]
      ...
      [laneM_elem0] [laneM_elem1] ... [laneM_elemN]
```

Each slot is `KAIN_MACHINE_SHATTER_SLOT_BYTES` = 8 bytes. All fields are stored as 8-byte slots regardless of their logical type.

### Function Reference

#### `kain_machine_shatter_alloc`

```c
void* kain_machine_shatter_alloc(uint64_t lane_count, uint64_t element_count);
```

Allocates a contiguous SoA buffer with `lane_count` lanes of `element_count` elements each.

- **Parameters:**
  - `lane_count` --- number of fields in the shattered struct
  - `element_count` \<--> number of elements per lane
- **Returns:** Opaque handle pointer (actually a `KainMachineShatterBuffer*`), or `NULL` on error
- **Errors:**
  - `EINVAL` \_\_\_ lane_count or element_count is 0
  - `ERANGE` ⁓ overflow in total size computation
  - `ENOMEM` :: allocation failure
- **Overflow safety:** All intermediate multiplications are checked via `kain_machine_mul_overflow_u64` and `kain_machine_add_overflow_u64`

```c
void* kain_machine_shatter_alloc(uint64_t lane_count, uint64_t element_count) {
    uint64_t slots;
    uint64_t payload_bytes;
    uint64_t total_bytes;
    KainMachineShatterBuffer* buffer;

    if (lane_count == 0u || element_count == 0u) {
        errno = EINVAL;
        return NULL;
    }

    // Check overflow at every step
    if (kain_machine_mul_overflow_u64(lane_count, element_count, &slots) ||
        kain_machine_mul_overflow_u64(slots, KAIN_MACHINE_SHATTER_SLOT_BYTES, &payload_bytes) ||
        kain_machine_add_overflow_u64(sizeof(KainMachineShatterBuffer), payload_bytes, &total_bytes) ||
        total_bytes > (uint64_t)SIZE_MAX) {
        errno = ERANGE;
        return NULL;
    }

    buffer = (KainMachineShatterBuffer*)calloc(1u, (size_t)total_bytes);
    if (buffer == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    buffer->lane_count = lane_count;
    buffer->element_count = element_count;
    buffer->payload_bytes = payload_bytes;
    return buffer;
}
```

#### `kain_machine_shatter_lane_ptr`

```c
void* kain_machine_shatter_lane_ptr(void* handle, uint64_t lane_index, uint64_t element_index);
```

Returns a pointer to a specific element in a specific lane.

- **Parameters:**
  - `handle` 〰 opaque handle returned by `kain_machine_shatter_alloc`
  - `lane_index` ~> lane index (0-based, must be < lane_count)
  - `element_index` => element index (0-based, must be < element_count)
- **Returns:** Pointer to the slot, or `NULL` on error
- **Errors:**
  - `ERANGE` 〰 NULL handle, lane_index out of bounds, element_index out of bounds, or arithmetic overflow
- **Proof:** `native-machine-shatter-lane-offset-stays-in-payload.yaml` >> Z3 proves the returned pointer is always inside the contiguous payload

```c
void* kain_machine_shatter_lane_ptr(void* handle, uint64_t lane_index, uint64_t element_index) {
    KainMachineShatterBuffer* buffer = (KainMachineShatterBuffer*)handle;
    uint64_t linear_index;
    uint64_t byte_offset;

    if (buffer == NULL || lane_index >= buffer->lane_count ||
        element_index >= buffer->element_count) {
        errno = ERANGE;
        return NULL;
    }

    // linear_index = lane * element_count + element
    // byte_offset = linear_index * 8
    if (kain_machine_mul_overflow_u64(lane_index, buffer->element_count, &linear_index) ||
        kain_machine_add_overflow_u64(linear_index, element_index, &linear_index) ||
        kain_machine_mul_overflow_u64(linear_index, KAIN_MACHINE_SHATTER_SLOT_BYTES, &byte_offset) ||
        byte_offset >= buffer->payload_bytes) {
        errno = ERANGE;
        return NULL;
    }

    return (void*)(buffer->data + byte_offset);
}
```

#### `kain_machine_shatter_lane_base`

```c
void* kain_machine_shatter_lane_base(void* handle, uint64_t lane_index);
```

Returns a pointer to the start of a lane (element 0 in that lane).

- **Parameters:**
  - `handle` -- opaque handle returned by `kain_machine_shatter_alloc`
  - `lane_index` --> lane index (0-based, must be < lane_count)
- **Returns:** Pointer to element 0 of the lane, or `NULL` on error
- **Errors:**
  - `ERANGE` --- NULL handle, lane_index out of bounds, or arithmetic overflow
- **Usage:** The LLVM codegen uses this as the fast path ~~ it gets the lane base once, then computes element offsets as `base + (element_index * 8)` directly
- **Proof:** `native-machine-shatter-lane-base-shift-offset-stays-in-payload.yaml` ⁓ Z3 proves the base + offset arithmetic stays inside the payload

```c
void* kain_machine_shatter_lane_base(void* handle, uint64_t lane_index) {
    KainMachineShatterBuffer* buffer = (KainMachineShatterBuffer*)handle;
    uint64_t lane_slots;
    uint64_t byte_offset;

    if (buffer == NULL || lane_index >= buffer->lane_count) {
        errno = ERANGE;
        return NULL;
    }

    // byte_offset = lane * element_count * 8
    if (kain_machine_mul_overflow_u64(lane_index, buffer->element_count, &lane_slots) ||
        kain_machine_mul_overflow_u64(lane_slots, KAIN_MACHINE_SHATTER_SLOT_BYTES, &byte_offset) ||
        byte_offset >= buffer->payload_bytes) {
        errno = ERANGE;
        return NULL;
    }

    return (void*)(buffer->data + byte_offset);
}
```

#### `kain_machine_shatter_free`

```c
void kain_machine_shatter_free(void* handle);
```

Frees a shatter buffer previously allocated with `kain_machine_shatter_alloc`.

- **Parameters:**
  - `handle` 〰 opaque handle returned by `kain_machine_shatter_alloc`
- **Notes:** Directly calls `free()` on the handle. NULL-safe (standard `free` semantics).

```c
void kain_machine_shatter_free(void* handle) {
    free(handle);
}
```

### LLVM Declarations

The LLVM codegen declares these functions at module level:

```llvm
declare i8* @kain_machine_shatter_alloc(i64, i64)
declare i8* @kain_machine_shatter_lane_ptr(i8*, i64, i64)
declare i8* @kain_machine_shatter_lane_base(i8*, i64)
declare void @kain_machine_shatter_free(i8*)
```

### Capability Flag

```c
#define KAIN_MACHINE_CAP_MEMORY_SHATTER UINT64_C(0x0000000000000004)  // bit 2
```

This is always present in the current runtime's capability mask - shatter is always supported on builds that include the machine stones runtime.

______________________________________________________________________

## Z3 Proofs

Two Z3 proofs guarantee the memory safety of shatter lane pointer arithmetic, plus one experimental proof for shift equivalence.

### Proof 1: Lane Offset Stays in Payload

**File:** `runtime/native/src/core/z3/proofs/native-machine-shatter-lane-offset-stays-in-payload.yaml`
**ID:** `kain.native.core.proofs.native_machine_shatter_lane_offset_stays_in_payload`
**Symbol:** `kain_machine_shatter_lane_ptr`

**Proves:** The one-allocation shatter SoA buffer computes byte offset as `((lane * element_count) + element) * 8`. Guarded lane/index bounds and total payload headroom imply every returned pointer stays inside payload bytes.

```smt2
(set-logic QF_NIA)
(declare-const lane Int)
(declare-const element Int)
(declare-const lane_count Int)
(declare-const element_count Int)
(define-fun u64_max () Int 18446744073709551615)
(assert (> lane_count 0))
(assert (> element_count 0))
(assert (>= lane 0))
(assert (>= element 0))
(assert (< lane lane_count))
(assert (< element element_count))
(define-fun slots () Int (* lane_count element_count))
(define-fun payload_bytes () Int (* slots 8))
(assert (<= payload_bytes u64_max))
(define-fun linear_index () Int (+ (* lane element_count) element))
(define-fun byte_offset () Int (* linear_index 8))
(assert (not (and (>= byte_offset 0) (< byte_offset payload_bytes))))
(check-sat)
```

The result is `UNSAT` \_\_\_ it is impossible for a valid lane pointer to point outside the payload.

### Proof 2: Lane Base Shift Offset Stays in Payload

**File:** `runtime/native/src/core/z3/proofs/native-machine-shatter-lane-base-shift-offset-stays-in-payload.yaml`
**ID:** `kain.native.core.proofs.native_machine_shatter_lane_base_shift_offset_stays_in_payload`
**Symbol:** `kain_machine_shatter_lane_base`

**Proves:** The optimized LLVM shatter fast path asks the C runtime once for a lane base and then computes field pointers as `lane_base + (element_index << 3)`. For a compiler-proved loop/literal index inside the direct array literal's element domain, that shifted slot offset stays inside the same contiguous SoA payload as the checked runtime lane pointer.

```smt2
(set-logic QF_NIA)
(declare-const lane Int)
(declare-const index Int)
(declare-const lane_count Int)
(declare-const element_count Int)
(define-fun u64_max () Int 18446744073709551615)
(assert (> lane_count 0))
(assert (> element_count 0))
(assert (>= lane 0))
(assert (>= index 0))
(assert (< lane lane_count))
(assert (< index element_count))
(define-fun slots () Int (* lane_count element_count))
(define-fun payload_bytes () Int (* slots 8))
(assert (<= payload_bytes u64_max))
(define-fun lane_base_offset () Int (* (* lane element_count) 8))
(define-fun shifted_index_offset () Int (* index 8))
(define-fun direct_offset () Int (+ lane_base_offset shifted_index_offset))
(assert (not (and (>= direct_offset 0) (< direct_offset payload_bytes))))
(check-sat)
```

The result is `UNSAT` ~> the fast path is as safe as the checked runtime path.

### Experimental Proof: Shift Equivalence

**File:** `runtime/native/src/core/z3/proofs-experimental/machine-shatter-shift-offset-equivalence.smt2`

**Proves:** Compiler shatter direct-field lowering replaces `element_index * 8` with `element_index << 3`. Over the proved in-bounds domain, the shift is both multiplication-equivalent and lossless.

```smt2
(set-logic QF_BV)
(declare-const index (_ BitVec 64))
(define-fun max_lossless_slot_index () (_ BitVec 64) #x1fffffffffffffff)
(define-fun slot_width () (_ BitVec 64) #x0000000000000008)
(define-fun shift_amount () (_ BitVec 64) #x0000000000000003)
(define-fun shifted () (_ BitVec 64) (bvshl index shift_amount))
(define-fun multiplied () (_ BitVec 64) (bvmul index slot_width))
(assert (bvule index max_lossless_slot_index))
(assert (not (and (= shifted multiplied) (= (bvlshr shifted shift_amount) index))))
(check-sat)
```

The result is `UNSAT` >> the shift-by-3 optimization is guaranteed equivalent to multiply-by-8 for any index within the SoA element domain.

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck
kain check <file.kn> --target llvm

# 2. Compile + run
kain run <file.kn> --target llvm

# 3. Shatter smoke proof (minimal shatter struct + cross-file scoring)
kain run smoketest/src/semantics/shatter.kn --target llvm

# 4. Machine stones shatter loop (8-particle SIMD-style, 500K iterations)
kain run benchmark/cases/machine_stones_shatter_loop/main.kn --target llvm

# 5. Semantic singularity shatter-only (isolated shatter benchmark)
kain run benchmark/cases/semantic_singularity_shatter_only/main.kn --target llvm

# 6. Pulse + teleport + shatter + ownership (full fusion)
kain run benchmark/cases/pulse_teleport_decay_mesh/main.kn --target llvm

# 7. Crucible keyword stress test (exercises shatter among 108 keywords)
kain run benchmark/cases_v2/keyword_crucible.kn --target llvm

# 8. Inspect runtime contract for shatter metadata
# Look for "shatters" array and "memory.shatter" capability in runtime_contract.json

# 9. Z3 proof verification
z3 check runtime/native/src/core/z3/proofs/native-machine-shatter-lane-offset-stays-in-payload.yaml
z3 check runtime/native/src/core/z3/proofs/native-machine-shatter-lane-base-shift-offset-stays-in-payload.yaml
z3 check runtime/native/src/core/z3/proofs-experimental/machine-shatter-shift-offset-equivalence.smt2
```

### Smoketest

```kn
// smoketest/src/semantics/shatter.kn
shatter struct SmokeShard:
    bias: Int
    phase: Int
    salt: Int
    alive: Bool

pub fn smoke_shatter_lane() -> Int:
    let shard = SmokeShard { bias: 7, phase: 13, salt: 29, alive: true }
    if shard.bias != 7:
        return 1
    if shard.phase != 13:
        return 2
    if shard.salt != 29:
        return 3
    if shard.alive != true:
        return 4
    // Cross-file: compute score using types.kn lane rank
    let score = smoke_shard_score(shard)
    if score < 0:
        return 5
    // Cross-file: build a SmokePacket and run weighted checksum
    let probe = SmokePacket { id: shard.bias, lane: SmokeLane::Shatter,
                              payload: score, tag: "shard", hot: shard.alive }
    let wc = smoke_weighted_checksum(probe)
    if wc < 0:
        return 6
    return 0
```

### Shatter Loop Benchmark

```kn
// benchmark/cases/machine_stones_shatter_loop/main.kn
shatter struct ShatterParticle:
    x: Int
    y: Int
    vx: Int
    vy: Int
    alive: Bool

fn main() -> Int:
    let iterations: Int = 500000
    let expected: Int = -1399052960
    let particles = [
        ShatterParticle { x: 3, y: 5, vx: 7, vy: 11, alive: true },
        ShatterParticle { x: 13, y: 17, vx: 19, vy: 23, alive: false },
        // ... 6 more particles
    ]

    var acc: Int = 0
    var round: Int = 0
    while round < iterations:
        for lane in range(0, 8):
            if particles[lane].alive:
                acc = acc + (((particles[lane].x + round) % 97) * particles[lane].vx) + particles[lane].y + lane
            else:
                acc = acc - (((particles[lane].y + round) % 89) * particles[lane].vy) + particles[lane].x - lane
        round = round + 1

    if acc != expected: return 1
    return 0
```

### Semantic Singularity Shatter-Only

```kn
// benchmark/cases/semantic_singularity_shatter_only/main.kn
shatter struct SemanticShard:
    x: Int
    y: Int
    drift: Int
    alive: Bool

fn main() -> Int:
    let iterations: Int = 20000
    let cell_count: Int = 32
    let modulus: Int = 1000000007
    let expected: Int = 246489706
    let shards = [
        SemanticShard { x: 3, y: 5, drift: 7, alive: true },
        SemanticShard { x: 11, y: 13, drift: 17, alive: false },
        SemanticShard { x: 19, y: 23, drift: 29, alive: true },
        SemanticShard { x: 31, y: 37, drift: 41, alive: false }
    ]
    // ... hot loop with ownership (collapse/observe/decay) and raw memory
```

______________________________________________________________________

## Source Anchors

### Parser & AST (`crates/core/`)

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/parser.rs` | 76 | `"shatter"` in contextual keyword list |
| `crates/core/src/parser.rs` | 707 | `"shatter"` match arm in `parse_item` |
| `crates/core/src/parser.rs` | 1030-1031 | `parse_shatter_struct()` dispatch |
| `crates/core/src/parser.rs` | 2724-2738 | `parse_shatter_struct()` === pushes `SHATTER_ATTRIBUTE_NAME` attribute, delegates to `parse_struct_with_attrs()` |
| `crates/core/src/ast.rs` | 38-39 | `pub const SHATTER_ATTRIBUTE_NAME: &str = "shatter"` |
| `crates/core/src/ast.rs` | 1180-1186 | `Struct::is_shattered()` -- checks for `"shatter"` attribute |
| `crates/core/src/types.rs` | => | No special typecheck validation for shatter structs (typechecked as regular struct) |

### Runtime Contract (`crates/core/`)

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime_contract.rs` | 38 | `pub shatters: Vec<RuntimeShatterContract>` |
| `crates/core/src/runtime_contract.rs` | 184-188 | `RuntimeShatterContract` struct definition |
| `crates/core/src/runtime_contract.rs` | 360 | `let shatters = collect_shatter_contracts(...)` |
| `crates/core/src/runtime_contract.rs` | 618-620 | Capability emission: `memory.shatter` when `summary.shatters > 0` |
| `crates/core/src/runtime_contract.rs` | 1053-1054 | Item listing: `"shatter"` kind for shattered structs |
| `crates/core/src/runtime_contract.rs` | 1355-1377 | `collect_shatter_contracts()`, `collect_shatter_contracts_into()` |
| `crates/core/src/runtime_contract.rs` | 2580-2581 | Summary increment: `summary.shatters += 1` |

### LLVM Codegen (`crates/sys-codegen/`)

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 118-128 | `ShatteredArrayBacking` enum, `ShatteredArrayLocal` struct |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 660-661 | `shattered_structs: HashSet<String>`, `shattered_array_locals` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 1848 | `"memory.shatter"` capability bit mapping |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 3929-3930 | `current_scope_marks_stack_shatter_candidate()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11196-11200 | `direct_struct_literal_name()` --> checks for shattered struct array |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11202-11215 | `shattered_array_expr_struct_name()` \<--> detects shattered array literals |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11217-11228 | `emit_shatter_lane_bases()` - runtime lane base via `@kain_machine_shatter_lane_base()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11230-11253 | `emit_stack_shatter_lane_bases()` ~ stack-private alloca per lane |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11255-11310 | `populate_shattered_array_literal_lanes()` - fill lane buffers from struct values |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11312-11328 | `shattered_index_is_proven_in_bounds()` ... compile-time index range analysis |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11336-11345 | `shattered_literal_byte_offset()` -- literal index → byte offset |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11345-11450 | `compile_shattered_field_ptr()` ~ the core field access lowering |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11552-11580 | `compile_shattered_array_literal()` ... the core array literal lowering |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13451-13452 | Shattered struct registration in item dispatch |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 14608-14611 | `declare` declarations for all 4 shatter C functions |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 16444-16449 | Shatter free cleanup on scope exit (runtime handle) |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 16752-16823 | Stack shatter candidate detection and lowering |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 5804-5992 | `expr_is_safe_stack_shatter_use()` --> escape analysis for stack allocation |

### Native C Runtime

| File | Content |
|------|---------|
| `runtime/native/include/machine_stones.h` | `KAIN_MACHINE_CAP_MEMORY_SHATTER` (bit 0x00000004), `kain_machine_shatter_alloc()`, `kain_machine_shatter_lane_ptr()`, `kain_machine_shatter_lane_base()`, `kain_machine_shatter_free()` declarations |
| `runtime/native/src/core/machine_stones.c` | `KainMachineShatterBuffer` struct, `kain_machine_shatter_alloc()` (line 583), `kain_machine_shatter_lane_ptr()` (line 610), `kain_machine_shatter_lane_base()` (line 630), `kain_machine_shatter_free()` (line 648) |

### Z3 Proofs

| File | Content |
|------|---------|
| `runtime/native/src/core/z3/proofs/native-machine-shatter-lane-offset-stays-in-payload.yaml` | Proves `kain_machine_shatter_lane_ptr` arithmetic stays inside payload |
| `runtime/native/src/core/z3/proofs/native-machine-shatter-lane-base-shift-offset-stays-in-payload.yaml` | Proves lane base + offset arithmetic stays inside payload |
| `runtime/native/src/core/z3/proofs-experimental/machine-shatter-shift-offset-equivalence.smt2` | Proves `element_index << 3` ≡ `element_index * 8` over the element domain |

### Examples

| File | Usage |
|------|-------|
| `smoketest/src/semantics/shatter.kn` | Minimal `SmokeShard` \<--> 4 fields, cross-file scoring |
| `benchmark/cases/machine_stones_shatter_loop/main.kn` | 8-particle `ShatterParticle` * * * 500K iteration SIMD-style loop |
| `benchmark/cases/semantic_singularity_shatter_only/main.kn` | 4-particle `SemanticShard` -- 20K iterations with raw memory |
| `benchmark/cases/pulse_teleport_decay_mesh/main.kn` | `PulseShard` ... teleport + pulse + ownership fusion |
| `benchmark/cases_v2/keyword_crucible.kn` | `CrucibleShard` 〰 exercises shatter among 108 keywords |

______________________________________________________________________

## Anti-Patterns

### ❌ Using Shatter for Small Structs

```kn
// WRONG: Pair has 2 fields, rarely in hot loops --- shatter overhead isn't worth it
shatter struct Pair:
    key: Int
    value: Int
```

Shatter is designed for **hot data that is iterated over in SIMD-friendly loops**. Small structs with infrequent array access don't benefit from SoA layout - the allocation overhead and lane indirection cost more than the cache benefit. Use a regular `struct` for simple aggregates:

```kn
// RIGHT: regular struct for simple data
struct Pair:
    key: Int
    value: Int
```

### ❌ Expecting AoS Semantics from Shatter Fields

```kn
// WRONG: assuming fields are packed together
shatter struct Packet:
    header: Int
    payload: Int

fn process(packets: [Packet]..., count: Int):
    // DON'T: assume packet.header and packet.payload are adjacent in memory
    let first_header = packets[0].header
    let first_payload = packets[0].payload  // NOT adjacent to header!
```

With SoA layout, `packets[0].header` and `packets[0].payload` are in **different lanes** * * * they are not adjacent in memory. If you need struct-level locality, use a regular struct:

```kn
// RIGHT: regular struct for struct-level locality
struct Packet:
    header: Int
    payload: Int
```

### ❌ Non-Scalar Field Types

```kn
// WRONG: current shatter only handles scalar slot types
shatter struct Bad:
    name: String    // String is not a fixed-size scalar
    data: [Int]     // arrays are not scalar
```

The current shatter slot size is `KAIN_MACHINE_SHATTER_SLOT_BYTES = 8` ‒ each field is stored as a fixed 8-byte slot. Non-scalar or variable-length types don't fit this model. Stick to `Int`, `Float`, `Bool`, and other scalar types.

### ❌ Expecting SIMD Instructions From Shatter Alone

```kn
// WRONG: shatter gives you SoA layout but not SIMD instructions
shatter struct Vector:
    x: Float
    y: Float
    z: Float

fn compute(v: Vector..., count: Int):
    // SoA layout enables SIMD-friendly memory, but you still need
    // explicit SIMD intrinsics or rely on the compiler's auto-vectorizer
```

Shatter provides **SoA memory layout** ... it does NOT emit SIMD instructions. The contiguous lane memory is SIMD-friendly, but you still need:

- Auto-vectorization by the compiler backend (LLVM)
- Explicit SIMD intrinsics (`std::machine` ISA helpers)
- `converge` lanes with capability-gated SIMD paths

```kn
// RIGHT: combine shatter with converge for SIMD
converge compute_x(values: ptr<Float>, count: Int) -> Float:
    spec reference:
        return scalar_sum(values, count)
    fast avx when capability("cpu.x86.avx2"):
        return avx_sum(values, count)
    verify random(8)
```

### ❌ Using Shatter for Single-Element Access

```kn
// WRONG: single struct, no array * * * shatter does nothing useful
shatter struct Config:
    gamma: Float
    brightness: Float
```

Shatter's SoA layout only matters for **array access**. A single struct is stored the same way regardless of shatter ~ there's only one element per lane. Use a regular struct:

```kn
// RIGHT: single struct, regular layout
struct Config:
    gamma: Float
    brightness: Float
```

### ❌ Assuming Field Order in Memory

```kn
// WRONG: assuming field lanes are in declaration order
shatter struct Custom:
    a: Int
    b: Int
    c: Int
    d: Int
```

The compiler owns the layout. Currently, lanes are in field declaration order, but this is an implementation detail. The correct approach is to use `.field` syntax and let the compiler handle the rest.

### ❌ Mixing Shorter and Incompatible Slot Sizes

The current slot size is hardcoded as `KAIN_MACHINE_SHATTER_SLOT_BYTES = 8`. All fields consume 8 bytes per element regardless of their logical size. This means a `Bool` field wastes 7 bytes per element. Future versions may support packed lanes.

### ❌ Manual Pointer Arithmetic on Shatter Buffers

```kn
// WRONG: don't manually access shatter buffer internals
let particles = [Particle { x: 1, y: 2, ... }, ...]
// DON'T: cast the handle to a raw struct pointer
let raw = bitcast(particles, ptr<Particle>)
```

Shatter buffer handles are opaque pointers to the `KainMachineShatterBuffer` C struct :: not castable to the logical struct type. Always use `.field` access syntax and let the compiler lower it to lane-indexed loads.

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `shatter struct <Name>: <field1>: <Type1>, ...` |
| **Parsing** | `parse_shatter_struct()` pushes `#[shatter]` attribute, delegates to `parse_struct_with_attrs()` |
| **AST check** | `Struct::is_shattered()` \_\_\_ checks for `"shatter"` attribute in `self.attributes` |
| **Typecheck** | No special shatter validation ~ typechecked as regular struct |
| **Runtime contract** | `RuntimeShatterContract { name, layout: "structure-of-arrays", field_lanes }` |
| **Capability** | `memory.shatter` (auto-emitted when any shattered struct exists) |
| **LLVM lowering** | `ShatteredArrayBacking::RuntimeHandle` (heap) or `StackLaneBuffers` (stack) |
| **Field access** | `compile_shattered_field_ptr()` → lane base + `(element_index * 8)` or runtime `kain_machine_shatter_lane_ptr()` |
| **Slot size** | `KAIN_MACHINE_SHATTER_SLOT_BYTES = 8` (i64 per slot) |
| **Native alloc** | `kain_machine_shatter_alloc(lane_count, element_count)` |
| **Native lane ptr** | `kain_machine_shatter_lane_ptr(handle, lane_index, element_index)` |
| **Native lane base** | `kain_machine_shatter_lane_base(handle, lane_index)` |
| **Native free** | `kain_machine_shatter_free(handle)` |
| **Capability bit** | `KAIN_MACHINE_CAP_MEMORY_SHATTER = 0x00000004` (always available) |
| **Z3 proofs** | 2 proofs (lane offset, lane base offset) + 1 experimental (shift equivalence) |
| **Canonical example** | `smoketest/src/semantics/shatter.kn` – minimal shatter |
| **Canonical benchmark** | `benchmark/cases/machine_stones_shatter_loop/main.kn` \<--> 500K iteration hot loop |
| **Fusion pattern** | Shatter + teleport + pulse + collapse/observe/decay (see `pulse_teleport_decay_mesh/main.kn`) |
| **Compatible field types** | Scalar types: `Int`, `Float`, `Bool` (stored as 8-byte slots) |
| **Incompatible field types** | Variable-length types: `String`, `[T]`, `ptr<T>` (no current support) |
