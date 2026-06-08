/*
 * check_union.c — CBMC verification harness for union module
 *
 * Verifies __kain_union_get, __kain_union_set, and __kain_union_wrap
 * with nondeterministic inputs but valid pointer provenance via static
 * backing buffers.
 *
 * Properties checked:
 *   - No out-of-bounds writes (copy span <= every buffer)
 *   - Get: fallback-seeded output, then partial overwrite from union
 *   - Set/Wrap: zero-fill entire union, then partial write from source
 *   - min3 deterministically clamps to the smallest size
 *   - Zero-length operations are safe
 *   - Negative-size casting through min3 prevents OOB
 *   - Set-then-get roundtrip preserves identity over copy_span
 *
 * Key insight: __CPROVER_havoc_object scrambles every byte but all
 * pointers point into static buffers, giving CBMC real pointer
 * provenance while keeping input data random.
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_union --unwind 5
 * Or:     cbmc --unwind 5 --trace test/cbmc/check_union.c src/core/union.c -I include -I src/core
 */

#include "union.h"
#include <string.h>

/* Maximum union / field size we test with */
#define MAX_UNION_SIZE   64
#define MAX_FIELD_SIZE   32

/* ── Static backing buffers (CBMC knows these are real objects) ── */
static unsigned char union_buf[MAX_UNION_SIZE];
static unsigned char fallback_buf[MAX_UNION_SIZE];
static unsigned char output_buf[MAX_UNION_SIZE];
static unsigned char source_buf[MAX_UNION_SIZE];   /* next / active_value */
static char field_name[16];
static char type_key[16];

/* ──────────────────────────────────────────────────────────────────────
 * Helper: nondeterministic contents for all buffers
 * ────────────────────────────────────────────────────────────────────── */
static void havoc_buffers(void) {
    __CPROVER_havoc_object(union_buf);
    __CPROVER_havoc_object(fallback_buf);
    __CPROVER_havoc_object(output_buf);
    __CPROVER_havoc_object(source_buf);
    __CPROVER_havoc_object(field_name);
    __CPROVER_havoc_object(type_key);
}


/* ──────────────────────────────────────────────────────────────────────
 * Helper: constrain sizes to sane non-negative ranges
 * ────────────────────────────────────────────────────────────────────── */
static void constrain_get_sizes(int64_t* byte_size,
                                int64_t* union_size,
                                size_t*  output_size) {
    __CPROVER_assume(*byte_size   >= 0 && *byte_size   <= MAX_FIELD_SIZE);
    __CPROVER_assume(*union_size  >= 0 && *union_size  <= MAX_UNION_SIZE);
    __CPROVER_assume(*output_size >= 0 && *output_size <= MAX_UNION_SIZE);
}

static void constrain_set_sizes(int64_t* byte_size,
                                int64_t* union_size,
                                size_t*  xfer_size) {
    __CPROVER_assume(*byte_size  >= 0 && *byte_size  <= MAX_FIELD_SIZE);
    __CPROVER_assume(*union_size >= 0 && *union_size <= MAX_UNION_SIZE);
    __CPROVER_assume(*xfer_size  >= 0 && *xfer_size  <= MAX_UNION_SIZE);
}


/* ====================================================================
 * Check 1: __kain_union_get — no OOB, fallback-then-copy semantics
 * ==================================================================== */
void check_union_get_safety(void) {
    havoc_buffers();

    int64_t byte_size;
    int64_t union_size;
    size_t  output_size;
    __CPROVER_havoc_object(&byte_size);
    __CPROVER_havoc_object(&union_size);
    __CPROVER_havoc_object(&output_size);
    constrain_get_sizes(&byte_size, &union_size, &output_size);

    /* Save pre-call output to verify no OOB writes */
    unsigned char pre_output[MAX_UNION_SIZE];
    memcpy(pre_output, output_buf, sizeof(pre_output));

    /* ── Call ── */
    __kain_union_get(union_buf, field_name, type_key,
                     byte_size, union_size,
                     fallback_buf, output_buf, output_size);

    /* Expected copy span = min(byte_size, union_size, output_size) */
    size_t span = (size_t)byte_size;
    if ((size_t)union_size < span) span = (size_t)union_size;
    if (output_size        < span) span = output_size;

    /* 1a. First span bytes copied from union to output */
    if (span > 0) {
        int ok = 1;
        for (size_t i = 0; i < span; i++) {
            if (output_buf[i] != union_buf[i]) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "get: copy_span bytes from union to output");
    }

    /* 1b. Bytes [span, output_size) retain fallback content */
    if (output_size > span) {
        int ok = 1;
        for (size_t i = span; i < output_size; i++) {
            if (output_buf[i] != fallback_buf[i]) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "get: bytes beyond copy_span are fallback");
    }

    /* 1c. No OOB write: output_size..MAX_UNION_SIZE-1 unchanged */
    if (output_size < sizeof(output_buf)) {
        int ok = 1;
        for (size_t i = output_size; i < sizeof(output_buf); i++) {
            if (output_buf[i] != pre_output[i]) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "get: no OOB write past output_size");
    }
}


