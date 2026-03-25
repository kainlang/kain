# Low-Level Memory Helper Implementation Checklist

**Task:** 4.1 Inventory canonical low-level helper requirements  
**Date:** 2026-03-01  
**Source Analysis:** `crates/kain-core/src/low_level_memory.rs` (2543 lines)  
**Status Document:** `crates/kain-core/LOW_LEVEL_MEMORY_STATUS.md`

## Executive Summary

The KAIN compiler's low-level memory layer (`low_level_memory.rs`) emits calls to **13 canonical runtime helper functions** that must be implemented in the native runtime. These helpers provide the ABI bridge between compiler-emitted code and native memory operations.

**Current Status:**
- ✅ **Compiler-side:** Complete lowering pipeline with bitfield/union/pointer support (2543 lines)
- ✅ **Rust backend:** Helpers implemented in generated code (found in smoketest artifacts)
- ❌ **Native C runtime:** No implementations found in `runtime/native/`
- ❌ **LLVM backend:** Helper bindings not yet wired
- ❌ **C++ backend:** Helper bindings not yet wired

**Requirements Coverage:**
- Requirement 3.1: Canonical low-level helper surface ✅ DEFINED
- Requirement 3.4: Backend/runtime helper parity ❌ MISSING
- Requirement 14.4: Implementation documentation ✅ THIS DOCUMENT

---

## Canonical Helper Surface

### Category 1: Pointer and Address Operations

#### 1.1 `__kain_bind_local`
**Signature:** `fn __kain_bind_local<T>(x: T) -> ptr<T>`

**Purpose:** Create a pointer binding to a local variable that has its address taken.

**Compiler Emission Context:**
```rust
// When address-taken analysis detects:
let x = 42
let ptr = addr_of(x)

// Compiler injects:
let x = 42
let __kain_ptr_x = __kain_bind_local(x)  // ← Helper call
let ptr = __kain_ptr_x
```

**Implementation Requirements:**
- Must return a stable pointer to the value
- Pointer must remain valid for the variable's lifetime
- For stack variables: return address of stack slot
- For heap variables: return existing heap pointer
- Must handle mutable and immutable bindings

**ABI Considerations:**
- Target-specific calling convention
- Pointer size: 64-bit on x86_64/ARM64, 32-bit on WASM32
- Alignment: natural pointer alignment for target

**Native Runtime Implementation Status:** ❌ NOT IMPLEMENTED

---

#### 1.2 `__kain_addr_of`
**Signature:** `fn __kain_addr_of<T>(x: T) -> ptr<T>`

**Purpose:** Take the address of a value expression (fallback when bind_local not applicable).

**Compiler Emission Context:**
```rust
// When taking address of non-addressable expression:
let ptr = addr_of(some_function_call())

// Compiler emits:
let ptr = __kain_addr_of(some_function_call())  // ← Helper call
```

**Implementation Requirements:**
- May need to allocate temporary storage for rvalue
- Return pointer to that storage
- Storage lifetime must extend to pointer use
- Consider using stack allocation for small values

**ABI Considerations:**
- May require stack frame manipulation
- Temporary storage alignment requirements
- Cleanup/deallocation strategy

**Native Runtime Implementation Status:** ❌ NOT IMPLEMENTED

---

#### 1.3 `__kain_ptr_offset`
**Signature:** `fn __kain_ptr_offset<T>(ptr: ptr<T>, offset: isize, stride: isize) -> ptr<T>`

**Purpose:** Perform pointer arithmetic with explicit stride.

**Compiler Emission Context:**
```rust
// Source:
let ptr = base_ptr.offset(10)

// Compiler emits:
let ptr = __kain_ptr_offset(base_ptr, 10, sizeof(T))  // ← Helper call
```

**Implementation Requirements:**
- Compute: `ptr + (offset * stride)`
- Handle negative offsets
- No bounds checking (unsafe operation)
- Preserve pointer provenance if tracked

