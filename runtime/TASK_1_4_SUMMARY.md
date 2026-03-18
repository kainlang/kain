# Task 1.4 Summary: Extend `native_runtime.toml` and Related Runtime Metadata

**Spec:** `.kiro/specs/kain-native-runtime-completion`  
**Task:** 1.4 Extend `native_runtime.toml` and related runtime metadata  
**Date:** 2025-01-01  
**Status:** ✅ COMPLETE

---

## Overview

Task 1.4 extended the native runtime manifest and metadata system to make runtime ABI, version, and service declarations explicit, transparent, and machine-checkable. This work builds on tasks 1.1-1.3 (ABI versioning, service headers, and service registry) by exposing that information in declarative metadata files.

---

## Requirements Addressed

- **Requirement 1.3:** ABI-significant structs documented with layout, version, and ownership rules
- **Requirement 2.4:** Runtime binary embeds version, ABI version, and build identifier
- **Requirement 14.2:** Service/capability additions are table-driven and data-driven

---

## Changes Made

### 1. Extended `runtime/native_runtime.toml`

**Location:** `runtime/native_runtime.toml`

**Changes:**
- Added `[version]` section with runtime and ABI version numbers
- Added `[metadata]` section with description, platforms, compatibility class
- Added `[[services]]` array declaring all runtime services with:
  - Service key (stable identifier)
  - Name and description
  - Provider (native-core, platform-win32, etc.)
  - Requirement level (required/optional)
  - Status (available/planned)
  - Platform restrictions where applicable
- Organized sources with comments explaining their purpose
- Documented link dependencies by platform

**Service Categories Declared:**
- Base services (memory, diagnostics)
- Contract and reflection
- Platform services (app-host, input, viewport)
- Graphics services (viewport, shader, material, compute)
- Asset services (glTF, realtime bundles)
- UI services (bundle, component)
- Actor runtime services
- Async/timer services
- Compatibility and hot reload
- Host bridge

**Status Tracking:**
- 9 services marked as `status = "available"` (currently implemented)
- 11 services marked as `status = "planned"` (future work)

### 2. Created `runtime/native_runtime_metadata.json`

**Location:** `runtime/native_runtime_metadata.json`

**Purpose:** Machine-readable metadata for tooling, validation, and build system integration

**Structure:**
```json
{
  "runtime_name": "kain-native-runtime",
  "runtime_lane": "raw-native",
  "version": {
    "runtime": { "major": 0, "minor": 1, "patch": 0, "string": "0.1.0" },
    "abi": { "major": 0, "minor": 1, "patch": 0, "string": "0.1.0", "encoded": 256 }
  },
  "metadata": { ... },
  "sources": { "core": [...], "asset": [...], "graphics": [...], ... },
  "services": [ ... ],
  "link_dependencies": { ... },
  "validation": { ... }
}
```

**Key Features:**
- Structured version information (runtime and ABI)
- Sources organized by subsystem (core, asset, graphics, platform, UI)
- Detailed service declarations including:
  - Implementation files
  - Header files
  - Platform restrictions
  - ABI version
- Link dependencies by platform
- Validation metadata (schema version, validation commands)

### 3. Created `runtime/NATIVE_RUNTIME_METADATA.md`

**Location:** `runtime/NATIVE_RUNTIME_METADATA.md`

**Purpose:** Documentation explaining the metadata format, usage, and maintenance

**Contents:**
- Overview of the two-file metadata system
- File format specifications (TOML and JSON)
- Service declaration model
- Service key naming conventions
- Service status values and requirement levels
- Version scheme (runtime vs ABI)
- Compatibility classes
- Platform support model
- Usage examples for build systems, tooling, and runtime
- Maintenance guidelines
- Version bump checklist
- Examples of adding new services
- Related files and future enhancements

---

## Metadata System Design

### Two-File Approach

**`native_runtime.toml`** (Primary manifest)
- Human-readable TOML format
- Consumed by build system
- Declares sources, services, link dependencies
- Version metadata

**`native_runtime_metadata.json`** (Machine-readable metadata)
- Structured JSON format
- For tooling and validation
- Detailed service metadata with file mappings
- Source organization by subsystem
- Validation commands

### Service Declaration Model

Each service has:
- **key**: Stable identifier (e.g., `base.memory`, `actor.runtime`)
- **name**: Human-readable name
- **provider**: Implementation provider (native-core, platform-win32, etc.)
- **requirement**: `required` or `optional`
- **status**: `available`, `planned`, `degraded`, `unavailable`
- **abi_version**: ABI version the service targets
- **description**: Brief description
- **platforms**: (optional) Platform restrictions
- **implementation_files**: Source files implementing the service
- **header_files**: Headers declaring the service API

### Service Key Hierarchy

- `base.*` - Base runtime services
- `contract` - Runtime contract loading
- `reflection` - Reflection and metadata
- `actor.*` - Actor runtime services
- `async.*` - Async/task/timer services
- `platform.*` - Platform-specific services
- `gfx.*` - Graphics services
- `ui.*` - UI runtime services
- `asset.*` - Asset loading services
- `host.*` - Host bridge and plugin services
- `compatibility` - Hot reload and versioning

