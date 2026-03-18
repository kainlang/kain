# KAIN Native Runtime Error Codes

This document defines the stable error code families for the KAIN native runtime. These codes are used in structured diagnostics to provide machine-readable error identification across all runtime subsystems.

## Error Code Scheme

Error codes are organized into families by subsystem, with each family occupying a 1000-code range:

- **0-999**: Common/generic codes
- **1000-1999**: Contract subsystem
- **2000-2999**: Reflection subsystem
- **3000-3999**: Actor subsystem
- **4000-4999**: Async subsystem
- **5000-5999**: UI subsystem
- **6000-6999**: Graphics subsystem
- **7000-7999**: Platform subsystem
- **8000-8999**: Host bridge subsystem
- **9000-9999**: Memory subsystem
- **10000-10999**: Compatibility subsystem

## Common Error Codes (0-999)

| Code | Name | Description |
|------|------|-------------|
| 0 | `KAIN_DIAG_CODE_SUCCESS` | Operation succeeded (not an error) |
| 1 | `KAIN_DIAG_CODE_GENERIC_ERROR` | Generic unspecified error |

## Contract Error Codes (1000-1999)

Base: `KAIN_DIAG_CODE_CONTRACT_BASE` (1000)

| Code | Name | Description |
|------|------|-------------|
| 1001 | `KAIN_DIAG_CODE_CONTRACT_NOT_FOUND` | Runtime contract file not found |
| 1002 | `KAIN_DIAG_CODE_CONTRACT_PARSE_FAILED` | Failed to parse runtime contract JSON |
| 1003 | `KAIN_DIAG_CODE_CONTRACT_INVALID_SCHEMA` | Contract schema validation failed |
| 1004 | `KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE` | Required service missing from contract |
| 1005 | `KAIN_DIAG_CODE_CONTRACT_ABI_MISMATCH` | Contract ABI version incompatible with runtime |

**Usage Context**: Emitted during startup validation when loading and validating runtime contract bundles.

## Reflection Error Codes (2000-2999)

Base: `KAIN_DIAG_CODE_REFLECTION_BASE` (2000)

| Code | Name | Description |
|------|------|-------------|
| 2001 | `KAIN_DIAG_CODE_REFLECTION_NOT_FOUND` | Reflection payload not found |
| 2002 | `KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED` | Failed to parse reflection payload |
| 2003 | `KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA` | Reflection schema validation failed |
| 2004 | `KAIN_DIAG_CODE_REFLECTION_LOOKUP_FAILED` | Failed to lookup reflected type/item |

**Usage Context**: Emitted when loading reflection metadata or performing runtime type lookups.

## Actor Error Codes (3000-3999)

Base: `KAIN_DIAG_CODE_ACTOR_BASE` (3000)

| Code | Name | Description |
|------|------|-------------|
| 3001 | `KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED` | Actor spawn/thread creation failed |
| 3002 | `KAIN_DIAG_CODE_ACTOR_MAILBOX_FULL` | Actor mailbox capacity exceeded |
| 3003 | `KAIN_DIAG_CODE_ACTOR_INVALID_MESSAGE` | Invalid message type or format |
| 3004 | `KAIN_DIAG_CODE_ACTOR_NOT_FOUND` | Actor not found in registry |
| 3005 | `KAIN_DIAG_CODE_ACTOR_SHUTDOWN_FAILED` | Actor shutdown/cleanup failed |

**Usage Context**: Emitted during actor lifecycle operations (spawn, message send, registry lookup, shutdown).

## Async Error Codes (4000-4999)

Base: `KAIN_DIAG_CODE_ASYNC_BASE` (4000)

| Code | Name | Description |
|------|------|-------------|
| 4001 | `KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED` | Async task spawn failed |
| 4002 | `KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED` | Async task was cancelled |
| 4003 | `KAIN_DIAG_CODE_ASYNC_TIMER_FAILED` | Timer registration/operation failed |
| 4004 | `KAIN_DIAG_CODE_ASYNC_WAKE_FAILED` | Task wake operation failed |

**Usage Context**: Emitted during async task lifecycle and timer operations.

## UI Error Codes (5000-5999)

Base: `KAIN_DIAG_CODE_UI_BASE` (5000)

| Code | Name | Description |
|------|------|-------------|
| 5001 | `KAIN_DIAG_CODE_UI_BUNDLE_NOT_FOUND` | UI bundle file not found |
| 5002 | `KAIN_DIAG_CODE_UI_BUNDLE_PARSE_FAILED` | Failed to parse UI bundle |
| 5003 | `KAIN_DIAG_CODE_UI_BUNDLE_INVALID_SCHEMA` | UI bundle schema validation failed |
| 5004 | `KAIN_DIAG_CODE_UI_COMPONENT_INIT_FAILED` | Component initialization failed |

**Usage Context**: Emitted during UI bundle loading and component lifecycle operations.

## Graphics Error Codes (6000-6999)

