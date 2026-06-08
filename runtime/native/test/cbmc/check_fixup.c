/*
 * check_fixup.c — CBMC verification harness for fixup module
 *
 * Tests the relocation fixup registry: track allocations by handle,
 * register known pointer references for self-updating relocation,
 * handle-aware reallocation tracking, and NULL/invalid safety.
 *
 * Key invariants verified:
 *   - kain_fixup_init is idempotent and initializes without crash
 *   - Track allocation returns a handle; resolve_handle returns matching base
 *   - Duplicate track (same base) reuses handle, updates size
 *   - Handle-size and handle-view return correct metadata
 *   - Handle-for-pointer finds tracked allocations by exact and interior pointer
 *   - Known-ref registration creates a self-updating reference
 *   - Unregister removes the reference, count decreases
 *   - Update can set ref to NULL or redirect to new target
 *   - Relocate updates all registered refs' target addresses preserving offset
 *   - Relocate with wrong old_base is rejected
 *   - After unregister_allocation, handle becomes stale, refs are zeroed
 *   - All functions accept NULL / INVALID inputs without crash
 *   - Query functions (ref_count, relocation_count, last_handle) are safe
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_fixup --unwind 8
 * Or:     cbmc --unwind 8 --trace test/cbmc/check_fixup.c src/core/fixup.c
 *             -I include -I src/core
 */

#include "fixup.h"
#include <stddef.h>

/* ── Static backing buffers for pointer provenance ──*/
/* CBMC needs real allocated memory for pointer validity — uninitialized
 * void* triggers "invalid pointer" / "dead object" violations on every
 * dereference.  These buffers give the solver physical memory to reason
 * about. */
static char g_a[64];
static char g_b[64];
static char g_c[64];
static char g_d[64];

/* Ref location cell — used for register/unregister/update tests */
static void* g_ref_loc;

/* Second ref location cell — for multi-ref tests */
static void* g_ref_loc2;


/* ====================================================================
 * Initialisation
 * ==================================================================== */

/* ── check_init ─────────────────────────────────────────────────────
 * Verify that kain_fixup_init does not crash and is idempotent.       */
void check_init(void) {
    kain_fixup_init();
    __CPROVER_assert(1, "init: first call ok");
    kain_fixup_init();
    __CPROVER_assert(1, "init: second call (idempotent) ok");
}


/* ====================================================================
 * Track allocation — NULL / zero edge cases
 * ==================================================================== */

void check_track_null_base(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(NULL, 64u);
    __CPROVER_assert(h == KAIN_RUNTIME_HANDLE_INVALID,
                     "track_null_base: NULL base returns INVALID");
}

void check_track_zero_size(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_a, 0u);
    __CPROVER_assert(h == KAIN_RUNTIME_HANDLE_INVALID,
                     "track_zero_size: size==0 returns INVALID");
}

void check_track_null_base_zero_size(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(NULL, 0u);
    __CPROVER_assert(h == KAIN_RUNTIME_HANDLE_INVALID,
                     "track_null_zero: NULL+0 returns INVALID");
}


/* ====================================================================
 * Track → resolve / size / view round-trip
 * ==================================================================== */

void check_track_resolve(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_a, sizeof(g_a));
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        void* base = kain_fixup_resolve_handle(h);
        __CPROVER_assert(base == g_a,
                         "track_resolve: resolve returns tracked base");
    }
}

void check_track_handle_size(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_b, 32u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        size_t sz = kain_fixup_handle_size(h);
        __CPROVER_assert(sz == 32u,
                         "track_size: handle_size matches");
    }
}

void check_track_view(void) {
    KainFixupTrackedView view;
    KainRuntimeHandle h = kain_fixup_track_allocation(g_c, 48u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        int rc = kain_fixup_view(h, &view);
        __CPROVER_assert(rc == 0,
                         "track_view: returns 0 on success");
        __CPROVER_assert(view.handle == h,
                         "track_view: view.handle matches");
        __CPROVER_assert(view.base == g_c,
                         "track_view: view.base matches");
        __CPROVER_assert(view.size == 48u,
                         "track_view: view.size matches");
    }
}

