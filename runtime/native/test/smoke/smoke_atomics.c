// Smoke test: atomic operations
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include "memory.h"

int main(void) {
    uint64_t val = 0;

    // ── Store / Load ──
    __kain_atomic_store_ordered(&val, 42, 3); // 3 = memory_order_seq_cst
    int64_t loaded = __kain_atomic_load_ordered(&val, 3);
    assert(loaded == 42 && "store/load mismatch");
    printf("  store/load: OK (%lld)\n", (long long)loaded);

    // ── Add ──
    __kain_atomic_add_ordered(&val, 8, 3);
    loaded = __kain_atomic_load_ordered(&val, 3);
    assert(loaded == 50 && "add failed");
    printf("  add: OK (%lld)\n", (long long)loaded);

    // ── Sub ──
    __kain_atomic_sub_ordered(&val, 10, 3);
    loaded = __kain_atomic_load_ordered(&val, 3);
    assert(loaded == 40 && "sub failed");
    printf("  sub: OK (%lld)\n", (long long)loaded);

    // ── Exchange ──
    int64_t old = __kain_atomic_exchange_ordered(&val, 99, 3);
    assert(old == 40 && "exchange returned wrong old value");
    loaded = __kain_atomic_load_ordered(&val, 3);
    assert(loaded == 99 && "exchange didn't set new value");
    printf("  exchange: OK\n");

    // ── AND ──
    __kain_atomic_and_ordered(&val, 0x0F, 3);
    loaded = __kain_atomic_load_ordered(&val, 3);
    assert(loaded == (99 & 0x0F) && "and failed");
    printf("  and: OK (%lld)\n", (long long)loaded);

    // ── OR ──
    // val is 0x03 from AND above; 0x03 | 0xF0 = 0xF3
    __kain_atomic_or_ordered(&val, 0xF0, 3);
    loaded = __kain_atomic_load_ordered(&val, 3);
    assert(loaded == 0xF3 && "or failed");
    printf("  or: OK (%lld)\n", (long long)loaded);

    // ── Compare-exchange (match) ──
    // val is 0xF3; CAS against 0xF3 should succeed
    int matched = __kain_atomic_compare_exchange_ordered(&val, 0xF3, 0x100, 3, 3);
    assert(matched && "cas should have matched 0xF3");
    loaded = __kain_atomic_load_ordered(&val, 3);
    assert(loaded == 0x100 && "cas didn't set desired");
    printf("  cas (match): OK (%lld)\n", (long long)loaded);

    // ── Compare-exchange (no match) ──
    matched = __kain_atomic_compare_exchange_ordered(&val, 0xDEAD, 0xBEEF, 3, 3);
    assert(!matched && "cas should NOT have matched 0xDEAD");
    loaded = __kain_atomic_load_ordered(&val, 3);
    assert(loaded == 0x100 && "cas changed value on mismatch");
    printf("  cas (mismatch): OK (%lld)\n", (long long)loaded);

    // ── Fence ──
    __kain_atomic_fence(3);
    printf("  fence: OK\n");

    printf("\nsmoke_atomics: PASS\n");
    return 0;
}