Base: `KAIN_DIAG_CODE_GFX_BASE` (6000)

| Code | Name | Description |
|------|------|-------------|
| 6001 | `KAIN_DIAG_CODE_GFX_SHADER_LOAD_FAILED` | Shader artifact loading failed |
| 6002 | `KAIN_DIAG_CODE_GFX_MATERIAL_LOAD_FAILED` | Material artifact loading failed |
| 6003 | `KAIN_DIAG_CODE_GFX_COMPUTE_DISPATCH_FAILED` | Compute dispatch operation failed |
| 6004 | `KAIN_DIAG_CODE_GFX_BINDING_FAILED` | Resource binding failed |

**Usage Context**: Emitted during graphics/shader/material runtime operations.

## Platform Error Codes (7000-7999)

Base: `KAIN_DIAG_CODE_PLATFORM_BASE` (7000)

| Code | Name | Description |
|------|------|-------------|
| 7001 | `KAIN_DIAG_CODE_PLATFORM_UNSUPPORTED` | Platform not supported |
| 7002 | `KAIN_DIAG_CODE_PLATFORM_INIT_FAILED` | Platform initialization failed |
| 7003 | `KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE` | Platform service unavailable (file I/O, sockets, etc.) |

**Usage Context**: Emitted during platform-specific operations (file I/O, networking, window management).

## Host Bridge Error Codes (8000-8999)

Base: `KAIN_DIAG_CODE_HOST_BRIDGE_BASE` (8000)

| Code | Name | Description |
|------|------|-------------|
| 8001 | `KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED` | Host bridge/plugin loading failed |
| 8002 | `KAIN_DIAG_CODE_HOST_BRIDGE_ABI_MISMATCH` | Host bridge ABI version mismatch |
| 8003 | `KAIN_DIAG_CODE_HOST_BRIDGE_SERVICE_MISSING` | Required host service missing |

**Usage Context**: Emitted during host/plugin bridge operations and foreign service integration.

## Memory Error Codes (9000-9999)

Base: `KAIN_DIAG_CODE_MEMORY_BASE` (9000)

| Code | Name | Description |
|------|------|-------------|
| 9001 | `KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED` | Memory allocation failed |
| 9002 | `KAIN_DIAG_CODE_MEMORY_INVALID_POINTER` | Invalid pointer/index access |
| 9003 | `KAIN_DIAG_CODE_MEMORY_ALIGNMENT_ERROR` | Memory alignment requirement violated |

**Usage Context**: Emitted during memory allocation, array/map operations, and low-level memory helpers.

## Compatibility Error Codes (10000-10999)

Base: `KAIN_DIAG_CODE_COMPATIBILITY_BASE` (10000)

| Code | Name | Description |
|------|------|-------------|
| 10001 | `KAIN_DIAG_CODE_COMPAT_VERSION_MISMATCH` | Version compatibility check failed |
| 10002 | `KAIN_DIAG_CODE_COMPAT_MIGRATION_FAILED` | Hot reload migration failed |
| 10003 | `KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE` | Update incompatible with current state |

**Usage Context**: Emitted during hot reload, version checking, and compatibility validation.

## Stability Guarantees

1. **Code Stability**: Error codes in this document are stable and will not be reused for different errors
2. **Range Reservation**: Each subsystem's 1000-code range is reserved for future expansion
3. **Backward Compatibility**: New codes may be added within ranges, but existing codes will not change meaning
4. **Deprecation Policy**: If a code must be retired, it will be marked deprecated for at least one major version

## Usage Guidelines

### For Runtime Implementers

1. Always use the defined constants (e.g., `KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED`) rather than numeric literals
2. Include detailed context in the diagnostic `detail` field
3. Use appropriate severity levels (INFO, WARNING, ERROR, FATAL)
4. Emit diagnostics before returning error values

### For Diagnostic Consumers

1. Check error codes for programmatic error handling
2. Use subsystem information for routing/filtering
3. Display both code and message for user-facing errors
4. Log full diagnostic details for debugging

## Example Usage

```c
/* Emitting a diagnostic */
KainDiagnostic diag;
kain_diagnostic_create(
    &diag,
    KAIN_DIAG_SUBSYSTEM_ACTOR,
    KAIN_DIAG_SEVERITY_ERROR,
    KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED,
    "Actor spawn failed",
    "Failed to allocate thread arguments structure",
    NULL
);
kain_diagnostic_print(&diag);

/* Checking error codes */
if (diag.code == KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED) {
    /* Handle spawn failure specifically */
}
```

## Future Expansion

Reserved ranges for future subsystems:

- **11000-11999**: Reserved for shader/compute subsystem expansion
- **12000-12999**: Reserved for networking subsystem
- **13000-13999**: Reserved for serialization subsystem
- **14000-14999**: Reserved for debugging/profiling subsystem

## Version History

- **v0.1.0** (2024): Initial error code family definition
  - Defined 11 subsystem families
  - Established 1000-code range per subsystem
  - Documented 40+ specific error codes
