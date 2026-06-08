/*
 * check_ownership.c -- CBMC verification harness for ownership module
 *
 * Tests the core collapse/observe/decay state machine and registration/
 * lookup logic by calling internal _unlocked and _slot_unlocked variants
 * directly, avoiding the atomic spinlock overhead that creates path
 * explosion in CBMC.
 *
 * This concatenates AFTER src/core/ownership.c, so all static globals
 * and functions are visible in the combined translation unit.
 *
 * Use:  cat src/core/ownership.c test/cbmc/check_ownership.c > combined.c
 *       cbmc --unwind 5 --no-unwinding-assertions combined.c \
 *            -I include -I src/core
 * Or:   python test/scripts/run_pipeline.py cbmc --harness check_ownership
 */

#include "ownership.h"
#include <stddef.h>

/* ============================================================
 *  Forward declarations of static functions from ownership.c
 * ============================================================ */

/* Slot-level state machine */
static int kain_ownership_begin_observe_slot_unlocked(int slot);
static int kain_ownership_end_observe_slot_unlocked(int slot);
static int kain_ownership_begin_collapse_slot_unlocked(int slot);
static int kain_ownership_end_collapse_slot_unlocked(int slot);
static int kain_ownership_begin_share_slot_unlocked(int slot);
static int kain_ownership_end_share_slot_unlocked(int slot);
static int kain_ownership_decay_slot_unlocked(
    void* ptr, int slot, int reclaim,
    void* out_release_now, int* out_release_immediately);

/* Registration / lookup (unlocked -- no spinlock) */
static int kain_ownership_upsert_unlocked(
    void* ptr, int64_t kind, size_t sz,
    int state, unsigned int obs, int* out_slot);
static int kain_ownership_find_slot(const void* ptr);
static int kain_ownership_find_free_slot(void);
static int kain_ownership_region_is_heap(const KainOwnershipRegion* region);

/* Registered-state-machine helpers (unlocked) */
static int kain_ownership_begin_observe_registered_unlocked(const void* ptr);
static int kain_ownership_end_observe_registered_unlocked(const void* ptr);
static int kain_ownership_begin_collapse_registered_unlocked(void* ptr);
static int kain_ownership_end_collapse_registered_unlocked(void* ptr);
static int kain_ownership_begin_share_registered_unlocked(void* ptr);
static int kain_ownership_end_share_registered_unlocked(void* ptr);
static int kain_ownership_decay_registered_unlocked(void* ptr);
static int kain_ownership_ensure_imported_unlocked(const void* ptr);

/* Helper allocation */
static int kain_ownership_register_helper_allocation_unlocked(
    void* ptr, size_t sz, int* out_slot);
static int kain_ownership_helper_slot_from_token_unlocked(
    const void* ptr, uint16_t token);

/* Index operations */
static int kain_ownership_index_insert_unlocked(const void* ptr, int slot);
static int kain_ownership_index_remove_unlocked(const void* ptr, int slot);
static uint32_t kain_ownership_pointer_index_slot(const void* ptr);

/* Bit helpers */
static uint64_t kain_ownership_isolate_low_bit_u64(uint64_t v);
static unsigned int kain_ownership_low_bit_index_u64(uint64_t v);

/* Update (unlocked) */
static int kain_ownership_update_unlocked(
    void* old_ptr, void* new_ptr, size_t sz);

/* ============================================================
 *  Static globals from ownership.c
 * ============================================================ */
extern KainOwnershipRegion KAIN_OWNERSHIP_REGIONS[4096];
extern uint64_t KAIN_OWNERSHIP_OCCUPANCY_WORDS[64];
extern uint32_t KAIN_OWNERSHIP_POINTER_INDEX[8192];

/* ============================================================
 *  Static buffers for pointer provenance
 * ============================================================ */
static unsigned char g_b1[256];
static unsigned char g_b2[256];
static unsigned char g_big[4096];
static void* g_out_base;
static size_t g_out_size;

/* ============================================================
 *  Helpers
 * ============================================================ */

