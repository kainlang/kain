# Hot Reload Conformance Tests

**Category:** Hot Reload  
**Purpose:** Validate versioning, compatibility classes, and state migration

---

## Test Coverage

### Version Validation
- [ ] Runtime version checking
- [ ] ABI version checking
- [ ] Bundle version checking
- [ ] Version mismatch handling
- [ ] Version upgrade paths

### Compatibility Checking
- [ ] Compatible update acceptance
- [ ] Incompatible update rejection
- [ ] Compatibility class validation
- [ ] Feature compatibility checking
- [ ] Service compatibility checking

### Migration Hooks
- [ ] Migration hook execution
- [ ] Migration failure handling
- [ ] Migration rollback
- [ ] Migration validation
- [ ] Migration diagnostics

### State Transfer
- [ ] Actor state transfer
- [ ] Task state transfer
- [ ] UI state transfer
- [ ] App state transfer
- [ ] Service state transfer

### Lifecycle Operations
- [ ] Install bundle
- [ ] Update bundle
- [ ] Uninstall bundle
- [ ] Activate bundle
- [ ] Deactivate bundle

---

## Running Tests

```bash
# Run all hot reload tests
./run_tests.sh

# Run specific test
./run_tests.sh test_hot_reload_compatible.kn

# Run migration tests
./run_tests.sh --migration
```

---

## Notes

- Hot reload tests validate live update capabilities
- Tests should cover both compatible and incompatible updates
- Focus on state preservation and migration correctness
- Document compatibility rules and migration requirements