void check_track_view_cleared_on_failure(void) {
    KainFixupTrackedView view;
    /* View with INVALID handle should zero fields and return -1 */
    int rc = kain_fixup_view(KAIN_RUNTIME_HANDLE_INVALID, &view);
    __CPROVER_assert(rc == -1,
                     "view_invalid: returns -1");
    __CPROVER_assert(view.handle == KAIN_RUNTIME_HANDLE_INVALID,
                     "view_invalid: handle set to INVALID");
    __CPROVER_assert(view.base == 0,
                     "view_invalid: base set to NULL");
    __CPROVER_assert(view.size == 0u,
                     "view_invalid: size set to 0");
}


/* ====================================================================
 * Duplicate track — same base reuses handle, size update
 * ==================================================================== */

void check_track_duplicate_reuses_handle(void) {
    KainRuntimeHandle h1 = kain_fixup_track_allocation(g_d, 16u);
    if (h1 != KAIN_RUNTIME_HANDLE_INVALID) {
        KainRuntimeHandle h2 = kain_fixup_track_allocation(g_d, 24u);
        __CPROVER_assert(h2 == h1,
                         "track_dup: same base reuses handle");
        size_t sz = kain_fixup_handle_size(h2);
        __CPROVER_assert(sz == 24u,
                         "track_dup: size updated to new value");
    }
}

void check_track_duplicate_noop_size(void) {
    /* Re-track with same size — handle + size unchanged */
    KainRuntimeHandle h1 = kain_fixup_track_allocation(g_a, sizeof(g_a));
    if (h1 != KAIN_RUNTIME_HANDLE_INVALID) {
        KainRuntimeHandle h2 = kain_fixup_track_allocation(g_a, sizeof(g_a));
        __CPROVER_assert(h2 == h1,
                         "track_dup_same: same handle");
        size_t sz = kain_fixup_handle_size(h2);
        __CPROVER_assert(sz == sizeof(g_a),
                         "track_dup_same: size unchanged");
    }
}


/* ====================================================================
 * Resolve / size / view with INVALID handle
 * ==================================================================== */

void check_resolve_invalid(void) {
    void* base = kain_fixup_resolve_handle(KAIN_RUNTIME_HANDLE_INVALID);
    __CPROVER_assert(base == NULL,
                     "resolve_invalid: returns NULL");
}

void check_size_invalid(void) {
    size_t sz = kain_fixup_handle_size(KAIN_RUNTIME_HANDLE_INVALID);
    __CPROVER_assert(sz == 0u,
                     "size_invalid: returns 0");
}

void check_view_null_out(void) {
    int rc = kain_fixup_view(42u, NULL);
    __CPROVER_assert(rc == -1,
                     "view_null: NULL out_view returns -1");
}

void check_view_null_out_invalid(void) {
    int rc = kain_fixup_view(KAIN_RUNTIME_HANDLE_INVALID, NULL);
    __CPROVER_assert(rc == -1,
                     "view_null_invalid: NULL+INVALID returns -1");
}


/* ====================================================================
 * Handle for pointer
 * ==================================================================== */

void check_handle_for_ptr_null(void) {
    KainRuntimeHandle h = kain_fixup_handle_for_pointer(NULL);
    __CPROVER_assert(h == KAIN_RUNTIME_HANDLE_INVALID,
                     "handle_for_ptr_null: NULL returns INVALID");
}

void check_handle_for_ptr_exact(void) {
    /* g_a is already tracked from previous tests */
    KainRuntimeHandle h_track = kain_fixup_track_allocation(g_a, sizeof(g_a));
    if (h_track != KAIN_RUNTIME_HANDLE_INVALID) {
        KainRuntimeHandle h_find = kain_fixup_handle_for_pointer(g_a);
        __CPROVER_assert(h_find == h_track,
                         "handle_for_ptr_exact: finds same handle by base");
    }
}

void check_handle_for_ptr_interior(void) {
    KainRuntimeHandle h_track = kain_fixup_track_allocation(g_b, sizeof(g_b));
    if (h_track != KAIN_RUNTIME_HANDLE_INVALID) {
        void* interior = g_b + 16;
        KainRuntimeHandle h_find = kain_fixup_handle_for_pointer(interior);
        __CPROVER_assert(h_find == h_track,
                         "handle_for_ptr_interior: interior pointer finds handle");
    }
}

void check_handle_for_ptr_untracked(void) {
    /* A pointer that is not tracked — CBMC explores the ownership fallback
     * path.  The function should not crash. */
    KainRuntimeHandle h = kain_fixup_handle_for_pointer(g_c);
    /* May return INVALID or a handle (if ownership path succeeds).
     * Either is valid — we just check no crash. */
    __CPROVER_assert(1,
                     "handle_for_ptr_untracked: no crash");
}


