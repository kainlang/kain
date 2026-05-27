#include "../../include/interop_contracts.h"

#include "../../include/base.h"
#include "../../include/diagnostics.h"
#include "../../include/json.h"
#include "../../include/interop_zero_copy.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KAIN_RC_TYPE_SHARED_BUFFER UINT64_C(0x4b53484255460001)
#define KAIN_RC_TYPE_SHARED_IMAGE  UINT64_C(0x4b5348494d470001)
#define KAIN_SHARED_JSON_INT(value)  ((((int64_t)(value)) << 3) | 1LL)
#define KAIN_SHARED_JSON_NULL        4LL
#define KAIN_SHARED_CONTRACT_VERSION 1LL
#define KAIN_SHARED_STORAGE_OWNED    0
#define KAIN_SHARED_STORAGE_BORROWED 1

typedef struct KainSharedBufferHandle {
    unsigned char* bytes;
    int64_t byte_length;
    char* element_type;
    int64_t element_size;
    int64_t shape;
    int64_t strides;
    char* format;
    char* mime_type;
    char* source_runtime;
    char* source_backend;
    char* ownership;
    int64_t labels;
    int64_t zero_copy_owner;
    int storage_mode;
} KainSharedBufferHandle;

typedef struct KainSharedImageHandle {
    KainSharedBufferHandle* buffer;
    int64_t width;
    int64_t height;
    int64_t channels;
    int64_t row_stride;
    char* layout;
    char* pixel_format;
    char* mime_type;
    char* representation;
    char* color_space;
    char* alpha_mode;
} KainSharedImageHandle;

void* kain_alloc_rc(size_t size, long long type_tag);
void KAIN_set_destructor(void* ptr, void (*dtor)(void*));
KainArray* array_new(long long cap);
void array_push(KainArray* arr, long long val);
long long array_get(KainArray* arr, long long index);
long long array_len(KainArray* arr);

static RcHeader* kain_shared_rc_header(const void* ptr) {
    return ptr ? (((RcHeader*)ptr) - 1) : NULL;
}

static int kain_shared_type_tag_matches(const void* ptr, long long type_tag) {
    RcHeader* header = kain_shared_rc_header(ptr);
    return header != NULL &&
        header->magic == KAIN_RC_MAGIC_ALIVE &&
        header->type_tag == type_tag;
}

static KainSharedBufferHandle* kain_shared_as_buffer_handle(int64_t value) {
    return kain_shared_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_SHARED_BUFFER)
        ? (KainSharedBufferHandle*)(intptr_t)value
        : NULL;
}

static KainSharedImageHandle* kain_shared_as_image_handle(int64_t value) {
    return kain_shared_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_SHARED_IMAGE)
        ? (KainSharedImageHandle*)(intptr_t)value
        : NULL;
}

static void kain_shared_emit_error(int code, const char* message, const char* detail) {
    KainDiagnostic diag;
    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_HOST_BRIDGE,
        KAIN_DIAG_SEVERITY_ERROR,
        code,
        message,
        detail,
        "runtime/native/src/core/interop_contracts.c"
    );
    kain_diagnostic_print(&diag);
}

static char* kain_shared_dup_cstr(const char* text) {
    size_t length;
    char* out;
    if (!text) {
        return NULL;
    }
    length = strlen(text);
    out = (char*)malloc(length + 1u);
    if (!out) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Shared interop allocation failed",
            "Failed to duplicate metadata text"
        );
        return NULL;
    }
    memcpy(out, text, length + 1u);
    return out;
}

static void kain_shared_retain_any_handle(int64_t value) {
    if (value == 0 || value == KAIN_SHARED_JSON_NULL) {
        return;
    }
    if ((value & 7LL) == 0) {
        rc_retain((void*)(intptr_t)value);
        return;
    }
    json_retain(value);
}

static void kain_shared_release_any_handle(int64_t value) {
    if (value == 0 || value == KAIN_SHARED_JSON_NULL) {
        return;
    }
    if ((value & 7LL) == 0) {
        rc_release((void*)(intptr_t)value);
        return;
    }
    json_release(value);
}

static int kain_shared_json_kind(int64_t value) {
    return json_any_kind(value);
}

static int kain_shared_array_len_any(int64_t value, int64_t* out_len) {
    KainArray* array;
    if (!out_len) {
        return 0;
    }
    array = ((value & 7LL) == 0) ? (KainArray*)(intptr_t)value : NULL;
    if (array && kain_shared_type_tag_matches(array, 2)) {
        *out_len = array_len(array);
        return 1;
    }
    if (kain_shared_json_kind(value) == KAIN_JSON_KIND_ARRAY) {
        *out_len = json_array_len(value);
        return 1;
    }
    return 0;
}

