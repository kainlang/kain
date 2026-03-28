# sazh.md

## Current Focus
- Reviewing the Kain UI system as a compiler-owned, React-like authoring surface for expressive UI and LLM-friendly structure.
- Keeping the distinction clear between authored semantic truth and renderer/runtime-local behavior.

## What I Observed
- The UI system already has a solid core:
  - JSX-like authoring in `.kn`
  - backend profiles for `Runtime`, `ReactDom`, `BrowserDom`, and `Slate`
  - semantic layout extraction for docks, tabs, focus, selection, and workspace persistence
  - authored signals, computed contracts, event routes, motion tracks, surfaces, and shader-canvas support
  - realtime bundle emission with explicit UI contract payloads
- The main risk is not lack of power; it’s drift:
  - too much fallback inference
  - renderer-local interpretation creeping in
  - missing or implicit contracts where explicit ones would be safer
- The architecture docs are aligned on one core rule:
  - compiler-owned bundle truth stays central
  - hosts and backends consume it instead of inventing their own meaning

## Recommended Swarm Shape
Use a hard 10-agent / 12-agent style split with one output per agent, same brief, no coordination until all responses are in.

Suggested focus areas:
- contract sentinel
- runtime truth inspector
- backend parity auditor
- workspace layout specialist
- computed/signal specialist
- event/command flow specialist
- native host renderer specialist
- React-like ergonomics specialist
- visual semantics / spatial verifiability specialist
- shader canvas specialist
- hot reload / state transfer specialist
- regression harness / proof builder

## Files Touched
- `M:\Code\Kain\README.md` — reviewed
- `M:\Code\Kain\architecture.md` — reviewed
- `M:\Code\Kain\memory.md` — reviewed
- `M:\Code\Kain\crates\kain-core\src\ui.rs` — reviewed
- `M:\Code\Kain\crates\kain-core\src\realtime_app_bundle.rs` — reviewed
- `M:\Code\Kain\party\sazh.md` — created

## Notes
- Keep the UI conversation focused on semantics, contracts, and backend parity before polishing ergonomics.
- The system is already expressive enough to be dangerous; the real job is making it trustworthy and legible.