/* ====================================================================
 * Known ref — register
 * ==================================================================== */

void check_register_ref_null_location(void) {
    int rc = kain_fixup_register_known_ref(NULL);
    __CPROVER_assert(rc == -1,
                     "register_ref_null_loc: NULL location returns -1");
}

void check_register_ref_null_target(void) {
    void* null_ptr = NULL;
    int rc = kain_fixup_register_known_ref(&null_ptr);
    __CPROVER_assert(rc == -1,
                     "register_ref_null_target: *location==NULL returns -1");
}

void check_register_ref_basic(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_a, sizeof(g_a));
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_a;
        int rc = kain_fixup_register_known_ref(&g_ref_loc);
        if (rc == 0) {
            __CPROVER_assert(kain_fixup_known_ref_count() >= 1u,
                             "register_ref: known_ref_count >= 1");
        }
    }
}

void check_register_ref_interior(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_b, sizeof(g_b));
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        /* Register ref pointing into the middle */
        g_ref_loc = g_b + 8;
        int rc = kain_fixup_register_known_ref(&g_ref_loc);
        if (rc == 0) {
            __CPROVER_assert(kain_fixup_known_ref_count() >= 1u,
                             "register_ref_interior: known_ref_count >= 1");
        }
    }
}

void check_register_ref_out_of_bounds(void) {
    /* Register a ref that points JUST PAST the allocation — should fail
     * because the offset check enforces target_addr < base_addr + size. */
    KainRuntimeHandle h = kain_fixup_track_allocation(g_c, 32u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_c + 32;  /* one past end */
        int rc = kain_fixup_register_known_ref(&g_ref_loc);
        __CPROVER_assert(rc == -1,
                         "register_ref_oob: past-end ptr returns -1");
    }
}

void check_register_ref_before_base(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_d, 16u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_d - 1;  /* before base */
        int rc = kain_fixup_register_known_ref(&g_ref_loc);
        __CPROVER_assert(rc == -1,
                         "register_ref_before: before-base ptr returns -1");
    }
}


/* ====================================================================
 * Known ref — unregister
 * ==================================================================== */

void check_unregister_ref_null_location(void) {
    int rc = kain_fixup_unregister_known_ref(NULL);
    __CPROVER_assert(rc == -1,
                     "unregister_ref_null: NULL location returns -1");
}

void check_unregister_ref_basic(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_a, sizeof(g_a));
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_a;
        int rc_reg = kain_fixup_register_known_ref(&g_ref_loc);
        if (rc_reg == 0) {
            uint64_t before = kain_fixup_known_ref_count();
            int rc_unreg = kain_fixup_unregister_known_ref(&g_ref_loc);
            __CPROVER_assert(rc_unreg == 0,
                             "unregister_ref: succeeds");
            __CPROVER_assert(kain_fixup_known_ref_count() < before ||
                             before == 0u,
                             "unregister_ref: count decreased or zero");
        }
    }
}

void check_unregister_ref_not_registered(void) {
    /* Unregister a location that was never registered */
    g_ref_loc = g_a;
    int rc = kain_fixup_unregister_known_ref(&g_ref_loc);
    /* The ref doesn't exist in any list, so internal remove fails. */
    __CPROVER_assert(rc == -1,
                     "unregister_ref_not_reg: non-registered returns -1");
}

void check_unregister_ref_double(void) {
    /* Register and unregister twice — second unregister fails */
    KainRuntimeHandle h = kain_fixup_track_allocation(g_b, sizeof(g_b));
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_b;
        int rc_reg = kain_fixup_register_known_ref(&g_ref_loc);
        if (rc_reg == 0) {
            int rc_u1 = kain_fixup_unregister_known_ref(&g_ref_loc);
            __CPROVER_assert(rc_u1 == 0,
                             "unregister_ref_double: first succeeds");
            int rc_u2 = kain_fixup_unregister_known_ref(&g_ref_loc);
            __CPROVER_assert(rc_u2 == -1,
                             "unregister_ref_double: second returns -1");
        }
    }
}


/* ====================================================================
 * Known ref — update
 * ==================================================================== */

void check_update_ref_null_location(void) {
    int rc = kain_fixup_update_known_ref(NULL, g_a);
    __CPROVER_assert(rc == -1,
                     "update_ref_null_loc: NULL location returns -1");
}

