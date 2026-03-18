# Async Runtime Conformance Tests

**Category:** Async Runtime  
**Purpose:** Validate async/await, futures, timers, and task scheduling

---

## Test Coverage

### Task Spawn and Completion
- [ ] Basic task spawn
- [ ] Task with return value
- [ ] Task spawn failure handling
- [ ] Task completion notification

### Wake/Poll Mechanics
- [ ] Task wake from external source
- [ ] Poll once behavior
- [ ] Wake queue management
- [ ] Multiple wake handling

### Timer Operations
- [ ] Timer registration
- [ ] Timer cancellation
- [ ] Timer wake delivery
- [ ] Multiple concurrent timers
- [ ] Timer precision and drift

### Cancellation
- [ ] Task cancellation
- [ ] Cancellation propagation
- [ ] Cleanup on cancellation
- [ ] Cancellation token handling

### Actor/Task Interop
- [ ] Actor awaiting task
- [ ] Task sending to actor
- [ ] Mixed actor/task scheduling
- [ ] Deadlock prevention

### Scheduler Integration
- [ ] Task queue management
- [ ] Fair scheduling
- [ ] Blocking wait integration
- [ ] Scheduler overhead

---

## Running Tests

```bash
# Run all async runtime tests
./run_tests.sh

# Run specific test
./run_tests.sh test_task_spawn_basic.kn

# Run on specific backend
./run_tests.sh --backend llvm
```

---

## Notes

- Async runtime tests validate the async/await execution model
- Tests should be deterministic despite async scheduling
- Focus on observable behavior and timing guarantees
- Document any known timing-dependent behavior

