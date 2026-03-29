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
- [x] Bundle interpretation parity for the canonical `output.tree.root` and `output.tree.nodes` contract

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
- The raw-native runtime now treats the canonical output tree as authoritative and resolves titles/scenes directly from that tree before any host-local viewport defaults are applied.