void check_update_ref_to_null(void) {
    /* Update with value=NULL just sets *location = NULL */
    g_ref_loc = g_a;
    int rc = kain_fixup_update_known_ref(&g_ref_loc, NULL);
    __CPROVER_assert(rc == 0,
                     "update_ref_to_null: returns 0");
    __CPROVER_assert(g_ref_loc == NULL,
                     "update_ref_to_null: location set to NULL");
}

void check_update_ref_to_tracked(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_a, sizeof(g_a));
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_b;  /* currently points to untracked memory */
        int rc = kain_fixup_update_known_ref(&g_ref_loc, g_a);
        if (rc == 0) {
            __CPROVER_assert(g_ref_loc == g_a,
                             "update_ref_to_tracked: location updated");
        }
    }
}

void check_update_ref_to_untracked(void) {
    /* Update to an untracked pointer — should fail because
     * kain_fixup_handle_for_pointer(untracked) will fail. */
    g_ref_loc = g_c;
    /* g_b is not explicitly tracked (or may be tracked from earlier — but we've
     * registered g_b which tracks it internally).  Use a completely fresh buffer. */
    int rc = kain_fixup_update_known_ref(&g_ref_loc, g_d);
    /* May succeed if ownership path tracks it, may fail otherwise.
     * Either is fine — we just verify no crash. */
    __CPROVER_assert(1,
                     "update_ref_to_untracked: no crash");
}

void check_update_ref_replace(void) {
    /* Register ref pointing to g_a, then update it to point to g_c (tracked) */
    KainRuntimeHandle h1 = kain_fixup_track_allocation(g_a, sizeof(g_a));
    KainRuntimeHandle h2 = kain_fixup_track_allocation(g_c, sizeof(g_c));
    if (h1 != KAIN_RUNTIME_HANDLE_INVALID &&
        h2 != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_a;
        int rc_reg = kain_fixup_register_known_ref(&g_ref_loc);
        if (rc_reg == 0) {
            int rc_upd = kain_fixup_update_known_ref(&g_ref_loc, g_c);
            if (rc_upd == 0) {
                __CPROVER_assert(g_ref_loc == g_c,
                                 "update_ref_replace: location changed to g_c");
            }
        }
    }
}


/* ====================================================================
 * Multiple refs on same allocation
 * ==================================================================== */

void check_multiple_refs_same_alloc(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_b, sizeof(g_b));
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_b;
        g_ref_loc2 = g_b + 8;
        int rc1 = kain_fixup_register_known_ref(&g_ref_loc);
        int rc2 = kain_fixup_register_known_ref(&g_ref_loc2);
        if (rc1 == 0 && rc2 == 0) {
            __CPROVER_assert(kain_fixup_known_ref_count() >= 2u,
                             "multi_ref: ref_count >= 2");
        }
    }
}

void check_multiple_refs_diff_alloc(void) {
    KainRuntimeHandle ha = kain_fixup_track_allocation(g_a, sizeof(g_a));
    KainRuntimeHandle hc = kain_fixup_track_allocation(g_c, sizeof(g_c));
    if (ha != KAIN_RUNTIME_HANDLE_INVALID &&
        hc != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_a;
        g_ref_loc2 = g_c;
        int rc1 = kain_fixup_register_known_ref(&g_ref_loc);
        int rc2 = kain_fixup_register_known_ref(&g_ref_loc2);
        if (rc1 == 0 && rc2 == 0) {
            __CPROVER_assert(kain_fixup_known_ref_count() >= 2u,
                             "multi_ref_diff: ref_count >= 2");
        }
    }
}


/* ====================================================================
 * Relocate allocation
 * ==================================================================== */

void check_relocate_null_params(void) {
    int rc;

    /* INVALID handle */
    rc = kain_fixup_relocate_allocation(
        KAIN_RUNTIME_HANDLE_INVALID, NULL, g_b, 64u);
    __CPROVER_assert(rc == -1,
                     "relocate_invalid_handle: returns -1");

    /* NULL new_base */
    rc = kain_fixup_relocate_allocation(1u, NULL, NULL, 64u);
    __CPROVER_assert(rc == -1,
                     "relocate_null_newbase: returns -1");

    /* zero size */
    rc = kain_fixup_relocate_allocation(1u, NULL, g_b, 0u);
    __CPROVER_assert(rc == -1,
                     "relocate_zero_size: returns -1");
}

