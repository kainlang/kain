# Kain Working Memory

This file is the running memory for major architectural moves in `M:\Code\Kain`.
It is not meant to be a raw changelog.
It should preserve:

- what we were trying to make true
- what the system understands now that it did not before
- what design bets we made on purpose
- what remains incomplete or dangerous
- what future work should preserve instead of accidentally undoing

## 2026-03-20 - Tensor-Stream Compute Lane Becomes Real Compiler/Runtime Memory

Today the work stopped being "Kain has compute shaders somewhere" and started becoming "Kain is learning how to describe a compute-native execution lane as compiler-owned truth."

The most important shift is semantic, not cosmetic:
we pushed the system from vague GPU capability toward a structured model where a compute shader can now carry authored intent about dispatch, tensor payloads, stream roles, and neural-node planning.
That matters because the long-term goal is not just to emit SPIR-V blobs.
The goal is for Kain to understand continuous dataflow at compile time and then hand native runtimes enough structure to execute that intent coherently.

### What Changed In Practice

On the compiler side, `kain-core` now supports explicit compute-plan metadata authored from shader `comptime` blocks.
The current convention is intentionally constrained and conservative:

```kn
let compute = (
    [dispatch_x, dispatch_y, dispatch_z],
    [
        ("binding", "element_type", ["shape", "dims"], "role", "contract")
    ],
    [
        ("node_key", "op", ["inputs"], ["outputs"], stateful)
    ]
)
```

That data is now parsed, validated, and threaded into emitted realtime bundles and runtime contracts.
When present, it overrides the older heuristic-only path for dispatch/tensor/node planning.
When absent, the legacy fallback still exists so the broader tree does not collapse.

On the native-runtime side, the raw-native lane now does more than validate `primary_compute`.
It has a real execution handoff/fallback state path:

- the graphics bundle is loaded explicitly in the raw-native viewport lane
- `primary_compute` is executed into a per-frame runtime state record
- that execution state is surfaced in overlay/debug information
- viewport rendering now reflects that compute state instead of pretending it does not exist

This is not yet true GPU compute dispatch.
It is a runtime-owned bridge between "metadata exists" and "execution semantics are visible and alive."
That bridge is important because it gives us a place to evolve dispatch, residency, scheduling, and future SPIR-V execution without falling back into one-off demo code.

### Architectural Meaning

Three ideas became more concrete today:

1. Compute is no longer just a shader stage.
It is being treated as a first-class execution domain with tensor, stream, and neural semantics.

2. The compiler is beginning to own dispatch intent.
Even though some fallback behavior remains, the direction is now explicit:
dispatch sizing and operator metadata should be authored and emitted, not guessed by hosts forever.

3. The raw-native runtime is no longer purely passive.
It now has a legitimate role in executing and surfacing compute plans rather than only rejecting malformed metadata.

### Design Bets We Made On Purpose

- We preferred `comptime`-block authored metadata over adding a wider public AST break for shader constructors.
  That let the feature land without detonating other crates that instantiate `Shader` directly.

- We treated tensor and stream semantics as data attached to resource bindings, not as hardcoded runtime assumptions.
  This keeps the door open for future backends, NPUs, CUDA, or other ML/runtime targets.

- We added a native execution fallback/handoff instead of pretending full GPU dispatch was already solved.
  This gives us a truthful intermediate substrate that can still drive viewport/runtime behavior.

### What Is Still Incomplete

- True native GPU compute execution is not wired yet.
  The runtime fallback executes a compute-state model, not actual SPIR-V dispatch.

- `workgroup_size` still has fallback behavior when authored metadata is absent.

- Tensor shapes are explicit only when authored.
  Otherwise they still fall back to inferred/simple defaults.

- Neural nodes are still a compiler-emitted operator plan, not a true runtime scheduler with residency, fusion, or dependency orchestration.

- Full workspace tests are still not globally green due to repo-level issues outside this slice.
  The notable blockers during validation were:
  an external missing fixture under `M:\Code\Other\kainselfhosting\...`
  and linker OOM pressure in large CLI test binaries on Windows.

