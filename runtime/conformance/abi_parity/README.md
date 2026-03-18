# ABI Parity Conformance Tests

**Category:** ABI Parity  
**Purpose:** Validate that runtime helpers behave consistently across interpreter, LLVM, C++, Rust-hosted, and raw-native lanes

---

## Test Coverage

### Memory Layout and Alignment
- [ ] Struct packing and alignment
- [ ] Union layout
- [ ] Bitfield layout
- [ ] Array stride and alignment
- [ ] Nested struct alignment

### Pointer Operations
- [ ] Address-of operations
- [ ] Field pointer offsets
- [ ] Index pointer arithmetic
- [ ] Pointer-to-pointer operations
- [ ] Null pointer handling

### Load/Store Operations
- [ ] Basic load/store (i8, i16, i32, i64, f32, f64)
- [ ] Unaligned load/store
- [ ] Volatile load/store
- [ ] Atomic load/store
- [ ] Struct load/store

### Type Operations
- [ ] Type conversion (int to float, float to int)
- [ ] Type casting (pointer casts, numeric casts)
- [ ] Sign extension
- [ ] Zero extension
- [ ] Truncation

### Allocation and Lifetime
- [ ] Allocation and deallocation
- [ ] Retain/release semantics
- [ ] Reference counting
- [ ] Memory leak detection
- [ ] Double-free detection

---

## Running Tests

```bash
# Run all ABI parity tests
./run_tests.sh

# Run specific test
./run_tests.sh test_pointer_field_offset.kn

# Run on specific backend
./run_tests.sh --backend llvm
```

---

## Adding New Tests

1. Create a new `test_<area>_<behavior>.kn` file
2. Add expected output files in `expected/` directory
3. Update this README with the new test
4. Run the test suite to verify

---

## Notes

- ABI parity tests are critical for ensuring consistent behavior across backends
- Tests should be deterministic and produce identical results on all backends
- Focus on low-level operations that are most likely to diverge
- Document any known platform-specific differences

