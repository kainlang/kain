# How To Write Kain — The Rule Book

**Status:** Research synthesis, 2026-06-06
**Based on:** CATALOG.MD, GLOSSARY.MD, MEMORY.md, lang-semantics SKILL, lang-systems SKILL, lang-gpu SKILL, ~11,500 code-chunk semantic search, **fusion_chain.kn (550 lines — the definitive causal-chain proof exercising all 7 semantic layers simultaneously)**, **convergence rat experiment (6 files, `blades/experiments/convergence/` — creative semantic abuse: converge as strategy selector, orchestrate as multi-algorithm composition, world as experiment telemetry, patch as frame journaling, law as domain model validation, shatter as experiment layout, actors as simulation agents)**, 24-tet effects engine (843 lines — the most comprehensive single-file semantic stack), actor_ownership_backpressure benchmark, orchestration benchmark, axiom/pulse/teleport smoke proofs.

______________________________________________________________________

## The Central Problem: Kain Is Not Rust

The single hardest thing about writing Kain is resisting the gravitational pull of Rust-like tendencies. Kain looks familiar enough (`fn`, `struct`, `let`, `mut`, `if`, `while`) that you fall into writing Rust-with-Kain-syntax. This is wrong.

Kain's innovation is its **compiler-owned semantic stack**: 15+ constructs where the compiler, not the programmer, owns the truth about state, mutation, dispatch, timing, coupling, layout, and handoff. When you use `fn` and `let` for a problem that should be a `world`, `patch`, `converge`, or `pulse`, you're paying the semantic cost without getting the compiler's help.

This rule book answers one question: **given a problem, which Kain construct should I reach for?**

______________________________________________________________________

## The Decision Ladder

Every time you're about to write a new piece of code, climb this ladder from top to bottom. The first rung that fits is your construct.

```
                    ┌──────────────────────────────┐
                    │ "Am I crossing into C/OS?"    │──▶ include ... as ...
                    │ "Is this Python host code?"   │──▶ import ...
                    │ "Is this a GPU kernel?"       │──▶ shader compute
                    │ "Is this a UI component?"     │──▶ component
                    ├──────────────────────────────┤
LAYER 7: SYSTEMS    │ "Concurrent message state?"   │──▶ actor
                    │ "Raw memory lifecycle?"       │──▶ collapse/observe/decay
                    ├──────────────────────────────┤
LAYER 6: MACHINE    │ "Capability assumption?"      │──▶ axiom
  STONES            │ "Hot-data layout?"            │──▶ shatter struct
                    │ "Cross-world zero-copy?"      │──▶ teleport
                    ├──────────────────────────────┤
LAYER 5: TEMPORAL   │ "Timed recurrence?"           │──▶ pulse
                    │ "React to state change?"      │──▶ resonate
                    ├──────────────────────────────┤
LAYER 4: STAGE      │ "Multi-stage pipeline?"       │──▶ orchestrate
  GRAPH             │ "Cross-runtime scheduling?"   │──▶ orchestrate
                    ├──────────────────────────────┤
LAYER 3: DISPATCH   │ "Spec + fast lanes?"          │──▶ converge
                    │ "Platform-specific perf?"     │──▶ converge
                    ├──────────────────────────────┤
LAYER 2: STATE      │ "Journaled mutation?"         │──▶ patch
  INTEGRITY         │ "Invariant predicate?"        │──▶ law
                    ├──────────────────────────────┤
LAYER 1: STATE      │ "Global named state?"         │──▶ world
  AUTHORITY         │ "Mirrored state?"             │──▶ world + entangle
                    │ "Coupled fields?"             │──▶ entangle
                    ├──────────────────────────────┤
LAYER 0: PLAIN      │ None of the above?            │──▶ fn, struct, let, etc.
  CODE              │                              │──▶ Use effects for intent
                    └──────────────────────────────┘
```

______________________________________________________________________

## Layer 0 — Plain Code (fn, struct, let, enum, trait, impl)

**When to use:** Always. This is the baseline. Every `.kn` file is mostly plain code.

**What it is:** Functions, data types, control flow, modules. Same conceptual space as Rust/C++ but with Kain syntax.

**Key differentiators from Rust:**

- No borrow checker. Ownership is explicit via `collapse`/`observe`/`decay`, not inferred.
- Effects (`Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe`) are declared, not inferred.
- `defer expr` for block-scoped cleanup (LIFO).
- `ptr<T>` for raw pointers, not `*const T` / `*mut T`.
- `Option<T>` and `Result<T, E>` with `?` operator.

**When NOT to use plain code:** When any Layer 1-7 construct fits. The ladder exists because plain `fn` is the fallback, not the first choice. A `patch` is not "a function that writes to a global." A `law` is not "an `if` statement that checks bounds." A `converge` is not "a function with `#[cfg]`."

**Effects rule:**

```kn
// Pure: no side effects, no IO, no memory
fn mix_pure(value: Int) -> Int with Pure:
    return ((value * 31) + 7) % MODULUS

// IO: console, filesystem, network
fn log(value: Int) with IO:
    println("value = " + str(value))

// Unsafe: raw pointers, atomics, asm, ABI calls
fn dirty_lane(cells: ptr<Int>) -> Int with Unsafe:
    return mem_load(cells, "Int")

// GPU: dispatch keyword, GPU runtime calls
fn compute_frame() -> Int with GPU, Unsafe:
    dispatch "shader::Kernel::compute" [32, 32, 1]
    return 0
```

______________________________________________________________________

## Layer UI — Components (component, JSX)

`component` is Kain's native UI abstraction. It is a **full React-like component model** — typed props, local state, methods, JSX render body, and JSX composition via `<ComponentName />`. Every `component` in the current codebase looks like this:

```kn
component App():
    render <panel title="My App" />

world MyWorld:
    state signal: Int = 1
    surface native_ui => App
```

**That is correct but it is also the bare minimum.** The power of `component` goes far beyond world surface declarations. Here's what component actually is.

### Component Anatomy

```kn
component Counter(initial: Int, label: String):
    state count: Int = initial

    fn increment(_self: Self_) -> Int:
        return _self.count + 1

    fn label_text(_self: Self_) -> String:
        return _self.label + ": " + str(_self.count)

    render <box>
        <text value={label_text()} />
        <text value="clicks" />
    </box>
```

A component has:

- **Typed props** — `(initial: Int, label: String)` — like function parameters but for UI.
- **Local state** — `state count: Int = initial` — with initializers that can reference props.
- **Methods** — `fn increment(_self: Self_) -> Int` — functions that receive `_self: Self_` to access component state.
- **JSX render body** — `render <box>...</box>` — the visual tree.
- **Optional effects** — `component Widget() with Reactive:` — signals this component handles events.

### JSX Composition: Components Calling Components

Any JSX tag with an uppercase first letter is a **component call**:

```kn
component Button(label: String, kind: String):
    state hovered: Bool = false
    render <box>
        <text value={label} />
    </box>

component Toolbar():
    render <stack direction="horizontal">
        <Button label="Save" kind="primary" />
        <Button label="Load" kind="secondary" />
        <Button label="Export" kind="ghost" />
    </stack>

component App():
    render <panel title="Dashboard">
        <Toolbar />
        <Counter initial={0} label="Saves" />
        <Counter initial={10} label="Edits" />
        <text value="Status: ready" />
    </panel>
```

**Tag case is the dispatch mechanism:**

- `<panel>`, `<text>`, `<box>`, `<stack>` → lowercase → **native UI elements** (routed to `std::ui` renderer).
- `<Button>`, `<Toolbar>`, `<Counter>` → uppercase → **component calls** (resolved to `component Button(...)` declarations in scope).

### JSX Has Full Control Flow

```kn
component TodoList(items: [String], selected_index: Int):
    render <stack direction="vertical">
        for item in items:
            <text value={item} />
        if selected_index >= 0:
            <text value={"Selected: " + items[selected_index]} />
        else:
            <text value="Nothing selected" />
    </stack>
```

- **`for item in list:`** — loops inside JSX.
- **`if cond:` / `else:`** — conditional rendering.
- **`{expr}`** — expression interpolation in attributes and text.
- **`<Fragment>`** — multiple root nodes without a wrapper element.

### When To Use Component vs World vs Plain Fn