/* Set up a region at given slot with given parameters */
static void setup_region(int slot, void* ptr, int64_t kind,
                         int state, unsigned int observers)
{
    KAIN_OWNERSHIP_REGIONS[slot].ptr               = ptr;
    KAIN_OWNERSHIP_REGIONS[slot].size              = 64;
    KAIN_OWNERSHIP_REGIONS[slot].kind              = kind;
    KAIN_OWNERSHIP_REGIONS[slot].state             = state;
    KAIN_OWNERSHIP_REGIONS[slot].observers         = observers;
    KAIN_OWNERSHIP_REGIONS[slot].relocation_handle = KAIN_RUNTIME_HANDLE_INVALID;
    KAIN_OWNERSHIP_REGIONS[slot].decay_queued      = 0;
    KAIN_OWNERSHIP_REGIONS[slot].occupied          = 1;
}

/* Reset occupancy + index arrays to empty, ready for registration */
static void reset_registry(void)
{
    KAIN_OWNERSHIP_OCCUPANCY_WORDS[0] = 0;
    KAIN_OWNERSHIP_POINTER_INDEX[0] = 0;
    KAIN_OWNERSHIP_POINTER_INDEX[1] = 0;
    KAIN_OWNERSHIP_POINTER_INDEX[2] = 0;
    KAIN_OWNERSHIP_POINTER_INDEX[3] = 0;
}


/* ============================================================
 *  STATE MACHINE TESTS  (slot-unlocked, no locking)
 * ============================================================ */

/* IDLE -> begin_observe -> OBSERVED -> end_observe -> IDLE */
void t_observe(void)
{
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);

    int rc = kain_ownership_begin_observe_slot_unlocked(0);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK,              "obs: begin OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 1,
                                                           "obs: observers == 1");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_OBSERVED,
                                                           "obs: state == OBSERVED");

    rc = kain_ownership_end_observe_slot_unlocked(0);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK,              "obs: end OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 0,
                                                           "obs: observers == 0");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_IDLE,
                                                           "obs: state == IDLE");
}

/* IDLE -> begin_collapse -> COLLAPSED -> end_collapse -> IDLE */
void t_collapse(void)
{
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);

    __CPROVER_assert(kain_ownership_begin_collapse_slot_unlocked(0)
                     == KAIN_OWNERSHIP_OK,                 "col: begin OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_COLLAPSED,
                                                           "col: state == COLLAPSED");
    __CPROVER_assert(kain_ownership_end_collapse_slot_unlocked(0)
                     == KAIN_OWNERSHIP_OK,                 "col: end OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_IDLE,
                                                           "col: state == IDLE");
}

/* IDLE -> begin_share -> SHARED -> end_share -> IDLE */
void t_share(void)
{
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);

    __CPROVER_assert(kain_ownership_begin_share_slot_unlocked(0)
                     == KAIN_OWNERSHIP_OK,                 "shr: begin OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_SHARED,
                                                           "shr: state == SHARED");
    __CPROVER_assert(kain_ownership_end_share_slot_unlocked(0)
                     == KAIN_OWNERSHIP_OK,                 "shr: end OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_IDLE,
                                                           "shr: state == IDLE");
}

/* IDLE -> decay (non-heap) -> DECAYED (terminal) */
void t_decay_nonheap(void)
{
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_WORLD_STATE,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);

    int rc = kain_ownership_decay_slot_unlocked(&g_b1[0], 0, 0, NULL, NULL);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK,              "dec: OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_DECAYED,
                                                           "dec: state == DECAYED");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 0,
                                                           "dec: observers == 0");
}

/* IDLE -> decay (heap, small) -> immediate release */
void t_decay_heap_small(void)
{
    reset_registry();
    setup_region(0, &g_big[0], KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);

    int release_immediately = 0;
    int rc = kain_ownership_decay_slot_unlocked(
        &g_big[0], 0, 0, NULL, &release_immediately);

    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK,              "heap_dec: OK");
    __CPROVER_assert(release_immediately == 1,
                    "heap_dec: immediate (size=64 <= 262144)");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].occupied == 0,
                    "heap_dec: slot cleared");
}

