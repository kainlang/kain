# World — Compiler-Owned State Authority

**Status:** Research synthesis, 2026-06-08
**Based on:** `crates/core/src/ast.rs`, `crates/core/src/parser.rs`, `crates/core/src/types.rs`, `crates/core/src/runtime_contract.rs`, `crates/core/src/runtime.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `runtime/native/include/machine_stones.h`, `stdlib/intent.kn`, `smoketest/src/semantics/world.kn`, `benchmark/cases_v2/fusion_chain.kn`, `blades/experiments/convergence/src/world.kn`, `blades/python/24_tet/src/resonate_py_effects.kn`, GLOSSARY.MD, RULEBOOK.MD, CATALOG.MD

______________________________________________________________________

## Table of Contents

1. [What is World?](#what-is-world)
1. [The Core Idea — Compiler-Owned State Authority](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [How It Works — Full Pipeline](#how-it-works)
1. [Surface Projections](#surface-projections)
1. [The World Patterns](#the-world-patterns)
1. [World + Entangle](#world--entangle)
1. [Under-Utilized Ideas — Creative Semantic Abuse](#under-utilized-ideas)
1. [Combining with Other Keywords](#combining-with-other-keywords)
1. [Stdlib Telemetry API](#stdlib-telemetry-api)
1. [Runtime Contract Metadata](#runtime-contract-metadata)
1. [LLVM Codegen](#llvm-codegen)
1. [Native C Runtime](#native-c-runtime)
1. [Z3 Proofs](#z3-proofs)
1. [Validation Ladder](#validation-ladder)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)
1. [Source Anchors](#source-anchors)

______________________________________________________________________

## What is World?

`world` is a **top-level declaration** in Kain that establishes a **compiler-owned state graph with surfaces**. It is the foundation of LAYER 1: STATE AUTHORITY on Kain's decision ladder. A world owns named state slots with types and initializers, and can project surfaces (`native_ui`, `viewport3d`, `web`, `ue5`) that bind state to rendering or UI targets.

```kn
world SimulationState:
    state frame: Int = 0
    state particles: Int = 65536
    state epoch: Int = 0
    surface native_ui => SimulationPanel
```

A world is **not a global variable**. It is a **compiler-owned state authority** — the compiler, not the programmer, owns the lifecycle, the entangle coupling graph, the patch journal integration, and the surface projection system. When you write `world`, you are handing state sovereignty to the compiler's semantic graph.

Worlds are the backbone of Kain's state semantics. Every other LAYER 1-7 construct either reads from a world (`patch`, `resonate`, `pulse`), writes to a world (`patch`, `teleport`), couples worlds (`entangle`), or projects world state (`surface`). Without worlds, there is no state graph — only unmanaged variables.

______________________________________________________________________

## The Core Idea — Compiler-Owned State Authority

### What World Is NOT

- ❌ Not a **global variable** — you don't `let mut` a world; the compiler owns the allocation and initialization. Worlds are not `var` declarations.
- ❌ Not a **plain struct** — a `struct` has no surface projection, no entangle participation, no runtime contract, no LLVM init guard. A world is a first-class compiler semantic.
- ❌ Not a **singleton** — you can have multiple worlds, each with distinct state and surfaces. There is no single-world restriction.
- ❌ Not an **actor** — actors are concurrent message-processing units. Worlds are synchronous state authorities. Actors read and write worlds; they are not worlds.
- ❌ Not a **database** — there's no persistence, no query language, no ACID transactions. The world is an in-memory state graph with compiler-owned coupling semantics.
- ❌ Not a **module** — `world` does not create a namespace or scope in the module system. It's a declared entity at the file level alongside `fn`, `actor`, `struct`, etc.
- ❌ Not a **dictionary / HashMap** — state slots are typed, fixed at compile time, and validated by the typechecker. You cannot dynamically add fields to a world.

### What World IS

- ✅ A **compiler-owned state authority** — the compiler registers, initializes, and tracks the world's lifecycle. The programmer declares what state exists; the compiler ensures it exists correctly.
- ✅ A **named container of typed state slots** — each `state field: Type = initial` is a typed, initialized field validated at compile time.
- ✅ A **surface projection host** — every world must declare at least one surface (`native_ui`, `viewport3d`, `web`, `ue5`) that binds its state to a component or render target.
- ✅ An **entangle participant** — world fields can be coupled to fields in other worlds via `entangle`, creating a compiler-owned propagation graph.
- ✅ A **patch target** — `patch` declarations can mutate world fields, journal the mutation, and trigger entangle propagation.
- ✅ A **resonate root** — `resonate` handlers can be attached to world state slots, firing on mutation.
- ✅ A **teleport boundary** — `teleport` expressions can move values from one world to another via a typed bus.
- ✅ A **runtime contract entity** — worlds appear in the program's runtime contract bundle with all state slots and surfaces enumerated.
- ✅ An **LLVM global struct** — worlds compile to LLVM globals with lazy-init guards and getelementptr field access.
- ✅ A **capability manifest entry** — the program's capabilities include all surface kinds used by its worlds.

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|-----------|-------------------|-------------------|
| `world` | "Named state container with surfaces" | **Compiler-owned state authority** — a typed, projected, entangle-capable, patchable, LLVM-backed state graph node. The atom of Kain's state semantics. |

Every other compiler-owned semantic construct relates to world as either:

- **A mutation mechanism on worlds** (patch, resonate, teleport → world)
- **A coupling mechanism between worlds** (entangle ↔ world)
- **A temporal driver of worlds** (pulse → world)
- **A pipeline that processes world state** (orchestrate → world)
- **A projection of world state** (surface → component)
- **A constraint on world state** (law → world fields)

Without `world`, none of the other constructs have anything to operate on.

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
[pub] world <Name>:
    state <field_name>: <Type> = <initial_expr>
    ...
    surface <kind> => <ComponentExpr>
    ...
```

Where:

- `world` is a contextual keyword (recognized in item-start position)
- `<Name>` is the world identifier (used in dotted paths like `WorldName.field`)
- `state` introduces a state slot with name, type, and initializer
- `surface` introduces a surface projection with a kind and a component expression
- At least one `surface` declaration is **required** (enforced by the typechecker)
- `pub` controls visibility (world is visible to other modules if `pub`)

### Concrete Examples

```kn
// Minimal world — one state slot, one surface
world Counter:
    state value: Int = 0
    surface native_ui => CounterPanel

// Multi-field world with different types
world Simulation:
    state frame: Int = 0
    state tick_rate: Int = 60
    state running: Bool = true
    state label: String = "simulation"
    surface native_ui => SimulationPanel

// World with multiple surfaces
world Telemetry:
    state best: Int = 0
    state frame: Int = 0
    surface native_ui => TelemetryPanel
    surface web => TelemetryPanel

// World with pointer state (raw buffer)
world Experiment:
    state maze: ptr<Int> = int_to_ptr(0, "Int")
    state width: Int = 0
    state height: Int = 0
    state cell_count: Int = 0
    surface native_ui => Visualizer

// Public world (visible across modules)
pub world SharedState:
    state epoch: Int = 0
    surface native_ui => SharedPanel
```

