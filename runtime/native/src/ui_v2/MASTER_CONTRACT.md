# MASTER_CONTRACT.md — Kaintana ↔ Core Runtime Integration

**Synthesis of 4-Part Core Contract Analysis (P1–P4)**
**Date:** 2026-06-27
**Total Sources Analyzed:** 4 contracts (3,297 lines) + KAINTANA.md + MASTER_PLATFORM.md
**Core Runtime Scope:** 59 `src/core/*.c` files (55,813 lines), 50+ `include/*.h` headers
**Integration Points Identified:** 86 (18 P0 blocking, 27 P1 critical, 17 P2 important, 4 P3 nice-to-have, 20 never)
**Target:** Kaintana (`ui_v2/`) — the Kain UI framework redesign

---

## 1. Executive Summary

The Kain core runtime (`runtime/native/src/core/`) is a 59-file, 55,813-line C11 substrate providing arena allocation, input event routing, a 24-slot component surface vtable, service registry, machine-stones (pulse/teleport/axiom), diagnostics, profiling, crash forensics, GPU loader shims, platform detection, and 140+ Z3 proof packs. Kaintana MUST integrate with this substrate instead of duplicating it because: (1) the arena allocator is CBMC-proven (833 assertions), giving Kaintana O(1) frame cleanup with no memory bugs; (2) the input system is Z3-proven for collision-free event routing, saving reimplementation of action/axis dispatch; (3) the service registry has Z3-proven perfect-hash O(1) capability queries; (4) the Kain compiler's LLVM codegen emits calls into the `KainComponentSurface` vtable — any divergence means broken compilation; (5) the machine-stones subsystem provides pulse-driven animation timing, teleport for zero-copy surface-to-surface state handoff, and axiom for capability-gated GPU backend selection. KUIF's fatal mistake was building its OWN arena (fixed arrays in session struct), its OWN input system (raw `abi_ui_*` calls), its OWN everything — sitting completely disconnected from the core runtime. Kaintana must do the opposite: every alloy to the proven core, zero duplication.

---

## 2. The 86 Integration Points — Reordered by Priority

From the Master Integration Table (P4, Section B). Reordered P0 first, with key details for each.

### P0 — Blocking (18 points, ship-stoppers)

