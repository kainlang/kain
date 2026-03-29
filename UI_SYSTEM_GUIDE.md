# Kain UI System Guide

This document is the fast operator and agent summary for the current Kain UI system.

It explains:

- what the UI system is now
- what is canonical and what is compatibility-only
- how agents should use it
- what is available in the native runtime today
- what is still not finished

## Bottom Line

Kain now has a real contract-first UI system.

The normal source of truth is:

- compiler-emitted `output.tree`
- compiler-emitted `output.systems`
- runtime-owned execution inside `UiRuntime`

The system is no longer supposed to depend on:

- retained-tree guessing as the normal path
- host-local semantic inference
- `native_projection` as a normal ABI surface
- raw-native `primary_*` UI fallback fields

## Canonical Ownership

The ownership model is now:

- [crates/kain-core/src/ui.rs](/M:/Code/Kain/crates/kain-core/src/ui.rs)
  Compiler emission of UI truth: tree, computed state, event routes, workspace layout, focus, selection, overlays, and other runtime systems.
- [crates/kain-ui/src/lib.rs](/M:/Code/Kain/crates/kain-ui/src/lib.rs)
  Shared UI bundle types, validation, compatibility projection helper, and semantic bundle contract.
- [crates/kain-ui/src/runtime_execution.rs](/M:/Code/Kain/crates/kain-ui/src/runtime_execution.rs)
  Real runtime execution: reload, invalidation, derived recompute, routing, transfer, and spatial queries.
- [crates/kain-ui-native/src/lib.rs](/M:/Code/Kain/crates/kain-ui-native/src/lib.rs)
  Rust native host that consumes `UiRuntimeBundle` and runs through `UiRuntime`.
- [runtime/native/src/ui/kain_ui_compiled_bundle.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_compiled_bundle.c)
  Raw-native C loader for the canonical tree contract.

The important rule is simple:

`output.tree` and `output.systems` are the semantic contract.

Everything else is adapter behavior.

## Normal Data Flow

The intended flow is:

`Kain source -> kain-core emits UI contract -> UiRuntimeBundle -> UiRuntime executes semantics -> native/raw-native hosts render and interact`

More concretely:

1. The compiler emits a retained semantic tree in `output.tree`.
2. The compiler emits runtime systems in `output.systems`.
3. The bundle is serialized as `UiRuntimeBundle`.
4. `kain-ui` owns runtime meaning such as reload, invalidation, focus, selection, overlays, transactions, and derived values.
5. Native hosts consume that contract instead of inventing a second UI model.

## What Is Canonical

These are canonical now:

- `UiRuntimeBundle.output.tree`
- `UiRuntimeBundle.output.systems`
- `UiRuntime::reload(...)`
- runtime-owned focus, selection, overlays, workspace layout, motion policy, and signal state
- canonical raw-native loading from `output.tree.root` and `output.tree.nodes`

These are not canonical:

- `UiNativeProjection`
- host-local tree-shape rediscovery
- raw-native convenience fields like the old `primary_panel_title`, `primary_viewport_title`, and `primary_viewport_scene`

## `native_projection` Status

`native_projection` still exists only as explicit compatibility data.

Normal bundle creation:

- [crates/kain-ui/src/lib.rs](/M:/Code/Kain/crates/kain-ui/src/lib.rs)
  `ui_runtime_bundle_from_output(...)`

This canonical path keeps `native_projection` empty and omits it from serialized JSON.

Legacy compatibility path:

- [crates/kain-ui/src/lib.rs](/M:/Code/Kain/crates/kain-ui/src/lib.rs)
  `ui_runtime_bundle_from_output_with_native_projection(...)`

Use that helper only when a legacy consumer explicitly needs the flat projection sidecar.

Agent rule:

- never treat `native_projection` as the semantic UI ABI
- never add new semantics to `native_projection`
- if a host needs meaning, put it in `output.tree` or `output.systems`

## Raw-Native Status

Raw-native is available now.

It is no longer projection-first.

Current raw-native behavior:

- requires canonical `output.tree.root`
- requires canonical `output.tree.nodes`
- resolves panel, viewport, and scene information from canonical nodes
- validates overlay compatibility from canonical node kinds
- no longer relies on the removed `primary_*` UI fallback fields

Important files:

- [runtime/native/include/kain_runtime_ui.h](/M:/Code/Kain/runtime/native/include/kain_runtime_ui.h)
- [runtime/native/src/ui/kain_ui_compiled_bundle.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_compiled_bundle.c)
- [runtime/native/src/ui/kain_ui_compiled_overlay.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_compiled_overlay.c)
- [runtime/native/src/ui/kain_ui_runtime.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_runtime.c)
- [runtime/native/src/platform/win32/kain_runtime_viewport_win32.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_viewport_win32.c)
- [runtime/native/src/platform/win32/kain_runtime_sculpt_win32.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_sculpt_win32.c)

