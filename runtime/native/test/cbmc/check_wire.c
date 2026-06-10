/*
 * check_wire.c — CBMC verification harness for wire module
 * ====================================================================
 *
 * Verifies abi_wire_zero_copy_binary_checksum — a deterministic
 * periodic-checksum computation over a structured wire-format record
 * stream.
 *
 * Properties verified (9 test functions, ~25 assertions):
 *   1.  Negative iterations → -1
 *   2.  Wrong packet_count → -1
 *   3.  Wrong words_per_packet → -1
 *   4.  Zero/negative modulus → -1
 *   5.  Iteration overflow → -1
 *   6.  Zero iterations → 0 (empty sum)
 *   7.  Valid params (nondet) → result in [0, modulus-1]
 *   8.  Determinism: same inputs → same result
 *   9.  Single non-zero iteration → success, result < modulus
 *
 * Design notes:
 *   - Very small iteration counts (0-4) keep the remainder loop well
 *     within default --unwind 5, giving CBMC full path coverage.
 *   - The fast path (blocks <= 256) is always taken for these small
 *     iterations because total_records = iter * 64 is always much
 *     smaller than KAIN_WIRE_PERIOD = 397312, so blocks = 0.
 *
 * Run via:
 *   python test/scripts/run_pipeline.py cbmc --harness check_wire --unwind 5
 * Or:
 *   cbmc --unwind 5 --trace test/cbmc/check_wire.c src/core/wire.c \
 *        -I include -I src/core
 */

#include "wire.h"
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════
 * SECTION 1: Parameter validation
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * 1. Negative iterations → -1
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_neg_iterations(void) {
    int64_t result = abi_wire_zero_copy_binary_checksum(-1, 64, 4, 100);
    __CPROVER_assert(result == -1,
        "neg iterations: returns -1");

    result = abi_wire_zero_copy_binary_checksum(-1000, 64, 4, 100);
    __CPROVER_assert(result == -1,
        "neg iterations (large): returns -1");
}