| You want to... | Use | Because |
|---|---|---|
| Define a reusable piece of UI with its own state | `component` | Component owns its render tree, state, and methods |
| Track application-level state that survives frame boundaries | `world` + `surface native_ui => Component` | World is the authority; component is the view |
| Compose UI from smaller pieces | `component` calling `<OtherComponent />` | JSX composition — uppercase tags dispatch to components |
| Bind a world to a root UI view | `surface native_ui => ComponentName` on world | This is the canonical world→UI wiring |
| Do computation that produces no UI | `fn` | Components are for rendering; functions are for logic |
| Render data from a world inside a component | Read `world.field` inside JSX `{expr}` | Components have read access to world state |
| Handle live events, polling, input | `component` with `state` + Kaintana/UI bridge | Component state + event system = interactive UI |
| Build a whole UI framework | `component` + `world` + `resonate` + `patch` | See Kaintana — component is the view layer in a full MVC |

### Component Is NOT Tied To World

This is the most important thing to unlearn. Every example in the repo places `component` immediately before a `world` that references it:

```kn
component SieveDisplayPanel():
    render <panel title="Neural Entanglement Scope" />

world CorticalAuthority:
    state network_charge: Int = 0
    surface native_ui => SieveDisplayPanel
```

This is **convention, not requirement**. A single file could host 10,000 components and zero worlds. Components can be composed, nested, and rendered independently of any world. The `surface native_ui => ComponentName` syntax is just one way to wire a world to a root component — it's not the only way, and it's not required for components to exist.

### The Component's Place in the Decision Ladder

Component sits at the very top of the decision ladder, alongside the other "what kind of thing am I building?" questions:

```
"Am I rendering UI?"
  ├── Is it a single, reusable widget?          → component
  ├── Is it application state behind the UI?     → world + surface => Component
  ├── Is it an interactive widget framework?     → component + world + resonate + patch
  └── Is it computation that feeds the UI?       → fn (called from component JSX {expr})
```

### Anti-Pattern: Component as World Decoration

**Wrong:** Using component ONLY as a single-line `render <panel>` wrapper for world surfaces, never composing components, never using state or methods.

```kn
// Underutilized — component is just a title string
component App():
    render <panel title="My App" />
```

**Right:** Components as the full UI composition layer.

```kn
// Component as UI composition — the full model
component MetricCard(title: String, value: Int, unit: String):
    state expanded: Bool = false
    fn display_value(_self: Self_) -> String:
        return str(_self.value) + " " + _self.unit
    render <box>
        <text value={title} />
        <text value={display_value()} />
    </box>

component Dashboard(signal: Int, hot: Int):
    render <panel title="Neural Scope">
        <stack direction="horizontal">
            <MetricCard title="Signal" value={signal} unit="ms" />
            <MetricCard title="Hot Synapses" value={hot} unit="nodes" />
            <MetricCard title="Delta" value={signal - hot} unit="diff" />
        </stack>
        <text value="All systems nominal" />
    </panel>
```

### Component vs Kaintana

`component` is the language-level primitive. **Kaintana** (`blades/ui/kaintana/`) is a framework built ON TOP of components using `std::ui`, `world`, `entangle`, `resonate`, `patch`, `law`, and a desktop bridge. It provides themes, layout helpers, widget reconciliation, event routing, input handling, and Vulkan/winit desktop adapters. Think of `component` as the language's built-in `<div>` and Kaintana as its React. You can use component without Kaintana (via `std::ui` directly), but Kaintana gives you the full retained-mode widget framework.

______________________________________________________________________

## Layer 1 — State Authority (world, entangle)

### `world` — Named, Compiler-Owned State Container

**When to use:**

- You have state that needs to be globally visible across the program.
- You need compiler-tracked state with surfaces (native_ui, web, viewport3d, ue5).
- You plan to entangle, patch, teleport, resonate, or orchestrate against this state.
- The state represents an "authority" — a single source of truth for a domain.

**When NOT to use:**

- State is local to a single function scope → just use `let mut` or `var`.
- State is an implementation detail of one struct → use struct fields.
- You just want a global variable → that's what `world` does, but don't use it when a function parameter would suffice.

**The dual-world authority+mirror pattern (canonical):**

```kn
// Authority: owns mutable state, has native_ui surface
world RenderAuthority:
    state frame: Int = 0
    state particles: Int = 65536
    surface native_ui => RenderPanel

// Mirror: receives state via entangle, has web surface
world RenderMirror:
    state frame_copy: Int = 0
    state particles_copy: Int = 65536
    surface web => RenderPanel

// Coupling: single_writer means authority → mirror only
entangle RenderAuthority.frame <-> RenderMirror.frame_copy with single_writer
entangle RenderAuthority.particles <-> RenderMirror.particles_copy with single_writer
```

**Why the mirror pattern exists:** The authority world owns mutable state (typically with a `native_ui` surface for the main thread). The mirror world receives state via entangle propagation and exposes it through a `web` surface for read-only inspection — at zero cost, through the compiler-owned observer graph, not a heap observer registry.

### `entangle` — Compiler-Owned State Coupling

**When to use:**

- Two world fields should stay in sync automatically.
- You need single-writer semantics: one world writes, the other reads.
- You want the compiler to track propagation counts and coupling metadata.

**When NOT to use:**

- You're syncing two local variables → just assign.
- You need bidirectional writes → entangle is single_writer only (currently).
- You just want to pass data between functions → use parameters.

**Rules:**

- Endpoints must be struct paths: `WorldName.field_name`.
- Both endpoints must have matching types.
- A field can only participate in one entangle.
- `single_writer` policy means the authority writes, the mirror receives — mirror writes are rejected.

______________________________________________________________________

## Layer 2 — State Integrity (law, patch)

### `law` — Invariant Predicate (Returns `Bool`)

**When to use:**

- You have a constraint that must hold for correctness (bounds, ranges, valid states).
- The invariant should be compiler-witnessable, not hidden in an `if` inside a function.
- You want the invariant to be part of the runtime contract (shows up in telemetry, orchestrate stages, patch guards).
- You want Z3 to be able to prove the invariant given known preconditions.

**When NOT to use:**

- It's a one-off check in a function body → use `if`.
- It's business logic, not an invariant → use `fn`.
- It doesn't return `Bool` → `law` must return `Bool`.

**Canonical shape:**

```kn
law value_in_range(v: Int) -> Bool:
    return v >= 0 and v < 1000000007

law shape_valid(s: Int) -> Bool:
    return s >= 0 and s < SHAPE_COUNT
```

**Runtime integration:** Laws can be invoked from orchestrate stages (`stage check: law my_law(value)`), referenced by patch guards, and queried via `law_status(law_name(value))` / `law_is_valid_status(status)`.

### `patch` — Journaled, Tracked Mutation

**When to use:**

- A mutation should be recorded in the patch journal (auditability, replay, undo).
- The mutation targets world state fields.
- You want runtime telemetry: `patch_journal_count()`, `patch_last_path()`.
- The mutation might be part of an orchestrate pipeline stage.

**When NOT to use:**

- You're setting a local variable → just assign.
- You're modifying a struct field that isn't world state → use `impl` methods.
- The mutation doesn't need tracking → plain world field assignment in a function is fine.

**Canonical shape:**

```kn
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return authority.signal

patch set_effect_params(world: FxWorld, mix: Int, depth: Int) -> Int:
    world.effect_mix = mix
    world.effect_depth = depth
    world.fx_epoch = world.fx_epoch + 1
    return world.fx_epoch
```

**Key insight from the effects engine:** Every `patch` increments an epoch counter. This is an intentional pattern — epoch counters make the mutation visible to entangle propagation, resonate handlers, and orchestrate stage dependency tracking. A `patch` without an epoch bump is still valid, but loses the "this state changed" signal.

______________________________________________________________________

## Layer 3 — Dispatch (converge)

### `converge` — Spec + Platform-Specific Fast Lanes

**When to use:**

- You have a reference implementation and one or more platform-specific optimized versions.
- The optimization is gated by `target("...")` or `capability("...")`.
- You need the compiler/runtime to select the best lane automatically.
- You need `verify random(N)` to fuzz-test fast lanes against the spec.

**When NOT to use:**

- There's only one implementation → use `fn`.
- The "fast" path is the same code as the "spec" path → use `fn`.
- You're doing `if target == "windows"` inside a function → `converge` is the right tool, use it.