static int kain_shared_array_get_int_any(int64_t value, int64_t index, int64_t* out_value) {
    KainArray* array;
    if (!out_value || index < 0) {
        return 0;
    }
    array = ((value & 7LL) == 0) ? (KainArray*)(intptr_t)value : NULL;
    if (array && kain_shared_type_tag_matches(array, 2)) {
        if (index >= array_len(array)) {
            return 0;
        }
        *out_value = array_get(array, index);
        return 1;
    }
    if (kain_shared_json_kind(value) == KAIN_JSON_KIND_ARRAY) {
        if (index >= json_array_len(value)) {
            return 0;
        }
        *out_value = json_any_to_int(json_array_get(value, index));
        return 1;
    }
    return 0;
}

static int kain_shared_checked_mul_i64(int64_t left, int64_t right, int64_t* out_value) {
    if (!out_value || left < 0 || right < 0) {
        return 0;
    }
    if (left == 0 || right == 0) {
        *out_value = 0;
        return 1;
    }
    if (left > (LLONG_MAX / right)) {
        return 0;
    }
    *out_value = left * right;
    return 1;
}

static int kain_shared_element_count(int64_t shape, int64_t* out_count) {
    int64_t len;
    int64_t index;
    int64_t product = 1;
    if (!out_count) {
        return 0;
    }
    if (!kain_shared_array_len_any(shape, &len)) {
        return 0;
    }
    if (len == 0) {
        *out_count = 0;
        return 1;
    }
    for (index = 0; index < len; ++index) {
        int64_t dim = 0;
        if (!kain_shared_array_get_int_any(shape, index, &dim) || dim < 0) {
            return 0;
        }
        if (!kain_shared_checked_mul_i64(product, dim, &product)) {
            return 0;
        }
    }
    *out_count = product;
    return 1;
}

static int64_t kain_shared_compact_strides(int64_t shape) {
    int64_t len;
    int64_t index;
    int64_t stride = 1;
    KainArray* strides;
    if (!kain_shared_array_len_any(shape, &len)) {
        return 0;
    }
    if (len <= 0) {
        strides = array_new(1);
        if (!strides) {
            return 0;
        }
        array_push(strides, 1);
        return (int64_t)(intptr_t)strides;
    }
    strides = array_new(len);
    if (!strides) {
        return 0;
    }
    for (index = 0; index < len; ++index) {
        array_push(strides, 0);
    }
    for (index = len - 1; index >= 0; --index) {
        int64_t dim = 0;
        if (!kain_shared_array_get_int_any(shape, index, &dim)) {
            rc_release(strides);
            return 0;
        }
        ((KainArray*)strides)->data[index] = stride;
        if (!kain_shared_checked_mul_i64(stride, dim > 0 ? dim : 1, &stride)) {
            rc_release(strides);
            return 0;
        }
        if (index == 0) {
            break;
        }
    }
    return (int64_t)(intptr_t)strides;
}

static int kain_shared_extract_bytes(int64_t bytes_value, unsigned char** out_bytes, int64_t* out_len) {
    int64_t len;
    int64_t index;
    unsigned char* bytes;
    if (!out_bytes || !out_len) {
        return 0;
    }
    *out_bytes = NULL;
    *out_len = 0;
    if (!kain_shared_array_len_any(bytes_value, &len) || len < 0) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared interop expected Array<Int> bytes",
            "The bytes lane must be an array of integers in the u8 range"
        );
        return 0;
    }
    bytes = len > 0 ? (unsigned char*)malloc((size_t)len) : NULL;
    if (len > 0 && !bytes) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Shared interop allocation failed",
            "Failed to allocate shared byte snapshot"
        );
        return 0;
    }
    for (index = 0; index < len; ++index) {
        int64_t value = 0;
        if (!kain_shared_array_get_int_any(bytes_value, index, &value) || value < 0 || value > 255) {
            free(bytes);
            kain_shared_emit_error(
                KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
                "Shared interop byte value out of range",
                "Each byte entry must be an integer between 0 and 255"
            );
            return 0;
        }
        bytes[index] = (unsigned char)value;
    }
    *out_bytes = bytes;
    *out_len = len;
    return 1;
}

static int64_t kain_shared_build_labels2(const char* first, const char* second) {
    KainArray* labels = array_new(2);
    if (!labels) {
        return 0;
    }
    array_push(labels, (int64_t)(intptr_t)string_new((char*)(first ? first : "")));
    array_push(labels, (int64_t)(intptr_t)string_new((char*)(second ? second : "")));
    return (int64_t)(intptr_t)labels;
}

