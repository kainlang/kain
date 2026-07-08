# Kaintana Test Architecture — Master Synthesis

**File:** `tests/master_test.md`
**Date:** 2026-06-28
**Status:** Architecture specification
**Purpose:** Unify the test philosophy, directory layout, test categories, phase plan, and backend roles into a single document that drives all Kaintana CI testing.

---

## 1. Philosophy: Test Framework in Kain Itself

Kaintana tests are **written in Kain**, not C, Python, or shell scripts. Kain has native C interop (`include <kaintana.h> as kt`) and Python interop (`import PIL`). This means:

- **Zero FFI glue.** `include <kaintana.h> as kt` compiles to direct LLVM `call` instructions — same address space, same calling convention. No ctypes, no dlopen, no marshalling.
- **Framebuffer as file.** The null backend renders into a `uint32_t*` buffer on the C heap. Kain reads it directly via `ptr_offset`/`mem_load`. Golden `.bin` files are raw pixel dumps for byte-for-byte comparison.
- **TSV as test language.** Test specs are tab-delimited — one row per test case, parseable in ~20 lines of Kain. No YAML, no TOML, no JSON config for metadata.
- **CI-ready JSON output.** Every test run writes structured JSON to stdout. CI ingests this directly.
- **Arena-only allocation.** The test runner uses kaintana's built-in 64KB arena (not malloc) for all per-frame allocations.

---

## 2. The Test Target: What Gets Tested

The Kaintana C substrate (`tree.c`, `box_math.c`, `damage.c`, `draw_pixels.c`, `arena.c`, `hash_table.c`, `color.c`, `attr_table.c`) is compiled as companion translation units to a Kain test runner. Every test calls the **same 34 public functions** (`kt_*` API) that production code uses.

The null backend (`backends/null/host_null.c`) is the ground truth:
- 256 lines, zero platform headers, pure C11
- Renders to `uint32_t* g_fb` in row-major premultiplied ARGB
- 16-deep clip stack
- Handles `KT_CMD_FILL`, `KT_CMD_CLIP`, `KT_CMD_UNCLIP` (others silently skipped)
- Deterministic: same ABI calls → same framebuffer, every time, on every machine

The C test helpers (`kaintana_test_helpers.{c,h}`) expose the framebuffer pointer, width, and height to Kain via `@extern`:

```kain
include <kaintana.h> as kt
include <kaintana_test_helpers.h> as test

fn read_pixel(fb: ptr<u32>, x: Int, y: Int, stride: Int) -> u32 with Unsafe:
    let off = (y * stride) + x
    return mem_load(ptr_offset(fb, off, "u32"), "u32")
```

---

## 3. Directory Layout

```
tests/
├── master_test.md              ← This file
├── README.md                   ← Project overview
├── build.kn                    ← Kain build authority
│
├── src/
│   ├── main.kn                 ← Test runner entry point
│   ├── spec_parser.kn          ← TSV spec parser (Pure Kain)
│   └── reporters/
│       ├── json_rep.kn         ← JSON output formatter
│       └── html_rep.kn         ← HTML dashboard (Phase B)
│
├── specs/
│   ├── kaintana_tests.tsv      ← Master test catalog (all layers)
│   ├── layout_basic.tsv        ← Width/height/pad/gap/direction
│   ├── layout_flex.tsv         ← Flex grow/shrink/wrap
│   ├── color_parsing.tsv       ← Hex colors, named colors
│   ├── blend_modes.tsv         ← Porter-Duff compositing
│   ├── damage_smoke.tsv        ← Dirty rect correctness
│   └── stress.tsv              ← 4096 node, deep nesting
│
├── golden/
│   ├── hello_box.bin           ← Reference framebuffer dumps
│   ├── two_boxes.bin           ← Raw uint32_t ARGB, row-major
│   ├── rounded_rect.bin
│   └── ... (expanded per spec)
│
├── docs/
│   ├── kaintana_test_1.md      ← Kernel layer (C interop, TSV, golden)
│   └── kaintana_test_3.md      ← Pipeline, phases, Ghost Harness
│
├── reference/
│   ├── py_c_test.kn            ← Kain + C + Python interop reference
│   └── python_interop_god.kn   ← Full Python interop reference
│
├── kaintana_test_helpers.h     ← C helper: framebuffer ptr expose
├── kaintana_test_helpers.c     ← Implementation
└── python_abi/                 ← Phase C: Ghost Harness scripts
    ├── harness.py
    └── test_interaction.py
```