**ABI Considerations:**
- Pointer arithmetic must match target ABI
- Overflow behavior: wrap or trap (target-specific)
- Alignment: result may be misaligned

**Native Runtime Implementation Status:** ❌ NOT IMPLEMENTED

---

#### 1.4 `__kain_field_ptr`
**Signature:** `fn __kain_field_ptr<T>(ptr: ptr<T>, field: &str, offset: usize) -> ptr<u8>`

**Purpose:** Compute pointer to struct field given base pointer and field offset.

**Compiler Emission Context:**
```rust
// Source:
let field_ptr = addr_of(obj.field)

// Compiler emits (when obj is address-taken):
let field_ptr = __kain_field_ptr(__kain_ptr_obj, "field", 16)  // ← Helper call
```

**Implementation Requirements:**
- Compute: `ptr + offset`
- Return byte pointer (cast to appropriate type at use site)
- Field name is for diagnostics/debugging only
- No validation of field existence

**ABI Considerations:**
- Offset is pre-computed by layout engine
- Result pointer may require alignment adjustment
- Bitfield fields are NOT handled by this helper (see `__kain_bitfield_get/set`)

**Native Runtime Implementation Status:** ❌ NOT IMPLEMENTED

---

#### 1.5 `__kain_index_ptr`
**Signature:** `fn __kain_index_ptr<T>(ptr: ptr<T>, index: isize, stride: isize) -> ptr<T>`

**Purpose:** Compute pointer to array element.

**Compiler Emission Context:**
```rust
// Source:
let elem_ptr = addr_of(arr[5])

// Compiler emits (when arr is address-taken):
let elem_ptr = __kain_index_ptr(__kain_ptr_arr, 5, sizeof(T))  // ← Helper call
```

**Implementation Requirements:**
- Compute: `ptr + (index * stride)`
- Identical to `__kain_ptr_offset` but semantically distinct
- No bounds checking
- Handle negative indices (for pointer arithmetic)

**ABI Considerations:**
- Same as `__kain_ptr_offset`
- May be optimized to same implementation

**Native Runtime Implementation Status:** ❌ NOT IMPLEMENTED

---

### Category 2: Memory Load/Store Operations

#### 2.1 `__kain_mem_load`
**Signature:** `fn __kain_mem_load<T>(ptr: ptr<T>) -> T`

**Purpose:** Load value from pointer (raw memory read).

**Compiler Emission Context:**
```rust
// Source:
let val = mem_load(ptr)

// Compiler emits:
let val = __kain_mem_load(ptr) as TargetType  // ← Helper call + cast
```

**Implementation Requirements:**
- Read `sizeof(T)` bytes from pointer
- Return value as type `T`
- No alignment checking (unsafe)
- No null checking (unsafe)
- Preserve bit pattern for unions/bitfields

**ABI Considerations:**
- Must respect target endianness
- Unaligned loads may trap on some architectures (ARM, older x86)
- Volatile semantics: NOT guaranteed (use explicit volatile load if needed)

**Native Runtime Implementation Status:** ❌ NOT IMPLEMENTED

---

#### 2.2 `__kain_mem_store`
**Signature:** `fn __kain_mem_store<T>(ptr: ptr<T>, value: T)`

**Purpose:** Store value to pointer (raw memory write).

**Compiler Emission Context:**
```rust
// Source:
mem_store(ptr, 42)

// Compiler emits:
__kain_mem_store(ptr, 42)  // ← Helper call
```

**Implementation Requirements:**
- Write `sizeof(T)` bytes to pointer
- No alignment checking (unsafe)
- No null checking (unsafe)
- Preserve bit pattern for unions/bitfields

**ABI Considerations:**
- Must respect target endianness
- Unaligned stores may trap on some architectures
- Volatile semantics: NOT guaranteed

**Native Runtime Implementation Status:** ❌ NOT IMPLEMENTED

---

### Category 3: Bitfield Operations

