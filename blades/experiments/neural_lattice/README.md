# Neural Lattice — Semantic Entanglement Visualized

> *An OpenGL window that watches Kain's compiler-owned semantics in real time. Authority vs Mirror. Collapse vs Decay. Entangled buses pulsing between worlds.*

This experiment is a blade-owned OpenGL presenter that visualizes Kain's semantic constructs as they execute. The Kain side computes a "neural lattice" — a synthetic network of 128 synapses — running through every semantic primitive in sequence. The native C side renders the resulting state as a dual-waveform visualization with 5 interactive modes, each revealing a different semantic relationship.

## What You See

```
┌──────────────────────────────────────────────────────────────┐
│  ┌─────────────────────┐    ENTANGLE    ┌─────────────────────┐ │
│  │     AUTHORITY        │  ◄───B U S───▶ │       MIRROR         │ │
│  │  ╱╲   ╱╲   ╱╲       │  ●  ●  ●  ●  │  ╱╲   ╱╲   ╱╲       │ │
│  │ ╱  ╲_╱  ╲_╱  ╲_     │  ●  ●  ●  ●  │ ╱  ╲_╱  ╲_╱  ╲_     │ │
│  │  amber waveform      │  magenta bus  │  cyan waveform       │ │
│  │  SIGNAL 12345        │  DELTA 0      │  SIGNAL 12345        │ │
│  │  HOT 67              │  EPOCH 8      │  ECHO 23456          │ │
│  └──────────────────────┘  LOCK 511     └──────────────────────┘ │
│                          ENT 3/4                                  │
│                          PATCH 2                                  │
│                          TPORT 1                                  │
│                          SYNC LOCKED                              │
│                                                                   │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┐       │
│  │ ENTANGLE │ COLLAPSE │  DECAY   │  BURST   │  DRIFT   │       │
│  └──────────┴──────────┴──────────┴──────────┴──────────┘       │
└──────────────────────────────────────────────────────────────┘
```

- **Left panel (amber):** CorticalAuthority — the source of truth. Waveform shape driven by signal, hot synapses, lock state.
- **Right panel (cyan):** DeepMirror — the entangled copy. Waveform driven by mirror signal, entangle propagations, graphics score.
- **Center strip:** The entangle bus — 8 lanes with pulsing magenta particles flowing between authority and mirror.
- **Center stats:** Delta (authority − mirror), epoch, lock state, entangle registered/propagated, patch journal count, teleport count.
- **Bottom bar:** 5 clickable mode buttons + mode description text.
- **Hot synapse indicators:** 16-slot glow bars showing how many of the 128 synapses are "hot."

## File Structure

```
src/
├── main.kn                          Entry point. Boots runtime, runs deck, presents window.
├── neural_entangled_sieve.kn        The core engine. 3 worlds, 3 entangles, law, patches,
│                                    converge, actor, pulse with teleport, collapse/observe/decay
│                                    on ghost cells, shatter struct, graphics/UI probes,
│                                    the execute_visual_deck() orchestrator, and the
│                                    run_neural_lattice_demo() proof with telemetry guards.
└── neural_lattice_presenter.kn      Thin Kain wrappers over the native C bridge functions.

native/
├── neural_lattice_bridge.h          C header: 4 functions (probe, run_window, frames, cells, report)
└── neural_lattice_bridge_impl.c     ~700 lines of Win32 + OpenGL: window creation, GL context,
                                     waveform synthesis, scanline rendering, mode buttons,
                                     bus lane particle animation, screenshot BMP writer,
                                     keyboard controls, auto-cycle, frame budget.

build-neural-lattice-bridge.ps1      Compiles the C bridge to .obj via clang.
run.ps1                              Full build+run pipeline with screenshot verification.
KAIN.toml                            Blade manifest with C-FFI library declaration (user32, gdi32, opengl32).
build.kn                             Build graph: check task + native executable with bridge dependency.
```

## How It Works

### The Semantic Engine (`neural_entangled_sieve.kn`)

The Kain side runs a single-pass computation — no frame loop in Kain. The computation exercises every semantic construct and bundles the results into a `NeuralLatticeVisualDeck` struct. The native C bridge receives this deck and runs its own frame loop for visualization.

#### 1. Three Worlds, Three Entangles

