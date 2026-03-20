# KAIN GPU-Native UI And Shader Authoring Research 2026

> Date: March 19, 2026  
> Purpose: Ground Kain's GPU-native UI and shader direction in the current repo state and external primary-source research.

## Executive Read

Kain is already much closer to GPU-native software than most language projects:

- `crates/kain-ui` already defines a retained, semantic, patch-first UI direction.
- `crates/kain-3D` already contains a real `wgpu` renderer.
- `crates/kain-ui-native` can already pair with `wgpu`, even though it still defaults to the older path.
- `.kn` shader authoring already exists and the driver already emits SPIR-V, HLSL, reflection data, and shader bundles.

The gap is not "does Kain have any GPU path?"

The gap is:

- making GPU the canonical native UI path instead of a partial side lane
- making the developer loop feel as immediate as React and modern web tooling
- making shader authoring automatic and natural enough that users do not think in terms of SPIR-V files

## Current Kain Position

### 1. UI semantics are already pointed in the right direction

`crates/kain-ui/NORTH_STAR_SPEC.md` already calls for:

- a native-first UI runtime
- a renderer-agnostic semantic graph
- fine-grained reactive invalidation
- deterministic patch streams instead of virtual DOM as the center
- web as an adapter target, not the canonical truth

That is the right architecture for a language trying to surpass browser-era assumptions.

### 2. The native UI host is not fully GPU-native yet

`crates/kain-ui-native` currently proves that native host integration is real, but it is still transitional:

- it still centers an `egui` host shell
- it still carries `Glow` / software-style fallback behavior
- it can use `wgpu`
- some viewport paths still go through readback-style bridging instead of a fully unified compositor

This is enough to prove viability, but not enough to define Kain's end state.

### 3. The repo already has a serious GPU renderer

`crates/kain-3D/src/wgpu_renderer.rs` is a real `wgpu` renderer, not a placeholder. This matters because it means Kain does not need to invent its first modern GPU lane from scratch.

### 4. Kain shader authoring already mostly works

The repo already contains real in-language shader authoring examples:

- `shader compute ...`
- `shader fragment ...`
- `shader vertex ...`

These appear in tests and driver fixtures across the tree.

The driver already emits:

- SPIR-V modules
- reflection metadata
- shader bundle JSON
- derived HLSL
- Rust host wrappers

So the core idea, "write shaders naturally in Kain," is already real.

### 5. The main shader pain is not authoring syntax

The sharp edge is build flow. `docs/recent/KAIN_COMPILER_FEEDBACK.md` documents that shader blocks still require manual `shaders = []` bookkeeping in `kain.toml`.

That means Kain already supports natural shader authoring, but it does not yet make shader harvesting feel fully native or automatic.

## External Research

### React And Modern Web Tooling

React's performance story is increasingly about compilation and dev-loop quality, not just component syntax.

- React's official docs say React Compiler is a build-time optimization tool that automatically memoizes components and values.
- React's `memo` docs explicitly say that with React Compiler enabled, you typically do not need `React.memo` anymore because the compiler optimizes component re-rendering automatically.
- Next.js documents Fast Refresh as preserving temporary client-side state when safe, with most edits visible within a second.
- Next.js also documents that its Rust-based compiler stack improved Fast Refresh and build speed materially.

What that means for Kain:

- React-speed iteration is not "use a virtual DOM"
- React-speed iteration is "small invalidation scope, preserved state, instant rebuilds, and compiler help"

That is good news, because Kain's retained semantic graph is actually a better long-term base than broad rerender passes.

### Slint

Slint is one of the clearest external proofs that a native-first reactive UI stack can be both expressive and efficient.

Official Slint docs say:

- reactivity is core to the language
- the runtime tracks dependencies between properties
- dependent bindings are marked dirty and re-evaluated lazily
- backend selection can explicitly require `wgpu`

What that means for Kain:

- Kain should keep doubling down on exact dependency invalidation
- Kain should treat GPU-native backend selection as a first-class runtime capability
- Kain should avoid letting browser-first mental models set the ceiling

### Xilem And Vello

The current Xilem docs describe a model that combines ideas from Flutter, SwiftUI, and Elm while diffing lightweight view values into minimal updates to a retained UI.