---

## 4. Spec Format: TSV

Each spec TSV file has a comment header with metadata, then tab-delimited data rows:

```
# name=layout_basic_smoke    width=800    height=600    golden=a1b2c3d4...sha256
frame   call        arg1        arg2        arg3    arg4    x       y       color
0       kt_make     "Test"      800         600     ""      -1      -1      ""
0       kt_begin    ""          16.0        ""      ""      -1      -1      ""
0       kt_row      -1          "box"       "root"  ""      -1      -1      ""
0       kt_row      0           "box"       "child" ""      -1      -1      ""
0       kt_fill     ""          "#21D4A1"   ""      ""      -1      -1      ""
0       kt_width    ""          100.0       ""      ""      -1      -1      ""
0       kt_height   ""          30.0        ""      ""      -1      -1      ""
0       kt_end_row  ""          ""          ""      ""      -1      -1      ""
0       kt_end_row  ""          ""          ""      ""      -1      -1      ""
0       kt_end      ""          ""          ""      ""      50      15      "#21D4A1"
```

Fields:
- `frame`: Frame number (0 = first)
- `call`: ABI function name
- `arg1..arg4`: Arguments (type cast at runtime)
- `x`, `y`: Expected pixel coordinate (-1 = skip pixel check)
- `color`: Expected hex color at (x,y)

The header (`# name=...`) includes the golden file SHA-256 for integrity.

---

## 5. What to Test (Categories + Examples)

### Layer 0 — Session Lifecycle
The `kt_init`/`kt_make`/`kt_free` contract. Test null safety, double-free safety, and session isolation.

| Test | What It Proves |
|------|----------------|
| `kt_init()` called once | No crash, no segfault, idempotent |
| `kt_make("test", 800, 600)` | Returns non-NULL `kt_Session*` |
| `kt_make` extreme sizes | `(1,1)` smallest, `(4096,4096)` largest |
| `kt_free(NULL)` | No-op, doesn't crash |
| `kt_free(s)` then `kt_make` | Session handle is properly recycled |
| Double `kt_free(s)` | Second call is safe (no double-free) |

### Layer 1 — Element Tree
The `kt_row`/`kt_end_row`/`kt_text` nesting contract. Test parent-child relationships, stable key resolution, and depth limit enforcement.

| Test | What It Proves |
|------|----------------|
| Single root `kt_row(s, -1, "box", "root")` | Returns `id=1`, no errors |
| `kt_row(id=1)→kt_row(parent=1)` | Child is properly linked to parent |
| 64-deep nesting | Element stack depth limit enforced |
| 65-deep nesting (overflow) | Error handler fires, stack doesn't corrupt |
| `kt_text` on element | Text content stored correctly |
| Stable key reuse | `kt_row` with same `key` returns existing node ID |
| `kt_row(s, 999, ...)` invalid parent | Graceful fallback (parent becomes root) |

### Layer 2 — Layout Attributes
The `kt_width`/`kt_height`/`kt_pad`/`kt_pad_xy`/`kt_gap`/`kt_direction` contract. Test that constraints produce correct bounding boxes (verified via draw command bounds).