**Canonical shape:**

```kn
fn mix_scalar(value: Int) -> Int:
    return ((value * 31) + 7) % MODULUS

converge mix(value: Int) -> Int:
    spec reference:
        return mix_scalar(value)
    fast llvm_lane when target("llvm"):
        return ((value * 31) + 7) % MODULUS
    fast avx2_lane when capability("cpu.x86.avx2"):
        return ((value * 31) + 7) % MODULUS
    verify random(8)
```

**Rules:**

- Exactly one `spec` lane (the reference truth).
- At least one `fast` lane.
- Selectors: `target("llvm")`, `target("linux")`, `target("windows")`, `capability("cpu.x86.avx2")`, `capability("gpu.compute")`, etc.
- `verify random(N)` tests N random inputs against the spec at startup/selection time.
- The runtime probes capabilities, scans fast lanes in order, and falls back to spec if no fast lane matches.
- Use `converge_mismatch_count()` to detect divergence in production.

**The pattern seen in practice:** Most current `converge` usage has identical spec and fast lane bodies — the value is the lane-selection machinery and the `verify random(N)` contract, not divergent implementations yet. As the platform matrix grows (CUDA, AVX-512, ARM NEON), divergent fast lanes become the norm.

______________________________________________________________________

## Layer 4 — Stage Graph (orchestrate)

### `orchestrate` — Typed, Multi-Runtime Pipeline

**When to use:**

- You have a pipeline of dependent stages across different runtimes (CPU → GPU → law check → patch → world).
- Stages need explicit dependencies, residency declarations, transfer policies, fallback paths, and guard axioms.
- You want the compiler to validate the dependency graph for cycles, missing deps, and impossible transfer/residency pairs.
- You want runtime telemetry: `orchestrate_stage_count()`, stage timing, transfer counts, fallback activations.

**When NOT to use:**

- It's a linear sequence of function calls → plain `fn` with sequential calls.
- All stages run on the same runtime with no interesting residency/transfer → plain code.
- You just want to call a GPU function → use `dispatch "key" [x, y, z]` directly.

**Available stage runtimes:**

- `kain` — Kain function call (default)
- `cpu` — CPU-bound computation
- `gpu` — GPU compute
- `dispatch` — GPU dispatch statement
- `converge` — Converge lane selection
- `law` — Law invariant check
- `patch` — Patch mutation
- `world` — World state computation
- `c`, `python`, `rust`, `node` — Foreign runtime adapters

**Canonical shape:**

```kn
orchestrate fx_process_note(slot: Int, velocity: Int, frame: Int, world: FxWorld) -> Int:
    // Stage 1: CPU computation
    stage lfo1_stage: cpu fx_lfo_sin_scalar(world.phase, world.depth) using capability("cpu.scalar") residency host policy static

    // Stage 2: Dependent on stage 1, uses converge dispatch
    stage chorus_stage: converge fx_wet_dry_mix(0, world.chorus_mix, lfo1_stage) deps [lfo1_stage] residency host policy static

    // Stage 3: Law check, depends on stage 2
    stage law_check: law fx_mix_in_bounds(world.drive) deps [chorus_stage] residency host policy static

    // Stage 4: World score with multiple deps and a requirement
    stage world_score: world ((slot * 17) + (velocity * 31) + chorus_stage) deps [chorus_stage] requires law_check residency shared policy telemetry_prefer_cpu

    // Stage 5: Patch application
    stage patch_stage: patch fx_set_whole_state(world, frame + slot + velocity) deps [world_score] requires law_check residency host policy telemetry_balance_latency

    return world_score + patch_stage
```

**Graph clauses reference:**
| Clause | Values | Meaning |
|--------|--------|---------|
| `after <stage>` | Stage name | Linear dependency (sugar for `deps [stage]`) |
| `deps [a, b]` | Stage names | Explicit dependency list |
| `residency` | `host`, `shared`, `device` | Where data lives |
| `transfer` | `none`, `host_to_device`, `device_to_host`, `shared_view` | Data movement |
| `guarded by` | Axiom name | Capability gate |
| `fallback` | `abort`, stage name, `degrade <stage>` | Failure behavior |
| `requires` | Law stage name | Invariant must pass |
| `policy` | `static`, `telemetry_prefer_gpu`, `telemetry_prefer_cpu`, `telemetry_balance_latency` | Scheduling hint |

**Key insight from the effects engine:** The orchestrate block in `fx_process_note` chains 9 stages (cpu → converge → law → world → patch → dispatch) with explicit dependencies, residency policies (host vs shared), transfer declarations (shared_view), and fallback policies. This is not function composition — it's a typed graph the runtime can reason about, reorder where safe, and instrument.

______________________________________________________________________

## Layer 5 — Temporal Semantics (pulse, resonate)

### `pulse` — First-Class Temporal Beat

**When to use:**

- Timing/recurrence is part of your program's semantics, not an implementation detail.
- You want jitter-tolerant periodic execution (frame loops, physics ticks, LFO modulation, heartbeat).
- You want the runtime to own the scheduling, not a hand-rolled `while` + `sleep`.
- You need `pulse_tick`, `pulse_dt_ms`, `pulse_missed` locals for accurate timing.

**When NOT to use:**

- It's a one-shot timer → use `async` + timer or `ask_timeout`.
- It's a tight spin loop → use `while` + raw timing.
- It's an event-driven callback → use `resonate` or actor `on` handlers.

**Canonical shape:**

```kn
pulse render_clock every 16ms jitter 2ms:
    let next = Authority.frame + 1
    let committed = commit_frame(Authority, next)
    let _shape = frame_score(committed) + pulse_tick + pulse_dt_ms + pulse_missed

pulse fx_modulation_tick every 8ms jitter 1ms:
    let dt: Int = pulse_dt_ms
    if dt < 1:
        dt = 8
    let advance: Int = FxWorld.lfo1_rate * dt / 10
    FxWorld.lfo1_phase = (FxWorld.lfo1_phase + advance) % (PHASE_MAX + 1)
```

**Duration units:** `ns`, `us`, `ms`, `s`, `tick`, `ticks`.

**Key insight:** Pulse bodies have full access to world state (they can read and write directly). They fire once immediately on start, then on schedule. The `pulse_missed` local tells you how many beats were skipped (overload signal). Pulse bodies accept broad effects: IO, Async, GPU, Reactive, Unsafe.

### `resonate` — Reactive State-to-Execution Tripwire

**When to use:**

- A world field change should trigger a handler without polling.
- You want dampening to absorb rapid-fire changes (debouncing).
- The reaction should be a compiler-owned post-store shadow patch, not a heap observer registry or callback queue.
- You want direct LLVM lowering (after matching stores, splice the handler), not indirect dispatch.

**When NOT to use:**

- You want to poll the value → just read it in a pulse or loop.
- The reaction is simple enough to inline → just write it next to the store.
- You need complex event routing → actor message handlers.

**Canonical shape:**

```kn
resonate Authority.signal dampen 16ms:
    let new_rate: Int = resonate_new_i64
    if new_rate >= RATE_MIN and new_rate <= RATE_MAX:
        Authority.tremolo_rate = new_rate * 2

resonate FxWorld.distortion_drive dampen 32ms:
    let new_drive: Int = resonate_new_i64
    if new_drive < MIX_SCALE / 2:
        FxWorld.distortion_output = 500
    else:
        FxWorld.distortion_output = 600
```

**Handler locals:**

- `resonate_old_i64` / `resonate_old_f64` — value before the mutation
- `resonate_new_i64` / `resonate_new_f64` — value after the mutation
- `resonate_fired: Bool` — always true inside the handler

**Dampening:** `dampen Nms` creates an absorption window. During the window, subsequent changes to the same field don't re-trigger the handler. `dampen 0ms` means no absorption except active-target reentry suppression.

**Anti-self-feedback rule:** A resonance handler cannot directly assign to its own target field. You can write to *other* world fields (cascading effects) but not self-loop.

**Key insight from the effects engine:** `resonate` on `lfo1_rate` adjusts `tremolo_rate` — a cascading semantic dependency. `resonate` on `distortion_drive` adjusts `distortion_output` for level compensation. These are genuine reactive DSP relationships expressed as compiler-owned semantics, not ad hoc callbacks.

______________________________________________________________________