static void kain_shared_json_set_string_required(int64_t object, const char* key, const char* value) {
    char* text = string_new((char*)(value ? value : ""));
    int64_t tagged = ((int64_t)(intptr_t)text) | 3LL;
    json_object_set(object, key, tagged);
    rc_release(text);
}

static void kain_shared_json_set_string_optional(int64_t object, const char* key, const char* value) {
    if (!value || !value[0]) {
        json_object_set(object, key, KAIN_SHARED_JSON_NULL);
        return;
    }
    kain_shared_json_set_string_required(object, key, value);
}

static void kain_shared_buffer_set_ownership(KainSharedBufferHandle* buffer, const char* ownership) {
    char* next_ownership;
    if (!buffer) {
        return;
    }
    next_ownership = kain_shared_dup_cstr(ownership ? ownership : "owned");
    if (!next_ownership && ownership && ownership[0]) {
        return;
    }
    free(buffer->ownership);
    buffer->ownership = next_ownership;
}

static void kain_shared_buffer_drop_zero_copy_owner(KainSharedBufferHandle* buffer) {
    if (!buffer || buffer->zero_copy_owner == 0) {
        return;
    }
    kain_interop_zero_copy_owner_release(buffer->zero_copy_owner);
    buffer->zero_copy_owner = 0;
}

static void kain_shared_buffer_destructor(void* payload) {
    KainSharedBufferHandle* buffer = (KainSharedBufferHandle*)payload;
    if (!buffer) {
        return;
    }
    if (buffer->storage_mode == KAIN_SHARED_STORAGE_OWNED) {
        free(buffer->bytes);
    } else {
        kain_shared_buffer_drop_zero_copy_owner(buffer);
    }
    free(buffer->element_type);
    free(buffer->format);
    free(buffer->mime_type);
    free(buffer->source_runtime);
    free(buffer->source_backend);
    free(buffer->ownership);
    kain_shared_release_any_handle(buffer->shape);
    kain_shared_release_any_handle(buffer->strides);
    kain_shared_release_any_handle(buffer->labels);
}

static void kain_shared_image_destructor(void* payload) {
    KainSharedImageHandle* image = (KainSharedImageHandle*)payload;
    if (!image) {
        return;
    }
    free(image->layout);
    free(image->pixel_format);
    free(image->mime_type);
    free(image->representation);
    free(image->color_space);
    free(image->alpha_mode);
    if (image->buffer) {
        rc_release(image->buffer);
        image->buffer = NULL;
    }
}

static int kain_shared_validate_buffer_length(
    int64_t shape,
    int64_t element_size,
    int64_t byte_length,
    const char* lane
) {
    int64_t element_count = 0;
    int64_t expected = 0;
    /* Proof: runtime/native/src/core/z3/proofs/native-interop-shared-buffer-byte-length-matches-shape-times-element-size.yaml */
    if (element_size <= 0 || byte_length < 0) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared buffer metadata is invalid",
            lane
        );
        return 0;
    }
    if (!kain_shared_element_count(shape, &element_count) ||
        !kain_shared_checked_mul_i64(element_count, element_size, &expected)) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared buffer shape arithmetic failed",
            lane
        );
        return 0;
    }
    if (expected != byte_length) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared buffer byte length mismatch",
            lane
        );
        return 0;
    }
    return 1;
}

static int kain_shared_validate_image_length(
    int64_t width,
    int64_t height,
    int64_t channels,
    int64_t row_stride,
    int64_t byte_length,
    const char* lane
) {
    int64_t computed_row_stride;
    int64_t expected = 0;
    /* Proof: runtime/native/src/core/z3/proofs/native-interop-shared-image-raster-byte-length-matches-height-times-row-stride.yaml */
    if (width < 0 || height < 0 || channels < 0 || byte_length < 0) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared image metadata is invalid",
            lane
        );
        return 0;
    }
    if (row_stride > 0) {
        computed_row_stride = row_stride;
    } else if (!kain_shared_checked_mul_i64(width, channels > 0 ? channels : 1, &computed_row_stride)) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared image row-stride arithmetic failed",
            lane
        );
        return 0;
    }
    if (computed_row_stride < 0 ||
        !kain_shared_checked_mul_i64(height, computed_row_stride, &expected)) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared image row-stride arithmetic failed",
            lane
        );
        return 0;
    }
    if (expected != byte_length) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared image byte length mismatch",
            lane
        );
        return 0;
    }
    return 1;
}

