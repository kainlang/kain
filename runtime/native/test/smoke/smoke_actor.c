// Smoke test: actor runtime init/shutdown
// Verifies the actor runtime lifecycle is linkable and basic.
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "actor.h"

int main(void) {
    // Basic init/shutdown — no crash, no leak, no hanging
    kain_actor_runtime_init();
    printf("  actor_runtime_init: OK\n");

    kain_actor_runtime_shutdown();
    printf("  actor_runtime_shutdown: OK\n");

    // Double shutdown — should be idempotent or defensive
    kain_actor_runtime_shutdown();
    printf("  double shutdown: OK\n");

    // Re-init after shutdown
    kain_actor_runtime_init();
    printf("  re-init: OK\n");
    kain_actor_runtime_shutdown();

    printf("\nsmoke_actor: PASS\n");
    return 0;
}
