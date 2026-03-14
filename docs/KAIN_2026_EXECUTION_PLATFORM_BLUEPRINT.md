# KAIN 2026 Execution Platform Blueprint

> **Date:** March 14, 2026  
> **Purpose:** Define the architectural north star for Kain as a serious 2026 language and execution platform, not a demo-only compiler.  
> **Scope:** Native runtime, UI, graphics, scripting, interop, bundle format, execution lanes, and phased delivery.

---

## 1. Mission

Kain is not trying to become "another language with some targets."

Kain is trying to become a full execution platform for:

- native software
- native UI tooling
- 3D DCC suites
- engines and realtime tools
- games
- DAWs and creative software
- automation and scripting
- web software
- host-driven interoperability with other languages

The user experience target is simple:

**If a developer has a wild idea in a `.kn` file, Kain should be able to execute it as real software with real UI, real graphics, real interop, and real runtime power.**

This blueprint exists to keep Kain moving toward that target in a structured way.

---

## 2. Core Thesis

Kain must become five things at once:

1. **Kain the language**
2. **Kain the compiler**
3. **Kain the bundle format**
4. **Kain the runtime platform**
5. **Kain the UI and tooling substrate**

If any one of those stays weak, the entire system feels incomplete.

The biggest historical gap has been the runtime substrate. That gap is what has made native software, native UI, and native 3D tooling harder than they should be.

---

## 3. Current Reality

Kain already has major pieces in place:

- a mature language/compiler core in the crate stack
- a serious CLI and backend routing path
- UE5-oriented code generation and editor/runtime crates
- web output capability
- importer/interop work through `kain-driver`, `kain-host`, and import lanes
- a semantic UI direction through `kain-ui`
- a Rust-hosted native UI path through `kain-ui-native`
- a raw LLVM native lane backed by the modularizing native runtime in `runtime/native`

This means the problem is no longer "can Kain compile anything real?"

The problem is now:

**How do we converge the working pieces into one coherent execution platform?**

---

## 4. Non-Negotiable End State

Kain should eventually support all of the following as first-class outcomes:

- `.kn -> native executable` with no Rust host requirement
- `.kn -> native executable` with high-end UI, docking, inspectors, timelines, and 3D viewports
- `.kn -> scripting lane` for Python-like automation and command workflows
- `.kn -> web app` without rewriting the authoring model around browser constraints
- `.kn -> engine/tool runtime` that can power DCC tools, editors, DAWs, games, and mocap software
- `.kn -> host/interoperability bundle` that can call into C, C++, Rust, TypeScript, shader pipelines, and external runtimes

The long-term user promise is:

**Kain should feel like one language with multiple execution lanes, not multiple disconnected products.**

---

## 5. Design Rules

### 5.1 Native First

The strongest expression of Kain must be native desktop and native tooling, not browser-first abstractions.

### 5.2 Bundle First

The compiler should emit a stable app/runtime bundle that becomes the truth consumed by runtimes. Source text should not remain the runtime truth after compilation.

### 5.3 UI First

Native UI is not a bonus feature. It is a core pillar of Kain's identity.

### 5.4 Realtime First

3D viewports, editors, DCC tools, and game-quality rendering are first-class, not side experiments.

### 5.5 Interop First

Kain must cooperate with C, C++, Rust, UE5, tree-sitter, shaders, and external host systems without ideological friction.

### 5.6 Data-Driven First

Capabilities, runtime bundles, backend selection, renderer features, widget metadata, and asset mappings should be described as structured data, not scattered hardcoded switches.

### 5.7 Demo to Platform

Every impressive demo must graduate into a reusable runtime capability, not remain a one-off spectacle.

---

## 6. The Platform Stack

Kain should be organized into the following layers.

### 6.1 Authoring Layer

This is what users write:

- `.kn` source
- Kain UI authoring
- scripting authoring
- engine/game/tool authoring
- host interop declarations

### 6.2 Semantic Compilation Layer

This layer owns:

- parsing
- typing
- lowering
- semantic UI IR
- semantic scene/runtime IR
- host/import IR
- shader/material IR

This is where Kain understands intent before backend lowering.

### 6.3 Bundle Layer

This layer produces the stable compiler-owned artifact format.

The bundle should be able to contain:

- executable code artifacts
- semantic UI graph output
- scene/runtime descriptors
- shader artifacts
- asset metadata
- reflection/type metadata
- runtime capability requirements
- packaging metadata

### 6.4 Runtime Services Layer

This is the execution substrate that must exist regardless of app kind.

It should be split into:

- `core`
- `platform`
- `gfx`
- `ui`
- `asset/io`
- `scene/sim`
- `tooling/debug`

### 6.5 Backend Layer

This layer adapts the same semantic/bundle truth into concrete execution backends.

Examples:

- raw native host
- Rust native host
- web host
- UE5 host
- script/host embedding lane

### 6.6 Product Layer