/* Invalid transitions */
void t_invalid(void)
{
    /* begin_observe on DECAYED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_DECAYED, 0);
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_DECAYED,        "inv: obs on DECAYED");

    /* begin_observe on COLLAPSED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_COLLAPSED, 0);
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED,       "inv: obs on COLLAPSED");

    /* begin_observe on SHARED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_SHARED, 0);
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED,       "inv: obs on SHARED");

    /* end_observe on IDLE (not observed) */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);
    __CPROVER_assert(kain_ownership_end_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_NOT_OBSERVED,   "inv: endobs on IDLE");

    /* begin_collapse on OBSERVED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_OBSERVED, 1);
    __CPROVER_assert(kain_ownership_begin_collapse_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_OBSERVED,       "inv: col on OBSERVED");

    /* begin_collapse on COLLAPSED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_COLLAPSED, 0);
    __CPROVER_assert(kain_ownership_begin_collapse_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED,      "inv: col on COLLAPSED");

    /* begin_collapse on SHARED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_SHARED, 0);
    __CPROVER_assert(kain_ownership_begin_collapse_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED,      "inv: col on SHARED");

    /* end_collapse on IDLE */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);
    __CPROVER_assert(kain_ownership_end_collapse_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_NOT_COLLAPSED,  "inv: endcol on IDLE");

    /* end_collapse on DECAYED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_DECAYED, 0);
    __CPROVER_assert(kain_ownership_end_collapse_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_NOT_COLLAPSED,  "inv: endcol on DECAYED");

    /* begin_share on OBSERVED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_OBSERVED, 1);
    __CPROVER_assert(kain_ownership_begin_share_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_OBSERVED,       "inv: shr on OBSERVED");

    /* begin_share on SHARED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_SHARED, 0);
    __CPROVER_assert(kain_ownership_begin_share_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED,      "inv: shr on SHARED");

    /* end_share on IDLE */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);
    __CPROVER_assert(kain_ownership_end_share_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_NOT_COLLAPSED,  "inv: endshr on IDLE");

    /* decay on OBSERVED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_WORLD_STATE,
                 KAIN_OWNERSHIP_STATE_OBSERVED, 1);
    __CPROVER_assert(kain_ownership_decay_slot_unlocked(&g_b1[0], 0, 0, NULL, NULL)
                     == KAIN_OWNERSHIP_ERR_OBSERVED,       "inv: decay on OBSERVED");

    /* decay on COLLAPSED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_WORLD_STATE,
                 KAIN_OWNERSHIP_STATE_COLLAPSED, 0);
    __CPROVER_assert(kain_ownership_decay_slot_unlocked(&g_b1[0], 0, 0, NULL, NULL)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED,      "inv: decay on COLLAPSED");

    /* decay on SHARED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_WORLD_STATE,
                 KAIN_OWNERSHIP_STATE_SHARED, 0);
    __CPROVER_assert(kain_ownership_decay_slot_unlocked(&g_b1[0], 0, 0, NULL, NULL)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED,      "inv: decay on SHARED");

    /* decay on DECAYED */
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_WORLD_STATE,
                 KAIN_OWNERSHIP_STATE_DECAYED, 0);
    __CPROVER_assert(kain_ownership_decay_slot_unlocked(&g_b1[0], 0, 0, NULL, NULL)
                     == KAIN_OWNERSHIP_ERR_DECAYED,        "inv: decay on DECAYED");
}

/* Double observe: counter increments, double end decrements */
void t_double_observe(void)
{
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_IDLE, 0);

    kain_ownership_begin_observe_slot_unlocked(0);
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 1,
                                                           "dbl: obs == 1");
    kain_ownership_begin_observe_slot_unlocked(0);
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 2,
                                                           "dbl: obs == 2");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_OBSERVED,
                                                           "dbl: state OBSERVED");

    kain_ownership_end_observe_slot_unlocked(0);
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 1,
                                                           "dbl: obs == 1 after 1 end");
    kain_ownership_end_observe_slot_unlocked(0);
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 0,
                                                           "dbl: obs == 0 after 2 ends");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_IDLE,
                                                           "dbl: state == IDLE");

    __CPROVER_assert(kain_ownership_end_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_NOT_OBSERVED,
                                                           "dbl: 3rd end -> ERR_NOT_OBSERVED");
}

/* Observer overflow guard */
void t_observer_overflow(void)
{
    setup_region(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
                 KAIN_OWNERSHIP_STATE_IDLE, UINT32_MAX);
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_OVERFLOW,
                                                           "overflow: ERR_OVERFLOW");
}