### Grammar (from parser.rs)

```text
world <name>:
    <indent>
    (<state_slot> | <surface_projection>)*
    <dedent>

state_slot:
    "state" <ident> ":" <type> "=" <expr>

surface_projection:
    "surface" ("native_ui" | "viewport3d" | "web" | "ue5") "=>" <expr>
```

The parser (`parse_world()` at line ~2035 in `crates/core/src/parser.rs`):

1. Expects contextual keyword `world` (registered in `is_contextual_item_start_name` at line 702)
1. Parses the world name as an identifier
1. Expects `:` then an indented block
1. Within the block, dispatches on `TokenKind::State` → `parse_world_state_slot()` or on the ident `"surface"` → `parse_world_surface_projection()`
1. Anything else produces an error: `"Expected 'state' or 'surface' inside world block"`
1. Returns `Item::World(WorldDef { name, states, surfaces, visibility, attributes, span })`

### AST Types (from ast.rs)

```rust
pub struct WorldDef {
    pub name: String,
    pub states: Vec<WorldStateSlot>,
    pub surfaces: Vec<WorldSurfaceProjection>,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

pub struct WorldStateSlot {
    pub name: String,
    pub ty: Type,
    pub initial: Expr,
    pub span: Span,
}

pub struct WorldSurfaceProjection {
    pub kind: WorldSurfaceKind,
    pub expr: Expr,
    pub span: Span,
}

pub enum WorldSurfaceKind {
    NativeUi,
    Viewport3d,
    Web,
    Ue5,
}
```

______________________________________________________________________

## How It Works — Full Pipeline

### Source to Runtime Flow

```
.kn world declaration
  → parser.rs: parse_world()
    → WorldDef { name, states, surfaces } in AST
  → types.rs: check_world()
    → Resolves state slot types
    → Resolves initializer expressions (must match state type)
    → Checks at least one surface exists
    → Checks no duplicate surface kinds
    → Validates surface projection expressions
    → Registers world as a named struct type and global in the type environment
    → TypedWorld { ast: WorldDef }
  → runtime_contract.rs: collect_world_contracts()
    → RuntimeWorldContract { name, state_slots, surfaces }
  → runtime.rs: register_world_value()
    → Evaluates each state initializer
    → Stores as Arc<RwLock<HashMap<String, Value>>>
    → Registers as a named value in the interpreter's environment
  → codegen_llvm/mod.rs: register_world_type_and_global()
    → Emits LLVM struct type: %WorldName = type { i64, i64, ... }
    → Emits LLVM global: @__kain_world_WorldName
    → Emits init flag: @__kain_world_init_flag_WorldName
    → Registers WorldGlobalInfo { global_symbol, init_flag_symbol, init_fn_name }
  → codegen_llvm/mod.rs: compile_world_initializer()
    → define void @__kain_init_world_WorldName()
    → Guard: if already initialized, skip
    → For each state slot: GEP → store initial value
    → Set init flag to 1
  → codegen_llvm/mod.rs: emit_machine_stones_entry_preamble()
    → Calls init functions for all worlds at program start
```

### At Compile Time (Typechecker)

The typechecker `check_world()` in `types.rs` performs:

1. **State slot type resolution** — each `state ty` is resolved via `resolve_type_in_env()`
1. **Initializer type checking** — each `initial` expression type is inferred and compared against the declared state type via `ensure_type_compatible()`
1. **Surface requirement** — if no surfaces are declared, the typechecker produces a diagnostic with:
   - Code: `TypeWorldMissingSurface`
   - Message: `"world 'Name' must declare at least one surface"`
   - Note: `"Worlds currently need at least one surface projection so they can materialize into a runtime-facing view."`
   - Help: `"Add a line like 'surface native_ui => MyPanel' inside the world body."`
1. **Duplicate surface detection** — a `HashSet<WorldSurfaceKind>` tracks seen surface kinds; duplicate kinds produce: `"world 'Name' declares duplicate 'native_ui' surface"`
1. **Surface validation** — `check_world_surface_projection()` validates the expression based on the surface kind:
   - `NativeUi` / `Web` → expects a component identifier or call expression
   - `Viewport3d` / `Ue5` → expects an identifier or string literal
1. **Global registration** — the world name is registered both as a `ResolvedType::Struct` and as a `global_origins` entry (kind: `"world"`), making it available for `entangle`, `resonate`, and field access.

```rust
fn check_world(env: &mut TypeEnv, world: &WorldDef) -> KainResult<TypedWorld> {
    let old_world = env.in_world;
    env.in_world = true;
    let res = (|| {
        let mut state_types = HashMap::new();
        let mut seen_surface_kinds = HashSet::new();
        for state in &world.states {
            let resolved_ty = resolve_type_in_env(env, &state.ty)?;
            let initial_ty = infer_expr_type(env, &state.initial, None)?;
            ensure_type_compatible(env, &resolved_ty, &initial_ty,
                state.initial.span(), "world state initializer")?;
            state_types.insert(state.name.clone(), resolved_ty);
        }
        let world_ty = ResolvedType::Struct(world.name.clone(), state_types);
        env.types.insert(world.name.clone(), world_ty.clone());
        env.define_global(world.name.clone(), world_ty);
        // Surface requirement, duplicate detection, validation...
        Ok(TypedWorld { ast: world.clone() })
    })();
    env.in_world = old_world;
    res
}
```

### At Runtime (Interpreter)

The interpreter runtime (`runtime.rs`) handles worlds through:

- **`register_world_value()`** — on program load, evaluates each state slot's initializer and stores the resulting `HashMap<String, Value>` behind an `Arc<RwLock<>>`. The world value is then registered as `Value::Struct(world_name, world_value)` in the environment, making world fields accessible via `WorldName.field_name` dotted access.

```rust
fn register_world_value(&mut self, world: &WorldDef) -> KainResult<()> {
    let mut state_values = HashMap::new();
    for state in &world.states {
        let value = eval_expr(self, &state.initial)?;
        state_values.insert(state.name.clone(), value);
    }
    let world_value = Arc::new(RwLock::new(state_values));
    self.worlds.insert(world.name.clone(), world_value.clone());
    self.define(world.name.clone(), Value::Struct(world.name.clone(), world_value));
    Ok(())
}
```

- **World field reads** — the interpreter resolves `WorldName.field` by looking up the world value, acquiring the read lock, and retrieving the field from the HashMap.
- **World field writes** — the interpreter resolves the world, acquires the write lock, mutates the field, and checks entangle write permissions before allowing the write.

### At Runtime (LLVM Codegen)

The LLVM codegen handles worlds through two phases:

#### Phase 1: Type and Global Registration (`register_world_type_and_global()`)

