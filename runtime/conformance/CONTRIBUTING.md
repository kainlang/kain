# Contributing to Native Runtime Conformance Tests

This guide explains how to add new conformance tests as the native runtime completion work progresses.

---

## Quick Start

1. **Choose the right category** - Determine which test category your test belongs to
2. **Write a minimal test program** - Create a focused `.kn` test that validates one specific behavior
3. **Define expected outputs** - Create expected result files for each backend
4. **Update documentation** - Add your test to the category README
5. **Run and verify** - Ensure your test passes on all applicable backends

---

## Test Categories and Phases

Each test category corresponds to a phase in the native runtime completion spec:

| Category | Phase | Description |
|----------|-------|-------------|
| `abi_parity/` | Phase 4 | Low-level memory helper ABI parity |
| `actor_runtime/` | Phase 5-6 | Actor bootstrap and full actor semantics |
| `async_runtime/` | Phase 7 | Async/await, futures, and timers |
| `reflection/` | Phase 3 | Reflection payload loading and lookup |
| `diagnostics/` | Phase 2 | Structured diagnostics and error codes |
| `ui_runtime/` | Phase 8 | UI bundle interpretation and component lifecycle |
| `graphics_runtime/` | Phase 9 | Shader/material/compute artifact loading |
| `hot_reload/` | Phase 10 | Versioning, compatibility, and state migration |
| `platform_parity/` | Phase 12 | Cross-platform runtime behavior |

---

## Writing a Conformance Test

### Step 1: Create the Test Program

Create a new `.kn` file in the appropriate category directory:

```bash
# Example: ABI parity test for pointer field offsets
touch runtime/conformance/abi_parity/test_pointer_field_offset.kn
```

**Test Naming Convention:**
```
test_<area>_<behavior>_<condition>.kn
```

Examples:
- `test_actor_spawn_basic.kn`
- `test_mailbox_bounded_overflow.kn`
- `test_reflection_schema_version_mismatch.kn`
- `test_pointer_field_offset_alignment.kn`

### Step 2: Write Minimal, Focused Test Code

Each test should validate **one specific behavior**. Keep tests minimal and deterministic.

**Good Example (focused):**
```kain
// test_actor_spawn_basic.kn
// Validates that actors can be spawned and receive their initial message

actor Counter {
    state: i32 = 0
    
    fn handle_increment() {
        self.state += 1
        print("Counter: {}", self.state)
    }
}

fn main() {
    let counter = spawn Counter()
    counter.send(Increment)
    // Expected output: "Counter: 1"
}
```

**Bad Example (too complex):**
```kain
// DON'T DO THIS - tests multiple behaviors at once
actor ComplexActor {
    // Tests spawn, state, messages, supervision, registry, etc. all at once
    // This makes it hard to diagnose failures
}
```

### Step 3: Create Expected Output Files

Create an `expected/` directory in the category and add expected output files:

```bash
mkdir -p runtime/conformance/abi_parity/expected
```

Create expected output files for each backend:

**For success tests:**
```json
{
  "test_name": "test_actor_spawn_basic",
  "backend": "llvm",
  "expected_result": "success",
  "expected_output": "Counter: 1\n",
  "expected_diagnostics": [],
  "expected_exit_code": 0
}
```

**For failure tests:**
```json
{
  "test_name": "test_reflection_schema_version_mismatch",
  "backend": "native",
  "expected_result": "failure",
  "expected_output": "",
  "expected_diagnostics": [
    {
      "subsystem": "reflection",
      "code": "KAIN-RT-REFLECT-0001",
      "severity": "error",
      "message": "Schema version mismatch: expected 1.0, got 2.0"
    }
  ],
  "expected_exit_code": 1
}
```

### Step 4: Update Category README

Add your test to the category's README.md:

```markdown
### Actor Spawn and Bootstrap
- [x] Basic actor spawn (`test_actor_spawn_basic.kn`)
- [ ] Actor with initial state
- [ ] Actor spawn failure handling
```

### Step 5: Update Test Runner (if needed)

If this is the first test in a category, update the category's `run_tests.sh` to actually run tests:

```bash
#!/usr/bin/env bash
# Actor Runtime Conformance Test Runner

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND="${1:-all}"

echo "Running actor runtime tests (backend: $BACKEND)"
echo ""

# Find all test files
TEST_FILES=($(find "$SCRIPT_DIR" -name "test_*.kn" | sort))

if [[ ${#TEST_FILES[@]} -eq 0 ]]; then
    echo "No tests found"
    exit 0
fi

# Run each test
PASSED=0
FAILED=0

for test_file in "${TEST_FILES[@]}"; do
    test_name=$(basename "$test_file")
    echo "Running $test_name..."
    
    # TODO: Implement actual test execution
    # For now, just report as passed
    echo "  ✓ $test_name (placeholder)"
    ((PASSED++))
done

echo ""
echo "Passed: $PASSED"
echo "Failed: $FAILED"

exit 0
```

### Step 6: Run and Verify

```bash
# Run your specific test
./runtime/conformance/run_test.sh abi_parity/test_pointer_field_offset.kn

# Run all tests in the category
./runtime/conformance/abi_parity/run_tests.sh

# Run all conformance tests
./runtime/conformance/run_all.sh
```

---

## Test Design Principles

### 1. Minimal and Focused

Each test should validate **one specific behavior**. If you find yourself testing multiple things, split into multiple tests.

✅ **Good:** `test_actor_spawn_basic.kn` - Tests only actor spawn  
❌ **Bad:** `test_actor_everything.kn` - Tests spawn, messages, supervision, etc.

### 2. Deterministic

Tests must produce consistent, reproducible results across runs.

✅ **Good:** Use fixed values, predictable sequences  
❌ **Bad:** Use random numbers, timestamps, non-deterministic ordering

