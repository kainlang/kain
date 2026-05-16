#ifndef UNION_H
#define UNION_H

#include <stdint.h>
#include <stddef.h>

/*
 * KAIN Native Runtime Union Operations
 *
 * This header defines the canonical union access helpers for the KAIN
 * native runtime. These helpers provide C-compatible union semantics
 * with type-safe field access and deterministic initialization.
 *
 * Requirements Coverage:
 * - Requirement 3.1: Canonical low-level helper surface
 * - Requirement 3.2: Union operations
 * - Requirement 3.6: Memory layout and ABI policy
 *
 * Source: runtime/LOW_LEVEL_HELPER_IMPLEMENTATION_CHECKLIST.md
 * Compiler: crates/kain-core/src/low_level_memory.rs
 *
 * Union Semantics:
 * - Type punning: allowed (C-compatible)
 * - Padding bytes: undefined (do not rely on them)
 * - Active field tracking: NOT automatic (application responsibility)
 * - Initialization: entire union is zeroed before field write
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Union Operations
 * ============================================================================ */

/*
 * __kain_union_get
 *
 * Read union field with type-safe access.
 *
 * Purpose:
 *   Read a field from a union, copying bytes into the output buffer.
 *
 * Algorithm:
 *   1. Initialize output with fallback value
 *   2. Copy min(byte_size, union_size, output_size) bytes from value to output
 *   3. Return (output is modified in-place)
 *
 * Parameters:
 *   value - Pointer to union
 *   field - Field name (for diagnostics/debugging only)
 *   type_key - Type name (for diagnostics/debugging only)
 *   byte_size - Size of the field type in bytes
 *   union_size - Total size of the union in bytes
 *   fallback - Pointer to fallback value (for partial copies)
 *   output - Pointer to output buffer
 *   output_size - Size of output buffer in bytes
 *
 * ABI Considerations:
 *   - Union size is pre-computed by layout engine
 *   - Type punning: allowed (C-compatible)
 *   - Padding bytes: undefined (do not rely on them)
 *   - Copies min(byte_size, union_size, output_size) bytes
 *
 * Example Emission:
 *   @c_union
 *   struct Data:
 *       int_val: Int
 *       float_val: Float
 *
 *   let d = Data { int_val: 42 }
 *   let f = d.float_val
 *   // Compiler emits: __kain_union_get(&d, "float_val", "Float", 8, 8, &fallback, &f, 8)
 */
void __kain_union_get(
    const void* value,
    const char* field,
    const char* type_key,
    int64_t byte_size,
    int64_t union_size,
    const void* fallback,
    void* output,
    size_t output_size
);

/*
 * __kain_union_set
 *
 * Write union field with type-safe access.
 *
 * Purpose:
 *   Write a field to a union, zeroing the entire union first for
 *   deterministic behavior.
 *
 * Algorithm:
 *   1. Zero out union_size bytes in value
 *   2. Copy min(byte_size, union_size, next_size) bytes from next to value
 *
 * Parameters:
 *   value - Pointer to union
 *   field - Field name (for diagnostics/debugging only)
 *   type_key - Type name (for diagnostics/debugging only)
 *   byte_size - Size of the field type in bytes
 *   union_size - Total size of the union in bytes
 *   next - Pointer to value to write
 *   next_size - Size of value to write in bytes
 *
 * ABI Considerations:
 *   - Must zero entire union (for deterministic behavior)
 *   - Active field tracking: NOT automatic (application responsibility)
 *   - Copies min(byte_size, union_size, next_size) bytes
 *
 * Example Emission:
 *   d.float_val = 3.14
 *   // Compiler emits: __kain_union_set(&d, "float_val", "Float", 8, 8, &temp, 8)
 */
void __kain_union_set(
    void* value,
    const char* field,
    const char* type_key,
    int64_t byte_size,
    int64_t union_size,
    const void* next,
    size_t next_size
);

/*
 * __kain_union_wrap
 *
 * Initialize union with active field during aggregate initialization.
 *
 * Purpose:
 *   Initialize a union with a specific field value during struct/union
 *   initialization expressions.
 *
 * Algorithm:
 *   1. Zero out union_size bytes in value
 *   2. Copy min(byte_size, union_size, active_value_size) bytes from active_value to value
 *
 * Parameters:
 *   value - Pointer to union to initialize
 *   active - Active field name (for diagnostics/debugging only)
 *   type_key - Type name (for diagnostics/debugging only)
 *   byte_size - Size of the active field type in bytes
 *   union_size - Total size of the union in bytes
 *   active_value - Pointer to value to initialize with
 *   active_value_size - Size of active value in bytes
 *
 * ABI Considerations:
 *   - Used during struct initialization
 *   - Ensures deterministic union state
 *   - Copies min(byte_size, union_size, active_value_size) bytes
 *
 * Example Emission:
 *   let d = Data { float_val: 3.14 }
 *   // Compiler emits: __kain_union_wrap(&d, "float_val", "Float", 8, 8, &temp, 8)
 */
void __kain_union_wrap(
    void* value,
    const char* active,
    const char* type_key,
    int64_t byte_size,
    int64_t union_size,
    const void* active_value,
    size_t active_value_size
);

#ifdef __cplusplus
}
#endif

#endif /* UNION_H */