This is where flagship software emerges:

- DCC suite
- engine editor
- sculpt lab
- DAW
- mocap tools
- native app shells
- realtime games

---

## 7. Execution Lanes

Kain should explicitly support multiple lanes under one language identity.

| Lane | Purpose | Target Feel | Primary Outputs |
|---|---|---|---|
| Script Lane | Fast automation and scripting | Python-like speed of thought | host execution, tools, automations |
| Native App Lane | Desktop software and tools | native app shell, rich UI | `.exe`, app bundles |
| Realtime Lane | Engines, games, 3D tools | high-FPS, GPU-heavy, scene-centric | native executables, runtime bundles |
| Web Lane | Web software | semantic UI lowered to web adapters | JS/TS/WebAssembly/web bundles |
| UE5 Lane | Unreal interoperability | world-class engine integration | C++/plugin/codegen outputs |
| Host Lane | Embedding and cross-language control | "Kain orchestrates everything" | FFI, host bindings, service calls |

These lanes must share as much semantic infrastructure as possible.

They must not become six separate products.

---

## 8. Bundle Model

The compiler-owned bundle is the center of the platform.

### 8.1 Bundle Principles

- It is the runtime truth after compilation.
- It is backend-agnostic.
- It can be consumed by multiple runtimes.
- It declares required capabilities instead of assuming one host.
- It is serializable, inspectable, and versionable.

### 8.2 Bundle Families

Kain should eventually support these bundle shapes:

- `script bundle`
- `native app bundle`
- `realtime app bundle`
- `web bundle`
- `host service bundle`

### 8.3 Bundle Payload Areas

- `program`
- `ui`
- `scene`
- `render`
- `assets`
- `metadata`
- `interop`
- `requirements`
- `debug`

The exact schema can evolve, but the architectural center should not.

---

## 9. Runtime Ownership Boundaries

Kain should use the right language for the right layer.

### 9.1 C

Use C for the smallest stable ABI floor:

- bootstrap runtime
- minimal memory/platform glue
- portable host exports
- smallest cross-toolchain substrate

### 9.2 C++

Use C++ for heavier native systems where it buys real leverage:

- advanced renderer implementation
- asset import and conversion systems
- scene graph or ECS internals if needed
- editor/runtime integration layers
- engine-scale native modules

### 9.3 Rust

Use Rust where safety, maintainability, and ecosystem leverage matter:

- compiler infrastructure
- bundle tooling
- semantic UI runtime bootstrap
- asset pipeline orchestration
- reflection/metadata systems
- higher-level editor/tool hosts

### 9.4 Kain

Kain should progressively own:

- authored app logic
- UI declarations
- scripting
- tool behaviors
- graph and runtime semantics
- orchestration over lower-level services

### 9.5 Rule

The goal is not ideological purity.

The goal is **Kain-owned architecture with pragmatic implementation boundaries.**

---

## 10. Native UI Strategy

Native UI is the single biggest multiplier for Kain's long-term value.

### 10.1 What Exists Today

- `kain-ui` is the semantic direction
- `kain-ui-native` provides a Rust-hosted native path
- the raw native lane currently does not yet consume the same UI truth directly

### 10.2 What Must Happen

Kain UI must compile into a backend-agnostic semantic/render bundle that can be consumed by:

- Rust native host
- raw native runtime
- web host
- UE/editor host

### 10.3 Native UI Requirements

Kain UI must excel at professional tooling surfaces:

- docking
- inspectors
- outliners
- graph editors
- property grids
- asset browsers
- timelines
- overlays
- embedded realtime viewports
- command palettes
- data-heavy control surfaces

### 10.4 Native UI Convergence Rule

`kain-ui-native` should not remain "the truth."

It should become one backend implementation of the shared Kain UI semantic model.

---

## 11. Graphics and Realtime Strategy

Kain must support both "software exists" and "GPU-first power exists," but the future points toward GPU-native execution.

### 11.1 Realtime Requirements

- modern shader pipeline
- scene graph or equivalent entity/runtime model
- materials
- cameras
- lights
- asset loading
- viewport embedding in native UI
- character/controller support
- DCC/editor-grade manipulators

### 11.2 Rendering Architecture

The renderer stack should be split conceptually into:

- render API abstraction
- backend capability model
- shader pipeline
- frame graph or render pass graph
- resource lifetime management
- scene submission layer
- viewport compositing path

### 11.3 Backend Strategy

Near term:

- preserve the existing raw native path
- improve the native runtime platform split
- move away from duplicated demo rendering code

Medium term:

- introduce a real GPU backend strategy
- keep software rendering only as fallback/reference/testing path

Long term:

- make 3D viewports a normal Kain UI surface, not a special exception

---

## 12. Scripting Strategy

Kain should support a fast script lane that feels as immediate as Python, but with deeper integration into the platform.

### 12.1 Script Lane Goals

