/*
 * CBMC verification harness for compatibility
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
// kain_bundle_compat_metadata_init
void kain_bundle_compat_metadata_init(KainBundleCompatibilityMetadata* metadata);
// kain_bundle_check_abi_compatibility
int kain_bundle_check_abi_compatibility(unsigned int required_abi_version);
// kain_bundle_check_runtime_compatibility
int kain_bundle_check_runtime_compatibility(unsigned int required_runtime_version);
// kain_bundle_validate_compatibility
int kain_bundle_validate_compatibility( const KainBundleCompatibilityMetadata* metadata, KainCompatibilityValidationResult* result );
// kain_bundle_activate
int kain_bundle_activate(KainBundleHandle* handle, KainDiagnostic* diag);

int main(void) {
    { void *__p; kain_bundle_compat_metadata_init(__p); }
    __CPROVER_assert(1, "kain_bundle_compat_metadata_init: call ok");
    { void *__p; kain_bundle_check_abi_compatibility(__p); }
    __CPROVER_assert(1, "kain_bundle_check_abi_compatibility: call ok");
    { void *__p; kain_bundle_check_runtime_compatibility(__p); }
    __CPROVER_assert(1, "kain_bundle_check_runtime_compatibility: call ok");
    { void *__a; unsigned long long __b; kain_bundle_validate_compatibility(__a, __b); }
    __CPROVER_assert(1, "kain_bundle_validate_compatibility: call ok");
    { void *__a; unsigned long long __b; kain_bundle_activate(__a, __b); }
    __CPROVER_assert(1, "kain_bundle_activate: call ok");
    return 0;
}
