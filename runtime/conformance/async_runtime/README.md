# Async Runtime Conformance Tests

**Category:** Async Runtime  
**Purpose:** Validate the native async/task/timer lane in the KAIN runtime

---

## Coverage

- Task spawn and completion
- Task wake / poll mechanics
- Timer registration and cancellation
- Task cancellation
- Async sleep helper

---

## Tests

- `test_task_spawn_basic`
- `test_task_wake_poll`
- `test_timer_cancel`
- `test_task_cancel`
- `test_async_sleep`

---

## Running

```bash
# Run the async conformance lane
./run_tests.sh

# Run with explicit timeout overrides
./run_tests.sh --compile-timeout 300 --test-timeout 20

# Show test output while running
./run_tests.sh --verbose
```

The runner compiles each test with the native async implementation plus the
diagnostics/version support files, then executes each binary with hard
timeouts through the shared timeout helper.

---

## Notes

- The async tests are deterministic and should not rely on long waits.
- Timer-based tests keep their delays short and still enforce hard runtime
  timeouts at the runner level.
- The runtime exposes `KainTaskRuntimeState` through `KainFutureContext` so
  task functions can observe wake and timer state without touching actor code.
