# Kain Ecosystem Lane Contract

This file defines the hard ownership split between Rust, C, Python, and Node inside Kain.

The goal is speed without glue-code hell.

The rule is simple:

- **Rust defines**
- **C bridges**
- **Python prepares**
- **Node presents**

If a subsystem blurs those boundaries, the repo gets slower, more fragile, and harder to reason about.

---

## Core Principle

Kain is a multi-runtime system, but it must not become a multi-truth system.

Only one lane should own the semantic truth for a subsystem.
All other lanes consume, adapt, validate, present, or accelerate that truth.

That means:

- no duplicate business logic across ecosystems
- no parallel schema definitions that drift apart
- no runtime lane inventing semantics that belong to another lane
- no "temporary" glue that quietly becomes permanent ownership

---

# Lane Ownership

## 1. Rust / Cargo — Canonical Runtime And Contract Backbone

Rust is the semantic and operational backbone.

### Rust owns

- core model
- contracts
- registries
- bundle emission
- runtime state
- validation
- deterministic materialization
- asset routing
- UI tree and UI systems
- native host adapters
- schema enforcement
- runtime execution
- build graph and manifest loading
- canonical bundle generation

### Rust is the source of truth for

- subsystem schemas
- canonical IDs
- bundle structure
- runtime contract meaning
- validation rules
- native adapter contracts

### Approved Rust libraries

#### Core data + schema
- `serde`
- `serde_json`
- `toml`
- `uuid`
- `indexmap`
- `BTreeMap` / `BTreeSet`
- `smallvec`

#### Errors + diagnostics
- `anyhow`
- `thiserror`
- `tracing`
- `tracing-subscriber`

#### Concurrency + runtime convenience
- `parking_lot`
- `once_cell`
- `rayon`

#### Filesystem / repo / CLI
- `walkdir`
- `glob`
- `notify`
- `clap`

#### UI / rendering support where appropriate
- `eframe`
- `egui`
- `wgpu`
- `image`

### Rust guardrails

- Rust must remain the **canonical schema owner**.
- Rust may emit JSON and sidecars, but those outputs must stay deterministic.
- Rust may accelerate or host UI/runtime behavior, but it must still consume canonical bundle truth.
- Rust must not silently inherit policy logic from Python or Node outputs without validation.

### Rust must not become

- a giant dumping ground for every experiment before boundaries are clear
- a place where host-local UI assumptions override canonical UI tree meaning
- a second source of truth parallel to emitted runtime bundles

---

## 2. C — Hard Boundary And Native Runtime Seam

C is the low-level blade.
It is not the brain.

### C owns

- raw-native interop
- host-side ABI seams
- runtime loaders
- minimal adapter layers
- platform-specific glue
- viewport bootstrap
- overlay / compositor bridge
- foreign runtime loading
- Win32-facing native shell bits

### C should contain

- JSON or bundle loading only when unavoidable at the platform edge
- ABI structs
- dispatch tables
- platform entrypoints
- thin runtime shims into Rust or native code

### Approved C-side helpers

- `cJSON` or `yyjson` when a C-side JSON parser is truly necessary
- `stb` libraries for lightweight image/audio work when appropriate
- platform SDK headers only where unavoidable

### C guardrails

- Keep the C layer brutally small.
- Keep data passed across the ABI stable, explicit, and versionable.
- Prefer file-backed artifacts and explicit structs over magical global state.
- Treat C as an adapter seam, not a policy engine.

### C must not own

- manifest logic
- orchestration
- business rules
- schema authority
- iterative authoring logic
- product workflow behavior

### C must not become

- the layout brain
- the workflow controller
- the place where semantics accumulate because "it was easier"

If that starts happening, cut it back immediately.

---

## 3. Python — Orchestration, Content Generation, And Fast Experimentation

Python is the fast workshop.
It is allowed to move quickly, prototype, analyze, and emit useful artifacts.
It is not allowed to become semantic runtime law.

### Python owns

