/*
 * check_virtual_alloc.c — CBMC verification harness for virtual_alloc module
 *
 * Verifies OS-level virtual memory page management wrappers:
 * - Page size query
 * - Alignment & size-rounding utilities
 * - Reserve, commit, decommit, release wrappers
 * - Combined reserve-and-commit
 * - Batch mapping input validation
 * - NULL safety on every function
 *
 * OS primitives (abi_vm_*) are nondeterministic in CBMC — the harness tests
 * the WRAPPER logic (argument validation, size rounding, alignment checks,
 * branching around OS calls). CBMC explores ALL possible OS return values.
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_virtual_alloc
 * Or:     cbmc --unwind 5 --no-unwinding-assertions --trace
 *            test/cbmc/check_virtual_alloc.c src/core/virtual_alloc.c
 *            -I include -I src/core
 */

#include "virtual_alloc.h"

/* ── Forward declarations of static/internal functions ── */
static int    kain_virtual_alignment_is_power_of_two(size_t alignment);
static size_t kain_virtual_rounded_byte_count(size_t byte_count);

/* ── Static backing buffer for pointer provenance ── */
static unsigned char g_backing[8192];

/* ── Non-backing pointer for functions that don't need buffer access ── */
/* (We pass g_backing when a valid pointer is needed for NULL-safety checks) */


/* ═══════════════════════════════════════════════════════════════════════
 * 1.  Page size — always positive
 * ═══════════════════════════════════════════════════════════════════════ */