```kn
world CorticalAuthority:          // authority — single source of truth
    state network_charge: Int     // primary signal
    state epoch: Int              // mutation counter
    state lock_state: Int         // sync verification
    surface native_ui => SieveDisplayPanel

world DeepMirror:                 // mirror — entangled copy
    state charge_copy: Int
    state epoch_copy: Int
    state lock_copy: Int
    surface web => SieveDisplayPanel

world RogueProjection:            // rogue — deliberately NOT entangled
    state rogue_charge: Int       // drifts independently for comparison
    state rogue_epoch: Int
    surface web => SieveDisplayPanel

entangle CorticalAuthority.network_charge <-> DeepMirror.charge_copy with single_writer
entangle CorticalAuthority.epoch <-> DeepMirror.epoch_copy with single_writer
entangle CorticalAuthority.lock_state <-> DeepMirror.lock_copy with single_writer
```

The **authority+mirror** pattern is the canonical dual-world topology. The **rogue projection** is the twist: a third world that is NOT entangled, used to demonstrate what drift looks like when you compare entangled vs non-entangled state. In DRIFT mode, the visualization uses the rogue's values for the right panel instead of the mirror's — the desync is immediately visible.

#### 2. Synapse Buffer: Collapse → Observe → Decay

The engine allocates a raw buffer of 128 synapses × 4 words each, fills it inside a collapse region, observes the checksum and hot synapse count, then decays it:

```kn
let cells_count = 128 * 4   // 128 synapses, 4 words per synapse
let mut synapses: ptr<Int> = alloc_zeroed(cells_count, "Int")

collapse synapses:
    var index = 0
    while index < 128:
        let base = index * 4
        mem_store(ptr_offset(synapses, base + 0, "Int"), index, "Int")           // id
        mem_store(ptr_offset(synapses, base + 1, "Int"), mix_lattice_charge(...), "Int")  // charge
        mem_store(ptr_offset(synapses, base + 2, "Int", OPTIMAL_BIAS + ...), "Int")       // phase
        mem_store(ptr_offset(synapses, base + 3, "Int"), 2, "Int")               // state
        index = index + 1
    0

let observed_checksum = observe synapses:
    fold_synapse_charge(synapses, 128)       // sum all charges

let hot_synapses = observe synapses:
    count_hot_synapses(synapses, 128)        // count synapses where charge % 7 ≤ 2

decay synapses
```

This is the complete ownership lifecycle in one pass: allocate → exclusive fill → observe checksum → observe hot count → destroy. The "hot synapse" heuristic (charge mod 7 ≤ 2) is arbitrary — it creates a visually interesting ratio that feeds into the waveform amplitude and the hot-slot indicators.

#### 3. Patch, Converge, Actor, Pulse, Teleport

After the synapse buffer, the engine chains the remaining semantics:

```kn
// PATCH: journal a state mutation
let signal = commit_sieve_charge(authority, (checksum + observed_checksum + hot_synapses) % MODULUS)

// ACTOR: fire the NeuralIgniter
let actor_echo = ask(relay, "PulseIgnition", signal + observed_checksum + hot_synapses)

// CONVERGE: platform-selected mixing (used throughout)
converge mix_lattice_charge(value: Int) -> Int:
    spec reference: return mix_charge_scalar(value)
    fast avx2_lane when capability("cpu.x86.avx2"): return ((value * 53) + 13) % MODULUS
    verify random(8)

// COLLAPSE/HELPER: ghost cell ownership on a separate buffer
let collapse_signal = collapse_helper_signal(signal, hot_synapses, authority.lock_state)

// DECAY/HELPER: collapse→observe→collapse→observe→decay on ghost cells
let decay_signal = decay_helper_signal(signal + observed_checksum, actor_echo, hot_synapses)

// BURST: 6-turn actor ask loop
var burst_signal = signal
var burst_turn = 0
while burst_turn < 6:
    burst_signal = ask(relay, "PulseIgnition", burst_signal + hot_synapses + lock_state + ...)
    burst_turn = burst_turn + 1

// DRIFT: commit to the rogue (non-entangled) world
let drift_signal = commit_rogue_charge(rogue, mix_lattice_charge(signal + actor_echo + ...))

// LAW: verify stability
let _stable = charge_is_stable(signal)
```

#### 4. The Pulse Does Teleport

```kn
pulse neural_sieve_beat every 4ms jitter 1ms:
    let node = ShatteredSynapse { id: 101, charge: 999, phase: 0, state: SynapseState::Entangled }
    let moved = teleport node from CorticalAuthority to DeepMirror via pulse_bus
    let _sieve_dt = pulse_tick + moved.charge + moved.phase
```

Every 4ms, the pulse creates a synthetic synapse and **teleports** it cross-world — Authority → Mirror. The teleport count increments, and the visualization's teleport counter ticks up. This is a machine-stone fusion: temporal beat (`pulse`) drives zero-copy cross-world handoff (`teleport`), all inside a single semantic expression.