- asset prep
- pipeline scripting
- report generation
- data munging
- validation helpers
- ML / tensor experiments
- DCC automation
- preview/probe generation
- batch transforms
- training / inference steps
- structured report output
- authoring automation
- inspection tools
- throwaway prototypes that may later be promoted elsewhere

### Approved Python libraries

#### Structured data / CLI
- `pydantic`
- `typer` or `click`
- `rich`
- `pyyaml`
- `toml`
- `orjson`

#### Numeric / asset processing
- `numpy`
- `pillow`
- `opencv-python`
- `trimesh`
- `scipy`

#### Watching / automation
- `watchdog`

### Python guardrails

- Python should consume canonical schemas, not redefine them from scratch.
- Python should emit explicit file-backed artifacts with stable IDs.
- Python experiments should be easy to promote into Rust contracts later.
- Python may validate and enrich runtime-side data, but must not silently redefine runtime meaning.

### Python must not own

- canonical runtime state
- long-lived production business rules
- primary schema authority
- host-side presentation semantics

### Promotion rule

If a Python script becomes durable, critical, and reused across lanes, the contract it relies on must be promoted into Rust-owned schema or runtime truth.

---

## 4. Node / TypeScript — UI Glue, Web Surface, And Integration Harness

Node is the presentation and tooling lane for web-adjacent surfaces.
It is a strong place for UI experiments, shell tooling, and integration glue.
It is not the default semantic owner of the native product shell.

### Node owns

- web UI frontends
- packaging helpers
- browser-ish runtime glue
- build scripts
- dev server orchestration
- panel / web preview tooling
- API integration
- desktop web shell support
- UI composition experiments
- inspector-side tooling
- shell-side developer utilities
- local dev dashboards

### Approved Node libraries

#### Core UI / app tooling
- `react`
- `react-dom`
- `vite`
- `typescript`
- `esbuild`

#### Validation / CLI / integration
- `zod`
- `commander` or `yargs`
- `ws`
- `execa`
- `chokidar`
- `vitest`

#### Optional utility layer
- `@tanstack/*`
- `tiny-invariant`
- `clsx`

### Node guardrails

- Node may be the best place to discover UI composition ideas.
- Node may drive shell previews and dev-facing inspectors.
- Node may not quietly become the semantic source of truth for the canonical native runtime shell unless that is an explicit architectural decision.
- Node should consume stable bundle/schema outputs, not reinterpret them ad hoc.

### Node must not own

- canonical runtime contract meaning
- core business rules
- validation truth already owned by Rust
- host adapter semantics for native runtime lanes

---

# Cross-Lane Rules

## Shared interchange format

Default interchange should be:

- JSON
- explicit schema files
- stable IDs
- deterministic ordering
- file-backed artifacts
- one manifest-owned contract per subsystem

This is the default unless a lower-level ABI or performance constraint makes another format necessary.

## Stable IDs are mandatory

Every subsystem should use stable IDs for:

- bundles
- nodes
- surfaces
- commands
- resources
- reports
- artifacts
- runtime packs
- bridge contracts

No lane should invent fresh ephemeral naming when canonical IDs already exist.

## Deterministic output is mandatory

Generated outputs must be deterministic where feasible.
That means:

- stable map ordering
- stable list ordering when order matters
- stable IDs across reruns
- explicit version fields when shape changes

If output is nondeterministic, validation and multi-lane debugging become miserable.

## One subsystem, one contract owner

Examples:

- asset routing contract -> Rust-owned
- mesh contract -> Rust-owned with C/Rust native acceleration behind explicit seams
- UI runtime bundle contract -> Rust-owned
- native bridge contract -> Rust-owned, consumed by C/native and host lanes
- report schema -> Rust-owned, emitted by runtime or automation lanes

Other ecosystems may consume and transform these contracts, but they do not own them.

---

# Forbidden Drift

The following are explicitly forbidden unless there is a deliberate architectural decision recorded elsewhere.

## 1. Duplicate business logic across ecosystems