#### 3.1 `__kain_bitfield_get`
**Signature:** `fn __kain_bitfield_get<T>(value: T, field: &str, unit_offset: i64, bit_offset: i64, width: i64, is_signed: bool, promoted_bits: i64) -> i64`

**Purpose:** Extract bitfield value from struct.

**Compiler Emission Context:**
```rust
// Source:
struct Flags:
    @c_bitfield(3, true)
    a: Int

let f = Flags { a: -2 }
let x = f.a

// Compiler emits:
let x = __kain_bitfield_get(f, "a", 0, 0, 3, true, 32)  // ← Helper call
```

**Implementation Requirements:**
1. Load bitfield unit (8 bytes) from `value` at `unit_offset`
2. Extract bits `[bit_offset, bit_offset + width)`
3. If `is_signed`, sign-extend to `promoted_bits`
4. Return as `i64`

**Algorithm:**
```rust
let unit = load_u64_at(value, unit_offset)
let mask = (1u64 << width) - 1
let shifted = unit >> bit_offset
let extracted = shifted & mask
if is_signed:
    sign_extend(extracted, width)
else:
    extracted as i64
```

**ABI Considerations:**
- Bitfield packing order: LSB-first (x86_64, ARM64) vs MSB-first (some embedded)
- Unit size: always 8 bytes (u64)
- Promotion rules: C integer promotion (width < 32 → promote to i32)

**Native Runtime Implementation Status:** ✅ IMPLEMENTED (in generated Rust code)

---

#### 3.2 `__kain_bitfield_set`
**Signature:** `fn __kain_bitfield_set<T, TValue>(value: T, field: &str, unit_offset: i64, bit_offset: i64, width: i64, is_signed: bool, promoted_bits: i64, next: TValue) -> TValue`

**Purpose:** Write bitfield value to struct.

**Compiler Emission Context:**
```rust
// Source:
f.a = 5

// Compiler emits:
__kain_bitfield_set(f, "a", 0, 0, 3, true, 32, 5)  // ← Helper call
```

**Implementation Requirements:**
1. Load bitfield unit (8 bytes) from `value` at `unit_offset`
2. Clear bits `[bit_offset, bit_offset + width)`
3. Insert new value (masked to `width` bits)
4. Store unit back to `value`
5. Return `next` (for assignment expression value)

**Algorithm:**
```rust
let mut unit = load_u64_at(value, unit_offset)
let mask = (1u64 << width) - 1
let encoded = (next as u64) & mask
let shifted_mask = mask << bit_offset
unit = (unit & !shifted_mask) | (encoded << bit_offset)
store_u64_at(value, unit_offset, unit)
return next
```

**ABI Considerations:**
- Must preserve other bitfields in same unit
- Atomic operations: NOT guaranteed (use explicit atomics if needed)
- Bitfield packing order must match `__kain_bitfield_get`

**Native Runtime Implementation Status:** ✅ IMPLEMENTED (in generated Rust code)

---

### Category 4: Union Operations

#### 4.1 `__kain_union_get`
**Signature:** `fn __kain_union_get<TObject, TValue>(value: TObject, field: &str, type_key: &str, byte_size: i64, union_size: i64, fallback: TValue) -> TValue`

**Purpose:** Read union field with type-safe access.

**Compiler Emission Context:**
```rust
// Source:
@c_union
struct Data:
    int_val: Int
    float_val: Float

let d = Data { int_val: 42 }
let f = d.float_val

// Compiler emits:
let f = __kain_union_get(d, "float_val", "Float", 8, 8, 0.0)  // ← Helper call
```

**Implementation Requirements:**
1. Copy `min(byte_size, union_size, sizeof(TValue))` bytes from `value` to `result`
2. Initialize `result` with `fallback` first (for partial copies)
3. Return `result`

**Algorithm:**
```rust
let mut result = fallback
let copy_span = min(byte_size, union_size, sizeof(TValue))
memcpy(&mut result, &value, copy_span)
return result
```