During LLVM module setup, each world produces:

```llvm
; LLVM struct type for the world
%WorldName = type { i64, i8, ... }          ; state slot types

; LLVM global for the world's state
@__kain_world_WorldName = global %WorldName zeroinitializer

; LLVM init flag (lazy initialization guard)
@__kain_world_init_flag_WorldName = global i1 0

; Init function declaration
declare void @__kain_init_world_WorldName()
```

The struct field order matches the `state` declarations in source order. Field indices are computed by `field_index(world_name, field_name)` which looks up the world's struct definition.

#### Phase 2: Initializer Function (`compile_world_initializer()`)

Each world gets a lazy init function that is called before any field access:

```llvm
define void @__kain_init_world_WorldName() {
entry:
  %0 = load i1, i1* @__kain_world_init_flag_WorldName
  br i1 %0, label %already_init, label %init

init:
  %1 = getelementptr inbounds %WorldName, %WorldName* @__kain_world_WorldName, i32 0, i32 0
  store i64 0, i64* %1     ; first state slot initial value
  ; ... more GEP + store for each slot ...
  store i1 1, i1* @__kain_world_init_flag_WorldName
  br label %already_init

already_init:
  ret void
}
```

#### Phase 3: Entry Preamble

During program startup, `emit_machine_stones_entry_preamble()` calls all world init functions. When any referencing code loads a world field, the init function is called on first access via the lazy init guard pattern.

______________________________________________________________________

## Surface Projections

### The Four Surface Kinds

| Kind | Key | Expected Expression | Use Case |
|------|-----|--------------------|----------|
| `native_ui` | `WorldSurfaceKind::NativeUi` | Component identifier or call | Native desktop/mobile UI |
| `web` | `WorldSurfaceKind::Web` | Component identifier or call | Web browser rendering |
| `viewport3d` | `WorldSurfaceKind::Viewport3d` | Identifier or string literal | 3D viewport / scene graph |
| `ue5` | `WorldSurfaceKind::Ue5` | Identifier or string literal | Unreal Engine 5 integration |

### How Surfaces Bind State to Rendering

A surface projection declares that a world's state should be materialized into a component or render target. The typechecker validates the expression based on the surface kind:

```kn
world TelemetryWorld:
    state best_distance: Int = 0
    state frame: Int = 0
    surface native_ui => TelemetryPanel          // Native UI component
    surface web => TelemetryPanel                 // Same component, web projection

world Scene:
    state camera_x: Float = 0.0
    state camera_y: Float = 0.0
    state mesh_count: Int = 0
    surface viewport3d => "main_viewport"           // 3D viewport by name
    surface ue5 => "/Game/Maps/MainMap"             // UE5 map path
```

For `native_ui` and `web` surfaces, the expression must be a component identifier or a component call. The component receives the world's state implicitly — world field values are accessible inside `render` via `WorldName.field` dotted paths.

```kn
component TelemetryPanel():
    render <panel title="Telemetry">
        <text value={TelemetryWorld.best_distance} />
        <text value={TelemetryWorld.frame} />
    </panel>
```

For `viewport3d` and `ue5` surfaces, the expression is a string literal or identifier that identifies the rendering target. The runtime uses this to bind the world's state to the 3D engine or game engine.

### Surface Capabilities

Each surface kind implies a capability that appears in the program's runtime contract:

| Surface Kind | Implied Capability |
|-------------|-------------------|
| `native_ui` | `ui.native` |
| `web` | `ui.web` |
| `viewport3d` | `gpu.viewport` |
| `ue5` | `editor.ue5` |

The program's capability manifest is derived from the union of all surface kinds across all declared worlds.

### Surface Requirement — Why It Exists

The typechecker **requires** at least one surface per world. This is not an arbitrary restriction — it ensures that every world has a materialization path to a runtime-facing view. Without a surface, a world would be an orphan state container with no observable output, which violates Kain's principle that all state should be projectable.

The error message when missing surfaces:

```
Error: world 'OrphanWorld' must declare at least one surface
  ┌─ main.kn:2:1
  │
2 │ world OrphanWorld:
  │ ^^^^ world has no surface projections
  │
  = note: Worlds currently need at least one surface projection so they
          can materialize into a runtime-facing view.
  = help: Add a line like `surface native_ui => MyPanel` inside the world body.
```

______________________________________________________________________

## The World Patterns

### Pattern 1: Single-World Application

The simplest pattern — one world, one surface, all state in one place:

```kn
component App():
    render <window title="My App">
        <text value={AppWorld.counter} />
        <button label="Increment" onclick={on_increment} />
    </window>

world AppWorld:
    state counter: Int = 0
    state label: String = "hello"
    surface native_ui => App

patch increment(world: AppWorld) -> Int:
    world.counter = world.counter + 1
    return world.counter
```

**When to use:** Small applications, prototypes, single-threaded state, no need for read-only mirrors or multi-target surfaces.

### Pattern 2: Multi-World Decomposition

Split state across worlds by domain concern:

```kn
world AudioState:
    state master_volume: Int = 80
    state mix: Int = 500
    surface native_ui => AudioPanel

world VideoState:
    state brightness: Int = 500
    state contrast: Int = 500
    surface native_ui => VideoPanel

world InputState:
    state mouse_x: Int = 0
    state mouse_y: Int = 0
    state keys: Int = 0
    surface native_ui => InputPanel
```

**When to use:** When state domains are independent, need separate surfaces, or benefit from separate entangle/resonate graphs.

### Pattern 3: Authority + Mirror (Dual-World Pattern)

The canonical pattern for Kain applications — one world owns mutable state (the authority), another receives it via entangle (the mirror):

```kn
component RenderPanel():
    render <panel title="State">
        <text value={Authority.signal} />
        <text value={Mirror.signal_copy} />
    </panel>

world Authority:
    state signal: Int = 0
    state epoch: Int = 0
    surface native_ui => RenderPanel

world Mirror:
    state signal_copy: Int = 0
    state epoch_copy: Int = 0
    surface web => RenderPanel

entangle Authority.signal <-> Mirror.signal_copy with single_writer
entangle Authority.epoch <-> Mirror.epoch_copy with single_writer
```

**Why this pattern:**

- **Authority** owns mutable state, has a `native_ui` surface for the main thread
- **Mirror** receives state via entangle propagation, exposes a `web` surface for read-only introspection
- The compiler-owned observer graph handles propagation at zero cost
- Mirror writes are rejected at compile time by the typechecker

**When to use:** Any application where state is mutated in one place and observed in another. This is the **default pattern** for all non-trivial Kain programs.

### Pattern 4: Experiment Telemetry World

From the convergence rat experiment (`blades/experiments/convergence/src/world.kn`):

