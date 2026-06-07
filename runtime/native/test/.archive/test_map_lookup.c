/*
 * Test: Native Map Lookup Tiny Dispatch
 *
 * Validates the small-map perfect-token accelerator and its generic fallback.
 */

#include "../include/base.h"
#include <stdio.h>

extern KainMap* map_new(void);
extern void map_set(KainMap* map, char* key, long long value);
extern void map_set_static(KainMap* map, char* key, long long value);
extern long long map_get(KainMap* map, char* key);

static int expect_status(const char* label, int condition) {
    if (!condition) {
        printf("FAIL: %s\n", label);
        return 0;
    }
    return 1;
}

static int map_set_literal(KainMap* map, const char* literal, long long value) {
    char* owned = string_new((char*)literal);
    if (!owned) {
        printf("FAIL: string_new(%s) returned NULL\n", literal);
        return 0;
    }
    map_set(map, owned, value);
    rc_release(owned);
    return 1;
}

static int map_key_state(KainMap* map, const char* key) {
    long long index;
    if (!map || !key) {
        return KAIN_MAP_ENTRY_EMPTY;
    }
    for (index = 0; index < map->capacity; ++index) {
        MapEntry* entry = &map->entries[index];
        if (entry->occupied && strcmp(entry->key, key) == 0) {
            return entry->occupied;
        }
    }
    return KAIN_MAP_ENTRY_EMPTY;
}

static int test_tiny_dispatch_hits_literal_domain(void) {
    static const char* keys[] = {
        "alpha", "beta", "gamma", "delta",
        "epsilon", "zeta", "eta", "theta",
        "iota", "kappa", "lambda", "mu",
        "nu", "xi", "omicron", "pi",
    };
    static const long long values[] = {
        11, 23, 37, 41,
        53, 67, 79, 83,
        97, 101, 113, 127,
        131, 149, 157, 173,
    };
    KainMap* map = map_new();
    long long total = 0;
    size_t index;
    int ok = 1;

    printf("\n=== Test 1: tiny dispatch handles literal dictionary lookups ===\n");
    if (!expect_status("map_new", map != NULL)) {
        return 0;
    }

    for (index = 0u; index < sizeof(keys) / sizeof(keys[0]); ++index) {
        map_set_static(map, (char*)keys[index], values[index]);
    }

    ok &= expect_status("tiny dispatch enabled after 16 inserts", map->tiny_ready == 1u);
    ok &= expect_status("literal entries stay borrowed-static", map_key_state(map, "alpha") == KAIN_MAP_ENTRY_STATIC_KEY);
    for (index = 0u; index < sizeof(keys) / sizeof(keys[0]); ++index) {
        long long observed = map_get(map, (char*)keys[index]);
        ok &= expect_status("literal lookup returned inserted value", observed == values[index]);
        total += observed;
    }
    ok &= expect_status("missing literal returns zero", map_get(map, "omega") == 0);

    map_set_static(map, "alpha", 211);
    ok &= expect_status("tiny dispatch survives update", map->tiny_ready == 1u);
    ok &= expect_status("updated literal remains borrowed-static", map_key_state(map, "alpha") == KAIN_MAP_ENTRY_STATIC_KEY);
    ok &= expect_status("updated literal is visible", map_get(map, "alpha") == 211);
    ok &= expect_status("literal sum matches expected baseline", total == 1442);

    rc_release(map);
    if (ok) {
        printf("PASS: tiny dispatch handles small literal-key maps\n");
    }
    return ok;
}

static int test_large_domain_falls_back_to_generic_lookup(void) {
    KainMap* map = map_new();
    int ok = 1;
    int index;

    printf("\n=== Test 2: generic lookup survives beyond tiny dispatch limit ===\n");
    if (!expect_status("map_new", map != NULL)) {
        return 0;
    }

    for (index = 0; index < 32; ++index) {
        char literal[32];
        if (snprintf(literal, sizeof(literal), "service-%02d", index) <= 0) {
            printf("FAIL: snprintf(service-%02d) failed\n", index);
            rc_release(map);
            return 0;
        }
        if (!map_set_literal(map, literal, (long long)(index * 7 + 5))) {
            rc_release(map);
            return 0;
        }
    }

    ok &= expect_status("tiny dispatch disabled past 24 entries", map->tiny_ready == 0u);
    for (index = 0; index < 32; ++index) {
        char literal[32];
        long long expected = (long long)(index * 7 + 5);
        if (snprintf(literal, sizeof(literal), "service-%02d", index) <= 0) {
            printf("FAIL: snprintf(service-%02d) failed on lookup\n", index);
            rc_release(map);
            return 0;
        }
        ok &= expect_status("generic fallback returns inserted value", map_get(map, literal) == expected);
    }
    ok &= expect_status("generic fallback miss returns zero", map_get(map, "service-99") == 0);

    rc_release(map);
    if (ok) {
        printf("PASS: generic map lookup remains correct after tiny-dispatch cutoff\n");
    }
    return ok;
}

static int test_static_update_reclaims_owned_key(void) {
    KainMap* map = map_new();
    int ok = 1;

    printf("\n=== Test 3: static literal update can replace owned key storage ===\n");
    if (!expect_status("map_new", map != NULL)) {
        return 0;
    }
    if (!map_set_literal(map, "alpha", 17)) {
        rc_release(map);
        return 0;
    }
    ok &= expect_status("owned insertion starts as owned", map_key_state(map, "alpha") == KAIN_MAP_ENTRY_OWNED_KEY);
    map_set_static(map, "alpha", 29);
    ok &= expect_status("static update promotes entry to borrowed-static", map_key_state(map, "alpha") == KAIN_MAP_ENTRY_STATIC_KEY);
    ok &= expect_status("promoted entry returns new value", map_get(map, "alpha") == 29);

    rc_release(map);
    if (ok) {
        printf("PASS: static literal updates can reclaim owned key storage\n");
    }
    return ok;
}

int main(void) {
    int passed = 0;
    int total = 0;

    total++;
    passed += test_tiny_dispatch_hits_literal_domain();
    total++;
    passed += test_large_domain_falls_back_to_generic_lookup();
    total++;
    passed += test_static_update_reclaims_owned_key();

    printf("\nNative map lookup tests: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