**ABI Considerations:**
- Union size is pre-computed by layout engine
- Type punning: allowed (C-compatible)
- Padding bytes: undefined (do not rely on them)

**Native Runtime Implementation Status:** ✅ IMPLEMENTED (in generated Rust code)

---

#### 4.2 `__kain_union_set`
**Signature:** `fn __kain_union_set<TObject, TValue>(value: TObject, field: &str, type_key: &str, byte_size: i64, union_size: i64, next: TValue) -> TValue`

**Purpose:** Write union field with type-safe access.

**Compiler Emission Context:**
```rust
// Source:
d.float_val = 3.14

// Compiler emits:
__kain_union_set(d, "float_val", "Float", 8, 8, 3.14)  // ← Helper call
```

**Implementation Requirements:**
1. Zero out `union_size` bytes in `value`
2. Copy `min(byte_size, union_size, sizeof(TValue))` bytes from `next` to `value`
3. Return `next`

**Algorithm:**
```rust
memset(&mut value, 0, union_size)
let copy_span = min(byte_size, union_size, sizeof(TValue))
memcpy(&mut value, &next, copy_span)
return next
```

**ABI Considerations:**
- Must zero entire union (for deterministic behavior)
- Active field tracking: NOT automatic (application responsibility)

**Native Runtime Implementation Status:** ✅ IMPLEMENTED (in generated Rust code)

---

#### 4.3 `__kain_union_wrap`
**Signature:** `fn __kain_union_wrap<TObject, TValue>(value: TObject, active: &str, type_key: &str, byte_size: i64, union_size: i64, active_value: TValue) -> TObject`

**Purpose:** Initialize union with active field during aggregate initialization.

**Compiler Emission Context:**
```rust
// Source:
let d = Data { float_val: 3.14 }

// Compiler emits:
let d = __kain_union_wrap(Data::default(), "float_val", "Float", 8, 8, 3.14)  // ← Helper call
```

**Implementation Requirements:**
1. Zero out `union_size` bytes in `value`
2. Copy `min(byte_size, union_size, sizeof(TValue))` bytes from `active_value` to `value`
3. Return modified `value`

**Algorithm:**
```rust
memset(&mut value, 0, union_size)
let copy_span = min(byte_size, union_size, sizeof(TValue))
memcpy(&mut value, &active_value, copy_span)
return value
```

**ABI Considerations:**
- Used during struct initialization
- Ensures deterministic union state

**Native Runtime Implementation Status:** ✅ IMPLEMENTED (in generated Rust code)

---

### Category 5: Allocation Operations

#### 5.1 `__kain_alloc`
**Signature:** `fn __kain_alloc(size: usize, stride: usize, zeroed: bool, seed: T) -> ptr<u8>`

**Purpose:** Allocate heap memory with optional zero-initialization.

**Compiler Emission Context:**
```rust
// Source:
let buffer = alloc(1024, u8, zeroed: true)

// Compiler emits:
let buffer = __kain_alloc(1024, 1, true, 0)  // ← Helper call
```

**Implementation Requirements:**
- Allocate `size * stride` bytes
- If `zeroed`, zero-initialize memory
- `seed` is for type-specific initialization (currently unused)
- Return pointer to allocated memory
- Return null on allocation failure

**ABI Considerations:**
- Alignment: natural alignment for `stride` size
- Allocation strategy: malloc/calloc or custom allocator
- Failure handling: null return (no exceptions)

**Native Runtime Implementation Status:** ✅ IMPLEMENTED (as `__kain_alloc_bytes` in generated Rust code)

---

#### 5.2 `__kain_realloc`
**Signature:** `fn __kain_realloc(ptr: ptr<u8>, size: usize, stride: usize, zeroed_new: bool, seed: T) -> ptr<u8>`

**Purpose:** Resize heap allocation with optional zero-fill of new bytes.

**Compiler Emission Context:**
```rust
// Source:
let bigger = realloc(buffer, 2048, u8, zeroed_new: true)

// Compiler emits:
let bigger = __kain_realloc(buffer, 2048, 1, true, 0)  // ← Helper call
```