## Layer 6 — Machine Stones (axiom, shatter, teleport)

### `axiom` — Capability Assumption with Fallback

**When to use:**

- Your code depends on a target, architecture, or capability that may not be present.
- You need a declared fallback path when the assumption fails.
- The capability gate should be visible in the runtime contract (for orchestrate `guarded by` clauses).

**When NOT to use:**

- The capability is always present in your target → no need.
- You're doing a simple platform check → `converge` with `target("...")` is better.
- It's just documentation → use comments.

**Canonical shape:**

```kn
axiom machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "machine lane supports shatter, teleport, and pulse"
    fallback scalar_fallback
```

**Rules:**

- At least one predicate (`target`, `arch`, `capability`).
- At least one guarantee string.
- A non-empty fallback (function or expression).
- Orchestrate stages can use `guarded by axiom_name`.

### `shatter struct` — Structure-of-Arrays Layout Intent

**When to use:**

- You have hot data where field lanes are accessed independently (SoA beats AoS for SIMD/GPU).
- You're preparing data for GPU upload or SIMD processing.
- You want the compiler to track lane layout metadata for the runtime.

**When NOT to use:**

- Normal struct with mixed access patterns → use `struct`.
- Small structs that fit in cache lines → `struct` is fine.
- You don't have performance evidence that SoA matters → measure first.

**Canonical shape:**

```kn
shatter struct Particle:
    position_x: Float
    position_y: Float
    velocity_x: Float
    velocity_y: Float
    alive: Bool
```

**Key insight:** `shatter struct` is *layout intent*, not just a different struct. The compiler emits SoA metadata and the LLVM lane can lower shattered array literals through `kain_machine_shatter_alloc`. The runtime contract marks the layout as `structure-of-arrays` and records field lanes.

### `teleport` — Zero-Copy Cross-World Handoff

**When to use:**

- Moving data from one world to another without copying.
- The source should be considered "moved" (post-teleport reads are compile errors).
- You want the runtime to track the handoff as a machine stone event.

**When NOT to use:**

- You're just passing a value between functions → use parameters.
- Both worlds should keep the data → use entangle, not teleport.
- You want a copy → just assign or clone.

**Canonical shape:**

```kn
teleport shard from Authority to Mirror via pulse_bus
```

**Rules:**

- Source and target worlds must be distinct, declared worlds.
- Channel (`via ...`) cannot be empty.
- If the value is a simple identifier, it's marked moved — later reads are compile errors.
- Pointer payloads route through `kain_machine_teleport_ptr`; non-pointer through `kain_machine_teleport_note`.

______________________________________________________________________

## Layer 7 — Systems (actor, collapse/observe/decay)

### `actor` — Message-Oriented Stateful Concurrency

**When to use:**

- You need concurrent units of work that own state over time.
- Communication is message-passing (not shared memory).
- You want mailbox semantics with request/reply patterns.
- You're building worker pools, relays, supervisors, state machines.
- You want actor telemetry: scheduler depth, enqueue/dequeue counts, overflow spawns.

**When NOT to use:**

- Single-threaded sequential code → `fn`.
- Shared-memory concurrency → `collapse`/`observe` with atomics, not actors.
- Simple async/await → use `async fn` + `await`.
- You just want a thread → actors are heavier-weight, mailbox-driven concurrency.

**Canonical shape:**

```kn
actor FoldRelay:
    state bias: Int = 11
    state turns: Int = 0

    on Fold(reply_to: P, request: Int):
        self.turns = self.turns + 1
        let value = ((request * 17) + self.bias + self.turns) % MODULUS
        send reply_to.Reply(value = value)

// Usage:
let relay = spawn FoldRelay(bias = 11)
let result = ask(relay, "Fold", 42)
```

**Actor rules:**

- `state` declarations with initializers.
- `on MessageName(params)` handlers.
- `spawn Actor(field = value)` requires named init arguments.
- `send target.Message(field = value)` requires named message fields.
- `ask(actor, "Message", payload)` for request/reply.
- The `reply_to: P` parameter in handlers enables the reply port — the LLVM lane expects this naming.
- Actor telemetry: `actor_scheduler_queue_depth()`, `actor_scheduler_total_enqueued()`, etc.

**Worker pool pattern (from actor_ownership_backpressure benchmark):**

```kn
let w0 = spawn BackpressureRelay(bias = 5)
let w1 = spawn BackpressureRelay(bias = 7)
let w2 = spawn BackpressureRelay(bias = 11)
let w3 = spawn BackpressureRelay(bias = 13)
// ... warmup asks ...
var i: Int = 0
while i < rounds:
    let lane: Int = i & 3
    acc = (acc + ask_worker(lane, w0, w1, w2, w3, acc + i)) % MODULUS
    i = i + 1
```

### `collapse` / `observe` / `decay` — Explicit Ownership Lifecycle

**When to use:**

- You're managing raw memory (`ptr<T>`) and need explicit aliasing control.
- You need the compiler to verify: exclusive mutation → read-only observation → deterministic teardown.
- The ownership state machine matters for safety (no use-after-decay, no double-collapse).
- You want ownership telemetry and Z3-backed verification of the state transitions.

**When NOT to use:**

- Normal stack variables → Kain manages those automatically.
- World state → worlds have their own ownership model.
- Actor state → actors own their state.
- You don't have raw pointers → nothing to own.

**Canonical shape:**

```kn
fn ownership_lane(count: Int) -> Int with Unsafe:
    let mut cells: ptr<Int> = alloc_zeroed(count, "Int")

    // Exclusive mutation region
    collapse cells:
        var i: Int = 0
        while i < count:
            mem_store(ptr_offset(cells, i, "Int"), (i * 17) + 3, "Int")
            i = i + 1
        0   // collapse body expression is the "yield" value

    // Read-only observation region
    let observed: Int = observe cells:
        fold_cells(cells, count)

    // Deterministic teardown
    decay cells
    return observed
```

**Ownership state machine:**

```
Idle ──collapse──▶ Collapsed ──(block end)──▶ Idle
Idle ──observe───▶ Observed(n) ──(block end)──▶ Observed(n-1) or Idle
Idle ──decay─────▶ Decayed (terminal)
```

- `collapse` only legal from `Idle`.
- `observe` legal from `Idle` or `Observed(n)` (nested observation).
- `decay` only legal from `Idle` (after all observations end).

______________________________________________________________________

## Foreign Boundaries (include, import)

### `include ... as ...` — C Header Import

**When to use:**

- You need OS APIs (Windows, POSIX, Linux).
- You need vendor SDKs (Vulkan, CUDA, FFmpeg).
- You need C runtime functions (stdio, math, mman).

**Two forms:**

```kn
// Local header (relative to source file)
include native/win32_shim.h as win

// System header (registry-backed, libclang-powered)
include <windows.h> as win       // 605 extracted functions
include <vulkan/vulkan.h> as vk  // 755 extracted functions
include <stdio.h> as cstdio
include <math.h> as cmath
```

**Rules:**

- Local header: companion `.c` must be in same directory.
- System header: angle brackets, resolved through `crates/c-ffi/system_headers.toml`.
- Libclang extraction (clang-sys 0.29) handles WINAPI, \_\_declspec, SAL annotations, macros.
- No shim headers needed for complex vendor headers — libclang eats them raw.
- Tagged-int caveat: Kain's `0` encodes as `(0 << 3) | 1` in LLVM IR — when passed to `void*` params, the tag leaks. Workaround: declare pointer params as `unsigned long long` or `uintptr_t` in shim headers.

### `import` — Python Host Objects

**When to use:**

- You need Python libraries (NumPy, PyTorch, json, PIL, Qt, etc.).
- Kain should own the app logic; Python owns the library ecosystem.

**Canonical shape:**

```kn
import json as py_json

fn encode(value: String) -> String:
    return py_json.dumps(value, separators = [",", ":"])
```

**Key insight:** Named Kain args lower to Python kwargs automatically. `py_json.dumps(value, separators = [",", ":"])` becomes `json.dumps(value, separators=[",", ":"])` on the Python side. No `python_call_attr_kwargs` wrapper needed.

______________________________________________________________________

## GPU Lane (shader, dispatch, std::gpu, std::graphics)

### `shader compute` — GPU Kernel Authoring in Kain

**When to use:**