static int64_t kain_shared_buffer_element_count(const KainSharedBufferHandle* buffer) {
    int64_t element_count = 0;
    if (!buffer) {
        return 0;
    }
    if (!kain_shared_element_count(buffer->shape, &element_count)) {
        return 0;
    }
    return element_count;
}

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
) {
    KainSharedBufferHandle* buffer;
    size_t owned_size = 0u;
    if (!shape) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared buffer shape is required",
            "kain_shared_buffer_create_owned requires a shape handle"
        );
        return 0;
    }
    if (!kain_shared_validate_buffer_length(
            shape,
            element_size,
            byte_length,
            "kain_shared_buffer_create_owned"
        )) {
        return 0;
    }
    if (byte_length < 0 || (uint64_t)byte_length > (uint64_t)SIZE_MAX) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared buffer byte length does not fit the native size_t domain",
            "kain_shared_buffer_create_owned"
        );
        return 0;
    }
    owned_size = (size_t)byte_length;
    buffer = (KainSharedBufferHandle*)kain_alloc_rc(sizeof(KainSharedBufferHandle), KAIN_RC_TYPE_SHARED_BUFFER);
    if (!buffer) {
        return 0;
    }
    memset(buffer, 0, sizeof(*buffer));
    if (byte_length > 0) {
        buffer->bytes = (unsigned char*)malloc(owned_size);
        if (!buffer->bytes) {
            rc_release(buffer);
            kain_shared_emit_error(
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "Shared buffer allocation failed",
                "Failed to allocate shared buffer bytes"
            );
            return 0;
        }
        memcpy(buffer->bytes, bytes, owned_size);
    }
    buffer->byte_length = byte_length;
    buffer->element_type = kain_shared_dup_cstr(element_type ? element_type : "u8");
    buffer->element_size = element_size > 0 ? element_size : 1;
    buffer->shape = shape;
    buffer->strides = strides;
    buffer->format = kain_shared_dup_cstr(format);
    buffer->mime_type = kain_shared_dup_cstr(mime_type);
    buffer->source_runtime = kain_shared_dup_cstr(source_runtime ? source_runtime : "kain");
    buffer->source_backend = kain_shared_dup_cstr(source_backend);
    buffer->ownership = kain_shared_dup_cstr(ownership ? ownership : "owned");
    buffer->labels = labels;
    buffer->zero_copy_owner = 0;
    buffer->storage_mode = KAIN_SHARED_STORAGE_OWNED;
    kain_shared_retain_any_handle(buffer->shape);
    kain_shared_retain_any_handle(buffer->strides);
    kain_shared_retain_any_handle(buffer->labels);
    KAIN_set_destructor(buffer, kain_shared_buffer_destructor);
    return (int64_t)(intptr_t)buffer;
}

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
) {
    KainSharedBufferHandle* buffer;
    size_t borrowed_size = 0u;
    if (!shape) {
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared buffer shape is required",
            "kain_shared_buffer_create_borrowed requires a shape handle"
        );
        return 0;
    }
    if (!kain_shared_validate_buffer_length(
            shape,
            element_size,
            byte_length,
            "kain_shared_buffer_create_borrowed"
        )) {
        return 0;
    }
    if (!kain_interop_zero_copy_prepare_imported_span(
            bytes,
            byte_length,
            "kain_shared_buffer_create_borrowed",
            &borrowed_size
        )) {
        return 0;
    }
    buffer = (KainSharedBufferHandle*)kain_alloc_rc(sizeof(KainSharedBufferHandle), KAIN_RC_TYPE_SHARED_BUFFER);
    if (!buffer) {
        return 0;
    }
    memset(buffer, 0, sizeof(*buffer));
    buffer->bytes = (unsigned char*)bytes;
    buffer->byte_length = (int64_t)borrowed_size;
    buffer->element_type = kain_shared_dup_cstr(element_type ? element_type : "u8");
    buffer->element_size = element_size > 0 ? element_size : 1;
    buffer->shape = shape;
    buffer->strides = strides;
    buffer->format = kain_shared_dup_cstr(format);
    buffer->mime_type = kain_shared_dup_cstr(mime_type);
    buffer->source_runtime = kain_shared_dup_cstr(source_runtime ? source_runtime : "kain");
    buffer->source_backend = kain_shared_dup_cstr(source_backend);
    buffer->ownership = kain_shared_dup_cstr(ownership ? ownership : "shared");
    buffer->labels = labels;
    buffer->zero_copy_owner = zero_copy_owner;
    buffer->storage_mode = KAIN_SHARED_STORAGE_BORROWED;
    kain_shared_retain_any_handle(buffer->shape);
    kain_shared_retain_any_handle(buffer->strides);
    kain_shared_retain_any_handle(buffer->labels);
    kain_interop_zero_copy_owner_retain(buffer->zero_copy_owner);
    KAIN_set_destructor(buffer, kain_shared_buffer_destructor);
    return (int64_t)(intptr_t)buffer;
}

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
) {
    KainSharedImageHandle* image;
    int64_t buffer_handle_value;
    KainSharedBufferHandle* buffer_handle;
    if (!kain_shared_validate_image_length(
            width,
            height,
            channels,
            row_stride,
            byte_length,
            "kain_shared_image_create_owned"
        )) {
        return 0;
    }
    buffer_handle_value = kain_shared_buffer_create_owned(
        bytes,
        byte_length,
        "u8",
        1,
        shape,
        strides,
        pixel_format,
        mime_type,
        source_runtime,
        source_backend,
        ownership,
        labels
    );
    if (!buffer_handle_value) {
        return 0;
    }
    buffer_handle = (KainSharedBufferHandle*)(intptr_t)buffer_handle_value;
    image = (KainSharedImageHandle*)kain_alloc_rc(sizeof(KainSharedImageHandle), KAIN_RC_TYPE_SHARED_IMAGE);
    if (!image) {
        rc_release(buffer_handle);
        return 0;
    }
    memset(image, 0, sizeof(*image));
    image->buffer = buffer_handle;
    image->width = width;
    image->height = height;
    image->channels = channels;
    image->row_stride = row_stride;
    image->layout = kain_shared_dup_cstr(layout ? layout : "HWC");
    image->pixel_format = kain_shared_dup_cstr(pixel_format ? pixel_format : "rgba8");
    image->mime_type = kain_shared_dup_cstr(mime_type ? mime_type : "image/x-kain-raster");
    image->representation = kain_shared_dup_cstr(representation ? representation : "raster");
    image->color_space = kain_shared_dup_cstr(color_space ? color_space : "srgb");
    image->alpha_mode = kain_shared_dup_cstr(alpha_mode ? alpha_mode : "opaque");
    KAIN_set_destructor(image, kain_shared_image_destructor);
    return (int64_t)(intptr_t)image;
}

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
) {
    KainSharedImageHandle* image;
    int64_t buffer_handle_value;
    KainSharedBufferHandle* buffer_handle;
    if (!kain_shared_validate_image_length(
            width,
            height,
            channels,
            row_stride,
            byte_length,
            "kain_shared_image_create_borrowed"
        )) {
        return 0;
    }
    buffer_handle_value = kain_shared_buffer_create_borrowed(
        bytes,
        byte_length,
        "u8",
        1,
        shape,
        strides,
        pixel_format,
        mime_type,
        source_runtime,
        source_backend,
        ownership,
        labels,
        zero_copy_owner
    );
    if (!buffer_handle_value) {
        return 0;
    }
    buffer_handle = (KainSharedBufferHandle*)(intptr_t)buffer_handle_value;
    image = (KainSharedImageHandle*)kain_alloc_rc(sizeof(KainSharedImageHandle), KAIN_RC_TYPE_SHARED_IMAGE);
    if (!image) {
        rc_release(buffer_handle);
        return 0;
    }
    memset(image, 0, sizeof(*image));
    image->buffer = buffer_handle;
    image->width = width;
    image->height = height;
    image->channels = channels;
    image->row_stride = row_stride;
    image->layout = kain_shared_dup_cstr(layout ? layout : "HWC");
    image->pixel_format = kain_shared_dup_cstr(pixel_format ? pixel_format : "rgba8");
    image->mime_type = kain_shared_dup_cstr(mime_type ? mime_type : "image/x-kain-raster");
    image->representation = kain_shared_dup_cstr(representation ? representation : "raster");
    image->color_space = kain_shared_dup_cstr(color_space ? color_space : "srgb");
    image->alpha_mode = kain_shared_dup_cstr(alpha_mode ? alpha_mode : "opaque");
    KAIN_set_destructor(image, kain_shared_image_destructor);
    return (int64_t)(intptr_t)image;
}

