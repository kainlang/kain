# Reflection Conformance Tests

**Category:** Reflection  
**Purpose:** Validate reflection payload loading, schema validation, and runtime lookup

---

## Test Coverage

### Payload Loading
- [ ] Valid reflection payload parsing
- [ ] Invalid JSON handling
- [ ] Schema version validation
- [ ] Payload size limits
- [ ] Malformed payload handling

### Type Lookup
- [ ] Lookup by type name
- [ ] Lookup by type ID
- [ ] Nested type resolution
- [ ] Generic type resolution
- [ ] Type not found handling

### Item Identity
- [ ] Actor metadata lookup
- [ ] Message metadata lookup
- [ ] Component metadata lookup
- [ ] Service metadata lookup
- [ ] Item identity uniqueness

### Service Binding
- [ ] Required service resolution
- [ ] Optional service resolution
- [ ] Service version compatibility
- [ ] Missing service handling
- [ ] Service downgrade reporting

### Compatibility Metadata
- [ ] Runtime version compatibility
- [ ] ABI version compatibility
- [ ] Feature compatibility
- [ ] Migration metadata
- [ ] Compatibility class validation

---

## Running Tests

```bash
# Run all reflection tests
./run_tests.sh

# Run specific test
./run_tests.sh test_reflection_payload_valid.kn

# Run on specific backend
./run_tests.sh --backend native
```

---

## Notes

- Reflection tests validate metadata-driven runtime behavior
- Tests should cover both valid and invalid payloads
- Focus on schema validation and lookup correctness
- Document expected reflection payload format

