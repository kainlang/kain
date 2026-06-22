# Kain Regression Harness

**Date:** 2026-06-21
**Status:** Built — 4 proof suites, ~42 regression tests, covers 4 newsletters, 8 subsystems, 21 source files.
**Scope:** Every change made to the Kain compiler and runtime in the 2026-06-20/2026-06-21 cycle.
**Philosophy:** Every proof collects structured telemetry. Every test has a documented "what changed" anchor. Every function documents which newsletter/change it covers.

---

## Quick Start

```bash
# Typecheck all proofs
kain check src/regression_suite.kn --json
kain check src/component_surface_proof.kn --json
kain check src/vulkan_abi_proof.kn --json
kain check src/gpu_routing_proof.kn --json
kain check src/stdlib_vulkan_proof.kn --json

# Build and run the full suite
kain run src/regression_suite.kn --target llvm
```

---

## Architecture

```
regression_harness/
  build.kn                          # Bazel-style build graph (target: llvm)
  src/
    regression_suite.kn             # MASTER RUNNER: orchestrates all 4 suites
    telemetry.kn                    # SHARED: telemetry capture + JSON output
    component_surface_proof.kn      # Suite 1: Component Surface Pipeline
    vulkan_abi_proof.kn             # Suite 2: Vulkan ABI Library
    gpu_routing_proof.kn            # Suite 3: GPU Backend Routing
    stdlib_vulkan_proof.kn          # Suite 4: std::vulkan Module
  README.md                         # This file
  out/                              # GENERATED: telemetry JSON files
```

---

## Suite 1: Component Surface Proof (`component_surface_proof.kn`)

**Covers:** Newsletter #2 (Component Surface), Newsletter #3 (Multi-Backend GPU Presenter)
**Source files tested:** `component_surface.h`, `native_ui_surface.c`, `component.rs`
**Changes covered:**
- Codegen: compile_jsx rewrite (component.rs, 1150 lines)
- Codegen: vtable offsets 0-17 (was 0-14, added window_open + host_pump + session_attach_platform)
- C runtime: component_surface.c GPU backend routing via RENDERER_BACKEND env var
- C runtime: native_ui_surface.c auto-attaches winit host on session_create
- C runtime: native_ui_surface.c present now blits GDI framebuffer
- Merge: codegen calls through vtable instead of direct abi_ui_* functions
- Bug fix: state alloca PHI node (was uninitialized on frames 2+)
- Bug fix: state write-back loop (mutations were never persisted)
- Bug fix: sibling stable key indices (were all 0 due to reset inside loop)
- Bug fix: title attribute dropped by C backend (added to allowlist)
- Demos verified: `window_proof/src/main.kn` (17 lines), `window_proof/src/dashboard.kn`, `window_proof/src/world_read.kn`

### Tests (8)

| Test | What It Proves | Failure Mode |
|------|---------------|--------------|
| `test_surface_registry` | native_ui surface registered after runtime_init | session_count < 0 = corruption |
| `test_vtable_slot_count` | 18 vtable slots in codegen + C trait | mismatch breaks all component rendering |
| `test_world_state_read` | Components read world state via WorldName.field | regression in JSX codegen |
| `test_entangle_propagation` | Entangle counters are non-negative | corruption in state sync subsystem |
| `test_patch_journal` | Patch journal counters are non-negative | corruption in journaled mutation |
| `test_heap_validation` | Arena/buddy allocators healthy after component pipeline | memory corruption |
| `test_intent_telemetry` | All 5 intent telemetry counters accessible | crash in machine stones subsystem |
| `law signal_valid` | Law invariant holds after patch mutation | invariant break in state authority |

### Telemetry Collected
- `patch_journal_count()` — must be >= 0
- `entangle_propagation_count()` — must be >= 0
- `runtime_heap_validate()` — must be == 1
- `runtime_machine_teleport_count()` — must be >= 0
- `runtime_machine_pulse_total_fire_count()` — must be >= 0
- `resonate_fire_count()` — must be >= 0
- `converge_mismatch_count()` — must be >= 0
- `orchestrate_stage_count()` — must be >= 0

---

## Suite 2: Vulkan ABI Proof (`vulkan_abi_proof.kn`)