| Test | What It Proves |
|------|----------------|
| `kt_width(s, e, 200.0)` | `kt_cmd_get(0).bounds.w == 200.0` |
| `kt_height(s, e, 100.0)` | `kt_cmd_get(0).bounds.h == 100.0` |
| `kt_pad(s, e, 10.0)` | Inner element shrinks by 20px (10 left + 10 right) |
| `kt_pad_xy(s, e, 10.0, 20.0)` | Horizontal pad=10, vertical pad=20 |
| `kt_gap(s, e, 8.0)` | Two children have 8px gap between them |
| `kt_direction(s, e, KT_DIR_COLUMN)` | Children stack vertically, not horizontally |
| Flex grow/shrink | Remaining space distributed proportionally |
| Percent units | `width=50%` resolves to half of parent's width |

### Layer 3 — Style Attributes
The `kt_fill`/`kt_stroke`/`kt_radius`/`kt_opacity`/`kt_font` contract and color math.

| Test | What It Proves |
|------|----------------|
| `kt_fill(s, e, "#21D4A1")` | Hex parsed correctly, cmd.color == 0xFF21D4A1 |
| `kt_fill(s, e, "accent")` | Named color resolved from theme |
| `kt_radius(s, e, 8.0)` | `cmd.radius == 8.0` |
| `kt_opacity(s, e, 0.5)` | Premultiplied alpha in framebuffer is halved |
| `kt_stroke(s, e, "#FF0000", 2.0)` | Stroke command emitted with correct color/thickness |
| Color name `"bg"` | Theme color resolved to correct uint32 |
| `kt_font(s, e, 14.0)` | Font size stored, text commands reference correct size |

### Layer 4 — State Persistence
The `kt_put`/`kt_get` family. Test round-trip fidelity across frames.

| Test | What It Proves |
|------|----------------|
| `kt_put(s, "counter", 42)` then `kt_get(s, "counter", -1)` | Returns 42 |
| `kt_get(s, "missing", 99)` | Returns fallback 99 |
| `kt_put_f(s, "pi", 3.14159)` then `kt_get_f(s, "pi", 0.0)` | Float round-trip within 1e-6 |
| `kt_put_s(s, "name", "hello")` then `kt_get_s(s, "name", "")` | String round-trip |
| State survives `kt_begin`/`kt_end` | Frame boundary doesn't clear state (it's session-scoped) |
| 128 key limit | State entry capacity enforced |

### Layer 5 — Render Output
The `kt_cmd_count`/`kt_cmd_get` contract and framebuffer pixel verification.

| Test | What It Proves |
|------|----------------|
| Empty frame | `kt_cmd_count(s) == 0` |
| One filled box | `kt_cmd_count(s) >= 1`, first cmd is `KT_CMD_FILL` |
| Pixel at (50, 15) equals "#21D4A1" | Golden comparison |
| Multi-box pixel correctness | All pixel values match golden `.bin` |
| Draw merge | Adjacent same-color fills merged into one cmd |
| Opaque fill over transparent | Premultiplied blend produces correct result |

### Layer 6 — Clip Rectangles
The `KT_CMD_CLIP`/`KT_CMD_UNCLIP` contract.

| Test | What It Proves |
|------|----------------|
| Single clip rect | Pixels outside clip are not written |
| Nested clip (CLIP→CLIP) | Inner clip is proper intersection |
| CLIP→UNCLIP → CLIP | Stack correctly restores prior clip |
| 16-deep clip stack | Maximum depth enforced |
| 17 CLIP without UNCLIP | Overflow silently ignored, no corruption |
| Degenerate clip (w=0 or h=0) | Nothing writes in that region |
| Clip rect at framebuffer edge | Correct clamping to (0, 0, width, height) |

### Layer 7 — Stress Tests
Boundary conditions and extreme inputs.

| Test | What It Proves |
|------|----------------|
| 4096 nodes (max) | Node capacity limit, no crash |
| 4097 nodes (overflow) | Error handler fires |
| 1000 nested elements | Tree traversal doesn't stack-overflow C stack |
| 32000×24000 framebuffer | Allocation rejected (too large) |
| 1000 random attr calls | Arena doesn't leak, no crash within capacity |
| Multi-frame stability: 60 frames | Renderer doesn't accumulate state across frames |