int64_t kain_shared_buffer_from_bytes(
    int64_t bytes,
    char* element_type,
    int64_t shape,
    char* format,
    char* mime_type
) {
    unsigned char* byte_data = NULL;
    int64_t byte_length = 0;
    int64_t strides;
    int64_t labels;
    int64_t element_size = 1;
    int64_t handle = 0;
    const char* resolved_element_type = (element_type && element_type[0]) ? element_type : "u8";
    if (strcmp(resolved_element_type, "bool") == 0 ||
        strcmp(resolved_element_type, "u8") == 0 ||
        strcmp(resolved_element_type, "uint8") == 0 ||
        strcmp(resolved_element_type, "i8") == 0 ||
        strcmp(resolved_element_type, "int8") == 0) {
        element_size = 1;
    } else if (strcmp(resolved_element_type, "u16") == 0 ||
        strcmp(resolved_element_type, "uint16") == 0 ||
        strcmp(resolved_element_type, "i16") == 0 ||
        strcmp(resolved_element_type, "int16") == 0) {
        element_size = 2;
    } else if (strcmp(resolved_element_type, "u32") == 0 ||
        strcmp(resolved_element_type, "uint32") == 0 ||
        strcmp(resolved_element_type, "i32") == 0 ||
        strcmp(resolved_element_type, "int32") == 0 ||
        strcmp(resolved_element_type, "f32") == 0 ||
        strcmp(resolved_element_type, "float32") == 0) {
        element_size = 4;
    } else if (strcmp(resolved_element_type, "u64") == 0 ||
        strcmp(resolved_element_type, "uint64") == 0 ||
        strcmp(resolved_element_type, "i64") == 0 ||
        strcmp(resolved_element_type, "int64") == 0 ||
        strcmp(resolved_element_type, "f64") == 0 ||
        strcmp(resolved_element_type, "float64") == 0 ||
        strcmp(resolved_element_type, "double") == 0) {
        element_size = 8;
    }
    if (!kain_shared_extract_bytes(bytes, &byte_data, &byte_length)) {
        return 0;
    }
    strides = kain_shared_compact_strides(shape);
    if (!strides) {
        free(byte_data);
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared buffer shape could not produce strides",
            "kain_shared_buffer_from_bytes"
        );
        return 0;
    }
    labels = kain_shared_build_labels2("kain", "buffer");
    handle = kain_shared_buffer_create_owned(
        byte_data,
        byte_length,
        resolved_element_type,
        element_size,
        shape,
        strides,
        format,
        mime_type,
        "kain",
        NULL,
        "owned",
        labels
    );
    free(byte_data);
    rc_release((void*)(intptr_t)strides);
    rc_release((void*)(intptr_t)labels);
    return handle;
}