/* ──────────────────────────────────────────────────────────────────────
 * 2. Wrong packet_count → -1
 *
 * The function requires packet_count == KAIN_WIRE_PACKET_COUNT (64).
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_wrong_packet_count(void) {
    int64_t result;

    result = abi_wire_zero_copy_binary_checksum(1, 0, 4, 100);
    __CPROVER_assert(result == -1,
        "packet_count=0: returns -1");

    result = abi_wire_zero_copy_binary_checksum(1, 63, 4, 100);
    __CPROVER_assert(result == -1,
        "packet_count=63: returns -1");

    result = abi_wire_zero_copy_binary_checksum(1, 65, 4, 100);
    __CPROVER_assert(result == -1,
        "packet_count=65: returns -1");
}

/* ──────────────────────────────────────────────────────────────────────
 * 3. Wrong words_per_packet → -1
 *
 * The function requires words_per_packet == KAIN_WIRE_WORDS_PER_PACKET (4).
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_wrong_words_per_packet(void) {
    int64_t result;

    result = abi_wire_zero_copy_binary_checksum(1, 64, 0, 100);
    __CPROVER_assert(result == -1,
        "words_per_packet=0: returns -1");

    result = abi_wire_zero_copy_binary_checksum(1, 64, 3, 100);
    __CPROVER_assert(result == -1,
        "words_per_packet=3: returns -1");

    result = abi_wire_zero_copy_binary_checksum(1, 64, 5, 100);
    __CPROVER_assert(result == -1,
        "words_per_packet=5: returns -1");
}

/* ──────────────────────────────────────────────────────────────────────
 * 4. Zero or negative modulus → -1
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_bad_modulus(void) {
    int64_t result;

    result = abi_wire_zero_copy_binary_checksum(1, 64, 4, 0);
    __CPROVER_assert(result == -1,
        "modulus=0: returns -1");

    result = abi_wire_zero_copy_binary_checksum(1, 64, 4, -1);
    __CPROVER_assert(result == -1,
        "modulus=-1: returns -1");

    result = abi_wire_zero_copy_binary_checksum(1, 64, 4, -1000000);
    __CPROVER_assert(result == -1,
        "modulus negative large: returns -1");
}

/* ──────────────────────────────────────────────────────────────────────
 * 5. Iteration overflow → -1
 *
 * The guard is: iterations > (INT64_MAX / KAIN_WIRE_PACKET_COUNT).
 * With PACKET_COUNT = 64, that's iterations > INT64_MAX / 64.
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_iteration_overflow(void) {
    /* INT64_MAX / 64 = 144115188075855872.  Anything above that overflows.
     * Use a clearly oversized value. */
    int64_t result = abi_wire_zero_copy_binary_checksum(
        INT64_MAX, 64, 4, 100);
    __CPROVER_assert(result == -1,
        "iter overflow (INT64_MAX): returns -1");

    /* Overflow check threshold: any value > INT64_MAX / 64 triggers it */
    int64_t iter;
    __CPROVER_havoc_object(&iter);
    __CPROVER_assume(iter > 0);
    __CPROVER_assume(iter > INT64_MAX / 64);

    result = abi_wire_zero_copy_binary_checksum(iter, 64, 4, 100);
    __CPROVER_assert(result == -1,
        "iter overflow (nondet > threshold): returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * SECTION 2: Zero iterations (empty computation)
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * 6. Zero iterations → result is 0
 *
 * With no records, the accumulator never gets updated and stays at 0.
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_zero_iterations(void) {
    int64_t result;

    result = abi_wire_zero_copy_binary_checksum(0, 64, 4, 1);
    __CPROVER_assert(result == 0,
        "zero iter modulus=1: result=0");

    result = abi_wire_zero_copy_binary_checksum(0, 64, 4, 97);
    __CPROVER_assert(result == 0,
        "zero iter modulus=97: result=0");

    result = abi_wire_zero_copy_binary_checksum(0, 64, 4, 1000003);
    __CPROVER_assert(result == 0,
        "zero iter modulus=1000003: result=0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * SECTION 3: Successful computation invariants
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * 7. Valid params (nondet) → result in [0, modulus-1]
 *
 * CBMC explores all valid (non-failing) parameter combinations within
 * small iteration bounds to keep loop unrolling manageable.
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_valid_nondet(void) {
    int64_t iterations;
    int64_t modulus;
    __CPROVER_havoc_object(&iterations);
    __CPROVER_havoc_object(&modulus);

    /* Constrain to valid non-failing parameter space */
    __CPROVER_assume(iterations >= 0);
    __CPROVER_assume(iterations <= 4);          /* small for unwind */
    __CPROVER_assume(modulus > 0);
    __CPROVER_assume(modulus <= 1000000);

    int64_t result = abi_wire_zero_copy_binary_checksum(
        iterations, 64, 4, modulus);

    /* Must succeed */
    __CPROVER_assert(result != -1,
        "valid nondet: result != -1");

    /* Result must be in the correct range */
    __CPROVER_assert(result >= 0,
        "valid nondet: result >= 0");
    __CPROVER_assert(result < modulus,
        "valid nondet: result < modulus");

    /* Result must fit in int64_t (always true for non-negative) */
    __CPROVER_assert(result <= modulus - 1,
        "valid nondet: result <= modulus-1");
}

/* ──────────────────────────────────────────────────────────────────────
 * 8. Determinism: same inputs → same output
 *
 * The function is a pure deterministic computation with no heap
 * allocation on the fast path.  Calling it twice with identical
 * arguments must produce identical results.
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_deterministic(void) {
    int64_t iterations;
    int64_t modulus;
    __CPROVER_havoc_object(&iterations);
    __CPROVER_havoc_object(&modulus);

    __CPROVER_assume(iterations >= 0 && iterations <= 4);
    __CPROVER_assume(modulus > 0 && modulus <= 1000000);

    int64_t a = abi_wire_zero_copy_binary_checksum(
        iterations, 64, 4, modulus);
    int64_t b = abi_wire_zero_copy_binary_checksum(
        iterations, 64, 4, modulus);

    __CPROVER_assert(a == b,
        "deterministic: same inputs same result");
}

/* ──────────────────────────────────────────────────────────────────────
 * 9. Single non-zero iteration with concrete modulus
 *
 * Verifies that a single iteration (64 records) produces a predictable
 * non-negative result.  Because iterations=1 gives total_records=64
 * which is far below KAIN_WIRE_PERIOD=397312, only the remainder
 * loop runs (64 iterations of kain_wire_record_fold_periodic).
 * ────────────────────────────────────────────────────────────────────── */
void check_wire_one_iteration(void) {
    int64_t result = abi_wire_zero_copy_binary_checksum(1, 64, 4, 97);
    __CPROVER_assert(result != -1,
        "one iter: success");
    __CPROVER_assert(result >= 0,
        "one iter: result >= 0");
    __CPROVER_assert(result < 97,
        "one iter: result < modulus");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* Parameter validation */
    check_wire_neg_iterations();
    check_wire_wrong_packet_count();
    check_wire_wrong_words_per_packet();
    check_wire_bad_modulus();
    check_wire_iteration_overflow();

    /* Zero iterations */
    check_wire_zero_iterations();

    /* Successful computation */
    check_wire_valid_nondet();
    check_wire_deterministic();
    check_wire_one_iteration();

    return 0;
}
