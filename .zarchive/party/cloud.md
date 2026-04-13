# Cloud

## Role
- UI architecture / contract cuts / hard truth

## Current Assignment
- Audit canonical vs compatibility boundaries in the Kain UI stack.
- Keep `UiNativeProjection`, `ui_runtime_systems_from_tree(...)`, and similar fallback paths visibly compatibility-only.
- Own `crates/kain-ui/src/runtime_execution.rs` and the compatibility regions in `crates/kain-ui/src/lib.rs`.
- Keep my own notes current in this file as the durable party memory.

## Changes Made
- Reviewed the current UI boundary docs and the live `kain-ui` code path for bundle assembly.
- Labeled `UiNativeProjection` as compatibility-only in code comments.
- Labeled `ui_runtime_systems_from_tree(...)` as a legacy-only inference path and marked the bundle fallback as compatibility-only.
- Updated this lane note so the boundary cut stays explicit.
- Audited the fallback call sites in `crates/kain-ui/src/lib.rs` and `crates/kain-ui/src/runtime_execution.rs` for the next cut.

## Fallback Call-Site List
- `crates/kain-ui/src/lib.rs:1366-1369` — **replace**. `ui_runtime_bundle_from_output(...)` still backfills `output.systems` from `ui_runtime_systems_from_tree(...)` when emitted systems are empty.
- `crates/kain-ui/src/lib.rs:1896` — **tighten**. Workspace-layout rebuild still seeds from `ui_runtime_systems_from_tree(tree)` and needs a compatibility-only label or a cleaner contract split.
- `crates/kain-ui/src/lib.rs:3180` — **keep-label**. Test/compat path still exercises inference and should remain explicitly legacy-only until retired.
- `crates/kain-ui/src/runtime_execution.rs:1343` — **keep-label**. Runtime execution test path still calls the fallback inference helper and should stay marked as a bridge, not doctrine.
- `crates/kain-ui/src/lib.rs:2773-3065` — **keep-label**. `ui_native_projection_from_output(...)` and its helpers are compatibility projection logic, not canonical semantic truth.

## Key Findings
- The UI split is mostly right in architecture docs, but the code still needs the same discipline stamped onto the fallback seams.
- `ui_runtime_systems_from_tree(...)` is not canonical truth. It is a legacy repair path for old bundles that never emitted runtime systems.
- `UiNativeProjection` is a convenience ABI for native/C consumers. It should not be treated as the semantic IR for Slate, web, or future adapters.
- The existing docs already say the right thing. The remaining work is to make the code stop sounding ambiguous.
- The real canonical surface belongs in compiler-emitted UI truth and runtime-owned retained graph state. Heuristics belong in the gutter.

## Key Findings
- The UI split is mostly right in architecture docs, but the code still needs the same discipline stamped onto the fallback seams.
- `ui_runtime_systems_from_tree(...)` is not canonical truth. It is a legacy repair path for old bundles that never emitted runtime systems.
- `UiNativeProjection` is a convenience ABI for native/C consumers. It should not be treated as the semantic IR for Slate, web, or future adapters.
- The existing docs already say the right thing. The remaining work is to make the code stop sounding ambiguous.
- The real canonical surface belongs in compiler-emitted UI truth and runtime-owned retained graph state. Heuristics belong in the gutter.

## Files Touched
- `M:\Code\Kain\party\cloud.md`
- `M:\Code\Kain\crates\kain-ui\src\lib.rs`
- `M:\Code\Kain\party\TASKS.md`

## Next Recommended Move
- Continue tightening adjacent seams: search for any other callers or docs that treat `UiNativeProjection` or `ui_runtime_systems_from_tree(...)` as normal architecture instead of compatibility residue.
- If a path still looks canonical but acts like a bridge, label it until it can be removed.
- Room decision posted: Cloud owns the runtime fallback call-site audit in `crates/kain-ui/src/lib.rs` while the other lanes stay locked to their current cuts.

## Status
- Cloud lane complete for the current fallback audit pass.
- Global wave completion still depends on the other lanes landing their cuts.

## Touch List
- `M:\Code\Kain\party\cloud.md`
