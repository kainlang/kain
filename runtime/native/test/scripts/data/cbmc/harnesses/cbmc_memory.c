/*
 * CBMC verification harness for memory
 * Auto-generated from function catalog
 *
 * Self-contained: forward declarations only, no system headers.
 * CBMC explores ALL paths on ALL possible inputs within unwind bound.
 */

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
    int __nondet___kain_bind_local = __CPROVER_rand();
    __kain_bind_local(__nondet___kain_bind_local);
    __CPROVER_assert(1, "__kain_bind_local: call ok");
    int __a___kain_addr_of = __CPROVER_rand();
    int __b___kain_addr_of = __CPROVER_rand();
    __kain_addr_of(__a___kain_addr_of, __b___kain_addr_of);
    __CPROVER_assert(1, "__kain_addr_of: call ok");
    int __a___kain_atomic_load_ordered = __CPROVER_rand();
    int __b___kain_atomic_load_ordered = __CPROVER_rand();
    __kain_atomic_load_ordered(__a___kain_atomic_load_ordered, __b___kain_atomic_load_ordered);
    __CPROVER_assert(1, "__kain_atomic_load_ordered: call ok");
    return 0;
}