int64_t kain_shared_buffer_info(int64_t target) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    int64_t info;
    if (!buffer) {
        return 0;
    }
    info = json_object_new();
    json_object_set(info, "contract_version", KAIN_SHARED_JSON_INT(KAIN_SHARED_CONTRACT_VERSION));
    json_object_set(info, "element_size", KAIN_SHARED_JSON_INT(buffer->element_size));
    json_object_set(info, "shape", buffer->shape ? buffer->shape : KAIN_SHARED_JSON_NULL);
    json_object_set(info, "strides", buffer->strides ? buffer->strides : KAIN_SHARED_JSON_NULL);
    json_object_set(info, "labels", buffer->labels ? buffer->labels : KAIN_SHARED_JSON_NULL);
    json_object_set(info, "byte_length", KAIN_SHARED_JSON_INT(buffer->byte_length));
    json_object_set(
        info,
        "element_count",
        KAIN_SHARED_JSON_INT(kain_shared_buffer_element_count(buffer))
    );
    kain_shared_json_set_string_required(info, "contract", "kain.shared.buffer");
    kain_shared_json_set_string_required(info, "element_type", buffer->element_type ? buffer->element_type : "u8");
    kain_shared_json_set_string_required(
        info,
        "source_runtime",
        buffer->source_runtime ? buffer->source_runtime : "kain"
    );
    kain_shared_json_set_string_required(
        info,
        "ownership",
        buffer->ownership ? buffer->ownership : "owned"
    );
    kain_shared_json_set_string_optional(info, "format", buffer->format);
    kain_shared_json_set_string_optional(info, "mime_type", buffer->mime_type);
    kain_shared_json_set_string_optional(info, "source_backend", buffer->source_backend);
    json_object_set(
        info,
        "zero_copy",
        ((((int64_t)(buffer->storage_mode == KAIN_SHARED_STORAGE_BORROWED)) << 3) | 2LL)
    );
    return info;
}

int64_t kain_shared_buffer_byte_length(int64_t target) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    return buffer ? buffer->byte_length : 0;
}

int64_t kain_shared_buffer_element_count_value(int64_t target) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    return buffer ? kain_shared_buffer_element_count(buffer) : 0;
}

