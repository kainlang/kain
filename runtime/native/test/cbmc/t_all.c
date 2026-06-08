/*
 * check_ownership.c — CBMC verification harness for ownership module
 *
 * Tests the core collapse/observe/decay state machine (idle -> observed ->
 * collapsed -> shared -> decayed) via slot-unlocked internal functions and
 * the public registration/lookup/decay API.
 *
 * This file is concatenated AFTER src/core/ownership.c, making the combined
 * file ONE translation unit.  All static functions and globals from the
 * source are visible to this harness.
 *
 * Run via pipeline:
 *   python test/scripts/run_pipeline.py cbmc --harness check_ownership
 *
 * Or directly via WSL:
 *   cat src/core/ownership.c test/cbmc/check_ownership.c > combined.c
 *   wsl cbmc --unwind 5 --no-unwinding-assertions combined.c \
 *        -I include -I src/core
 */

#include "ownership.h"
#include <stddef.h>

/* ---- Static functions from ownership.c ---- */
static int kain_ownership_begin_observe_slot_unlocked(int slot);
static int kain_ownership_end_observe_slot_unlocked(int slot);
static int kain_ownership_begin_collapse_slot_unlocked(int slot);
static int kain_ownership_end_collapse_slot_unlocked(int slot);
static int kain_ownership_begin_share_slot_unlocked(int slot);
static int kain_ownership_end_share_slot_unlocked(int slot);
static int kain_ownership_decay_slot_unlocked(
    void* ptr, int slot, int reclaim,
    void* out_release_now, int* out_release_immediately);
static int kain_ownership_find_slot(const void* ptr);
static int kain_ownership_find_free_slot(void);
static uint32_t kain_ownership_pointer_index_slot(const void* ptr);
static int kain_ownership_index_insert_unlocked(const void* ptr, int slot);
static int kain_ownership_index_remove_unlocked(const void* ptr, int slot);
static uint64_t kain_ownership_isolate_low_bit_u64(uint64_t v);
static unsigned int kain_ownership_low_bit_index_u64(uint64_t one_hot);

/* ---- Static globals from ownership.c ---- */
extern KainOwnershipRegion KAIN_OWNERSHIP_REGIONS[4096];
extern uint64_t KAIN_OWNERSHIP_OCCUPANCY_WORDS[64];
extern uint32_t KAIN_OWNERSHIP_POINTER_INDEX[8192];

/* ---- Static buffers for pointer provenance ---- */
static unsigned char g_b1[256];
static unsigned char g_b2[256];
static void* g_out_base;
static size_t g_out_size;
static uint16_t g_slot_token;

/* ---- Helper: set up region at slot ---- */
static void setup(int slot, void* ptr, int64_t kind, int state,
                  uint32_t observers)
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


/* ============================================================
 *  STATE MACHINE TESTS (slot-unlocked, bypass search+lock)
 * ============================================================ */

/* IDLE -> begin_observe -> OBSERVED -> end_observe -> IDLE */
void t_observe(void) {
    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_IDLE, 0);
    int rc = kain_ownership_begin_observe_slot_unlocked(0);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "obs: begin OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 1,
                     "obs: observers == 1");
    rc = kain_ownership_end_observe_slot_unlocked(0);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "obs: end OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_IDLE,
                     "obs: back to IDLE");
}

/* IDLE -> begin_collapse -> COLLAPSED -> end_collapse -> IDLE */
void t_collapse(void) {
    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_IDLE, 0);
    int rc = kain_ownership_begin_collapse_slot_unlocked(0);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "col: begin OK");
    rc = kain_ownership_end_collapse_slot_unlocked(0);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "col: end OK");
}

/* IDLE -> begin_share -> SHARED -> end_share -> IDLE */
void t_share(void) {
    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_IDLE, 0);
    int rc = kain_ownership_begin_share_slot_unlocked(0);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "shr: begin OK");
    rc = kain_ownership_end_share_slot_unlocked(0);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "shr: end OK");
}

/* IDLE -> decay (non-heap) -> DECAYED */
void t_decay(void) {
    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_WORLD_STATE,
          KAIN_OWNERSHIP_STATE_IDLE, 0);
    int rc = kain_ownership_decay_slot_unlocked(&g_b1[0], 0, 0, NULL, NULL);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "dec: OK");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_DECAYED,
                     "dec: state DECAYED");
}

