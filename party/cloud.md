# Cloud

## Role
- UI architecture / contract cuts / hard truth

## Current Assignment
- Audit canonical vs compatibility boundaries in the Kain UI stack.
- Keep `UiNativeProjection`, `ui_runtime_systems_from_tree(...)`, and similar fallback paths visibly compatibility-only.
- Keep my own notes current in this file as the durable party memory.

## Changes Made
- Reviewed the current UI boundary docs and the live `kain-ui` code path for bundle assembly.
- Labeled `UiNativeProjection` as compatibility-only in code comments.
- Labeled `ui_runtime_systems_from_tree(...)` as a legacy-only inference path and marked the bundle fallback as compatibility-only.
- Updated this lane note so the boundary cut stays explicit.

## Key Findings
- The UI split is mostly right in architecture docs, but the code still needs the same discipline stamped onto the fallback seams.
- `ui_runtime_systems_from_tree(...)` is not canonical truth. It is a legacy repair path for old bundles that never emitted runtime systems.
- `UiNativeProjection` is a convenience ABI for native/C consumers. It should not be treated as the semantic IR for Slate, web, or future adapters.
- The existing docs already say the right thing. The remaining work is to make the code stop sounding ambiguous.
- The real canonical surface belongs in compiler-emitted UI truth and runtime-owned retained graph state. Heuristics belong in the gutter.

## Files Touched
- `M:\Code\Kain\party\Cloud.md`
- `M:\Code\Kain\crates\kain-ui\src\lib.rs`

## Next Recommended Move
- Continue tightening adjacent seams: search for any other callers or docs that treat `UiNativeProjection` or `ui_runtime_systems_from_tree(...)` as normal architecture instead of compatibility residue.
- If a path still looks canonical but acts like a bridge, label it until it can be removed.

## Touch List
- `M:\Code\Kain\party\cloud.md`