int64_t kain_shared_buffer_element_size(int64_t target) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    return buffer ? buffer->element_size : 0;
}

int64_t kain_shared_buffer_zero_copy_flag(int64_t target) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    return (buffer && buffer->storage_mode == KAIN_SHARED_STORAGE_BORROWED) ? 1 : 0;
}

int64_t kain_shared_buffer_shared_ownership(int64_t target) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    if (!buffer) {
        return 0;
    }
    if (!buffer->ownership) {
        return 0;
    }
    return strcmp(buffer->ownership, "shared") == 0 ? 1 : 0;
}

void kain_shared_buffer_release(int64_t target) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    if (!buffer) {
        return;
    }
    rc_release(buffer);
}

int64_t kain_shared_buffer_bytes(int64_t target) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    KainArray* bytes;
    int64_t index;
    if (!buffer) {
        return 0;
    }
    bytes = array_new(buffer->byte_length > 0 ? buffer->byte_length : 1);
    if (!bytes) {
        return 0;
    }
    for (index = 0; index < buffer->byte_length; ++index) {
        array_push(bytes, (int64_t)buffer->bytes[index]);
    }
    return (int64_t)(intptr_t)bytes;
}

void kain_shared_buffer_replace_bytes(int64_t target, int64_t bytes) {
    KainSharedBufferHandle* buffer = kain_shared_as_buffer_handle(target);
    unsigned char* byte_data = NULL;
    int64_t byte_length = 0;
    if (!buffer) {
        return;
    }
    if (!kain_shared_extract_bytes(bytes, &byte_data, &byte_length)) {
        return;
    }
    if (!kain_shared_validate_buffer_length(
            buffer->shape,
            buffer->element_size,
            byte_length,
            "kain_shared_buffer_replace_bytes"
        )) {
        free(byte_data);
        return;
    }
    if (buffer->storage_mode == KAIN_SHARED_STORAGE_OWNED) {
        free(buffer->bytes);
    } else {
        kain_shared_buffer_drop_zero_copy_owner(buffer);
        buffer->storage_mode = KAIN_SHARED_STORAGE_OWNED;
        kain_shared_buffer_set_ownership(buffer, "owned");
    }
    buffer->bytes = byte_data;
    buffer->byte_length = byte_length;
}

static int64_t kain_shared_image_default_shape(
    int64_t width,
    int64_t height,
    int64_t channels,
    const char* layout
) {
    KainArray* shape = NULL;
    if (!layout) {
        layout = "HWC";
    }
    if (strcmp(layout, "CHW") == 0) {
        shape = array_new(3);
        if (!shape) {
            return 0;
        }
        array_push(shape, channels);
        array_push(shape, height);
        array_push(shape, width);
        return (int64_t)(intptr_t)shape;
    }
    if (strcmp(layout, "NHWC") == 0) {
        shape = array_new(4);
        if (!shape) {
            return 0;
        }
        array_push(shape, 1);
        array_push(shape, height);
        array_push(shape, width);
        array_push(shape, channels);
        return (int64_t)(intptr_t)shape;
    }
    if (strcmp(layout, "NCHW") == 0) {
        shape = array_new(4);
        if (!shape) {
            return 0;
        }
        array_push(shape, 1);
        array_push(shape, channels);
        array_push(shape, height);
        array_push(shape, width);
        return (int64_t)(intptr_t)shape;
    }
    shape = array_new(channels > 1 ? 3 : 2);
    if (!shape) {
        return 0;
    }
    array_push(shape, height);
    array_push(shape, width);
    if (channels > 1) {
        array_push(shape, channels);
    }
    return (int64_t)(intptr_t)shape;
}

