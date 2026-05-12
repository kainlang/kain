# Service Table

The native runtime service table is the data-driven capability registry for the
C runtime.

## Canonical Headers

- `kain_runtime_services.h`
- `kain_runtime_diagnostics.h`
- `kain_runtime_actor.h`
- `kain_runtime_async.h`
- `kain_runtime_reflection.h`
- `kain_runtime_compatibility.h`

## Registry Model

Each service descriptor carries:

- a stable key
- a readable name and description
- provider lane
- service status
- requirement level
- ABI version
- function-table pointer

The registry is bounded and explicit. It is designed to be queried during
startup validation and capability discovery.

## How Service Discovery Works

The runtime uses the table to decide:

1. which families are available on the current host
2. which provider lane owns the implementation
3. which services are required versus optional
4. whether the ABI version matches what the runtime expects
5. which function tables can be bound into the host bridge

That is why the table is more than a list of names. It is the decision point
between declared capability and live capability.

`io.process` now points at the owned `kain_native_process_*` function table on
the native core lane instead of the older vendor/libuv stub table. On Windows
that means the service reports a real child-process and PTY substrate; on
unsupported hosts the ABI still exists, but the status stays degraded and the
functions return explicit unsupported diagnostics.

## Service Families

### Base

- `base.memory`
- `base.diagnostics`

### Contract And Reflection

- `contract`
- `reflection`

### Actor And Async

- `actor.runtime`
- `actor.registry`
- `async.runtime`
- `async.timers`
- `io.loop`
- `io.fs`
- `io.net`
- `io.process`
- `io.timers`

### Platform

- `platform.app-host`
- `platform.input`
- `platform.window`

### Graphics / Scene / Runtime Inspection

- `gfx.viewport`
- `gfx.backend.bgfx`
- `gfx.backend.filament`
- `gfx.backend.diligent`
- `gfx.backend.forge`
- `gfx.shader`
- `gfx.material`
- `gfx.compute`
- `scene.runtime`
- `scene.query`
- `scene.mutation`
- `runtime.inspection`
- `device.reflection`

### UI

- `ui.bundle`
- `ui.component`
- `ui.layout.yoga`
- `ui.render.skia`
- `ui.backend.imgui`
- `ui.backend.rmlui`
- `ui.backend.slint`
- `ui.backend.qt`
- `ui.surface.browser.cef`
- `ui.devtools`

### Assets

- `asset.gltf`
- `asset.image.bimg`
- `asset.texture.bimg`
- `asset.ingestion`
- `asset.realtime`

### Host Bridge / Script / Audio

- `host.bridge`
- `script.quickjs`
- `audio.backend`
- `audio.graph`
- `audio.device`
- `audio.assets`

### WASM / Allocators / Compatibility

- `wasm.runtime.light`
- `wasm.runtime.full`
- `wasm.module`
- `wasm.wasi`
- `allocator.mimalloc`
- `allocator.rpmalloc`
- `compatibility`

## Provider Lanes

The service registry can attribute a service to:

- native core
- platform Win32/Linux/macOS
- Rust host
- Python host
- Node host
- external integration

## Migration Note

Legacy masks in the older contract header are transition scaffolding. The new
service table is the direction of travel.