One important fix landed with this:

The raw-native parser now understands the real Rust `UiValue::String` JSON shape inside `props`, instead of only working on fake plain-string fixture data.

## Rust Native Host Status

The Rust native host is available now.

It is using the runtime object rather than a private side path.

Current Rust-native behavior:

- loads `UiRuntimeBundle`
- validates it
- reloads through `UiRuntime::reload(...)`
- frame-steps through runtime-owned state
- keeps render state synchronized with runtime state

This is the main high-level native UI lane today.

## What Agents Should Do

When an agent touches UI in Kain, default to these rules:

1. Add or extend semantics in compiler emission first.
2. Put runtime meaning in `output.systems`, not in host code.
3. Use `UiRuntime` as the semantic execution authority.
4. Treat native hosts as adapters and renderers, not semantic owners.
5. Treat raw-native C as a consumer of canonical bundle truth.
6. Only use `ui_runtime_bundle_from_output_with_native_projection(...)` for explicit legacy compatibility.

Good places to look first:

- [crates/kain-core/src/ui.rs](/M:/Code/Kain/crates/kain-core/src/ui.rs)
- [crates/kain-core/src/realtime_app_bundle.rs](/M:/Code/Kain/crates/kain-core/src/realtime_app_bundle.rs)
- [crates/kain-ui/src/lib.rs](/M:/Code/Kain/crates/kain-ui/src/lib.rs)
- [crates/kain-ui/src/runtime_execution.rs](/M:/Code/Kain/crates/kain-ui/src/runtime_execution.rs)
- [crates/kain-ui-native/src/lib.rs](/M:/Code/Kain/crates/kain-ui-native/src/lib.rs)
- [runtime/native/src/ui/kain_ui_compiled_bundle.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_compiled_bundle.c)

## What Agents Should Not Do

Do not:

- add new UI meaning only in `kain-ui-native`
- add new UI meaning only in raw-native C
- reintroduce tree-shape heuristics as the normal path
- reintroduce `primary_*` UI fallback metadata
- make `native_projection` required again
- describe the UI contract as if raw-native or native host code owns semantics

## Native Runtime Availability

Current availability:

- Rust native UI runtime: yes
- raw-native C UI runtime: yes
- hot reload/state transfer in runtime: yes
- canonical spatial queries from runtime-owned state: yes
- contract-based focus/selection/overlay/workspace data: yes
- UE5/Slate adapter consuming this exact runtime contract: not finished yet
- web adapter consuming this exact runtime contract as a first-class lane: not finished yet

So if the question is, “is this available in the native runtime?”

The answer is yes.

It is available in both:

- the Rust native host lane
- the raw-native C lane

But native runtime availability does not mean every future backend is already wired. UE5/Slate still needs an adapter layer that consumes this same contract.

## How To Use It

For normal compiler-to-native flow:

1. Compile or materialize UI through the normal Kain compiler/driver path.
2. Produce a `UiRuntimeBundle`.
3. Let the native host load the bundle and run through `UiRuntime`.
4. If you are in raw-native, load the same bundle through the C loader and consume canonical tree data.

For agent validation:

- `cargo test -p kain-ui --lib --tests`
- `cargo test -p kain-ui-native --lib`
- `bash run_tests.sh --verbose`
  from [runtime/conformance/ui_runtime](/M:/Code/Kain/runtime/conformance/ui_runtime)

## Verified State

At the time of writing, these validations passed:

- `cargo test -p kain-ui --lib --tests`
- `cargo test -p kain-ui-native --lib`
- `cargo test -p kain-ui-native tests::runtime_bundle_prefers_canonical_output_tree_in_shared_fixture -- --exact`
- `bash run_tests.sh --verbose` in [runtime/conformance/ui_runtime](/M:/Code/Kain/runtime/conformance/ui_runtime)

That means:

- shared Rust UI runtime contract is green
- Rust native host lane is green
- raw-native UI conformance is green

## What Is Still Not Finished

These are the honest remaining gaps:

- `UiNativeProjection` still exists as an explicit compatibility helper
- some older generated artifacts in the repo may still contain old compatibility sidecar data
- UE5/Slate is not yet a finished adapter over this contract
- docs under older planning folders may still describe transitional assumptions

Those are not the same thing as “the UI runtime is fake.”

The runtime and native contract path are real now.

## Short Mental Model

If an agent remembers only one thing, remember this:

Kain UI is now a compiler-emitted semantic contract executed by `UiRuntime`, with native and raw-native hosts consuming the same canonical tree-and-systems truth.