/* ====================================================================
 * Check 2: __kain_union_set — zero-fill union, then copy from next
 * ==================================================================== */
void check_union_set_safety(void) {
    havoc_buffers();

    int64_t byte_size;
    int64_t union_size;
    size_t  next_size;
    __CPROVER_havoc_object(&byte_size);
    __CPROVER_havoc_object(&union_size);
    __CPROVER_havoc_object(&next_size);
    constrain_set_sizes(&byte_size, &union_size, &next_size);

    unsigned char pre_union[MAX_UNION_SIZE];
    memcpy(pre_union, union_buf, sizeof(pre_union));

    /* ── Call ── */
    __kain_union_set(union_buf, field_name, type_key,
                     byte_size, union_size,
                     source_buf, next_size);

    /* Expected copy span */
    size_t span = (size_t)byte_size;
    if ((size_t)union_size < span) span = (size_t)union_size;
    if (next_size          < span) span = next_size;

    /* 2a. First span bytes copied from source to union */
    if (span > 0) {
        int ok = 1;
        for (size_t i = 0; i < span; i++) {
            if (union_buf[i] != source_buf[i]) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "set: first copy_span bytes from source to union");
    }

    /* 2b. Bytes [span, union_size) are zeroed by the preceding memset */
    if ((size_t)union_size > span) {
        int ok = 1;
        size_t limit = (size_t)union_size < sizeof(union_buf)
                       ? (size_t)union_size : sizeof(union_buf);
        for (size_t i = span; i < limit; i++) {
            if (union_buf[i] != 0) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "set: bytes beyond copy_span within union_size zeroed");
    }

    /* 2c. No OOB write: bytes past union_size unchanged */
    if ((size_t)union_size < sizeof(union_buf)) {
        int ok = 1;
        for (size_t i = (size_t)union_size; i < sizeof(union_buf); i++) {
            if (union_buf[i] != pre_union[i]) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "set: no OOB write past union_size");
    }
}


/* ====================================================================
 * Check 3: __kain_union_wrap — same zero-then-copy as set
 * ==================================================================== */
void check_union_wrap_safety(void) {
    havoc_buffers();

    int64_t byte_size;
    int64_t union_size;
    size_t  active_size;
    __CPROVER_havoc_object(&byte_size);
    __CPROVER_havoc_object(&union_size);
    __CPROVER_havoc_object(&active_size);
    constrain_set_sizes(&byte_size, &union_size, &active_size);

    unsigned char pre_union[MAX_UNION_SIZE];
    memcpy(pre_union, union_buf, sizeof(pre_union));

    /* ── Call ── */
    __kain_union_wrap(union_buf, field_name, type_key,
                      byte_size, union_size,
                      source_buf, active_size);

    size_t span = (size_t)byte_size;
    if ((size_t)union_size < span) span = (size_t)union_size;
    if (active_size        < span) span = active_size;

    /* 3a. First span bytes copied from source */
    if (span > 0) {
        int ok = 1;
        for (size_t i = 0; i < span; i++) {
            if (union_buf[i] != source_buf[i]) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "wrap: first copy_span bytes from active_value");
    }

    /* 3b. Bytes [span, union_size) are zeroed */
    if ((size_t)union_size > span) {
        int ok = 1;
        size_t limit = (size_t)union_size < sizeof(union_buf)
                       ? (size_t)union_size : sizeof(union_buf);
        for (size_t i = span; i < limit; i++) {
            if (union_buf[i] != 0) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "wrap: bytes beyond copy_span within union_size zeroed");
    }

    /* 3c. No OOB write past union_size */
    if ((size_t)union_size < sizeof(union_buf)) {
        int ok = 1;
        for (size_t i = (size_t)union_size; i < sizeof(union_buf); i++) {
            if (union_buf[i] != pre_union[i]) { ok = 0; break; }
        }
        __CPROVER_assert(ok, "wrap: no OOB write past union_size");
    }
}


/* ====================================================================
 * Check 4: Set-then-get roundtrip preserves identity
 *
 * Write a deterministic pattern into the union via __kain_union_set,
 * read it back via __kain_union_get, and verify the output matches
 * the original source over the copy span.
 * ==================================================================== */