- You're writing a GPU compute kernel.
- You're writing a vertex or fragment shader.
- The kernel is the deliverable, not host orchestration.

**When NOT to use:**

- Host-side GPU orchestration → use `dispatch "key" [x, y, z]`.
- Resource policy → use `std::gpu`.
- Graphics command recording → use `std::graphics`.

**Canonical shape:**

```kn
shader compute MyKernel(id: UVec3) -> Void workgroup(8, 8, 1):
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1

    comptime:
        let compute = (
            [32, 32, 1],
            [
                ("src", "f32", ["grid"], "input", "kain.shared.buffer"),
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

**GPU pipeline map:**

```
.kn source → kain-core parse/typecheck → split:
  → CPU host lane: LLVM IR → native ABI calls → dispatch keyword
  → SPIR-V lane: crates/gpu → rspirv → canonical .spv
  → CUDA/PTX lane: crates/gpu → direct PTX emission → NVIDIA Driver API
```

### `dispatch "key" [x, y, z]` — Host-Side GPU Launch

**Key truths:**

- Built-in keyword — no `use std::cuda` needed.
- Compute key MUST be a string literal: `"shader::KernelName::compute"`.
- Dimensions are workgroup counts, not thread counts.
- The function must be annotated `with GPU, Unsafe`.
- Runtime auto-selects Vulkan or CUDA executor.
- Sidecars (`shader_bundle.json`, `kain_compute_residency.json`) are auto-discovered.

______________________________________________________________________

## The Fusion Pattern — When Everything Comes Together

A high-value Kain file typically combines 3+ semantic layers. This is not over-engineering — it's how Kain earns its compile-time guarantees.

**Minimal fusion checklist:**

- [ ] `world` + `entangle` for state authority and mirroring
- [ ] `patch` for journaled mutations (with epoch counters)
- [ ] `law` for invariants on those mutations
- [ ] `converge` for platform-specific fast paths
- [ ] `orchestrate` for multi-stage pipelines
- [ ] `pulse` for temporal cadence
- [ ] `resonate` for reactive state-change handlers
- [ ] `actor` for concurrent message-processing units
- [ ] `collapse`/`observe`/`decay` for raw memory ownership
- [ ] `axiom` for capability assumptions
- [ ] `teleport` for cross-world handoff
- [ ] `shatter struct` for SoA layout intent

**The 24-tet effects engine (843 lines) and fusion_chain benchmark (550 lines) each use ALL of these in one file.** That's not an accident — it's a proof that Kain's semantic stack composes.

______________________________________________________________________

## The Fusion Chain Benchmark — Canonical Causal Chain Patterns

`benchmark/cases_v2/fusion_chain.kn` is the definitive executable reference for how Kain's semantic primitives compose in a single program. It exercises 7 distinct causal-chain scenarios, each proving that the semantics don't just coexist — they *causally depend* on each other. Here are the patterns it establishes that every Kain author should know.

### Pattern 1: Resonate → Orchestrate Join Point

A `resonate` handler calls an `orchestrate` pipeline. This is the canonical way to chain temporal reactivity into a typed stage graph:

```kn
resonate FusionAuthority.signal dampen 0 ms:
    FusionAuthority.last_old = resonate_old_i64
    FusionAuthority.last_new = resonate_new_i64
    FusionAuthority.shadow = fusion_signal_pipeline(
        resonate_new_i64 + FusionAuthority.tick,
        FusionAuthority.tick
    )
```

Where `fusion_signal_pipeline` is:

```kn
orchestrate fusion_signal_pipeline(value: Int, tick: Int) -> Int:
    stage host_mix: cpu fusion_mix(value + tick) when capability("cpu.scalar") residency host transfer none policy telemetry_prefer_cpu
    stage fast_mix: converge fusion_fast_mix(host_mix, tick) deps [host_mix] residency host policy static
    return fast_mix
```

**Why this matters:** The resonate handler doesn't just set a flag — it pushes the new value through a full orchestrate pipeline (cpu → converge) and writes the result to a *different* world field (`shadow`). Downstream actors read `shadow`, not the original signal. This is cascaded semantic processing — state change → pipeline → new state → actor consumption.

### Pattern 2: Actor Cascade (Spawn-Delegate-Verify)

An actor can spawn a verifier actor, delegate work via `send`, and the verifier replies directly to the original caller:

```kn
actor FusionWorker:
    state bias: Int = 0
    state multiplier: Int = 3

    on Compute(reply_to: P, val: Int):
        let result = (val * self.multiplier + self.bias) % FUSION_MODULUS
        let verifier = spawn FusionVerifier(expected_min = 0)
        send verifier.VerifyAndReply(reply_to = reply_to, val = result)
        // Control flow passes to verifier — worker does NOT reply directly

actor FusionVerifier:
    state expected_min: Int = 0

    on VerifyAndReply(reply_to: P, val: Int):
        let valid = val >= self.expected_min
        if valid == false:
            send reply_to.Reply(value = -99)
            return    // early return inside actor handler — fine
        send reply_to.Reply(value = val)
```

**Why this matters:** This is actor supervision and delegation in Kain. The worker spawns a verifier per-request (ephemeral actor), passes the original `reply_to: P` port through, and the verifier becomes the responder. The original `ask()` caller gets the verifier's reply. This is genuine actor composition, not a library abstraction.

### Pattern 3: Ownership + Teleport Inside an Actor Handler

An actor message handler can do the full `collapse → observe → decay` lifecycle and then `teleport` a shard cross-world:

```kn
actor FusionTeleporter:
    state teleports_done: Int = 0

    on ShatterAndSend(reply_to: P, payload: Int):
        self.teleports_done = self.teleports_done + 1

        // Step 1: Allocate and own raw memory
        let cell_count = 4
        let mut cells: ptr<Int> = alloc_zeroed(cell_count, "Int")

        // Step 2: Exclusive mutation
        collapse cells:
            var i: Int = 0
            while i < cell_count:
                mem_store(ptr_offset(cells, i, "Int"), (tick * (i + 1) * 7) % MODULUS, "Int")
                i = i + 1
            0

        // Step 3: Read-only observation
        let head: Int = observe cells:
            mem_load(ptr_offset(cells, 0, "Int"), "Int")

        // Step 4: Deterministic teardown
        decay cells

        // Step 5: Build shatter payload, teleport cross-world
        let shard = FusionShard { bias: 42, phase: 13, tick: tick, checksum: head, alive: true }
        let moved = teleport shard from FusionAuthority to FusionMirror via fusion_shard_bus

        send reply_to.Reply(value = fusion_shard_score(moved))
```

**Why this matters:** This proves that the full ownership lifecycle (allocate → collapse → observe → decay) composes with actor message handling AND cross-world teleport in a single handler. No C malloc/free. No manual synchronization. The compiler owns the state machine transitions.

### Pattern 4: The Telemetry Delta Guard (Prove It's Real)

Every benchmark case snapshots telemetry before the loop and verifies deltas after. If a semantic primitive didn't actually fire, the test returns a negative error code:

```kn
fn fusion_full_causal_chain_checksum(iterations: Int, modulus: Int) -> Int with Unsafe:
    let resonate_before = resonate_fire_count()
    let entangle_before = entangle_propagation_count()
    let teleport_before = runtime_machine_teleport_count()
    let patch_before = patch_journal_count()
    let orchestrate_before = orchestrate_stage_count()

    // ... run the causal chain ...

    let resonate_delta = resonate_fire_count() - resonate_before
    let entangle_delta = entangle_propagation_count() - entangle_before
    let teleport_delta = runtime_machine_teleport_count() - teleport_before
    let patch_delta = patch_journal_count() - patch_before
    let orchestrate_delta = orchestrate_stage_count() - orchestrate_before

    if resonate_delta < 1: return -10      // resonate never fired
    if entangle_delta < 1: return -11      // entangle never propagated
    if teleport_delta < 1: return -12      // teleport never happened
    if patch_delta < 1 and patch_before < 256: return -13  // patch journal overflow edge case
    if orchestrate_delta < 1: return -14   // orchestrate never ran

    return acc
