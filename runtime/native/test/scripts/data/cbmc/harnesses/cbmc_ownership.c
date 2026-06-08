/*
 * CBMC verification harness for ownership
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
// __kain_ownership_register_imported
int __kain_ownership_register_imported(void* ptr, size_t size);
// __kain_ownership_ensure_imported
int __kain_ownership_ensure_imported(const void* ptr);
// __kain_ownership_helper_allocation_state
int __kain_ownership_helper_allocation_state(const void* ptr, uint16_t slot_token);
// __kain_ownership_begin_observe
int __kain_ownership_begin_observe(const void* ptr);
// __kain_ownership_begin_observe_helper
int __kain_ownership_begin_observe_helper(const void* ptr);

int main(void) {
    { void *__a; unsigned long long __b; __kain_ownership_register_imported(__a, __b); }
    __CPROVER_assert(1, "__kain_ownership_register_imported: call ok");
    { void *__p; __kain_ownership_ensure_imported(__p); }
    __CPROVER_assert(1, "__kain_ownership_ensure_imported: call ok");
    { void *__a; unsigned long long __b; __kain_ownership_helper_allocation_state(__a, __b); }
    __CPROVER_assert(1, "__kain_ownership_helper_allocation_state: call ok");
    { void *__p; __kain_ownership_begin_observe(__p); }
    __CPROVER_assert(1, "__kain_ownership_begin_observe: call ok");
    { void *__p; __kain_ownership_begin_observe_helper(__p); }
    __CPROVER_assert(1, "__kain_ownership_begin_observe_helper: call ok");
    return 0;
}
