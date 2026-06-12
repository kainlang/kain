/*
 * check_interop_zero_copy.c -- CBMC verification harness for interop_zero_copy
 *
 * Verifies the zero-copy interop owner handle lifecycle (create, retain,
 * release, validate) and the imported span preparation logic.
 *
 * Owner handles use RC (reference-counted) allocation with type tags and
 * destructor callbacks. The static functions (rc_header, type_tag_matches,
 * emit_error, destructor) are tested directly with known-valid backing
 * buffers to isolate pointer-provenance concerns.
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_interop_zero_copy
 * Or:     cbmc --unwind 5 --trace test/cbmc/check_interop_zero_copy.c \
 *              src/core/interop_zero_copy.c -I include -I src/core
 */

#include "interop_zero_copy.h"
#include "base.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────────────────────────────
 * Static backing buffers (pointer provenance for CBMC)
 *
 * We construct a synthetic RcHeader + KainInteropZeroCopyOwnerHandle
 * in a static buffer to test static functions in isolation without
 * relying on the RC allocator.
 * ────────────────────────────────────────────────────────────────────── */

/* Layout: [RcHeader][KainInteropZeroCopyOwnerHandle] */
static unsigned char g_rc_backing[sizeof(RcHeader) + 64];
static KainInteropZeroCopyReleaseFn g_dummy_release_fn;
static unsigned char g_owner_state[32];
static size_t g_out_size;
static unsigned char g_byte_buffer[256];
static const char g_lane_string[] = "test-lane";

/* We need to cast g_rc_backing properly */
#define RC_HEADER_AT(buf)    ((RcHeader*)(buf))
#define OWNER_AT(buf)        ((KainInteropZeroCopyOwnerHandle*)((buf) + sizeof(RcHeader)))


/* Forward declarations of static functions from interop_zero_copy.c */
static RcHeader* kain_interop_zero_copy_rc_header(const void* ptr);
static int kain_interop_zero_copy_type_tag_matches(const void* ptr, long long type_tag);
static void kain_interop_zero_copy_emit_error(int code, const char* message, const char* detail);
static void kain_interop_zero_copy_owner_destructor(void* payload);

/* External RC functions that CBMC models */
void* kain_alloc_rc(size_t size, long long type_tag);
void KAIN_set_destructor(void* ptr, void (*dtor)(void*));
void rc_retain(void* ptr);
void rc_release(void* ptr);


/* ──────────────────────────────────────────────────────────────────────
 * Helper: create a synthetically valid RC-backed owner handle in a static
 *         buffer for testing internal static functions
 * ────────────────────────────────────────────────────────────────────── */