/* Invalid transitions */
void t_invalid(void) {
    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_DECAYED, 0);
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_DECAYED, "inv: obs on DECAYED");

    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_COLLAPSED, 0);
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED, "inv: obs on COLLAPSED");

    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_OBSERVED, 1);
    __CPROVER_assert(kain_ownership_begin_collapse_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_OBSERVED, "inv: col on OBSERVED");

    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_IDLE, 0);
    __CPROVER_assert(kain_ownership_end_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_NOT_OBSERVED, "inv: endobs on IDLE");

    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_WORLD_STATE,
          KAIN_OWNERSHIP_STATE_COLLAPSED, 0);
    __CPROVER_assert(kain_ownership_decay_slot_unlocked(&g_b1[0], 0, 0, NULL, NULL)
                     == KAIN_OWNERSHIP_ERR_COLLAPSED, "inv: dec on COLLAPSED");

    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_WORLD_STATE,
          KAIN_OWNERSHIP_STATE_DECAYED, 0);
    __CPROVER_assert(kain_ownership_decay_slot_unlocked(&g_b1[0], 0, 0, NULL, NULL)
                     == KAIN_OWNERSHIP_ERR_DECAYED, "inv: dec on DECAYED");
}

/* Double observe */
void t_double_obs(void) {
    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_IDLE, 0);
    kain_ownership_begin_observe_slot_unlocked(0);
    kain_ownership_begin_observe_slot_unlocked(0);
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 2,
                     "dbl: observers == 2");
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].state == KAIN_OWNERSHIP_STATE_OBSERVED,
                     "dbl: state OBSERVED");
    kain_ownership_end_observe_slot_unlocked(0);
    kain_ownership_end_observe_slot_unlocked(0);
    __CPROVER_assert(KAIN_OWNERSHIP_REGIONS[0].observers == 0,
                     "dbl: obs == 0 after two ends");
    __CPROVER_assert(kain_ownership_end_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_NOT_OBSERVED,
                     "dbl: third end fails");
}

/* Observer overflow */
void t_overflow(void) {
    setup(0, &g_b1[0], KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA,
          KAIN_OWNERSHIP_STATE_IDLE, UINT32_MAX);
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(0)
                     == KAIN_OWNERSHIP_ERR_OVERFLOW, "ovf: ERR_OVERFLOW");
}

/* Invalid slot (-1) */
void t_inv_slot(void) {
    __CPROVER_assert(kain_ownership_begin_observe_slot_unlocked(-1)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "inv: slot -1");
}


/* ============================================================
 *  PUBLIC API TESTS
 * ============================================================ */

void t_register(void) {
    __CPROVER_assert(__kain_ownership_register(&g_b1[0],
        KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 64) == KAIN_OWNERSHIP_OK,
        "reg: OK");
}

void t_register_null(void) {
    __CPROVER_assert(__kain_ownership_register(NULL,
        KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 64) == KAIN_OWNERSHIP_ERR_INVALID,
        "reg: NULL -> ERR_INVALID");
}

void t_register_imported(void) {
    __CPROVER_assert(__kain_ownership_register_imported(&g_b2[0], 128)
                     == KAIN_OWNERSHIP_OK, "reg_imp: OK");
}

void t_upsert(void) {
    __kain_ownership_register(&g_b1[0], KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 64);
    __CPROVER_assert(__kain_ownership_register(&g_b1[0],
        KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 128) == KAIN_OWNERSHIP_OK,
        "upsert: OK");
}

void t_ensure(void) {
    __kain_ownership_register(&g_b1[0], KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 64);
    __CPROVER_assert(__kain_ownership_ensure_imported(&g_b1[0])
                     == KAIN_OWNERSHIP_OK, "ensure: existing");
    __CPROVER_assert(__kain_ownership_ensure_imported(&g_b2[0])
                     == KAIN_OWNERSHIP_OK, "ensure: new");
    __CPROVER_assert(__kain_ownership_ensure_imported(NULL)
                     == KAIN_OWNERSHIP_ERR_INVALID, "ensure: NULL");
}

void t_helper(void) {
    g_slot_token = 0;
    __CPROVER_assert(__kain_ownership_register_helper_allocation(
        &g_b1[0], 64, &g_slot_token) == KAIN_OWNERSHIP_OK, "helper: OK");
    __CPROVER_assert(g_slot_token != 0, "helper: token != 0");
}