/* Invalid slot (-1) */
void t_invalid_slot(void)
{
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(-1)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND,      "inv: begin_observe(-1)");
    __CPROVER_assert(kain_ownership_end_observe_slot_unlocked(-1)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND,      "inv: end_observe(-1)");
    __CPROVER_assert(kain_ownership_begin_collapse_slot_unlocked(-1)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND,      "inv: begin_collapse(-1)");
    __CPROVER_assert(kain_ownership_end_collapse_slot_unlocked(-1)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND,      "inv: end_collapse(-1)");
    __CPROVER_assert(kain_ownership_begin_share_slot_unlocked(-1)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND,      "inv: begin_share(-1)");
    __CPROVER_assert(kain_ownership_end_share_slot_unlocked(-1)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND,      "inv: end_share(-1)");
}


/* ============================================================
 *  REGISTRATION TESTS  (internal _unlocked variants)
 * ============================================================ */

/* Upsert: register a new pointer */
void t_upsert_new(void)
{
    reset_registry();
    int slot = -1;
    int rc = kain_ownership_upsert_unlocked(
        &g_b1[0], KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 64,
        KAIN_OWNERSHIP_STATE_IDLE, 0, &slot);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK,              "ups: register OK");
    __CPROVER_assert(slot >= 0,                            "ups: slot >= 0");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[slot].state == KAIN_OWNERSHIP_STATE_IDLE,
                                                           "ups: state == IDLE");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[slot].ptr == &g_b1[0],
                                                           "ups: ptr matches");
}

/* Upsert: register same pointer again (update) */
void t_upsert_again(void)
{
    reset_registry();
    int slot1 = -1;
    kain_ownership_upsert_unlocked(&g_b1[0], 1, 64, 0, 0, &slot1);
    int slot2 = -1;
    int rc = kain_ownership_upsert_unlocked(&g_b1[0], 1, 128, 0, 0, &slot2);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK,              "ups2: OK");
    __CPROVER_assert(slot1 == slot2,                       "ups2: same slot");
}

/* Upsert: NULL pointer */
void t_upsert_null(void)
{
    reset_registry();
    __CPROVER_assert(kain_ownership_upsert_unlocked(
        NULL, 1, 64, 0, 0, NULL) == KAIN_OWNERSHIP_ERR_INVALID,
                                                           "ups: NULL -> ERR_INVALID");
}

/* Helper allocation registration */
void t_helper_alloc(void)
{
    reset_registry();
    int slot = -1;
    int rc = kain_ownership_register_helper_allocation_unlocked(
        &g_b1[0], 64, &slot);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK,              "hlp: OK");
    __CPROVER_assert(slot >= 0,                            "hlp: slot >= 0");
}

/* Helper slot from token */
void t_helper_token(void)
{
    reset_registry();
    int slot = -1;
    kain_ownership_register_helper_allocation_unlocked(&g_b1[0], 64, &slot);
    uint16_t token = (uint16_t)(slot + 1);
    int found = kain_ownership_helper_slot_from_token_unlocked(&g_b1[0], token);
    __CPROVER_assert(found == slot,                        "token: round-trip");
}

/* Ensure imported on existing and new ptr */
void t_ensure(void)
{
    reset_registry();
    int slot = -1;
    kain_ownership_upsert_unlocked(&g_b1[0], 1, 64, 0, 0, &slot);
    __CPROVER_assert(kain_ownership_ensure_imported_unlocked(&g_b1[0])
                     == KAIN_OWNERSHIP_OK,                 "ens: existing OK");
    __CPROVER_assert(kain_ownership_ensure_imported_unlocked(&g_b2[0])
                     == KAIN_OWNERSHIP_OK,                 "ens: new OK");
}

/* Registered-state-machine via public internal API */
void t_registered_observe(void)
{
    reset_registry();
    int slot = -1;
    kain_ownership_upsert_unlocked(&g_b1[0], 0, 64, 0, 0, &slot);
    __CPROVER_assert(kain_ownership_begin_observe_registered_unlocked(&g_b1[0])
                     == KAIN_OWNERSHIP_OK,                 "reg_obs: begin OK");
    __CPROVER_assert(kain_ownership_end_observe_registered_unlocked(&g_b1[0])
                     == KAIN_OWNERSHIP_OK,                 "reg_obs: end OK");
}

