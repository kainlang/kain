#ifndef KAIN_INTEROP_CONTRACTS_H
#define KAIN_INTEROP_CONTRACTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t kain_shared_buffer_from_bytes(
    int64_t bytes,
    char* element_type,
    int64_t shape,
    char* format,
    char* mime_type
);
int64_t kain_shared_buffer_info(int64_t target);
int64_t kain_shared_buffer_byte_length(int64_t target);
int64_t kain_shared_buffer_element_count_value(int64_t target);
int64_t kain_shared_buffer_element_size(int64_t target);
int64_t kain_shared_buffer_zero_copy_flag(int64_t target);
int64_t kain_shared_buffer_shared_ownership(int64_t target);
void kain_shared_buffer_release(int64_t target);
int64_t kain_shared_buffer_bytes(int64_t target);
void kain_shared_buffer_replace_bytes(int64_t target, int64_t bytes);
void kain_shared_buffer_set_adoption_metadata(
    int64_t target,
    const char* adoption_path,
    const char* fallback_reason
);

int64_t kain_shared_image_from_bytes(
    int64_t bytes,
    int64_t width,
    int64_t height,
    int64_t channels,
    char* layout,
    char* pixel_format,
    char* mime_type
);
int64_t kain_shared_image_info(int64_t target);
int64_t kain_shared_image_bytes(int64_t target);
void kain_shared_image_replace_bytes(int64_t target, int64_t bytes);

int64_t kain_shared_buffer_create_owned(
    const unsigned char* bytes,
    int64_t byte_length,
    const char* element_type,
    int64_t element_size,
    int64_t shape,
    int64_t strides,
    const char* format,
    const char* mime_type,
    const char* source_runtime,
    const char* source_backend,
    const char* ownership,
    int64_t labels
);

int64_t kain_shared_buffer_create_borrowed(
    const unsigned char* bytes,
    int64_t byte_length,
    const char* element_type,
    int64_t element_size,
    int64_t shape,
    int64_t strides,
    const char* format,
    const char* mime_type,
    const char* source_runtime,
    const char* source_backend,
    const char* ownership,
    int64_t labels,
    int64_t zero_copy_owner
);

int64_t kain_shared_image_create_owned(
    const unsigned char* bytes,
    int64_t byte_length,
    int64_t width,
    int64_t height,
    int64_t channels,
    const char* layout,
    const char* pixel_format,
    const char* mime_type,
    int64_t row_stride,
    const char* representation,
    const char* color_space,
    const char* alpha_mode,
    const char* source_runtime,
    const char* source_backend,
    const char* ownership,
    int64_t labels,
    int64_t shape,
    int64_t strides
);

int64_t kain_shared_image_create_borrowed(
    const unsigned char* bytes,
    int64_t byte_length,
    int64_t width,
    int64_t height,
    int64_t channels,
    const char* layout,
    const char* pixel_format,
    const char* mime_type,
    int64_t row_stride,
    const char* representation,
    const char* color_space,
    const char* alpha_mode,
    const char* source_runtime,
    const char* source_backend,
    const char* ownership,
    int64_t labels,
    int64_t shape,
    int64_t strides,
    int64_t zero_copy_owner
);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_INTEROP_CONTRACTS_H */
