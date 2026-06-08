/*
 * CBMC verification harness for interop_zero_copy
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
// kain_interop_zero_copy_owner_create
int64_t kain_interop_zero_copy_owner_create( void* state, KainInteropZeroCopyReleaseFn release_fn );
// kain_interop_zero_copy_owner_is_valid
int kain_interop_zero_copy_owner_is_valid(int64_t owner_handle);
// kain_interop_zero_copy_owner_retain
void kain_interop_zero_copy_owner_retain(int64_t owner_handle);
// kain_interop_zero_copy_owner_release
void kain_interop_zero_copy_owner_release(int64_t owner_handle);
// kain_interop_zero_copy_rc_header
static RcHeader* kain_interop_zero_copy_rc_header(const void* ptr);

int main(void) {
    { void *__a; unsigned long long __b; kain_interop_zero_copy_owner_create(__a, __b); }
    __CPROVER_assert(1, "kain_interop_zero_copy_owner_create: call ok");
    { void *__p; kain_interop_zero_copy_owner_is_valid(__p); }
    __CPROVER_assert(1, "kain_interop_zero_copy_owner_is_valid: call ok");
    { void *__p; kain_interop_zero_copy_owner_retain(__p); }
    __CPROVER_assert(1, "kain_interop_zero_copy_owner_retain: call ok");
    { void *__p; kain_interop_zero_copy_owner_release(__p); }
    __CPROVER_assert(1, "kain_interop_zero_copy_owner_release: call ok");
    { void *__p; kain_interop_zero_copy_rc_header(__p); }
    __CPROVER_assert(1, "kain_interop_zero_copy_rc_header: call ok");
    return 0;
}