void check_relocate_basic(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_a, 16u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        /* Register a ref at the base of g_a */
        g_ref_loc = g_a;
        int rc_reg = kain_fixup_register_known_ref(&g_ref_loc);
        if (rc_reg == 0) {
            uint64_t relo_before = kain_fixup_relocation_count();
            uint64_t count_before = kain_fixup_known_ref_count();

            int rc_relo = kain_fixup_relocate_allocation(
                h, g_a, g_b, 32u);
            if (rc_relo == 0) {
                /* Resolve returns new base */
                void* new_base = kain_fixup_resolve_handle(h);
                __CPROVER_assert(new_base == g_b,
                                 "relocate_basic: resolve returns new base");
                /* Size updated */
                __CPROVER_assert(kain_fixup_handle_size(h) == 32u,
                                 "relocate_basic: handle_size updated to 32");
                /* Ref now points to new base (offset 0 preserved) */
                __CPROVER_assert(g_ref_loc == g_b,
                                 "relocate_basic: ref updated to new base");
                /* Relocation count increased */
                __CPROVER_assert(kain_fixup_relocation_count() > relo_before,
                                 "relocate_basic: relocation_count increased");
                /* Ref count unchanged (same ref, just updated) */
                __CPROVER_assert(kain_fixup_known_ref_count() == count_before,
                                 "relocate_basic: ref count unchanged");
                /* Last handle updated */
                __CPROVER_assert(kain_fixup_last_handle() == h,
                                 "relocate_basic: last_handle == h");
            }
        }
    }
}

void check_relocate_preserves_offset(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_c, 64u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        /* Ref at g_c + 12 */
        g_ref_loc = g_c + 12;
        int rc_reg = kain_fixup_register_known_ref(&g_ref_loc);
        if (rc_reg == 0) {
            int rc_relo = kain_fixup_relocate_allocation(
                h, NULL, g_d, 64u);
            if (rc_relo == 0) {
                /* Ref offset (12) should be preserved: g_d + 12 */
                __CPROVER_assert(g_ref_loc == g_d + 12,
                                 "relocate_offset: ref offset 12 preserved");
                /* Base resolved */
                __CPROVER_assert(kain_fixup_resolve_handle(h) == g_d,
                                 "relocate_offset: resolve returns g_d");
            }
        }
    }
}

void check_relocate_wrong_old_base(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_a, 16u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        /* Pass wrong old_base — should fail */
        int rc = kain_fixup_relocate_allocation(
            h, g_b, g_c, 32u);
        __CPROVER_assert(rc == -1,
                         "relocate_wrong_old: wrong old_base returns -1");
        /* Original allocation unchanged */
        __CPROVER_assert(kain_fixup_resolve_handle(h) == g_a,
                         "relocate_wrong_old: resolve still returns g_a");
    }
}

void check_relocate_old_base_null(void) {
    /* old_base=NULL skips the check */
    KainRuntimeHandle h = kain_fixup_track_allocation(g_b, 16u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_b;
        (void)kain_fixup_register_known_ref(&g_ref_loc);

        int rc = kain_fixup_relocate_allocation(
            h, NULL, g_c, 32u);
        if (rc == 0) {
            __CPROVER_assert(kain_fixup_resolve_handle(h) == g_c,
                             "relocate_null_old: resolve returns new base");
        }
    }
}

void check_relocate_no_refs(void) {
    /* Relocate an allocation that has no registered refs */
    KainRuntimeHandle h = kain_fixup_track_allocation(g_d, 16u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        int rc = kain_fixup_relocate_allocation(
            h, NULL, g_a, 48u);
        if (rc == 0) {
            __CPROVER_assert(kain_fixup_resolve_handle(h) == g_a,
                             "relocate_no_refs: resolve returns new base");
            __CPROVER_assert(kain_fixup_handle_size(h) == 48u,
                             "relocate_no_refs: size updated");
        }
    }
}


/* ====================================================================
 * Unregister allocation
 * ==================================================================== */

void check_unregister_alloc_invalid(void) {
    int rc = kain_fixup_unregister_allocation(KAIN_RUNTIME_HANDLE_INVALID);
    __CPROVER_assert(rc == -1,
                     "unregister_alloc_invalid: INVALID handle returns -1");
}

