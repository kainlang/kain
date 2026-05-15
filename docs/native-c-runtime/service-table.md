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

The active production registry is the lean 31-service catalog. Service keys
that used to name runtime-owned vendor trees such as `ui.layout.yoga`,
`ui.backend.imgui`, `gfx.backend.bgfx`, `script.quickjs`, or the old audio/wasm
vendor lanes are archived history now, not supported runtime surface.

`io.net` now points at the owned `kain_native_net_*` function table on the
native core lane instead of the older vendor/libuv stub table. It currently
provides TCP and HTTP/1.1 primitives, with Windows HTTPS client support through
WinHTTP and higher protocols intentionally left to future adapters.

`io.process` now points at the owned `kain_native_process_*` function table on
the native core lane instead of the older vendor/libuv stub table. On Windows
that means the service reports a real child-process and PTY substrate; on
unsupported hosts the ABI still exists, but the status stays degraded and the
functions return explicit unsupported diagnostics.

## Service Families

### Base

- `base.memory`
- `memory.ownership`
- `base.diagnostics`

### Contract And Reflection

- `contract`
- `reflection`
- `runtime.inspection`
- `device.reflection`

### Actor And Async

- `actor.runtime`
- `actor.registry`
- `async.runtime`
- `async.timers`
- `io.net`
- `io.process`

### Platform

- `platform.app-host`
- `platform.input`
- `gfx.viewport`

### Graphics / Scene

- `gfx.raw-native`
- `gfx.shader.spirv`
- `gfx.backend.vulkan`
- `gfx.backend.d3d12`
- `gfx.compute`
- `scene.runtime`
- `scene.query`
- `scene.mutation`

### UI

- `ui.bundle`
- `ui.component`

### Assets

- `asset.gltf`
- `asset.ingestion`
- `asset.realtime`

### Host Bridge / Compatibility

- `host.bridge`
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
service table is the direction of travel, and archived vendor service keys
should stay archived unless they return as blade/package-owned capabilities
above the runtime ABI floor.
