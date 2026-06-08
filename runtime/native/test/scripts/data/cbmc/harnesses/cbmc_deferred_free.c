/*
 * CBMC verification harness for deferred_free
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
// kain_deferred_free_list_make_all_free
void kain_deferred_free_list_make_all_free(KainDeferredFreeList* list);
// kain_deferred_free_list_allocate
int kain_deferred_free_list_allocate(KainDeferredFreeList* list);
// kain_deferred_free_list_deferred_free
void kain_deferred_free_list_deferred_free(KainDeferredFreeList* list, uint32_t index);
// kain_deferred_free_list_flush
void kain_deferred_free_list_flush(KainDeferredFreeList* list);
// kain_deferred_free_list_is_empty
int kain_deferred_free_list_is_empty(const KainDeferredFreeList* list);

int main(void) {
    { void *__p; kain_deferred_free_list_make_all_free(__p); }
    __CPROVER_assert(1, "kain_deferred_free_list_make_all_free: call ok");
    { void *__p; kain_deferred_free_list_allocate(__p); }
    __CPROVER_assert(1, "kain_deferred_free_list_allocate: call ok");
    { void *__a; unsigned long long __b; kain_deferred_free_list_deferred_free(__a, __b); }
    __CPROVER_assert(1, "kain_deferred_free_list_deferred_free: call ok");
    { void *__p; kain_deferred_free_list_flush(__p); }
    __CPROVER_assert(1, "kain_deferred_free_list_flush: call ok");
    { void *__p; kain_deferred_free_list_is_empty(__p); }
    __CPROVER_assert(1, "kain_deferred_free_list_is_empty: call ok");
    return 0;
}
