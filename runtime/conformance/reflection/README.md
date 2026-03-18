# Reflection Conformance Tests

**Category:** Reflection  
**Purpose:** Validate reflection payload loading, schema validation, and runtime lookup

---

## Test Coverage

### Payload Loading
- [x] Valid reflection payload parsing
- [x] Invalid JSON handling
- [x] Schema version validation
- [ ] Payload size limits
- [x] Malformed payload handling

### Type Lookup
- [x] Lookup by type name
- [x] Lookup by type ID
- [ ] Nested type resolution
- [ ] Generic type resolution
- [x] Type not found handling

### Item Identity
- [x] Actor metadata lookup
- [x] Message metadata lookup
- [x] Component metadata lookup
- [ ] Service metadata lookup
- [x] Item identity uniqueness

### Service Binding
- [ ] Required service resolution
- [ ] Optional service resolution
- [ ] Service version compatibility
- [x] Missing service handling
- [ ] Service downgrade reporting

### Compatibility Metadata
- [x] Runtime version compatibility
- [x] ABI version compatibility
- [ ] Feature compatibility
- [ ] Migration metadata
- [x] Compatibility class validation

---

## Running Tests

```bash
# Run all reflection tests
./run_tests.sh

# Run with verbose output
./run_tests.sh --verbose

# Run with custom timeouts
./run_tests.sh --compile-timeout 300 --test-timeout 20

# Run on specific backend label
./run_tests.sh --backend native
```

---

## Notes

- Reflection tests validate metadata-driven runtime behavior
- The current lane focuses on compiler-shaped payload loading and lookup
- Tests cover valid and invalid payloads
- The native runtime currently stores type and item metadata; actor/component/message arrays are validated structurally on load

