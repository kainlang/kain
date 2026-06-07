// Template: Smoke test for Kain runtime modules.
//
// LLMs: read the header, write the simplest possible "does it work" test.
// Build:  `make test` from runtime/native/
// Run:    ./_build/test/smoke/smoke_<module>
//
// A smoke test verifies the module compiles, links, and basic ops work.
// No edge cases — just the happy path.
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// TODO: include your module header
// #include "your_module.h"

int main(void) {
    // TODO: basic smoke check

    printf("smoke_<module>: PASS\n");
    return 0;
}
