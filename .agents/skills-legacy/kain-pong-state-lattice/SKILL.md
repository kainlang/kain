---
name: kain-pong-state-lattice
description: Use when changing, debugging, validating, or extending `blades/pong`, or when building another small Kain demo that keeps simulation/proof logic in Kain source while driving a real window through a blade-local presenter. Covers the passive native-UI graph, the live Win32/WGL presenter bridge, the Pong blade's world/entangle/actor flow, and the blade-local proof/screenshot validation loop.
---

# Kain Pong State Lattice

Use this skill for `blades/pong` work and for similar Kain demo blades that split authored state from live presentation.

## What Lives Where

- `blades/pong/src/main.kn`
  Owns the simulation and proof surface:
  - `world PongAuthority`
  - `world PongMirror`
  - `entangle ... with single_writer`
  - `actor InputWorker`, `PhysicsWorker`, `RenderWorker`
  - `patch apply_frame(...)`
  - passive native-UI session/state/report logic
- `blades/pong/native/pong_window_bridge.c`
  Owns the actual visible Win32/WGL window, screenshot capture, and presenter report.
- `blades/pong/config/pong_demo.json`
  Owns window size, board size, swarm count, speeds, and demo toggles.
- `blades/pong/z3/proofs-experimental/`
  Holds the focused SMT checks for bounce clamp, paddle clamp, and swarm sample placement.

## Critical Rule

`ui_host_session_create(..., "software")` is passive in this checkout.

That means:

- keep the native-UI graph if you want authored state cells, node reconciliation, reports, or proof-friendly metrics
- do not assume it opens a real window
- if you need pixels, screenshots, or close events, use the blade-owned presenter in `native/pong_window_bridge.c`

## Editing Strategy

1. Change simulation/state math in `src/main.kn` first.
2. Change visual composition in `native/pong_window_bridge.c` second.
3. Keep the passive UI and the live presenter rendering the same frame state.
4. If a new invariant appears, add a small SMT file under `z3/proofs-experimental/`.

## Kain Shape To Preserve

The blade is intentionally not a giant mutable `GameState`. Keep the split between authored state and projected mirrors:

```kn
world PongAuthority:
    state ball_x: Int = 443
    state ball_y: Int = 273

world PongMirror:
    state mirrored_ball_x: Int = 443
    state mirrored_ball_y: Int = 273

entangle PongAuthority.ball_x <-> PongMirror.mirrored_ball_x with single_writer
entangle PongAuthority.ball_y <-> PongMirror.mirrored_ball_y with single_writer

patch apply_frame(authority: PongAuthority, ball_x: Int, ball_y: Int) -> Int:
    authority.ball_x = ball_x
    authority.ball_y = ball_y
    return authority.ball_x
```

When extending the blade:

- prefer new `state` fields on `PongAuthority`/`PongMirror` plus matching `entangle` lines
- update `apply_frame(...)` in one pass so authority writes stay explicit
- keep runtime-backed self-checks based on `native_entangle_registered_count()` and `native_entangle_propagation_count()` instead of trusting stale local mirror snapshots

## Validation Loop

Run the full blade:

```powershell
powershell -ExecutionPolicy Bypass -Command "& 'D:\Kain-Lang\blades\pong\run.ps1'"
```

Expect:

- `blades/pong/pong.exe`
- `blades/pong/.kain/run/pong_report.txt`
- `blades/pong/.kain/run/pong_window_report.txt`
- `blades/pong/.kain/run/pong.bmp`

Check the screenshot if the task is visual.

## Proof Loop

Focused proofs:

```text
blades/pong/z3/proofs-experimental/pong-vertical-bounce-clamp.smt2
blades/pong/z3/proofs-experimental/pong-paddle-clamp.smt2
blades/pong/z3/proofs-experimental/pong-swarm-sample-grid.smt2
```

Use `mcp__z3_local__.check_smt2(...)` on those files or reuse their contents when the math changes.

## Common Failure Modes

- Screenshot missing:
  The passive UI lane is not the problem solver here. Check `native/pong_window_bridge.c`, `build-pong-window.ps1`, and `PONG_WINDOW_SCREENSHOT_PATH`.
- `@Float` or mismatched scalar call signatures in LLVM:
  Inspect `crates/kain-sys-codegen/src/codegen_llvm/mod.rs::compile_direct_call` and the Pong-driven LLVM regressions.
- Entangle self-check false while runtime counters are healthy:
  Do not reintroduce direct mirror-equality shutdown gates. Prefer runtime evidence.
