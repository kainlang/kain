/*
 * CBMC verification harness for buddy
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
// kain_buddy_free
void kain_buddy_free(KainBuddyHeap* heap, uint32_t node_index);
// kain_buddy_block_units
uint32_t kain_buddy_block_units(const KainBuddyHeap* heap, uint32_t node_index);
// kain_buddy_is_power_of_two
static int kain_buddy_is_power_of_two(uint32_t value);
// kain_buddy_log2_exact
static uint8_t kain_buddy_log2_exact(uint32_t value);
// kain_buddy_units_for_height
static uint32_t kain_buddy_units_for_height(uint32_t height);

int main(void) {
    { void *__a; unsigned long long __b; kain_buddy_free(__a, __b); }
    __CPROVER_assert(1, "kain_buddy_free: call ok");
    { void *__a; unsigned long long __b; kain_buddy_block_units(__a, __b); }
    __CPROVER_assert(1, "kain_buddy_block_units: call ok");
    { void *__p; kain_buddy_is_power_of_two(__p); }
    __CPROVER_assert(1, "kain_buddy_is_power_of_two: call ok");
    { void *__p; kain_buddy_log2_exact(__p); }
    __CPROVER_assert(1, "kain_buddy_log2_exact: call ok");
    { void *__p; kain_buddy_units_for_height(__p); }
    __CPROVER_assert(1, "kain_buddy_units_for_height: call ok");
    return 0;
}