The same docs state:

- Xilem rendering is built on Vello
- Vello is a high-performance GPU compute-centric 2D renderer
- GPU compute infrastructure is provided by `wgpu`

What that means for Kain:

- a modern Rust-native GPU UI stack is viable today
- the winning pattern is retained UI plus minimal updates plus a GPU renderer
- Kain does not need to choose between professional native UI and rapid iteration

### WGPU And WGSL

The official `wgpu` repo describes it as a cross-platform, safe, pure-Rust graphics API that runs natively on Vulkan, Metal, D3D12, and OpenGL, and on top of WebGPU and WebGL on wasm.

The same source states:

- native `wgpu` uses Naga to translate WGSL to platform shading languages
- on the web, WGSL is passed through to the browser's WebGPU implementation

The WGSL specification is now a formal W3C standard-track document.

What that means for Kain:

- `wgpu` is the fastest realistic bridge between native GPU software and web GPU software
- Kain should treat WGSL as a first-class web payload even if SPIR-V remains the native-first payload today
- Kain can unify authoring while still emitting different backend artifacts for different runtime environments

### Rust GPU And Slang

Rust GPU proves that shaders can be authored in a host-adjacent language while compiling to SPIR-V under the hood.

Official Rust GPU docs explicitly describe:

- writing shaders in Rust
- using `spirv-builder` to compile them into SPIR-V modules
- attaching those modules to the host build

Slang proves a second, equally important pattern:

- author in a high-level shader language
- target HLSL, GLSL, SPIR-V, WGSL, Metal, and more
- allow target-specific escape hatches only where necessary

What that means for Kain:

- yes, shader code can feel native to the language
- no, the backend artifact does not disappear
- the right design is to hide target compilation from the author, not pretend no target payload exists

## Core Recommendation

### 1. Keep `kain-ui` as the semantic center

Do not let the end-state architecture collapse into "whatever `egui` can conveniently host."

`kain-ui` should remain the owner of:

- semantic node kinds
- retained identity
- reactive invalidation
- patch generation
- layout and style intent
- viewports, overlays, panels, timelines, inspectors, and graph surfaces as first-class semantics

### 2. Make native desktop fully GPU-native first

The fastest practical route is:

- `wgpu` as the canonical native GPU backend
- shared device/surface ownership between UI chrome and 3D viewport systems
- one compositor that can handle 2D UI chrome, text, vector and shape layers, image content, embedded realtime surfaces, and overlays and gizmos

For native desktop, Kain should stop aiming at "GPU-assisted UI" and aim at "GPU-owned presentation."

### 3. Use a retained patch model, not an immediate-mode end state

Immediate-mode UI is useful as a bootstrap tool, debug shell, and host scaffold.

It is not the best long-term center for:

- stable node identity
- rich docking systems
- low-cost partial updates
- editor-scale introspection
- preserving tool interaction state during live updates

Kain's own spec already points toward the better answer: retained graph plus exact patches.

### 4. Chase React-speed iteration through the dev loop, not by imitating React internals

Kain should target the user experience React developers actually care about:

- edits show up in under a second
- local state is preserved where safe
- broken edits recover cleanly
- only affected modules and surfaces update
- the UI runtime can explain what invalidated

That means Kain needs a development loop with:

- incremental compilation
- module-level and bundle-level hot reload
- state-preserving patch replay
- runtime invalidation boundaries
- good diagnostics for "why this edit forced remount/rebuild"

This is much closer to Fast Refresh plus an incremental compiler than it is to a traditional native app rebuild loop.

### 5. Prefer a hybrid web strategy first

For web, Kain should not force a fake purity test.

The practical first strategy is:

- ordinary app chrome can lower to DOM-backed or browser-native primitives where that buys accessibility and leverage
- heavy panels, node graphs, timelines, canvases, and 3D surfaces should run on WebGPU
- a full-canvas WebGPU editor shell can remain a higher-end lane for DCC-class apps

Native desktop should be uncompromisingly GPU-native first. Web should be strategically hybrid until the full-canvas editor lane is mature enough.

### 6. Make shader authoring feel native, but keep emitted payloads real

The correct user experience is:

- users write shader code in `.kn`
- the compiler recognizes shader blocks automatically
- the driver emits the right shader bundle automatically
- native and web runtimes pick the payload they need

The incorrect mental model is:

- "we do not compile to SPIR-V or WGSL at all"

GPUs still need concrete target payloads. The trick is to make that an internal compiler/runtime concern rather than a user concern.

## Recommended Kain Shader Model

### Authoring truth

Kain source remains the authored truth:

- `shader compute`
- `shader fragment`
- `shader vertex`
- future material and pipeline declarations

### Compiler truth

Kain should lower shader code into a compiler-owned shader IR and reflection model before target emission.

### Runtime truth

The runtime consumes a `ShaderArtifactBundle`, not free-floating backend-specific files.

### Target payloads

Kain should emit one bundle that may contain:

- native payloads such as SPIR-V
- web payloads such as WGSL
- derived outputs such as HLSL and USF
- reflection and resource layouts
- debug/source mapping data

This already lines up with the current `ShaderArtifactBundle` type, which supports `Spirv`, `Wgsl`, `Hlsl`, and `Usf`.

## What Already Works Today

These statements are already true in the repo right now:

- you can author shaders directly in `.kn`
- the driver can compile shader artifact bundles
- the runtime and app bundling path already has a place for shader bundles
- reflection metadata already exists
- `wgpu` native rendering already exists

## What Is Still Missing

These are the highest-value missing pieces:

1. Automatic shader discovery and harvesting during normal build passes
2. A first-class UI runtime bundle consumed across all lanes
3. A default native `wgpu` UI path
4. A single GPU compositor story for chrome plus 3D surfaces
5. Hot reload and state-preserving UI patch replay
6. Stronger SPIR-V and shader backend coverage for advanced shader workloads
7. A first-class WGSL story for the web lane

## Recommended Next Steps

1. Change the native UI default renderer preference from the legacy path to `wgpu`.

2. Introduce a compiler-owned `UiRuntimeBundle` that carries retained tree data, surface descriptors, layout metadata, and patch contracts.

3. Add a native `wgpu` compositor service that can present Kain UI chrome, text and vector content, embedded 3D viewports, and overlays and editor gizmos.

4. Build a Kain dev loop around incremental compilation plus state-preserving patch replay.

5. Make shader blocks auto-discover during normal `.kn` compilation.

6. Keep SPIR-V as a strong native payload, but elevate WGSL to a first-class web payload in the bundle model.

7. Add one flagship proving app that uses the same Kain semantics across native and web, including a docked shell, inspector, timeline, graph panel, and 3D viewport.

8. Add conformance that verifies the same authored app and shader bundle can travel across lanes without semantic drift.

## Bottom Line

Kain does not need to choose between:

- GPU-native power
- React-level iteration speed
- natural shader authoring

The architecture that supports all three is:

- retained semantic UI
- exact reactive invalidation
- incremental compiler-driven hot reload
- `wgpu` as the practical cross-platform GPU spine
- Kain-authored shaders compiled automatically into runtime bundles

That is the shortest serious path to a language that can turn intent into high-end software instead of just compiling code into isolated targets.

## Sources

- [React Compiler](https://react.dev/learn/react-compiler)
- [React memo reference](https://react.dev/reference/react/memo)
- [Fast Refresh](https://nextjs.org/docs/architecture/fast-refresh)
- [Next.js Compiler](https://nextjs.org/docs/architecture/nextjs-compiler)
- [Slint Reactivity](https://docs.slint.dev/latest/docs/slint/guide/language/concepts/reactivity/)
- [Slint BackendSelector](https://docs.slint.dev/latest/docs/rust/slint_interpreter/struct.BackendSelector)
- [Xilem crate docs](https://docs.rs/xilem/latest/xilem/)
- [wgpu repository](https://github.com/gfx-rs/wgpu)
- [WGSL specification](https://www.w3.org/TR/WGSL/)
- [Rust GPU writing shader crates](https://rust-gpu.github.io/rust-gpu/book/writing-shader-crates.html)
- [Slang interop and target switching](https://docs.shader-slang.org/en/stable/external/slang/docs/user-guide/a1-04-interop.html)
- [Your first Slang shader](https://shader-slang.org/docs/first-slang-shader)
