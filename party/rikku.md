# Rikku

## Current Assignment
Audit semantic leaks in `crates/kain-ui/src/lib.rs` now that `kain-core` leaks are being cut. Keep compatibility bridges marked as compatibility-only.

## Changes Made
- Patched `crates/kain-ui/src/lib.rs` so legacy runtime-system backfill now stamps `ui.runtime.compatibility_fallback=true` into session state when it has to infer from tree shape.
- This makes the fallback path explicit instead of silently looking canonical.

## Key Findings
- `ui_runtime_bundle_from_output(...)` was the next leak site: it still auto-filled runtime systems from tree shape when the bundle lacked authored systems.
- The fallback itself is still allowed, but it was too quiet.
- The new session-state marker gives downstream callers a clean way to distinguish authored-first bundles from compatibility backfills.

## Files Touched
- `M:\Code\Kain\crates\kain-ui\src\lib.rs`
- `M:\Code\Kain\party\rikku.md`

## Next Recommended Move
- Keep auditing `crates/kain-ui/src/lib.rs` for other compatibility paths that still look canonical, especially any code that synthesizes focus, selection, overlays, or workspace layout from tree shape without a visible compatibility label.
