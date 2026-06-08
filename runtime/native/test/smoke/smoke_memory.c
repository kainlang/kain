// Smoke test: core memory allocation primitives
// Verifies __kain_alloc, __kain_realloc, __kain_free (public API).
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "memory.h"

int main(void) {
    // ── Basic alloc / free ──
    void *p = __kain_alloc(64, 1, 0);
    assert(p != NULL && "alloc(64) returned NULL");
    memset(p, 0xAB, 64);
    assert(__kain_free(p) == 0 && "free failed");
    printf("  alloc(64)/free: OK\n");

    // ── Realloc grow ──
    uint8_t expected[32];
    memset(expected, 0xCD, 32);
    void *p2 = __kain_alloc(32, 1, 0);
    assert(p2 != NULL);
    memcpy(p2, expected, 32);
    void *p3 = __kain_realloc(p2, 128, 1, 1);
    assert(p3 != NULL && "realloc grow returned NULL");
    // Content should be preserved (first 32 bytes)
    assert(memcmp(p3, expected, 32) == 0 && "realloc did not preserve content");
    __kain_free(p3);
    printf("  realloc(grow): OK\n");


    // ── Zero-size alloc ──
    void *p4 = __kain_alloc(0, 1, 0);
    if (p4) __kain_free(p4);
    printf("  alloc(0): OK\n");

    // ── Large alloc ──
    void *p5 = __kain_alloc(1 << 20, 1, 0);  // 1MB
    if (p5) {
        memset(p5, 0, 1 << 20);
        __kain_free(p5);
        printf("  alloc(1MB)/free: OK\n");
    } else {
        printf("  alloc(1MB): SKIP (OOM)\n");
    }

    printf("\nsmoke_memory: PASS\n");
    return 0;
}