| # | Integration Point | Core Subsystem | Kaintana File(s) | What Changes | Status |
|---|---|---|---|---|---|
| 1 | Node arena allocation | `arena.h` (KAIN_ARENA_MAIN) | `tree.c`, `arena.c`, `internal.h` | Replace ALL `malloc`/`free` with `kain_arena_alloc_lo()`. Per-frame marker: `kain_frame_set_marker()` in begin_frame, `kain_frame_release_to_last_marker()` in end_frame. 64KB default arena, growable via `kain_virtual_reserve_and_commit()`. | Not started |
| 2 | 24-slot vtable contract alignment | `component_surface.h` | `kaintana.h` | `kaintana.h` must MATCH the exact slot order and signatures of `component_surface.h`. Slot numbers are ABSOLUTE — never reorder. The compiler's LLVM codegen calls through this layout. | Partial (copy exists) |
| 3 | Surface registration | `component_surface.c` | `kaintana.h`, `kaintana_init()` | Call `kain_component_surface_register("kaintana", &vtable)` at startup. Cannot use CRT constructor (old system's approach). Must happen BEFORE `renderer_session_boot()`. | Not started |
| 4 | Software framebuffer access | `kain_host.h` (kainHostVTable) | `backends/win32/host_win32.c` | Use `get_framebuffer()` slot from host vtable for pixel buffer pointer. On Win32: `CreateDIBSection` top-down 32-bit DIB, BitBlt on present. | Win32 host exists (needs retrofitting) |
| 5 | Renderer session boot | `renderer_session.h/c` | `kaintana_init()`, `kaintana_backend_init()` | Call `renderer_session_boot()` to resolve `RENDERER_BACKEND` env var, probe GPU surface shims (Vulkan/D3D12/WebGPU), fall back to software. The resolved `active_surface` is the vtable Kaintana calls through. | Not started |
| 6 | Platform detection | `platform.h` | `kaintana_init()`, `backends/` | `kain_platform_current_kind()` → select backend. Check service mask for clipboard/app-host/input availability. | Not started |
| 7 | TOML manifest update | `native_core_runtime.toml` | ALL ui_v2/ files | Replace old `src/ui/*.c` entries with `src/ui_v2/*.c`. Add backend files under `windows_sources`/`linux_sources`. Run `py -3 scripts/python/update_runtime.py` to regenerate Bazel BUILD files. | Not started |
| 8 | Input event query (replace abi_ui_*) | `input_system.h` | `tree.c` | Replace old `abi_ui_push_event`/`abi_ui_poll_event` with `abi_input_push_event`/`abi_input_event_count`/`abi_input_event_kind`. The existing `kain_input.c` in `src/ui/kain/` was a thin wrapper — now call the ABI directly. | Not started |
| 9 | Semantic action dispatch | `input_system.h` | `tree.c` | Use `abi_input_action_pressed`/`down`/`released` for slot 23 callback dispatch. Z3-proven action binding. | Not started |
| 10 | Input begin_frame per frame | `input_system.h` | `tree.c` | Call `abi_input_begin_frame()` each frame to enable action/axis event reduction. | Not started |
| 11 | Diagnostics subsystem | `diagnostics.h/c` | ALL core files | Use `KAIN_DIAG_SUBSYSTEM_UI` (code range 5000-5999) for invalid attributes, layout overflow, render errors. Use `KainDiagnosticCollector` for batch startup validation. | Not started |
| 12 | Crash handler (free) | `crash_handler.h/c` | ALL Kaintana files | No changes needed. The crash handler binary-searches the compiler-emitted crash table. Kaintana structures are already in-process. **Free correctness via existing infrastructure.** | Already works |
| 13 | Arena heap validation (free) | `memory.c` (KainAllocHeader) | `arena.c` | Kaintana's arena uses raw bump alloc — no headers per node. Separate from heap-managed memory. Already clean. | Already works |
| 14 | Stable key reconciliation | `handle.h` | `tree.c`, `hash_table.c` | Use `kain_handle_table_acquire`/`resolve` for stable key→node mapping. Z3-proven generation-tagged handles reject stale refs. | Not started |
| 15 | Node state persistence | `handle.h` | `tree.c` | State keys (i64/f64/string) stored on hidden `__kain_state_root` node, resolved via handle. | Not started |
| 16 | FNV-1a hash for stable keys | `input_system.h` (same hash) | `hash_table.c`, `tree.c` | Use FNV-1a open-addressing hash (same as input_system). Z3-proven collision bounds. | Not started |
| 17 | Arena backing buffer via virtual alloc | `virtual_alloc.h` | `arena.c` | `kain_virtual_reserve_and_commit()` for large dynamic arenas. Default 64KB, growable. | Not started |
| 18 | Service registration | `services.h/c` | `kaintana_init()` | Register `"ui.kaintana"` service key. Future: check `KAIN_SERVICE_KEY_PLATFORM_INPUT` before using input. | Not started |

### P1 — Critical (27 points)

Selected highlights:

| # | Integration Point | Core Subsystem | Kaintana File(s) | Priority |
|---|---|---|---|---|
| 19 | Backend catalog registration | `renderer_backend.h/c` | `kaintana_backend_init()` | P1 |
| 20 | Startup validation collector | `diagnostics.h/c` | `kaintana_init()` | P1 |
| 21 | Runtime tiers | `runtime_tiers.h` | ALL core files | P1 |
| 22 | Profile scopes | `profile.h/c` | `box_math.c`, `damage.c`, `draw_pixels.c` | P1 |
| 23 | Strict-aliasing-safe pixel ops | `memory.h` (memcpy-based) | `draw_pixels.c` | P1 |
| 24 | Python ABI tests | N/A (via ctypes) | `tests/python_abi/` | P1 |
| 25 | Fuzz suite | `diagnostics.h/c` | `tests/fuzzer/` | P1 |
| 26 | Service availability gates | `services.h/c` | `backends/*/host_*.c` | P1 |
| 27 | Surface registration name | `component_surface.c` | `kaintana.h` | P1 |
| 28 | Input session lifecycle | `input_system.h` | `tree.c` | P1 |
| 29 | Pulse animation timing | `machine_stones.h/c` | `kaintana_animation_init()`, `animation.kn` | P1 |
| 30 | Entangle surface discovery | `entangle.h/c` | `tree.c` | P1 |
| 31 | Axiom capability gating | `machine_stones.h/c` | `backends/vulkan/` | P1 |
| 32 | Version check at init | `version.h/c` | `kaintana.h` | P1 |
| 33 | Deferred free for damage | `deferred_free.h/c` | `damage.c` | P1 |
| 34 | Converge backend selection | `converge.h/c` | `core.kn` (Kain-side) | P1 |
| 35 | Teleport surface handoff | `machine_stones.h/c` | `surface_teleport()` | P1 |
| 36 | Atomic state flags | `memory.h` (atomic ops) | `draw_pixels.c`, `damage.c` | P1 |
| 37 | Platform library check | `platform_library.h` | `backends/vulkan/` (Phase 2) | P1 |

### P2 — Important (17 points)

| # | Integration Point | Core Subsystem | Priority |
|---|---|---|---|
| 38 | GPU extension for shader_canvas | `gpu_surface_extension.h` | P2 |
| 39 | Slot 23 callback binding | `ui_system.c` (internal) | P2 |
| 40 | GPU surface shim integration | Vulkan/D3D12/webgpu_surface_shim.c | P2 |
| 41 | Fan-out parallel render | `fanout.h/c` | P3 |
| 42 | Actor-backed event loop | `actor.h/c` | P3 |
| 43 | Async render tasks | `async.h/c` | P3 |
| 44 | Extended CBMC harness | `test/cbmc/check_arena.c` | P2 |
| 45 | CPU detection for SIMD | `cpu.h/c` | P3 |
| 46 | Compatibility class | `compatibility.h/c` | P3 |
| 47 | Contract bundle | `contract.h/c` | P3 |
| 48 | Safe pointer arithmetic | `memory.h` (`__kain_ptr_offset`) | P2 |
| 49 | Math subset (animations) | `c_runtime_math_subset.h` | P3 |
| 50 | Reflection for dynamic attrs | `reflection.h/c` | P3 |

### P3 — Nice-to-Have (4 points)

| # | Integration Point | Core Subsystem | Priority |
|---|---|---|---|
| 51 | Reflection-driven attribute registration | `reflection.h/c` | P3 |
| 52 | SIMD-accelerated pixel fill | `simd.h/c` + `cpu.h/c` | P3 |
| 53 | GPU extension load_shader/set_uniform stubs | `gpu_surface_extension.h` | P3 |
| 54 | Self-updating pointer for arena relocation (if ever needed) | `self_updating_ptr.h` | P3 |

### ❌ Never — Features Kaintana Does NOT Touch (20 points)

| Feature | Core Subsystem | Rationale |
|---------|---------------|-----------|
| Actor system | `actor.h/c` | UI events → patches, not actor messages |
| Async runtime | `async.h/c` | Kaintana is single-threaded per session |
| Event bus | `event.h/c` | For Kain `emit`, not UI event routing |
| CUDA compute | `cuda_runtime.h/c` | GPU compute, not graphics |
| Network/HTTP | `net_system.h/c` | Separate subsystem |
| Process management | `process_system.h/c` | Separate subsystem |
| Audio | `audio_system.h/c` | Separate subsystem |
| Python bridge | `python_runtime*.c` | Separate subsystem |
| Scene graph | `scene.h/c` | 3D scene management, not 2D UI |
| Graphics bundle | `graphics_bundle.h` | GPU compute scheduling, not UI |
| Realtime bundle loader | `realtime.h/c` | Scene/asset loading, NOT frame timing |
| Interop contracts | `interop_contracts.h/c` | Cross-runtime buffer sharing |
| Zero-copy interop | `interop_zero_copy.h/c` | Cross-runtime buffer sharing |
| JSON parser | `json.h/c` | Not used by Kaintana core |
| Wire benchmark | `wire.h/c` | Pure benchmark, NOT transport |
| SIMD benchmarks | `simd.h/c` | Phase 2+ optimization only |
| Attrition harness | `attrition.h/c` | Runtime certification pipeline |
| Ray-sphere benchmark | `ray_sphere_benchmark.h/c` | Pure benchmark |
| Fixup relocation | `fixup.h/c` | Arena never reallocs individual nodes |
| LRU cache | `lru.h` | Hash table uses different strategy |

---

## 3. Complete File Index — Core Runtime with Kaintana Relevance

All 59 `src/core/*.c` files, abridged with Kaintana relevance assessment. Total: 55,813 lines.

### ✅ DIRECTLY NEEDED (15 files)

| # | File | Lines | Purpose | Kaintana Role |
|---|---|---|---|---|
| 1 | `arena.c` | 205 | Grow-only arena allocator, 4 arenas (MAIN/SHARED/GPU/SCRATCH), frame markers. **833 CBMC assertions.** | Replace ALL `malloc`/`free` in `tree.c`. Per-frame O(1) cleanup. |
| 2 | `component_surface.c` | 201 | 16-entry name→vtable registry, GPU backend routing via `RENDERER_BACKEND`. | Kaintana registers here via `kain_component_surface_register()`. |
| 3 | `input_system.c` | 875 | Action/axis/replay input system. 512 bindings, 256 actions, 128 axes. **Z3-proven collision-free.** | Replace `abi_ui_push_event`/`abi_ui_poll_event` with `abi_input_*` calls. |
| 4 | `services.c` | 1,350 | ~35 canonical services, perfect-hash lookup. **Z3-proven collision-free.** | Register `"ui.kaintana"`. Check service availability before using platform features. |
| 5 | `diagnostics.c` | 514 | Structured diagnostics. `KAIN_DIAG_SUBSYSTEM_UI` (5000-5999). 32-entry collector. | Emit diagnostics on invalid attrs, layout overflows, render errors. |
| 6 | `profile.c` | 120 | Scoped profiling zones. 3 tiers. `KAIN_PROFILE_SCOPE("kaintana_layout")`. | Wrap hot paths in box_math, damage, draw_pixels. |
| 7 | `machine_stones.c` | 653 | Pulse (64-slot background thread), teleport (zero-copy handoff), axiom (capability predicate), shatter (SoA layout). **6 Z3 proofs.** | Pulse→animation timing, teleport→surface state handoff, axiom→GPU capability gating. |
| 8 | `ownership.c` | 1,183 | Collapse/observe/decay state machine. 4096 regions, golden-ratio hash. **38 Z3 proofs.** | Unsafe component GPU staging buffer guards. |
| 9 | `entangle.c` | 86 | World entangle registry. 128 max bindings. **5 Z3 proofs.** | Multi-surface state sync discovery. |
| 10 | `converge.c` | 177 | Multi-lane dispatch. 8 lanes, 64 telemetry samples, 64-entry tune cache. **5 Z3 proofs.** | Backend selection converge block (Kain-side, not C). |
| 11 | `renderer_backend.c` | 107 | Static catalog of 3 GPU backends. `kain_renderer_backend_active()`. | Kaintana backends register here. |
| 12 | `renderer_session.c` | 397 | Renderer session boot/shutdown. Resolves `RENDERER_BACKEND` env var, probes GPU shims, stores `active_surface`. | Entry point for Kaintana backend initialization. |
| 13 | `version.c` | 133 | Runtime/ABI version constants. `version_check_abi_compatibility()`. | Startup ABI check for Kaintana. |
| 14 | `core.c` | 3,874 | Catch-all: RC allocator, string ops, file I/O, CLI, math, spawn/sleep. | `abi_fs_read_text` for font loading, `to_string` for debug overlay. |
| 15 | `handle.c` | 161 | Generation-tagged runtime handles. Free-list slots. **4 Z3 proofs.** | Stable key→node mapping that survives arena resets. |

### ⚠️ MAYBE RELEVANT (10 files)

| # | File | Lines | Purpose | When Needed |
|---|---|---|---|---|
| 16 | `actor.c` | 4,447 | Full actor runtime. **5,676 CBMC assertions.** | Phase 3: mailbox-driven event loop. |
| 17 | `async.c` | 2,371 | Task/future runtime, wake handles, timers. | Phase 3: GPU fence polling as async tasks. |
| 18 | `stdlib_abi.c` | 3,796 | 150 `abi_*` functions: Option/Result/Future, entangle/converge propagations. | Already provides `@extern` bindings Kaintana widgets use. |
| 19 | `compatibility.c` | 461 | Bundle version validation, hot reload state. | Phase 3: live theme swap via hot reload. |
| 20 | `buddy.c` | 266 | Power-of-two block heap allocator. **2 Z3 proofs.** | Cross-frame persistent data (textures, font atlases). |
| 21 | `deferred_free.c` | 79 | Index-based deferral + flush. | Damage tracking: mark dirty, flush at end_frame. |
| 22 | `cpu.c` | 848 | CPU feature detection, ISA levels, topology. | Phase 2: `abi_cpu_logical_count()` for worker thread count. |
| 23 | `graphics_system.c` | 1,382 | Raw graphics kernel: buffers, SPIR-V, pipelines. | Phase 2: `shader_canvas` loads fragment shaders via SPIR-V. |
| 24 | `virtual_alloc.c` | 118 | OS page management. | Arena backing buffer for large dynamic arenas. |
| 25 | `host_bridge.c` | 437 | Plugin/foreign runtime module registration. | If Kaintana hosts Python/JS bridges. |

### ❌ NOT RELEVANT (34 files)

All remaining files: `attrition.c`, `audio_system.c`, `batch_queue.c`, `bitfield.c`, `contract.c`, `cuda_runtime.c`, `d3d12_surface_shim.c`, `event.c`, `fanout.c`, `fixup.c`, `freestanding_stubs.c`, `interop_contracts.c`, `interop_zero_copy.c`, `json.c`, `json_benchmark.c`, `net_system.c`, `process_system.c`, `python_runtime*.c` (5 files), `ray_sphere_benchmark.c`, `realtime.c`, `reflection.c`, `scene.c`, `simd.c`, `union.c`, `vulkan_stubs.c`, `vulkan_surface_shim.c`, `webgpu_surface_shim.c`, `wire.c`, `lru.h`, `self_updating_ptr.h`, `c_runtime_math_subset.h`.

---

## 4. The Three Integration Paths

### Path A: Arena Allocator (P0)

**What changes:** `tree.c` replaces ALL `malloc`/`free` with arena bump allocation.

**Exact API to call:**

```c
// kaintana.h — add arena to session
typedef struct KaintanaSession {
    KainArena arena;           // <-- ADD THIS
    unsigned char arena_buffer[KAINTANA_ARENA_SIZE];  // 64KB default
    // ... existing fields ...
} KaintanaSession;

// kaintana_init() — initialize arena
void kaintana_init(KaintanaSession* session) {
    kain_arena_init(&session->arena, KAIN_ARENA_MAIN,
                     session->arena_buffer, sizeof(session->arena_buffer),
                     KAIN_MEMTYPE_CPU_ACCESSIBLE);
}

// tree.c — frame lifecycle
void kaintana_begin_frame(KaintanaSession* session, double delta_ms) {
    kain_frame_set_marker(&session->arena);   // save frame start
    abi_input_begin_frame(session->input_sid, delta_ms);
}

void kaintana_end_frame(KaintanaSession* session) {
    kain_frame_release_to_last_marker(&session->arena);  // O(1) cleanup
}

// tree.c — node allocation replaces malloc
KaintanaNode* kaintana_node_alloc(KaintanaSession* session) {
    return (KaintanaNode*)kain_arena_alloc_lo(
        &session->arena,
        sizeof(KaintanaNode),
        _Alignof(KaintanaNode)
    );
}
```

**What proofs this buys:**
- 833 CBMC assertions on arena init, frame markers, alloc_lo/alloc_hi, reset — ALL pass
- Z3-proven: low bump cursor stays before high water, alignment never wraps, header plus payload doesn't wrap
- **No more memory leaks** — per-frame release is O(1), can't forget a free

**Where in frame lifecycle:**
- `kain_frame_set_marker()` at TOP of begin_frame
- `kain_frame_release_to_last_marker()` at END of end_frame
- ALL node allocations happen between marker and release
- Arena reset only needed if session destroyed

**What changes to which files:**
- `kaintana.h`: Add `KainArena` field to session struct
- `internal.h`: Define `KAINTANA_ARENA_SIZE` (64KB default, data-driven)
- `tree.c`: Replace ALL `malloc`/`free` with `kain_arena_alloc_lo()`
- `arena.c` (ui_v2): Arena wrapper that delegates to `kain_arena_init`/`alloc_lo`/`frame_set_marker`/`frame_release_to_last_marker`
- `backends/win32/host_win32.c`: Use `kain_virtual_reserve_and_commit()` for large arenas
- `tests/python_abi/`: Arena stress test (10,000 node pushes, verify no overflow)

---

### Path B: Vtable Convergence (P0)

**What changes:** `kaintana.h` must be a twin of `component_surface.h`. Kaintana implements the 24-slot vtable and registers it.

**The cardinal rule:** Slot numbers are ABSOLUTE. The Kain compiler's LLVM codegen emits calls by slot index. Reordering even one slot silently corrupts all compiled component code.

**Exact contract alignment:**

```c
// kaintana.h INCLUDES component_surface.h or re-exports it
#include <component_surface.h>  // <-- THE SINGLE SOURCE OF TRUTH

// Kaintana adds helper types alongside the vtable:
typedef struct KaintanaInput {  // filled by platform backend before begin_frame
    float mouse_x, mouse_y;
    bool mouse_down[5];
    float scroll_dx, scroll_dy;
    bool keys[256];
    uint32_t input_chars[32];
    int input_char_count;
    bool focus_gained;
    float delta_seconds;
    float display_width, display_height;
    float scale_factor;
} KaintanaInput;

typedef struct KaintanaDrawCmd {  // flat render command, consumed by backends
    float clip_x, clip_y, clip_w, clip_h;
    uint64_t texture_id;
    float x, y, w, h;
    uint32_t fill_color;       // ARGB
    uint32_t stroke_color;
    float stroke_width;
    float corner_radius[4];
    const char* text;
    int64_t font_id;
    float font_size;
    int16_t z_index;
    uint8_t command_type;      // fill, stroke, text, image, clip_start, clip_end
} KaintanaDrawCmd;

typedef struct KaintanaDrawData {
    KaintanaDrawCmd* cmds;
    int cmd_count;
    float display_width, display_height;
    float scale_x, scale_y;    // HiDPI
} KaintanaDrawData;

// Kaintana registration:
void kaintana_surface_register(const char* name, const KainComponentSurface* surface) {
    kain_component_surface_register(name, surface);
}

const KainComponentSurface* kaintana_surface_resolve(const char* name) {
    return kain_component_surface_resolve(name);
}
```

**Critical flow:**

```
Kain compiler emits:  surface->element_begin(sid, parent, "Box", "my-key")
                      ↓
               kaintana.h vtable slot 2
                      ↓
               tree.c: kaintana_node_begin(parent, kind, stable_key)
                      ↓
               arena alloc, hash lookup, return node_id
```

**What proofs this buys:**
- Compiler-vtable compatibility guaranteed by single header source
- Vtable drift between kaintana.h and component_surface.h = compile error
- The existing `native_ui_surface.c` vtable impl provides a reference implementation to diff against

**What changes to which files:**
- `kaintana.h`: Include `component_surface.h` as source of truth. Remove any redefined slot types.
- `tree.c`: Implement all 24 `KainComponentSurface` slots using `kaintana_*` internal functions.
- `kaintana_init()`: Call `kain_component_surface_register("kaintana", &kaintana_vtable)`.
- `internal.h`: Session struct carries the vtable-compatible session_id.

---

### Path C: Input Funnel (P0/P1)

**What changes:** The input pipeline replaces `abi_ui_push_event/poll_event` with `abi_input_push_event/begin_frame/action_pressed`. The `KaintanaInput` struct follows ImGui's 10-function `Add*Event()` pattern but routes through the existing Z3-proven input system.

**Two-layer architecture:**

```
Layer 1: Platform backends → abi_input_push_event()
    host_win32.c: On WM_KEYDOWN/WM_MOUSEMOVE/etc., call
    abi_input_push_event(sid, "keyboard", "kb0", "key_down", "w", 1.0, "", 1.0)

Layer 2: tree.c per-frame query → abi_input_begin_frame()
    At top of kaintana_begin_frame:
        abi_input_begin_frame(session->input_sid, delta_ms);
    
    In kaintana_pump_events (after begin_frame, before end_frame):
        count = abi_input_event_count(session->input_sid);
        for i = 0..count:
            kind = abi_input_event_kind(session->input_sid, i);
            code = abi_input_event_code(session->input_sid, i);
            // route to slot 23 callbacks
        
        if (abi_input_action_pressed(session->input_sid, "click")):
            // invoke on_click callbacks
```

**The KaintanaInput wrapper** (for platform backends, following ImGui's 10-function pattern):

```c
typedef struct KaintanaInput {
    void (*add_mouse_pos)(int64_t sid, float x, float y);
    void (*add_mouse_button)(int64_t sid, int button, bool down);
    void (*add_mouse_wheel)(int64_t sid, float dx, float dy);
    void (*add_key)(int64_t sid, const char* key_name, bool down);
    void (*add_character)(int64_t sid, uint32_t codepoint);
    void (*add_focus)(int64_t sid, bool focused);
    void (*add_touch)(int64_t sid, int id, int phase, float x, float y, float force);
    void (*add_text)(int64_t sid, const char* text);  // clipboard paste, IME commit
} KaintanaInput;
```

Each function is a thin wrapper that calls `abi_input_push_event()` with the correct source_kind/event_kind/code tuple. This keeps the Z3-proven event routing intact while giving platform backends a clean typed API.

**What proofs this buys:**
- Z3-proven: all 9 event kind token signatures are collision-free
- Z3-proven: hash probe bounds for all powers-of-two tables (actions=256, axes=128, events=1024, bindings=512)
- CBMC-verified mailbox semantics for event ring buffer
- **Free correctness** — Kaintana wraps verified infrastructure instead of reimplementing event routing

**What changes to which files:**
- `kaintana.h`: Add `KaintanaInput` wrapper struct
- `tree.c`: Replace `abi_ui_push_event`/`abi_ui_poll_event` with `abi_input_event_count`/`abi_input_event_kind` + `abi_input_action_pressed`
- `tree.c`: Call `abi_input_begin_frame()` at top of `kaintana_begin_frame`
- `backends/win32/host_win32.c`: Call `kaintana_input.add_mouse_pos()` etc. in message pump (WndProc)
- `backends/testing/host_null.c`: Stub input (no-op wrappers)
- `tests/python_abi/`: Drive 24-slot vtable + input system from Python

---

## 5. What KUIF Got Wrong

KUIF (the old UI system in `src/ui/`) made exactly 5 fatal architectural decisions that Kaintana must reverse:

### Mistake 1: Own Arena — Disconnected from Core

KUIF had its own "per-frame arena" implemented as fixed arrays inside the session struct (`KainNativeUiSession`). It was NOT `kain_arena_alloc_lo()` — it was hand-rolled fixed-size buffers with `malloc`/`free` for oversize nodes. Result: 0 CBMC proofs, 0 Z3 proofs, manual memory management in ~3,000 lines of `ui_system.c`.

**Kaintana fix:** `kain_arena_alloc_lo()` from the CBMC-proven arena. Per-frame markers. O(1) cleanup. 833 assertions proven.

### Mistake 2: Own Input System — Wrapped Nothing From Core

KUIF had its own input system: `abi_ui_push_event()` / `abi_ui_poll_event()` in `ui_system.c`. This was a separate event ring buffer with no action/axis binding, no replay, no trace, no agent intent — just raw event kind + text + coordinates. It duplicated what `input_system.c` already did better.

**Kaintana fix:** Direct `abi_input_push_event()` / `abi_input_begin_frame()` / `abi_input_action_pressed()`. Z3-proven collision-free token dispatch. CBMC-verified mailbox. Action binding for free.

### Mistake 3: Own Everything — No Service Registration

KUIF never registered itself in the service registry. There was no `"ui.kaintana"` or `"ui.component_v2"` key. The runtime didn't know a UI system existed until `kain_component_surface_resolve("native_ui")` was called (triggered by CRT constructor). No capability query, no dependency validation.

**Kaintana fix:** Register `"ui.kaintana"` at startup. Check `kain_service_registry_is_available()` before using platform features. Fail early with diagnostics if required services are missing.

### Mistake 4: No Diagnostics Utilization

KUIF never called `kain_diagnostic_create()` with `KAIN_DIAG_SUBSYSTEM_UI`. Every error was either `printf` to stdout or silent failure. No collector, no structured error codes, no startup validation.

**Kaintana fix:** Every invalid attribute, layout overflow, render error routes through `kain_diagnostic_create()` with codes in range 5000-5999. Startup validation via `KainDiagnosticCollector`. Fuzz suite reads diagnostics for pass/fail.

### Mistake 5: No Profiling

KUIF had zero scoped profiling. Hot paths (layout solving, damage propagation, render) were unmeasured.

**Kaintana fix:** `KAIN_PROFILE_SCOPE("kaintana_desired_sizes")` in box_math, `KAIN_PROFILE_SCOPE("kaintana_present")` in backend present. Gated via `runtime_tiers.h` — zero-cost in release.

### The Root Cause

KUIF was written BEFORE the core runtime had stable ABIs for arena, input, diagnostics, and profiling. By the time those subsystems existed and were verified (CBMC, Z3), KUIF was already too large to refactor (~11,000 lines). The Rosetta Stone's first line says it best: *"The UI system should have come last, not first."* Kaintana comes last. The core is ready. Integrate, don't duplicate.

---

## 6. Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| 1 | KArena API changes break `box_math.c` | Low | High | `arena.h` is part of the core runtime public ABI (ABI v0.1.0). Changes require ABI version bump. Kaintana includes `arena.h` directly — caught at compile time. |
| 2 | Vtable drift between `kaintana.h` and `component_surface.h` | Medium | Critical | Single source of truth: `kaintana.h` includes `component_surface.h`. Slot order differences are compile errors. Python ABI test (#24) validates layout at runtime. |
| 3 | Input system ABI changes | Low | High | `input_system.h` is ABI v0.1.0 with Z3-proven internal hashes. Kaintana calls `abi_input_*` functions through standard FFI. ABI compatibility check at startup via `version_check_abi_compatibility()`. |
| 4 | Arena capacity overflow in complex UIs | Medium | Medium | Start with 64KB default, measure real-world usage. Use `kain_virtual_reserve_and_commit()` for growable arenas. Fuzz test at 10,000+ nodes. |
| 5 | Pulse animation conflicts with frame timer | Low | Medium | `kain_machine_pulse_start()` runs on a background thread. Kaintana's `kaintana_begin_frame()` reads `kain_machine_pulse_total_fire_count()` to determine if animation state changed. No direct conflict. |
| 6 | Service `"ui.kaintana"` key collision | Low | Low | `services.h` uses perfect-hash registration. Collision would be detected at compile time by Z3 checks. Kaintana uses the same `kain_service_registry_register()` API as all other services. |
| 7 | Backend selection converge block not supported by LLVM codegen yet | Medium | High | Fall back to env var chain (`RENDERER_BACKEND`) until converge codegen is ready. The C `renderer_session_boot()` already handles this. |
| 8 | Old `abi_ui_*` ABI calls linger in `backends/win32/host_win32.c` | Medium | Medium | Systematic audit: replace every `abi_ui_push_event` with `abi_input_push_event`. Remove `#include "ui_system.h"` from backend files. |
| 9 | CRT constructor auto-registration conflicts with explicit `kaintana_surface_register()` | Low | Low | Ensure `native_ui_surface.c` auto-registration is removed before Kaintana takes over. Or register `"kaintana"` not `"native_ui"` during transition. |
| 10 | Kaintana team unfamiliar with core runtime APIs | Medium | Medium | This document is the reference. Each integration path (Section 4) has exact API names and call sites. File index (Section 3) maps every core file to its Kaintana role. |

---

## 7. Next Steps — Ordered Execution Plan

### Phase 0: Readiness (parallel, no dependencies)
- [ ] **P0-6**: Call `kain_platform_current_kind()` in `kaintana_init()` to detect OS
- [ ] **P1-21**: Include `runtime_tiers.h` and gate assertions via `KAIN_RUNTIME_DIAG_ENABLED()`
- [ ] **P0-12**: Confirm crash handler resolves Kaintana structures (test by deliberate crash in debug build)

### Phase 1: Core Integration (sequential, P0 blocking)
- [ ] **P0-1**: Add `KainArena` to `KaintanaSession`. Replace ALL `malloc`/`free` in `tree.c` with `kain_arena_alloc_lo()`. Wire into frame lifecycle: `kain_frame_set_marker()` in `begin_frame`, `kain_frame_release_to_last_marker()` in `end_frame`.
- [ ] **P0-2**: Make `kaintana.h` include `component_surface.h`. Remove duplicate vtable definitions. Verify slot order matches.
- [ ] **P0-3**: Register Kaintana vtable via `kain_component_surface_register("kaintana", &vtable)` in `kaintana_init()`.
- [ ] **P0-11**: Wire `kain_diagnostic_create()` into `tree.c`/`box_math.c`/`draw_pixels.c` for invalid inputs. Use `KAIN_DIAG_SUBSYSTEM_UI` (5000-5999).
- [ ] **P0-7**: Update `native_core_runtime.toml`: replace old `src/ui/*.c` entries with `src/ui_v2/*.c`. Run `update_runtime.py`.
- [ ] **P0-18**: Register `"ui.kaintana"` service key in `services.h` catalog.

### Phase 2: Event Pipeline (P1, depends on Phase 1)
- [ ] **P0-8**: Replace `abi_ui_push_event`/`abi_ui_poll_event` with `abi_input_push_event`/`abi_input_event_count`/`abi_input_event_kind` in `tree.c`.
- [ ] **P0-9**: Wire `abi_input_action_pressed`/`down`/`released` for slot 23 callback dispatch.
- [ ] **P0-10**: Call `abi_input_begin_frame()` at top of `kaintana_begin_frame()`.
- [ ] **P0-4**/#16: Wire `kain_host_get()` framebuffer access in `backends/win32/host_win32.c`.
- [ ] **P0-17**: Use `kain_virtual_reserve_and_commit()` for arena backing buffer.
- [ ] **P0-14**/#15: Wire `kain_handle_table_acquire`/`resolve` for stable key→node mapping.
- [ ] **P0-16**: FNV-1a hash for stable key lookup (same hash function as input_system).
- [ ] **P0-5**: Wire `renderer_session_boot()` to resolve `RENDERER_BACKEND` env var and select backend.
- [ ] **P1-23**: Enforce strict-aliasing-safe pixel ops via `memcpy` (not `uint64_t*` casts) in `draw_pixels.c`.
- [ ] **P1-31**/#30: Register service gates: check `kain_service_registry_is_available()` before `platform.input`.

### Phase 3: Render Pipeline + Tests (P1)
- [ ] **P1-19**: Register Kaintana backends in `renderer_backend.c` catalog.
- [ ] **P1-22**: Wrap hot paths in `KAIN_PROFILE_SCOPE("kaintana_*")`.
- [ ] **P1-24**: Write Python ctypes ABI tests (`tests/python_abi/`) driving 24-slot vtable from Python.
- [ ] **P1-25**: Write libFuzzer targets (`tests/fuzzer/`) bombing `element_set_attr_string` with random keys/values.
- [ ] **P1-20**: Startup validation collector: batch all init errors in `KainDiagnosticCollector`, print on failure.

### Phase 4: Damage + Semantic Stack (P1-P2)
- [ ] **P1-33**: Deferred free list for damage tracking in `damage.c`.
- [ ] **P1-29**: Wire `kain_machine_pulse_start()` for Kain-side animation timing.
- [ ] **P1-30**: Wire `entangle_registry_get()` for multi-surface discovery.
- [ ] **P1-34**: Author `converge kaintana_backend()` block in `core.kn`.
- [ ] **P1-35**: Wire `kain_machine_teleport_ptr()` for surface-to-surface state handoff.
- [ ] **P1-36**: Atomic state flags via `__kain_atomic_load_seqcst/store_seqcst`.
- [ ] **P1-32**: ABI version check at startup via `version_check_abi_compatibility()`.
- [ ] **P2-44**: Extend CBMC arena harness with Kaintana's grow/reset patterns.
- [ ] **P2-48**: Use `__kain_ptr_offset()` for safe framebuffer row offset computation.

### Phase 5: GPU Backends (P2-P3)
- [ ] **P2-38**: Wire slot 18 (`get_gpu_extension`) for `shader_canvas` backend.
- [ ] **P2-39**: Wire slot 23 (`element_set_callback`) for event→callback dispatch.
- [ ] **P2-40**: Connect Vulkan/D3D12/WebGPU backends via surface shim resolver.
- [ ] **P3-42**/#43: Actor-backed event loop + async render tasks.

### Phase 6: Archive (after Kaintana verified)
- [ ] Move 25 files from `src/ui/` to `archive/legacy/ui/`
- [ ] Move 25 UI headers from `include/` to `archive/legacy/ui_headers/`
- [ ] `include/` goes from 84 headers to 59 (pure core runtime, zero UI pollution)
- [ ] Rename `src/ui_v2/` to `src/ui/`

---

## Appendix A: Proof Leverage Summary

Kaintana gains the following proven-correct infrastructure FOR FREE by integrating with the core runtime instead of duplicating:

| Subsystem | Proof Type | Count | What's Proven |
|---|---|---|---|
| Arena allocator | CBMC | 833 assertions | Init, frame markers, alloc_lo/alloc_hi, reset — ALL pass |
| Arena allocation | Z3 | 6 proofs | Bump bounds, alignment overflow, header+payload wrap |
| Input system | Z3 | 2 proofs | Event kind token signatures collision-free, hash probe bounds |
| Input event ring | CBMC (actor mailbox) | 5,676 assertions | FIFO order, capacity, bounded/unbounded — proven |
| Service registry | Z3 | 4 proofs | Perfect-hash collision-free, spinlock safety, buffer bounds |
| Machine stones | Z3 | 6 proofs | Pulse missed-beat math, shatter lane bounds, teleport exclusivity |
| Ownership | Z3 | 38 proofs | Observer count overflow/underflow, state machine totality, golden-ratio hash |
| Entangle | Z3 | 5 proofs | Text copy bounds, index bounds, atomic early-return, capacity limit |
| Convergence | Z3 | 5 proofs | Cache odd-stride coverage, telemetry ring bounds, De Bruijn CTZ |
| Buddy allocator | Z3 | 2 proofs | Log2/CLZ equivalence, merge span bounds |
| Handles | Z3 | 4 proofs | Magic validation, stale handle rejection, branchless slot extraction |

**Total: 833 CBMC + 72 Z3 proofs = 905 verified invariants that Kaintana inherits.**

---

## Appendix B: File State Tracking

| File | Phase | Status | Notes |
|---|---|---|---|
| `kaintana.h` | Phase 1 | **NEEDS WORK** | Must include component_surface.h, remove duplicate slots, add Input/DrawData types |
| `internal.h` | Phase 1 | **NEEDS WORK** | Add arena field, input_sid, stable_key handle table to session struct |
| `tree.c` | Phase 1 | **NEEDS WORK** | Replace malloc with arena, replace abi_ui_* with abi_input_*, add diagnostics |
| `box_math.c` | Phase 1 | ☑️ Clean | Pure math. Add `KAIN_PROFILE_SCOPE` + tier-gated assertions. |
| `damage.c` | Phase 2 | **NEEDS WORK** | Add deferred_free list integration, atomic flag state |
| `draw_pixels.c` | Phase 2 | **NEEDS WORK** | Enforce strict-aliasing-safe pixel ops, add profiling |
| `arena.c` (ui_v2) | Phase 1 | **NEEDS WORK** | Delegate to kain_arena_init/alloc_lo/frame_set_marker |
| `hash_table.c` | Phase 2 | **NEEDS WORK** | Use FNV-1a hash matching input_system |
| `backends/win32/host_win32.c` | Phase 1 | **NEEDS WORK** | Retrofitted for kainHostVTable, abi_input_push_event, present via BitBlt |
| `backends/testing/host_null.c` | Phase 2 | Not started | ~100 lines, no-op wrapper |
| `tests/python_abi/` | Phase 3 | Not started | ctypes driver for 24-slot vtable |
| `tests/fuzzer/` | Phase 3 | Not started | libFuzzer targets |
| `kaintana_init()` | Phase 1 | **NEEDS WORK** | Full init sequence: arena, vtable register, service register, platform detect, diagnostics |

**Clean (no changes needed):** `kaintana/core.kn`, `kaintana/theme.kn`, `kaintana/layout.kn`, `kaintana/widgets.kn`, `kaintana/animation.kn`, `kaintana/kaintana.kn` — these are Kain-side (`stdlib/kaintana/`) and only call `@extern` bindings.

---

*End of MASTER_CONTRACT.md — the capstone synthesis of all 4 core contract parts, mapping every integration point, API call, proof lever, and risk for Kaintana's integration with the core runtime. The rule is simple: integrate, don't duplicate.*
