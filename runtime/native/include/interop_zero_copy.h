#ifndef KAIN_INTEROP_ZERO_COPY_H
#define KAIN_INTEROP_ZERO_COPY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*KainInteropZeroCopyReleaseFn)(void* state);

int64_t kain_interop_zero_copy_owner_create(
    void* state,
    KainInteropZeroCopyReleaseFn release_fn
);
int kain_interop_zero_copy_owner_is_valid(int64_t owner_handle);
void kain_interop_zero_copy_owner_retain(int64_t owner_handle);
void kain_interop_zero_copy_owner_release(int64_t owner_handle);

int kain_interop_zero_copy_prepare_imported_span(
    const unsigned char* bytes,
    int64_t byte_length,
    const char* lane,
    size_t* out_size
);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_INTEROP_ZERO_COPY_H */
