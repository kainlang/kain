# Native Runtime Conformance Tests

**Spec:** `.kiro/specs/kain-native-runtime-completion`  
**Task:** 0.4 Add a conformance test directory for native runtime behavior  
**Created:** 2026-03-17

---

## Purpose

This directory contains runtime-specific harnesses and ABI parity tests for the KAIN native runtime. Unlike the smoke fixtures in `runtime/fixtures/` which validate startup paths, conformance tests validate that runtime behavior matches the canonical ABI contract across different backends and execution modes.

**Critical Rule:** Future phases MUST extend this harness family instead of scattering ad hoc checks across the codebase. All runtime behavior validation should be centralized here.

## Reality Check (March 18, 2026)

- `runtime/conformance/run_all.sh --verbose` currently passes all 10 registered categories on the active Windows lane
- `abi_parity`, `actor_runtime`, `async_runtime`, `ui_runtime`, `graphics_runtime`, `hot_reload`, `host_bridge`, and `platform_parity` are compile-and-run harnesses with timeout-guarded execution
- `reflection/` and `diagnostics/` are still placeholder category runners today; they report status and return success, but they do not yet provide the same end-to-end executable coverage as the other categories
- Treat the green aggregate run as strong progress, not as proof that reflection and diagnostics are fully conformance-complete

---

## Conformance Test Categories

### 1. ABI Parity Tests (`abi_parity/`)

**Purpose:** Validate that runtime helpers behave consistently across interpreter, LLVM, C++, Rust-hosted, and raw-native lanes

**Test Areas:**
- Memory layout and alignment
- Pointer operations (address-of, field pointer, index pointer)
- Load/store operations
- Union and bitfield operations
- Allocation and lifetime helpers
- Type conversion and casting

**Expected Behavior:** All backends produce identical results for the same operations

---

### 2. Actor Runtime Tests (`actor_runtime/`)

**Purpose:** Validate actor semantics, mailbox operations, supervision, and lifecycle

**Test Areas:**
- Actor spawn and bootstrap
- Mailbox send/receive operations
- Bounded mailbox and backpressure
- Actor registry (register/lookup/unregister)
- Monitors and links
- Supervision policies (restart, shutdown, escalation)
- Actor exit and cleanup
- Scheduler fairness

**Expected Behavior:** Actor semantics match the documented runtime contract

---

### 3. Async Runtime Tests (`async_runtime/`)

**Purpose:** Validate async/await, futures, timers, and task scheduling

**Test Areas:**
- Task spawn and completion
- Wake/poll mechanics
- Timer registration and delivery
- Cancellation propagation
- Actor/task interop
- Scheduler integration
- Async value ownership

**Expected Behavior:** Async semantics match the documented runtime contract

---

### 4. Reflection Tests (`reflection/`)

**Purpose:** Validate reflection payload loading, schema validation, and runtime lookup

**Test Areas:**
- Reflection payload parsing
- Schema version validation
- Type/item identity lookup
- Actor/message/component metadata
- Service binding resolution
- Compatibility metadata

**Expected Behavior:** Reflection data is correctly loaded and queryable

---

### 5. Diagnostics Tests (`diagnostics/`)

**Purpose:** Validate structured diagnostics, error codes, and failure reporting

**Test Areas:**
- Subsystem-specific diagnostics
- Stable error code emission
- Startup validation failures
- Contract mismatch reporting
- Service downgrade reporting
- Version compatibility failures

**Expected Behavior:** All failures produce structured diagnostics with stable error codes

---

### 6. UI Runtime Tests (`ui_runtime/`)

**Purpose:** Validate UI bundle interpretation, component lifecycle, and event routing

**Test Areas:**
- Bundle validation and loading
- Component state management
- Invalidation and redraw
- Focus and event routing
- Input event dispatch
- Rust-native vs raw-native parity

**Expected Behavior:** UI runtime behavior matches the compiled bundle contract

---

### 7. Graphics Runtime Tests (`graphics_runtime/`)

**Purpose:** Validate shader/material/compute artifact loading and execution

**Test Areas:**
- Shader artifact loading
- Material instance creation
- Resource binding validation
- Compute pipeline dispatch
- Backend contract compliance
- Hot reload compatibility

**Expected Behavior:** Graphics artifacts load and execute correctly

---

### 8. Hot Reload Tests (`hot_reload/`)

**Purpose:** Validate versioning, compatibility classes, and state migration

**Test Areas:**
- Runtime version validation
- Bundle compatibility checking
- Migration hook execution
- State transfer boundaries
- Compatible update acceptance
- Incompatible update rejection

