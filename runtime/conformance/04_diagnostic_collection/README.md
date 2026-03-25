# Diagnostic Collection Conformance Tests

This directory contains conformance tests for the KAIN native runtime diagnostic collection and reporting APIs.

## Purpose

These tests validate the diagnostic collector and startup validation result structures introduced in Phase 2 of the native runtime completion work. The diagnostic collection APIs enable:

- Aggregating multiple diagnostics during startup and runtime operations
- Batch reporting of collected diagnostics
- Structured startup validation results with version information
- Severity-based filtering and counting

## Test Coverage

- `test_diagnostic_collection.c` - Core diagnostic collection tests
  - Collector initialization and clearing
  - Adding diagnostics (individual and batch)
  - Error and fatal detection
  - Severity counting
  - Startup validation result formatting

## Building and Running

### Compile the test:
```bash
gcc -o test_diagnostic_collection \
    runtime/conformance/04_diagnostic_collection/test_diagnostic_collection.c \
    runtime/native/src/core/kain_runtime_diagnostics.c \
    runtime/native/src/core/kain_runtime_version.c \
    -I runtime/native/include
```

### Run the test:
```bash
./test_diagnostic_collection
```

## Expected Output

All tests should pass with output showing:
- Collector initialization
- Diagnostic addition and counting
- Error/fatal detection
- Startup validation result formatting

## Requirements Validated

- **Requirement 2.1**: Structured diagnostics with subsystem, code, severity, summary, detail, and source path
- **Requirement 2.2**: APIs for collecting and reporting diagnostics during startup and runtime operations
- **Requirement 2.6**: Diagnostic reporting with explicit downgrade information

## Related Files

- `runtime/native/include/kain_runtime_diagnostics.h` - Diagnostic API declarations
- `runtime/native/src/core/kain_runtime_diagnostics.c` - Diagnostic implementation
- `runtime/native/include/kain_runtime_version.h` - Version information for diagnostics
