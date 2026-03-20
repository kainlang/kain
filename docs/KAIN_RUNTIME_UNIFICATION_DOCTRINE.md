# KAIN Runtime Unification Doctrine

> Date: March 19, 2026  
> Purpose: Keep Kain on a single architectural spine while allowing multiple execution lanes to grow in parallel.

## Why This Exists

Kain only becomes the "god language" if it stays one language with one semantic center and many execution lanes.

The biggest failure mode is fragmentation:

- one model for scripting
- one model for native UI
- one model for web
- one model for shaders
- one model for tools
- one model for runtime services

If those become separate truths, Kain stops being a platform and turns into several adjacent products.

This doctrine exists to prevent that.

## Non-Negotiable Rules

1. One semantic truth

`crates/kain-core` and the semantic runtime crates define what Kain code means.

2. One bundle truth

`crates/kain-driver` owns the compiler-emitted runtime-facing artifacts. Runtimes consume bundles. They do not become normal source-reparsing engines.

3. One capability truth

Runtime services, ABI surfaces, graphics capabilities, UI capabilities, and host contracts must be declared as structured data with stable names and versions.

4. One runtime story

There may be multiple host implementations, but there may not be multiple incompatible runtime philosophies.

5. Many execution lanes

Native C, native Rust, web, UE5, selfhost, and future Zig lanes are all valid, but they must consume the same semantic and bundle contracts.

## Canonical Ownership

| Layer | Canonical owner | Secondary implementations | Rule |
|---|---|---|---|
| Language semantics, AST, typing, effects | `crates/kain-core` | none | Backend code must not redefine language meaning. |
| Semantic UI model, retained graph, patches | `crates/kain-ui` | native/web/editor adapters | UI meaning must not fork by renderer. |
| Shader authoring semantics and reflection shape | `crates/kain-core`, `crates/kain-driver` | backend codegen crates | Shader source is authored in Kain, not in backend-specific truth models. |
| Bundle emission and app materialization | `crates/kain-driver` | packagers and adapters | Hosts consume the same bundle families. |
| Runtime ABI floor and service contract | `runtime/native_runtime.toml`, `runtime/native/include/*` | Rust/Zig pairing lanes | Service names, signatures, and capability keys live here. |
| Low-level substrate: startup, memory, platform, input, windowing, host hooks | `runtime/native` (C) | per-platform modules | C remains the canonical ABI floor. |
| Accelerated native graphics, UI, and tooling hosts | `crates/kain-ui-native`, `crates/kain-3D`, `runtime/parallel/rust` | future native crates | May optimize execution, but may not invent separate semantics. |
| Web execution lane | `crates/web`, future `kain-ui-web` | browser adapters | Web is an adapter target, not the semantic center. |
| UE5 execution lane | `crates/ue5*` | engine/editor adapters | UE-specific lowering must still consume Kain-owned contracts. |
| Selfhost execution lane | `crates/kain-selfhost` and future Kain-native runtime pieces | paired hosts | Self-hosting changes who builds the compiler, not the runtime doctrine. |

## What The C Runtime Should Own

The C runtime is not a mistake. It is useful and should stay.

It should continue to own:

- the stable ABI floor
- startup and process bootstrap
- low-level memory helpers
- core platform services
- input, window, host, filesystem, timing, and process primitives
- runtime service registration and versioning
- minimal graphics and surface bootstrap
- cross-toolchain native linking friendliness

This is the part of the platform that benefits from being small, explicit, portable, and ABI-stable.

## What The C Runtime Must Not Own

The C runtime should not become a second semantic universe.

It must not become the long-term owner of:

- a separate UI authoring model
- a separate scene graph truth
- a separate shader metadata model
- a separate tool runtime model
- runtime-only app schemas that drift from `kain-driver`
- backend-specific interpretations of Kain semantics

If a feature changes what authored Kain means, it starts in `kain-core` and the emitted bundles, not in an ad hoc runtime implementation.

## Execution Lane Model

### C substrate lane

This is the canonical low-level execution floor. It is responsible for ABI stability, service boundaries, bootstrap, and platform primitives.

### Rust paired lane

This is the accelerated native lane for richer graphics, UI, tooling, and host-side execution modules. It should pair with the C service and ABI model instead of replacing it with a different truth system.

### Web lane

This lane adapts the same semantic and bundle model to browser constraints. It should not become the architecture driver for native Kain.

### Engine/editor lane

UE5 and future engine integrations are adapters and consumers. They are not allowed to redefine Kain's source-of-truth schemas.

### Selfhost lane

As Kain becomes more self-hosting, Kain code may progressively own more compiler and runtime implementation. The doctrine stays the same: one semantic center, one bundle center, many execution lanes.

## Anti-Fragmentation Guardrails

The following are architectural violations:

- adding a new lane-specific widget taxonomy instead of extending the semantic UI model
- inventing runtime-only graphics metadata not emitted by `kain-driver`
- requiring hosts to re-parse source as the normal execution path
- keeping one shader bundle shape for native and another unrelated one for web
- letting platform adapters infer capabilities implicitly instead of reading declared capability data
- allowing OpenGL-era viewport code to remain the default after a modern GPU path exists
- building separate devtools and patch semantics per host instead of one introspection story

## Immediate Five Code Moves

These are the first five moves that reduce drift the fastest.

1. Freeze the OpenGL lane as compatibility-only.

Do not delete it yet, but stop treating it as the future default. Mark it as legacy in runtime selection, docs, and build flows once the replacement lane is ready enough to carry the main path.

2. Add a canonical graphics service contract.

Expose graphics surfaces, device capabilities, shader bundle requirements, and presentation requirements as runtime contract data instead of ad hoc renderer assumptions.

3. Make the compiler-owned UI bundle real.

Formalize one `UiRuntimeBundle` and one patch/update contract consumed by both `kain-ui-native` and raw-native UI consumers.

4. Make shader discovery automatic.

`shader` blocks authored in `.kn` files should be harvested during the normal build pass and attached to emitted runtime bundles without manual `kain.toml` bookkeeping.

5. Add cross-lane bundle conformance.

The same sample app bundle should validate across:

- C native lane
- Rust native lane
- web adapter lane

If the same authored app cannot travel across those lanes, drift has already started.

## Decision On The Native Runtime Stack

The right answer is not "C runtime or Rust runtime."

The right answer is:

- C runtime as the canonical substrate
- Rust as the paired accelerated native lane
- Kain bundles as the one truth

That preserves the real work already present in `runtime/native`, keeps the ABI floor strong, and still gives Kain a practical path toward GPU-native UI and higher-end tool hosts.

## Practical Next-Step Standard

Before any new runtime feature is accepted, it should answer four questions:

1. Where is the canonical semantic meaning defined?
2. Which compiler-owned bundle carries it?
3. Which runtime service or capability key exposes it?
4. How do the native, web, and future selfhost lanes consume the same truth?

If a proposed feature cannot answer those four questions, it is not ready.