```kn
component SpeculativeScentVisualizer():
    render <panel title="Convergence // RatTelemetry" />

world RatTelemetry:
    state maze: ptr<Int> = int_to_ptr(0, "Int")
    state pure_trail: ptr<Int> = int_to_ptr(0, "Int")
    state greedy_trail: ptr<Int> = int_to_ptr(0, "Int")
    state chaos_trail: ptr<Int> = int_to_ptr(0, "Int")
    state width: Int = 0
    state height: Int = 0
    state cell_count: Int = 0
    state trail_capacity: Int = 0
    state start_index: Int = 0
    state target_index: Int = 0
    state frame: Int = 0
    state best_distance: Int = 0
    state best_lane: Int = 0
    state pure_count: Int = 0
    state greedy_count: Int = 0
    state chaos_count: Int = 0
    state frame_signature: Int = 0
    state status: Int = 0
    surface native_ui => SpeculativeScentVisualizer
```

**Characteristics:**

- Holds raw buffer pointers (`ptr<Int>`) for maze trails — NOT entangled
- Holds scalar telemetry counters that ARE entangled to a mirror
- The world acts as the central aggregator for experiment data
- The `native_ui` surface binds to a visualizer component

**When to use:** Data-intensive applications where some state (buffers) should stay local while other state (counters) is mirrored.

### Pattern 5: Effects Engine World (Multi-Parameter Authority)

From the 24-tet audio effects engine (`blades/python/24_tet/src/resonate_py_effects.kn`):

```kn
pub world ResonatePyFxWorld:
    // LFO 1 — primary modulation oscillator
    state lfo1_phase: Int = 0
    state lfo1_rate: Int = 23
    state lfo1_shape: Int = FX_LFO_SIN
    state lfo1_depth: Int = 300

    // LFO 2 — secondary modulation oscillator
    state lfo2_phase: Int = 16384
    state lfo2_rate: Int = 7
    state lfo2_shape: Int = FX_LFO_TRI
    state lfo2_depth: Int = 200

    // Chorus — modulated short delay
    state chorus_mix: Int = 0
    state chorus_delay_ms: Int = 18
    state chorus_rate: Int = 15
    state chorus_depth: Int = 200
    state chorus_feedback: Int = 150

    // Delay / Echo
    state delay_mix: Int = 0
    state delay_time_ms: Int = 320
    state delay_feedback: Int = 280

    // Reverb — simple decay diffusion
    state reverb_mix: Int = 0
    state reverb_decay: Int = 350
    state reverb_damping: Int = 400
    state reverb_diffusion: Int = 300

    // Distortion — waveshaping drive
    state distortion_drive: Int = 0
    state distortion_tone: Int = 500
    state distortion_output: Int = 500

    // Filter — resonant low-pass parameter control
    state filter_cutoff: Int = 1000
    state filter_resonance: Int = 0
    state filter_env_mod: Int = 0

    // Tremolo — amplitude modulation
    state tremolo_depth: Int = 0
    state tremolo_rate: Int = 20
    state tremolo_shape: Int = FX_LFO_SIN

    // Modulation routing matrix — 4 routes
    state mod_route_0_source: Int = FX_MOD_SOURCE_LFO1
    state mod_route_0_target: Int = 2
    state mod_route_0_amount: Int = 400
    state mod_route_1_source: Int = FX_MOD_SOURCE_LFO2
    state mod_route_1_target: Int = 0
    state mod_route_1_amount: Int = 300
    state mod_route_2_source: Int = FX_MOD_SOURCE_LFO1
    state mod_route_2_target: Int = 5
    state mod_route_2_amount: Int = 500
    state mod_route_3_source: Int = FX_MOD_SOURCE_LFO2
    state mod_route_3_target: Int = 3
    state mod_route_3_amount: Int = 200

    // Global
    state fx_bypass: Int = 0
    state fx_epoch: Int = 0
    state fx_frame: Int = 0
    surface native_ui => ResonatePyPanel

pub world ResonatePyFxMirror:
    state lfo1_phase_copy: Int = 0
    state lfo1_rate_copy: Int = 23
    state lfo2_phase_copy: Int = 16384
    state lfo2_rate_copy: Int = 7
    state chorus_mix_copy: Int = 0
    state chorus_delay_ms_copy: Int = 18
    state delay_mix_copy: Int = 0
    state delay_time_ms_copy: Int = 320
    state reverb_mix_copy: Int = 0
    state distortion_drive_copy: Int = 0
    state filter_cutoff_copy: Int = 1000
    state tremolo_depth_copy: Int = 0
    state fx_epoch_copy: Int = 0
    state fx_frame_copy: Int = 0
    surface web => ResonatePyPanel
```

**Characteristics:**

- 30+ state slots in the authority world
- Only a subset are entangled to the mirror (useful for introspection)
- Organized by semantic section (LFO, Chorus, Delay, Reverb, etc.)
- The world is the single source of truth for all effect parameters
- Mirrors only copy what's needed for web introspection, not every field

**When to use:** Complex systems with many related parameters that need structured grouping, selective mirroring, and domain-organized state.

### Pattern 6: Frame State World (Fusion Chain)

From the fusion chain benchmark (`benchmark/cases_v2/fusion_chain.kn`):

```kn
world FusionAuthority:
    state signal: Int = 1
    state tick: Int = 0
    state ack_count: Int = 0
    state shadow: Int = 0
    state last_old: Int = 0
    state last_new: Int = 0
    state teleport_landing: Int = 0
    state pulse_ticks: Int = 0
    surface web => FusionChainPanel

world FusionMirror:
    state signal_copy: Int = 1
    state tick_copy: Int = 0
    state ack_count_copy: Int = 0
    state pulse_ticks_copy: Int = 0
    surface web => FusionChainPanel
```

**Characteristics:**

- Small number of fields that are updated every frame
- Every authority field is entangled to a mirror field
- The world is the hub for a full causal chain: patch → resonate → entangle → pulse → actor → teleport
- State is reset between benchmark cases via a `patch`

**When to use:** Game loops, realtime systems, frame-driven state, where every field participates in the causal chain.

______________________________________________________________________

## World + Entangle

Worlds and entangle are inseparable. A world without entangle is an island; a world with entangle is part of a graph.

### The Entangle Contract

Every entangle declaration references world fields by their dotted path:

```kn
entangle Authority.signal <-> Mirror.signal_copy with single_writer
```

The left and right endpoints must be dotted paths with at least 2 segments: `WorldName.field`. The typechecker resolves them by looking up the world in the global origins table, then walking the field path.

### What Entangle Enables for Worlds

1. **Authority-Mirror coupling** — one world writes, the other receives. The mirror is guaranteed fresh by the propagation graph.
1. **Multi-surface projection** — authority has `native_ui`, mirror has `web`. Same state, different render targets.
1. **Read-safe observation** — actors and components can read from mirrors without worrying about write conflicts.
1. **Zero-cost propagation** — the compiler emits GEP + store + telemetry call at the LLVM level. No heap observer, no callback, no message queue.

### What Entangle Restricts on Worlds

Under `single_writer` policy, mirror world fields cannot be written directly:

```kn
Authority.signal = 42    // ✅ allowed (authority)
Mirror.signal_copy = 42  // ❌ rejected at compile time (mirror)
```

The typechecker enforces this through the entangle endpoint registry — each endpoint is tracked, and mirror writes are denied.

### The Canonical Dual-World Pattern

```kn
world Authority:
    state value: Int = 0
    surface native_ui => Panel

world Mirror:
    state value_copy: Int = 0
    surface web => Panel

entangle Authority.value <-> Mirror.value_copy with single_writer
```

For full details on entangle semantics, policies, telemetry, and patterns, see `X:/docs/ENTANGLE.MD`.

______________________________________________________________________

## Under-Utilized Ideas — Creative Semantic Abuse

### 1. World as Telemetry Aggregator with Selective Entangle

The convergence experiment world holds `ptr<Int>` raw buffers that are NOT entangled, alongside scalar counters that ARE entangled. This selective coupling is under-utilized — most examples entangle everything 1:1.

**Under-explored:** Entangling only the "safe for display" subset while keeping performance-critical or ownership-sensitive state local:

```kn
world Engine:
    state vertex_buffer: ptr<Float> = int_to_ptr(0, "Float")   // NOT entangled
    state index_buffer: ptr<Int> = int_to_ptr(0, "Int")        // NOT entangled
    state triangle_count: Int = 0                                // entangled
    state fps: Int = 60                                          // entangled
    surface native_ui => DebugPanel

world DebugMirror:
    state triangle_count_copy: Int = 0
    state fps_copy: Int = 60
    surface web => DebugPanel

entangle Engine.triangle_count <-> DebugMirror.triangle_count_copy with single_writer
entangle Engine.fps <-> DebugMirror.fps_copy with single_writer
```

### 2. World as a Multi-Surface Hub with Different Component Renderers

Currently, mirror worlds typically use the same component as the authority. You could have authority and mirror project to completely different components:

```kn
world Authority:
    state data: Int = 0
    surface native_ui => EditorPanel

world Mirror:
    state data_copy: Int = 0
    surface web => ReadOnlyPanel     // different component!
    surface native_ui => DashboardPanel  // also different!

entangle Authority.data <-> Mirror.data_copy with single_writer
```

**Under-explored:** The authority is for editing, the mirror is for display — with entirely different component trees.

### 3. World as a Multi-Stage Pipeline State Boundary

An `orchestrate` pipeline can use different worlds as state boundaries between stages. The output of one stage writes to world A; the next stage reads from world A's mirror:

```kn
orchestrate fx_pipeline():
    stage compute: cpu fx_process(authority) ...
    stage propagate: law fx_signal_in_bounds(authority.signal)
    // After the pipeline, authority.signal was updated
    // entangle auto-propagates to mirror
    // A separate pipeline can read from mirror safely
```

**Under-explored:** Orchestrate stages that explicitly target authority worlds for output and mirror worlds for input, creating a pipeline-stage boundary that's compiler-owned.

### 4. Dynamic Surface Switching (If Implemented)

Currently, surfaces are static — declared at compile time. A world with three surfaces always has three surfaces. There's no mechanism to add or remove surfaces at runtime.

**Under-explored (if implemented):** World surfaces as a compile-time selection mechanism where `capability("ui.web")` or `target("wasm")` determines which surfaces are active.

### 5. World as a Namespace for State Versioning

Worlds with epoch counters can serve as a state versioning mechanism:

```kn
world VersionedState:
    state data: Int = 0
    state epoch: Int = 0
    surface native_ui => Panel

resonate VersionedState.data bump_epoch 0ms:
    // Every mutation automatically bumps the epoch
    VersionedState.epoch = VersionedState.epoch + 1
```

**Under-explored:** Using epoch counters as a global ordering primitive for causal chains — the epoch tells downstream consumers which version of the state they're seeing.

### 6. World as a Firewall for Raw Ownership

Worlds can hold raw pointers (`ptr<Int>`), which means they can serve as a "safe zone" for unsafe memory — the world owns the pointer, and all access goes through the world's patch/resonate surface:

```kn
world BufferManager:
    state buffer: ptr<Int> = int_to_ptr(0, "Int")
    state size: Int = 0
    state epoch: Int = 0
    surface native_ui => BufferPanel

patch allocate_buffer(world: BufferManager, size: Int) -> Int:
    world.buffer = alloc_zeroed(size, "Int")
    world.size = size
    world.epoch = world.epoch + 1
    return 0

fn read_buffer(world: BufferManager, index: Int) -> Int with Unsafe:
    return observe world.buffer:
        mem_load(ptr_offset(world.buffer, index, "Int"), "Int")
```

**Under-explored:** Using worlds as the exclusive API surface for raw memory operations — all `collapse`, `observe`, `decay` on pointers happen through world state rather than as free functions.

### 7. World as Cross-Language State Bridge

In a Python + Kain hybrid, the world serves as the Kain-side state authority that Python reads/writes via the `import` bridge:

```kn
world PythonInterop:
    state tensor_shape: Int = 0
    state tensor_data: ptr<Float> = int_to_ptr(0, "Float")
    state epoch: Int = 0
    surface native_ui => InteropPanel
```

**Under-explored:** Worlds explicitly designed as the semantic boundary between Kain and Python — Python owns the ML computation, Kain owns the state authority and UI projection.

______________________________________________________________________

## Combining with Other Keywords

### World + Patch

Patches are the canonical mutation mechanism for world state. Every `patch` that assigns to a world field triggers entangle propagation for any entangled fields:

```kn
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value          // triggers entangle → Mirror.signal_copy
    authority.epoch = authority.epoch + 1
    return authority.signal
```

The patch system also journals the mutation history:

```kn
let journal_len = patch_journal_count()
```

### World + Law

Laws can read world state directly and constrain what values are valid for world fields:

```kn
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

law world_in_invariant_state(world: Authority) -> Bool:
    return signal_in_bounds(world.signal)
```

Laws can be used in orchestrate stages to guard world mutations:

```kn
orchestrate guarded_process(input: Int) -> Int:
    stage check: law signal_in_bounds(input)
    stage mutate: patch commit_signal(authority, input)
        requires check
    return mutate
```

### World + Resonate

Resonate handlers attach to world state slots and fire on mutation:

```kn
resonate Authority.signal dampen 16ms:
    Authority.shadow = resonance_pipeline(
        resonate_new_i64 + Authority.tick,
        Authority.tick
    )
```

The resonate handler fires **after** the world field is written, receives the old and new values, and can run arbitrary logic (including orchestrate pipelines).

### World + Pulse

Pulse handlers can read and write world state on a timed interval:

```kn
pulse tick_world every 16ms:
    Authority.tick = Authority.tick + 1
    Authority.pulse_ticks = Authority.pulse_ticks + pulse_tick + 1
```

Pulse + world is the standard pattern for realtime state drivers (game loops, audio engines, simulation stepping).

