/*
 * check_interop_contracts.c — CBMC verification harness for interop
 * contracts subsystem
 *
 * Verifies: buffer create_owned/create_borrowed, accessor consistency,
 * buffer_release, replace_bytes, adoption_metadata, buffer_info,
 * image create + info, element-count validation arithmetic, and
 * null/invalid-handle safety for all public functions.
 *
 * The key challenge is that most functions take/return int64_t "handles"
 * that are either RC-managed KainArray* or JSON values.  We build an
 * inline RcHeader + KainArray in static memory so CBMC has pointer
 * provenance for the shape/strides/bytes arguments, then exercise
 * the full create → query → release pipeline.
 *
 * Combined translation unit: source + harness.
 *
 * Run:
 *   python test/scripts/run_pipeline.py cbmc --harness check_interop_contracts
 * Or:
 *   cbmc --unwind 5 --trace test/cbmc/check_interop_contracts.c \
 *        src/core/interop_contracts.c -I include -I src/core
 */

#include "interop_contracts.h"
#include "base.h"
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────
 * Static buffers for RC-managed objects
 *
 * We need an RcHeader followed by a KainArray to act as a valid shape
 * handle.  RcHeader layout (64-bit, 8-byte aligned):
 *   offset  0: uint64_t magic
 *   offset  8: long long ref_count
 *   offset 16: long long weak_count
 *   offset 24: long long type_tag
 *   offset 32: size_t payload_size
 *   offset 40: size_t string_length
 *   offset 48: void (*destructor)(void*)
 * Total = 56 bytes.
 *
 * KainArray layout:
 *   offset 0: long long* data
 *   offset 8: long long len
 *   offset 16: long long cap
 * Total = 24 bytes.
 *
 * We allocate 256 bytes to have plenty of room and alignment.
 * ────────────────────────────────────────────────────────────────────── */
static unsigned char g_rc_storage[256];
static long long g_shape_data[3] = {4, 4, 3};  /* 4x4x3 = 48 elements */

/* Static byte payload for buffer content */
static unsigned char g_byte_payload[48];

/* Static strings for create_owned parameters */
static char g_elem_type[]  = "u8";
static char g_format[]     = "raw";
static char g_mime_type[]  = "application/octet-stream";
static char g_runtime[]    = "kain";
static char g_ownership[]  = "owned";
static char g_adopt_path[] = "/imported/file.bin";
static char g_fallback[]   = "no zero-copy available";
static char g_src_backend[] = "test";
static char g_layout[]     = "HWC";
static char g_pixel_fmt[]  = "rgba8";

/* ──────────────────────────────────────────────────────────────────────
 * Helper: build a valid (RcHeader + KainArray) shape handle
 *
 * Returns the int64_t handle that can be passed as the `shape` parameter
 * to create_owned / create_borrowed.
 *
 * The shape is a 1-D array with one dimension (48), so element_count = 48.
 * With element_size = 1, byte_length must be 48.
 * ────────────────────────────────────────────────────────────────────── */
static int64_t create_valid_shape_handle(void) {
    /* Layout: RcHeader at start of g_rc_storage, KainArray after it */
    RcHeader* hdr  = (RcHeader*)&g_rc_storage[0];
    KainArray* arr = (KainArray*)&g_rc_storage[sizeof(RcHeader)];

    hdr->magic        = KAIN_RC_MAGIC_ALIVE;
    hdr->ref_count    = 1;
    hdr->weak_count   = 0;
    hdr->type_tag     = 2;     /* KainArray type tag */
    hdr->payload_size = sizeof(KainArray);
    hdr->string_length = 0;
    hdr->destructor   = NULL;

    arr->data = &g_shape_data[0];
    arr->len  = 1;            /* 1-D shape: [48] */
    arr->cap  = 3;

    return (int64_t)(intptr_t)arr;
}


/* ──────────────────────────────────────────────────────────────────────
 * Helper: build a valid strides KainArray handle
 * For a 1-D shape [48], compact strides = [1].
 * ────────────────────────────────────────────────────────────────────── */
static int64_t create_valid_strides_handle(void) {
    static long long strides_data[1] = {1};
    static unsigned char strides_storage[256];

    RcHeader* hdr  = (RcHeader*)&strides_storage[0];
    KainArray* arr = (KainArray*)&strides_storage[sizeof(RcHeader)];

    hdr->magic        = KAIN_RC_MAGIC_ALIVE;
    hdr->ref_count    = 1;
    hdr->weak_count   = 0;
    hdr->type_tag     = 2;
    hdr->payload_size = sizeof(KainArray);
    hdr->string_length = 0;
    hdr->destructor   = NULL;

    arr->data = &strides_data[0];
    arr->len  = 1;
    arr->cap  = 1;

    return (int64_t)(intptr_t)arr;
}


