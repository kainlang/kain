# Diagnostics Conformance Tests

**Category:** Diagnostics  
**Purpose:** Validate structured diagnostics, error codes, and failure reporting

---

## Test Coverage

### Subsystem Diagnostics
- [ ] Contract subsystem errors
- [ ] Reflection subsystem errors
- [ ] Actor subsystem errors
- [ ] Async subsystem errors
- [ ] UI subsystem errors
- [ ] Graphics subsystem errors
- [ ] Platform subsystem errors
- [ ] Host bridge subsystem errors

### Error Code Stability
- [ ] Stable error codes across versions
- [ ] Error code uniqueness
- [ ] Error code documentation
- [ ] Error code categorization

### Startup Validation
- [ ] Contract mismatch diagnostics
- [ ] Missing required service
- [ ] Optional service downgrade
- [ ] Version incompatibility
- [ ] Invalid bundle path
- [ ] Malformed artifact

### Runtime Failures
- [ ] Actor spawn failure
- [ ] Mailbox overflow
- [ ] Task cancellation
- [ ] Resource exhaustion
- [ ] Invalid operation
- [ ] Platform capability missing

### Diagnostic Format
- [ ] Structured diagnostic output
- [ ] Human-readable messages
- [ ] Machine-readable codes
- [ ] Source path attribution
- [ ] Severity levels
- [ ] Detail information

---

## Running Tests

```bash
# Run all diagnostics tests
./run_tests.sh

# Run specific test
./run_tests.sh test_diagnostics_contract_mismatch.kn

# Run on specific backend
./run_tests.sh --backend native
```

---

## Notes

- Diagnostics tests validate error reporting quality
- Tests should verify both error codes and messages
- Focus on diagnostic stability across versions
- Document expected diagnostic format

