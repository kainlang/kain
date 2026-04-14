# Decisions: KSculpt And KPainter Parity

**Spec Type:** full  
**Slug:** `ksculpt-kpainter-parity`  
**Created:** 2026-04-14

## Decision Log

### Decision: Use `apps/kain-fabric-dcc-suite` as the flagship parity destination
- Context: The repo already contains multiple partial app surfaces:
  `apps/kain-fabric-dcc-suite`, `apps/kain-canvas-forge`, and several `labs/*`
  proofs. A parity program needs one canonical destination.
- Options:
  1. Evolve `apps/kain-fabric-dcc-suite` into the flagship native DCC app.
  2. Build a new parity app from scratch.
  3. Keep multiple apps as equal parity destinations.
- Decision: Use `apps/kain-fabric-dcc-suite` as the flagship parity destination.
- Rationale: It already owns the broadest DCC session, registry, and native-app
  scaffolding, so parity work can converge instead of fragmenting further.
- Tradeoffs: The existing scaffold carries complexity and some unfinished lanes,
  so early implementation passes must spend time cleaning contracts rather than
  only adding features.

### Decision: Define KPainter parity from Graphos plus Kain’s existing painter scaffolds
- Context: `.reference/README.MD` describes a paint lane, but the checked-in
  reference corpus does not contain a single dedicated `paint/` folder matching
  the sculpting layout.
- Options:
  1. Delay painter parity until a new canonical legacy baseline exists.
  2. Use `.reference/graphos/*` alone as the painter baseline.
  3. Use `.reference/graphos/*` plus current Kain painter scaffolds as the
     explicit painter baseline.
- Decision: Use `.reference/graphos/*` plus the current Kain painter scaffolds
  as the explicit baseline.
- Rationale: It keeps the parity program grounded in real reference behavior
  without pretending the baseline is cleaner than it is.
- Tradeoffs: The baseline is slightly more composite, so the capability matrix
  must record the exact source for every painter feature.

### Decision: Treat TypeScript import as migration support, not as the parity strategy
- Context: The importer is useful for discovery and bootstrap, but it is still a
  lossy transformation path and should not define the long-term authoring model.
- Options:
  1. Keep pushing toward full TS transliteration as the main path.
  2. Use importer output only as a migration aid while native Kain semantics own
     the final parity product.
- Decision: Use the importer as migration support only.
- Rationale: Flagship parity needs one semantic model inside Kain, not a growing
  pile of leaked React and Three runtime assumptions.
- Tradeoffs: More native compiler and runtime work must land up front, and some
  migrations will take longer than a superficial transliteration.

### Decision: Keep sculpt and painter on one shared DCC session and workbench contract
- Context: It would be easy for sculpt and painter to grow separate app-specific
  state models because their runtime needs differ.
- Options:
  1. Split sculpt and painter into separate session and shell contracts.
  2. Force both lanes through one shared DCC session and workbench layer with
     lane-specific subcontracts.
- Decision: Use one shared DCC session and workbench contract with lane-specific
  subcontracts.
- Rationale: Shared workbench, export, history, hot reload, and asset vocabulary
  are core parity requirements and should not be re-solved twice.
- Tradeoffs: The shared schema becomes more complex and must be designed with
  strong boundaries to avoid turning into an unstructured blob.

### Decision: Land risky kernels in `labs/*` before integrating into the flagship app
- Context: Compute-heavy painter effects, topology services, and host changes
  carry outsized technical risk.
- Options:
  1. Land all risky work directly in the flagship app.
  2. Prove risky work in `labs/*`, then integrate after benchmarks and contracts
     stabilize.
- Decision: Prove risky work in `labs/*` first when the risk is primarily
  runtime, GPU, or host stability rather than app semantics.
- Rationale: This keeps the flagship app usable while still allowing aggressive
  technical experimentation.
- Tradeoffs: Some functionality exists in two places temporarily, so the
  integration and cleanup plan must be explicit.