**Covers:** Newsletter #3 (Multi-Backend GPU Presenter), Newsletter #4 (Vulkan Rendering Pipeline)
**Source files tested:** `vulkan_abi.c` (3,520 lines), `vulkan_abi.h`, `vulkan_loader_subset.h`, `component_surface.c`
**Changes covered:**
- vulkan_abi.c sections 10-15: +1,340 lines of rendering pipeline code
- vulkan_loader_subset.h: +29 PFN prototypes for rendering (vkCreateShaderModule through vkGetBufferMemoryRequirements)
- vulkan_abi.h: +17 PFN typedefs, +10 session fields, +2 exported symbols
- component_surface.c: +60 lines GPU backend routing
- 3 GPU shims (vulkan/d3d12/webgpu) with dlopen patterns
- Blade migration: 4 dead stubs deleted (~428 lines), chronosim bridge refactored (~300 lines smaller)

### Tests (10)

| Test | What It Proves | Failure Mode |
|------|---------------|--------------|
| `test_vulkan_capability_probe` | kain_vulkan_runtime_capability returns 0/1 not garbage | crash or corrupted return value |
| `test_vulkan_telemetry` | abi_vulkan_last_status/error accessible without crash | null pointer deref, segfault |
| `test_vulkan_present_counter` | abi_vulkan_present_count >= 0 | corrupted counter |
| `test_vulkan_swapchain_counter` | abi_vulkan_swapchain_recreations >= 0 | corrupted counter |
| `test_graceful_degradation` | vulkan_available() returns 0 (not crash) without GPU | crash on missing GPU driver |
| `test_vulkan_wrapper_error` | vulkan_last_error() returns valid string | null pointer in string return |
| `test_vulkan_wrapper_present` | vulkan_present_count() returns >= 0 | corrupted counter |
| `test_vulkan_vtable_slots` | 18-slot vtable contract maintained | mismatch with codegen offsets |
| `test_uniform_binding_constants` | Binding 0/1/2 = time/resolution/mouse | mismatch with descriptor set layout |
| `test_vulkan_heap_validation` | Heap healthy after Vulkan probe | memory corruption from GPU init |

### Telemetry Collected
- `kain_vulkan_runtime_capability()` — 0 or 1
- `abi_vulkan_last_status()` — <= 0
- `abi_vulkan_last_error()` — valid string
- `abi_vulkan_present_count()` — >= 0
- `abi_vulkan_swapchain_recreations()` — >= 0
- `runtime_heap_validate()` — must be == 1

---

## Suite 3: GPU Routing Proof (`gpu_routing_proof.kn`)

**Covers:** Newsletter #3 (3 ABI paths converged), Newsletter #4 (RENDERER_BACKEND env var routing)
**Source files tested:** `component_surface.c` (resolve_gpu_backend), `vulkan_surface_shim.c`, `d3d12_surface_shim.c`, `webgpu_surface_shim.c`, `stdlib_abi.c`
**Changes covered:**
- component_surface.c: +60 lines GPU backend routing
- vulkan_surface_shim.c: NEW — dlopen + capability probe + vtable fill
- d3d12_surface_shim.c: NEW — COM-based D3D12 backend shim
- webgpu_surface_shim.c: NEW — cross-platform WebGPU shim with WASM path
- stdlib_abi.c: +3 lines — auto-register native_ui surface
- HWND_GAP_RESOLUTION.md: Option D — GDI wins path for native_ui

### Tests (9)

| Test | What It Proves | Failure Mode |
|------|---------------|--------------|
| `test_gdi_path_default` | GDI backend works when RENDERER_BACKEND unset | session_create fails |
| `test_vulkan_routing_probe` | Vulkan capability probe doesn't crash | crash on GPU probe |
| `test_vulkan_without_gpu` | vulkan_available() returns 0 without GPU | crash on missing driver |
| `test_registry_state_after_probe` | Surface registry intact after GPU probe | corruption in registry |
| `test_heap_after_gpu_probe` | Heap healthy after GPU probe | memory corruption |
| `test_backend_name_constants` | "vulkan", "d3d12", "webgpu", "native_ui" match registry | silent routing failure |
| `test_entangle_after_gpu_probe` | Entangle counters healthy after probe | corruption in state sync |
| `test_patch_journal_after_gpu_probe` | Patch journal healthy after probe | corruption in journal |
| `test_intent_after_gpu_probe` | Intent telemetry healthy after probe | corruption in machine stones |

