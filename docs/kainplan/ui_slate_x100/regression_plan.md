# UI Slate X100 Regression Plan

- Owner: Aegis
- Purpose: Define the regression program that proves the overhaul as a platform system across authoring, bundle output, runtime execution, backend realization, and packaged product behavior.
- Rule: A smoke is evidence only when it participates in a repeatable regression lane with explicit expected outputs.

## Validation Layers

| Layer | What Must Be Proven | Typical Artifacts |
| --- | --- | --- |
| Authoring | `.kn` contracts express state, commands, focus, selection, paint, motion, schema widgets, and chrome ownership without backend-aware hacks | Source fixture, compile output, schema fixture |
| Bundle | Compiler output preserves authored semantics in stable, inspectable structures | Emitted bundle snapshots, contract diffs |
| Runtime | Retained graph, runtime state, transactions, and patch streams reflect the authored change | Patch traces, runtime snapshots, invalidation logs |
| Backend | Realization consumes shared semantics and applies explicit capability/fallback policy | Capability report, backend trace, unsupported-state capture |
| Packaged app | Product mode owns the screen and the app behaves like authored software, not a debug shell | Native `.exe`, launch capture, screenshot set, operator notes |
| Performance | Richer semantics remain responsive and bounded | Timing captures, patch counts, idle-state traces |

## Canonical Proof Surfaces

Existing repo-local UI smokes already cover useful slices and should be preserved or upgraded instead of bypassed:

| Proof Surface | Current Strength | Required Role In Regression |
| --- | --- | --- |
| `smoketest/UI/theme_authoring_shell` | Theme blocks, widget variants, text roles | Theme/token and authored-shell ownership proof |
| `smoketest/UI/dock_layout_workbench` | Dock composition and resize layout | Docking, split, persistence, and transaction proof |
| `smoketest/UI/surface_modes_gallery` | Widget surface-mode breadth | Widget-family and fallback proof |
| `smoketest/UI/spv_ui_surface_probe` | Shader-backed UI surfaces | Paint/surface and backend-capability proof |
| `smoketest/UI/gpu_compute_surface_probe` | Compute plus UI packaging | Viewport/surface integration and packaged-sidecar proof |
| `smoketest/UI/kinetic_ui_atlas` | Semantic tabs and varied shell composition | Tab persistence and multi-shell layout proof |
| `smoketest/UI/website_clone_signalcraft` | Editorial top-nav shell | Product-shell and visual-distinctiveness proof |

## Required New Or Refreshed Showcase Classes

The accepted baseline must include three showcase-grade packaged apps:

1. Editorial shell
2. Dense operator shell
3. Workbench/property-grid shell

Each showcase must prove:

- authored product chrome
- no default debug contamination
- distinct typography and surface language
- at least one hard interaction path beyond static layout

## Regression Suites

### 1. Authoring Contract Suite

Validate that authored source can describe the target semantics without backend-local escape hatches.

- Compile fixtures for state, derived values, commands, focus scopes, selection scopes, transactions, paint, motion, schema widgets, and viewport overlays.
- Fail if the fixture needs backend-specific props for core behavior.
- Fail if the compiler lowers critical meaning into opaque string bags that the backend must reinterpret.

### 2. Bundle Contract Snapshot Suite

Validate emitted truth directly.

- Snapshot emitted UI/runtime bundles for each semantic family.
- Diff on stable fields only; ignore disposable paths and timestamps.
- Fail if a semantic addition exists only in backend code and not in the bundle family.
- Fail if a fallback path is required by the capability table but absent from the emitted truth.

### 3. Runtime Graph And Patch Trace Suite

Validate retained-tree and patch-stream behavior.

- Record patch traces for tab switch, dock move, menu open, command execution, property edit, graph selection, timeline scrub, and overlay toggle.
- Record invalidation scope for each interaction.
- Fail if a local interaction emits a full-root patch without a matching root-level authored cause.
- Fail if node identity churns unnecessarily during tab switches, docking, or schema-driven edits.
- Fail if idle state continues to emit patches after the UI has settled.

### 4. Backend Capability And Fallback Suite

Validate explicit capability handling.

- Emit backend capability reports for `Native`, `Web`, `Slate`, and `Debug`.
- Exercise at least one unsupported or downgraded feature per non-shipping backend and verify an explicit fallback or unsupported-state signal.
- Fail if a backend silently drops authored chrome, motion, or tooling semantics.
- Fail if `Debug` is required to access a product interaction that should exist in `Native`.

### 5. Native Packaging And Product-Mode Suite

Validate the packaged-app posture.

- Build the packaged native outputs for the accepted showcase set.
- Capture startup screenshots and a short interaction path for each.
- Fail if the app opens with runtime inspector, host labels, or debug shell furniture visible by default.
- Fail if authored chrome is visually subordinate to host scaffolding.
- Fail if packaged output requires hand-edited generated files or smoke-local cleanup to look correct.

### 6. Distinctiveness Review Suite

Validate that the overhaul changed the platform look, not only the content.

- Store a canonical screenshot set for editorial, operator, and workbench shells.
- Review typography, color systems, chrome density, navigation pattern, and surface treatment across the set.
- Fail if the three captures still read as the same host shell with reskinned text.
- Fail if distinctiveness depends on custom renderer paths that other authored apps cannot reuse.

### 7. Performance And Responsiveness Suite

Validate that richer semantics remain tool-grade.

- Record median and p95 timings for representative interactions on the accepted workstation baseline.
- Required interactions: tab switch, command-palette open/filter/execute, dock drag, property edit, menu open, overlay toggle, viewport interaction with surrounding shell active.
- Hard failures:
- idle patch churn after steady state
- more than one-frame visible lag for simple shell interactions on the baseline machine
- continuous drag or docking behavior that starves input or drops stable semantic state
- viewport activity that collapses adjacent shell responsiveness
- Runtime work must remain bounded. A local edit should touch the affected region, not re-drive the full tree by default.

### 8. Negative And Contamination Suite

Validate that legacy failures stay dead.

- Start each packaged showcase in default mode and assert no debug chrome is visible.
- Disable selected backend capabilities intentionally and verify fallback behavior remains explicit.
- Attempt to reproduce known contamination patterns: host badges, root labels, debug-first status panels, smoke-local post-processing, and hidden widget-state ownership.
- Fail on any reintroduction.

## Baseline Governance

- Every accepted suite needs a named fixture or showcase owner.
- Baselines must store the expected artifact set: bundle snapshot, patch trace, capability report, package capture, and timing summary.
- A change is accepted only if it updates the baseline deliberately. Silent drift is failure, not “close enough.”

## Merge Gate

No lane should declare the overhaul complete until the regression set proves:

1. semantic depth
2. backend contract integrity
3. packaged product-mode ownership
4. visual distinctiveness
5. bounded interactive performance

That gate is the only valid exit from “better demo” to “better platform.”