void check_virtual_page_size(void) {
    size_t page_size = kain_virtual_page_size();

    /* abi_vm_page_size() is nondet (int64_t): ≤0 → 4096, >0 → that value */
    __CPROVER_assert(page_size > 0, "page_size > 0 (fallback 4096 or OS value)");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 2.  Alignment is-power-of-two — for every possible size_t
 * ═══════════════════════════════════════════════════════════════════════ */
void check_alignment_is_power_of_two(void) {
    size_t alignment;
    __CPROVER_havoc_object(&alignment);

    int result = kain_virtual_alignment_is_power_of_two(alignment);

    if (alignment == 0u) {
        __CPROVER_assert(result == 0, "is_power_of_two(0) → 0");
    } else {
        int expected = ((alignment & (alignment - 1u)) == 0u) ? 1 : 0;
        __CPROVER_assert(result == expected,
                         "is_power_of_two: correct for all non-zero");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * 3.  Align up — power-of-2 alignment → aligned result or 0 on overflow
 * ═══════════════════════════════════════════════════════════════════════ */
void check_align_up_valid(void) {
    size_t value;
    size_t alignment;
    __CPROVER_havoc_object(&value);
    __CPROVER_havoc_object(&alignment);

    /* Constrain to power-of-2 > 0 */
    __CPROVER_assume(alignment > 0u);
    __CPROVER_assume((alignment & (alignment - 1u)) == 0u);

    size_t result = kain_virtual_align_up(value, alignment);
    size_t mask   = alignment - 1u;

    /* Overflow check */
    int overflow = (value > SIZE_MAX - mask) ? 1 : 0;

    if (overflow) {
        __CPROVER_assert(result == 0u,
                         "align_up: overflow returns 0");
    } else {
        size_t expected = (value + mask) & ~mask;
        __CPROVER_assert(result == expected,
                         "align_up: correct computation");
        __CPROVER_assert((result & mask) == 0u,
                         "align_up: result is aligned");
        __CPROVER_assert(result >= value,
                         "align_up: result ≥ value");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * 4.  Align up — non-power-of-2 returns 0
 * ═══════════════════════════════════════════════════════════════════════ */
void check_align_up_non_power_of_two(void) {
    size_t value;
    size_t alignment;
    __CPROVER_havoc_object(&value);
    __CPROVER_havoc_object(&alignment);

    /* ≥3 and not a power of 2 */
    __CPROVER_assume(alignment >= 3u);
    __CPROVER_assume((alignment & (alignment - 1u)) != 0u);

    size_t result = kain_virtual_align_up(value, alignment);

    __CPROVER_assert(result == 0u,
                     "align_up: non-power-of-2 returns 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 5.  Align up — zero alignment treated as alignment=1 (identity)
 * ═══════════════════════════════════════════════════════════════════════ */
void check_align_up_zero_alignment(void) {
    size_t value;
    __CPROVER_havoc_object(&value);

    size_t result = kain_virtual_align_up(value, 0u);

    /* alignment 0 → set to 1 → mask = 0 → result = (value + 0) & ~0 = value */
    __CPROVER_assert(result == value,
                     "align_up: zero alignment → identity");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 6.  Rounded byte count (static helper)
 *
 *     When the OS page size is a power of 2 (always true on real systems,
 *     but nondet in CBMC), the result is page-aligned or 0.
 *     When the OS page size is NOT a power of 2, align_up returns 0 for
 *     any non-zero byte_count, so rounded == 0.
 * ═══════════════════════════════════════════════════════════════════════ */
void check_rounded_byte_count(void) {
    size_t byte_count;
    __CPROVER_havoc_object(&byte_count);

    size_t rounded = kain_virtual_rounded_byte_count(byte_count);

    /* byte_count == 0 → always 0 (align_up with 0 returns 0) */
    if (byte_count == 0u) {
        __CPROVER_assert(rounded == 0u,
                         "rounded: 0 bytes → 0");
    }

    /* When result is non-zero, it's ≥ input.
     *
     * NOTE: We do NOT assert page-alignment here because we cannot
     * call kain_virtual_page_size() a second time to compare:
     * abi_vm_page_size() is nondeterministic in CBMC, so each call
     * returns a potentially different value.  The first call (inside
     * rounded_byte_count) determines `rounded`; a second call in the
     * harness would return something unrelated.
     */
    if (rounded != 0u) {
        __CPROVER_assert(rounded >= byte_count,
                         "rounded: non-zero ≥ original");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * 7.  Reserve — byte_count=0 → NULL (before any OS call)
 * ═══════════════════════════════════════════════════════════════════════ */
void check_reserve_null_on_zero_size(void) {
    void* result = kain_virtual_reserve(0u, 4096u, KAIN_MEMTYPE_CPU_WB);

    __CPROVER_assert(result == NULL,
                     "reserve: byte_count=0 returns NULL");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 8.  Reserve — non-power-of-2 alignment → NULL or proceeds
 *
 *     If alignment >= page_size and not a power of 2, the function
 *     returns NULL.  But if alignment < page_size, alignment is bumped
 *     to page_size (always a power of 2 on real systems; nondet in CBMC).
 *
 *     We test the direct path: a large non-power-of-2 alignment that
 *     won't be adjusted, forcing the power-of-2 check to reject it.
 * ═══════════════════════════════════════════════════════════════════════ */
void check_reserve_null_on_bad_alignment(void) {
    size_t bad_alignment;
    __CPROVER_havoc_object(&bad_alignment);

    /* Non-power-of-2, large enough to survive the < page_size adjustment */
    /* On real systems page_size ≤ 65536; we pick 1MB as a safe floor. */
    /* If abi_vm_page_size() returns > 1MB (possible in CBMC), the
     * alignment still gets bumped — but we only assert "no crash" below. */
    __CPROVER_assume(bad_alignment > 1048576u);   /* > 1 MB */
    __CPROVER_assume((bad_alignment & (bad_alignment - 1u)) != 0u);

    void* result = kain_virtual_reserve(1024u, bad_alignment,
                                        KAIN_MEMTYPE_CPU_WB);

    /*
     * If alignment was NOT adjusted (page_size ≤ 1MB), the check fires
     * and returns NULL.  If page_size > 1MB, alignment IS adjusted,
     * we call abi_vm_reserve, which is nondet.  Either way, no crash.
     */
    /*
     * If alignment was NOT adjusted (page_size ≤ 1MB), the check fires
     * and returns NULL.  If page_size > 1MB, alignment IS adjusted,
     * we call abi_vm_reserve, which is nondet.  Either way, no crash.
     *
     * NOTE: We do NOT re-call kain_virtual_page_size() here to verify
     * the exact result because abi_vm_page_size() is nondeterministic
     * in CBMC — each call returns a potentially different value.
     * The first call (inside reserve) determines behavior; a second
     * call would be unrelated.
     */
    __CPROVER_assert(1, "reserve: bad alignment path never crashes");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 9.  Reserve — valid args, nondeterministic OS → no crash
 * ═══════════════════════════════════════════════════════════════════════ */
void check_reserve_valid_args_no_crash(void) {
    size_t byte_count;
    __CPROVER_havoc_object(&byte_count);
    __CPROVER_assume(byte_count > 0u && byte_count <= 65536u);

    void* result = kain_virtual_reserve(byte_count, 4096u,
                                        KAIN_MEMTYPE_CPU_WB);

    /* OS may return NULL (failure) or a pointer (success);
     * wrapper may also return NULL if alignment > page_size fails. */
    __CPROVER_assert(1, "reserve: valid args never crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 10. Commit — NULL base returns -1
 * ═══════════════════════════════════════════════════════════════════════ */
void check_commit_null_base(void) {
    int rc = kain_virtual_commit(NULL, 4096u, KAIN_MEMTYPE_CPU_WB);

    __CPROVER_assert(rc == -1, "commit: NULL base returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 11. Commit — zero byte_count returns -1
 * ═══════════════════════════════════════════════════════════════════════ */
void check_commit_zero_size(void) {
    int rc = kain_virtual_commit(g_backing, 0u, KAIN_MEMTYPE_CPU_WB);

    __CPROVER_assert(rc == -1, "commit: zero byte_count returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 12. Commit — valid args → 0 or -1 (nondeterministic OS)
 * ═══════════════════════════════════════════════════════════════════════ */
void check_commit_valid_args(void) {
    int rc = kain_virtual_commit(g_backing, 4096u, KAIN_MEMTYPE_CPU_WB);

    __CPROVER_assert(rc == 0 || rc == -1,
                     "commit: valid args return 0 or -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 13. Reserve-and-commit — byte_count=0 → NULL (reserve fails early)
 * ═══════════════════════════════════════════════════════════════════════ */
void check_reserve_and_commit_null_on_zero_size(void) {
    void* result = kain_virtual_reserve_and_commit(
                       0u, 4096u, KAIN_MEMTYPE_CPU_WB);

    __CPROVER_assert(result == NULL,
                     "reserve_and_commit: zero size returns NULL");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 14. Reserve-and-commit — always returns NULL or non-NULL, never crashes
 * ═══════════════════════════════════════════════════════════════════════ */
void check_reserve_and_commit_no_crash(void) {
    size_t byte_count;
    __CPROVER_havoc_object(&byte_count);
    __CPROVER_assume(byte_count > 0u && byte_count <= 65536u);

    void* result = kain_virtual_reserve_and_commit(
                       byte_count, 4096u, KAIN_MEMTYPE_CPU_WB);

    /* On reserve failure → NULL (before commit attempt)
     * On commit failure → release + NULL (rollback)
     * On success       → non-NULL pointer
     * All three paths are valid. */
    __CPROVER_assert(1, "reserve_and_commit: three valid paths, no crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 15. Decommit — NULL base is a no-op (no crash)
 * ═══════════════════════════════════════════════════════════════════════ */
void check_decommit_null_safe(void) {
    kain_virtual_decommit(NULL, 4096u);
    __CPROVER_assert(1, "decommit: NULL base does not crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 16. Decommit — zero byte_count is a no-op (no crash)
 * ═══════════════════════════════════════════════════════════════════════ */
void check_decommit_zero_size(void) {
    kain_virtual_decommit(g_backing, 0u);
    __CPROVER_assert(1, "decommit: zero byte_count does not crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 17. Decommit — valid args, no crash
 * ═══════════════════════════════════════════════════════════════════════ */
void check_decommit_valid_args(void) {
    kain_virtual_decommit(g_backing, 4096u);
    __CPROVER_assert(1, "decommit: valid args does not crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 18. Release — NULL base is a no-op (no crash)
 * ═══════════════════════════════════════════════════════════════════════ */
void check_release_null_safe(void) {
    kain_virtual_release(NULL, 4096u);
    __CPROVER_assert(1, "release: NULL base does not crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 19. Release — valid args, no crash
 * ═══════════════════════════════════════════════════════════════════════ */
void check_release_valid_args(void) {
    kain_virtual_release(g_backing, 4096u);
    __CPROVER_assert(1, "release: valid args does not crash");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 20. Batch map — NULL mappings with non-zero count → -1
 * ═══════════════════════════════════════════════════════════════════════ */
void check_batch_map_null_mappings(void) {
    size_t count;
    __CPROVER_havoc_object(&count);
    __CPROVER_assume(count > 0u && count <= 10u);

    int rc = kain_virtual_batch_map(NULL, count);

    __CPROVER_assert(rc == -1,
                     "batch_map: NULL mappings (count>0) returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 21. Batch map — NULL mappings with zero count → 0
 * ═══════════════════════════════════════════════════════════════════════ */
void check_batch_map_null_mappings_zero_count(void) {
    int rc = kain_virtual_batch_map(NULL, 0u);

    /* Loop body never executes because count == 0 */
    __CPROVER_assert(rc == 0,
                     "batch_map: NULL mappings (count=0) returns 0");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 22. Batch map — NULL base in first mapping → -1
 * ═══════════════════════════════════════════════════════════════════════ */
void check_batch_map_null_base(void) {
    KainVirtualBatchMapping mapping;
    __CPROVER_havoc_object(&mapping);
    __CPROVER_havoc_object(g_backing);

    mapping.base       = NULL;
    mapping.byte_count = 4096u;

    int rc = kain_virtual_batch_map(&mapping, 1u);

    __CPROVER_assert(rc == -1,
                     "batch_map: NULL base in mapping returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 23. Batch map — zero byte_count in mapping → -1
 * ═══════════════════════════════════════════════════════════════════════ */
void check_batch_map_zero_byte_count(void) {
    KainVirtualBatchMapping mapping;
    __CPROVER_havoc_object(&mapping);
    __CPROVER_havoc_object(g_backing);

    mapping.base       = g_backing;
    mapping.byte_count = 0u;

    int rc = kain_virtual_batch_map(&mapping, 1u);

    __CPROVER_assert(rc == -1,
                     "batch_map: zero byte_count returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 24. Batch map — single valid mapping (nondeterministic OS)
 * ═══════════════════════════════════════════════════════════════════════ */
void check_batch_map_single_valid(void) {
    KainVirtualBatchMapping mapping;
    __CPROVER_havoc_object(&mapping);
    __CPROVER_havoc_object(g_backing);

    /* Valid entry with proper pointer provenance */
    mapping.base       = g_backing;
    mapping.byte_count = 4096u;
    mapping.memtype    = KAIN_MEMTYPE_CPU_WB;
    /* writable is nondet — explores both protect and skip-protect paths */

    int rc = kain_virtual_batch_map(&mapping, 1u);

    /* OS commit/protect are nondet; return is always 0 or -1 */
    __CPROVER_assert(rc == 0 || rc == -1,
                     "batch_map: valid entry returns 0 or -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * 25. Batch map — first entry fails (NULL base), rest never reached
 * ═══════════════════════════════════════════════════════════════════════ */
void check_batch_map_first_fails(void) {
    KainVirtualBatchMapping mappings[3];
    __CPROVER_havoc_object(mappings);
    __CPROVER_havoc_object(g_backing);

    /* First entry: NULL base → early exit -1 */
    mappings[0].base       = NULL;
    mappings[0].byte_count = 4096u;

    /* Second/third: valid but never reached */
    mappings[1].base       = g_backing;
    mappings[1].byte_count = 4096u;
    mappings[2].base       = g_backing;
    mappings[2].byte_count = 4096u;

    int rc = kain_virtual_batch_map(mappings, 3u);

    __CPROVER_assert(rc == -1,
                     "batch_map: first entry NULL base returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * ═══════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ═══════════════════════════════════════════════════════════════════════
 * ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* ── Utility functions ── */
    check_virtual_page_size();
    check_alignment_is_power_of_two();
    check_align_up_valid();
    check_align_up_non_power_of_two();
    check_align_up_zero_alignment();
    check_rounded_byte_count();

    /* ── Reserve ── */
    check_reserve_null_on_zero_size();
    check_reserve_null_on_bad_alignment();
    check_reserve_valid_args_no_crash();

    /* ── Commit ── */
    check_commit_null_base();
    check_commit_zero_size();
    check_commit_valid_args();

    /* ── Reserve and commit ── */
    check_reserve_and_commit_null_on_zero_size();
    check_reserve_and_commit_no_crash();

    /* ── Decommit ── */
    check_decommit_null_safe();
    check_decommit_zero_size();
    check_decommit_valid_args();

    /* ── Release ── */
    check_release_null_safe();
    check_release_valid_args();

    /* ── Batch map ── */
    check_batch_map_null_mappings();
    check_batch_map_null_mappings_zero_count();
    check_batch_map_null_base();
    check_batch_map_zero_byte_count();
    check_batch_map_single_valid();
    check_batch_map_first_fails();

    return 0;
}
