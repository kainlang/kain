/*
 * Actor Runtime Smoke Test: Actor Registry
 *
 * Tests:
 * - Register named actor
 * - Lookup registered actor
 * - Unregister actor
 * - Duplicate name handling
 */

#include "../../native/include/kain_runtime_actor.h"
#include "../../native/include/kain_runtime_diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simple actor that just exits */
KainActorExitReason simple_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    return KAIN_ACTOR_EXIT_NORMAL;
}

int main(void) {
    printf("=== Actor Runtime Smoke Test: Registry ===\n\n");
    
    /* Initialize actor runtime */
    kain_actor_runtime_init();
    
    /* Spawn an actor */
    KainActorSpawnConfig config;
    kain_actor_spawn_config_init(&config);
    config.bootstrap_fn = simple_actor_bootstrap;
    strncpy(config.name, "test_actor", KAIN_ACTOR_NAME_MAX);
    
    KainDiagnostic diag;
    KainActorId actor_id = kain_actor_spawn(&config, &diag);
    
    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Actor spawn failed: %s\n", diag.message);
        return 1;
    }
    
    printf("Actor spawned with ID: %llu\n", actor_id);
    
    /* Register actor with a name */
    int result = kain_actor_registry_register("my_service", actor_id, &diag);
    if (result != 0) {
        printf("FAIL: Registry register failed: %s\n", diag.message);
        return 1;
    }
    
    printf("Actor registered as 'my_service'\n");
    
    /* Lookup the actor */
    KainActorId looked_up_id = kain_actor_registry_lookup("my_service");
    if (looked_up_id != actor_id) {
        printf("FAIL: Registry lookup returned wrong ID: %llu (expected %llu)\n", 
               looked_up_id, actor_id);
        return 1;
    }
    
    printf("Actor lookup successful: %llu\n", looked_up_id);
    
    /* Try to register duplicate name (should fail) */
    result = kain_actor_registry_register("my_service", actor_id, &diag);
    if (result == 0) {
        printf("FAIL: Duplicate registration should have failed\n");
        return 1;
    }
    
    printf("Duplicate registration correctly rejected\n");
    
    /* Unregister the actor */
    result = kain_actor_registry_unregister("my_service", &diag);
    if (result != 0) {
        printf("FAIL: Registry unregister failed\n");
        return 1;
    }
    
    printf("Actor unregistered\n");
    
    /* Lookup should now fail */
    looked_up_id = kain_actor_registry_lookup("my_service");
    if (looked_up_id != KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Lookup after unregister should return INVALID\n");
        return 1;
    }
    
    printf("Lookup after unregister correctly returns INVALID\n");
    
    /* Shutdown */
    kain_actor_runtime_shutdown();
    
    printf("\nPASS: Actor registry test completed successfully\n");
    return 0;
}
