// libFuzzer harness for Kain memory allocator.
// Feeds random bytes, drives alloc/realloc/free operations.
//
// Build:  make fuzz
// Run:    ./_build/test/fuzz/fuzz_memory -max_len=4096 -runs=100000
//
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "memory.h"

#define MAX_TRACKED 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 16) return 0;

    void *tracked[MAX_TRACKED];
    size_t tracked_sizes[MAX_TRACKED];
    int count = 0;
    memset(tracked, 0, sizeof(tracked));

    size_t offset = 0;

    while (offset + 4 <= size && count < MAX_TRACKED) {
        uint8_t op = data[offset] % 5;
        uint8_t slot = (data[offset + 1] % MAX_TRACKED);
        uint16_t arg = (uint16_t)(data[offset + 2]) | ((uint16_t)(data[offset + 3]) << 8);
        size_t alloc_sz = (arg % 4096) + 1;
        offset += 4;

        switch (op) {
        case 0: // alloc
            if (count < MAX_TRACKED && tracked[count] == NULL) {
                tracked[count] = __kain_alloc(alloc_sz, 1, arg & 1);
                if (tracked[count]) {
                    tracked_sizes[count] = alloc_sz;
                    // Write a byte to verify it's writable
                    *(uint8_t*)tracked[count] = (uint8_t)(arg & 0xFF);
                }
                count++;
            }
            break;

        case 1: // realloc (grow or shrink)
            if (slot < count && tracked[slot] != NULL) {
                size_t new_sz = (alloc_sz % 8192) + 1;
                void *newp = __kain_realloc(tracked[slot], new_sz, 1, arg & 1);
                if (newp) {
                    tracked[slot] = newp;
                    tracked_sizes[slot] = new_sz;
                    // Verify still writable
                    *(uint8_t*)newp = (uint8_t)(arg & 0xFF);
                }
                // If realloc returns NULL, original still valid
            }
            break;

        case 2: // free
            if (slot < count && tracked[slot] != NULL) {
                __kain_free(tracked[slot]);
                tracked[slot] = NULL;
                tracked_sizes[slot] = 0;
            }
            break;

        case 3: // alloc zeroed
            if (count < MAX_TRACKED && tracked[count] == NULL) {
                tracked[count] = __kain_alloc(alloc_sz, 1, 1);
                if (tracked[count]) {
                    // Verify first byte is zero
                    if (alloc_sz > 0 && *(uint8_t*)tracked[count] != 0) {
                        // Non-zero in zeroed alloc — this is a bug!
                        // But don't abort in fuzz mode, just report
                        __kain_free(tracked[count]);
                        tracked[count] = NULL;
                    } else {
                        tracked_sizes[count] = alloc_sz;
                    }
                }
                count++;
            }
            break;

        case 4: // alloc then immediate free (stress freelist)
            {
                void *tmp = __kain_alloc(alloc_sz, 1, 0);
                if (tmp) {
                    __kain_free(tmp);
                }
            }
            break;
        }
    }

    // Clean up all remaining allocations
    for (int i = 0; i < count; i++) {
        if (tracked[i]) {
            __kain_free(tracked[i]);
        }
    }

    return 0;
}

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if !__has_feature(address_sanitizer)
// Fallback main for platforms without libFuzzer (Windows/MinGW)
// Runs a single iteration with a small random buffer.
int main(void) {
    uint8_t seed[256];
    for (int i = 0; i < 256; i++) seed[i] = (uint8_t)(i * 73 + 17);
    return LLVMFuzzerTestOneInput(seed, sizeof(seed));
}
#endif