---

## Integration Points

### Build System
- Reads `native_runtime.toml` to compile correct sources
- Links platform-specific dependencies
- Validates ABI version compatibility
- Generates runtime metadata for bundles

### Tooling
- Parses `native_runtime_metadata.json` to:
  - Generate service documentation
  - Validate service implementations exist
  - Check ABI compatibility
  - Generate capability reports
  - Verify header/implementation consistency

### Runtime Initialization
- Uses service declarations to:
  - Register services in service registry
  - Validate required services are available
  - Report missing/degraded services
  - Provide capability discovery APIs

---

## Validation

### Compilation Check
- Native runtime still compiles successfully with updated manifest
- All source files are correctly declared
- Include directories are correct
- Link dependencies are properly specified

### Metadata Consistency
- Version numbers match between TOML and JSON
- Service declarations are consistent
- Source lists match
- Link dependencies match

### Documentation
- Comprehensive documentation explains metadata format
- Usage examples provided
- Maintenance guidelines established
- Version bump checklist documented

---

## Current Service Status

### Available Services (9)
1. `base.memory` - Core allocation, retain/release, memory management
2. `base.diagnostics` - Structured diagnostics and error reporting
3. `contract` - Runtime contract bundle loading and validation
4. `platform.app-host` - Win32 application host and window management
5. `platform.input` - Win32 input capture and event handling
6. `gfx.viewport` - Win32 viewport host and OpenGL rendering
7. `asset.gltf` - glTF 2.0 asset loading and parsing
8. `asset.realtime` - Realtime bundle loading and scene management
9. `ui.bundle` - Compiled UI bundle loading and overlay rendering

### Planned Services (11)
1. `reflection` - Reflection payload loading and runtime type lookup
2. `actor.runtime` - Actor spawn, mailbox, lifecycle, and scheduling
3. `actor.registry` - Named actor registration and lookup
4. `async.runtime` - Task/future execution, wake/poll, and cancellation
5. `async.timers` - Timer registration and wake delivery
6. `gfx.shader` - Shader artifact loading and validation
7. `gfx.material` - Material instance management and resource binding
8. `gfx.compute` - Compute pipeline creation and dispatch
9. `ui.component` - Component state, invalidation, and event routing
10. `compatibility` - Version validation, migration, and hot reload support
11. `host.bridge` - Plugin and foreign service integration

---

## Benefits

### Transparency
- Runtime capabilities are explicitly declared
- Service status is visible (available vs planned)
- Platform support is clear
- Version information is accessible

### Machine-Checkable
- JSON format enables automated validation
- Tooling can verify implementation completeness
- Build systems can validate compatibility
- Documentation can be auto-generated

### Data-Driven
- Services are declared in tables, not scattered code
- Adding new services follows a clear pattern
- Status tracking is centralized
- Version management is explicit

### Maintainability
- Clear documentation of metadata format
- Maintenance guidelines established
- Version bump checklist provided
- Examples of common operations

---

## Future Work

### Immediate Next Steps (Task 1.5)
- Teach CLI/driver about runtime version metadata
- Update bundle output to include metadata
- Ensure version metadata is preserved in artifacts

### Future Enhancements
1. **Schema validation**: JSON schema for metadata validation
2. **Automated sync checking**: Tool to verify TOML/JSON consistency
3. **Service dependency tracking**: Declare service dependencies
4. **Capability negotiation**: Runtime capability negotiation protocol
5. **Multi-platform builds**: Conditional compilation based on metadata
6. **Documentation generation**: Auto-generate docs from metadata

---

## Files Created/Modified

### Created
- `runtime/native_runtime_metadata.json` - Machine-readable metadata
- `runtime/NATIVE_RUNTIME_METADATA.md` - Metadata documentation

### Modified
- `runtime/native_runtime.toml` - Extended with version, metadata, and service declarations
- `runtime/NATIVE_RUNTIME_COMPLETION_TRACKER.md` - Updated task 1.4 status

---

## Related Work

### Dependencies (Completed)
- Task 1.1: ABI versioning constants and API
- Task 1.2: Service table headers
- Task 1.3: Service registry implementation

### Enables
- Task 1.5: CLI/driver integration with version metadata
- Task 1.6: ABI and startup validation tests
- Future phases: Data-driven service expansion

---

## Conclusion

Task 1.4 successfully extended the native runtime manifest and metadata system to make runtime capabilities explicit, transparent, and machine-checkable. The two-file approach (TOML for build system, JSON for tooling) provides both human-readable declarations and structured metadata for automation.

The service declaration model establishes a clear pattern for tracking implementation status (available vs planned) and organizing runtime capabilities by subsystem. This foundation enables data-driven runtime expansion and automated validation in future phases.

**Status:** ✅ COMPLETE - Ready for task 1.5 (CLI/driver integration)
