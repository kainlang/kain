# Native KAIN Parallel Workstreams

## Purpose

This document identifies useful KAIN-native work that can be authored in parallel with the Ouroboros V2 selfhost bootstrap without destabilizing the active Rust -> KAIN -> Rust stage-2 lane.

The rule is simple:

- build things that increase long-term leverage
- avoid changing the fragile bootstrap corridor unless the work directly removes a known blocker

## What can safely move in parallel

## Workstream A: bootstrap-owned KAIN libraries

These are the best early KAIN code targets because they are compiler-adjacent and data-oriented.

### Candidates

- diagnostic formatting helpers
- span/location mapping helpers
- string/path utility helpers
- stable collection helpers built on existing stdlib forms
- selfhost report formatting helpers
- dependency policy evaluators
- pipeline manifest readers/writers

### Why this is safe

- low platform coupling
- high reuse across bootstrap tooling
- easy to validate with golden-input/output tests later

## Workstream B: selfhost metadata and planning formats

These should be authored now because they reduce hardcoded policy later.

### Candidates

- selfhost profile manifests
- crate wave manifests
- lane promotion criteria
- blocker taxonomy manifests
- artifact expectation manifests
- command verification manifests

### Why this is safe

- no runtime/compiler semantics need to change immediately
- creates a durable control plane for the pipeline

## Workstream C: native KAIN utility/runtime layer

These are foundational systems KAIN-native software will need regardless of final app or engine direction.

### Candidates

- path utilities
- process/task abstraction surface
- structured logging/event model
- typed config loader abstraction
- resource identifiers and asset key types
- message/event envelopes for actor-style systems

### Why this is safe

- useful to compiler tooling and future native apps
- data-model heavy rather than backend fragile

## Workstream D: UI/runtime model prototypes in KAIN

These should stay lightweight and declarative for now.

### Candidates

- VNode diff test corpus in KAIN
- declarative component state examples
- layout tree data model
- event routing data model
- render command list data model

### Why this is safe

- validates KAIN-native architecture without committing to a full host runtime yet
- avoids blocking on OS/windowing integration

## Workstream E: selfhost golden corpus

A curated KAIN corpus will help both the parser-safe emitter and the stage-2 round-trip lane.

### Candidates

- enum variant expression samples
- pattern matching samples
- aggregate initialization samples
- impl/trait samples
- `Self_` method/variant samples
- JSX samples
- low-level memory samples

### Why this is safe

- pure validation content
- directly useful for regression protection

## Recommended near-term KAIN code to start writing

These are the best first KAIN-native deliverables because they are valuable even before full selfhost parity.

## 1. Selfhost profile model

Suggested KAIN-facing concepts:

- `SelfhostProfile`
- `SelfhostWave`
- `SelfhostLane`
- `ArtifactExpectation`
- `PromotionGate`
- `BlockerClass`

## 2. Diagnostic and location domain types

Suggested KAIN-facing concepts:

- `DiagnosticCategory`
- `DiagnosticRecord`
- `SourceLocation`
- `SpanLocation`
- `BuildArtifactRef`

## 3. Validation matrix domain types

Suggested KAIN-facing concepts:

- `ValidationCommand`
- `ValidationLane`
- `ValidationResult`
- `CrateValidationPlan`

## 4. Native app/runtime seed types

Suggested KAIN-facing concepts:

- `AppEvent`
- `InputEvent`
- `RenderCommand`
- `WidgetNode`
- `LayoutNode`
- `AssetHandle`

## What should wait

These should not be front-loaded until the stage-2 lane is more stable:

- full OS window host
- renderer backend implementation
- production native UI renderer
- editor shell runtime
- full asset pipeline runtime
- broad engine-scale runtime rewrite

## Parallel execution model

### You continue driving

- parser-sensitive selfhost emitter fixes
- stage-2 compile blocker removal
- active slice stabilization

### Parallel track can produce

- docs
- metadata schemas
- validation specs
- golden corpus design
- KAIN-native data models and sample source

## Suggested next native-KAIN source areas

When you want to start writing KAIN immediately, the lowest-risk targets are:

- `selfhost_profile.kn`
- `diagnostics_model.kn`
- `validation_matrix.kn`
- `ui_layout_model.kn`
- `render_command_model.kn`

These should begin as pure data/domain modules first.

## Authoring rule

Keep early native-KAIN modules:

- pure or mostly pure
- schema-heavy
- serialization-friendly
- isolated from OS/backend assumptions
- useful to both compiler tooling and future native software

## Bottom line

Yes, there is parallel work worth doing now.

The highest-leverage non-destructive parallel effort is:

- formalize the selfhost control plane as data
- build the validation/golden corpus plan
- start writing KAIN-native domain modules for diagnostics, validation, UI/layout, and render-command models

That moves Ouroboros V2 forward without tangling with the hottest stage-2 bootstrap issues.
