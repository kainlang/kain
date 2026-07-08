# Kain UI C Substrate Fuzz Suite

> **Part of KUIF Phase 1 (P1-C-015): Fuzz verification for the Phase 1 C substrate extraction.**
> Tests every `kain_*` API function with randomized, boundary, and crash-reproduction inputs.

## Files

| File | Purpose |
|------|---------|
| `fuzzer.c` | Main entry point: config parsing, domain dispatch, report writer |
| `fuzzer.h` | Shared types: `FuzzTelemetry`, `FuzzState`, RNG utilities, crash-safe macros |
| `geometry_fuzzer.c` | Fuzzes `kain_geometry.h`: rect ops, color math, matrix/point transforms |
| `render_fuzzer.c` | Fuzzes `kain_render_software.h`: 16 draw primitives, clip/transform stacks |
| `compositor_fuzzer.c` | Fuzzes `kain_compositor.h`: damage rect tracking, overflow, frame sequences |
| `input_fuzzer.c` | Fuzzes `kain_input.h`: event push/poll/hit-test with floods and extremes |
| `font_fuzzer.c` | Fuzzes `kain_font.h`: corrupt TTF loading, glyph access, text measurement |
| `surface_fuzzer.c` | Fuzzes `kain_surface.h`: create/destroy/resize/query with edge params |
| `vtable_fuzzer.c` | Fuzzes `KainComponentSurface` (24 slots): every function pointer via native_ui |
| `fuzz_taxonomy.json` | **Data-driven taxonomy**: valid ranges, boundary values, crash reproduction. Edit this to change fuzz parameters without touching C. |
| `run_fuzz.py` | Python orchestrator: builds C fuzzer, runs iterations, parses telemetry, generates Markdown reports |
| `reports/` | Generated timestamped Markdown reports with full telemetry |

## Quick Start

```bash
# One-command fuzz run (build + 10k iterations + report):
python fuzz/run_fuzz.py --quick

# Standard fuzz run (50k iterations per domain):
python fuzz/run_fuzz.py

# Stress run (500k iterations per domain):
python fuzz/run_fuzz.py --stress
```

Or use the Makefile from `src/ui/`:

```bash
make fuzz-quick    # Build + 10k iterations
make fuzz-run      # Build + 50k iterations
make fuzz-stress   # Build + 500k iterations
```

## Orchestrator CLI

```bash
# Full options:
python fuzz/run_fuzz.py --help

# Custom iterations and seed:
python fuzz/run_fuzz.py --iterations 100000 --seed 42

# Repeatable crash reproduction:
python fuzz/run_fuzz.py --seed 12345 --iterations 50000

# Different framebuffer size:
python fuzz/run_fuzz.py --fb-width 1920 --fb-height 1080

# Clean rebuild:
python fuzz/run_fuzz.py --clean

# Skip build, just run existing binary:
python fuzz/run_fuzz.py --no-build

# Re-generate report from saved output:
python fuzz/run_fuzz.py --report-only _build/fuzz_report.json
```

## Data-Driven Taxonomy

The test parameters live in `fuzz_taxonomy.json`. This is the single source of truth:

```json
{
  "domains": {
    "geometry": {
      "fuzz_weight": 15,
      "functions": ["kain_rect_contains", "kain_color_lerp", ...],
      "valid_ranges": {"coord": {"min": -100000.0, "max": 100000.0}},
      "boundary_values": [
        {"values": [0.0, 0.0, 0.0, 0.0], "desc": "Zero rect"},
        {"values": ["NAN", "NAN", ...], "desc": "NaN coordinates"}
      ]
    }
  },
  "orchestration": {
    "default_iterations": 100000,
    "quick_iterations": 10000,
    "stress_iterations": 1000000
  }
}
```

**To add a new boundary case** → edit `fuzz_taxonomy.json`. No C changes needed.
**To change iteration counts** → edit `orchestration` section. No code changes.
**To add a new function to test** → add to `functions` array + add fuzz logic in the corresponding `*_fuzzer.c`.

## What Gets Fuzzed

### 1. Geometry (`geometry_fuzzer.c`) — 15% fuzz weight
- `kain_rect_make`, `kain_rect_contains`, `kain_rect_overlaps`, `kain_rect_intersect`, `kain_rect_union`
- `kain_point_make`, `kain_point_add`, `kain_point_sub`
- `kain_size_make`
- `kain_color_rgba`, `kain_color_from_u32`, `kain_color_to_u32`, `kain_color_lerp`, `kain_color_clamp`
- `kain_matrix_identity`, `kain_matrix_translate`, `kain_matrix_scale`, `kain_matrix_rotate`
- `kain_matrix_mul`, `kain_matrix_transform_point`
- Boundary: zero rects, negative, NaN, INF, huge values, transparent/white/black ARGB

