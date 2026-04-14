# DCC Parity Matrix

Snapshot: April 14, 2026.

This page is the operator-facing companion to the machine-readable parity
inventory at
`apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json`.

It defines how the repo tracks flagship parity against the sculpt and painter
reference suites without pretending that a vague status label is enough.

## Canonical Inputs

- `apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json`
- `apps/kain-fabric-dcc-suite/config/runtime_lanes.json`
- `.specs/ksculpt-kpainter-parity/*`
- `.reference/sculpting/*`
- `.reference/graphos/*`
- `apps/kain-canvas-forge/*` for the strongest current in-repo painter proofs

## Baselines

### KSculpt

KSculpt parity is defined from `.reference/sculpting/*`.
That suite is the acceptance oracle for:

- sculpt brush workflow
- projected cursor and symmetry posture
- topology and remesh behavior
- sculpt layers
- clay or PBR preview posture
- transform and export flow

### KPainter

KPainter parity is intentionally composite in this checkout.

- `.reference/graphos/*` defines the legacy painter feature surface.
- `apps/kain-canvas-forge/*` is the strongest current Kain-owned painter proof.
- `apps/kain-fabric-dcc-suite/*` is the flagship native destination where the
  final parity work should land.

## Status Meanings

| Status | Meaning |
| --- | --- |
| `reference_only` | The capability is proven in the legacy reference, but Kain does not yet own a meaningful implementation seam. |
| `scaffolded` | Kain has a real owning surface, registry, or placeholder seam, but the behavior is still materially below parity. |
| `in_progress` | The feature has active Kain-owned implementation work and should be treated as a moving target. |
| `implemented` | The feature exists in Kain-owned code with a durable owner, but full parity acceptance is not yet claimed. |
| `validated` | The feature exists in Kain-owned code and has an explicit validation hook that proves the claim. |

## Current Posture

The matrix should be read as a live inventory, not as marketing prose.
The important current shape is:

- Shared foundation is ahead of feature depth. `kain native-ui dev` and the
  desktop packaging loop are now real, but the shell and session lanes still
  need more artist-facing acceptance coverage.
- Sculpt parity has meaningful mesh, topology, and export seams in the flagship
  app, but projected cursor, symmetry, layers, and full topology depth are
  still behind KSculpt.
- Painter parity is strongest in the Node-first `kain-canvas-forge` proof and
  in the material lane scaffolds, not yet in the flagship native painter lane.
- The matrix itself is part of the implementation. A parity claim is weak until
  it has a feature id, reference source, owning surfaces, runtime lanes, and a
  validation hook.

## Validation

Run the validator from the repo root:

```bash
python3 scripts/python/validate_dcc_parity_matrix.py
python3 scripts/python/test_validate_dcc_parity_matrix.py
```

The validator checks:

- required fields and status enums
- unique feature ids
- baseline-family path existence
- reference-source path existence
- Kain-surface path existence
- owner path existence
- validation-hook shape and uniqueness
- `app_manifest.json` wiring for the parity matrix

## Update Rule

When a parity-facing feature changes:

1. update the machine-readable matrix entry first,
2. keep the owning Kain surfaces honest,
3. add or tighten a validation hook,
4. then update any summary prose.

If the matrix and the implementation disagree, trust the implementation and fix
the matrix immediately.