```

**Why this matters:** A checksum alone can be coincidentally correct. The telemetry delta guard is the *proof* that the semantic machinery actually engaged. Error codes are specific (-10 = resonate, -11 = entangle, etc.) so you know exactly which layer failed. This pattern should be mandatory for any benchmark or proof blade that claims to exercise semantic features.

### Pattern 5: The `ask()` Single-Payload Constraint

`ask(actor, "MessageName", payload)` currently accepts a single `Int` payload. When you need to pass multiple values, pack them:

```kn
const PACK_SHIFT: Int = 100000  // ceiling above max individual value

fn pack(a: Int, b: Int) -> Int:
    return a + b * PACK_SHIFT

fn unpack_a(packed: Int) -> Int:
    return packed % PACK_SHIFT

fn unpack_b(packed: Int) -> Int:
    return packed / PACK_SHIFT

// Usage:
let reply = ask(relay, "Signal", pack(shadow_val, tick))
```

**Why this matters:** Multi-field messages are not yet natively supported in the `ask()` lane. The packing pattern is the current workaround. Name your packing functions clearly (`fusion_pack`/`fusion_unpack_a`/`fusion_unpack_b`) so the encoding is self-documenting. Use a `PACK_SHIFT` constant that's well above your maximum individual value to avoid collisions.

### Pattern 6: Re-Entrant Guard Verification

When a `resonate` handler calls code that might trigger another patch (which could re-trigger the same resonate), the dampen/reentry guard should absorb the cascade. Prove it:

```kn
fn fusion_resonate_reentrant_guard_checksum(iterations: Int, modulus: Int) -> Int:
    let absorb_before = resonate_absorb_count()

    while index < iterations:
        let actor_reply = ask(relay, "Signal", pack(value, index))
        let tick = fusion_strike_signal(FusionAuthority, actor_reply)  // triggers resonate
        // ...

    let absorb_delta = resonate_absorb_count() - absorb_before
    if absorb_delta > iterations:
        return -20   // re-entrant cascade — absorb count exploded
    return acc
```

**Why this matters:** Without this guard, a resonate → patch → resonate cycle could deadlock or storm. The `resonate_absorb_count()` counter tells you how many firings were *suppressed* by the dampen window. If it exceeds `iterations`, something is recursively re-entering. The test proves the guard works.

### The Full Causal Chain (All 7 Layers, One Loop Body)

The signature case (`fusion_full_causal_chain_checksum`) compresses every layer into one loop iteration:

```text
iteration:
  1. patch fusion_strike_signal(authority, signal)     → LAYER 1: world mutation
  2. [resonate fires automatically]                     → LAYER 2: resonate tripwire
  3. [orchestrate pipeline runs inside resonate]        → LAYER 3: orchestrate stage graph
  4. read FusionAuthority.shadow                        → LAYER 2+3 result consumed
  5. read FusionMirror.signal_copy (entangle propagated)→ LAYER 4: entangle propagation
  6. ask(relay, "Signal", ...)                          → LAYER 5: actor message
  7. ask(teleporter, "ShatterAndSend", ...)             → LAYER 6: ownership + teleport
  8. fusion_land_teleport(authority, chain_result)      → LAYER 7: world receives result
```

**That's 8 operations crossing all 7 semantic layers in every single iteration.** The loops run 64-256 iterations, each one exercising the full causal chain. This is not a demo — it's a stress test.

______________________________________________________________________

## Beyond the Obvious — Creative Semantic Abuse (The Hack Side)

> *"It effectively places two rats in a maze and pins them against finding the fastest path."*

`blades/experiments/convergence/` is the canonical proof that Kain's semantic constructs are **general-purpose relationship descriptors**, not domain-locked features. The same `converge` that picks an AVX2 lane can pick a maze-solving strategy. The same `orchestrate` that schedules GPU compute can run BFS, A\*, and random walk simultaneously and compare results. The constructs describe **relationships between things** (spec vs alternative, stage dependencies, state authority, invariant constraints) — not the things themselves.

This is the anti-anti-pattern: do not assume a construct is "only for" its most obvious use case.

### Converge Is Strategy Selection, Not Just Performance

Obvious use: `spec reference` + `fast avx2_lane when capability("cpu.x86.avx2")` — pick the fastest implementation.

Creative use from `quantum_maze_run`:

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

**What's happening:** Three pathfinding strategies compete. `reference` is the conservative heuristic. `greedy_rat` is an optimistic biased heuristic. `chaos_rat` is a random walk with heat-based timeout. `verify random(8)` ensures the winning strategy is consistent across random inputs. The `converge` selector picks whichever lane matches the capability, and the verify clause proves they produce comparable results.

**The generalization:** `converge` is a **multi-strategy selection construct**. The `spec` lane is the ground truth. The `fast` lanes are competing alternatives. `verify random(N)` is the arbitration mechanism. This applies to:

- Solver strategies (BFS vs A\* vs Dijkstra vs greedy)
- Search heuristics (conservative vs optimistic vs random)
- Optimization approaches (exact vs approximate vs learned)
- Simulation models (high-fidelity vs fast-approximate vs statistical)

### Orchestrate Is Algorithm Composition, Not Just Compute Pipeline

Obvious use: `stage gpu_result: gpu kernel(data)` → `stage host_result: cpu process(gpu_result)` — GPU-to-CPU pipeline.

Creative use from `rat_frame_step`:

```kn
orchestrate rat_frame_step(maze: ptr<Int>, start: Int, target: Int, telemetry: RatTelemetry) -> Int:
    // Run ALL THREE algorithms in one frame
    let pure_distance: Int = kain run_bfs_trace(maze, start, target, ...)        // BFS
    let greedy_distance: Int = kain run_astar_trace(maze, start, target, ...)    // A*
    let chaos_distance: Int = kain run_chaos_trace(maze, start, target, ...)     // Random
    let winner_distance: Int = kain quantum_maze_run(signature, start, target, ...) // converge picks winner
    let committed = kain commit_search(telemetry, ...)                            // patch records result
    return committed + pure + greedy + chaos + winner
```

**What's happening:** All three algorithms run in the same orchestrate frame using `kain` stage runtime. They clear their trails, run their searches, then converge selects the winner. The patch records which strategy won and the distances. This is a **multi-algorithm comparison graph**, not a compute pipeline.

**The generalization:** `orchestrate` is a **typed multi-stage composition graph**. Stages don't have to be different runtimes — they can be different algorithms, different models, different strategies, all using `kain` runtime. Use `deps`, `requires`, and `policy` clauses to express which stages depend on which results, which invariants must hold, and how the runtime should schedule them.

### World Is Experiment State, Not Just App State

Obvious use: `world RenderAuthority: state frame: Int = 0` — application state with UI surface.

Creative use from `RatTelemetry`:

```kn
world RatTelemetry:
    state maze: ptr<Int> = int_to_ptr(0, "Int")        // raw maze buffer pointer
    state pure_trail: ptr<Int> = int_to_ptr(0, "Int")  // BFS trail buffer
    state greedy_trail: ptr<Int> = int_to_ptr(0, "Int")// A* trail buffer
    state chaos_trail: ptr<Int> = int_to_ptr(0, "Int") // random trail buffer
    state frame: Int = 0                                 // experiment frame counter
    state best_distance: Int = 0                         // winning distance
    state best_lane: Int = 0                             // which rat won (0=bfs, 1=greedy, 2=chaos)
    state pure_count: Int = 0                            // BFS win count
    state greedy_count: Int = 0                          // A* win count
    state chaos_count: Int = 0                           // random win count
    state status: Int = 0                                // experiment health
    surface native_ui => SpeculativeScentVisualizer
```

**What's happening:** The world holds not just application state but **experiment telemetry** — raw buffer pointers, per-algorithm win counts, frame signatures, status codes. The `native_ui` surface is a visualization dashboard ("SpeculativeScentVisualizer") that paints all three rat trails simultaneously.

**The generalization:** `world` is a **typed, surfaced state container**. The state can be anything — raw pointers, counters, signatures, status codes. The surface can be a visualization dashboard, not just a UI panel. Use worlds for any state that needs compiler-owned tracking, surface projection, or cross-boundary visibility.

### Patch Is Experiment Journaling, Not Just Data Mutation

Obvious use: `patch set_params(world, value) -> Int:` — record a parameter change.

Creative use from `commit_search`:

```kn
patch commit_search(
    authority: RatTelemetry,
    frame: Int,
    start_index: Int,
    target_index: Int,
    pure_distance: Int,      // BFS result
    greedy_distance: Int,    // A* result
    chaos_distance: Int,     // random result
    winner_distance: Int     // converge-selected winner
) -> Int:
    authority.frame = frame
    // Determine which rat won (compare distances, handle -1 failures)
    var safe_pure: Int = 1000000000
    if pure_distance >= 0: safe_pure = pure_distance
    // ... compare, pick winner, record best_lane ...
    if safe_pure <= safe_greedy and safe_pure <= safe_chaos:
        authority.best_lane = 0    // BFS won
    // ...
    authority.frame_signature = ((frame * 31) + authority.best_distance + ...) % MODULUS
    if rat_distance_non_negative(authority.best_distance) == false:
        authority.status = 12      // invariant violation
    return authority.frame_signature
