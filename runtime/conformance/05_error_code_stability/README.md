# Error Code Stability and Primitive Error Path Tests

This test suite validates Phase 2 Task 2.2, 2.4, and 2.5 of the KAIN Native Runtime Completion spec:

- **Task 2.2**: Replace primitive error paths with structured diagnostics
- **Task 2.4**: Define stable native runtime error codes
- **Task 2.5**: Add diagnostics conformance tests

## Test Coverage

### test_error_codes.c

Validates error code stability and range allocation:

1. **Error Code Bases**: Verifies that each subsystem has the correct base code (1000, 2000, etc.)
2. **Error Code Ranges**: Ensures all error codes fall within their designated 1000-code ranges
3. **Error Code Stability**: Validates that specific error codes have stable numeric values
4. **Diagnostic Integration**: Tests that diagnostics correctly preserve error codes

### test_primitive_error_paths.c

Validates that primitive error paths have been replaced with structured diagnostics:

1. **Allocation Diagnostics**: Tests that allocation failures emit diagnostics
2. **File Operation Diagnostics**: Validates file I/O error reporting
3. **Socket Operation Diagnostics**: Validates network error reporting
4. **Data Structure Diagnostics**: Tests array/map/queue allocation
5. **Diagnostic Formatting**: Ensures error codes appear in formatted output

## Running Tests

```bash
# Run all error code tests
./compile_test.sh

# Or compile and run individually
gcc -o test_error_codes test_error_codes.c \
    ../../src/core/kain_runtime_diagnostics.c \
    ../../src/core/kain_runtime_version.c \
    -I../../include -std=c99 -Wall -Wextra

./test_error_codes
```

## Expected Output

All tests should pass with output like:

```
=== KAIN Native Runtime Error Code Stability Tests ===

PASS: Error code bases are correctly defined
PASS: All error codes are within their designated ranges
PASS: Error codes have stable values
PASS: Diagnostics correctly preserve error codes

=== All tests passed ===
```

## Error Code Documentation

See `runtime/native/NATIVE_RUNTIME_ERROR_CODES.md` for complete error code documentation including:

- Error code families and ranges
- Specific error code definitions
- Usage guidelines
- Stability guarantees

## Integration with Phase 2

These tests ensure that:

1. Primitive error paths (printf + exit, null returns) have been replaced with structured diagnostics
2. Error codes are stable and well-documented
3. Diagnostics provide machine-readable error identification
4. The diagnostic system is ready for use by all runtime subsystems

## Future Work

Additional tests to add:

- Contract validation diagnostic tests (Task 2.3)
- Startup validation report tests
- Diagnostic collector stress tests
- Cross-platform diagnostic behavior tests
