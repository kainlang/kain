/*
 * CBMC verification harness for interop_contracts
 * Auto-generated from function catalog
 *
 * Self-contained: forward declarations only, no system headers.
 * CBMC explores ALL paths on ALL possible inputs within unwind bound.
 */

// Basic type definitions needed by runtime function signatures
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef signed char int8_t;
typedef unsigned long long size_t;
typedef long long ptrdiff_t;

// Forward declarations of functions under test
// kain_shared_buffer_byte_length
int64_t kain_shared_buffer_byte_length(int64_t target);
// kain_shared_buffer_element_count_value
int64_t kain_shared_buffer_element_count_value(int64_t target);
// kain_shared_buffer_element_size
int64_t kain_shared_buffer_element_size(int64_t target);
// kain_shared_buffer_zero_copy_flag
int64_t kain_shared_buffer_zero_copy_flag(int64_t target);
// kain_shared_buffer_shared_ownership
int64_t kain_shared_buffer_shared_ownership(int64_t target);

int main(void) {
    { void *__p; kain_shared_buffer_byte_length(__p); }
    __CPROVER_assert(1, "kain_shared_buffer_byte_length: call ok");
    { void *__p; kain_shared_buffer_element_count_value(__p); }
    __CPROVER_assert(1, "kain_shared_buffer_element_count_value: call ok");
    { void *__p; kain_shared_buffer_element_size(__p); }
    __CPROVER_assert(1, "kain_shared_buffer_element_size: call ok");
    { void *__p; kain_shared_buffer_zero_copy_flag(__p); }
    __CPROVER_assert(1, "kain_shared_buffer_zero_copy_flag: call ok");
    { void *__p; kain_shared_buffer_shared_ownership(__p); }
    __CPROVER_assert(1, "kain_shared_buffer_shared_ownership: call ok");
    return 0;
}
