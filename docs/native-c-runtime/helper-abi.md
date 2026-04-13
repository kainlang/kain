# Helper ABI

Kain's native helper ABI is the low-level memory contract used by compiler
lowering.

## Canonical Helper Headers

- `kain_runtime_memory.h`
- `kain_runtime_bitfield.h`
- `kain_runtime_union.h`

## Helper Inventory

### Pointer And Address Operations

- `__kain_bind_local`
- `__kain_addr_of`
- `__kain_ptr_offset`
- `__kain_field_ptr`
- `__kain_index_ptr`

### Memory Load / Store

- `__kain_mem_load`
- `__kain_mem_store`

### Allocation

- `__kain_alloc`
- `__kain_realloc`

### Bitfields

- `__kain_bitfield_get`
- `__kain_bitfield_set`

### Unions

- `__kain_union_get`
- `__kain_union_set`
- `__kain_union_wrap`

## Total Surface

The canonical helper ABI currently exposes 13 helpers.

## Practical Rule

If a language form turns into a pointer, bitfield, union, load/store, or
allocation operation, it should lower through these helpers instead of inventing
an ad hoc backend-specific shortcut.