### Telemetry Collected
- GDI session create/destroy lifecycle
- Vulkan capability probe result
- Surface registry session count
- All intent telemetry counters
- Heap validation

---

## Suite 4: stdlib::vulkan Proof (`stdlib_vulkan_proof.kn`)

**Covers:** Newsletter #4 (Vulkan Rendering Pipeline), Newsletter #3 (Stdlib wiring audit)
**Source files tested:** `stdlib/vulkan.kn`, `vulkan_abi.c` exported symbols, `component_surface.c`
**Changes covered:**
- stdlib/vulkan.kn: NEW — 9 public functions, 5 raw @extern + 4 telemetry @extern
- Runtime: vulkan_abi.c new exported symbols (kain_vulkan_abi_load_shader, kain_vulkan_abi_set_uniform)
- Component surface: GPU backend routing in component_surface.c
- Blade migration: 4 dead stubs deleted, chronosim bridge refactored
- Stdlib audit: 73+ modules, 465+ @extern verified against C runtime

### Tests (8)

| Test | What It Proves | Failure Mode |
|------|---------------|--------------|
| `test_vulkan_available_public` | vulkan_available() returns 0/1 | corrupted return |
| `test_uniform_setters_typecheck` | 3 uniform set functions compile + handle errors | runtime crash on invalid session |
| `test_composite_uniform_update` | vulkan_update_shader_uniforms composes all 3 setters | runtime crash |
| `test_read_spirv_path` | vulkan_read_spirv handles missing files gracefully | crash on missing file |
| `test_vulkan_last_error_public` | vulkan_last_error() wrapper returns valid string | null pointer |
| `test_vulkan_present_count_public` | vulkan_present_count() wrapper returns >= 0 | corrupted counter |
| `test_extern_linkable` | All 9 @extern symbols resolved at link time | linker error |
| `test_stdlib_vulkan_heap` | Heap healthy after std::vulkan usage | memory corruption |

### Telemetry Collected
- `kain_vulkan_runtime_capability()` — linked
- `abi_vulkan_last_status()` — linked
- `abi_vulkan_present_count()` — linked
- `abi_vulkan_swapchain_recreations()` — linked
- `kain_vulkan_abi_get_vtable()` — linked
- `runtime_heap_validate()` — must be == 1

---

## Telemetry Output

Every proof file writes telemetry to the `out/` directory using the shared `telemetry.kn` module.

### Generated Files

| File | Contents |
|------|----------|
| `out/component_surface_proof_telemetry.json` | Suite 1 telemetry snapshot |
| `out/vulkan_abi_proof_telemetry.json` | Suite 2 telemetry snapshot |
| `out/gpu_routing_proof_telemetry.json` | Suite 3 telemetry snapshot |
| `out/stdlib_vulkan_proof_telemetry.json` | Suite 4 telemetry snapshot |
| `out/regression_report.json` | Aggregate report with all suite results |

### Per-Test Telemetry Format

```json
{
  "test_name": "component_surface_proof",
  "status": "passed",
  "timestamp_ms": 1782039085731,
  "failures": 0,
  "telemetry": {
    "patch_journal_count": 2,
    "entangle_propagation_count": 0,
    "heap_valid": 1,
    "teleport_count": 0,
    "pulse_fire_count": 0,
    "converge_mismatch_count": 0,
    "orchestrate_stage_count": 0,
    "resonate_fire_count": 0
  }
}
```

### Aggregate Report Format

```json
{
  "suite_name": "Kain Regression Suite",
  "version": "0.1.0",
  "status": "passed",
  "timestamp_ms": 1782039085900,
  "total_suites": 4,
  "passed_suites": 4,
  "failed_suites": 0,
  "heap_valid": 1,
  "telemetry": { ... }
}
```

### Telemetry Counters

| Counter | Meaning | Expected Range |
|---------|---------|---------------|
| `patch_journal_count` | Total patch mutations committed | >= 0 |
| `entangle_propagation_count` | Total entangle sync propagations | >= 0 |
| `heap_valid` | Arena/buddy allocator health | 1 = valid, 0 = corruption |
| `teleport_count` | Cross-world zero-copy handoffs | >= 0 |
| `pulse_fire_count` | Pulse timer firings | >= 0 |
| `converge_mismatch_count` | Converge spec-vs-fast mismatches | >= 0 |
| `orchestrate_stage_count` | Orchestrate stage executions | >= 0 |
| `resonate_fire_count` | Resonate handler invocations | >= 0 |