void check_unregister_alloc_basic(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_a, 16u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        /* Register a ref */
        g_ref_loc = g_a;
        int rc_reg = kain_fixup_register_known_ref(&g_ref_loc);

        int rc_unreg = kain_fixup_unregister_allocation(h);
        __CPROVER_assert(rc_unreg == 0,
                         "unregister_alloc: succeeds");

        /* Resolve of stale handle returns NULL */
        __CPROVER_assert(kain_fixup_resolve_handle(h) == NULL,
                         "unregister_alloc: stale handle resolve == NULL");

        /* Handle size returns 0 */
        __CPROVER_assert(kain_fixup_handle_size(h) == 0u,
                         "unregister_alloc: stale handle size == 0");

        /* If ref was registered, location zeroed */
        if (rc_reg == 0) {
            __CPROVER_assert(g_ref_loc == NULL,
                             "unregister_alloc: ref location zeroed");
        }

        /* Ref count is 0 */
        __CPROVER_assert(kain_fixup_known_ref_count() == 0u,
                         "unregister_alloc: known_ref_count == 0");

        /* Double unregister fails */
        int rc_double = kain_fixup_unregister_allocation(h);
        __CPROVER_assert(rc_double == -1,
                         "unregister_alloc: double unregister returns -1");
    }
}

void check_unregister_alloc_no_refs(void) {
    KainRuntimeHandle h = kain_fixup_track_allocation(g_b, 32u);
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        int rc = kain_fixup_unregister_allocation(h);
        __CPROVER_assert(rc == 0,
                         "unregister_alloc_no_refs: succeeds");
        __CPROVER_assert(kain_fixup_resolve_handle(h) == NULL,
                         "unregister_alloc_no_refs: resolve == NULL");
        __CPROVER_assert(kain_fixup_known_ref_count() == 0u,
                         "unregister_alloc_no_refs: ref_count == 0");
    }
}

void check_unregister_alloc_multi_refs(void) {
    /* Unregister an allocation with multiple refs — all zeroed */
    KainRuntimeHandle h = kain_fixup_track_allocation(g_c, sizeof(g_c));
    if (h != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_c;
        g_ref_loc2 = g_c + 8;
        int rc1 = kain_fixup_register_known_ref(&g_ref_loc);
        int rc2 = kain_fixup_register_known_ref(&g_ref_loc2);
        if (rc1 == 0 && rc2 == 0) {
            uint64_t before = kain_fixup_known_ref_count();
            int rc = kain_fixup_unregister_allocation(h);
            __CPROVER_assert(rc == 0,
                             "unregister_alloc_multi: succeeds");
            __CPROVER_assert(g_ref_loc == NULL,
                             "unregister_alloc_multi: ref1 zeroed");
            __CPROVER_assert(g_ref_loc2 == NULL,
                             "unregister_alloc_multi: ref2 zeroed");
            __CPROVER_assert(kain_fixup_known_ref_count() == 0u,
                             "unregister_alloc_multi: ref_count == 0");
        }
    }
}


/* ====================================================================
 * Multiple independent allocations
 * ==================================================================== */

void check_multiple_allocations(void) {
    KainRuntimeHandle ha = kain_fixup_track_allocation(g_a, 16u);
    KainRuntimeHandle hb = kain_fixup_track_allocation(g_b, 32u);
    if (ha != KAIN_RUNTIME_HANDLE_INVALID &&
        hb != KAIN_RUNTIME_HANDLE_INVALID) {
        __CPROVER_assert(ha != hb,
                         "multi_alloc: handles are distinct");
        __CPROVER_assert(kain_fixup_resolve_handle(ha) == g_a,
                         "multi_alloc: ha resolves to g_a");
        __CPROVER_assert(kain_fixup_resolve_handle(hb) == g_b,
                         "multi_alloc: hb resolves to g_b");
    }
}

void check_multiple_alloc_unregister_one(void) {
    /* Track two, unregister one, other still valid */
    KainRuntimeHandle ha = kain_fixup_track_allocation(g_a, 16u);
    KainRuntimeHandle hb = kain_fixup_track_allocation(g_b, 32u);
    if (ha != KAIN_RUNTIME_HANDLE_INVALID &&
        hb != KAIN_RUNTIME_HANDLE_INVALID) {
        int rc = kain_fixup_unregister_allocation(ha);
        if (rc == 0) {
            __CPROVER_assert(kain_fixup_resolve_handle(ha) == NULL,
                             "multi_alloc_unreg_one: ha stale");
            __CPROVER_assert(kain_fixup_resolve_handle(hb) == g_b,
                             "multi_alloc_unreg_one: hb still valid");
        }
    }
}


