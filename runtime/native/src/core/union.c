/*
 * KAIN Native Runtime Union Operations
 *
 * Implementation of canonical union access helpers for the KAIN
 * native runtime. These helpers provide C-compatible union semantics
 * with type-safe field access and deterministic initialization.
 *
 *
 */

#include "../../include/union.h"
#include <string.h>

/* Helper to compute minimum of three size_t values */
static size_t min3(size_t a, size_t b, size_t c) {
    size_t min_ab = (a < b) ? a : b;
    return (min_ab < c) ? min_ab : c;
}

/* ============================================================================
 * Union Operations
 * ============================================================================ */

/*
 * __kain_union_get
 *
 * Read union field with type-safe access.
 *
 * Algorithm:
 *   1. Initialize output with fallback value
 *   2. Copy min(byte_size, union_size, output_size) bytes from value to output
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
) {
    /* Field name and type key are for diagnostics/debugging only */
    (void)field;
    (void)type_key;

    /* Initialize output with fallback value */
    memcpy(output, fallback, output_size);

    /* Compute how many bytes to copy */
    size_t copy_span = min3((size_t)byte_size, (size_t)union_size, output_size);

    /* Copy bytes from union to output */
    memcpy(output, value, copy_span);
}

/*
 * __kain_union_set
 *
 * Write union field with type-safe access.
 *
 * Algorithm:
 *   1. Zero out union_size bytes in value
 *   2. Copy min(byte_size, union_size, next_size) bytes from next to value
 */
void __kain_union_set(
    void* value,
    const char* field,
    const char* type_key,
    int64_t byte_size,
    int64_t union_size,
    const void* next,
    size_t next_size
) {
    /* Field name and type key are for diagnostics/debugging only */
    (void)field;
    (void)type_key;

    /* Zero out the entire union for deterministic behavior */
    memset(value, 0, (size_t)union_size);

    /* Compute how many bytes to copy */
    size_t copy_span = min3((size_t)byte_size, (size_t)union_size, next_size);

    /* Copy bytes from next to union */
    memcpy(value, next, copy_span);
}

/*
 * __kain_union_wrap
 *
 * Initialize union with active field during aggregate initialization.
 *
 * Algorithm:
 *   1. Zero out union_size bytes in value
 *   2. Copy min(byte_size, union_size, active_value_size) bytes from active_value to value
 */
void __kain_union_wrap(
    void* value,
    const char* active,
    const char* type_key,
    int64_t byte_size,
    int64_t union_size,
    const void* active_value,
    size_t active_value_size
) {
    /* Active field name and type key are for diagnostics/debugging only */
    (void)active;
    (void)type_key;

    /* Zero out the entire union for deterministic behavior */
    memset(value, 0, (size_t)union_size);

    /* Compute how many bytes to copy */
    size_t copy_span = min3((size_t)byte_size, (size_t)union_size, active_value_size);

    /* Copy bytes from active_value to union */
    memcpy(value, active_value, copy_span);
}
