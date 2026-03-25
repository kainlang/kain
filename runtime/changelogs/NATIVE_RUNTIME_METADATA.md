# KAIN Native Runtime Metadata

## Overview

The KAIN native runtime uses two complementary metadata files to declare its ABI, version, services, and implementation details:

1. **`native_runtime.toml`** - Primary manifest for build system integration
2. **`native_runtime_metadata.json`** - Machine-readable metadata for tooling and validation

These files provide explicit, transparent, and machine-checkable declarations of the runtime's capabilities, making it easier to:

- Validate ABI compatibility between compiler and runtime
- Discover available services programmatically
- Track implementation status of planned features
- Generate documentation and validation reports
- Integrate with build systems and tooling

## File Formats

### native_runtime.toml

TOML-based manifest consumed by the build system. Contains:

- **Version metadata**: Runtime and ABI version numbers
- **Runtime metadata**: Description, platforms, compatibility class
- **Sources**: List of C source files organized by category
- **Services**: Declared services with keys, providers, requirements, and status
- **Link dependencies**: Platform-specific linker flags

**Key sections:**

```toml
[version]
runtime_major = 0
runtime_minor = 1
runtime_patch = 0
abi_major = 0
abi_minor = 1
abi_patch = 0

[metadata]
description = "KAIN native C runtime for compiled programs"
target_platforms = ["windows", "linux", "macos"]
active_platforms = ["windows"]
runtime_lane = "raw-native"
compatibility_class = "experimental"

[[services]]
key = "base.memory"
name = "Base Memory Services"
provider = "native-core"
requirement = "required"
status = "available"
description = "Core allocation, retain/release, and memory management"
```

### native_runtime_metadata.json

JSON-based metadata for tooling and validation. Contains:

- **Version information**: Structured runtime and ABI versions
- **Source organization**: Sources grouped by subsystem (core, asset, graphics, platform, UI)
- **Service declarations**: Detailed service metadata including implementation files and headers
- **Link dependencies**: Platform-specific linker requirements
- **Validation metadata**: Schema version, validation commands

**Key structure:**

```json
{
  "runtime_name": "kain-native-runtime",
  "runtime_lane": "raw-native",
  "version": {
    "runtime": { "major": 0, "minor": 1, "patch": 0, "string": "0.1.0" },
    "abi": { "major": 0, "minor": 1, "patch": 0, "string": "0.1.0", "encoded": 256 }
  },
  "services": [
    {
      "key": "base.memory",
      "name": "Base Memory Services",
      "provider": "native-core",
      "requirement": "required",
      "status": "available",
      "abi_version": "0.1.0",
      "description": "Core allocation, retain/release, and memory management",
      "implementation_files": ["native/src/core/kain_runtime_core.c"],
      "header_files": ["native/include/kain_runtime_base.h"]
    }
  ]
}
```

## Service Declaration Model

Services are the fundamental unit of runtime capability. Each service has:

- **key**: Stable identifier (e.g., `base.memory`, `actor.runtime`)
- **name**: Human-readable name
- **provider**: Implementation provider (`native-core`, `platform-win32`, etc.)
- **requirement**: `required` or `optional`
- **status**: `available`, `planned`, `degraded`, `unavailable`
- **abi_version**: ABI version the service targets
- **description**: Brief description of the service
- **platforms**: (optional) Platform restrictions
- **implementation_files**: Source files implementing the service
- **header_files**: Headers declaring the service API

### Service Keys

Service keys follow a hierarchical naming convention:

- `base.*` - Base runtime services (memory, diagnostics)
- `contract` - Runtime contract loading
- `reflection` - Reflection and metadata
- `actor.*` - Actor runtime services
- `async.*` - Async/task/timer services
- `platform.*` - Platform-specific services (app host, input, window)
- `gfx.*` - Graphics services (viewport, shader, material, compute)
- `ui.*` - UI runtime services
- `asset.*` - Asset loading services
- `host.*` - Host bridge and plugin services
- `compatibility` - Hot reload and versioning

### Service Status Values

- **`available`**: Implemented and functional
- **`planned`**: Declared but not yet implemented
- **`degraded`**: Implemented but with known limitations
- **`unavailable`**: Not available on current platform/configuration

### Service Requirement Levels

- **`required`**: Must be available for runtime to function
- **`optional`**: Can be absent; runtime adapts gracefully

## Version Scheme

### Runtime Version

Tracks the overall runtime implementation version. May evolve independently of ABI version.

- **MAJOR**: Breaking changes to runtime behavior or architecture
- **MINOR**: Backward-compatible feature additions
- **PATCH**: Bug fixes and non-breaking changes

### ABI Version

Tracks the binary interface contract between compiler and runtime.

- **MAJOR**: Breaking ABI changes (incompatible)
- **MINOR**: Backward-compatible ABI additions
- **PATCH**: Bug fixes that don't affect ABI

