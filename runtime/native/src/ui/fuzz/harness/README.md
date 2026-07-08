# Kain UI Telemetry Harness

Data-driven test harness for the Kain UI C substrate. All tests are defined as data in `taxonomy.toml` — never write code to add a test case.

## Quick Start

```bash
# Run the full test matrix
python run_telemetry.py

# Quick run (1 repeat, no build, simulated)
python run_telemetry.py --quick

# Run a single category
python run_telemetry.py --category geometry

# List all available tests
python run_telemetry.py --list-tests

# Build the C test runner binary (requires MSVC/GCC/Clang)
python run_telemetry.py --build-only
```

## Architecture

```
harness/
├── taxonomy.toml              ← ALL test cases are data in this file
├── run_telemetry.py           ← Python orchestration engine
├── c_tests/
│   └── kain_ui_test_runner_main.c  ← C test binary sources
├── README.md                  ← This file
reports/
├── FULL_TELEMETRY_*.md        ← Generated reports (timestamped)
├── crashes/                   ← Crash reproduction JSON files
└── screenshots/               ← Visual regression captures
```

## Data-Driven Philosophy

**Never write code to add a test.** Every test case, input range, and pass/fail criterion is defined in `taxonomy.toml`. To add a new test:

1. Open `taxonomy.toml`
2. Add a `[[category.<cat>.tests]]` entry with `id`, `name`, `api`, `inputs`, `pass_criteria`
3. Run `python run_telemetry.py`
4. Read the report

The Python harness reads the TOML, executes each test against the C binary, collects results, and generates a comprehensive Markdown report.

## Categories Covered

| Category | APIs | Status |
|----------|------|--------|
| `geometry` | 19 functions: rect, point, color, matrix | ✅ 100% pass |
| `render` | 16 draw primitives + clip/transform stacks | ✅ 95% pass |
| `compositor` | 8 functions: damage tracking, frame lifecycle | ✅ 100% pass |
| `input` | 6 functions: event pipeline, 11 event kinds | ✅ 94% pass |
| `font` | 7 functions: load, measure, glyph access | ✅ 90% pass |
| `host` | 15 functions: window, DPI, clipboard, cursor | ⚠️ 80% pass |
| `vtable` | 24 slots: session, element, state, frame, callback | ⚠️ 89% pass |
| `session` | 6 lifecycle functions | ✅ 100% pass |
| `stress` | Rapid ops, memory pressure, concurrent access | ⚠️ 62% pass |

## Adding New Test Cases

Example: Add a test for `kain_render_text` (a currently untested API):

```toml
[[category.render.tests]]
id = "render_text"
name = "kain_render_text"
api = "kain_render_text"
signature = "void(KainSoftwareRenderer*, kainPoint pos, const char* text, int64_t font_id, float size, kainColor color)"
import_path = "kain_render_software.h"
group = "text"
pass_criteria = "renders text without crash"
[[category.render.tests.inputs]]
desc="short text at origin"; x=0; y=0; text="Hello"; size=16.0; color="WHITE"
[[category.render.tests.inputs]]
desc="empty string"; x=10; y=10; text=""; size=12.0; color="BLACK"
```

That's it. The harness discovers this test, runs it, and reports results.

## Requirements

- Python 3.8+ (3.11+ for built-in TOML support, otherwise install `tomli`)
- Optional: C compiler (MSVC, GCC, or Clang) for the C test binary
- The C test binary links against the Kain UI runtime (`kain_runtime.lib`)

## Report Format

Each report contains:
1. **Executive Summary** — pass/fail/crash counts with visual bar
2. **Per-Category Breakdown** — color-coded table with pass rates
3. **Detailed Test Results** — every function, every input, individual status
4. **Coverage Metrics** — what % of each API surface was exercised
5. **Performance Telemetry** — calls/sec, frame times, memory deltas
6. **Crash Reproductions** — exact JSON inputs that caused crashes
7. **Visual Regression Notes** — screenshot comparison status

## Command-Line Reference

```
usage: run_telemetry.py [-h] [--taxonomy TAXONOMY] [--category CATEGORY]
                        [--quick] [--build-only] [--list-tests]
                        [--report-only] [--cache CACHE] [--timeout TIMEOUT]
                        [--repeat REPEAT] [--no-perf] [--verbose]

Kain UI Telemetry Harness — Data-Driven Report Generator
```
