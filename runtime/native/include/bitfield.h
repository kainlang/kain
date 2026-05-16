#ifndef BITFIELD_H
#define BITFIELD_H

#include <stdint.h>

/*
 * KAIN Native Runtime Bitfield Operations
 *
 * This header defines the canonical bitfield access helpers for the KAIN
 * native runtime. These helpers provide C-compatible bitfield semantics
 * with explicit control over packing, width, and sign extension.
 *
 * Requirements Coverage:
 * - Requirement 3.1: Canonical low-level helper surface
 * - Requirement 3.2: Bitfield operations
 * - Requirement 3.6: Memory layout and ABI policy
 *
 * Source: runtime/LOW_LEVEL_HELPER_IMPLEMENTATION_CHECKLIST.md
 * Compiler: crates/kain-core/src/low_level_memory.rs
 *
 * Bitfield Packing Rules:
 * - Bitfield unit size: always 8 bytes (uint64_t)
 * - Bit ordering: LSB-first (x86_64, ARM64, WASM)
 * - Integer promotion: fields < 32 bits promote to i32/u32
 * - Sign extension: applied for signed fields during get operations
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Bitfield Operations
 * ============================================================================ */

/*
 * __kain_bitfield_get
 *
 * Extract bitfield value from struct.
 *
 * Purpose:
 *   Read a bitfield from a struct, applying sign extension if needed.
 *
 * Algorithm:
 *   1. Load bitfield unit (8 bytes) from value at unit_offset
 *   2. Extract bits [bit_offset, bit_offset + width)
 *   3. If is_signed, sign-extend to promoted_bits
 *   4. Return as int64_t
 *
 * Parameters:
 *   value - Pointer to struct containing bitfield
 *   field - Field name (for diagnostics/debugging only)
 *   unit_offset - Byte offset of bitfield unit from struct base
 *   bit_offset - Bit offset within the unit (0-63)
 *   width - Number of bits in the field (1-64)
 *   is_signed - If non-zero, apply sign extension
 *   promoted_bits - Target bit width for promotion (typically 32 or 64)
 *
 * Returns:
 *   Extracted bitfield value as int64_t
 *
 * ABI Considerations:
 *   - Bitfield packing order: LSB-first (x86_64, ARM64)
 *   - Unit size: always 8 bytes (uint64_t)
 *   - Promotion rules: C integer promotion (width < 32 → promote to i32)
 *   - Sign extension: applied for signed fields
 *
 * Example Emission:
 *   struct Flags:
 *       @c_bitfield(3, true)
 *       a: Int
 *
 *   let f = Flags { a: -2 }
 *   let x = f.a
 *   // Compiler emits: let x = __kain_bitfield_get(&f, "a", 0, 0, 3, 1, 32)
 */
int64_t __kain_bitfield_get(
    const void* value,
    const char* field,
    int64_t unit_offset,
    int64_t bit_offset,
    int64_t width,
    int is_signed,
    int64_t promoted_bits
);

/*
 * __kain_bitfield_set
 *
 * Write bitfield value to struct.
 *
 * Purpose:
 *   Write a value to a bitfield in a struct, preserving other bitfields
 *   in the same unit.
 *
 * Algorithm:
 *   1. Load bitfield unit (8 bytes) from value at unit_offset
 *   2. Clear bits [bit_offset, bit_offset + width)
 *   3. Insert new value (masked to width bits)
 *   4. Store unit back to value
 *
 * Parameters:
 *   value - Pointer to struct containing bitfield
 *   field - Field name (for diagnostics/debugging only)
 *   unit_offset - Byte offset of bitfield unit from struct base
 *   bit_offset - Bit offset within the unit (0-63)
 *   width - Number of bits in the field (1-64)
 *   is_signed - If non-zero, field is signed (affects masking)
 *   promoted_bits - Target bit width for promotion (typically 32 or 64)
 *   next - Value to write to the bitfield
 *
 * ABI Considerations:
 *   - Must preserve other bitfields in same unit
 *   - Atomic operations: NOT guaranteed (use explicit atomics if needed)
 *   - Bitfield packing order must match __kain_bitfield_get
 *
 * Example Emission:
 *   f.a = 5
 *   // Compiler emits: __kain_bitfield_set(&f, "a", 0, 0, 3, 1, 32, 5)
 */
void __kain_bitfield_set(
    void* value,
    const char* field,
    int64_t unit_offset,
    int64_t bit_offset,
    int64_t width,
    int is_signed,
    int64_t promoted_bits,
    int64_t next
);

#ifdef __cplusplus
}
#endif

#endif /* BITFIELD_H */
