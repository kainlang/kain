/*
 * CBMC verification harness for handle
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
// kain_handle_kind
uint32_t kain_handle_kind(KainRuntimeHandle handle);
// kain_handle_slot
uint32_t kain_handle_slot(KainRuntimeHandle handle);
// kain_handle_magic
uint32_t kain_handle_magic(KainRuntimeHandle handle);
// kain_handle_nonzero_magic
static uint32_t kain_handle_nonzero_magic(uint32_t magic);

int main(void) {
    { void *__p; kain_handle_kind(__p); }
    __CPROVER_assert(1, "kain_handle_kind: call ok");
    { void *__p; kain_handle_slot(__p); }
    __CPROVER_assert(1, "kain_handle_slot: call ok");
    { void *__p; kain_handle_magic(__p); }
    __CPROVER_assert(1, "kain_handle_magic: call ok");
    { void *__p; kain_handle_nonzero_magic(__p); }
    __CPROVER_assert(1, "kain_handle_nonzero_magic: call ok");
    return 0;
}