Do not implement the same runtime or policy logic separately in Rust, Python, and Node because it felt faster in the moment.

## 2. Node becoming the semantic source of truth

Node may preview or present the shell.
It may not silently define the real runtime meaning of the shell unless the architecture explicitly changes.

## 3. Python becoming runtime ownership

Python may orchestrate, inspect, and experiment.
It may not quietly become the canonical runtime rule engine.

## 4. C accumulating orchestration

C must not absorb workflow logic, schema authority, or orchestration because the ABI seam happened to be nearby.

## 5. Schema fork drift

Do not maintain separate "equivalent" schema definitions in:

- Rust structs
- Python models
- TypeScript types
- C structs

without a single canonical owner and a clear translation path.

## 6. Artifact format drift

Do not let each tool emit slightly different JSON for the same subsystem.
That is rot.

## 7. Hidden host-local state becoming truth

If the native host needs state, it should come from:

- runtime bundles
- snapshots
- explicit bridge contracts
- validated sidecars

not magical host-local assumptions.

## 8. Fifth-toolchain boredom

Do not invent a fifth ecosystem because someone got bored.
Use the four lanes intentionally before adding another build and deployment burden.

---

# Promotion Rules

The system needs a clean path from prototype to canonical runtime.

## Promotion ladder

### Stage 1 — Experimental
Usually Python or Node.

Use when:
- exploring a new tool flow
- testing a UI concept
- trying a new transform
- building a report or analysis helper
- probing a rendering or pipeline idea quickly

Requirements:
- explicit output files
- stable enough IDs for traceability
- no hidden semantic ownership

### Stage 2 — Durable Tooling
Still Python or Node, but repeated and useful.

Use when:
- the tool is reused often
- operators depend on it
- it generates artifacts consumed by other lanes
- it now influences shipping workflows

Requirements:
- documented inputs/outputs
- validation against Rust-owned schema if applicable
- clear ownership boundary

### Stage 3 — Canonical Contract
Move contract definition into Rust.

Use when:
- the shape is stable
- multiple lanes depend on it
- runtime correctness depends on it
- drift would be dangerous

Requirements:
- Rust schema / contract ownership
- deterministic materialization
- stable IDs and versioning
- validation path in Rust/CLI/runtime

### Stage 4 — Native / Hot Path
Use C or Rust runtime acceleration where needed.

Use when:
- performance matters
- ABI boundary is required
- platform access is necessary
- viewport, sculpt, mesh, render, or host glue needs low-level code

Requirements:
- contract remains Rust-owned
- ABI surface is explicit and narrow
- data flow is file-backed or struct-backed, not magical
- native code does not seize semantic ownership

---

# Practical Lane Map For Kain Fabric

If speed matters right now, the preferred split is:

1. **Rust** for canonical Fabric engine, bundle validation, runtime contract, UI tree, and host adapter semantics
2. **Python** for generators, asset transforms, reports, and data-heavy experiments
3. **Node** for shell/dev tooling, presentation experiments, and preview surfaces
4. **C** only where native ABI or platform glue is unavoidable

That is the default blend.

---

# Decision Filter

When adding new code, ask these questions in order:

1. Is this semantic truth or just a consumer of truth?
2. Does this need to be canonical and validated?
3. Does this require native ABI or platform access?
4. Is this mainly authoring automation or analysis?
5. Is this mainly presentation or dev tooling?
6. Is this performance-sensitive enough to justify native/hot-path code?

Then choose the lane accordingly.

If the answer is unclear, default to:

- Rust for truth
- Python for experiments
- Node for presentation tooling
- C for the hard seam only

---

# Non-Negotiable Summary

- Rust defines.
- C bridges.
- Python prepares.
- Node presents.
- Schemas do not fork.
- IDs stay stable.
- Outputs stay deterministic.
- C does not become the brain.
- Python does not become runtime law.
- Node does not become canonical truth by accident.

If this file and the code disagree, fix the code or update this contract intentionally.
Do not let drift happen silently.