### 3. Cross-Backend Aware

Consider how your test applies to different backends:

- **Interpreter:** Direct execution, no compilation
- **LLVM:** Native compilation, full optimization
- **C++:** C++ codegen, different ABI
- **Native Runtime:** Raw C runtime, direct ABI

Some tests may only apply to certain backends - document this clearly.

### 4. Self-Documenting

Use clear names, comments, and expected outputs:

```kain
// test_mailbox_bounded_overflow.kn
// Validates that bounded mailboxes reject messages when full
// Expected: Send succeeds until capacity reached, then fails with diagnostic

actor BoundedActor {
    mailbox_capacity: 2  // Explicit capacity limit
    
    fn handle_message(msg: i32) {
        // Process slowly to fill mailbox
        sleep(100)
    }
}
```

### 5. Test Both Success and Failure

Don't just test happy paths - validate error handling too:

- `test_actor_spawn_basic.kn` - Success case
- `test_actor_spawn_invalid_state.kn` - Failure case

---

## Expected Output Format

### Success Test Output

```json
{
  "test_name": "test_actor_spawn_basic",
  "backend": "llvm",
  "expected_result": "success",
  "expected_output": "Counter: 1\n",
  "expected_diagnostics": [],
  "expected_exit_code": 0,
  "notes": "Basic actor spawn should succeed without diagnostics"
}
```

### Failure Test Output

```json
{
  "test_name": "test_mailbox_bounded_overflow",
  "backend": "native",
  "expected_result": "failure",
  "expected_output": "",
  "expected_diagnostics": [
    {
      "subsystem": "actor",
      "code": "KAIN-RT-ACTOR-0003",
      "severity": "error",
      "message": "Mailbox capacity exceeded",
      "detail": "Attempted to send message to full mailbox (capacity: 2)"
    }
  ],
  "expected_exit_code": 1,
  "notes": "Bounded mailbox should reject messages when full"
}
```

### Backend-Specific Expectations

If behavior differs across backends, create separate expected output files:

```
expected/
├── test_actor_spawn_basic_interpreter.json
├── test_actor_spawn_basic_llvm.json
├── test_actor_spawn_basic_cpp.json
└── test_actor_spawn_basic_native.json
```

---

## Common Patterns

### Testing ABI Parity

```kain
// test_pointer_field_offset.kn
// Validates that field pointer offsets match across backends

struct Point {
    x: f32
    y: f32
}

fn main() {
    let p = Point { x: 1.0, y: 2.0 }
    let x_ptr = &p.x
    let y_ptr = &p.y
    
    // Offset should be sizeof(f32) = 4 bytes
    let offset = (y_ptr as usize) - (x_ptr as usize)
    assert(offset == 4, "Field offset mismatch")
    
    print("Field offset: {}", offset)
}
```

### Testing Actor Runtime

```kain
// test_actor_registry_lookup.kn
// Validates that registered actors can be looked up by name

actor Service {
    fn handle_ping() {
        print("pong")
    }
}

fn main() {
    let service = spawn Service()
    register("my_service", service)
    
    let found = lookup("my_service")
    assert(found.is_some(), "Service not found in registry")
    
    found.unwrap().send(Ping)
    // Expected output: "pong"
}
```

### Testing Diagnostics

```kain
// test_diagnostics_contract_mismatch.kn
// Validates that contract mismatches produce structured diagnostics

// This test intentionally uses an invalid contract version
// Expected: Structured diagnostic with code KAIN-RT-CONTRACT-0001

fn main() {
    // Runtime should fail during startup with diagnostic
    print("This should not execute")
}
```

---

## Integration with CI/CD

Conformance tests are designed to run in CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Run Conformance Tests
  run: |
    ./runtime/conformance/run_all.sh --mode full
```

**Test Modes:**

- `--mode quick` - Run only smoke tests (fast validation)
- `--mode full` - Run all tests on all backends (complete validation)
- `--mode regression` - Compare against baseline (detect regressions)

---

## Troubleshooting

### Test Fails on One Backend

If a test passes on some backends but fails on others:

1. Check if the behavior is backend-specific (document this)
2. Verify expected outputs are correct for each backend
3. Look for ABI or codegen differences
4. File an issue if this indicates a bug

### Test is Non-Deterministic

If a test produces different results across runs:

1. Remove any sources of randomness
2. Avoid timing-dependent behavior
3. Use fixed seeds for any pseudo-random operations
4. Consider if the test is testing the right thing

### Test is Too Slow

If a test takes too long to run:

1. Reduce iteration counts
2. Use smaller data sets
3. Consider if the test is too complex (split it)
4. Mark as a performance test (run separately)

---

## Review Checklist

Before submitting a new conformance test:

- [ ] Test name follows naming convention
- [ ] Test is minimal and focused (one behavior)
- [ ] Test is deterministic
- [ ] Expected output files created for all backends
- [ ] Category README updated
- [ ] Test includes clear comments
- [ ] Test passes on all applicable backends
- [ ] Both success and failure paths tested (if applicable)
- [ ] Test runner script updated (if first test in category)

---

## Questions?

If you have questions about writing conformance tests:

1. Review existing tests in other categories
2. Check the main README: `runtime/conformance/README.md`
3. Refer to the spec: `.kiro/specs/kain-native-runtime-completion/`
4. Ask in the project's communication channels

---

## Related Documentation

- **Main README:** `runtime/conformance/README.md`
- **Spec Requirements:** `.kiro/specs/kain-native-runtime-completion/requirements.md`
- **Spec Design:** `.kiro/specs/kain-native-runtime-completion/design.md`
- **Spec Tasks:** `.kiro/specs/kain-native-runtime-completion/tasks.md`
- **Smoke Fixtures:** `runtime/fixtures/README.md`