### World + Entangle

As covered in the previous section — entangle couples world fields. The pairing is the foundation of the dual-world pattern.

### World + Teleport

Teleport moves values destructively from one world to another:

```kn
let moved = teleport shard from FusionAuthority to FusionMirror via shard_bus
```

Worlds serve as the source and destination for teleport operations. The teleport bus is a typed, zero-copy cross-world handoff.

### World + Orchestrate

Orchestrate pipelines can use world operations as stages:

```kn
orchestrate process_frame():
    stage read: converge compute(authority.signal, authority.tick) ...
    stage audit: law signal_in_bounds(authority.signal)
    stage write: patch commit_signal(authority, read_value)
        deps [audit]
```

The orchestrate graph sees worlds as state endpoints — stages read from worlds (converge, law) and write to worlds (patch).

### World + Actor

Actors can read from and write to worlds:

```kn
actor StateManager:
    on Update(reply_to: P, value: Int):
        Authority.signal = value   // write world
        let mirror_val = Mirror.signal_copy  // read mirror
        send reply_to.Done(status = 0)
```

The canonical actor access pattern: **read mirrors, write authorities**. Mirrors are safe for concurrent reads; authorities are the single write target.

### World + Component / Surface

World surfaces bind state to components:

```kn
component MyPanel():
    render <panel title="My World">
        <text value={MyWorld.field} />
    </panel>

world MyWorld:
    state field: Int = 0
    surface native_ui => MyPanel
```

The component can read any field from any world in scope. The surface declaration is the explicit binding that says "this world projects to this component."

### World + Converge

Converge lanes can select computation paths that depend on world state:

```kn
converge compute_mix(value: Int, depth: Int) -> Int:
    spec reference:
        return value * depth / 1000
    fast simd_lane when capability("cpu.x86.avx2"):
        return value * depth / 1000
```

While converge doesn't directly reference worlds, the values passed to converge lanes often come from world state reads.

______________________________________________________________________

## Stdlib Telemetry API

### World ABI Functions

The stdlib exposes world telemetry through `std::intent`:

| Function | Signature | Description |
|----------|-----------|-------------|
| `runtime_world_field_count(name)` | `(String) -> Int` | Number of state fields in a world |
| `runtime_world_surface_count(name)` | `(String) -> Int` | Number of surfaces on a world |
| `runtime_world_name(index)` | `(Int) -> String` | World name by registered index |

These are exposed via the `std::runtime` module and allow runtime introspection of the world graph:

```kn
use std::runtime

fn enumerate_worlds() -> Int:
    let count = runtime_world_count()
    var i: Int = 0
    while i < count:
        let name = runtime_world_name(i)
        let fields = runtime_world_field_count(name)
        let surfaces = runtime_world_surface_count(name)
        i = i + 1
    return count
```

### Related Runtime Telemetry

While worlds themselves don't have dedicated telemetry counters (unlike entangle's `entangle_propagation_count`), the operations that touch worlds have counters:

| Operation | Telemetry Function | What It Counts |
|-----------|-------------------|----------------|
| Patch mutation | `patch_journal_count()` | Number of patch mutations (world writes) |
| Entangle propagation | `entangle_propagation_count()` | Number of entangle propagations (world→world copies) |
| Teleport | `runtime_machine_teleport_count()` | Number of teleport operations (world→world moves) |
| Pulse fire | `runtime_machine_pulse_total_fire_count()` | Number of pulse handler fires (world reads/writes) |

### Stdlib Module

All world-related telemetry lives in `stdlib/intent.kn` and `stdlib/runtime.kn`. Query with `kain_stdlib search_symbols --query "world" --module "runtime"`.

______________________________________________________________________

## Runtime Contract Metadata

### RuntimeWorldContract

When a world is compiled, it produces a `RuntimeWorldContract` that is included in the program's runtime contract bundle:

```rust
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeWorldContract {
    pub name: String,
    pub state_slots: Vec<RuntimeWorldStateContract>,
    pub surfaces: Vec<RuntimeWorldSurfaceContract>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeWorldStateContract {
    pub name: String,
    pub type_name: String,
    pub initial_expr: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimeWorldSurfaceContract {
    pub kind: String,           // "native_ui", "web", "viewport3d", "ue5"
    pub authored_expr: String,  // component name or target string
}
```

This is generated by `collect_world_contracts()` which walks all typed items, collects all `TypedItem::World` entries (including those nested in modules), sorts by name, and produces the contract list.

### Active World Detection

When a program has exactly one world, the runtime contract sets `active_world`:

```rust
let active_world = if worlds.len() == 1 {
    worlds.first().cloned()
} else {
    None
};
```

### Capability Emission

World surfaces imply capabilities in the runtime contract:

| Condition | Capability |
|-----------|-----------|
| Any world has `surface native_ui` | `ui.native` |
| Any world has `surface web` | `ui.web` |
| Any world has `surface viewport3d` | `gpu.viewport` |
| Any world has `surface ue5` | `editor.ue5` |
| Any world participates in entangle | `state.entangle` |

The capabilities are collected from the world declarations and emitted alongside other capabilities (law, pulse, etc.) in the runtime contract's `capabilities` section.

### Realtime App Bundle

In `realtime_app_bundle.rs`, each world contributes metadata about its surfaces and state slots for the realtime app manifest.

______________________________________________________________________

## LLVM Codegen

### World Struct Emission

Each world becomes an LLVM struct type:

```llvm
%WorldName = type { i64, i64, i8, ... }
```

The fields are emitted in declaration order. Field types are mapped from Kain types via `map_type_from_ast()`.

### Global Allocation

Each world gets a zero-initialized global:

```llvm
@__kain_world_WorldName = global %WorldName zeroinitializer
```

### Init Flag + Init Function

Each world gets a lazy initialization guard:

```llvm
@__kain_world_init_flag_WorldName = global i1 0

define void @__kain_init_world_WorldName() {
  ; check init flag, skip if already initialized
  ; GEP each field, store initial value
  ; set init flag to 1
}
```

### World Access Codegen

When user code reads `WorldName.field`, the LLVM codegen emits:

```llvm
call void @__kain_init_world_WorldName()    ; ensure initialized
%ptr = getelementptr inbounds %WorldName, %WorldName* @__kain_world_WorldName, i32 0, i32 <field_index>
%val = load i64, i64* %ptr
```

The field index is computed by `field_index(struct_name, field_name)` which maps field names to their ordinal position in the struct definition.

### World Global Registration

The codegen maintains a `world_globals: HashMap<String, WorldGlobalInfo>` mapping world names to their LLVM metadata:

```rust
struct WorldGlobalInfo {
    global_symbol: String,        // "@__kain_world_WorldName"
    init_flag_symbol: String,     // "@__kain_world_init_flag_WorldName"
    init_fn_name: String,         // "__kain_init_world_WorldName"
}
```

This map is used during codegen to:

- Look up the world's init function when a world field is referenced
- Look up the world's global and field when generating entangle propagation
- Look up the world's field path for `native_world_field_path()`

### Native World Field Path

The function `native_world_field_path(struct_name, field)` returns `Some("WorldName.field")` if the struct is a registered world, or `None` if not. This is used by the entangle propagation codegen to verify that mirror fields exist in the world's struct definition.

______________________________________________________________________

## Native C Runtime

### Machine Stones Support

The native C runtime in `runtime/native/include/machine_stones.h` supports world teleport operations:

```c
// World teleport is handled via the machine stones system
// Worlds are the source and destination for teleport operations
```

World-specific C runtime support exists for:

1. **World init function calls** — the machine stones entry preamble calls `__kain_init_world_*()` for each declared world
1. **Entangle propagation** — the C entangle registry (`entangle.c`) tracks which worlds participate in entangle via their endpoint paths
1. **Teleport** — the teleport API (`machine_stones.c`) moves values between world globals
1. **Telemetry counters** — the ABI functions (`stdlib_abi.c`) track world-touching operations

### No Dedicated World Registry

Unlike entangle (which has `entangle_registry_register()`, `entangle_registry_get()`, etc.), worlds do NOT have a separate C-level registry. Worlds are LLVM globals — the init functions, the globals themselves, and the field access patterns are all handled at the LLVM level, not the C level.

The absence of a `world.h` / `world.c` is deliberate: worlds are LLVM-level constructs with no C runtime overhead beyond their initialization.

______________________________________________________________________

## Z3 Proofs

There are no dedicated Z3 proofs for world as a standalone construct. Unlike:

| Construct | Proofs |
|-----------|--------|
| Entangle | Bounded propagation, slot collision, string copy bounds |
| Patch | Journal bounds, undo mode transitions |
| Pulse | Missed-beat bounds |
| **World** | **None (standalone)** |

World doesn't need standalone proofs because it is the substrate — other constructs have proofs that implicitly cover world behavior:

- **Entangle propagation proofs** verify that world-to-world propagation doesn't overflow the registry
- **Patch journal proofs** verify that world mutation journals stay within bounds
- **Teleport machine stones proofs** verify that cross-world teleport doesn't corrupt state

A Z3 proof for world would be needed if there were:

- A bounded world registry (currently worlds are LLVM globals with no runtime-imposed limit)
- A world lifecycle state machine (init → active → teardown)
- A world capacity constraint (max fields, max surfaces)

None of these exist yet, so no proofs are needed.

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck — catches missing surfaces, type mismatches, duplicate surface kinds
kain check <file.kn> --target llvm

# 2. Compile + run — worlds initialize correctly, surfaces wire up
kain run <file.kn> --target llvm

# 3. Test world field reads/writes
fn test_world() -> Int:
    if Authority.signal != 1:       // default initializer
        return 1
    Authority.signal = 42
    if Authority.signal != 42:      // verify write
        return 2
    return 0

# 4. Test entangle propagation — verify mirror updates
fn test_entangle() -> Int:
    Authority.signal = 99
    if Mirror.signal_copy != 99:
        return -11
    return 0

# 5. Test orphan world detection
# The typechecker should reject a world with no surfaces

# 6. Check runtime contract — worlds appear in the contract manifest
kain build <file.kn> --target llvm
# Inspect output runtime_contract.json for worlds[]

# 7. Multi-world test — verify distinct state
fn test_multi_world() -> Int:
    Authority.signal = 1
    OtherWorld.signal = 2
    if Authority.signal != 1 or OtherWorld.signal != 2:
        return 1
    return 0

# 8. For benchmark proof
python benchmark/run.py --case fusion_chain --languages kain --runs 1 --warmups 0
```

### Smoketest

```kn
// smoketest/src/semantics/world.kn
use std::runtime

component WorldTestPanel():
    render <panel title="World Test" />

world TestWorld:
    state value: Int = 42
    state flag: Bool = true
    surface native_ui => WorldTestPanel

pub fn smoke_world_lane() -> Int:
    if TestWorld.value != 42:
        return 1
    if TestWorld.flag != true:
        return 2
    TestWorld.value = 99
    if TestWorld.value != 99:
        return 3
    return 0
```

______________________________________________________________________

## Anti-Patterns

### ❌ Using Global Variables Instead of World

```kn
// WRONG: mutable global outside a world
var global_signal: Int = 0

fn set_signal(value: Int):
    global_signal = value
```

```kn
// RIGHT: world as state authority
world Authority:
    state signal: Int = 0
    surface native_ui => Panel

patch set_signal(world: Authority, value: Int) -> Int:
    world.signal = value
    return value
```

**Why:** A `var` global has no surface projection, no entangle participation, no patch journal, no resonate attachment, no runtime contract. It's invisible to the compiler-owned state graph.

### ❌ Using Struct Instead of World

```kn
// WRONG: plain struct — no world semantics
struct AppState:
    signal: Int = 0
```

```kn
// RIGHT: world with surface
world AppState:
    state signal: Int = 0
    surface native_ui => AppPanel
```

**Why:** A `struct` cannot participate in entangle, doesn't get LLVM init guards, has no surface projection, and isn't visible in the runtime contract.

### ❌ Declaring a World Without a Surface

```kn
// WRONG: compiler rejects this
world Orphan:
    state value: Int = 0
    // no surface!
```

The typechecker enforces at least one surface per world. Every world must have a projection path to the runtime.

### ❌ Duplicate Surface Kinds

```kn
// WRONG: two surfaces of the same kind
world Duplicate:
    state value: Int = 0
    surface native_ui => Panel
    surface native_ui => Panel    // error: duplicate surface
```

### ❌ Treating World like a Mutable Global in Actors

```kn
// WRONG: writing to a mirror world
on Process(reply_to: P):
    Mirror.signal_copy = 42       // compile error: entangle mirror
```

```kn
// RIGHT: write authority, read mirror
on Process(reply_to: P):
    Authority.signal = 42          // allowed — authority
    let val = Mirror.signal_copy   // allowed — mirror read
    send reply_to.Done(value = val)
```

### ❌ World Without Epoch / Telemetry

```kn
// WRONG: no epoch — no way to observe state versioning
world Flat:
    state value: Int = 0
    surface native_ui => Panel
```

```kn
// RIGHT: epoch counter makes state changes observable
world Versioned:
    state value: Int = 0
    state epoch: Int = 0
    surface native_ui => Panel
```

The epoch counter is the standard pattern for making state transitions observable to downstream consumers (entangle mirrors, resonate handlers, telemetry checks).

### ❌ One World for Everything

```kn
// WRONG: monolithic world
world Monolith:
    state audio_volume: Int = 80
    state video_brightness: Int = 500
    state input_mouse_x: Int = 0
    state network_latency: Int = 0
    state physics_gravity: Int = 98
    // 20+ more unrelated fields
    surface native_ui => BigPanel
