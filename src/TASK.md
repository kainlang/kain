# Source Rewrite Task Board

This file is the operator handoff for parallel source ownership work under
`src/`.

## Global Rules

- All hand-owned work goes under `src/<folder>`.
- `src/.rustimport` is reference-only. Do not edit it.
- `src/.legacy` is donor/reference-only. Do not edit it.
- Keep the language name as `Kain`. Do not introduce a rename campaign.
- Keep folder names aligned with the bootstrap/runtime shape where practical.
- Do not pull in UE5 surfaces for this wave.
- Prefer complete, owned Kain files over placeholders, but keep scope bounded to
  the assigned lane.
- If a lane depends on another unfinished lane, define the clean boundary and
  keep moving instead of blocking on unrelated work.

## Current Folder Plan

- `src/core`
- `src/driver`
- `src/sys-codegen`
- `src/interop`
- `src/c-ffi`
- `src/crate-ffi`
- `src/ui`
- `src/3d`
- `src/gpu-runtime`
- `src/host`

## Important Note About UI

For this wave, we are **not** splitting `ui-native` into a separate owned source
folder yet.

`src/ui` owns:
- Kain UI semantics
- runtime-facing UI models
- temporary host projection boundaries if needed

Do not create `src/ui-native` as a parallel rewrite lane right now unless the
operator explicitly asks for it later.

## Agent Assignments

### Agent Alpha

- Folder: `src/core`
- Task: Own the foundational language core.
- Deliver: `ast`, `span`, `diagnostic`, `error`, `lexer`, `parser`, `effects`,
  `types`, `comptime`, `runtime`, `stdlib`, `low_level_abi`,
  `low_level_memory`, `low_level_memory_metadata`, `kainc`.
- Goal: Make `src/core` the canonical owned semantic center.

### Agent Delta

- Folder: `src/driver`
- Task: Own the Kain-side compiler driver and orchestration surface.
- Deliver: source loading, target selection, compile flow, artifact planning,
  and the bridge from `src/core` into codegen/runtime lanes.
- Goal: Replace Rust-side driver assumptions with a Kain-owned driver layer.

### Agent Forge

- Folder: `src/sys-codegen`
- Task: Own system/native code generation.
- Deliver: LLVM-first codegen, low-level output planning, and any later C++/Rust
  compatibility boundaries only if they help bootstrap.
- Goal: Make native codegen a Kain-owned lane with LLVM as the priority target.

### Agent Bridge

- Folder: `src/interop`
- Task: Own shared runtime payload contracts.
- Deliver: shared value, buffer, image, metadata, and transport-friendly
  structures that other lanes can consume.
- Goal: Give Kain a clean runtime data contract for host, UI, GPU, and native
  execution.

### Agent Anvil

- Folder: `src/c-ffi`
- Task: Own the C runtime bridge.
- Deliver: C import model, generated binding surfaces, runtime preparation, and
  C-facing glue needed by native execution.
- Goal: Make the C runtime a first-class Kain-owned dependency lane.

### Agent Cargo

- Folder: `src/crate-ffi`
- Task: Own external crate ecosystem bridging.
- Deliver: crate import metadata, manifest resolution boundaries, and the
  Kain-side model for talking to Rust crates without making Rust the semantic
  center.
- Goal: Keep external crates as tools Kain can use, not the thing running the
  show.

### Agent Canvas

- Folder: `src/ui`
- Task: Own the Kain UI semantic model.
- Deliver: UI graph/types, runtime execution model, patch/update semantics, and
  any temporary host projection seams.
- Goal: Keep UI expressive and Kain-owned without splitting off `ui-native`
  yet.

### Agent Vector

- Folder: `src/3d`
- Task: Own 3D scene, primitives, interaction, and renderer-facing contracts.
- Deliver: authored scene model, primitive library, math/interaction surfaces,
  and renderer handoff boundaries.
- Goal: Make 3D a native Kain capability instead of an adapter afterthought.

### Agent Vulkan

- Folder: `src/gpu-runtime`
- Task: Own the compute/runtime GPU execution lane.
- Deliver: dispatch requests, bindings, executor model, and runtime-facing GPU
  contracts that consume Kain-emitted plans.
- Goal: Keep GPU execution close to Kain-native runtime truth.

### Agent Anchor

- Folder: `src/host`
- Task: Own host/runtime embedding and execution boundaries.
- Deliver: host registration, runtime bridge surfaces, execution entry seams,
  and the host-side model for running Kain programs safely.
- Goal: Keep host integration explicit, clean, and subordinate to Kain
  semantics.

## Suggested Prompt Format

Use this pattern when delegating to another agent:

`You are Agent Alpha. Own src/core only. Do not edit src/.rustimport or src/.legacy. Your job is to translate the assigned lane into hand-owned Kain code, keep the name as Kain, skip UE5, and preserve clean boundaries with the other src folders.`