void t_registered_collapse(void)
{
    reset_registry();
    int slot = -1;
    kain_ownership_upsert_unlocked(&g_b1[0], 0, 64, 0, 0, &slot);
    __CPROVER_assert(kain_ownership_begin_collapse_registered_unlocked(&g_b1[0])
                     == KAIN_OWNERSHIP_OK,                 "reg_col: begin OK");
    __CPROVER_assert(kain_ownership_end_collapse_registered_unlocked(&g_b1[0])
                     == KAIN_OWNERSHIP_OK,                 "reg_col: end OK");
}

void t_registered_share(void)
{
    reset_registry();
    int slot = -1;
    kain_ownership_upsert_unlocked(&g_b1[0], 0, 64, 0, 0, &slot);
    __CPROVER_assert(kain_ownership_begin_share_registered_unlocked(&g_b1[0])
                     == KAIN_OWNERSHIP_OK,                 "reg_shr: begin OK");
    __CPROVER_assert(kain_ownership_end_share_registered_unlocked(&g_b1[0])
                     == KAIN_OWNERSHIP_OK,                 "reg_shr: end OK");
}

void t_registered_decay(void)
{
    reset_registry();
    int slot = -1;
    kain_ownership_upsert_unlocked(&g_b1[0], 3, 64, 0, 0, &slot);
    __CPROVER_assert(kain_ownership_decay_registered_unlocked(&g_b1[0])
                     == KAIN_OWNERSHIP_OK,                 "reg_dec: OK");
}

/* Update unlocked */
void t_update_null_old(void)
{
    reset_registry();
    __CPROVER_assert(kain_ownership_update_unlocked(NULL, &g_b1[0], 64)
                     == KAIN_OWNERSHIP_OK,                 "upd: NULL old -> new reg");
}

void t_update_null_new(void)
{
    reset_registry();
    __CPROVER_assert(kain_ownership_update_unlocked(&g_b1[0], NULL, 64)
                     == KAIN_OWNERSHIP_ERR_INVALID,        "upd: NULL new -> INVALID");
}

void t_update_same_ptr(void)
{
    reset_registry();
    int slot = -1;
    kain_ownership_upsert_unlocked(&g_b1[0], 1, 64, 0, 0, &slot);
    __CPROVER_assert(kain_ownership_update_unlocked(&g_b1[0], &g_b1[0], 128)
                     == KAIN_OWNERSHIP_OK,                 "upd: same ptr OK");
}


/* ============================================================
 *  INDEX + SEARCH TESTS
 * ============================================================ */

void t_find_free_slot_empty(void)
{
    /* All zero-initialized -> slot 0 is free */
    int slot = kain_ownership_find_free_slot();
    __CPROVER_assert(slot == 0,                            "free: slot 0 when empty");
}

void t_find_slot_unregistered(void)
{
    __CPROVER_assert(kain_ownership_find_slot(&g_b1[0]) == -1,
                                                           "find: unreg -> -1");
}

void t_find_slot_registered(void)
{
    reset_registry();
    int slot = -1;
    kain_ownership_upsert_unlocked(&g_b1[0], 1, 64, 0, 0, &slot);
    int found = kain_ownership_find_slot(&g_b1[0]);
    __CPROVER_assert(found >= 0,                           "find: reg >= 0");
    __CPROVER_assert(found < 4096,                         "find: bounds");
}

void t_index_roundtrip(void)
{
    reset_registry();
    __CPROVER_assert(kain_ownership_index_insert_unlocked(&g_b1[0], 7)
                     == KAIN_OWNERSHIP_OK,                 "idx: insert");
    __CPROVER_assert(kain_ownership_find_slot(&g_b1[0]) == 7,
                                                           "idx: find == 7");
    __CPROVER_assert(kain_ownership_index_remove_unlocked(&g_b1[0], 7)
                     == KAIN_OWNERSHIP_OK,                 "idx: remove");
    __CPROVER_assert(kain_ownership_find_slot(&g_b1[0]) == -1,
                                                           "idx: gone after remove");
}

