// Smoke test: core memory allocation primitives
// Verifies kain_alloc, kain_realloc, kain_free link and work.
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "memory.h"

int main(void) {
    // ── Basic alloc / free ──
    void *p = kain_alloc(64);
    assert(p != NULL && "alloc(64) returned NULL");
    memset(p, 0xAB, 64);
    kain_free(p);
    printf("  alloc(64)/free: OK\n");

    // ── Realloc grow ──
    void *p2 = kain_alloc(32);
    assert(p2 != NULL);
    memset(p2, 0xCD, 32);
    void *p3 = kain_realloc(p2, 128);
    assert(p3 != NULL && "realloc grow returned NULL");
    // Content should be preserved (first 32 bytes)
    assert(memcmp(p3, p2, 32) == 0 && "realloc did not preserve content");
    kain_free(p3);
    printf("  realloc(grow): OK\n");

    // ── Zero-size alloc ──
    void *p4 = kain_alloc(0);
    // Implementation may return NULL or a valid pointer; both are OK
    if (p4) kain_free(p4);
    printf("  alloc(0): OK\n");

    // ── Large alloc ──
    void *p5 = kain_alloc(1 << 20);  // 1MB
    if (p5) {
        memset(p5, 0, 1 << 20);
        kain_free(p5);
        printf("  alloc(1MB)/free: OK\n");
    } else {
        printf("  alloc(1MB): SKIP (OOM)\n");
    }

    printf("\nsmoke_memory: PASS\n");
    return 0;
}