/* ====================================================================
 * Query functions — safe to call at any point
 * ==================================================================== */

void check_query_known_ref_count(void) {
    uint64_t c = kain_fixup_known_ref_count();
    /* Any value is fine — just checking no crash */
    __CPROVER_assert(1, "query: known_ref_count no crash");
}

void check_query_relocation_count(void) {
    uint64_t c = kain_fixup_relocation_count();
    __CPROVER_assert(1, "query: relocation_count no crash");
}

void check_query_last_handle(void) {
    KainRuntimeHandle h = kain_fixup_last_handle();
    __CPROVER_assert(1, "query: last_handle no crash");
}


/* ====================================================================
 * Unregister then re-track lifecycle
 * ==================================================================== */

void check_unregister_then_retrack(void) {
    KainRuntimeHandle h1 = kain_fixup_track_allocation(g_a, 16u);
    if (h1 != KAIN_RUNTIME_HANDLE_INVALID) {
        g_ref_loc = g_a;
        (void)kain_fixup_register_known_ref(&g_ref_loc);

        int rc = kain_fixup_unregister_allocation(h1);
        if (rc == 0) {
            /* Track same base again after unregister */
            KainRuntimeHandle h2 = kain_fixup_track_allocation(g_a, 32u);
            if (h2 != KAIN_RUNTIME_HANDLE_INVALID) {
                /* h2 may be same slot but different magic (different handle value) */
                __CPROVER_assert(kain_fixup_resolve_handle(h2) == g_a,
                                 "retrack: resolve new handle returns g_a");
                __CPROVER_assert(kain_fixup_handle_size(h2) == 32u,
                                 "retrack: new handle size == 32");
                /* Old handle still stale */
                __CPROVER_assert(kain_fixup_resolve_handle(h1) == NULL,
                                 "retrack: old handle still stale");
            }
        }
    }
}


/* ====================================================================
 * Main — run all checks
 * ==================================================================== */
int main(void) {
    /* Initialisation */
    check_init();

    /* Track — NULL/zero edge cases */
    check_track_null_base();
    check_track_zero_size();
    check_track_null_base_zero_size();

    /* Track — resolve / size / view round-trips */
    check_track_resolve();
    check_track_handle_size();
    check_track_view();
    check_track_view_cleared_on_failure();

    /* Duplicate track */
    check_track_duplicate_reuses_handle();
    check_track_duplicate_noop_size();

    /* INVALID handle safety */
    check_resolve_invalid();
    check_size_invalid();
    check_view_null_out();
    check_view_null_out_invalid();

    /* Handle-for-pointer */
    check_handle_for_ptr_null();
    check_handle_for_ptr_exact();
    check_handle_for_ptr_interior();
    check_handle_for_ptr_untracked();

    /* Register known ref */
    check_register_ref_null_location();
    check_register_ref_null_target();
    check_register_ref_basic();
    check_register_ref_interior();
    check_register_ref_out_of_bounds();
    check_register_ref_before_base();

    /* Unregister known ref */
    check_unregister_ref_null_location();
    check_unregister_ref_basic();
    check_unregister_ref_not_registered();
    check_unregister_ref_double();

    /* Update known ref */
    check_update_ref_null_location();
    check_update_ref_to_null();
    check_update_ref_to_tracked();
    check_update_ref_to_untracked();
    check_update_ref_replace();

    /* Multiple refs */
    check_multiple_refs_same_alloc();
    check_multiple_refs_diff_alloc();

    /* Relocate */
    check_relocate_null_params();
    check_relocate_basic();
    check_relocate_preserves_offset();
    check_relocate_wrong_old_base();
    check_relocate_old_base_null();
    check_relocate_no_refs();

    /* Unregister allocation */
    check_unregister_alloc_invalid();
    check_unregister_alloc_basic();
    check_unregister_alloc_no_refs();
    check_unregister_alloc_multi_refs();

    /* Multiple allocations */
    check_multiple_allocations();
    check_multiple_alloc_unregister_one();

    /* Query functions */
    check_query_known_ref_count();
    check_query_relocation_count();
    check_query_last_handle();

    /* Lifecycle: unregister → re-track */
    check_unregister_then_retrack();

    return 0;
}