### What Future Work Should Preserve

- Do not collapse tensor/stream/neural metadata back into anonymous compute bindings.
  The whole point is that Kain should progressively understand dataflow structure, not just pass through lower-level payloads.

- Do not move dispatch ownership back into host heuristics if authored metadata exists.

- Do not let raw-native, Rust-native, and future backends invent separate compute-plan dialects.
  The emitted bundle must stay the center of truth.

- If we add real SPIR-V/native compute dispatch next, it should consume the same `primary_compute` plan and enrich it, not replace it with a host-local shortcut.

### Next Serious Move

The next step is to connect this authored compute-plan lane to actual backend execution:

- compiler-owned dispatch/workgroup truth should become standard, not optional
- tensor shape metadata should map to real residency/buffer layouts
- `primary_compute` should dispatch through a real execution backend
- neural-node plans should graduate from descriptive metadata into runtime scheduling primitives

If this direction holds, Kain stops looking like "a language that can target GPU shaders" and starts looking more like "a language/runtime that understands heterogeneous dataflow as part of compilation itself."

## 2026-03-20 - Explicit Compute Plans Landed, Runtime Execution Stopped Being Purely Decorative

The follow-up move today was to stop pretending the compiler and runtime were "close enough" on compute intent.
We added an authored compute-plan path and then made the raw-native viewport consume executable compute state instead of treating `primary_compute` as a validation artifact.

### What Changed

On the compiler side, compute shaders can now carry an explicit authored plan through `comptime` data.
That plan gives the compiler an intentional source of truth for:

- dispatch size
- tensor binding metadata
- neural node planning

This is materially different from the earlier heuristic pass.
The heuristic path still exists for compatibility, but there is now a real authored lane that tells the compiler what the compute workload is supposed to mean.

On the native side, the raw-native viewport now loads the graphics bundle and drives a real per-frame compute execution state.
This is still a fallback execution substrate, not full SPIR-V/Vulkan dispatch in the C runtime, but it means:

- `primary_compute` is stepped every frame
- dispatch counts, tensor counts, stream counts, and neural-node counts now live as runtime state
- that execution state feeds overlay/debug output
- viewport rendering can respond to compute phase instead of acting like compute metadata is inert

### Why This Matters

Before this pass, the runtime could say "the compute plan is valid."
After this pass, the runtime can at least say "the compute lane is alive right now, here is the state it is producing, and the host is reacting to it."

That is still not the final end state, but it is a meaningful transition:
validation-only systems die in place.
Execution-visible systems become pressure points that force the backend story to mature.

### What Is Still Missing

The actual last leap is still ahead:

- full SPIR-V dispatch promoted from test-only Vulkan code into a reusable runtime service
- shared-buffer residency and binding moved from descriptive contracts into true backend resource ownership
- native runtime compute results feeding real scene buffers, materials, particles, terrain, or viewport surfaces instead of only debug/live-state channels

The repo already contains a strong clue for the next move:
`crates/gpu/tests/spirv_execute.rs` is not hypothetical.
It is a real Vulkan SPIR-V execution harness.
The correct direction is to promote that into a reusable runtime/backend service rather than rebuilding execution semantics from scratch in every host.

### Guardrail

Do not let the fallback execution path become the final architecture.
It exists to keep the compute lane alive while we promote real backend dispatch into the runtime story.

## 2026-03-20 - Root Repo Map And Compute Docs Were Brought Back Into Sync

This run tightened the repo's documentation around the compute lane and the top-level layout.

### What Changed

- Added a top-level `repomap.md` so the root workspace has the same folder-guide treatment as `crates/`.
- Updated the README to describe authored compute metadata as compiler-owned truth, not a runtime heuristic.
- Documented the raw-native viewport bridge as a compute execution/state surface, not a full GPU dispatcher.

### Lesson

When a feature starts crossing compiler, runtime contract, and native viewport boundaries, the docs should call out the ownership split explicitly.
That keeps future changes from collapsing authored intent back into host-local inference.