**Expected Behavior:** Hot reload follows documented compatibility rules

---

### 9. Host Bridge Tests (`host_bridge/`)

**Purpose:** Validate host/plugin bridge registration, ABI validation, and foreign runtime contracts

**Test Areas:**
- Module install and activation
- Capability-aware registration
- Required service validation
- ABI mismatch rejection
- Service registration and discovery
- Module uninstall cleanup
- Python/Node/Rust/C/Zig bridge contract exposure

**Expected Behavior:** Host bridge modules behave like first-class runtime extensions with explicit diagnostics

---

### 10. Platform Parity Tests (`platform_parity/`)

**Purpose:** Validate cross-platform runtime behavior and capability discovery

**Test Areas:**
- Platform service availability
- Capability advertisement
- Unsupported platform diagnostics
- Platform-specific service boundaries
- Cross-platform startup validation

**Expected Behavior:** Platform differences are explicit and well-diagnosed

---

## Test Organization

Each test category follows this structure:

```
runtime/conformance/<category>/
├── README.md              # Category-specific documentation
├── harness.c              # C test harness (if applicable)
├── harness.rs             # Rust test harness (if applicable)
├── test_*.kn              # Kain test programs
├── expected/              # Expected output/behavior
│   ├── interpreter.json   # Expected interpreter results
│   ├── llvm.json          # Expected LLVM results
│   ├── cpp.json           # Expected C++ results
│   └── native.json        # Expected native runtime results
└── run_tests.sh           # Test runner script
```

---

## Writing Conformance Tests

### Test Design Principles

1. **Focused and Minimal** - Each test validates one specific behavior
2. **Cross-Backend** - Tests should run on multiple backends when applicable
3. **Deterministic** - Tests produce consistent, verifiable results
4. **Self-Documenting** - Test names and comments explain what is being validated
5. **Failure-Aware** - Tests validate both success and failure paths

### Test Naming Convention

```
test_<area>_<behavior>_<condition>.kn
```

Examples:
- `test_actor_spawn_basic.kn`
- `test_mailbox_bounded_overflow.kn`
- `test_reflection_schema_version_mismatch.kn`
- `test_pointer_field_offset_alignment.kn`

### Expected Output Format

Expected outputs are JSON files containing:

```json
{
  "test_name": "test_actor_spawn_basic",
  "backend": "llvm",
  "expected_result": "success",
  "expected_output": "Actor spawned with ID: 1\nActor completed successfully\n",
  "expected_diagnostics": [],
  "expected_exit_code": 0
}
```

For failure tests:

```json
{
  "test_name": "test_reflection_schema_version_mismatch",
  "backend": "native",
  "expected_result": "failure",
  "expected_diagnostics": [
    {
      "subsystem": "reflection",
      "code": "KAIN-RT-REFLECT-0001",
      "severity": "error",
      "message": "Schema version mismatch"
    }
  ],
  "expected_exit_code": 1
}
```

---

## Running Conformance Tests

### Run All Tests

```bash
# Run all conformance tests across all backends
./runtime/conformance/run_all.sh

# Run all tests for a specific backend
./runtime/conformance/run_all.sh --backend llvm
```

### Run Category Tests

```bash
# Run all ABI parity tests
./runtime/conformance/abi_parity/run_tests.sh

# Run all actor runtime tests
./runtime/conformance/actor_runtime/run_tests.sh
```

### Run Individual Tests

```bash
# Run a specific test on all backends
./runtime/conformance/run_test.sh test_actor_spawn_basic.kn

# Run a specific test on a specific backend
./runtime/conformance/run_test.sh test_actor_spawn_basic.kn --backend llvm
```

---

## Integration with CI/CD

Conformance tests are designed to be run in CI/CD pipelines:

```bash
# Quick validation (smoke tests only)
./runtime/conformance/run_all.sh --quick

# Full validation (all tests, all backends)
./runtime/conformance/run_all.sh --full

# Regression validation (compare against baseline)
./runtime/conformance/run_all.sh --regression
```

---

## Test Development Workflow

### Adding a New Test

1. **Identify the behavior** - What specific runtime behavior needs validation?
2. **Choose the category** - Which test category does this belong to?
3. **Write the test program** - Create a minimal `.kn` program that exercises the behavior
4. **Define expected outputs** - Create expected output files for each backend
5. **Update category README** - Document the new test
6. **Run and verify** - Ensure the test passes on all applicable backends

### Extending an Existing Test

1. **Review the existing test** - Understand what it currently validates
2. **Add new test cases** - Extend the test program with new scenarios
3. **Update expected outputs** - Adjust expected results for all backends
4. **Document changes** - Update the test comments and category README
5. **Verify backward compatibility** - Ensure existing validations still pass

