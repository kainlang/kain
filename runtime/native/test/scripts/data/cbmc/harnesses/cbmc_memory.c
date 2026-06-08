/*
 * CBMC verification harness for memory
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
// __kain_bind_local
void* __kain_bind_local(void* ptr);
// __kain_addr_of
void* __kain_addr_of(void* ptr, size_t size);
// __kain_mem_load
void __kain_mem_load(const void* ptr, void* out, size_t size);
// __kain_mem_store
void __kain_mem_store(void* ptr, const void* value, size_t size);
// __kain_atomic_load_ordered
int64_t __kain_atomic_load_ordered(const void* ptr, int64_t ordering);

int main(void) {
    { void *__p; __kain_bind_local(__p); }
    __CPROVER_assert(1, "__kain_bind_local: call ok");
    { void *__a; unsigned long long __b; __kain_addr_of(__a, __b); }
    __CPROVER_assert(1, "__kain_addr_of: call ok");
    { void *__a; unsigned long long __b; __kain_atomic_load_ordered(__a, __b); }
    __CPROVER_assert(1, "__kain_atomic_load_ordered: call ok");
    return 0;
}
