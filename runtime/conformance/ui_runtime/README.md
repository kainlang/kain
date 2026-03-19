# UI Runtime Conformance Tests

**Category:** UI Runtime  
**Purpose:** Validate UI bundle interpretation, component lifecycle, and event routing

---

## Test Coverage

### Bundle Validation
- [x] Valid bundle loading (fixture-backed)
- [x] Semantic node validation (via `kain_ui_runtime_validate_bundle`)

### Component Lifecycle
- [x] Component initialization and state materialization

### Event Routing
- [x] Focus management and editable text input routing

### State Management
- [x] State invalidation/dirty tracking (smoke-level)

### Rust-Native vs Raw-Native Parity
- [x] Bundle interpretation parity for the raw-native ABI projection (`native_projection`)

---

## Running Tests

```bash
# Run UI runtime conformance with hard timeouts (compiles into ./bin/)
./run_tests.sh --verbose

# Tight timeouts (useful for CI)
./run_tests.sh --compile-timeout 120 --test-timeout 10
```

---

## Notes

- The shared parity fixture lives at `fixtures/ui_runtime_parity_bundle.json`.
- Override the fixture path for parity runs via `KAIN_UI_PARITY_FIXTURE=...`.
- The raw-native runtime consumes `native_projection` from the serialized runtime bundle; Rust parses the same schema via Serde in `crates/kain-ui/tests/ui_runtime_native_projection_parity.rs`.
