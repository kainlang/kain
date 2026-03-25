# ABI and Startup Validation Tests

**Spec:** `.kiro/specs/kain-native-runtime-completion`  
**Task:** 1.6 Add ABI and startup validation tests  
**Phase:** Phase 1 - Canonical ABI, Service Tables, and Version Metadata  
**Requirements:** 1.5, 2.2, 2.5, 13.1

---

## Purpose

This test suite validates the runtime's ABI versioning, service registry, and startup validation mechanisms. It ensures that:

1. Runtime version information is correctly exposed and accessible
2. Service registry resolution works correctly for required and optional services
3. Startup mismatches are detected and reported with structured diagnostics
4. Required vs optional service reporting is accurate

These tests are critical for ensuring that the native runtime can validate compatibility, detect missing services, and provide clear diagnostics during startup.

---

## Test Coverage

### Test 1: Runtime Version Exposure
**Requirement:** 1.5, 2.2

Validates that runtime version information is correctly exposed through the API:
- ABI version (major.minor.patch)
- Runtime version (major.minor.patch)
- Build information (date/time)
- Formatted version strings

**Expected Behavior:** All version fields are populated and match the defined constants.

---

### Test 2: Service Registry Resolution
**Requirement:** 2.5

Validates that services can be registered and resolved:
- Service registration (required and optional)
- Service lookup by key
- Availability checking
- Non-existent service handling

**Expected Behavior:** Services are correctly registered and can be looked up by key.

---

### Test 3: Required Service Validation
**Requirement:** 2.2, 2.5

Validates that missing required services are detected:
- Required available services pass validation
- Required unavailable services fail validation
- Optional unavailable services do not fail validation
- Diagnostics are generated for failures

**Expected Behavior:** Validation fails when required services are unavailable, with proper diagnostics.

---

### Test 4: Optional Service Reporting
**Requirement:** 13.1

Validates that optional services are correctly reported:
- Count services by requirement level (required vs optional)
- Count services by status (available vs unavailable)
- Distinguish between required and optional failures

**Expected Behavior:** Service counts are accurate for both requirement levels and statuses.

---

### Test 5: ABI Version Compatibility Checking
**Requirement:** 1.5

Validates that ABI compatibility is correctly checked:
- Same version is compatible
- Lower minor version is compatible (backward compatible)
- Different major version is incompatible
- Higher minor version is incompatible (forward incompatible)

**Expected Behavior:** Compatibility rules follow semantic versioning principles.

---

### Test 6: Startup Mismatch Detection
**Requirement:** 2.2, 2.5

Validates that startup mismatches are detected and reported:
- Missing required services generate diagnostics
- Diagnostics contain proper subsystem, severity, and error codes
- Multiple failures are aggregated correctly

**Expected Behavior:** Startup validation detects and reports all mismatches with structured diagnostics.

---

### Test 7: Global Service Registry Integration
**Requirement:** 13.1

Validates that the global registry is properly initialized:
- Global registry singleton is accessible
- Native services are registered at startup
- Expected platform services are available

**Expected Behavior:** Global registry contains all native runtime services.

---

### Test 8: Diagnostic Formatting
**Requirement:** 2.2

Validates that diagnostics are properly formatted:
- Diagnostic creation with all fields
- Formatting to human-readable strings
- Inclusion of subsystem, severity, code, message, detail, and source

**Expected Behavior:** Diagnostics are formatted with all relevant information.

---

## Running the Tests

### Compile and Run

```bash
cd runtime/conformance/03_abi_startup_validation
./compile_test.sh
```

The script will:
1. Compile all required runtime sources
2. Compile the test program
3. Link the test executable
4. Run the test and report results

### Expected Output

```
=== KAIN Runtime ABI and Startup Validation Test ===
Task 1.6: Add ABI and startup validation tests
Requirements: 1.5, 2.2, 2.5, 13.1

Test 1: Runtime Version Exposure
  Runtime Version: 0.1.0
  ABI Version: 0.1.0
  Build Info: Built Mar 18 2026 03:27:47
  ✅ PASS: Runtime version correctly exposed

[... 7 more tests ...]

=== Test Results: 8/8 Passed ===
✅ All tests passed!
```

---

## Test Files

- `test_abi_startup_validation.c` - Main test program
- `compile_test.sh` - Compilation and execution script
- `README.md` - This file

---

## Dependencies

The test requires the following runtime sources:
- `runtime/native/src/core/kain_runtime_version.c`
- `runtime/native/src/core/kain_runtime_diagnostics.c`
- `runtime/native/src/core/kain_runtime_services.c`
- `runtime/native/src/core/kain_runtime_contract.c`
- `runtime/native/src/platform/win32/kain_runtime_win32_shared.c`

And the following headers:
- `runtime/native/include/kain_runtime_version.h`
- `runtime/native/include/kain_runtime_diagnostics.h`
- `runtime/native/include/kain_runtime_services.h`
- `runtime/native/include/kain_runtime_contract.h`

---

## Integration with Phase 1

This test suite completes Phase 1 (Canonical ABI, Service Tables, and Version Metadata) by validating:

- **Task 1.1:** Runtime ABI versioning is exposed and accessible
- **Task 1.2:** Service table headers are functional
- **Task 1.3:** Service registry works correctly
- **Task 1.4:** Runtime metadata is available
- **Task 1.5:** CLI/driver integration preserves version metadata
- **Task 1.6:** ABI and startup validation (this test)

---

## Future Extensions

As the native runtime evolves, this test suite should be extended to cover:

- **Phase 2:** Structured diagnostics for all subsystems
- **Phase 3:** Reflection payload validation
- **Phase 4:** Low-level memory helper validation
- **Phase 5+:** Actor, async, UI, graphics runtime validation

---

## Notes

- All tests must pass for Phase 1 to be considered complete
- Tests are designed to be deterministic and reproducible
- Diagnostics are validated for proper structure and content
- Service registry behavior is validated for both success and failure paths
- ABI compatibility rules follow semantic versioning principles

---

## Related Documentation

- **Spec Requirements:** `.kiro/specs/kain-native-runtime-completion/requirements.md`
- **Spec Design:** `.kiro/specs/kain-native-runtime-completion/design.md`
- **Spec Tasks:** `.kiro/specs/kain-native-runtime-completion/tasks.md`
- **Conformance README:** `runtime/conformance/README.md`
- **Version Info Test:** `runtime/conformance/01_abi_version/test_version_info.c`
- **Service Registry Test:** `runtime/conformance/02_service_registry/test_service_registry.c`