```

**What's happening:** The patch records an entire experiment frame — which rat won, what distances, lane selection, frame signature. It also validates invariants inline (distance non-negative) and sets error status codes. This is **experiment frame recording**, not just state mutation.

**The generalization:** `patch` is a **journaled state transition with validation**. The "state" can be experiment telemetry. The "journal" records which strategy won, what the distances were, and the frame signature. Use patches whenever you need auditability of state transitions — experiment frames, simulation steps, search iterations, optimization epochs.

### Law Is Domain Model Validation, Not Just Parameter Bounds

Obvious use: `law value_in_range(v: Int) -> Bool:` — parameter bounds checking.

Creative use from the rat experiment (9 laws):

```kn
law rat_cell_in_bounds(index: Int, cell_count: Int) -> Bool:
    return index >= 0 and index < cell_count

law rat_coordinate_in_bounds(x: Int, y: Int, width: Int, height: Int) -> Bool:
    return x >= 0 and y >= 0 and x < width and y < height

law rat_start_target_distinct(start_index: Int, target_index: Int, cell_count: Int) -> Bool:
    return rat_cell_in_bounds(start_index, cell_count)
        and rat_cell_in_bounds(target_index, cell_count)
        and start_index != target_index

law rat_maze_geometry_valid(width: Int, height: Int) -> Bool:
    return width >= 4 and height >= 4

law rat_heat_visible(heat: Int) -> Bool:
    return heat >= 0 and heat < 256
```

**What's happening:** These laws validate the **domain model itself** — maze geometry, cell bounds, start/target distinctness, trail capacity, heat visibility. They're not parameter bounds; they're **world-model integrity constraints**. The main function calls `rat_law_lane()` at startup to verify all laws are satisfiable before the experiment begins.

**The generalization:** `law` is a **domain invariant** — any predicate about your world-model that must hold for the system to be meaningful. Use laws for:

- Geometry constraints (valid dimensions, non-degenerate shapes)
- Topology invariants (start ≠ target, connectivity)
- Capacity bounds (trail length ≤ buffer size)
- Domain-specific physics (heat must be in visible spectrum)
- State-machine invariants (status codes only take valid values)

### Shatter Struct Is Experiment Layout, Not Just Hot Data

Obvious use: `shatter struct Particle:` — SoA for SIMD particle simulation.

Creative use from the rat experiment:

```kn
shatter struct TrailSample:
    cell: Int      // which maze cell
    step: Int      // which step in the search
    lane: Int      // which rat (0=bfs, 1=greedy, 2=chaos)
    heat: Int      // search intensity

shatter struct MazeTile:
    wall: Int      // is it a wall?
    scent: Int     // how many rats visited
    visit: Int     // visit count
    seen: Bool     // has any rat been here?

shatter struct RatPulseEcho:
    current: Int   // current position
    target: Int    // target position
    distance: Int  // best distance found
    turn: Int      // actor turn count
```

**What's happening:** These are **experiment data schemas** with SoA layout. `TrailSample` records a single step in any rat's trail. `MazeTile` tracks per-cell visitation. `RatPulseEcho` carries actor communication payloads. The shatter layout makes them cache-friendly for the frame loop that reads all three trails simultaneously.

**The generalization:** `shatter struct` is **lane-oriented data layout** for any data that's accessed by lane rather than by record. If you're reading all `cell` fields across all samples, all `step` fields, etc. — shatter them. This applies to logs, traces, telemetry records, and simulation outputs, not just particle systems.

### Actor Is Simulation Agent, Not Just Service Worker

Obvious use: `actor Relay: on Compute(reply_to, val):` — worker in a pool.

Creative use from the rat experiment:

```kn
actor CheeseOracle:       // Dynamic target offset — "where's the cheese?"
    state bias: Int = 19
    on Taste(reply_to: P, frame: Int):
        let offset = ((frame * 7) + self.bias + self.turns) % 5
        send reply_to.Reply(value = offset)

actor SchrodingersRat:    // Path follower — "the rat itself"
    state current_pos: Int = 0
    on Pulse(reply_to: P, request: Int):
        let distance = unpack_rat_distance(request)
        let target_pos = unpack_rat_target(request)
        self.current_pos = advance_along_path(self.current_pos, target_pos, ...)
        send reply_to.Reply(value = self.current_pos)

actor TrailArchivist:     // Frame recorder — "the lab notebook"
    state samples: Int = 0
    state checksum: Int = 0
    on Record(reply_to: P, sample: Int):
        self.samples = self.samples + 1
        self.checksum = ((self.checksum * 31) + sample + self.samples) % MODULUS
        send reply_to.Reply(value = self.checksum)