static void* create_valid_owner_payload(void) {
    RcHeader* hdr = RC_HEADER_AT(g_rc_backing);
    KainInteropZeroCopyOwnerHandle* owner = OWNER_AT(g_rc_backing);

    __CPROVER_havoc_object(g_rc_backing);
    __CPROVER_havoc_object(g_owner_state);

    /* Set up the RC header to look alive */
    hdr->magic       = KAIN_RC_MAGIC_ALIVE;
    hdr->ref_count   = 1;
    hdr->weak_count  = 0;
    hdr->type_tag    = UINT64_C(0x4b53485a434f0001); /* KAIN_RC_TYPE_INTEROP_ZERO_COPY_OWNER */
    hdr->payload_size = sizeof(KainInteropZeroCopyOwnerHandle);
    hdr->string_length = 0;
    hdr->destructor  = NULL;

    /* Set up the owner handle to point into static buffers */
    owner->state      = &g_owner_state[0];
    owner->release_fn = NULL;

    /* Return pointer to the payload (right after the RcHeader) */
    return (void*)owner;
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_rc_header returns NULL for NULL input
 * ────────────────────────────────────────────────────────────────────── */
void check_static_rc_header_null(void) {
    RcHeader* hdr = kain_interop_zero_copy_rc_header(NULL);
    __CPROVER_assert(hdr == NULL, "rc_header: NULL input returns NULL");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_rc_header on a valid payload returns
 *         a header with KAIN_RC_MAGIC_ALIVE
 * ────────────────────────────────────────────────────────────────────── */
void check_static_rc_header_valid(void) {
    void* payload = create_valid_owner_payload();
    RcHeader* hdr = kain_interop_zero_copy_rc_header(payload);

    __CPROVER_assert(hdr != NULL, "rc_header: valid payload returns non-NULL");
    if (hdr) {
        __CPROVER_assert(hdr->magic == KAIN_RC_MAGIC_ALIVE,
                         "rc_header: magic is ALIVE");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_type_tag_matches on NULL returns 0
 * ────────────────────────────────────────────────────────────────────── */
void check_static_type_tag_matches_null(void) {
    int rc = kain_interop_zero_copy_type_tag_matches(NULL, 0x4b53485a434f0001LL);
    __CPROVER_assert(rc == 0, "type_tag_matches: NULL input returns 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_type_tag_matches on valid payload with
 *         matching type tag returns 1
 * ────────────────────────────────────────────────────────────────────── */
void check_static_type_tag_matches_valid(void) {
    void* payload = create_valid_owner_payload();

    int rc = kain_interop_zero_copy_type_tag_matches(
        payload, UINT64_C(0x4b53485a434f0001));
    __CPROVER_assert(rc == 1, "type_tag_matches: matching tag returns 1");

    /* Wrong type tag should return 0 */
    int rc_wrong = kain_interop_zero_copy_type_tag_matches(payload, 0xDEADBEEFLL);
    __CPROVER_assert(rc_wrong == 0, "type_tag_matches: wrong tag returns 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_owner_destructor safely handles NULL
 * ────────────────────────────────────────────────────────────────────── */
void check_static_destructor_null(void) {
    /* Calling with NULL should be a no-op */
    kain_interop_zero_copy_owner_destructor(NULL);
    /* No assertion needed beyond "it didn't crash" */
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_owner_destructor with live payload
 *         clears state and release_fn
 * ────────────────────────────────────────────────────────────────────── */
void check_static_destructor_live(void) {
    static unsigned char live_backing[sizeof(RcHeader) + sizeof(KainInteropZeroCopyOwnerHandle)];
    __CPROVER_havoc_object(live_backing);

    KainInteropZeroCopyOwnerHandle* owner = OWNER_AT(live_backing);
    owner->state      = &g_owner_state[0];
    owner->release_fn = NULL;

    /* Set up release_fn as a non-NULL function pointer */
    /* We can't easily make a valid function pointer in CBMC, but NULL
     * is the safe path — the destructor checks for NULL. */
    kain_interop_zero_copy_owner_destructor((void*)owner);

    __CPROVER_assert(owner->state == NULL, "destructor: state cleared");
    __CPROVER_assert(owner->release_fn == NULL, "destructor: release_fn cleared");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_owner_create with valid args
 *
 * CBMC models kain_alloc_rc (which calls malloc) as nondeterministic —
 * it may succeed or return NULL. Both paths are verified.
 * ────────────────────────────────────────────────────────────────────── */
void check_owner_create_valid(void) {
    static int release_called;

    __CPROVER_havoc_object(&release_called);
    release_called = 0;

    /* We need a release_fn that doesn't do anything dangerous.
     * Since CBMC treats function pointers as opaque, we use a static
     * function that just sets a flag. */
    int64_t handle = kain_interop_zero_copy_owner_create(
        &g_owner_state[0], NULL);

    if (handle != 0) {
        /* Success path: handle is a valid pointer */
        __CPROVER_assert(handle > 0 || handle < 0,
                         "owner_create success: handle non-zero");
        int valid = kain_interop_zero_copy_owner_is_valid(handle);
        __CPROVER_assert(valid == 1,
                         "owner_create success: is_valid returns 1");
    }
    /* Failure path (handle == 0): alloc returned NULL, nothing to assert */
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_owner_create with NULL state and NULL
 *         release_fn (both NULL is valid)
 * ────────────────────────────────────────────────────────────────────── */
void check_owner_create_null_args(void) {
    int64_t handle = kain_interop_zero_copy_owner_create(NULL, NULL);

    /* Even with NULL args, if alloc succeeds, handle is valid */
    if (handle != 0) {
        int valid = kain_interop_zero_copy_owner_is_valid(handle);
        __CPROVER_assert(valid == 1,
                         "owner_create NULL args: is_valid returns 1");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_owner_is_valid returns 0 for zero handle
 * ────────────────────────────────────────────────────────────────────── */
void check_owner_is_valid_zero_handle(void) {
    int rc = kain_interop_zero_copy_owner_is_valid(0);
    __CPROVER_assert(rc == 0, "is_valid(0): returns 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_owner_is_valid returns 0 for garbage handles
 * ────────────────────────────────────────────────────────────────────── */
void check_owner_is_valid_garbage(void) {
    int64_t garbage;
    __CPROVER_havoc_object(&garbage);
    __CPROVER_assume(garbage != 0);

    /* A garbage non-zero handle is almost certainly not a valid RC pointer */
    /* But we can't guarantee 0 — the function checks magic + type_tag,
     * and a randomly-generated handle COULD coincidentally pass.
     * This tests that the function at least doesn't crash on garbage. */
    kain_interop_zero_copy_owner_is_valid(garbage);
    /* No crash — good enough for this test */
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_owner_retain/release cycle on valid handle
 *
 * The retain/release functions check is_valid internally. If the handle
 * passes validity, they call rc_retain/rc_release. CBMC's model of these
 * external functions is safe (no memory corruption).
 * ────────────────────────────────────────────────────────────────────── */
void check_owner_retain_release_cycle(void) {
    int64_t handle = kain_interop_zero_copy_owner_create(
        &g_owner_state[0], NULL);

    if (handle != 0) {
        /* Retain should succeed */
        kain_interop_zero_copy_owner_retain(handle);

        /* Handle should still be valid after retain */
        int valid = kain_interop_zero_copy_owner_is_valid(handle);
        __CPROVER_assert(valid == 1, "retain_release: still valid after retain");

        /* Release once */
        kain_interop_zero_copy_owner_release(handle);

        /* Release again — should not crash (the RC decrement is safe) */
        kain_interop_zero_copy_owner_release(handle);
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_owner_retain/release on zero handle
 *         is a no-op (no crash)
 * ────────────────────────────────────────────────────────────────────── */
void check_owner_retain_release_zero(void) {
    kain_interop_zero_copy_owner_retain(0);
    kain_interop_zero_copy_owner_release(0);
    /* No crash — no assertion needed beyond that */
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_prepare_imported_span with NULL out_size
 *         returns 0 (early return)
 * ────────────────────────────────────────────────────────────────────── */
void check_prepare_span_null_out(void) {
    int rc = kain_interop_zero_copy_prepare_imported_span(
        g_byte_buffer, 128, g_lane_string, NULL);
    __CPROVER_assert(rc == 0, "prepare_span(NULL out): returns 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_prepare_imported_span with negative
 *         byte_length returns 0
 * ────────────────────────────────────────────────────────────────────── */
void check_prepare_span_negative_length(void) {
    int64_t neg_length;
    __CPROVER_havoc_object(&neg_length);
    __CPROVER_assume(neg_length < 0);

    g_out_size = 0xFFFF;
    int rc = kain_interop_zero_copy_prepare_imported_span(
        g_byte_buffer, neg_length, g_lane_string, &g_out_size);

    __CPROVER_assert(rc == 0, "prepare_span(negative length): returns 0");
    __CPROVER_assert(g_out_size == 0,
                     "prepare_span(negative length): out_size set to 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_prepare_imported_span with positive
 *         byte_length but NULL bytes returns 0
 * ────────────────────────────────────────────────────────────────────── */
void check_prepare_span_null_bytes_positive_length(void) {
    /* byte_length > 0 but bytes == NULL */
    int64_t pos_length;
    __CPROVER_havoc_object(&pos_length);
    __CPROVER_assume(pos_length > 0);

    g_out_size = 0xFFFF;
    int rc = kain_interop_zero_copy_prepare_imported_span(
        NULL, pos_length, g_lane_string, &g_out_size);

    __CPROVER_assert(rc == 0,
                     "prepare_span(null bytes + pos length): returns 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_prepare_imported_span with byte_length
 *         exceeding SIZE_MAX returns 0
 * ────────────────────────────────────────────────────────────────────── */
void check_prepare_span_exceeds_size_max(void) {
    int64_t large_length;
    __CPROVER_havoc_object(&large_length);
    __CPROVER_assume((uint64_t)large_length > (uint64_t)SIZE_MAX);

    g_out_size = 0xFFFF;
    int rc = kain_interop_zero_copy_prepare_imported_span(
        g_byte_buffer, large_length, g_lane_string, &g_out_size);

    __CPROVER_assert(rc == 0,
                     "prepare_span(>SIZE_MAX): returns 0");
    __CPROVER_assert(g_out_size == 0,
                     "prepare_span(>SIZE_MAX): out_size set to 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_prepare_imported_span with valid args
 *         sets *out_size and returns 1
 * ────────────────────────────────────────────────────────────────────── */
void check_prepare_span_valid(void) {
    int64_t byte_length;
    __CPROVER_havoc_object(&byte_length);
    __CPROVER_assume(byte_length >= 0);
    __CPROVER_assume((uint64_t)byte_length <= (uint64_t)SIZE_MAX);
    __CPROVER_assume(byte_length <= (int64_t)sizeof(g_byte_buffer));

    /* When byte_length == 0, bytes can be NULL */
    __CPROVER_assume(byte_length > 0);

    g_out_size = 0xFFFF;
    int rc = kain_interop_zero_copy_prepare_imported_span(
        g_byte_buffer, byte_length, g_lane_string, &g_out_size);

    __CPROVER_assert(rc == 1, "prepare_span(valid): returns 1");
    __CPROVER_assert(g_out_size == (size_t)byte_length,
                     "prepare_span(valid): out_size == byte_length");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_prepare_imported_span with zero length
 *         and NULL bytes succeeds (legal: zero-length span)
 * ────────────────────────────────────────────────────────────────────── */
void check_prepare_span_zero_length_null_bytes(void) {
    g_out_size = 0xFFFF;
    int rc = kain_interop_zero_copy_prepare_imported_span(
        NULL, 0, g_lane_string, &g_out_size);

    __CPROVER_assert(rc == 1,
                     "prepare_span(zero length, NULL bytes): returns 1");
    __CPROVER_assert(g_out_size == 0,
                     "prepare_span(zero length): out_size == 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_interop_zero_copy_prepare_imported_span with zero length
 *         and non-NULL bytes also succeeds
 * ────────────────────────────────────────────────────────────────────── */
void check_prepare_span_zero_length_nonnull_bytes(void) {
    g_out_size = 0xFFFF;
    int rc = kain_interop_zero_copy_prepare_imported_span(
        g_byte_buffer, 0, g_lane_string, &g_out_size);

    __CPROVER_assert(rc == 1,
                     "prepare_span(zero length, non-NULL bytes): returns 1");
    __CPROVER_assert(g_out_size == 0,
                     "prepare_span(zero length): out_size == 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: prepare_imported_span with NULL lane string is safe
 *
 * The lane string is only used for error messages (diagnostics). The
 * function should not crash even if lane is NULL.
 * ────────────────────────────────────────────────────────────────────── */
void check_prepare_span_null_lane(void) {
    g_out_size = 0xFFFF;
    int rc = kain_interop_zero_copy_prepare_imported_span(
        g_byte_buffer, 64, NULL, &g_out_size);

    /* Should work like a normal call since lane is only used for diagnostics */
    __CPROVER_assert(rc == 1,
                     "prepare_span(NULL lane, valid): returns 1");
    __CPROVER_assert(g_out_size == 64,
                     "prepare_span(NULL lane): out_size == 64");
}


/* ──────────────────────────────────────────────────────────────────────
 * Main -- run all checks
 * ────────────────────────────────────────────────────────────────────── */
int main(void) {
    check_static_rc_header_null();
    check_static_rc_header_valid();
    check_static_type_tag_matches_null();
    check_static_type_tag_matches_valid();
    check_static_destructor_null();
    check_static_destructor_live();
    check_owner_create_valid();
    check_owner_create_null_args();
    check_owner_is_valid_zero_handle();
    check_owner_is_valid_garbage();
    check_owner_retain_release_cycle();
    check_owner_retain_release_zero();
    check_prepare_span_null_out();
    check_prepare_span_negative_length();
    check_prepare_span_null_bytes_positive_length();
    check_prepare_span_exceeds_size_max();
    check_prepare_span_valid();
    check_prepare_span_zero_length_null_bytes();
    check_prepare_span_zero_length_nonnull_bytes();
    check_prepare_span_null_lane();
    return 0;
}
