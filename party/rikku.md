# Rikku

## Current read
The UI system is aiming at a React-like authoring surface, but the important part is not the visuals — it is compiler-owned semantic truth, runtime graph authority, and backend realization that never becomes the source of meaning.

The current shape appears to be:
- `kain-core` owns authored UI meaning and lowering
- `kain-ui` owns retained runtime graph, patching, state, and spatial verification
- backends (native/web/Slate future) realize contracts, not invent them
- `UiNativeProjection` exists as compatibility, not doctrine
- the real fight is removing inference and host-local behavior from the critical path

## Top 3 architectural risks
1. Events are still too stringy and can collapse into placeholder semantics.
2. Runtime systems can still be inferred from tree shape instead of authored explicitly.
3. Native adapter chrome / product posture can leak meaning that should live in bundles.

## Top 3 missing contracts or semantics
1. A fully typed event-route contract with handler identity, command linkage, and transaction labels.
2. First-class geometry / containment / anchor / focus-traversal truth for structural verification.
3. A richer widget registry contract that lets tools and LLMs reason about widget families, props, slots, events, and capabilities.

## Concrete phase plan
### Phase 1: semantic cleanup
- audit every place UI meaning is still implicit
- list all placeholder events, inferred systems, and prop-flattened state paths

### Phase 2: runtime authority
- make runtime systems explicit
- ensure `kain-ui` is the source of state/focus/selection/transactions/patches
- keep legacy inference only as compatibility glue

### Phase 3: spatial proof
- emit verification-grade geometry and ownership facts
- make wrong-region detection structural, not screenshot-based

### Phase 4: adapter discipline
- quarantine native convenience behavior
- keep backend-specific UI realization from becoming product meaning

### Phase 5: proof harness
- build regression cases for editing, docking, tabs, commands, overlays, and packaged app posture

## What I would personally own
- the semantic leak audit
- the compatibility-vs-canonical ABI map
- the plan merge across all 10 agents
- the final priority list that decides what gets fixed first

## One thing I would not touch yet
- `UiNativeProjection` as a live compatibility layer, except to label and isolate it more clearly. Yank it too early and we break the bridge before the new contract is fully standing.

## Touched files
- `M:\Code\Kain\party\rikku.md`
