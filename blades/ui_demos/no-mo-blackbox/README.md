# 🔍 no-mo-blackbox — Kain UI Component Forensics Kit

> **"Tear the lid off the black box."**

A comprehensive debugging and introspection suite for Kain component `.exe` files. When your component segfaults, hangs, or shows a blank window with zero visibility into what's happening inside the runtime — this kit gives you answers.

## 🎯 What Problem This Solves

Kain component executables are black boxes. They compile from `.kn` → LLVM → native `.exe`, but when something goes wrong at runtime, you get... nothing. No stack trace. No crash location. No visibility into which vtable calls were made. Just a segfault, a frozen window, or a blank screen.

This kit provides **6 tools** that together give you complete runtime visibility:

| Tool | What It Does |
|------|-------------|
| **Vtable Call Tracer** | Logs every `KainComponentSurface` vtable call (slots 0-23) with args, return values, and timing |
| **Crash Forensics** | Catches segfaults, maps fault address → Kain source file:line via `__kain_crash_table` and PDB symbols |
| **Hang Detector** | Detects frozen processes, samples call stacks to identify the blocking function |
| **Blank Window Analyzer** | Captures window pixels, determines BLANK/PARTIAL/FULL render status, identifies exact blank color |
| **Taxonomy** | Data-driven test case registry — defines what to test, expected behavior, failure signatures |
| **Master Runner** | Runs ALL analyzers and generates a unified forensics report |

## 🚀 Quick Start

```powershell
# Install dependencies
pip install pywin32 Pillow

# Run full forensics on a component
python run_forensics.py my_component.kn

# Run on an already-built .exe
python run_forensics.py my_component.exe --skip-build

# Run a test suite
python run_forensics.py --suite smoke
python run_forensics.py --suite full

# Individual tools
python vtable_tracer.py my_component.exe
python crash_forensics.py my_component.exe
python hang_detector.py my_component.exe --timeout 5
python blank_analyzer.py my_component.exe
```

## 📁 Output

All reports go to `forensics_output/` (set `NO_MO_BLACKBOX_OUTPUT` env var to override).

Each run produces:
- `FORENSICS_REPORT_<name>_<timestamp>.md` — unified report
- `FORENSICS_REPORT_<name>_<timestamp>.json` — machine-readable
- `vtable_trace_<name>_<timestamp>.json` + `.md`
- `crash_report_<name>_<timestamp>.json` + `.md`
- `hang_report_<name>_<timestamp>.json` + `.md`
- `blank_analysis_<name>_<timestamp>.json` + `.md`
- `blank_captures/blank_cap_<name>_<timestamp>.png` — screenshot

### Example Unified Report

````markdown
# 🔍 Kain Component Forensics Report

**Target:** `basic_component.kn`
**Verdict:** ✅ **ALL_CLEAN**

## Pipeline Stages
| Stage | Time |
|-------|------|
| build | 2.3s |
| vtable_trace | 8.1s |
| crash_forensics | 1.2s |
| hang_detector | 4.0s |
| blank_analyzer | 3.5s |
| **Total** | **19.1s** |

## 🔌 Vtable Call Trace
- **Total calls:** 47
- **Slots hit:** 9/24
- **Missing:** Slot 18 (get_gpu_extension), Slot 23 (element_set_callback)

## 💥 Crash Analysis
- **Result:** NO_CRASH — process exited normally

## ⏸️ Hang Detection
- **Hung:** NO — completed within 15s timeout

## 🖼️ Visual Analysis
- **Verdict:** **FULL** — proper render detected
- **Dominant color:** 0x1A1A2E (Kain dark theme bg)
````

## 🧰 Tool Reference

### 1. Vtable Call Tracer (`vtable_tracer.py`)

**What it traces:** All 24 slots of the `KainComponentSurface` vtable (defined in `runtime/native/include/component_surface.h`).

**Strategies:**
1. **Subprocess + IAT scan** (default) — launches exe, scans import table for known Kain runtime exports
2. **Debug Process Hook** — launches as debug child, patches vtable pointers at runtime
3. **DLL Proxy** — injects a proxy DLL that wraps every vtable slot