### Layer 8 — Backend Registration & Input Funnel
The backend registry contract and 7 input functions.

| Test | What It Proves |
|------|----------------|
| `kt_backend_register(s, "null", ...)` | Returns 0 on success |
| `kt_backend_select(s, "null")` | Returns 0 on success |
| `kt_backend_select(s, "bogus")` | Returns -1 (not found) |
| `kt_backend_probe(s)` | Returns bitmask of registered backends |
| `kt_input_mouse_move(s, 400, 300)` | Input state updated for next frame |
| `kt_input_mouse_down/up` | Button state transitions correctly |
| `kt_input_key_down/up` | Key state toggles in `keys_down[]` |
| `kt_input_scroll(s, 0, -120)` | Scroll delta recorded |
| `kt_input_text(s, "hello")` | Text buffer populated |
| `kt_should_close(s)` | Returns 0 (null backend never closes) |

---

## 6. The Null vs Terminal Backend

| Concern | Null Backend | Terminal Backend |
|---------|-------------|------------------|
| **File** | `backends/null/host_null.c` (256 lines) | `backends/terminal/host_terminal.c` (117 lines) |
| **Output** | `uint32_t* g_fb` in-memory buffer | ANSI truecolor to stdout |
| **Framebuffer** | Configurable `width × height` (calloc) | Fixed 80×24 cell grid |
| **Determinism** | ✅ Perfect | ❌ Terminal-dependent |
| **CI-able** | ✅ Yes (no DISPLAY) | ⚠️ Requires ANSI terminal |
| **Speed** | 10,000+ tests/s | ~100 tests/s |
| **Pixel Readback** | ✅ Direct `g_fb[y*w + x]` | ❌ Pixels rendered, not machine-readable |
| **Clip Stack** | ✅ 16-deep with intersection | ✅ 16-deep with intersection |
| **Blend Modes** | Full premultiplied ARGB | Block colors via ANSI 24-bit |
| **Use Case** | **CI assertions** (golden compare) | **Visual debug** (see layout geometry) |

**Rule of thumb:** Always develop tests against the null backend (it gives you pixel-level diffs). Use the terminal backend only during active layout iteration to visually inspect element positions.

---

## 7. Phase Plan

### Phase A — C Interop Kernel (Now → P2)
- `tests/specs/*.tsv` — 5+ spec files covering L0-L6
- `tests/src/spec_parser.kn` — TSV → `Array<AbiCall>` parser
- `tests/src/main.kn` — Test runner with `--json` output
- `tests/golden/*.bin` — Reference framebuffer dumps
- `tests/build.kn` — Build authority
- **Gate:** `kain build tests/` produces .exe, exit 0 = all pass

### Phase B — Reporting & Pixel Diff (P2 → P4)
- `tests/src/reporters/json_rep.kn` — Structured JSON with pixel diffs
- `tests/src/reporters/html_rep.kn` — Standalone HTML dashboard
- Expanded golden corpus (20+ files)
- Row-by-row SAD comparison with per-channel error tolerance
- **Gate:** `--html` flag produces viewable dashboard

### Phase C — Ghost Harness & Interaction (P4 → P6)
- `tests/python_abi/harness.py` — Python ctypes loader
- `harness.py` launches Kain app invisible, captures `PrintWindow`
- Vision LLM rubric comparison (Rosetta Stone §V.2)
- Oracle integration for OS-level telemetry
- **Gate:** Python harness verifies real OS window behavior

---

## 8. Z3 Pro Pack Integration

The 240+ formulas in `formulas.tsv` are Z3-proven UNSAT. The test pipeline empirically validates the same properties:

| Proof Pack | File | Validates |
|------------|------|-----------|
| `box_math_proofs.yaml` | `box_math.c` | Layout bounds safety, two-pass convergence |
| `damage_proofs.yaml` | `damage.c` | No orphan dirty nodes, merge threshold correct |
| `arena_proofs.yaml` | `arena.c` | Overflow safety, growth bounds |
| `hash_table_proofs.yaml` | `hash_table.c` | No false negatives, O(1) bounds |
| `draw_proofs.yaml` | `draw_pixels.c` | Cmd count bounded, DIV255 error ±0.5 |

