# KAIN Native Runtime Service Table Mapping

## Overview

This document describes how existing runtime services map to the new canonical service table headers introduced in Phase 1, Task 1.2.

## New Canonical Headers

The following headers define the canonical runtime service table ABI:

1. **kain_runtime_diagnostics.h** - Structured diagnostics and error reporting
2. **kain_runtime_services.h** - Service registry and capability discovery
3. **kain_runtime_actor.h** - Actor runtime ABI (spawn, mailbox, supervision)
4. **kain_runtime_async.h** - Async/task/timer runtime ABI
5. **kain_runtime_reflection.h** - Reflection payload loading and type metadata
6. **kain_runtime_compatibility.h** - Hot reload, versioning, and migration

## Existing Service Mapping

### Current Services (from kain_runtime_contract.h)

| Old Service Key | Old Mask | Core/Optional | New Service Key | New Header |
|----------------|----------|---------------|-----------------|------------|
| `native.app-host` | `KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST` | Core | `platform.app-host` | kain_runtime_services.h |
| `native.input` | `KAIN_RUNTIME_SERVICE_NATIVE_INPUT` | Core | `platform.input` | kain_runtime_services.h |
| `native.viewport` | `KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT` | Core | `gfx.viewport` | kain_runtime_services.h |
| `native.asset.gltf` | `KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF` | Optional | `asset.gltf` | kain_runtime_services.h |
| `native.ui.compiled-bundle` | `KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED` | Optional | `ui.bundle` | kain_runtime_services.h |

### New Service Families

The canonical service table introduces additional service families for future runtime features:

#### Base Services
- `base.memory` - Memory allocation and lifetime management
- `base.diagnostics` - Structured diagnostics and error reporting

#### Contract and Reflection
- `contract` - Runtime contract loading and validation
- `reflection` - Reflection payload loading and type metadata

#### Actor Runtime
- `actor.runtime` - Actor spawn, mailbox, lifecycle
- `actor.registry` - Named actor registry and lookup

#### Async Runtime
- `async.runtime` - Task/future execution and wake/poll
- `async.timers` - Timer registration and wake integration

#### Platform Services
- `platform.app-host` - Application host (Win32, Linux, macOS)
- `platform.input` - Input capture and event routing
- `platform.window` - Window management

#### Graphics Services
- `gfx.viewport` - Viewport rendering and scene management
- `gfx.shader` - Shader artifact loading and execution
- `gfx.material` - Material runtime and resource binding
- `gfx.compute` - Compute pipeline and dispatch

#### UI Services
- `ui.bundle` - UI bundle loading and validation
- `ui.component` - Component runtime and state management

#### Asset Services
- `asset.gltf` - glTF asset loading
- `asset.realtime` - Realtime bundle loading

#### Host Bridge
- `host.bridge` - Plugin and foreign service integration

#### Compatibility
- `compatibility` - Hot reload, versioning, migration

## Implementation Status

### Phase 1, Task 1.2 (Current)
- ✅ Created canonical service table headers (declarations only)
- ✅ Defined service registry data structures
- ✅ Mapped existing services to new model
- ⏳ Implementation of service registry (Task 1.3)
- ⏳ Integration with startup validation (Task 1.6)

### Backward Compatibility

The existing service mask constants in `kain_runtime_contract.h` will be preserved during the transition:

```c
// Legacy service masks (preserved for compatibility)
#define KAIN_RUNTIME_SERVICE_NATIVE_APP_HOST        (1u << 0)
#define KAIN_RUNTIME_SERVICE_NATIVE_INPUT           (1u << 1)
#define KAIN_RUNTIME_SERVICE_NATIVE_VIEWPORT        (1u << 2)
#define KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF      (1u << 3)
#define KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED     (1u << 4)
```

These will be mapped to the new service registry during Task 1.3 implementation.

## Service Provider Lanes

Services are provided by different runtime lanes:

- **NATIVE_CORE** - Core runtime services (memory, diagnostics, contract, reflection)
- **PLATFORM_WIN32** - Win32-specific platform services (current implementation)
- **PLATFORM_LINUX** - Linux platform services (future)
- **PLATFORM_MACOS** - macOS platform services (future)
- **HOST_RUST** - Rust host-provided services
- **HOST_PYTHON** - Python bridge services
- **HOST_NODE** - Node.js bridge services
- **EXTERNAL** - External plugin/module services

## Next Steps

1. **Task 1.3** - Implement service registry with table-driven capability resolution
2. **Task 1.4** - Extend `native_runtime.toml` with service metadata
3. **Task 1.5** - Update CLI/driver to preserve runtime version metadata
4. **Task 1.6** - Add ABI and startup validation tests

## Design Principles

1. **Centralized Declarations** - All service ABIs declared in `runtime/native/include`
2. **Data-Driven Registry** - Services registered through tables, not scattered checks
3. **Explicit Capabilities** - Required vs optional services clearly marked
4. **Version Awareness** - All services carry ABI version information
5. **Diagnostic-First** - Failures produce structured diagnostics, not null/print-only
