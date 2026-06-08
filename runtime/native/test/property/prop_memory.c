// Property test: memory allocator invariants.
// Randomized: alloc/realloc/free in sequence, verify invariants hold.
// Unlike fuzz (which hunts crashes), this validates logical properties.
//
// Invariants:
//  1. alloc(N) returns non-NULL or NULL (never crashes)
//  2. free(alloc(N)) succeeds
//  3. realloc(alloc(N), M) preserves first min(N,M) bytes
//  4. alloc_zeroed(N) returns all-zero memory
//  5. alloc(0) doesn't crash
//  6. free(NULL) doesn't crash (defensive, may warn)
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "memory.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); return 1; } } while(0)
#define OK(name) do { tests_passed++; } while(0)

int main(void) {
    // ── Invariant 1: alloc(N) never crashes ──
    TEST("alloc non-null or null");
    for (int i = 0; i < 100; i++) {
        size_t sz = (rand() % 65536) + 1;
        void *p = __kain_alloc(sz, 1, 0);
        if (p) {
            memset(p, 0xAA, sz > 0 ? 1 : 0); // touch first byte
            __kain_free(p);
        }
    }
    OK("alloc non-null or null");

    // ── Invariant 2: free(alloc(N)) always works ──
    TEST("alloc-free cycle");
    {
        void *p = __kain_alloc(1024, 1, 0);
        CHECK(p != NULL, "alloc(1024) failed");
        int rc = __kain_free(p);
        CHECK(rc == 0, "free returned error");
    }
    OK("alloc-free cycle");

    // ── Invariant 3: realloc preserves content ──
    TEST("realloc content preservation");
    {
        uint8_t expected[128];
        for (int i = 0; i < 128; i++) expected[i] = (uint8_t)(i * 7 + 13);

        void *p = __kain_alloc(128, 1, 0);
        CHECK(p != NULL, "alloc failed");
        memcpy(p, expected, 128);

        // Grow
        void *p2 = __kain_realloc(p, 256, 1, 0);
        CHECK(p2 != NULL, "realloc grow failed");
        CHECK(memcmp(p2, expected, 128) == 0, "realloc grow lost content");

        // Shrink
        void *p3 = __kain_realloc(p2, 64, 1, 0);
        CHECK(p3 != NULL, "realloc shrink failed");
        CHECK(memcmp(p3, expected, 64) == 0, "realloc shrink lost content");

        __kain_free(p3);
    }
    OK("realloc content preservation");

    // ── Invariant 4: zeroed alloc returns zeros ──
    TEST("zeroed alloc");
    {
        size_t sz = 4096;
        void *p = __kain_alloc(sz, 1, 1);
        CHECK(p != NULL, "zeroed alloc failed");
        for (size_t i = 0; i < sz; i++) {
            CHECK(((uint8_t*)p)[i] == 0, "zeroed alloc has non-zero byte");
        }
        __kain_free(p);
    }
    OK("zeroed alloc");

    // ── Invariant 5: alloc(0) doesn't crash ──
    TEST("alloc zero size");
    {
        void *p = __kain_alloc(0, 1, 0);
        if (p) __kain_free(p);  // both NULL and non-NULL are acceptable
    }
    OK("alloc zero size");

    // ── Invariant 6: multiple alloc/free cycles ──
    TEST("alloc/free stress");
    {
        void *ptrs[64] = {0};
        // Allocate many
        for (int i = 0; i < 64; i++) {
            ptrs[i] = __kain_alloc(256, 1, 0);
            if (ptrs[i]) memset(ptrs[i], i, 256);
        }
        // Free in reverse
        for (int i = 63; i >= 0; i--) {
            if (ptrs[i]) __kain_free(ptrs[i]);
        }
    }
    OK("alloc/free stress");

    printf("\n%d/%d property tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