**Implementation Requirements:**
- Resize allocation to `size * stride` bytes
- Preserve existing data
- If `zeroed_new` and size increased, zero-fill new bytes
- Return pointer to resized memory (may be different address)
- Return null on allocation failure

**ABI Considerations:**
- Realloc semantics: may move memory
- Failure handling: original pointer remains valid on failure
- Zero-fill: only new bytes, not existing data

**Native Runtime Implementation Status:** ✅ IMPLEMENTED (as `__kain_realloc_bytes` in generated Rust code)

---

## Helper Implementation Matrix

| Helper | Compiler Emits | Rust Backend | Native C Runtime | LLVM Backend | C++ Backend |
|--------|----------------|--------------|------------------|--------------|-------------|
| `__kain_bind_local` | ✅ Yes | ❌ No | ❌ No | ❌ No | ❌ No |
| `__kain_addr_of` | ✅ Yes | ❌ No | ❌ No | ❌ No | ❌ No |
| `__kain_ptr_offset` | ✅ Yes | ❌ No | ❌ No | ❌ No | ❌ No |
| `__kain_field_ptr` | ✅ Yes | ❌ No | ❌ No | ❌ No | ❌ No |
| `__kain_index_ptr` | ✅ Yes | ❌ No | ❌ No | ❌ No | ❌ No |
| `__kain_mem_load` | ✅ Yes | ❌ No | ❌ No | ❌ No | ❌ No |
| `__kain_mem_store` | ✅ Yes | ❌ No | ❌ No | ❌ No | ❌ No |
| `__kain_bitfield_get` | ✅ Yes | ✅ Yes | ❌ No | ❌ No | ❌ No |
| `__kain_bitfield_set` | ✅ Yes | ✅ Yes | ❌ No | ❌ No | ❌ No |
| `__kain_union_get` | ✅ Yes | ✅ Yes | ❌ No | ❌ No | ❌ No |
| `__kain_union_set` | ✅ Yes | ✅ Yes | ❌ No | ❌ No | ❌ No |
| `__kain_union_wrap` | ✅ Yes | ✅ Yes | ❌ No | ❌ No | ❌ No |
| `__kain_alloc` | ✅ Yes | ✅ Yes | ❌ No | ❌ No | ❌ No |
| `__kain_realloc` | ✅ Yes | ✅ Yes | ❌ No | ❌ No | ❌ No |

**Legend:**
- ✅ Yes: Implemented and tested
- ❌ No: Not implemented
- ⚠️ Partial: Partially implemented or needs work

---

## Native Runtime Implementation Plan

### Phase 1: Core Pointer Operations (Priority: HIGH)
**Estimated Effort:** 2-3 days

**Helpers:**
1. `__kain_bind_local` - Stack pointer binding
2. `__kain_addr_of` - Address-of fallback
3. `__kain_ptr_offset` - Pointer arithmetic
4. `__kain_field_ptr` - Field pointer calculation
5. `__kain_index_ptr` - Array element pointer

**Implementation Location:** `runtime/native/src/core/kain_runtime_memory.c`

**Header:** `runtime/native/include/kain_runtime_memory.h`

**Validation:** Add conformance tests in `runtime/conformance/07_low_level_memory/`

---

### Phase 2: Memory Load/Store (Priority: HIGH)
**Estimated Effort:** 1 day

**Helpers:**
1. `__kain_mem_load` - Raw memory read
2. `__kain_mem_store` - Raw memory write

**Implementation Location:** `runtime/native/src/core/kain_runtime_memory.c`

**Validation:** Add load/store tests with various types and alignments

---

### Phase 3: Bitfield Operations (Priority: MEDIUM)
**Estimated Effort:** 2 days

**Helpers:**
1. `__kain_bitfield_get` - Bitfield read
2. `__kain_bitfield_set` - Bitfield write

