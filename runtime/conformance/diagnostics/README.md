# Diagnostics Conformance Tests

**Category:** Diagnostics  
**Purpose:** Validate structured diagnostics, error codes, and startup failure reporting

---

## Harness

This directory now contains a real compile-and-run diagnostics harness with hard per-step timeouts.

- `run_tests.sh` compiles the lane and runs each executable with a timeout guard
- `compile_tests.sh` builds the focused diagnostics test binaries
- `_shared/run_with_timeout.py` is used for both compilation and execution time limits

## Current Test Coverage

### Structured Diagnostics
- `test_structured_runtime_diagnostics.c`
- Diagnostic record creation
- Diagnostic formatting
- Collector aggregation
- Severity counters
- Collector clearing

### Error Code Stability
- `test_diagnostic_error_codes.c`
- Stable family bases
- Representative stable codes
- Subsystem and severity name mappings

### Startup Failure Reporting
- `test_startup_failure_reporting.c`
- Required service failure reporting
- Structured fatal diagnostics
- Optional service downgrade reporting
- Startup report formatting

---

## Running Tests

```bash
# Run all diagnostics tests with the default timeouts
./run_tests.sh

# Run with explicit timeouts
./run_tests.sh --compile-timeout 300 --test-timeout 20

# Run in verbose mode
./run_tests.sh --verbose
```

---

## Notes

- The diagnostics lane is intentionally focused on the canonical runtime APIs that already exist today.
- Startup validation tests should verify both the legacy validation result and the structured startup report.
- Keep new diagnostics coverage centered in this directory so the lane stays easy to execute and audit.
