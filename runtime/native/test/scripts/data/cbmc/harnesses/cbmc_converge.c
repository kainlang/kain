/*
 * CBMC verification harness for converge
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
// abi_converge_telemetry_count
uint64_t abi_converge_telemetry_count(void);
// abi_converge_cache_probe_count
uint64_t abi_converge_cache_probe_count(void);
// abi_converge_cache_hit_count
uint64_t abi_converge_cache_hit_count(void);
// kain_converge_atomic_fetch_add_u64
static uint64_t kain_converge_atomic_fetch_add_u64(volatile uint64_t* target, uint64_t increment);
// kain_converge_mix64
static uint64_t kain_converge_mix64(uint64_t value);

int main(void) {
    abi_converge_telemetry_count();
    __CPROVER_assert(1, "abi_converge_telemetry_count: call ok");
    abi_converge_cache_probe_count();
    __CPROVER_assert(1, "abi_converge_cache_probe_count: call ok");
    abi_converge_cache_hit_count();
    __CPROVER_assert(1, "abi_converge_cache_hit_count: call ok");
    { void *__a; unsigned long long __b; kain_converge_atomic_fetch_add_u64(__a, __b); }
    __CPROVER_assert(1, "kain_converge_atomic_fetch_add_u64: call ok");
    { void *__p; kain_converge_mix64(__p); }
    __CPROVER_assert(1, "kain_converge_mix64: call ok");
    return 0;
}