---

## Relationship to Other Test Suites

### vs. Smoke Fixtures (`runtime/fixtures/`)

- **Fixtures:** Validate startup paths and basic initialization
- **Conformance:** Validate runtime behavior and ABI parity

### vs. Crate Tests (`crates/*/tests/`)

- **Crate Tests:** Validate individual crate functionality in isolation
- **Conformance:** Validate end-to-end runtime behavior across the full stack

### vs. Smoketests (`smoketest/`)

- **Smoketests:** Validate runtime bridge integration and mixed-language orchestration
- **Conformance:** Validate native runtime behavior and ABI contracts

---

## Implemented Tests

### Phase 1: Canonical ABI, Service Tables, and Version Metadata

- **01_abi_version/** - Runtime version information API tests
  - Validates ABI version exposure
  - Validates runtime version exposure
  - Validates build information
  - Validates version formatting and compatibility checking

- **02_service_registry/** - Service registry conformance tests
  - Validates service registration and lookup
  - Validates service availability checking
  - Validates service counting by status and requirement
  - Validates required service validation

- **03_abi_startup_validation/** - ABI and startup validation tests (Task 1.6)
  - Validates runtime version exposure (Requirements 1.5, 2.2)
  - Validates service registry resolution (Requirements 2.5)
  - Validates startup mismatch failures (Requirements 2.2, 2.5)
  - Validates required vs optional service reporting (Requirements 13.1)

---

## Phase-by-Phase Test Expansion

As the native runtime completion work progresses, conformance tests will be added in phases:

- **Phase 0:** Test infrastructure and baseline ✅
- **Phase 1:** ABI parity and service table tests ✅
- **Phase 2:** Diagnostics and error code tests
- **Phase 3:** Reflection payload tests
- **Phase 4:** Low-level memory helper tests
- **Phase 5:** Actor bootstrap and basic runtime tests
- **Phase 6:** Full actor runtime semantics tests
- **Phase 7:** Async/timer runtime tests
- **Phase 8:** UI runtime convergence tests
- **Phase 9:** Graphics runtime tests
- **Phase 10:** Hot reload and compatibility tests
- **Phase 11:** Host/plugin bridge tests
- **Phase 12:** Platform parity tests

---

## Test Harness Architecture

### C Test Harness

The C test harness (`harness.c`) provides:
- Direct native runtime API access
- Low-level memory and ABI validation
- Platform-specific service testing
- Performance benchmarking

### Rust Test Harness

The Rust test harness (`harness.rs`) provides:
- `kain-host` integration testing
- Cross-backend comparison
- Structured test result collection
- CI/CD integration

### Kain Test Programs

Kain test programs (`.kn` files) provide:
- Language-level behavior validation
- Cross-backend portability
- Semantic correctness verification
- Regression detection

---

## Validation Metrics

Conformance tests track:

- **Coverage:** Percentage of runtime ABI surface validated
- **Parity:** Agreement between backends on identical inputs
- **Stability:** Test pass rate over time
- **Performance:** Runtime overhead compared to baseline

---

## Related Documentation

- **Spec Requirements:** `.kiro/specs/kain-native-runtime-completion/requirements.md`
- **Spec Design:** `.kiro/specs/kain-native-runtime-completion/design.md`
- **Spec Tasks:** `.kiro/specs/kain-native-runtime-completion/tasks.md`
- **Smoke Fixtures:** `runtime/fixtures/README.md`
- **Validation Commands:** `runtime/NATIVE_RUNTIME_VALIDATION.md`
- **Progress Tracker:** `runtime/NATIVE_RUNTIME_COMPLETION_TRACKER.md`

---

## Contributing Guidelines

When adding conformance tests:

1. **Follow the test organization structure** - Use the established directory layout
2. **Write minimal, focused tests** - One behavior per test
3. **Document expected behavior** - Clear comments and expected output files
4. **Test both success and failure** - Validate error paths as well as happy paths
5. **Update this README** - Keep the documentation current
6. **Run the full suite** - Ensure new tests don't break existing ones
7. **Consider all backends** - Think about how the test applies to different execution modes

---

## Notes

- Conformance tests are **not** meant to replace unit tests in individual crates
- Tests should be **deterministic** and **reproducible** across runs
- Platform-specific tests should **fail gracefully** on unsupported platforms
- Focus on **ABI contracts** and **runtime semantics**, not implementation details
- Keep tests **minimal** - resist the urge to test everything in one program
- **Reuse test infrastructure** - don't create duplicate harnesses