int64_t kain_shared_image_from_bytes(
    int64_t bytes,
    int64_t width,
    int64_t height,
    int64_t channels,
    char* layout,
    char* pixel_format,
    char* mime_type
) {
    unsigned char* byte_data = NULL;
    int64_t byte_length = 0;
    int64_t row_stride = 0;
    int64_t shape = 0;
    int64_t strides = 0;
    int64_t labels = 0;
    int64_t handle = 0;
    if (!kain_shared_extract_bytes(bytes, &byte_data, &byte_length)) {
        return 0;
    }
    if (!kain_shared_checked_mul_i64(width, channels > 0 ? channels : 1, &row_stride)) {
        free(byte_data);
        kain_shared_emit_error(
            KAIN_DIAG_CODE_PLATFORM_INVALID_ARGUMENT,
            "Shared image row-stride arithmetic failed",
            "kain_shared_image_from_bytes"
        );
        return 0;
    }
    shape = kain_shared_image_default_shape(width, height, channels, layout);
    if (!shape) {
        free(byte_data);
        return 0;
    }
    strides = kain_shared_compact_strides(shape);
    if (!strides) {
        free(byte_data);
        rc_release((void*)(intptr_t)shape);
        return 0;
    }
    labels = kain_shared_build_labels2("kain", "image");
    handle = kain_shared_image_create_owned(
        byte_data,
        byte_length,
        width,
        height,
        channels,
        layout ? layout : "HWC",
        pixel_format ? pixel_format : "rgba8",
        mime_type ? mime_type : "image/x-kain-raster",
        row_stride,
        "raster",
        "srgb",
        channels == 4 ? "straight" : "opaque",
        "kain",
        NULL,
        "owned",
        labels,
        shape,
        strides
    );
    free(byte_data);
    rc_release((void*)(intptr_t)shape);
    rc_release((void*)(intptr_t)strides);
    rc_release((void*)(intptr_t)labels);
    return handle;
}

int64_t kain_shared_image_info(int64_t target) {
    KainSharedImageHandle* image = kain_shared_as_image_handle(target);
    int64_t info;
    if (!image || !image->buffer) {
        return 0;
    }
    info = json_object_new();
    json_object_set(info, "contract_version", KAIN_SHARED_JSON_INT(KAIN_SHARED_CONTRACT_VERSION));
    json_object_set(info, "width", KAIN_SHARED_JSON_INT(image->width));
    json_object_set(info, "height", KAIN_SHARED_JSON_INT(image->height));
    json_object_set(info, "channels", KAIN_SHARED_JSON_INT(image->channels));
    json_object_set(info, "row_stride", KAIN_SHARED_JSON_INT(image->row_stride));
    json_object_set(info, "labels", image->buffer->labels ? image->buffer->labels : KAIN_SHARED_JSON_NULL);
    json_object_set(info, "byte_length", KAIN_SHARED_JSON_INT(image->buffer->byte_length));
    kain_shared_json_set_string_required(info, "contract", "kain.shared.image");
    kain_shared_json_set_string_required(
        info,
        "representation",
        image->representation ? image->representation : "raster"
    );
    kain_shared_json_set_string_required(info, "layout", image->layout ? image->layout : "HWC");
    kain_shared_json_set_string_required(
        info,
        "pixel_format",
        image->pixel_format ? image->pixel_format : "rgba8"
    );
    kain_shared_json_set_string_required(
        info,
        "mime_type",
        image->mime_type ? image->mime_type : "image/x-kain-raster"
    );
    kain_shared_json_set_string_required(
        info,
        "color_space",
        image->color_space ? image->color_space : "srgb"
    );
    kain_shared_json_set_string_required(
        info,
        "alpha_mode",
        image->alpha_mode ? image->alpha_mode : "opaque"
    );
    kain_shared_json_set_string_required(
        info,
        "source_runtime",
        image->buffer->source_runtime ? image->buffer->source_runtime : "kain"
    );
    kain_shared_json_set_string_required(
        info,
        "ownership",
        image->buffer->ownership ? image->buffer->ownership : "owned"
    );
    kain_shared_json_set_string_optional(info, "source_backend", image->buffer->source_backend);
    json_object_set(
        info,
        "zero_copy",
        ((((int64_t)(image->buffer->storage_mode == KAIN_SHARED_STORAGE_BORROWED)) << 3) | 2LL)
    );
    return info;
}

int64_t kain_shared_image_bytes(int64_t target) {
    KainSharedImageHandle* image = kain_shared_as_image_handle(target);
    if (!image || !image->buffer) {
        return 0;
    }
    return kain_shared_buffer_bytes((int64_t)(intptr_t)image->buffer);
}

void kain_shared_image_replace_bytes(int64_t target, int64_t bytes) {
    KainSharedImageHandle* image = kain_shared_as_image_handle(target);
    unsigned char* byte_data = NULL;
    int64_t byte_length = 0;
    if (!image || !image->buffer) {
        return;
    }
    if (!kain_shared_extract_bytes(bytes, &byte_data, &byte_length)) {
        return;
    }
    if (!kain_shared_validate_image_length(
            image->width,
            image->height,
            image->channels,
            image->row_stride,
            byte_length,
            "kain_shared_image_replace_bytes"
        )) {
        free(byte_data);
        return;
    }
    free(image->buffer->bytes);
    image->buffer->bytes = byte_data;
    image->buffer->byte_length = byte_length;
}