### 2. Render Primitives (`render_fuzzer.c`) — 25% fuzz weight
- All 16 draw primitives: fill/stroke rect, rounded rect, circle, blit, text, gradient, blur
- `kain_renderer_clear`, `kain_renderer_set_framebuffer`, `kain_renderer_submit`, `kain_renderer_present`
- Clip stack: push/pop with random rects, 64-deep overflows
- Transform stack: push/pop with random affine matrices, 64-deep overflows
- All 17 public API functions tolerate NULL renderer gracefully
- Boundary: zero-size fb, negative dimensions, extreme coordinates, invalid font IDs

### 3. Compositor (`compositor_fuzzer.c`) — 20% fuzz weight
- Frame cycles: begin→damage→end with random rect counts (0-200)
- 65+ rect overflow (exceeds 64-rect ceiling)
- 100 empty frames (begin→end with no damage)
- Clear damage mid-frame
- `damage_node` stub with random node IDs and NULL
- All 8 API functions tolerate NULL compositor gracefully

### 4. Input Pipeline (`input_fuzzer.c`) — 15% fuzz weight
- Push/poll cycle with all 12 event kinds and random fields
- 1025-event flood (exceeds 1024-event ring buffer)
- Hit-test with coordinates across ±1e9 range
- Event type name utility with out-of-range enum values
- All 5 API functions tolerate NULL pipeline gracefully

### 5. Font Subsystem (`font_fuzzer.c`) — 10% fuzz weight
- Corrupt TTF loading: random bytes (4K), minimal headers, empty data
- Font measurement with extreme session/font IDs and random UTF-8 text
- Glyph access with codepoints across entire Unicode range + negatives
- NULL data, negative length, length over 64MB limit
- All 7 API functions tolerate NULL/failure gracefully

### 6. Surface Abstraction (`surface_fuzzer.c`) — 10% fuzz weight
- Create/destroy/resize with sizes 0-2000 and all backend kinds (including invalid)
- Pixel access, width/height/backend query
- 100 rapid resize operations
- NULL pointer tolerance for all operations
- Kind name utility with out-of-range values

### 7. Vtable Surface (`vtable_fuzzer.c`) — 15% fuzz weight
- All 24 slots called through `KainComponentSurface` vtable
- Session lifecycle: create → nested element_begin (4 kind strings) → destroy
- State persistence: i64/f64/String get/set with boundary values
- Frame lifecycle: begin_frame with NaN/INF/negative deltas, end_frame, present
- Event polling, should_close, window_open, host_pump, session_attach_platform
- GPU extension discovery (expects NULL for software backend)
- Callback binding with NULL event names, NULL callbacks, invalid element IDs
- Double destroy of sessions

## Reports

Reports are written to `reports/fuzz_report_YYYY-MM-DD_HH-MM-SS.md`:

- **Summary table**: ops, boundary tests, NULL-tolerance, time per domain
- **Domain details**: functions tested, boundary cases applied
- **Crash reproduction**: command to reproduce any crash with same seed
- **Taxonomy coverage**: table of functions/cases/ranges per domain
- **Fuzzer config**: orchestration parameters from taxonomy JSON
- **Raw output**: last 100 lines of fuzzer stdout

## Adding a New Domain

1. Add the domain to `fuzz_taxonomy.json` with functions, valid ranges, and boundary values
2. Create `your_domain_fuzzer.c` with a `FuzzTelemetry fuzz_<domain>(FuzzState*, int iterations)` function
3. Add the `#include` and call to `main()` in `fuzzer.c`
4. Done — the orchestrator automatically discovers new fuzzer C files

## Verification Gate

Before merging, run:
```bash
python fuzz/run_fuzz.py --stress
```
This runs 500k iterations per domain and produces a timestamped report.
Zero failures + zero crashes = green gate.

## Design Principles

- **Data-driven first**: All test parameters live in `fuzz_taxonomy.json`. Change fuzz behavior without touching C.
- **Crash-safe**: NULL-pointer tolerance is a feature, not a bug. Every API should survive NULL inputs.
- **Boundary heavy**: Focus on zero, negative, NaN, INF, MAX_INT, and other edge cases.
- **Reproducible**: Fixed seed + same binary = identical output.
- **Telemetry-rich**: Every run produces a comprehensive Markdown report.