**Implementation Location:** `runtime/native/src/core/kain_runtime_bitfield.c`

**Header:** `runtime/native/include/kain_runtime_bitfield.h`

**Validation:** Port existing Rust tests to C conformance tests

**Note:** Rust implementation already exists and can be used as reference

---

### Phase 4: Union Operations (Priority: MEDIUM)
**Estimated Effort:** 1-2 days

**Helpers:**
1. `__kain_union_get` - Union field read
2. `__kain_union_set` - Union field write
3. `__kain_union_wrap` - Union initialization

**Implementation Location:** `runtime/native/src/core/kain_runtime_union.c`

**Header:** `runtime/native/include/kain_runtime_union.h`

**Validation:** Port existing Rust tests to C conformance tests

**Note:** Rust implementation already exists and can be used as reference

---

### Phase 5: Allocation Operations (Priority: LOW)
**Estimated Effort:** 1 day

**Helpers:**
1. `__kain_alloc` - Heap allocation
2. `__kain_realloc` - Heap reallocation

**Implementation Location:** `runtime/native/src/core/kain_runtime_core.c` (already has allocation)

**Validation:** Extend existing allocation tests

**Note:** May already be partially implemented in `kain_runtime_core.c`

---

## Backend Integration Requirements

### LLVM Backend (`crates/kain-sys-codegen/src/codegen_llvm/mod.rs`)

**Required Changes:**
1. Add external function declarations for all 13 helpers
2. Wire helper calls in lowered memory operations
3. Add target-specific calling conventions
4. Handle pointer types correctly (i8* for generic pointers)

**Example:**
```rust
// Declare external helper
let bind_local_fn = module.add_function(
    "__kain_bind_local",
    fn_type,
    Some(Linkage::External)
);

// Emit call
builder.build_call(bind_local_fn, &[value], "ptr")
```

---

### C++ Backend (`crates/kain-sys-codegen/src/codegen_cpp/mod.rs`)

**Required Changes:**
1. Add forward declarations for all 13 helpers
2. Emit helper calls in lowered memory operations
3. Handle template instantiation for generic helpers
4. Add runtime header include: `#include <kain_runtime_memory.h>`

**Example:**
```cpp
// Forward declaration
template<typename T>
T* __kain_bind_local(T& value);

// Emit call
auto ptr = __kain_bind_local(x);
```

---

### Rust Backend (Already Implemented)

**Status:** ✅ Helpers already generated in output code

**Location:** Generated `lib.rs` files in smoketest artifacts

**No changes needed** - Rust backend already emits helper implementations inline

---

## Validation Strategy

### Conformance Test Suite
**Location:** `runtime/conformance/07_low_level_memory/`

**Test Categories:**
1. **Pointer Operations**
   - `test_bind_local.c` - Stack pointer binding
   - `test_addr_of.c` - Address-of operations
   - `test_ptr_offset.c` - Pointer arithmetic
   - `test_field_ptr.c` - Field pointer calculation
   - `test_index_ptr.c` - Array indexing

2. **Memory Operations**
   - `test_mem_load.c` - Load various types
   - `test_mem_store.c` - Store various types
   - `test_unaligned_access.c` - Unaligned load/store

3. **Bitfield Operations**
   - `test_bitfield_get.c` - Read bitfields (signed/unsigned, various widths)
   - `test_bitfield_set.c` - Write bitfields
   - `test_bitfield_packing.c` - Multi-field bitfield structs

4. **Union Operations**
   - `test_union_get.c` - Read union fields
   - `test_union_set.c` - Write union fields
   - `test_union_wrap.c` - Union initialization

5. **Allocation Operations**
   - `test_alloc.c` - Heap allocation (zeroed/unzeroed)
   - `test_realloc.c` - Heap reallocation

---

## ABI Compatibility Matrix