**Compatibility rule**: Runtime ABI is compatible with required ABI if:
- Same major version
- Runtime minor >= required minor

**Encoded format**: `(major << 16) | (minor << 8) | patch`

Example: ABI 0.1.0 encodes to `0x000100` (256 decimal)

## Compatibility Classes

- **`experimental`**: Early development, breaking changes expected
- **`stable`**: API/ABI stable, suitable for development
- **`production`**: Production-ready, strict compatibility guarantees

## Platform Support

### Target Platforms

Platforms the runtime is designed to support:
- `windows`
- `linux`
- `macos`

### Active Platforms

Platforms currently implemented and tested:
- `windows` (Win32 implementation)

Platform-specific services declare their platform restrictions:

```toml
[[services]]
key = "platform.app-host"
platforms = ["windows"]
```

## Usage

### Build System Integration

The build system reads `native_runtime.toml` to:
- Compile the correct source files
- Link platform-specific dependencies
- Validate ABI version compatibility
- Generate runtime metadata for bundles

### Tooling and Validation

Tools can parse `native_runtime_metadata.json` to:
- Generate service documentation
- Validate service implementations exist
- Check ABI compatibility
- Generate capability reports
- Verify header/implementation consistency

### Runtime Initialization

The runtime uses service declarations to:
- Register services in the service registry
- Validate required services are available
- Report missing/degraded services
- Provide capability discovery APIs

## Maintenance Guidelines

### When to Update Metadata

Update metadata files when:

1. **Adding a new service**: Add service declaration with `status = "planned"`, then update to `"available"` when implemented
2. **Implementing a planned service**: Update status, add implementation files and headers
3. **Changing ABI**: Increment ABI version according to compatibility rules
4. **Adding platform support**: Add platform to `active_platforms`, update service platform restrictions
5. **Adding source files**: Add to appropriate source category

### Keeping Files in Sync

The two metadata files must stay synchronized:

- Version numbers must match
- Service declarations must match (keys, names, providers, requirements)
- Source lists must match
- Link dependencies must match

**Validation**: Run `cargo test --package kain-driver` to validate metadata consistency.

### Version Bump Checklist

When bumping versions:

1. Update version numbers in both files
2. Update `KAIN_RUNTIME_*_VERSION_*` constants in `runtime/native/include/kain_runtime_version.h`
3. Update `last_updated` in JSON metadata
4. Document breaking changes if ABI major version changes
5. Run validation tests

## Examples

### Adding a New Service

1. Add to `native_runtime.toml`:

```toml
[[services]]
key = "async.runtime"
name = "Async Runtime"
provider = "native-core"
requirement = "optional"
status = "planned"
description = "Task/future execution, wake/poll, and cancellation"
```

2. Add to `native_runtime_metadata.json`:

```json
{
  "key": "async.runtime",
  "name": "Async Runtime",
  "provider": "native-core",
  "requirement": "optional",
  "status": "planned",
  "abi_version": "0.1.0",
  "description": "Task/future execution, wake/poll, and cancellation",
  "implementation_files": [],
  "header_files": []
}
```

3. When implementing, update status to `"available"` and add files:

```json
{
  "status": "available",
  "implementation_files": ["native/src/core/kain_runtime_async.c"],
  "header_files": ["native/include/kain_runtime_async.h"]
}
```

### Checking Service Availability

Runtime code can check service availability:

```c
#include "kain_runtime_services.h"

KainServiceRegistry* registry = kain_service_registry_global();

if (kain_service_registry_is_available(registry, "actor.runtime")) {
    // Actor runtime is available
} else {
    // Actor runtime not available, use fallback or fail
}
```

### Validating ABI Compatibility

```c
#include "kain_runtime_version.h"

unsigned int required_abi = KAIN_RUNTIME_ABI_VERSION_ENCODE(0, 1, 0);

if (kain_runtime_version_check_abi_compatibility(required_abi)) {
    // Compatible
} else {
    // Incompatible - fail with diagnostic
}
```

## Related Files

- `runtime/native/include/kain_runtime_version.h` - ABI version constants
- `runtime/native/include/kain_runtime_services.h` - Service registry API
- `runtime/native/src/core/kain_runtime_version.c` - Version implementation
- `runtime/native/src/core/kain_runtime_services.c` - Service registry implementation
- `runtime/native/src/core/kain_runtime_contract.c` - Service registration
- `runtime/NATIVE_RUNTIME_COMPLETION_TRACKER.md` - Implementation progress tracking

## Future Enhancements

Planned improvements to the metadata system:

1. **Schema validation**: JSON schema for metadata validation
2. **Automated sync checking**: Tool to verify TOML/JSON consistency
3. **Service dependency tracking**: Declare service dependencies
4. **Capability negotiation**: Runtime capability negotiation protocol
5. **Multi-platform builds**: Conditional compilation based on metadata
6. **Documentation generation**: Auto-generate docs from metadata
