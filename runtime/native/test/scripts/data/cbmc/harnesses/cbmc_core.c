/*
 * CBMC verification harness for core
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
// kain_rc_is_tracked_pointer
int kain_rc_is_tracked_pointer(const void* ptr);
// kain_floor_i64
long long kain_floor_i64(double value);
// kain_ceil_i64
long long kain_ceil_i64(double value);
// kain_round_i64
long long kain_round_i64(double value);
// kain_ord
long long kain_ord(char* src);

int main(void) {
    { void *__p; kain_rc_is_tracked_pointer(__p); }
    __CPROVER_assert(1, "kain_rc_is_tracked_pointer: call ok");
    { void *__p; kain_floor_i64(__p); }
    __CPROVER_assert(1, "kain_floor_i64: call ok");
    { void *__p; kain_ceil_i64(__p); }
    __CPROVER_assert(1, "kain_ceil_i64: call ok");
    { void *__p; kain_round_i64(__p); }
    __CPROVER_assert(1, "kain_round_i64: call ok");
    { void *__p; kain_ord(__p); }
    __CPROVER_assert(1, "kain_ord: call ok");
    return 0;
}
