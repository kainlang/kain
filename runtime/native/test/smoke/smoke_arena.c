// Smoke test: arena memtype validation
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "arena.h"

int main(void) {
    // Test valid memtypes
    for (uint8_t mt = 0; mt < 16; mt++) {
        int legal = kain_memtype_is_legal(mt);
        printf("  memtype %u: %s\n", mt, legal ? "legal" : "illegal");
    }

    // Known-illegal values
    assert(!kain_memtype_is_legal(255) && "255 should be illegal");

    printf("\nsmoke_arena: PASS\n");
    return 0;
}