#### 5. The NeuralLatticeVisualDeck

The `execute_visual_deck()` function bundles everything into a single struct:

```kn
struct NeuralLatticeVisualDeck:
    core: NeuralLatticeCore          // signal, mirror, epoch, lock, checksum, hot, echo
    collapse_signal: Int             // ghost cell collapse result
    collapse_mirror: Int             // mirror snapshot at collapse time
    decay_signal: Int                // ghost cell decay result
    decay_mirror: Int                // mirror snapshot at decay time
    burst_signal: Int                // 6-turn burst result
    burst_mirror: Int                // mirror snapshot at burst time
    drift_signal: Int                // rogue projection value
    entangle_registered: Int         // how many entangles registered
    entangle_propagations: Int       // how many propagations fired
    patch_journal: Int               // patch journal count
    teleport_count: Int              // teleport fire count
```

All 22 fields are passed as raw integers through the C ABI bridge into the native presenter. The presenter uses each mode's authority/mirror pair to drive the waveform rendering.

### The Native OpenGL Presenter (`neural_lattice_bridge_impl.c`)

~700 lines of C that create a Win32 window with an OpenGL 1.1 context and render the visualization loop.

#### Waveform Synthesis

Each frame, the renderer synthesizes two 40-sample waveforms (left/authority, right/mirror) using the Kain-computed seeds:

```c
neural_lattice_build_wave(left_wave, 40,
    active_authority,                        // primary seed
    state->actor_echo + state->hot_synapses, // secondary seed
    left_mode,                               // which mode shapes the wave
    frame_phase,                             // animation time
    hot_ratio, lock_lane,                    // visual parameters
    left_amplitude, 0.0f);                   // amplitude + phase bias
```

The waveform is a composite of sin/cos waves with frequency content derived from the seed values. Each mode applies a different transformation:

| Mode | Waveform Effect |
|------|----------------|
| **ENTANGLE** | Clean sine/cosine composite. Both panels track together. Delta → 0. |
| **COLLAPSE** | Left wave is stair-stepped (quantized). Right panel uses ENTANGLE mode with reduced amplitude — shows the mirror "frozen" at last entangle state. |
| **DECAY** | Left wave amplitude reduced to 0.62×. Right wave fades to 0.28× with temporal decay envelope. The ghost decays over the frame budget. |
| **BURST** | Spike injection: sin(t × 34 − phase × 4) values > 0.72 punch dents into the waveform. Right phase bias added. Bus particles enlarge. |
| **DRIFT** | Left panel stays as ENTANGLE. Right panel uses rogue seeds with +0.55 phase bias and inverted wave contributions. Bus alpha drops to 0.12 (barely visible). Delta increases. |

#### Bus Lane Animation

8 horizontal lanes in the center strip connect the left and right panels. Each lane has a pulsing magenta particle that travels from left to right based on `bus_phase`. In BURST mode, particles are larger. In DRIFT mode, bus alpha drops and the SYNC LOCKED indicator flips to SYNC DRIFT.

#### Interactive Controls

| Input | Action |
|-------|--------|
| Click mode buttons | Switch visualization mode |
| Keys 1-5 | Switch to ENTANGLE/COLLAPSE/DECAY/BURST/DRIFT |
| Space | Toggle auto-cycle (mode changes every 240 frames) |
| R | Reset to ENTANGLE mode |

#### Screenshot Capture

Set `NEURAL_LATTICE_SCREENSHOT_PATH` to a `.bmp` path. After frame 10, the renderer captures a screenshot via `glReadPixels` and writes it as a 24-bit BMP (custom writer, no external library).

### The Proof Guards (`run_neural_lattice_demo()`)

After the deck is computed and the presenter runs, the demo validates that every semantic layer actually fired:

```kn
if deck.entangle_registered < 3:      return 36   // entangle not registered
if deck.entangle_propagations < 1:    return 37   // entangle never propagated
if deck.patch_journal < 2:            return 38   // patch journal empty
if deck.teleport_count < 1:           return 39   // teleport never fired
if frames_presented < 1:              return 32   // presenter didn't render
if cells_drawn < 64:                  return 33   // presenter didn't draw enough
if ui_hash <= 0:                      return 34   // UI probe failed
if graphics_score <= 0:               return 35   // graphics probe failed
```

Specific error codes for each layer. Same telemetry delta guard pattern as fusion_chain.

### Dual Passive Probes