---

## Changes Covered (Master Index)

### Newsletter #1: GPU System Evolution (2026-06-20)
- `subgroup(N) { }` keyword — AST + parser + divergence validator
- `ShaderStage` enum 4→12 — mesh/task/raytracing stages
- Indirect dispatch — `dispatch "key" from buf` grammar variant
- GPU barrier inference — compiler-owned barrier JSON from orchestrate DAG
- Push constant inference — SPIR-V backend auto-lowering
- Specialization constants — comptime 6-element tuple extension
- Tensor core @extern — 8 intrinsics + 9 capability predicates
- Async compute queue hints — `policy prefer_async_compute`
- Pipeline library types — `PipelineHandle`, `PipelineLibrary`, `DispatchIndirectCommand`
- Memory visibility inference — `with GPU` (no Unsafe) in orchestrate blocks

### Newsletter #2: Component Surface (2026-06-21)
- KainComponentSurface trait — 18-slot vtable contract
- World-surface frame loop — auto-generated LLVM IR
- Component state persistence — PHI node + write-back loop
- Stable keys — retained-mode element reconciliation
- JSX attribute → surface call mapping — 16 attributes mapped
- Full JSX construct support — 7 node types
- Merge story — codegen direct abi_ui_* → vtable calls
- 3 bugs fixed during merge audit

### Newsletter #3: Multi-Backend GPU Presenter (2026-06-21)
- 3 ABI paths converged — std::graphics, std::ui, surface→Component → KainComponentSurface
- Vulkan ABI library (3,520 lines) — dynamic loader, WSI, device, swapchain, pipeline, render pass, draw, descriptors
- D3D12 ABI library — COM-based with native mesh shader support
- WebGPU ABI library — cross-platform with native/WASM dual path
- 4 dead stubs deleted (~428 lines)
- Chronosim bridge refactored (~300 lines smaller)
- Stdlib wiring audit — 73+ modules, 465+ @extern verified

### Newsletter #4: Vulkan Rendering Pipeline (2026-06-21)
- Shader module creation — SPIR-V hex decode + VkShaderModule
- Graphics pipeline creation — embedded 340-byte fullscreen-triangle VS + user FS
- Render pass creation — attachment, subpass, framebuffers
- Draw command recording — bind pipeline, set viewport/scissor, bind descriptors, draw, end render pass
- Descriptor sets & uniform buffers — 3 bindings matching ocean.kn / blackhole.kn
- Exported API — `kain_vulkan_abi_load_shader` + `kain_vulkan_abi_set_uniform`
- GPU backend routing — RENDERER_BACKEND env var in component_surface.c
- `surface vulkan => ShaderFragment` proposal (not yet implemented)

---

## Telemetry Reference

Every test collects and verifies these counters. Any negative value indicates a crash, corruption, or integer underflow.

| Telemetry Function | Source | Meaning |
|-------------------|--------|---------|
| `patch_journal_count()` | runtime/machine stones | Number of patch mutations committed |
| `entangle_propagation_count()` | runtime/machine stones | Number of entangle syncs propagated |
| `runtime_heap_validate()` | runtime/allocator | 1 = valid, 0 = corruption |
| `runtime_machine_teleport_count()` | runtime/machine stones | Number of teleport handoffs |
| `runtime_machine_pulse_total_fire_count()` | runtime/machine stones | Total pulse timer fires |
| `resonate_fire_count()` | runtime/machine stones | Total resonate handler fires |
| `converge_mismatch_count()` | runtime/machine stones | Converge spec-vs-fast mismatches |
| `orchestrate_stage_count()` | runtime/machine stones | Orchestrate stage executions |
| `kain_vulkan_runtime_capability()` | runtime/shim | 0=no Vulkan, 1=Vulkan loaded |
| `abi_vulkan_last_status()` | runtime/shim | Last Vulkan error code (<= 0) |
| `abi_vulkan_present_count()` | runtime/shim | Frames presented to swapchain |
| `abi_vulkan_swapchain_recreations()` | runtime/shim | Swapchain rebuild count |

---

## Known Limitations