```

**What's happening:** Three actors form a simulation loop. `CheeseOracle` generates dynamic target offsets (simulating moving cheese). `SchrodingersRat` advances along the best path from the converge result. `TrailArchivist` records every frame's data with an accumulating checksum. These are **simulation agents with independent state and behavior**, not service workers.

**The generalization:** `actor` is a **stateful autonomous agent**. The state can be simulation position, accumulated knowledge, or recording history. The message handlers can be sensory inputs ("Taste"), action triggers ("Pulse"), or recording requests ("Record"). Use actors for any concurrent entity that owns state over time — simulation agents, game entities, sensor processors, data loggers, not just service workers.

### The Meta-Pattern: Constructs Are Relationships, Not Domains

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `converge` | "Performance lane selector" | **Strategy selector** — any spec + alternative(s) with verification |
| `orchestrate` | "Compute pipeline graph" | **Multi-stage composition** — any typed stage graph with dependencies, residency, fallback |
| `world` | "Application state container" | **Surfaced state authority** — any state that needs compiler tracking, surface projection, or cross-boundary visibility |
| `patch` | "Journaled data mutation" | **Journaled transition** — any state change you want auditable, replayable, or undo-able |
| `law` | "Parameter bounds checker" | **Domain invariant** — any predicate about your world-model that must hold |
| `shatter struct` | "SIMD-friendly layout" | **Lane-oriented layout** — any data accessed by field-lane rather than by record |
| `actor` | "Concurrent worker" | **Autonomous agent** — any stateful entity that communicates via messages |
| `resonate` | "Reactive state handler" | **Causal tripwire** — any "when X changes, do Y" relationship |
| `pulse` | "Frame clock" | **Temporal driver** — any recurring beat that owns scheduling |
| `teleport` | "Zero-copy transfer" | **Destructive handoff** — any ownership transfer between authorities |
| `entangle` | "State mirroring" | **Compile-time coupling** — any bidirectional state relationship with policy |
| `component` | "UI widget definition" | **Renderable view** — any typed, stateful, composable visual unit with props, state, methods, and JSX body. Can nest other components. Is to UI what `fn` is to logic. |

### When to Use the Hack Side

Reach for creative semantic usage when:

- You're building a **simulation** (agents, environments, frames, telemetry)
- You're doing **algorithmic experimentation** (multiple strategies, comparison, verification)
- You're building a **research tool** (data collection, visualization, auditing)
- You're modeling a **domain** with real-world constraints (geometry, topology, physics)
- You're building a **dashboard** (live telemetry, multi-view state, frame recording)
- You need **auditability** (every state transition recorded, every invariant checked)

**The litmus test:** If you find yourself writing plain `fn` wrappers that manually track state, compare strategies, record results, and validate constraints — you're reinventing what `converge`, `orchestrate`, `world`, `patch`, and `law` already do. The constructs are the machinery. The domain is up to you.

______________________________________________________________________

## Anti-Patterns — What NOT To Do

| Anti-Pattern | Why It's Wrong | The Right Way |
|---|---|---|
| `component` only as `render <panel title="x" />` for world surfaces | Component is a full React model; using it as a title wrapper wastes props/state/methods/JSX composition | Compose components, use state, nest `<Other />` calls |
| `fn set_param(w, v)` instead of `patch` | No journal, no telemetry, no epoch tracking | `patch set_param(w, v) -> Int:` |
| `if v < 0 or v > MAX: return` instead of `law` | No compile witness, no contract | `law in_bounds(v) -> Bool:` |
| Plain `while` + `sleep` instead of `pulse` | No jitter tolerance, no missed-beat tracking | `pulse clock every 16ms:` |
| Two plain structs with sync code instead of `world + entangle` | Manual sync, no compiler-owned propagation | `world + entangle with single_writer` |
| `if target == "llvm"` inside `fn` instead of `converge` | No spec lane, no verify, no telemetry | `converge with spec + fast lanes` |
| Sequential `fn` calls instead of `orchestrate` | No graph metadata, no residency, no fallback | `orchestrate with stage graph` |
| Comments about exclusivity instead of `collapse` | No compile enforcement, no state machine | `collapse cells: ...` |
| Shared mutable globals instead of `actor` | No message ordering, no backpressure visibility | `actor with state + on handlers` |
| `Unsafe` as a trash can for all effects | Masks the real danger boundary | Use `Unsafe` ONLY for raw pointers/asm/ABI |
| Shader logic in C strings instead of `shader compute` | No typecheck, no SPIR-V validation | `shader compute` items in `.kn` files |

______________________________________________________________________

## Quick Decision Table

| You want to... | Reach for... | NOT... |
|---|---|---|
| Define a reusable UI widget with state | `component` | `fn` that returns strings |
| Compose UI from smaller pieces | `component` calling `<Other />` | Manual UI node reconciliation |
| Track global state with surfaces | `world` | global `let mut` or module-level `var` |
| Mirror state to another surface | `world` + `entangle` | Manual copy in a `fn` |
| Bind a world to a root UI view | `world` with `surface => Component` | Ad hoc render function |
| Record a state mutation | `patch` | Plain world field assignment |
| Enforce a runtime constraint | `law` | `if` + `return` |
| Pick the best implementation for the platform | `converge` | `if target == "..."` |
| Model a multi-stage computation pipeline | `orchestrate` | Sequential `fn` calls |
| Run something every N milliseconds | `pulse` | `while true { sleep }` |
| React to a state change without polling | `resonate` | Poll in a `pulse` or loop |
| Declare a capability assumption | `axiom` | Comment or `if capability_exists()` |
| Control data layout for hot paths | `shatter struct` | Normal `struct` + manual layout |
| Move data across world boundaries | `teleport` | Copy + assign |
| Concurrent stateful message processing | `actor` | Threads + shared state |
| Exclusive raw memory mutation | `collapse` | Comment "// exclusive access" |
| Read-only raw memory access | `observe` | Plain `mem_load` |
| Destroy raw memory | `decay` | `free` or leak |
| Call Windows/Vulkan/C functions | `include <header.h> as alias` | Manual FFI shim boilerplate |
| Use Python libraries | `import module as alias` | Subprocess or manual bridge |
| Write a GPU kernel | `shader compute` | C string or inline PTX |
| Launch a GPU kernel from host | `dispatch "key" [x, y, z]` | Raw CUDA/Vulkan API calls |
| Describe GPU resource policy | `std::gpu` | Raw buffer handles |
| Record graphics commands | `std::graphics` | Raw Vulkan/D3D API |

______________________________________________________________________

## Authoring Heuristics

1. **Start with the ladder.** Before typing `fn`, ask: does Layer 1-7 have a better answer?

1. **One construct, one concern.** A `world` defines state; a `patch` mutates it; a `law` constrains it; a `resonate` reacts to it. Don't make a `patch` also do the validation of a `law`.

1. **Epoch counters are the universal heartbeat.** When a `patch` changes world state, increment an epoch. Entangle propagation, resonate handlers, and orchestrate dependency tracking all benefit from explicit "something changed" signals.

1. **Dual-world authority+mirror is the canonical state topology.** Authority = mutable, native_ui surface. Mirror = read-only, web surface. Entangle connects them.

1. **Verify your converge lanes.** `verify random(N)` is cheap insurance. Use it.

1. **Orchestrate clauses are not optional decoration.** Dependencies (`deps`), residency (`host` vs `shared` vs `device`), transfers, fallbacks, and guards are the point — they make the pipeline visible to the compiler and runtime.

1. **Actors need warmup.** Always call `ask` at least once before the timing loop. Cold actor paths have different scheduler behavior.

1. **Ownership proves intent.** `collapse` says "I am mutating this exclusively." `observe` says "I am only reading." `decay` says "I am done forever." These are contracts, not comments.

1. **Effects are not decoration.** `with Pure`, `with IO`, `with Unsafe`, `with GPU` are compile-checked capability gates. Callers must be at least as permissive as callees.

1. **Telemetry is the proof.** After any semantic construct, check the counters: `patch_journal_count()`, `entangle_propagation_count()`, `converge_mismatch_count()`, `resonate_fire_count()`, `orchestrate_stage_count()`, `actor_scheduler_queue_depth()`. If they're zero, your semantic feature isn't actually firing.

1. **Component is the UI atom — compose it.** Every `component` in the repo currently looks like `render <panel title="x" />` because they're all used as world surface declarations. But component is a full React model: props, state, methods, JSX composition (`<Other />`), for/if, expressions. A file can host 10,000 components and zero worlds. Don't let the world-surface pattern trick you into underusing the most powerful UI primitive in the language.

______________________________________________________________________

## Validation Ladder

For any non-trivial Kain file:

```powershell
# 1. Typecheck
kain check <file.kn> --target llvm

# 2. Compile + run
kain run <file.kn> --target llvm

# 3. For GPU kernels
kain gpu-artifacts <kernel.comp.kn> --target spirv --output .kain/gpu/kernel.spv
spirv-val --target-env vulkan1.3 .kain/gpu/kernel.spv

# 4. For projects
kain check <project-dir> --target llvm
kain run <project-dir> --target llvm

# 5. For benchmark proof
python benchmark/run.py --case <case-name> --languages kain --runs 1 --warmups 0

# 6. For attrition (teardown health)
python attrition/run.py --help

# 7. For Z3 proofs on unsafe invariants
# Use z3-mcp when changing pointer math, bounds, state machines, or layout rules
```

______________________________________________________________________

## References

- **CATALOG.MD** — All 110 Kain keywords with categories and syntax examples.
- **GLOSSARY.MD** — Every Kain concept with definitions and source locations.
- **lang-semantics SKILL** — Feature index with AST anchors and validation ladders.
- **lang-systems SKILL** — Actors, ownership, raw memory, atomics, ISA escape hatches.
- **lang-gpu SKILL** — Shaders, dispatch, GPU artifact flow, host graphics loops.
- **convergence rat experiment** (6 files, `blades/experiments/convergence/src/`) — **The hack side.** Proves Kain's semantic constructs are general-purpose relationship descriptors, not domain-locked features. `converge` as multi-strategy selection (BFS vs A\* vs random walk rats competing in a maze). `orchestrate` as multi-algorithm comparison graph. `world` as experiment telemetry container with visualization surface. `patch` as experiment frame journaling. `law` as domain model validation (9 geometry/topology/constraint laws). `shatter struct` as experiment data layout. `actor` as simulation agents (oracle, rat, archivist). Python interop for live visualization. The constructs describe **relationships**, not domains.
- **fusion_chain.kn** (550 lines, `benchmark/cases_v2/fusion_chain.kn`) — **The definitive reference.** 7 causal-chain cases proving all 15+ semantic primitives compose in a single program. Establishes 6 canonical patterns: resonate→orchestrate join point, actor cascade (spawn-delegate-verify), ownership+teleport inside actor handler, telemetry delta guards, ask() single-payload packing, and re-entrant guard verification. This is the executable truth the rule book is derived from.
- **resonate_py_effects.kn** (843 lines, `blades/python/24_tet/src/`) — The most comprehensive single-file semantic stack demonstration. World/entangle/law/patch/converge/orchestrate/pulse/resonate all in service of real-time audio effects.
- **actor_ownership_backpressure benchmark** — Actor swarm + ownership region + world/entangle + converge/orchestrate.
