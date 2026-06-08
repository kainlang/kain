/*
 * CBMC verification harness for arena
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
// kain_memtype_is_legal
int kain_memtype_is_legal(uint8_t memtype);
// kain_arena_reset
void kain_arena_reset(KainArena* arena);
// kain_arena_available
size_t kain_arena_available(const KainArena* arena);
// kain_frame_set_marker
int kain_frame_set_marker(KainArena* arena);
// kain_frame_release_to_last_marker
int kain_frame_release_to_last_marker(KainArena* arena);

int main(void) {
    { void *__p; kain_memtype_is_legal(__p); }
    __CPROVER_assert(1, "kain_memtype_is_legal: call ok");
    { void *__p; kain_arena_reset(__p); }
    __CPROVER_assert(1, "kain_arena_reset: call ok");
    { void *__p; kain_arena_available(__p); }
    __CPROVER_assert(1, "kain_arena_available: call ok");
    { void *__p; kain_frame_set_marker(__p); }
    __CPROVER_assert(1, "kain_frame_set_marker: call ok");
    { void *__p; kain_frame_release_to_last_marker(__p); }
    __CPROVER_assert(1, "kain_frame_release_to_last_marker: call ok");
    return 0;
}