The deck also exercises `std::graphics` and `std::ui` without needing a real GPU or window:

- **`passive_graphics_probe`:** Creates a software graphics session, registers hex-encoded SPIR-V (minimal 4-vertex mesh), draws, presents, inspects draw command count. Returns a composite score. This proves the graphics ABI is reachable.
- **`passive_ui_probe`:** Creates a software UI session, reconciles text nodes, sets style colors/padding, begins a frame, renders boxes and text, submits. Returns a frame hash. This proves the UI ABI is reachable.

Both probes run in-process, headless, and feed their results into the visualization deck.

## The Architectural Insight

This experiment demonstrates a **computation-then-visualization** split that is genuinely novel for a language compiler toolchain:

```
Kain (.kn)                          C (.c)
─────────                          ─────
Owns:                               Owns:
  - Semantic state computation       - OpenGL window + context
  - World/entangle/patch/law         - Waveform synthesis
  - Converge lane selection          - Scanline rendering
  - Actor message passing            - Bus particle animation
  - Collapse/observe/decay           - Mode button UI
  - Pulse timing + teleport          - Screenshot capture
  - Telemetry gathering              - Frame budget loop
  - Proof guards
                                     Receives:
Produces:                              NeuralLatticeVisualDeck
  NeuralLatticeVisualDeck             (22 integers via C ABI)
  (22 integers)
```

**Kain owns semantics. C owns pixels.** The bridge is 22 integers — no pointers, no structs, no callbacks. The entire semantic computation is a single function call (`execute_visual_deck()`) that returns a flat struct. The native side reads those integers and runs a visualization loop. This is the cleanest Kain↔C separation in the entire repo.

## Running It

```powershell
# Full build + run (compiles C bridge, builds Kain exe, executes, verifies outputs)
.\run.ps1

# Interactive mode (no screenshot, window stays open)
.\run.ps1 -Interactive

# Custom frame budget
.\run.ps1 -FrameBudget 360

# With screenshot
$env:NEURAL_LATTICE_SCREENSHOT_PATH = ".kain/run/neural_lattice.bmp"
.\run.ps1
```

Outputs:
- `.kain/run/neural_lattice_report.txt` — full telemetry dump (20+ fields)
- `.kain/run/neural_lattice_window_report.txt` — presenter counters
- `.kain/run/neural_lattice.bmp` — screenshot (non-interactive mode)

## Why This Matters

Every Kain semantic construct is exercised, and every construct's effect is **directly visible** in the OpenGL window:

| Construct | Where It Fires | What You See |
|-----------|---------------|--------------|
| `world` | CorticalAuthority, DeepMirror, RogueProjection | Left/right panels, amber/cyan color coding |
| `entangle` | 3 single_writer couplings | Bus lanes connecting panels; delta counter; SYNC LOCKED/DRIFT |
| `law` | charge_is_stable | Exit code 31 if violated |
| `patch` | commit_sieve_charge, commit_rogue_charge | PATCH counter in center stats; epoch increments |
| `converge` | mix_lattice_charge (avx2 lane) | Waveform seed quality |
| `actor` | NeuralIgniter.PulseIgnition | ECHO value in right panel; burst loop |
| `pulse` | neural_sieve_beat (4ms) | TPORT counter |
| `teleport` | inside pulse body | TPORT counter; proof guard exit 39 |
| `collapse` | synapse buffer fill, ghost cells | COLLAPSE mode waveform quantization |
| `observe` | checksum + hot synapse count | HOT indicator, waveform amplitude |
| `decay` | synapse buffer + ghost cells | DECAY mode waveform fading |
| `shatter struct` | ShatteredSynapse | Type-safe teleport payload |
| `std::graphics` | passive_graphics_probe | graphics_score in right panel seed |
| `std::ui` | passive_ui_probe | ui_hash in left panel seed |
| C ABI bridge | neural_lattice_bridge | The entire window |

This is not a debugger. It's a **domain-specific visualization of compiler-owned semantics**. You can watch entangle propagation lock authority and mirror together. You can see collapse quantize the waveform. You can observe decay fade the ghost. The constructs aren't abstract — they have visible consequences.

## See Also

- `blades/experiments/convergence/` — Rats in a maze: converge as strategy selector, orchestrate as multi-algorithm composition.
- `benchmark/cases_v2/fusion_chain.kn` — 7-layer causal chain benchmark.
- `blades/python/24_tet/src/resonate_py_effects.kn` — Audio effects engine using the full semantic stack.
- `research/how-to-write-kain-rulebook.md` — The full Kain authoring rule book.
