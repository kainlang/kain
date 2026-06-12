---
name: oracle
description: >-
  Gaslight-proof Windows UI validator for Kain applications. Use when launching,
  testing, or debugging any Kain-compiled native executable (.exe) and you need
  OS-level proof that: (1) a real GUI window spawned (not a console phantom),
  (2) the window is actually rendering content (not a black screen), (3) UI
  input (clicks, keystrokes) produced a real response, (4) the render loop is
  alive (not frozen), or (5) an LLM claims "the app works" and you need
  mathematical counter-evidence. Use after any kain build / kain_native /
  bazel build that produces a .exe. Use when a Kain app launch is suspected to
  have silently failed. Use instead of trusting visual descriptions — this tool
  queries the OS directly via Win32 P/Invoke and returns structured JSON
  telemetry that no LLM can fabricate.
---

# Oracle — Gaslight-Proof Windows UI Validator

The oracle is a compiled C# binary (`oracle.exe`) that queries the Windows OS directly through `user32.dll`, `gdi32.dll`, and `kernel32.dll` P/Invoke. It bypasses all LLM hallucination paths: no vision guesswork, no "it looks fine to me." Every result is structured JSON backed by OS-level truth.

**Tool:** `oracle` (pi extension tool at `X:\.pi\extensions\gaslight\`)

**Binary:** `X:\.pi\extensions\gaslight\oracle\oracle.exe`

## When This Skill Triggers

Any of these scenarios:

- A Kain `.exe` was just built and needs launch verification
- An LLM claims "the window spawned" — prove or disprove it
- A Kain app window exists but might be black/frozen/hung
- UI interaction (button click, text input) needs end-to-end verification
- "Why isn't my app showing a window?" — diagnostic needed
- After `kain build`, `kain_native`, or `bazel build` that produces a binary

## The Canonical Workflow

Every Kain app verification follows this sequence. Never skip steps — each one gates the next on actual OS evidence.

### Step 1: Scan — Find What Was Just Built

```
oracle scan --dir <project_dir> --limit 5
```

Returns the freshest `.exe` files by last-write time. After `kain build`, the output is in `.kain/out/.../main.exe`. The scan finds it without the LLM guessing paths.

**Success:** `"best_match"` contains the path. Use this for the next step.

### Step 2: Launch — Start the Process

```
oracle launch <exe_path> --wait 3000
```

Starts the executable and waits 3 seconds for initialization. Returns the PID.

**Success:** `"status": "PASSED"` with a PID. If `"has_exited": true` after the wait, the app crashed on startup — do not proceed.

### Step 3: Debug — Prove Window Creation (or Diagnose Failure)

```
oracle debug --pid <pid>
```

Enumerates EVERY window handle owned by the process. Tags each with a rejection reason if it's not a valid GUI window: `INVISIBLE`, `TERMINAL`, `ZERO_SIZE`.

**Interpreting the verdict:**

| Verdict | What Happened |
|---------|--------------|
| `"Created N window(s), ALL rejected"` | Process launched but produced only terminals, invisible windows, or zero-size windows. **The app is a console binary or crashed before creating a GUI.** |
| `"Found N valid app window(s)"` | Real GUI windows exist. Proceed to Step 4. |
| `"Created no windows at all"` | Process started and exited without any window. Likely a CLI tool or immediate crash. |

**Key rejection classes:**
- `PseudoConsoleWindow` with `ZERO_SIZE` → Kain compiled as console target, not GUI
- `ConsoleWindowClass` → cmd.exe host wrapper
- `CASCADIA_HOSTING_WINDOW_CLASS` → Windows Terminal host

### Step 4: Find — Locate the Valid Window

```
oracle find --pid <pid> --timeout 10000
```

Polls every 500ms for up to 10 seconds until a valid GUI window appears. Returns the handle, dimensions, title, and class name.

**If this fails:** The process is alive but has no visible GUI window. Return to Step 3 for diagnosis.

### Step 5: Matrix — Prove the Window Is Rendering

```
oracle matrix --handle 0xN --rows 30 --cols 50 --format brightness
```

Samples the window at grid points and returns a 0-9 brightness matrix.

**Reading the matrix:**
- **All zeros (all `0` / all `·`):** The window is a black rectangle. The GPU buffer is empty or the render loop never started. **FAIL.**
- **Mostly 1s with scattered 2-4s:** Dark theme UI with text — content is rendering.
- **Scattered 7-9 clusters:** Bright content areas — text, images, UI elements with high contrast.
- **Vertical bands of uniform values:** UI panels or columns.
- **Sharp horizontal transition from 2s to 1s:** Content area boundary, toolbar/content split.

The `coverage_percent` field tells you what fraction of cells are non-zero. Below 5% is suspicious. 0% is definitely broken.

**For color analysis:**
```
oracle matrix --handle 0xN --format color
```

Returns `#RRGGBB` hex values per cell — detect color themes, branding, rendering correctness.

### Step 6: Verify — Prove the UI Responds to Input

```
oracle verify --handle 0xN --do "click:<x>,<y>" --wait 500 --expect "pixels>100"
```

This is the end-to-end proof. It:
1. Captures a baseline (before)
2. Executes the action (click, type, key press)
3. Waits for the UI to respond
4. Captures the result (after)
5. Diffs pixel-by-pixel
6. Checks the expectation

**Action formats (`--do`):**
- `"click:X,Y"` — left click at client coordinates
- `"dblclick:X,Y"` — double click
- `"type:text"` — type Unicode text
- `"key:0x0D"` — press virtual key (hex code, e.g. 0x0D = Enter)
- `"moveto:X,Y"` — move mouse without clicking

**Expectation formats (`--expect`):**
- `"changed"` — any pixel difference (default)
- `"frozen"` — no pixels changed (confirm static state)
- `"pixels>N"` — at least N pixels different
- `"pixels<N"` — at most N pixels different
- `"nonzero"` — after-matrix has any non-zero cells
- `"allzero"` — after-matrix is all black
- `"coverage>P"` — after-matrix coverage percent above P

**Result fields to check:**
- `"passed": true/false` — did the expectation hold?
- `"pixels_changed"` — raw pixel diff count
- `"cells_changed"` — matrix cell diff count (useful when window resized)
- `"fraction_changed"` — fraction of total pixels that changed
- `-1` for `pixels_changed` means the window dimensions changed between captures (the window was resized during the test — use `cells_changed` instead).

### Step 7: Delta — Prove the Render Loop Is Alive

```
oracle delta --handle 0xN --interval 200
```

Captures two frames 200ms apart and diffs them. If the app renders at 60fps, every frame pair should show pixel differences.

- `"is_frozen": true` → Render loop is dead. The window is showing a static image.
- `"is_frozen": false` with `"fraction"` > 0.01 → Active rendering.

## Quick Utility Commands

| Command | Use |
|---------|-----|
| `oracle list` | See all visible GUI windows on the system |
| `oracle info --handle 0xN` | Full metadata for one window |
| `oracle capture --handle 0xN` | Save a PNG screenshot |
| `oracle click --handle 0xN --x X --y Y` | Click without verifying |
| `oracle type --handle 0xN --text "..."` | Type without verifying |
| `oracle kill --pid N` | Kill a process |

## Failure Pattern Reference

### "The window exists but the matrix is all zeros"

→ The window surface was created but nothing is rendering to it. The GPU/drawing pipeline never produced a frame, or the clear color is black and no objects were drawn.

### "PseudoConsoleWindow, ZERO_SIZE"

→ The Kain project was compiled as a console application (`target = "console"` or no `target` in `build.kn`). Rebuild with the GUI target or check the project manifest.

### "ConsoleWindowClass"

→ The .exe spawned inside cmd.exe. The LLM is looking at the terminal, not your app. The app's window was never created.

### "pixels_changed: -1" in verify

→ Window dimensions changed between before/after captures. The app resized or maximized during the test. Use `cells_changed` for the actual change metric.

### "Process exited before window appeared"

→ The .exe crashed or completed as a CLI tool. Check stderr output, missing DLLs, or runtime errors.

## Coordinate System

All click/touch coordinates are in **client space** (relative to the window's content area, not including title bar or borders). The `info` command shows both `dimensions` (full window) and `client_size` (content area). Use client_size for coordinate planning.

Matrix cells map to approximate client coordinates:
- `px = (col + 1) * (clientWidth / (cols + 1))`
- `py = (row + 1) * (clientHeight / (rows + 1))`

## When To Escalate

After the oracle proves a failure, hand off to the owning skill:
- Console target instead of GUI → `lang-projects` (build.kn configuration)
- Window spawned but black → `lang-gpu` or `runtime-gpu` (GPU/render pipeline)
- Crash on startup → `test-crash-forensics`
- Compiler producing wrong binary type → `plumber` agent or relevant `bootstrap-*` skill

## Rebuilding the Oracle

If `oracle.exe` is missing or stale:
```
X:\.pi\extensions\gaslight\build.cmd
```

Requires Visual Studio BuildTools 2022 with .NET Framework targeting pack. The Roslyn compiler at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\Roslyn\csc.exe` is used.