**Output includes:**
- Per-slot call counts (which slots fired, which didn't)
- Missing slot analysis (e.g., "Slot 18 was never called — GPU extension not used")
- Timing data per call
- Call sequence (did `begin_frame` come before `element_begin`?)

```powershell
python vtable_tracer.py component.exe --strategy subprocess
python vtable_tracer.py component.exe --json-only
```

### 2. Crash Forensics (`crash_forensics.py`)

**What it detects:**
- ACCESS_VIOLATION (0xC0000005)
- STACK_OVERFLOW (0xC00000FD)
- ILLEGAL_INSTRUCTION (0xC000001D)
- DLL_NOT_FOUND, DLL_INIT_FAILED
- Integer/float exceptions

**How it resolves source locations:**
1. Reads `__kain_crash_table` from the `.exe` (compiler-emitted symbol → source mapping)
2. Falls back to `dbghelp.dll` for PDB/DWARF symbol resolution
3. Scans the binary for embedded `.kn` source file paths

```powershell
python crash_forensics.py component.exe
python crash_forensics.py component.exe --stack-overflow-check
```

### 3. Hang Detector (`hang_detector.py`)

**How it works:**
1. Launches with configurable timeout (default: 15s)
2. If process survives timeout, samples all thread call stacks via `StackWalk64` (dbghelp.dll)
3. Matches sampled symbols against known hang patterns

**Hang patterns detected:**
| Pattern | Indicates |
|---------|----------|
| `surface_loop` | Deadlock in frame loop (begin_frame/end_frame/present cycle stuck) |
| `actor_mailbox` | Actor waiting on undelivered message |
| `gpu_sync` | GPU fence/queue blocking indefinitely |
| `spin_loop` | Unbounded while/for loop in user code |
| `async_await` | Future blocked on incomplete task |

```powershell
python hang_detector.py component.exe --timeout 5
python hang_detector.py --pid 12345  # attach to running process
```

### 4. Blank Window Analyzer (`blank_analyzer.py`)

**How it works:**
1. Launches exe via ghost harness (invisible, fullscreen, alpha=1)
2. Captures raw GPU framebuffer via `PrintWindow(PW_RENDERFULLCONTENT)`
3. Performs pixel-level analysis:

| Metric | Thresholds |
|--------|-----------|
| **BLANK** | >95% of pixels same color |
| **PARTIAL** | 50-95% same color |
| **FULL** | <50% same color + edges detected |

**Known blank color signatures:**
| Color | Meaning |
|-------|---------|
| `0xFFFFFF` | White — GDI default clear, no draw calls |
| `0x000000` | Black — Vulkan/D3D12 initialized but no shaders dispatched |
| `0xCDCDCD` | Gray — uninitialized heap memory, render backend failed |
| `0xC00000` | Dark red — crash handler backdrop |
| `0x1A1A2E` | Kain dark theme bg — possible render with no content |

```powershell
python blank_analyzer.py component.exe
python blank_analyzer.py --png screenshot.png  # analyze existing capture
```

### 5. Data-Driven Taxonomy (`taxonomy.toml`)

All test cases live in this file — tools read from it, no hardcoded paths.

**Categories:**
- `SEGFAULT` — access violations
- `BLANK` — unrendered windows
- `HANG` — frozen processes
- `RENDERING` — success cases
- `ACCESS_VIOLATION`, `LINK_FAILURE`, `STACK_OVERFLOW`, `GPU_CRASH`, `STATE_CORRUPTION`

**Test suites:**
- `smoke` — quick 6-test verification
- `full` — all 10 rendering tests
- `forensics` — crash/hang/blank detection validation

To add a test case:
```toml
[[test_cases]]
id = "my_component"
category = "RENDERING"
source = "X:/path/to/my_component.kn"
expected_verdict = "RENDERING"
vtable_profile = "basic_component"
```

### 6. Master Runner (`run_forensics.py`)

The single entry point that runs everything:

```powershell
# Full pipeline on one file
python run_forensics.py my_component.kn

# Skip certain analyzers
python run_forensics.py my_component.kn --skip-trace --skip-hang

# Run all .kn files found in taxonomy component roots
python run_forensics.py --all

# Generate JSON-only output (no markdown)
python run_forensics.py my_component.kn --json-only
```

## 🔌 Import from Kain

The kit is designed to be importable from Kain for CI/CD and automated testing:

```kain
import no_mo_blackbox.run_forensics as forensics

// Run full pipeline
let report = forensics.run_full_pipeline("my_component.exe")
if report.verdict != "ALL_CLEAN":
    for err in report.all_errors:
        println("[FAIL] " + err)
```

Or from Python:

```python
from no_mo_blackbox.run_forensics import run_full_pipeline, run_suite

# Single file
report = run_full_pipeline("component.kn")
print(f"Verdict: {report.verdict}")
print(f"Vtable slots hit: {report.vtable_slots_hit}/24")

# Full suite
results = run_suite("full")
all_clean = all(r.verdict == "ALL_CLEAN" for r in results)
```

## 📋 The Vtable — What We're Tracing

The `KainComponentSurface` vtable has 24 slots (192 bytes on x64):

| Slot | Function | What It Does |
|------|----------|-------------|
| 0 | `session_create` | Creates UI session, returns session_id |
| 1 | `session_destroy` | Destroys session |
| 2 | `element_begin` | Creates/finds element in retained tree |
| 3 | `element_end` | Completes element for this frame |
| 4 | `element_set_text` | Sets text content |
| 5 | `element_set_attr_i64` | Sets integer attribute |
| 6 | `element_set_attr_f64` | Sets float attribute (padding, spacing, etc.) |
| 7 | `element_set_attr_string` | Sets string attribute (color, title, etc.) |
| 8 | `state_get_i64` | Reads integer component state |
| 9 | `state_set_i64` | Persists integer component state |
| 10 | `begin_frame` | Starts frame (receives delta_ms) |
| 11 | `end_frame` | Ends frame |
| 12 | `present` | Presents framebuffer to screen |
| 13 | `poll_event` | Polls for input events |
| 14 | `should_close` | Checks if window should close |
| 15 | `window_open` | Opens native window |
| 16 | `host_pump` | Pumps host message loop |
| 17 | `session_attach_platform` | Attaches platform window handle |
| 18 | `get_gpu_extension` | Gets GPU surface extension (Vulkan/D3D12/WebGPU) |
| 19 | `state_get_f64` | Reads float component state |
| 20 | `state_set_f64` | Persists float component state |
| 21 | `state_get_string` | Reads string component state |
| 22 | `state_set_string` | Persists string component state |
| 23 | `element_set_callback` | Binds event callback function pointer |

## 🔗 Architecture

```
┌────────────────────────────────────────────────────────┐
│                 run_forensics.py (orchestrator)         │
│  Takes .kn/.exe → Builds → Runs all 4 analyzers        │
│  → Unified FORENSICS_REPORT                            │
├────────────────────────────────────────────────────────┤
│  vtable_tracer.py    crash_forensics.py                 │
│  hang_detector.py    blank_analyzer.py                  │
├────────────────────────────────────────────────────────┤
│  taxonomy.toml (data-driven test case definitions)      │
├────────────────────────────────────────────────────────┤
│  harness.py (ghost window capture)    Win32 API         │
│  dbghelp.dll (symbol resolution)     PrintWindow        │
│  StackWalk64 (stack walking)         Debugger API       │
└────────────────────────────────────────────────────────┘
```

## 📦 Dependencies

- Python 3.9+
- `pywin32` — Win32 API access (ghost window, PrintWindow)
- `Pillow` — Pixel analysis
- `kain` CLI — for building .kn → .exe (optional if using pre-built exes)

Install:
```powershell
pip install pywin32 Pillow
```

## 🗺️ File Map

```
no-mo-blackbox/
├── README.md              ← You are here
├── __init__.py             ← Importable package
├── taxonomy.toml           ← Data-driven test case registry
├── run_forensics.py        ← Master runner (orchestrates all tools)
├── vtable_tracer.py        ← Vtable call tracer (3 strategies)
├── crash_forensics.py      ← Crash detection + source mapping
├── hang_detector.py        ← Hang detection + stack sampling
├── blank_analyzer.py       ← Ghost capture + pixel analysis
└── forensics_output/       ← All reports go here
    ├── FORENSICS_REPORT_*.md
    ├── FORENSICS_REPORT_*.json
    ├── vtable_trace_*.json
    ├── crash_report_*.json
    ├── hang_report_*.json
    ├── blank_analysis_*.json
    └── blank_captures/
```

## 🧪 Example: Debugging a Blank Window

```powershell
# 1. Run full forensics
python run_forensics.py my_broken_component.kn

# 2. Read the unified report — it shows:
#    - Vtable trace: only slots 0, 10, 12 called (no element_begin!)
#    - Blank analysis: dominant color 0xFFFFFF (GDI default clear)
#    - Verdict: BLANK

# 3. The report tells you:
#    - Missing vtable slots: element_begin (2), element_end (3),
#      element_set_text (4)
#    - Recommendation: "Verify component render block produces
#      at least one JSX element"

# 4. Fix the component, re-run:
python run_forensics.py my_fixed_component.kn

# 5. Now the report shows:
#    - Vtable: all 9 expected slots called
#    - Blank analysis: FULL render, dominant color 0x1A1A2E
#    - Verdict: ALL_CLEAN
```

## 🔧 Extending

### Add a new test case
Edit `taxonomy.toml`, add an entry under `[[test_cases]]`.

### Add a new hang pattern
Edit `hang_detector.py`, add to `HANG_PATTERNS` dict.

### Add a new blank color signature
Edit `blank_analyzer.py`, add to `KNOWN_BLANK_COLORS` dict.

### Add a new exception code
Edit `crash_forensics.py`, add to `EXCEPTION_CODES` dict.

### Use as a library
```python
from no_mo_blackbox import (
    run_full_pipeline, trace_exe, analyze_exe,
    detect_hang, analyze_blank,
)

# Individual tools
trace = trace_exe("component.exe")
crash = analyze_exe("component.exe")
hang = detect_hang("component.exe", timeout_s=5)
blank = analyze_blank("component.exe")

# Or all at once
report = run_full_pipeline("component.kn")
print(report.verdict)
```

## 🤝 Related Files

- `X:/runtime/native/include/component_surface.h` — The 24-slot vtable definition
- `X:/runtime/native/src/ui/native_ui_surface.c` — GDI backend vtable implementation
- `X:/blades/ui_demos/harness.py` — Ghost harness (blank_analyzer builds on this)
- `X:/blades/ui_demos/test_ui/` — Test component source files
- `X:/docs/COMPONENT.MD` — Component reference
- `X:/docs/UI.MD` — UI architecture guide
- `X:/runtime/native/src/ui/research/MASTER_DOC.md` — KUIF master plan

## 📜 License

Same as the Kain project.

---

*Built on the Kain UI component system. Every vtable slot, every crash code, every blank color — all data-driven. No hardcoded paths. No assumptions. Just answers.*
