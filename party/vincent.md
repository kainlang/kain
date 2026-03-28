# Vincent

## Current Assignment
Quarantine compatibility debt in the Kain UI stack. Own the exact bridge surfaces, their risk level, and the replacement target.

## Changes Made
- Reviewed UI architecture and compatibility boundaries in:
  - `M:\Code\Kain\docs\kainplan\ui_slate_x100\target_architecture.md`
  - `M:\Code\Kain\MEMORY.md`
- Tightened the legacy fallback comment in `crates\kain-ui\src\lib.rs` so `ui_runtime_bundle_from_output(...)` now labels `ui_runtime_systems_from_tree(...)` as compatibility-only.
- Aligned this role away from Cloud-style call-site auditing; this lane is inventory only.

## Key Findings
- `ui_runtime_systems_from_tree(...)` is the main legacy synthesis path.
- `UiNativeProjection` is compatibility-only and still treated as a stable convenience ABI for legacy native/C consumers.
- The runtime-authority side is already strong; the real risk is compatibility debt becoming the default architecture.

## Bridge Surface Inventory
- `crates\kain-ui\src\lib.rs:1367` — compatibility fallback in `ui_runtime_bundle_from_output(...)`
  - Risk: medium
  - Replacement target: compiler-emitted runtime systems on `UiBuildOutput`
- `crates\kain-ui\src\lib.rs:1411` — `ui_runtime_systems_from_tree(...)`
  - Risk: high
  - Replacement target: emitted runtime systems only; keep as legacy-only backfill
- `crates\kain-ui\src\lib.rs:2769` — `ui_native_projection_from_output(...)`
  - Risk: medium
  - Replacement target: narrow compatibility sidecar, never semantic IR
- `crates\kain-ui\src\lib.rs:1892` / `crates\kain-ui\src\runtime_execution.rs:1343` — runtime layout rebuilding from inferred tree shape
  - Risk: medium
  - Replacement target: explicit workspace/layout contract data
- `crates\kain-ui\src\lib.rs:3176` — tests that still rely on legacy synthesis behavior
  - Risk: low
  - Replacement target: emitted-runtime-system fixtures when available

## Files Touched
- `M:\Code\Kain\crates\kain-ui\src\lib.rs`
- `M:\Code\Kain\party\vincent.md`
- Inspected:
  - `M:\Code\Kain\crates\kain-ui\src\runtime_execution.rs`
  - `M:\Code\Kain\crates\kain-core\src\ui.rs`
  - `M:\Code\Kain\crates\kain-core\src\realtime_app_bundle.rs`

## Next Recommended Move
- Hold the compatibility inventory line.
- If the next pass opens a safe cut, target the `ui_runtime_bundle_from_output(...)` fallback and the runtime-layout rebuild path before touching adapter behavior.
