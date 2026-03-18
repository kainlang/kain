# Actor Runtime Conformance Tests

**Category:** Actor Runtime  
**Purpose:** Validate actor semantics, mailbox operations, supervision, and lifecycle

---

## Test Coverage

### Actor Spawn and Bootstrap
- [ ] Basic actor spawn
- [ ] Actor with initial state
- [ ] Actor spawn failure handling
- [ ] Actor identity and metadata

### Mailbox Operations
- [ ] Send and receive messages
- [ ] Typed message delivery
- [ ] Message ordering guarantees
- [ ] Mailbox capacity limits
- [ ] Backpressure behavior

### Actor Registry
- [ ] Register named actor
- [ ] Lookup registered actor
- [ ] Unregister actor
- [ ] Registry cleanup on actor exit
- [ ] Duplicate name handling

### Monitors and Links
- [ ] Monitor actor exit
- [ ] Link actor lifecycle
- [ ] Exit reason propagation
- [ ] Monitor cleanup
- [ ] Link cleanup

### Supervision
- [ ] Supervisor-child relationship
- [ ] Restart policy (one-for-one)
- [ ] Restart policy (one-for-all)
- [ ] Shutdown policy
- [ ] Escalation policy
- [ ] Bounded restart attempts

### Scheduler
- [ ] Fair scheduling across actors
- [ ] Blocking wait integration
- [ ] Actor yield behavior
- [ ] Scheduler queue management

---

## Running Tests

```bash
# Run all actor runtime tests
./run_tests.sh

# Run specific test
./run_tests.sh test_actor_spawn_basic.kn

# Run on specific backend
./run_tests.sh --backend llvm
```

---

## Adding New Tests

1. Create a new `test_actor_<behavior>.kn` file
2. Add expected output files in `expected/` directory
3. Update this README with the new test
4. Run the test suite to verify

---

## Notes

- Actor runtime tests validate the core actor model semantics
- Tests should cover both success and failure paths
- Focus on observable behavior, not implementation details
- Document any known limitations or platform-specific behavior