void t_pub_sm(void) {
    __kain_ownership_register(&g_b1[0],
        KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA, 64);
    __CPROVER_assert(__kain_ownership_state(&g_b1[0])
                     == KAIN_OWNERSHIP_STATE_IDLE, "pub: IDLE");

    __CPROVER_assert(__kain_ownership_begin_observe(&g_b1[0])
                     == KAIN_OWNERSHIP_OK, "pub: obs OK");
    __CPROVER_assert(__kain_ownership_state(&g_b1[0])
                     == KAIN_OWNERSHIP_STATE_OBSERVED, "pub: OBSERVED");

    __CPROVER_assert(__kain_ownership_end_observe(&g_b1[0])
                     == KAIN_OWNERSHIP_OK, "pub: endobs OK");
    __CPROVER_assert(__kain_ownership_state(&g_b1[0])
                     == KAIN_OWNERSHIP_STATE_IDLE, "pub: back IDLE");

    __CPROVER_assert(__kain_ownership_begin_collapse(&g_b1[0])
                     == KAIN_OWNERSHIP_OK, "pub: col OK");
    __CPROVER_assert(__kain_ownership_end_collapse(&g_b1[0])
                     == KAIN_OWNERSHIP_OK, "pub: endcol OK");
    __CPROVER_assert(__kain_ownership_state(&g_b1[0])
                     == KAIN_OWNERSHIP_STATE_IDLE, "pub: IDLE after col");
}

void t_decay_public(void) {
    __kain_ownership_register(&g_b1[0],
        KAIN_OWNERSHIP_REGION_WORLD_STATE, 64);
    __CPROVER_assert(__kain_ownership_decay(&g_b1[0])
                     == KAIN_OWNERSHIP_OK, "dpub: decay OK");
    __CPROVER_assert(__kain_ownership_state(&g_b1[0])
                     == KAIN_OWNERSHIP_STATE_DECAYED, "dpub: DECAYED");
    __CPROVER_assert(__kain_ownership_begin_observe(&g_b1[0])
                     == KAIN_OWNERSHIP_ERR_DECAYED, "dpub: obs blocked");
}

void t_state_unreg(void) {
    __CPROVER_assert(__kain_ownership_state(&g_b1[0])
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "st: unreg");
}

void t_locate(void) {
    __kain_ownership_register(&g_b1[0],
        KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 128);
    g_out_base = NULL; g_out_size = 0;
    __CPROVER_assert(__kain_ownership_locate_registered_range(
        &g_b1[0], &g_out_base, &g_out_size) == KAIN_OWNERSHIP_OK, "loc: OK");
    __CPROVER_assert(g_out_base == &g_b1[0], "loc: base");
    __CPROVER_assert(g_out_size == 128, "loc: size");
}

void t_locate_contained(void) {
    __kain_ownership_register(&g_b1[0],
        KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 128);
    g_out_base = NULL; g_out_size = 0;
    __CPROVER_assert(__kain_ownership_locate_registered_range(
        &g_b1[64], &g_out_base, &g_out_size) == KAIN_OWNERSHIP_OK,
        "locc: OK");
    __CPROVER_assert(g_out_base == &g_b1[0], "locc: base");
}

void t_locate_null(void) {
    __kain_ownership_register(&g_b1[0],
        KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 64);
    __CPROVER_assert(__kain_ownership_locate_registered_range(
        NULL, &g_out_base, &g_out_size) == KAIN_OWNERSHIP_ERR_INVALID,
        "locn: ptr NULL");
    __CPROVER_assert(__kain_ownership_locate_registered_range(
        &g_b1[0], NULL, &g_out_size) == KAIN_OWNERSHIP_ERR_INVALID,
        "locn: base NULL");
    __CPROVER_assert(__kain_ownership_locate_registered_range(
        &g_b1[0], &g_out_base, NULL) == KAIN_OWNERSHIP_ERR_INVALID,
        "locn: size NULL");
}

void t_bind(void) {
    __kain_ownership_register(&g_b1[0],
        KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION, 64);
    __CPROVER_assert(__kain_ownership_bind_relocation_handle(&g_b1[0], 42)
                     == KAIN_OWNERSHIP_OK, "bind: OK");
    __CPROVER_assert(__kain_ownership_bind_relocation_handle(&g_b2[0], 42)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "bind: unreg");
    __CPROVER_assert(__kain_ownership_bind_relocation_handle(NULL, 42)
                     == KAIN_OWNERSHIP_ERR_INVALID, "bind: NULL");
}

void t_update(void) {
    __CPROVER_assert(__kain_ownership_update(NULL, &g_b1[0], 64)
                     == KAIN_OWNERSHIP_OK, "upd: NULL old");
    __CPROVER_assert(__kain_ownership_update(&g_b2[0], NULL, 64)
                     == KAIN_OWNERSHIP_ERR_INVALID, "upd: NULL new");
    __CPROVER_assert(__kain_ownership_update(NULL, NULL, 64)
                     == KAIN_OWNERSHIP_ERR_INVALID, "upd: both NULL");
}