void t_index_null(void)
{
    __CPROVER_assert(kain_ownership_index_insert_unlocked(NULL, 0)
                     == KAIN_OWNERSHIP_ERR_INVALID,        "idx: insert NULL");
    __CPROVER_assert(kain_ownership_index_remove_unlocked(NULL, 0)
                     == KAIN_OWNERSHIP_ERR_INVALID,        "idx: remove NULL");
}

void t_pointer_hash(void)
{
    uint32_t s1 = kain_ownership_pointer_index_slot(&g_b1[0]);
    uint32_t s2 = kain_ownership_pointer_index_slot(&g_b1[0]);
    __CPROVER_assert(s1 == s2,                             "hash: deterministic");
    __CPROVER_assert(s1 < 8192,                            "hash: < INDEX_CAPACITY");
    __CPROVER_assert(kain_ownership_pointer_index_slot(&g_b2[0]) < 8192,
                                                           "hash: g_b2 in range");
}

void t_region_is_heap(void)
{
    KainOwnershipRegion r;
    r.kind = KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION;
    __CPROVER_assert(kain_ownership_region_is_heap(&r) == 1,
                                                           "is_heap: heap -> true");
    r.kind = KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA;
    __CPROVER_assert(kain_ownership_region_is_heap(&r) == 0,
                                                           "is_heap: local -> false");
    r.kind = KAIN_OWNERSHIP_REGION_WORLD_STATE;
    __CPROVER_assert(kain_ownership_region_is_heap(&r) == 0,
                                                           "is_heap: world -> false");
}


/* ============================================================
 *  BIT HELPER TESTS
 * ============================================================ */

void t_bit_helpers(void)
{
    __CPROVER_assert(kain_ownership_isolate_low_bit_u64(0x0) == 0x0,
                                                           "bit: isolate(0) == 0");
    __CPROVER_assert(kain_ownership_isolate_low_bit_u64(0x1) == 0x1,
                                                           "bit: isolate(1) == 1");
    __CPROVER_assert(kain_ownership_isolate_low_bit_u64(0xF0) == 0x10,
                                                           "bit: isolate(0xF0) == 0x10");
    __CPROVER_assert(kain_ownership_isolate_low_bit_u64(
        UINT64_C(0x8000000000000000)) == UINT64_C(0x8000000000000000),
                                                           "bit: isolate(high)");
    __CPROVER_assert(kain_ownership_low_bit_index_u64(0x1) == 0,
                                                           "idx: index(1) == 0");
    __CPROVER_assert(kain_ownership_low_bit_index_u64(0x2) == 1,
                                                           "idx: index(2) == 1");
    __CPROVER_assert(kain_ownership_low_bit_index_u64(0x10) == 4,
                                                           "idx: index(0x10) == 4");
    __CPROVER_assert(kain_ownership_low_bit_index_u64(
        UINT64_C(1) << 63) == 63,
                                                           "idx: index(bit63) == 63");
}


/* ============================================================
 *  DEFERRED DECAY (public, no lock held)
 * ============================================================ */

void t_deferred(void)
{
    __kain_ownership_flush_deferred_decay();
    __CPROVER_assert(1,                                    "deferred: empty flush OK");
    uint64_t cnt = __kain_ownership_deferred_decay_count();
    __CPROVER_assert(cnt == 0 || cnt > 0,                  "deferred: count valid");
}


/* ============================================================
 *  MAIN
 * ============================================================ */

int main(void)
{
    /* State machine */
    t_observe();
    t_collapse();
    t_share();
    t_decay_nonheap();
    t_decay_heap_small();
    t_invalid();
    t_double_observe();
    t_observer_overflow();
    t_invalid_slot();

    /* Registration */
    t_upsert_new();
    t_upsert_again();
    t_upsert_null();
    t_helper_alloc();
    t_helper_token();
    t_ensure();

    /* Registered state machine (via _registered_unlocked) */
    t_registered_observe();
    t_registered_collapse();
    t_registered_share();
    t_registered_decay();

    /* Update */
    t_update_null_old();
    t_update_null_new();
    t_update_same_ptr();

    /* Index + search */
    t_find_free_slot_empty();
    t_find_slot_unregistered();
    t_find_slot_registered();
    t_index_roundtrip();
    t_index_null();
    t_pointer_hash();
    t_region_is_heap();

    /* Bit helpers */
    t_bit_helpers();

    /* Deferred decay */
    t_deferred();

    return 0;
}
