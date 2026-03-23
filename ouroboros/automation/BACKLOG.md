# Selfhost Backlog

This is the starting backlog for the hourly loop. Agents should pick the highest-value item that fits the active lane.

## Tier 1

- Collapse the current `E0428` duplicate-type blocker family in repaired `kain-core` (`Span`/`Spanned` and typed-program/type-env clusters) with repair-rule coverage proven by `python tools/selfhost_pipeline/run_pipeline.py run --lane phase2-core`.
- Harden `crates/kain-import/src/rust/selfhost.rs` so strict self-host import rejects less noise and classifies diagnostics more cleanly.
- Align `M:/Code/OuroborosV2/docs/selfhost/inventories/module_map.json` with the real crate/module graph in `M:/Code/Kain`.
- Reduce the hottest stage-2 failures for `kain-core`, `kain-import`, `kain-sys-codegen`, and `cli` without widening bootstrap exceptions casually.
- Make the `kain selfhost phase1` and `kain selfhost phase2` reports more actionable for automated triage.
- Build a battle-tested importer regression corpus for self-host-targeted Rust constructs.

## Tier 2

- Add stronger importer validation around module discovery, nested module maps, test filtering, and external mod handling.
- Normalize repair taxonomy and rule promotion so recurring failures move out of ad hoc fixes.
- Tighten stage-2 workspace assembly checks so manifest/path drift fails fast.
- Expand parser-safe KAIN emission coverage for known fragile forms documented in `docs/selfhost/parser-safe-variant-forms.md`.
- Make `kain-selfhost` the typed source of truth for lane, artifact, report, rule, and taxonomy contracts.

## Tier 3

- Add executable parity checks for selected self-hosted `kain` commands after stage-2 builds pass.
- Expand the self-host slice beyond `kain-core`, `kain-import`, `kain-sys-codegen`, and `cli`.
- Build a stronger native-KAIN workstream that can proceed in parallel without destabilizing the active bootstrap lane.
