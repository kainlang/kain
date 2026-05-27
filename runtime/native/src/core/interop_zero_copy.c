#include "../../include/interop_zero_copy.h"

#include "../../include/base.h"
#include "../../include/diagnostics.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#define KAIN_RC_TYPE_INTEROP_ZERO_COPY_OWNER UINT64_C(0x4b53485a434f0001)

typedef struct KainInteropZeroCopyOwnerHandle {
    void* state;
    KainInteropZeroCopyReleaseFn release_fn;
} KainInteropZeroCopyOwnerHandle;

void* kain_alloc_rc(size_t size, long long type_tag);
void KAIN_set_destructor(void* ptr, void (*dtor)(void*));

static RcHeader* kain_interop_zero_copy_rc_header(const void* ptr) {
    return ptr ? (((RcHeader*)ptr) - 1) : NULL;
}

static int kain_interop_zero_copy_type_tag_matches(const void* ptr, long long type_tag) {
    RcHeader* header = kain_interop_zero_copy_rc_header(ptr);
    return header != NULL &&
        header->magic == KAIN_RC_MAGIC_ALIVE &&
        header->type_tag == type_tag;
}

static void kain_interop_zero_copy_emit_error(int code, const char* message, const char* detail) {
    KainDiagnostic diag;
    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_HOST_BRIDGE,
        KAIN_DIAG_SEVERITY_ERROR,
        code,
        message,
        detail,
        "runtime/native/src/core/interop_zero_copy.c"
    );
    kain_diagnostic_print(&diag);
}

static void kain_interop_zero_copy_owner_destructor(void* payload) {
    KainInteropZeroCopyOwnerHandle* owner = (KainInteropZeroCopyOwnerHandle*)payload;
    if (!owner || !owner->release_fn) {
        return;
    }
    owner->release_fn(owner->state);
    owner->state = NULL;
    owner->release_fn = NULL;
}

int64_t kain_interop_zero_copy_owner_create(
    void* state,
    KainInteropZeroCopyReleaseFn release_fn
) {
    KainInteropZeroCopyOwnerHandle* owner;
    owner = (KainInteropZeroCopyOwnerHandle*)kain_alloc_rc(
        sizeof(KainInteropZeroCopyOwnerHandle),
        KAIN_RC_TYPE_INTEROP_ZERO_COPY_OWNER
    );
    if (!owner) {
        if (release_fn) {
            release_fn(state);
        }
        return 0;
    }
    owner->state = state;
    owner->release_fn = release_fn;
    KAIN_set_destructor(owner, kain_interop_zero_copy_owner_destructor);
    return (int64_t)(intptr_t)owner;
}

int kain_interop_zero_copy_owner_is_valid(int64_t owner_handle) {
    return owner_handle != 0 &&
        kain_interop_zero_copy_type_tag_matches(
            (void*)(intptr_t)owner_handle,
            KAIN_RC_TYPE_INTEROP_ZERO_COPY_OWNER
        );
}

void kain_interop_zero_copy_owner_retain(int64_t owner_handle) {
    if (kain_interop_zero_copy_owner_is_valid(owner_handle)) {
        rc_retain((void*)(intptr_t)owner_handle);
    }
}

void kain_interop_zero_copy_owner_release(int64_t owner_handle) {
    if (kain_interop_zero_copy_owner_is_valid(owner_handle)) {
        rc_release((void*)(intptr_t)owner_handle);
    }
}

int kain_interop_zero_copy_prepare_imported_span(
    const unsigned char* bytes,
    int64_t byte_length,
    const char* lane,
    size_t* out_size
) {
    size_t span_size;
    if (out_size) {
        *out_size = 0u;
    }
    if (!out_size) {
        return 0;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-interop-zero-copy-span-cast-to-size_t-is-lossless-under-guard.yaml */
    if (byte_length < 0) {
        kain_interop_zero_copy_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Zero-copy interop span has a negative byte length",
            lane
        );
        return 0;
    }
    if (byte_length > 0 && bytes == NULL) {
        kain_interop_zero_copy_emit_error(
            KAIN_DIAG_CODE_MEMORY_INVALID_POINTER,
            "Zero-copy interop span is missing its foreign byte pointer",
            lane
        );
        return 0;
    }
    if ((uint64_t)byte_length > (uint64_t)SIZE_MAX) {
        kain_interop_zero_copy_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Zero-copy interop span does not fit the native size_t domain",
            lane
        );
        return 0;
    }
    span_size = (size_t)byte_length;
    *out_size = span_size;
    return 1;
}