```

```kn
// RIGHT: decompose by domain
world AudioState:
    state volume: Int = 80
    surface native_ui => AudioPanel

world VideoState:
    state brightness: Int = 500
    surface native_ui => VideoPanel

world InputState:
    state mouse_x: Int = 0
    surface native_ui => InputPanel
```

**Why:** Monolithic worlds defeat the purpose of surface projection — unrelated state shouldn't be coupled to the same component. Decompose by domain for independent surfaces, entangles, and resonate handlers.

### ❌ World as a Bag of Random State

```kn
// WRONG: inconsistent grouping
world Misc:
    state temperature: Int = 22
    state user_name: String = "admin"
    state frame_count: Int = 0
    state volume: Int = 50
    surface native_ui => MiscPanel
```

**Why:** Each world should represent a coherent state domain. If the fields don't participate in the same surfaces, entangles, or resonate handlers, they should be in separate worlds.

### ❌ Not Using Surfaces for Sidecar Debugging

```kn
// WRONG: world with no debug surface
world Engine:
    state fps: Int = 60
    state draw_calls: Int = 1000
    // no debug surface — how does the developer see this?
    surface native_ui => EnginePanel
```

Every world should have at least one surface, even if it's just a debug panel. Without a surface, the world's state is invisible at runtime.

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `[pub] world <Name>:\n  state <field>: <Type> = <Expr>\n  ...\n  surface <Kind> => <Expr>` |
| **Minimum surfaces** | 1 (enforced by typechecker, `TypeWorldMissingSurface`) |
| **Surface kinds** | `native_ui`, `web`, `viewport3d`, `ue5` |
| **State slots** | Typed fields with compile-time validated initializers |
| **Duplicate surfaces** | Rejected per world |
| **Parser** | `parse_world()` at line ~2035 in `crates/core/src/parser.rs` |
| **AST type** | `WorldDef { name, states, surfaces, visibility, attributes, span }` |
| **Typed AST** | `TypedWorld { ast: WorldDef }` |
| **Typecheck** | `check_world()` in `crates/core/src/types.rs` (~line 5987) |
| **LLVM struct** | `%WorldName = type { field_types... }` |
| **LLVM global** | `@__kain_world_WorldName` |
| **LLVM init guard** | `@__kain_world_init_flag_WorldName` + `@__kain_init_world_WorldName()` |
| **Interpreter** | `register_world_value()` — `Arc<RwLock<HashMap>>` per world |
| **Runtime contract** | `RuntimeWorldContract { name, state_slots, surfaces }` |
| **Active world** | Set when exactly one world declared |
| **Capabilities** | `ui.native`, `ui.web`, `gpu.viewport`, `editor.ue5` from surfaces |
| **Stdlib module** | `std::runtime` (world enumeration), `std::intent` (telemetry) |
| **Z3 proofs** | None (world is the substrate; other constructs have proofs that cover world behavior) |
| **Canonical example** | `benchmark/cases_v2/fusion_chain.kn` — dual-world authority+mirror |
| **Canonical pattern** | Dual-world authority+mirror with entangle + surface projection |
| **Real-world example** | `blades/python/24_tet/src/resonate_py_effects.kn` — 30+ field effects engine |
| **Experiment example** | `blades/experiments/convergence/src/world.kn` — telemetry aggregator |

______________________________________________________________________

## Source Anchors

### Core Crate — Parser & AST

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/parser.rs` | 690-705 | `is_contextual_item_start_name()` — world in contextual keyword list |
| `crates/core/src/parser.rs` | 2035-2095 | `parse_world()` — full world parser with state/surface dispatch |
| `crates/core/src/ast.rs` | 390-432 | `WorldDef`, `WorldStateSlot`, `WorldSurfaceProjection`, `WorldSurfaceKind` |

### Core Crate — Type System

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/types.rs` | 203-206 | `TypedWorld` struct definition |
| `crates/core/src/types.rs` | 5987-6037 | `check_world()` — type resolution, surface requirement, duplicate detection |
| `crates/core/src/types.rs` | 6254-6303 | `check_world_surface_projection()` — per-kind expression validation |
| `crates/core/src/types.rs` | 6000-6060 | `resolve_entangle_endpoint_type()` — world field resolution (shared with entangle) |

### Core Crate — Runtime Contract

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime_contract.rs` | 212-230 | `RuntimeWorldStateContract`, `RuntimeWorldSurfaceContract`, `RuntimeWorldContract` |
| `crates/core/src/runtime_contract.rs` | 358-364 | World contract collection with `active_world` detection |
| `crates/core/src/runtime_contract.rs` | 1471-1513 | `collect_world_contracts()`, `runtime_world_contract()` — contract emission |

### Core Crate — Interpreter Runtime

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime.rs` | 3985-4010 | `register_world_value()` — initializer eval, Arc<RwLock> registration |
| `crates/core/src/runtime.rs` | 4176-4179 | Item dispatch for world registration |
| `crates/core/src/runtime.rs` | 4298-4301 | TypedItem dispatch for world registration |

### LLVM Codegen

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 49-54 | `WorldGlobalInfo { global_symbol, init_flag_symbol, init_fn_name }` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 640-645 | `world_globals: HashMap<String, WorldGlobalInfo>` field in codegen struct |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13460-13490 | `register_world_type_and_global()` — struct type, global, init flag emission |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15953-16030 | `compile_world_initializer()` — init function with lazy guard + GEP stores |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 19698-19701 | World load codegen — init call + global load |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13374 | `TypedItem::World` dispatch to `compile_world_initializer()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11086-11092 | `native_world_field_path()` — entangle codegen world field lookup |

### Native C Runtime

| File | Content |
|------|---------|
| `runtime/native/include/machine_stones.h` | World teleport support declarations |
| `runtime/native/src/core/machine_stones.c` | Teleport implementation (world→world) |
| `runtime/native/src/core/entangle.c` | Entangle registry (world endpoint tracking) |
| `runtime/native/src/core/stdlib_abi.c` | World-touching telemetry counters |

### Stdlib

| File | Content |
|------|---------|
| `stdlib/intent.kn` | World-touching telemetry (entangle count, patch count, etc.) |
| `stdlib/runtime.kn` | World enumeration functions (world count, name, field count) |

### Examples

| File | Usage |
|------|-------|
| `benchmark/cases_v2/fusion_chain.kn` | Dual-world authority+mirror, full causal chain, 8 state slots per world, 4 entangles |
| `blades/python/24_tet/src/resonate_py_effects.kn` | 30+ state slot authority + 13-slot mirror, 14 entangles, effects engine parameters |
| `blades/experiments/convergence/src/world.kn` | Telemetry world with pointer buffers + scalar counters, single surface |
| `smoketest/src/semantics/world.kn` | Minimal world smoke test: 2 state slots, single surface |
| `library_of_kain/world.kn` | Minimal world demonstration |
| `library_of_kain/semantics.kn` | Compact full semantic stack with world |