### Component Surface (Suite 1)
1. **GDI backend ignores element styles** — `background`, `color` stored but not rendered. Vulkan backend will fix.
2. **Component methods from `{expr}`** — produce runtime errors (known gap #1). Use top-level fns instead.
3. **`pulse` runtime** — causes immediate process exit (known gap #2). Not tested in this suite.
4. **JSX `if` with operators** — `==`, `<`, `>` rejected by parser (known gap #3).

### Vulkan ABI (Suite 2)
1. **No Oracle-verified Vulkan window** — the ABI library builds and links but has never produced a visible GPU window. Tests only verify ABI contract and graceful degradation.
2. **Actual vkQueuePresentKHR** requires physical GPU hardware and a real window handle — CI likely doesn't have this.
3. **D3D12 and WebGPU tests** are build-gated behind `KAIN_RUNTIME_HAS_D3D12` / `KAIN_RUNTIME_HAS_WEBGPU`. Only Vulkan is tested here.

### GPU Routing (Suite 3)
1. **RENDERER_BACKEND env var** cannot be tested from within the harness — it's read by the C runtime at surface resolution time, before Kain code runs. Tests verify the routing code exists and capability probes work.
2. **Actual GPU backend swap** requires setting RENDERER_BACKEND before launch and having the GPU ABI library available on the system.

### stdlib::vulkan (Suite 4)
1. **vulkan_load_shader** requires a real Vulkan session with a valid window handle — cannot be tested without GPU hardware.
2. **vulkan_read_spirv** returns empty string when file absent — the test verifies this gracefully.

---

## Expected Output

```
=== Kain Regression Suite v0.1.0 ===

Suite 1: component_surface_proof ... PASS (0 failures)
Suite 2: vulkan_abi_proof ......... PASS (0 failures)
Suite 3: gpu_routing_proof ........ PASS (0 failures)
Suite 4: stdlib_vulkan_proof ...... PASS (0 failures)

All 4 proof suites passed.
Heap validation: OK
Return code: 0
```

### On Failure

```
Suite 1: component_surface_proof ... FAIL (2 failures)
  - test_heap_validation: FAIL (returned -20)
  - test_world_state_read: FAIL (returned 1)

Return code: 2
```

---

## Files That Enable This Harness

| Layer | File | Role |
|-------|------|------|
| C trait | `runtime/native/include/component_surface.h` | 18-slot KainComponentSurface vtable struct |
| C registry | `runtime/native/src/core/component_surface.c` | Surface name → vtable pointer + GPU routing |
| C backend | `runtime/native/src/ui/native_ui_surface.c` | GDI window backend (reference implementation) |
| C window | `runtime/native/src/ui/ui_host_adapter.c` | CreateWindowExA + GDI framebuffer |
| C init | `runtime/native/src/core/stdlib_abi.c` | Auto-registers native_ui_surface |
| C Vulkan ABI | `runtime/native/extras/vulkan-abi/vulkan_abi.c` | 3,520-line Vulkan rendering library |
| C Vulkan header | `runtime/native/include/vulkan_loader_subset.h` | 73 PFN prototypes |
| C GPU shims | `runtime/native/src/core/vulkan_surface_shim.c` | dlopen + capability probe |
| Rust codegen | `crates/sys-codegen/src/codegen_llvm/component.rs` | JSX → vtable call LLVM IR |
| Rust wiring | `crates/sys-codegen/src/codegen_llvm/mod.rs` | Frame loop flush + world-surface init |
| Kain stdlib | `stdlib/vulkan.kn` | 9 public functions bridging to C ABI |
| Kain stdlib | `stdlib/ui.kn` | 123 @extern to retained-mode UI engine |

---

## Design Notes

1. **Every test function has a `test_` prefix** — grep-friendly, self-documenting.
2. **Every test returns Int** — 0 = pass, negative = crash, positive = failure count. Standardized error signaling.
3. **Every proof file has a world + component** — even if not the primary test target, this proves the component surface pipeline is healthy.
4. **Worlds use law + patch** — exercising the L2 semantic layer proves the compiler's decision ladder is intact.
5. **Telemetry is always verified** — `>= 0` checks catch integer underflow, pointer corruption, and crash aftermath.
6. **Heap validation is always the last check** — if anything corrupts the allocators, the final `runtime_heap_validate()` catch it.
7. **Each suite is independent** — can be built and run standalone: `kain run src/component_surface_proof.kn --target llvm`