void check_union_roundtrip(void) {
    havoc_buffers();

    int64_t byte_size;
    int64_t union_size;
    size_t  xfer_size;
    __CPROVER_havoc_object(&byte_size);
    __CPROVER_havoc_object(&union_size);
    __CPROVER_havoc_object(&xfer_size);
    __CPROVER_assume(byte_size  >= 0 && byte_size  <= MAX_FIELD_SIZE);
    __CPROVER_assume(union_size >= 0 && union_size <= MAX_UNION_SIZE);
    __CPROVER_assume(xfer_size  >= 0 && xfer_size  <= MAX_UNION_SIZE);

    /* Deterministic pattern visible to CBMC */
    for (size_t i = 0; i < sizeof(source_buf); i++) {
        source_buf[i] = (unsigned char)(0xA0 + (i & 0x0F));
    }

    /* Clear output before roundtrip */
    memset(output_buf, 0, sizeof(output_buf));

    /* ── Step 1: set union from source ── */
    __kain_union_set(union_buf, field_name, type_key,
                     byte_size, union_size,
                     source_buf, xfer_size);

    /* ── Step 2: read union into output ── */
    __kain_union_get(union_buf, field_name, type_key,
                     byte_size, union_size,
                     fallback_buf, output_buf, xfer_size);

    size_t span = (size_t)byte_size;
    if ((size_t)union_size < span) span = (size_t)union_size;
    if (xfer_size          < span) span = xfer_size;

    if (span > 0) {
        int ok = 1;
        for (size_t i = 0; i < span; i++) {
            if (output_buf[i] != (unsigned char)(0xA0 + (i & 0x0F))) {
                ok = 0; break;
            }
        }
        __CPROVER_assert(ok, "roundtrip: set-then-get preserves identity");
    }
}


/* ====================================================================
 * Check 5: Zero-size operations are memory-safe
 *
 * memcpy and memset with size 0 are defined as no-ops by the C
 * standard. Verify all three functions tolerate zero-length sizes
 * without crashing or producing side effects beyond the buffers.
 * ==================================================================== */
void check_union_zero_sizes(void) {
    __kain_union_get(union_buf, field_name, type_key,
                     0, 0, fallback_buf, output_buf, 0);
    __kain_union_set(union_buf, field_name, type_key,
                     0, 0, source_buf, 0);
    __kain_union_wrap(union_buf, field_name, type_key,
                      0, 0, source_buf, 0);
    /* If we reach here, no crash — memcpy/memset with size 0 is valid */
}


/* ====================================================================
 * Check 6: Negative-size clamping via min3
 *
 * When int64_t parameters are negative, casting to size_t wraps to
 * huge values. min3 must clamp against the other (small) sizes to
 * prevent OOB.
 *
 * We constrain union_size to non-negative here — the real compiler
 * never passes a negative union_size (it is computed from layout).
 * byte_size is allowed to be negative to test the min3 casting path.
 * ==================================================================== */
void check_union_negative_byte_size(void) {
    havoc_buffers();

    int64_t neg_byte;
    int64_t pos_union;
    size_t  small_size;

    __CPROVER_havoc_object(&neg_byte);
    __CPROVER_havoc_object(&pos_union);
    __CPROVER_havoc_object(&small_size);
    __CPROVER_assume(neg_byte   < 0);
    __CPROVER_assume(pos_union >= 0 && pos_union <= 32);
    __CPROVER_assume(small_size > 0 && small_size <= 16);

    /*
     * For __kain_union_get:
     *   memcpy(output, fallback, small_size)        — fine, small_size <= 16
     *   copy_span = min3(huge, pos_union, small)     = min(pos_union, small)
     *   memcpy(output, value, copy_span)             — bounds: copy_span <= 16
     */
    __kain_union_get(union_buf, field_name, type_key,
                     neg_byte, pos_union,
                     fallback_buf, output_buf, small_size);

    /*
     * For __kain_union_set:
     *   memset(value, 0, pos_union)                  — fine, pos_union <= 32
     *   copy_span = min3(huge, pos_union, small)     = min(pos_union, small)
     *   memcpy(value, next, copy_span)               — bounds: copy_span <= 16
     */
    __kain_union_set(union_buf, field_name, type_key,
                     neg_byte, pos_union,
                     source_buf, small_size);

    /*
     * For __kain_union_wrap: same reasoning as set.
     */
    __kain_union_wrap(union_buf, field_name, type_key,
                      neg_byte, pos_union,
                      source_buf, small_size);

    /*
     * CBMC's built-in bounds checking proves that all memcpy/memset
     * accesses stay within the static buffer bounds. If any access
     * went out of bounds, CBMC would produce a counterexample at
     * the offending instruction.
     */
}


/* ====================================================================
 * Main — run all checks
 * ==================================================================== */
int main(void) {
    /* All buffers start nondeterministic */
    __CPROVER_havoc_object(union_buf);
    __CPROVER_havoc_object(fallback_buf);
    __CPROVER_havoc_object(output_buf);
    __CPROVER_havoc_object(source_buf);
    __CPROVER_havoc_object(field_name);
    __CPROVER_havoc_object(type_key);

    check_union_get_safety();
    check_union_set_safety();
    check_union_wrap_safety();
    check_union_roundtrip();
    check_union_zero_sizes();
    check_union_negative_byte_size();

    return 0;
}