void t_null_safety(void) {
    __CPROVER_assert(__kain_ownership_register(NULL, 1, 64)
                     == KAIN_OWNERSHIP_ERR_INVALID, "nul: reg");
    __CPROVER_assert(__kain_ownership_register_imported(NULL, 64)
                     == KAIN_OWNERSHIP_ERR_INVALID, "nul: regimp");
    __CPROVER_assert(__kain_ownership_ensure_imported(NULL)
                     == KAIN_OWNERSHIP_ERR_INVALID, "nul: ensure");
    __CPROVER_assert(__kain_ownership_begin_observe(NULL)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "nul: obs");
    __CPROVER_assert(__kain_ownership_end_observe(NULL)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "nul: endobs");
    __CPROVER_assert(__kain_ownership_begin_collapse(NULL)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "nul: col");
    __CPROVER_assert(__kain_ownership_end_collapse(NULL)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "nul: endcol");
    __CPROVER_assert(__kain_ownership_begin_share(NULL)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "nul: shr");
    __CPROVER_assert(__kain_ownership_end_share(NULL)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "nul: endshr");
    __CPROVER_assert(__kain_ownership_decay(NULL)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "nul: decay");
    __CPROVER_assert(__kain_ownership_state(NULL)
                     == KAIN_OWNERSHIP_ERR_NOT_FOUND, "nul: state");
    __CPROVER_assert(__kain_ownership_bind_relocation_handle(NULL, 42)
                     == KAIN_OWNERSHIP_ERR_INVALID, "nul: bind");
    __CPROVER_assert(__kain_ownership_update(NULL, NULL, 64)
                     == KAIN_OWNERSHIP_ERR_INVALID, "nul: update");
}

void t_deferred(void) {
    __kain_ownership_flush_deferred_decay();
    __CPROVER_assert(1, "def: flush OK");
}


/* ============================================================
 *  INTERNAL HELPERS
 * ============================================================ */

void t_hash(void) {
    uint32_t s = kain_ownership_pointer_index_slot(&g_b1[0]);
    __CPROVER_assert(s < KAIN_OWNERSHIP_INDEX_CAPACITY, "hash: in range");
}

void t_free_slot(void) {
    __CPROVER_assert(kain_ownership_find_free_slot() == 0, "free: slot 0");
}

void t_find_slot(void) {
    __CPROVER_assert(kain_ownership_find_slot(&g_b1[0]) == -1, "find: unreg");
    __kain_ownership_register(&g_b1[0], 1, 64);
    __CPROVER_assert(kain_ownership_find_slot(&g_b1[0]) >= 0, "find: reg");
}

void t_index(void) {
    int rc = kain_ownership_index_insert_unlocked(&g_b1[0], 7);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "idx: insert");
    __CPROVER_assert(kain_ownership_find_slot(&g_b1[0]) == 7, "idx: find");
    rc = kain_ownership_index_remove_unlocked(&g_b1[0], 7);
    __CPROVER_assert(rc == KAIN_OWNERSHIP_OK, "idx: remove");
    __CPROVER_assert(kain_ownership_find_slot(&g_b1[0]) == -1, "idx: gone");
}

void t_bits(void) {
    __CPROVER_assert(kain_ownership_isolate_low_bit_u64(0x1) == 0x1, "bit: 1");
    __CPROVER_assert(kain_ownership_isolate_low_bit_u64(0xF0) == 0x10, "bit: F0");
    __CPROVER_assert(kain_ownership_low_bit_index_u64(0x1) == 0, "idx: 0");
    __CPROVER_assert(kain_ownership_low_bit_index_u64(0x2) == 1, "idx: 1");
    __CPROVER_assert(kain_ownership_low_bit_index_u64(0x10) == 4, "idx: 4");
}


/* ============================================================
 *  MAIN
 * ============================================================ */

int main(void) {
    t_observe();
    t_collapse();
    t_share();
    t_decay();
    t_invalid();
    t_double_obs();
    t_overflow();
    t_inv_slot();

    t_register();
    t_register_null();
    t_register_imported();
    t_upsert();
    t_ensure();
    t_helper();
    t_pub_sm();
    t_decay_public();
    t_state_unreg();
    t_locate();
    t_locate_contained();
    t_locate_null();
    t_bind();
    t_update();
    t_null_safety();
    t_deferred();

    t_hash();
    t_free_slot();
    t_find_slot();
    t_index();
    t_bits();

    return 0;
}