| Target | Pointer Size | Endianness | Bitfield Order | Alignment | Status |
|--------|--------------|------------|----------------|-----------|--------|
| **x86_64 Linux** | 64-bit | Little | LSB-first | 8-byte | ✅ Supported |
| **x86_64 Windows** | 64-bit | Little | LSB-first | 8-byte | ✅ Supported |
| **ARM64** | 64-bit | Little | LSB-first | 8-byte | ✅ Supported |
| **WASM32** | 32-bit | Little | LSB-first | 4-byte | ⚠️ Limited |
| **WASM64** | 64-bit | Little | LSB-first | 8-byte | ⚠️ Limited |

**Notes:**
- WASM targets have limited pointer support (no raw pointers in WASM spec)
- Bitfield order is ABI-specific (LSB-first for all current targets)
- Alignment requirements vary by target (handled by layout engine)

---

## Open Questions and Decisions Needed

### 1. Pointer Provenance Tracking
**Question:** Should native runtime track pointer provenance (Stack/Heap/CImport)?

**Options:**
- A) No tracking (current Rust implementation)
- B) Runtime tracking with metadata
- C) Compile-time only (no runtime overhead)

**Recommendation:** Option A for now (defer to Phase 5 of memory layer design)

---

### 2. Alignment Checking
**Question:** Should helpers validate alignment before load/store?

**Options:**
- A) No checking (unsafe, C-compatible)
- B) Debug-only checking (assertions)
- C) Always check (runtime overhead)

**Recommendation:** Option B (debug assertions, disabled in release)

---

### 3. Null Pointer Checking
**Question:** Should helpers validate non-null pointers?

**Options:**
- A) No checking (unsafe, C-compatible)
- B) Debug-only checking
- C) Always check

**Recommendation:** Option A (match C semantics, let OS trap on null deref)

---

### 4. Bitfield Unit Size
**Question:** Should bitfield units be fixed at 8 bytes (u64)?

**Options:**
- A) Fixed 8 bytes (current implementation)
- B) Variable (1/2/4/8 bytes based on field type)
- C) Target-specific (match C compiler behavior)

**Recommendation:** Option A (simplifies implementation, matches current Rust code)

---

## Dependencies and Blockers

### Upstream Dependencies
1. ✅ Compiler lowering pipeline (`low_level_memory.rs`) - COMPLETE
2. ✅ Layout engine (`LayoutRegistry`) - COMPLETE
3. ✅ ABI policy system (`low_level_abi.rs`) - COMPLETE
4. ❌ Parser support for memory operations - MISSING (not a blocker for runtime)

### Downstream Dependencies
1. ❌ LLVM backend helper binding - BLOCKED on this task
2. ❌ C++ backend helper binding - BLOCKED on this task
3. ❌ Native runtime compilation tests - BLOCKED on this task

---

## Success Criteria

**Task 4.1 is complete when:**
1. ✅ This checklist document exists and is comprehensive
2. ✅ All 13 helpers are documented with signatures, purposes, and requirements
3. ✅ Implementation plan is defined with effort estimates
4. ✅ Validation strategy is documented
5. ✅ ABI considerations are documented per helper
6. ✅ Backend integration requirements are specified

**Subsequent tasks (4.2-4.6) will implement the helpers based on this checklist.**

---

## References

- **Compiler Source:** `crates/kain-core/src/low_level_memory.rs` (2543 lines)
- **Status Document:** `crates/kain-core/LOW_LEVEL_MEMORY_STATUS.md`
- **Design Document:** `crates/kain-core/LOW_LEVEL_MEMORY_LAYER_DESIGN.md`
- **ABI Policies:** `crates/kain-core/src/low_level_abi.rs`
- **Metadata Parsing:** `crates/kain-core/src/low_level_memory_metadata.rs`
- **Rust Reference Implementation:** `smoketest/*/generated/lib.rs` (bitfield/union/alloc helpers)

---

**Document Status:** ✅ COMPLETE  
**Next Task:** 4.2 Add canonical helper declarations to native headers  
**Estimated Total Effort:** 7-9 days for full native runtime implementation
