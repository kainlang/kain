# KAIN Graphics Vendor Integration Plan

Date: 2026-04-11

## Goal

Integrate the new graphics-oriented third-party vendors into Kain without collapsing the native runtime into three competing renderers.

The target shape is:

- `bgfx` as the first practical cross-platform renderer/backend lane
- `bx` and `bimg` as support infrastructure for that lane
- `filament-core` as a higher-level premium scene/material renderer experiment
- `diligentcore` as the future explicit Kain-native render-graph / compute / device-control lane

The non-goal is wiring all three renderers into the same `gfx.viewport` path as peers.

## Current Runtime Reality

The current native runtime already has real graphics-facing seams, but they are uneven:

- `gfx.viewport` is still a Windows-only host path with Win32 + OpenGL-oriented behavior.
- `scene.runtime`, `scene.query`, and `scene.mutation` already exist as Kain-owned scene/runtime contracts.
- `gfx.shader`, `gfx.material`, and `gfx.compute` already exist as Kain-owned service families.
- `kain_runtime_graphics.h` already carries render-graph, residency, and compute-schedule contract structures.
- The current manifest/compiler path is still effectively C-oriented. It compiles C sources directly and does not yet have a deliberate mixed C/C++ vendor bridge model.

That last point matters because the new renderer vendors are not shaped like `libuv` or `miniaudio`.

## Vendor Assessment

### `bgfx`

Best first integration candidate.

Why:

- strong cross-platform backend coverage
- useful for viewport bring-up, swapchain/device control, debug draw, editor rendering, and early scene/runtime execution
- includes a C99 surface under `include/bgfx/c99/bgfx.h`, which lowers wrapper friction for a C-first runtime

What Kain should use it for first:

- `gfx.backend.bgfx`
- `gfx.device.bgfx`
- `gfx.swapchain.bgfx`
- `gfx.debugdraw.bgfx`
- `gfx.viewport.bgfx`

### `bx`

Support substrate for the `bgfx` lane.

Why:

- `bgfx` is not realistically a one-folder drop without it
- useful for math, platform glue, allocators, and small engine primitives

Kain rule:

- treat `bx` as internal support for the `bgfx` lane, not as a standalone Kain service family

### `bimg`

Texture/image support for the `bgfx` lane.

Why:

- image loading and decoding
- texture format plumbing
- mip/container support

What Kain should use it for first:

- `asset.image.bimg`
- `asset.texture.bimg`

### `filament-core`

High-value, but not a first direct runtime lane.

Why:

- strongest path to premium viewport visuals, PBR, lighting, IBL, sky, shadows, and authored material presentation
- good fit for a “Kain scenes look serious quickly” experiment

Constraint:

- this tree is C++-heavy and should not be stuffed directly into the current C-first native manifest path

What Kain should use it for later:

- `scene.renderer.filament`
- `gfx.material.filament`
- `gfx.lighting.filament`

### `diligentcore`

Strategically important, but not the first renderer to wire.

Why:

- lower-level than Filament
- more compatible with a future Kain-owned render graph, compute lane, shader pipeline model, and explicit device architecture
- useful if Kain wants to own more renderer policy without hand-writing raw backend duplication

Constraint:

- also C++-heavy and substantially larger than `bgfx`

What Kain should use it for later:

- `gfx.backend.diligent`
- `gfx.rendergraph.diligent`
- `gfx.compute.diligent`
- `gfx.pipeline.diligent`

## Non-Negotiable Architecture Rules

1. Kain keeps ownership of renderer-facing service identity.

Vendor code may provide implementation, but Kain owns:

- service keys
- startup contract truth
- diagnostics
- resource lifetime policy
- scene/runtime semantics
- future scheduler integration

2. Do not wire all three renderers into the same host path as peers.

The runtime should not expose one ambiguous “some renderer” path backed by three unrelated engines.

3. Do not let `gfx.viewport` become the renderer contract.

`gfx.viewport` is the host-facing presentation lane. It should consume a Kain renderer backend, not define the renderer model itself.

4. Do not push raw vendor APIs upward.

The rest of Kain should not depend on `bgfx_*`, Filament classes, or Diligent interfaces directly.

5. Do not pull large C++ vendors straight into the current native C manifest path without a bridge strategy.

`bgfx` may be able to enter first because of the C99 API surface, but `filament-core` and `diligentcore` should first land behind explicit bridge libraries or a mixed-language build extension.

## Recommended Integration Model

### Layer 1: Host And Viewport

Keep these Kain-owned:

- `platform.app-host`
- `platform.input`
- `gfx.viewport`

The host owns:

- window creation
- native handles
- resize lifecycle
- input flow
- presentation timing hooks

The host does not own renderer semantics.

### Layer 2: Renderer Backend

Introduce a backend layer that `gfx.viewport` talks to:

- `gfx.backend.bgfx`
- later `gfx.backend.diligent`

This layer owns:

- renderer initialization
- native window/swapchain binding
- frame begin/end
- render target lifecycle
- debug drawing
- texture upload staging

### Layer 3: Scene / Material / Premium Rendering

Keep higher-level rendering separate from the backend:

- `scene.renderer.filament`
- later Kain-owned `scene.renderer.native`

This layer owns:

- scene presentation
- lighting
- material model application
- sky / IBL / shadows
- premium viewport appearance

This lets Kain use `bgfx` first without locking the language/runtime to `bgfx` forever.

## First Wiring Order

### Phase 0: Build-Path Preparation

Before adding graphics vendors to the runtime manifest directly:

- decide whether `runtime/native` will support mixed C/C++ compilation in the manifest path
- or create explicit bridge libraries with a narrow C ABI surface that the native runtime links against

Recommended answer:

- use a bridge strategy for `filament-core` and `diligentcore`
- allow direct manifest integration only for the pieces that naturally expose a C-facing surface

### Phase 1: `bgfx` + `bx` + `bimg`

This is the first real renderer incorporation lane.

Implement:

- Kain-owned bgfx wrapper surface
- Kain-owned texture/image helper surface using `bimg`
- platform window-handle handoff from existing viewport host code

Target service additions:

- `gfx.backend.bgfx`
- `gfx.device.bgfx`
- `gfx.swapchain.bgfx`
- `gfx.debugdraw.bgfx`
- `asset.image.bimg`
- `asset.texture.bimg`

Target files:

- `runtime/native_runtime.toml`
- `runtime/native_runtime_metadata.json`
- `runtime/native/include/kain_runtime_services.h`
- `runtime/native/src/core/kain_runtime_services.c`
- `runtime/native/include/kain_runtime_contract.h`
- `runtime/native/src/core/kain_runtime_contract.c`
- `runtime/native/include/kain_runtime_vendor_lane.h`
- `runtime/native/src/vendor/kain_runtime_vendor_lane.c`
- new `runtime/native/include/kain_runtime_renderer_backend.h`
- new `runtime/native/src/gfx/kain_runtime_bgfx_backend.c`
- new `runtime/native/src/gfx/kain_runtime_bimg_assets.c`
- new platform adapters such as:
  - `runtime/native/src/platform/win32/kain_runtime_viewport_bgfx_win32.c`
  - `runtime/native/src/platform/linux/kain_runtime_viewport_bgfx_linux.c`

Why this comes first:

- cross-platform viewport/rendering value is immediate
- `bgfx` has a C99 API surface
- it creates a usable runtime rendering substrate before the premium renderer and future render-graph lane

### Phase 2: Viewport Backend Split

Refactor `gfx.viewport` so the host path is not welded to one renderer implementation.

Do:

- keep the host/window layer platform-specific
- move rendering commands and swapchain/device behavior behind backend selection
- make Windows and Linux use the same renderer-facing backend surface even if their window/input code differs

Outcome:

- Kain gains a real cross-platform viewport contract instead of a Windows-only OpenGL path

### Phase 3: `filament-core`

Land Filament as a separate premium renderer experiment, not as the default renderer backend.

Do:

- build a narrow C ABI bridge around the Filament scene/material/lighting surface
- map Kain-authored scene/material data into that bridge
- keep it optional and clearly scoped

Target services:

- `scene.renderer.filament`
- `gfx.material.filament`
- `gfx.lighting.filament`

Why this is not first:

- it solves “look expensive,” not “get a real cross-platform renderer lane online”
- it is a higher-level renderer worldview and should not become the base backend accidentally

### Phase 4: `diligentcore`

Land Diligent as the future explicit-engine lane.

Do:

- build a bridge around device, pipeline, shader, and compute setup
- use it to prototype a more explicit Kain-owned render graph and compute/runtime model

Target services:

- `gfx.backend.diligent`
- `gfx.rendergraph.diligent`
- `gfx.compute.diligent`
- `gfx.pipeline.diligent`

Why this comes after `bgfx`:

- it is the deeper architecture play, not the fastest practical viewport unlock

## Practical Build Doctrine

For the next pass, treat the new graphics vendors in two groups.

### Group A: Can enter the native runtime path earlier

- `bgfx`
- `bx`
- `bimg`

Reason:

- `bgfx` already exposes a C99 layer, which makes the first Kain wrapper strategy much lighter

### Group B: Should use dedicated bridge libraries first

- `filament-core`
- `diligentcore`

Reason:

- both are C++-forward engines
- forcing them directly into the current manifest-driven C runtime path would create build-system churn before Kain has even stabilized the renderer-service model

## Immediate Next Step

Do not wire Filament or Diligent first.

The next clean execution slice is:

1. teach the runtime build story how to host a graphics backend cleanly
2. wire `bgfx` + `bx` + `bimg` as the first real cross-platform renderer lane
3. split `gfx.viewport` into host-facing and backend-facing halves
4. only then start Filament and Diligent as separate experiments

## Decision Summary

- `bgfx` is the first renderer to wire
- `bx` and `bimg` move with `bgfx`
- `filament-core` is a premium scene renderer experiment, not the base backend
- `diligentcore` is the future explicit Kain render-architecture lane, not the first bring-up target
- `gfx.viewport` stays host-facing
- Kain owns service contracts
- large C++ vendors should cross into the runtime through deliberate bridge strategy, not by dumping their full source trees into the current C manifest path
