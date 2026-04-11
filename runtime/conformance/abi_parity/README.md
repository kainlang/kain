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
- [x] Address-of operations (`__kain_bind_local`, `__kain_addr_of`)
- [x] Field pointer offsets (`__kain_field_ptr`)
- [x] Index pointer arithmetic (`__kain_index_ptr`, `__kain_ptr_offset`)
- [x] Pointer-to-pointer operations
- [x] Null pointer handling

### Load/Store Operations
- [x] Basic load/store (i8, i16, i32, i64, f32, f64)
- [x] Unaligned load/store
- [ ] Volatile load/store
- [ ] Atomic load/store
- [x] Struct load/store
- [x] Array load/store
- [x] Bit pattern preservation

### Union Operations
- [x] Union field read (`__kain_union_get`)
- [x] Union field write (`__kain_union_set`)
- [x] Union initialization (`__kain_union_wrap`)
- [x] Type punning
- [x] Different field sizes
- [x] Zero initialization

### Bitfield Operations
- [x] Bitfield read (`__kain_bitfield_get`)
- [x] Bitfield write (`__kain_bitfield_set`)
- [x] Signed bitfields
- [x] Unsigned bitfields
- [x] Multiple bitfields in same unit
- [x] Bitfield width variations (1-32 bits)
- [x] Boundary values and overflow

### Type Operations
- [ ] Type conversion (int to float, float to int)
- [ ] Type casting (pointer casts, numeric casts)
- [ ] Sign extension
- [ ] Zero extension
- [ ] Truncation

### Allocation and Lifetime
- [ ] Allocation and deallocation (`__kain_alloc`)
- [ ] Reallocation (`__kain_realloc`)
- [ ] Retain/release semantics
- [ ] Reference counting
- [ ] Memory leak detection
- [ ] Double-free detection

---

## Running Tests

```bash
# Compile all tests
./compile_tests.sh

# Run all ABI parity tests
./run_tests.sh

# Run with a backend label in the report output
./run_tests.sh --backend llvm
```

`--backend` is currently a reporting label for this harness family, not proof that generated LLVM programs were compiled and executed end to end. Use `runtime/fixtures/validate_all.sh` for the LLVM executable proof lane.

---

## Implemented Tests

### test_pointer_operations.c
Tests canonical pointer helper implementations:
- `__kain_bind_local` - Create pointer binding to local variable
- `__kain_addr_of` - Take address of value expression
- `__kain_ptr_offset` - Pointer arithmetic with explicit stride
- `__kain_field_ptr` - Compute pointer to struct field
- `__kain_index_ptr` - Compute pointer to array element
- Pointer arithmetic consistency between helpers

**Coverage:** 6 test cases validating pointer operations, field offsets, array indexing, and consistency

### test_load_store_operations.c
Tests canonical memory load/store helpers:
- `__kain_mem_load` - Load value from pointer (raw memory read)
- `__kain_mem_store` - Store value to pointer (raw memory write)
- Various data types (int8, int16, int32, int64, float, double)
- Struct and array operations
- Partial data operations
- Bit pattern preservation

**Coverage:** 12 test cases validating load/store for all primitive types, structs, arrays, and edge cases

### test_union_operations.c
Tests canonical union helper implementations:
- `__kain_union_get` - Read union field with type-safe access
- `__kain_union_set` - Write union field with type-safe access
- `__kain_union_wrap` - Initialize union with active field
- Type punning and bit pattern preservation
- Different field sizes
- Zero initialization

**Coverage:** 8 test cases validating union operations, type punning, and field size variations

### test_bitfield_operations.c
Tests canonical bitfield helper implementations:
- `__kain_bitfield_get` - Extract bitfield value from struct
- `__kain_bitfield_set` - Write bitfield value to struct
- Signed and unsigned bitfields
- Various widths (1-32 bits)
- Multiple fields in same storage unit
- Boundary values and overflow handling

**Coverage:** 8 test cases validating bitfield extraction, insertion, sign extension, and field preservation

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

1. Create a new `test_<area>_<behavior>.c` file in this directory
2. Include the canonical helper headers: `../../native/include/kain_runtime_memory.h`
3. Follow the existing test pattern:
   - Use `TEST_PASS(name)` and `TEST_FAIL(name, ...)` macros
   - Return 1 for pass, 0 for fail from each test function
   - Implement `main()` that runs all tests and reports results
4. Add the test to the `TESTS` array in `compile_tests.sh`
5. Update this README with the new test coverage
6. Run `./compile_tests.sh` and `./run_tests.sh` to verify

---

## Notes

- ABI parity tests are critical for ensuring consistent behavior across backends
- Tests should be deterministic and produce identical results on all backends
- Focus on low-level operations that are most likely to diverge
- Document any known platform-specific differences

