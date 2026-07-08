# UI Demos — Baseline Assessment
> **Date:** 2026-06-25  
> **Source:** 3 runtime-agent investigations | dare_me2.exe segfault | component_basic.exe blank window  
> **Status:** PRE-FIX BASELINE — document before any further changes

---

## Agent 1: Build & Linkage Audit (`da617ebf`)

**Verdict: Build system is healthy. The exe is correctly linked.**

| Artifact | Status |
|----------|--------|
| `X:/.kain/lib/kain_runtime.lib` | ✅ 3.3 MB, 41 .obj files, built Jun 25 19:49 |
| `native_ui_surface.obj` in .lib | ✅ Archive member #23 |
| `component_surface.obj` in .lib | ✅ Archive member #62 |
| `native_ui_surface.c` in TOML | ✅ Line 48 of `native_core_runtime.toml` |
| Smoke test compilation | ✅ Passes (no runtime execution test) |
| `bare_min.exe` (no UI) | ✅ Exit code 42 (correct) |
| `dare_me2.exe` Win32 imports | ✅ USER32.dll, GDI32.dll — confirms UI code linked |
| Surface registration (CRT init) | ✅ `.CRT$XCU` section present |

**Root cause ruled out:** Missing .obj files, TOML gaps, linker omission, stale .lib.

**Suspicion:** Runtime segfault inside UI subsystem execution, not a build/linkage failure. Candidates: Win32 window creation failure, null framebuffer, or crash handler self-destruct.

---

## Agent 2: Segfault Investigation (`75e0c28f`)

**Verdict: The C runtime changes are correct. Crash is not from modified C code.**

Five vectors examined:

| Vector | File | Finding |
|--------|------|---------|
| `getenv("KAIN_UI_BG")` | `ui_renderer.c:419` | ✅ Safe — `getenv` buffer is per-thread static, pointer valid through immediate use |
| `#include "ui_font.h"` | `native_ui_surface.c:24` | ✅ Safe — pure header, include-guarded, no name conflicts with `<windows.h>` |
| `native_ui_load_default_font()` | `native_ui_surface.c:208` | ✅ Safe — DEAD CODE (call commented out), all failure paths handled |
| GDI changes (full-rect BitBlt, DIB deletion) | `ui_host_adapter.c` | ✅ Safe — guarded by NULL checks, stale DIB cleaned after `CreateWindowExA` |
| Struct layout ABI mismatch | `ui_system_internal.h` | ✅ Not plausible — no field reordering, same header for compiler + runtime |

**Suspicion:** Compiler/runtime version mismatch. Crash is component-tree-size-dependent (affects dare_me2 but not component_basic). Codegen issue rather than runtime logic bug.

**Additional finding:** Blank window on component_basic — `ui_render_frame` clears framebuffer but finds no renderable nodes (either no nodes with `in_use && parent_id == 0`, or nodes fail `nw <= 0 || nh <= 0` size check).

---

## Agent 3: dare_me2 vs component_basic Comparison (`2af8e2b7`)

**Verdict: Surface loops and vtable dispatch are structurally identical. One anomaly found.**

### Identical between both programs:
- Init order: `kain_actor_runtime_init()` → `abi_runtime_init()` → surface loop
- All 15 vtable slots accessed in same order
- Session lifecycle: resolve → create → attach → window_open → frame loop → destroy
- Surface registration via `abi_runtime_init()` → `kain_component_surface_register("native_ui", ...)`

### Differences:

| Aspect | dare_me2 | component_basic |
|--------|----------|-----------------|
| Crash behavior | **SEGFAULT immediate** (exit 139) | **Runs** (blank window, exit 124 timeout) |
| `rc_release` calls | ❌ None — pooled strings leak | ✅ Called in helper fns (`title_text()`, etc.) |
| Style strings set | 6× `element_set_attr_string` (fill_color, ink_color) | 0 string style attributes |
| World state | Empty `{}` — no fields | Has `title: String`, `count: Int`, `status: String` |
| World init function | None (empty world) | `__kain_init_world_AppWorld()` called by helpers |

### Anomaly found:

**`element_set_attr_i64` type mismatch** — In both programs, the compiler passes `i8*` (string pointer) where `int64_t` is expected:

```llvm
; Slot 5 signature: void (i64, i64, i8*, i64)*
call void %slot5(i64 %sid, i64 %eid, i8* %key, i8* %value_ptr)
;                                    ^^^^ i8* passed where i64 expected
```

Works on x86-64 (same register width) but risks ABI issues. Both programs have this — not unique to dare_me2.

---

## Crash Summary

| What | Finding |
|------|---------|
| **Build system** | Healthy — correct .lib, correct linkage, CRT init runs |
| **C runtime changes** | All safe — no crash vectors in modified files |
| **Vtable dispatch** | Identical between crashing and non-crashing programs |
| **Likely root cause** | Compiler/runtime version mismatch or codegen quality issue |
| **Confirmed NOT cause** | Missing .obj, TOML gaps, linker omission, font loading code, `getenv` usage, GDI changes, struct ABI drift |
| **Blank window (component_basic)** | Separate issue — `ui_render_frame` finds no renderable nodes |
| **Next diagnostic step** | Run under debugger (cdb/WinDbg) for exact fault address + callstack |

---

## Test File Taxonomy

```
X:/blades/ui_demos/
├── src/                    ← authored demo source files
├── test/                   ← test harness files
├── oracle-extended/        ← Oracle-based automation
├── .gitignore
├── *.kn                    ← variation test files (v1-v9)
└── BASELINE.md             ← this file
```