Relationship: **Z3 proofs** catch deep structural bugs (invariant violations under any input); **test pipeline** catches regression in specific visual outcomes (pixel values matching golden).

---

## 9. Test Runner Lifecycle (from Kain)

```
kt_init()                         Initialize Kaintana system
kt_make("test", W, H)            Create session
kt_backend_register("null")      Register null backend
kt_backend_select("null")        Select null backend
parse_tsv("specs/tests.tsv")     Load test spec
for each test case:
    kt_begin(s, 16.0)            Start frame
    kt_row(...)                  Build element tree
    kt_fill(...)/kt_width(...)   Set attributes
    kt_end_row(...)              Close element
    kt_end(s)                    Finish frame → generate commands
    kt_cmd_count(s)              Assert command count
    check_framebuffer(golden)    Compare pixels to golden .bin
    kt_present(s)                No-op in null backend
    emit_json_result()           Write result to stdout
kt_free(s)                       Destroy session
emit_json_suite()                Final summary
runtime_shutdown()               Shutdown runtime
```

---

## 10. Key Architectural Decisions

| Decision | Rationale |
|----------|-----------|
| Direct `include <kaintana.h> as kt` | Zero marshalling overhead, LLVM `call` same as C call |
| TSV for test specs | 20-line Kain parser, git-diffable, no YAML/JSON dep |
| Golden `.bin` files | Raw uint32_t = byte-exact compare, no image decoder needed |
| 4-function backend contract | Every backend implements init/shutdown/new_frame/render |
| JSON to stdout | CI ingests stdout, no file path coordination |
| Arena-only allocation | 64KB bump arena per frame, no malloc during test frame |
| `kt_cmd_count()` as assertion target | Proves layout engine produced commands; catches empty-frame bugs before pixel check |
| Null backend for CI | Deterministic, 10,000+ tests/s, no DISPLAY needed |
| Terminal backend for debug | Colored blocks in terminal, zero platform deps |

---

## 11. Lessons from Reference Frameworks

### From ImGui (imgui_test.md)
- **Null backend as linchpin**: 102 lines enable 98% CI coverage. Kaintana's null backend (256 lines) exceeds this.
- **Compile-time feature flag matrix**: 25+ build config variations catch every compile regression.
- **Separate test engine repo**: `imgui_test_engine` provides assertion-based runtime tests. Kaintana's TSV approach is equivalent.
- **No screenshot diffs in public CI**: ImGui relies on runtime assertions, not pixel comparison. Kaintana adds golden pixel comparison on top.

### From Clay (clay_test.md)
- **Zero runtime assertion tests**: Clay has compilation tests but zero pixel-level or mathematical assertions.
- **Error handler as assertion sink**: Kaintana should adopt `kt_error_handler` that panics in test mode.
- **Shared layout across backends**: Clay's `shared-layouts/clay-video-demo.c` is a single layout every backend renders. Kaintana should have `kaintana_test_layout()`.
- **Terminal renderer as quick-check**: Before testing GPU/GDI backends, test the terminal backend. If terminal output looks right, layout math is correct.
- **Golden bounding-box snapshots**: Clay doesn't do this. Kaintana should — run layout, dump bounding boxes to JSON, compare against golden files.

---

## 12. The `test.tsv` Master Catalog

The companion file `test.tsv` at `tests/test.tsv` provides a quick-reference catalog of every test case across all 9 layers (0-8). Each row has: `id`, `layer`, `name`, `what`, `how`, `phase`, `done`.

This serves as the single source of truth for:
- CI pipeline gating (phase A tests must pass before merge)
- Sprint planning (which tests are still `no`)
- Coverage analysis (which layers are under-tested)
- New contributor onboarding ("start with the `no` tests")