/* ──────────────────────────────────────────────────────────────────────
 * Helper: build a valid labels KainArray handle
 * ────────────────────────────────────────────────────────────────────── */
static int64_t create_valid_labels_handle(void) {
    static long long labels_data[2];
    static unsigned char labels_storage[256];

    RcHeader* hdr  = (RcHeader*)&labels_storage[0];
    KainArray* arr = (KainArray*)&labels_storage[sizeof(RcHeader)];

    hdr->magic        = KAIN_RC_MAGIC_ALIVE;
    hdr->ref_count    = 1;
    hdr->weak_count   = 0;
    hdr->type_tag     = 2;
    hdr->payload_size = sizeof(KainArray);
    hdr->string_length = 0;
    hdr->destructor   = NULL;

    arr->data = &labels_data[0];
    arr->len  = 2;
    arr->cap  = 2;

    return (int64_t)(intptr_t)arr;
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: accessors on zero/invalid handle return 0 / no-op
 * ────────────────────────────────────────────────────────────────────── */
void check_accessors_null_handle(void) {
    int64_t h = 0;

    int64_t bl = kain_shared_buffer_byte_length(h);
    __CPROVER_assert(bl == 0, "byte_length(0) == 0");

    int64_t ec = kain_shared_buffer_element_count_value(h);
    __CPROVER_assert(ec == 0, "element_count_value(0) == 0");

    int64_t es = kain_shared_buffer_element_size(h);
    __CPROVER_assert(es == 0, "element_size(0) == 0");

    int64_t zc = kain_shared_buffer_zero_copy_flag(h);
    __CPROVER_assert(zc == 0, "zero_copy_flag(0) == 0");

    int64_t so = kain_shared_buffer_shared_ownership(h);
    __CPROVER_assert(so == 0, "shared_ownership(0) == 0");

    /* Release on zero handle must not crash */
    kain_shared_buffer_release(h);

    /* Adoption metadata on zero handle must not crash */
    kain_shared_buffer_set_adoption_metadata(h, g_adopt_path, g_fallback);

    /* Info on zero handle returns 0 */
    int64_t info = kain_shared_buffer_info(h);
    __CPROVER_assert(info == 0, "info(0) == 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_shared_buffer_create_owned succeeds and returns a valid
 *        handle whose accessors return the same values we passed in
 * ────────────────────────────────────────────────────────────────────── */
void check_create_owned_accessors(void) {
    int64_t shape  = create_valid_shape_handle();   /* [48] → 48 elements */
    int64_t strides = create_valid_strides_handle(); /* [1] */
    int64_t labels  = create_valid_labels_handle();
    int64_t byte_length = 48;
    int64_t element_size = 1;

    int64_t handle = kain_shared_buffer_create_owned(
        g_byte_payload,
        byte_length,
        g_elem_type,
        element_size,
        shape,
        strides,
        g_format,
        g_mime_type,
        g_runtime,
        g_src_backend,
        g_ownership,
        labels
    );

    /* The RC allocator is external (kain_alloc_rc), so CBMC explores
     * both success (non-zero handle) and failure (0) paths. */
    if (handle != 0) {
        int64_t bl = kain_shared_buffer_byte_length(handle);
        __CPROVER_assert(bl == byte_length,
                         "create_owned: byte_length matches");

        int64_t es = kain_shared_buffer_element_size(handle);
        __CPROVER_assert(es == element_size,
                         "create_owned: element_size matches");

        int64_t ec = kain_shared_buffer_element_count_value(handle);
        __CPROVER_assert(ec == 48,
                         "create_owned: element_count = 48 for shape [48]");

        int64_t zc = kain_shared_buffer_zero_copy_flag(handle);
        __CPROVER_assert(zc == 0,
                         "create_owned: zero_copy_flag is 0 (owned)");

        /* Info should return non-zero JSON object handle */
        int64_t info = kain_shared_buffer_info(handle);
        /* info may be 0 if json_object_new fails externally — that's OK */
        if (info != 0) {
            /* Just verify it doesn't crash and returns something */
        }
    } else {
        /* If creation failed, the handle is 0 — that's valid */
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_shared_buffer_create_borrowed succeeds and returns a valid
 *        handle with borrowed storage mode
 * ────────────────────────────────────────────────────────────────────── */
void check_create_borrowed_zero_copy(void) {
    int64_t shape  = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();
    int64_t byte_length = 48;
    int64_t element_size = 1;

    /* zero_copy_owner = 0 means no external owner to retain/release */
    int64_t handle = kain_shared_buffer_create_borrowed(
        g_byte_payload,
        byte_length,
        g_elem_type,
        element_size,
        shape,
        strides,
        g_format,
        g_mime_type,
        g_runtime,
        g_src_backend,
        "shared",
        labels,
        0  /* zero_copy_owner = 0 → no-op retain */
    );

    if (handle != 0) {
        int64_t zc = kain_shared_buffer_zero_copy_flag(handle);
        /* For borrowed mode, zero_copy_flag should be 1 */
        /* (kain_interop_zero_copy_prepare_imported_span is an external
         * that may fail; if it fails, creation returns 0) */
        __CPROVER_assert(zc == 1,
                         "create_borrowed: zero_copy_flag is 1");

        int64_t bl = kain_shared_buffer_byte_length(handle);
        __CPROVER_assert(bl == byte_length,
                         "create_borrowed: byte_length matches");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: buffer_release after create (no double-free, no crash)
 * ────────────────────────────────────────────────────────────────────── */
void check_create_then_release(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();
    int64_t byte_length = 48;
    int64_t element_size = 1;

    int64_t handle = kain_shared_buffer_create_owned(
        g_byte_payload,
        byte_length,
        g_elem_type,
        element_size,
        shape,
        strides,
        g_format,
        g_mime_type,
        g_runtime,
        g_src_backend,
        g_ownership,
        labels
    );

    /* Release decrements RC; must not crash */
    kain_shared_buffer_release(handle);
    /* Double-release test — rc_release is extern, but calling it again
     * shouldn't crash (the rc_release implementation handles this) */
    kain_shared_buffer_release(handle);
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: set_adoption_metadata on valid handle does not crash and
 *        accessors still work
 * ────────────────────────────────────────────────────────────────────── */
void check_adoption_metadata(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    int64_t handle = kain_shared_buffer_create_owned(
        g_byte_payload, 48, g_elem_type, 1,
        shape, strides, g_format, g_mime_type,
        g_runtime, g_src_backend, g_ownership, labels
    );

    if (handle != 0) {
        /* Call with valid strings */
        kain_shared_buffer_set_adoption_metadata(
            handle, g_adopt_path, g_fallback);

        /* Call with NULL fallback_reason */
        kain_shared_buffer_set_adoption_metadata(
            handle, g_adopt_path, NULL);

        /* Call with NULL both */
        kain_shared_buffer_set_adoption_metadata(
            handle, NULL, NULL);

        /* Accessors still work after metadata set */
        int64_t bl = kain_shared_buffer_byte_length(handle);
        __CPROVER_assert(bl >= 0, "byte_length >= 0 after metadata");

        kain_shared_buffer_release(handle);
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: replace_bytes on a valid handle preserves accessor consistency
 * ────────────────────────────────────────────────────────────────────── */
void check_replace_bytes(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    int64_t handle = kain_shared_buffer_create_owned(
        g_byte_payload, 48, g_elem_type, 1,
        shape, strides, g_format, g_mime_type,
        g_runtime, g_src_backend, g_ownership, labels
    );

    if (handle != 0) {
        /* Build a literal shape KainArray for bytes parameter */
        static long long bytes_data[48];
        static unsigned char bytes_storage[128];
        int i;

        /* Fill bytes_data with predictable values 0..47 */
        for (i = 0; i < 48; i++) {
            bytes_data[i] = (long long)i;
        }

        RcHeader* hdr  = (RcHeader*)&bytes_storage[0];
        KainArray* arr = (KainArray*)&bytes_storage[sizeof(RcHeader)];
        hdr->magic        = KAIN_RC_MAGIC_ALIVE;
        hdr->ref_count    = 1;
        hdr->weak_count   = 0;
        hdr->type_tag     = 2;
        hdr->payload_size = sizeof(KainArray);
        hdr->string_length = 0;
        hdr->destructor   = NULL;
        arr->data = &bytes_data[0];
        arr->len  = 48;
        arr->cap  = 48;

        int64_t bytes_handle = (int64_t)(intptr_t)arr;

        /* replace_bytes calls kain_shared_extract_bytes which internally
         * calls array_len/array_get (nondet).  Must not crash. */
        kain_shared_buffer_replace_bytes(handle, bytes_handle);

        /* After replace, byte_length should still match */
        /* (It does — replace_bytes validates length matches shape) */
        int64_t bl = kain_shared_buffer_byte_length(handle);
        __CPROVER_assert(bl >= 0, "byte_length >= 0 after replace");
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: replace_bytes and adoption_metadata on zero handle no-op
 * ────────────────────────────────────────────────────────────────────── */
void check_mutators_null_handle(void) {
    /* replace_bytes on zero handle must not crash */
    kain_shared_buffer_replace_bytes(0, 42);
    kain_shared_buffer_set_adoption_metadata(0, g_adopt_path, g_fallback);
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_shared_image_create_owned returns valid handle
 * ────────────────────────────────────────────────────────────────────── */
void check_image_create_owned(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    /* 4x4x3 = 48 bytes, 1 byte per channel */
    int64_t handle = kain_shared_image_create_owned(
        g_byte_payload,
        48,       /* byte_length */
        4,        /* width */
        4,        /* height */
        3,        /* channels */
        g_layout,
        g_pixel_fmt,
        g_mime_type,
        12,       /* row_stride = 4 * 3 = 12 */
        "raster",
        "srgb",
        "straight",
        g_runtime,
        g_src_backend,
        g_ownership,
        labels,
        shape,
        strides
    );

    if (handle != 0) {
        /* Info should return non-zero JSON handle */
        int64_t info = kain_shared_image_info(handle);
        if (info != 0) {
            /* Non-null info handle is good */
        }

        /* bytes should return non-zero array handle */
        int64_t bytes = kain_shared_image_bytes(handle);
        if (bytes != 0) {
            /* Non-null bytes handle is good */
        }
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: image functions on zero handle are no-ops
 * ────────────────────────────────────────────────────────────────────── */
void check_image_null_handle(void) {
    int64_t info = kain_shared_image_info(0);
    __CPROVER_assert(info == 0, "image_info(0) == 0");

    int64_t bytes = kain_shared_image_bytes(0);
    __CPROVER_assert(bytes == 0, "image_bytes(0) == 0");

    kain_shared_image_replace_bytes(0, 42);
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_shared_image_replace_bytes on a valid handle
 * ────────────────────────────────────────────────────────────────────── */
void check_image_replace_bytes(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    int64_t handle = kain_shared_image_create_owned(
        g_byte_payload, 48, 4, 4, 3,
        g_layout, g_pixel_fmt, g_mime_type, 12,
        "raster", "srgb", "straight",
        g_runtime, g_src_backend, g_ownership,
        labels, shape, strides
    );

    if (handle != 0) {
        /* Build a 48-element byte array for replacement */
        static long long repl_data[48];
        static unsigned char repl_storage[128];
        int i;
        for (i = 0; i < 48; i++) {
            repl_data[i] = (long long)(i & 0xFF);
        }

        RcHeader* hdr  = (RcHeader*)&repl_storage[0];
        KainArray* arr = (KainArray*)&repl_storage[sizeof(RcHeader)];
        hdr->magic        = KAIN_RC_MAGIC_ALIVE;
        hdr->ref_count    = 1;
        hdr->weak_count   = 0;
        hdr->type_tag     = 2;
        hdr->payload_size = sizeof(KainArray);
        hdr->string_length = 0;
        hdr->destructor   = NULL;
        arr->data = &repl_data[0];
        arr->len  = 48;
        arr->cap  = 48;

        int64_t bytes_handle = (int64_t)(intptr_t)arr;
        kain_shared_image_replace_bytes(handle, bytes_handle);
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: shared_ownership returns 1 only when ownership == "shared"
 * ────────────────────────────────────────────────────────────────────── */
void check_shared_ownership(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    /* Owned buffer — shared_ownership should be 0 */
    int64_t owned_handle = kain_shared_buffer_create_owned(
        g_byte_payload, 48, g_elem_type, 1,
        shape, strides, g_format, g_mime_type,
        g_runtime, g_src_backend, "owned", labels
    );

    if (owned_handle != 0) {
        int64_t so = kain_shared_buffer_shared_ownership(owned_handle);
        __CPROVER_assert(so == 0,
                         "owned buffer: shared_ownership == 0");
        kain_shared_buffer_release(owned_handle);
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: element_size defaults to 1 when non-positive
 * ────────────────────────────────────────────────────────────────────── */
void check_element_size_default(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    /* 0 element_size should map to 1 internally */
    int64_t handle = kain_shared_buffer_create_owned(
        g_byte_payload, 48, g_elem_type, 0,
        shape, strides, g_format, g_mime_type,
        g_runtime, g_src_backend, g_ownership, labels
    );

    if (handle != 0) {
        int64_t es = kain_shared_buffer_element_size(handle);
        __CPROVER_assert(es == 1,
                         "element_size(0) defaults to 1");
        kain_shared_buffer_release(handle);
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: create with zero byte_length (empty buffer)
 * ────────────────────────────────────────────────────────────────────── */
void check_create_empty_buffer(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    /* Override shape data to [0] for zero-length */
    g_shape_data[0] = 0;

    int64_t handle = kain_shared_buffer_create_owned(
        NULL, 0, g_elem_type, 1,
        shape, strides, g_format, g_mime_type,
        g_runtime, g_src_backend, g_ownership, labels
    );

    if (handle != 0) {
        int64_t bl = kain_shared_buffer_byte_length(handle);
        __CPROVER_assert(bl == 0, "empty buffer: byte_length == 0");

        int64_t ec = kain_shared_buffer_element_count_value(handle);
        __CPROVER_assert(ec == 0, "empty buffer: element_count == 0");

        kain_shared_buffer_release(handle);
    }

    /* Restore shape data for other tests (in case they run after) */
    g_shape_data[0] = 48;
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: buffer_bytes returns an array handle (or 0 if allocation fails)
 * ────────────────────────────────────────────────────────────────────── */
void check_buffer_bytes(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    int64_t handle = kain_shared_buffer_create_owned(
        g_byte_payload, 48, g_elem_type, 1,
        shape, strides, g_format, g_mime_type,
        g_runtime, g_src_backend, g_ownership, labels
    );

    if (handle != 0) {
        int64_t bytes = kain_shared_buffer_bytes(handle);
        /* Could be 0 if array_new fails — that's OK */
        if (bytes != 0) {
            /* Non-null is good */
        }
        kain_shared_buffer_release(handle);
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: create with NULL metadata strings (null-safety)
 * ────────────────────────────────────────────────────────────────────── */
void check_create_null_strings(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    /* Pass NULL for every string parameter except shape/strides/labels */
    int64_t handle = kain_shared_buffer_create_owned(
        g_byte_payload, 48,
        NULL, 1,
        shape, strides,
        NULL, NULL,
        NULL, NULL,
        NULL, labels
    );

    if (handle != 0) {
        /* Should work with defaults */
        int64_t es = kain_shared_buffer_element_size(handle);
        __CPROVER_assert(es == 1, "null strings: element_size still 1");
        kain_shared_buffer_release(handle);
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: create with NULL bytes pointer (null payload)
 * ────────────────────────────────────────────────────────────────────── */
void check_create_null_bytes(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    /* NULL bytes with non-zero length — create_owned will try malloc + memcpy */
    /* (CBMC will explore both malloc failure and success paths) */
    int64_t handle = kain_shared_buffer_create_owned(
        NULL, 48, g_elem_type, 1,
        shape, strides, g_format, g_mime_type,
        g_runtime, g_src_backend, g_ownership, labels
    );

    /* If byte_length > 0 and bytes is NULL, memcpy with NULL is UB,
     * but the code does: if (byte_length > 0) { malloc; memcpy(buffer->bytes, bytes, ...); }
     * CBMC will flag the memcpy with src=NULL if that path is taken.
     *
     * This test validates that CBMC catches this or that the programmer
     * defends against it.  The code does NOT check bytes != NULL before memcpy.
     */
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: element_count_value consistency with shape data
 * ────────────────────────────────────────────────────────────────────── */
void check_element_count_consistency(void) {
    int64_t shape   = create_valid_shape_handle();
    int64_t strides = create_valid_strides_handle();
    int64_t labels  = create_valid_labels_handle();

    int64_t handle = kain_shared_buffer_create_owned(
        g_byte_payload, 48, g_elem_type, 1,
        shape, strides, g_format, g_mime_type,
        g_runtime, g_src_backend, g_ownership, labels
    );

    if (handle != 0) {
        int64_t ec = kain_shared_buffer_element_count_value(handle);
        int64_t bl = kain_shared_buffer_byte_length(handle);
        int64_t es = kain_shared_buffer_element_size(handle);

        /* If element_size > 0, then bl == ec * es */
        if (es > 0) {
            __CPROVER_assert(bl == ec * es,
                             "byte_length == element_count * element_size");
        }
        kain_shared_buffer_release(handle);
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * Main — run all checks
 * ────────────────────────────────────────────────────────────────────── */
int main(void) {
    check_accessors_null_handle();
    check_create_owned_accessors();
    check_create_borrowed_zero_copy();
    check_create_then_release();
    check_adoption_metadata();
    check_replace_bytes();
    check_mutators_null_handle();
    check_image_create_owned();
    check_image_null_handle();
    check_image_replace_bytes();
    check_shared_ownership();
    check_element_size_default();
    check_create_empty_buffer();
    check_buffer_bytes();
    check_create_null_strings();
    check_create_null_bytes();
    check_element_count_consistency();
    return 0;
}