- fast startup
- dynamic workflows
- filesystem automation
- tree-sitter and tooling access
- external process orchestration
- UI and runtime access when needed
- host embedding

### 12.2 Script Lane Rule

Kain scripting should not be a separate toy language.

It should be a lighter execution lane of the same platform.

---

## 13. Interop Strategy

Interop is not optional for a dream language in 2026.

Kain should be able to:

- import from C
- import from Rust
- import from TypeScript
- interoperate with C and C++ runtime layers
- emit C++ for UE5
- drive tree-sitter and native libraries
- host external processes and services

Interop must be:

- explicit
- inspectable
- ABI-aware
- bundle-aware
- capability-aware

`kain-driver` and `kain-host` should evolve into first-class parts of this story, not side utilities.

---

## 14. Runtime Architecture Target

The native runtime should converge toward this layout:

```text
runtime/
  native_runtime.toml
  native/
    include/
    src/
      core/
      platform/
        win32/
        linux/
        macos/
      gfx/
        opengl/
        d3d12/
        vulkan/
        metal/
      ui/
      asset/
      scene/
      sim/
      tools/
      debug/
```

### 14.1 Native Runtime Rules

- no giant monolithic file as the long-term center
- capability boundaries must be clear
- platform-specific code stays isolated
- high-level features consume services instead of patching the whole runtime
- runtime bundles define what gets compiled and linked

---

## 15. Convergence Rules

To avoid fragmentation, Kain must obey these convergence rules.

### 15.1 One Semantic Truth

UI, scene, and runtime semantics should be authored once and consumed by multiple backends.

### 15.2 One Bundle Truth

The compiler-owned bundle must replace source-reparsing hosts as the normal runtime truth.

### 15.3 One Capability Registry

Backend capabilities should be described as data:

- renderer capabilities
- UI surface capabilities
- runtime service availability
- shader backend support
- interop availability

### 15.4 One Runtime Story

There can be multiple host implementations, but there cannot be six conflicting runtime philosophies.

---

## 16. Phased Delivery Plan

### Phase 1. Substrate Completion

Finish the serious runtime foundation:

- modular native runtime
- platform separation
- GPU backend path
- diagnostics/profiling hooks
- asset/io groundwork
- stable service boundaries

### Phase 2. Bundle Truth

Finish the compiler-owned bundle model:

- shared bundle schema
- bundle loading across lanes
- capability declarations
- runtime consumption without source reparsing

### Phase 3. Native UI Convergence

Make Kain UI a real cross-backend semantic system:

- semantic UI bundle payload
- native host consumer
- Rust host consumer
- viewport embedding as a first-class surface

### Phase 4. Realtime Core

Stand up the engine/tool stack:

- scene/runtime model
- render submission
- materials/shaders
- cameras/lights
- asset loading
- controller and interaction primitives

### Phase 5. Tooling Stack

Build the professional tool substrate:

- docking shell
- inspector
- timeline
- outliner
- graph systems
- asset browser
- command/debug surfaces

### Phase 6. Script and Host Lane

Make Kain a daily-driver automation language:

- lightweight execution mode
- tree-sitter and filesystem tooling
- host APIs
- shell/process orchestration
- bundled script services

### Phase 7. Flagship Apps

Prove the platform with software that feels undeniable:

- DCC tool
- engine editor
- sculpt tool
- DAW-style app
- game/tool runtime

---

## 17. Success Criteria

Kain becomes a real 2026 dream language when the following are true:

- a `.kn` file can produce native software with high-quality UI and graphics
- the same language can power scripting, apps, tools, and realtime systems
- UI and 3D are normal parts of the platform, not stitched-on demos
- multiple backends consume a shared semantic and bundle truth
- native apps do not depend on ad hoc runtime hacks
- high-end tools feel plausible, not hypothetical
- interop feels powerful instead of awkward

---

## 18. Anti-Patterns To Avoid

- adding impressive demos without extracting reusable runtime services
- keeping source text as runtime truth after compilation
- baking backend details directly into authoring semantics
- allowing Rust host, raw native host, and web host to drift into separate products
- using giant monolithic runtime files as the permanent model
- hardcoding runtime capabilities instead of declaring them as data
- treating native UI as optional

---

## 19. Immediate Repo Priorities

Given the current state of the repository, the next serious priorities are:

1. Finish modular native runtime extraction beyond the current `core`, `gfx/opengl`, and `platform/win32` seams.
2. Define the shared bundle schema for UI, scene, runtime, and capabilities.
3. Make the raw native runtime consume backend-agnostic UI semantics.
4. Introduce a real GPU-oriented renderer/backend plan for viewport surfaces.
5. Convert one or two current "holy shit demos" into reusable runtime services.

---

## 20. North Star Sentence

**Kain should feel like one language that can execute insane ideas as real software, with native UI, native graphics, deep interop, and enough runtime power that nobody mistakes it for a toy.**

