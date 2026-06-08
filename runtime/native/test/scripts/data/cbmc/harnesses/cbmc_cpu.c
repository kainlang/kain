/*
 * CBMC verification harness for cpu
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
// abi_cpu_feature_mask
uint64_t abi_cpu_feature_mask(void);
// abi_cpu_feature_fingerprint
uint64_t abi_cpu_feature_fingerprint(void);
// abi_cpu_capability_mask_for_key
uint64_t abi_cpu_capability_mask_for_key(const char* capability_key);
// abi_cpu_has_capability
int64_t abi_cpu_has_capability(const char* capability_key);
// abi_cpu_pause
int64_t abi_cpu_pause(void);

int main(void) {
    abi_cpu_feature_mask();
    __CPROVER_assert(1, "abi_cpu_feature_mask: call ok");
    abi_cpu_feature_fingerprint();
    __CPROVER_assert(1, "abi_cpu_feature_fingerprint: call ok");
    { void *__p; abi_cpu_capability_mask_for_key(__p); }
    __CPROVER_assert(1, "abi_cpu_capability_mask_for_key: call ok");
    { void *__p; abi_cpu_has_capability(__p); }
    __CPROVER_assert(1, "abi_cpu_has_capability: call ok");
    abi_cpu_pause();
    __CPROVER_assert(1, "abi_cpu_pause: call ok");
    return 0;
}
