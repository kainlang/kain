// Template: libFuzzer harness for Kain runtime modules.
//
// LLMs: read the header, fill in the body. Pattern is always the same.
// Build:  `make fuzz` from runtime/native/
// Run:    ./_build/test/fuzz/fuzz_<module> -max_len=4096 -runs=100000
//
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// TODO: include your module header
// #include "your_module.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    // TODO: use data to drive API calls
    // Example for a memory module:
    //   uint64_t alloc_size = *(uint64_t*)data % (1<<20);
    //   void *ptr = kain_alloc(alloc_size);
    //   if (ptr) kain_free(ptr);

    return 0;
}
