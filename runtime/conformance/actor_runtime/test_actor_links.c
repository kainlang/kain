/*
 * Actor Runtime Conformance Test: Links
 *
 * Tests bidirectional link relationships and crash propagation.
 *
 * Requirements: 6.3
 */

#include "../../native/include/kain_runtime_actor.h"
#include "../../native/include/kain_runtime_diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

/* Actor that crashes after a delay */
static KainActorExitReason crashing_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)mailbox;
    (void)user_data;
    printf("Crashing actor %llu starting\n", actor_id);
    sleep(1);
    printf("Crashing actor %llu crashing!\n", actor_id);
    return KAIN_ACTOR_EXIT_CRASHED;
}

/* Actor that should be terminated when linked actor crashes */
static KainActorExitReason linked_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)user_data;
    printf("Linked actor %llu starting\n", actor_id);
    
    /* Wait indefinitely for messages */
    KainActorMessage msg;
    KainDiagnostic diag;
    
    while (1) {
        int result = kain_actor_receive(mailbox, &msg, &diag);
        if (result != 0) {
            /* Mailbox closed or error */
            printf("Linked actor %llu mailbox closed\n", actor_id);
            break;
        }
        if (msg.data != NULL) {
            free(msg.data);
        }
    }
    
    printf("Linked actor %llu exiting\n", actor_id);
    return KAIN_ACTOR_EXIT_NORMAL;
}

int main(void) {
    printf("=== Actor Runtime Smoke Test: Links ===\n\n");
    
    /* Initialize actor runtime */
    kain_actor_runtime_init();
    
    /* Spawn first actor */
    KainActorSpawnConfig config1;
    kain_actor_spawn_config_init(&config1);
    config1.bootstrap_fn = crashing_actor_bootstrap;
    strncpy(config1.name, "crasher", KAIN_ACTOR_NAME_MAX);
    
    KainDiagnostic diag;
    KainActorId actor1_id = kain_actor_spawn(&config1, &diag);
    
    if (actor1_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Actor 1 spawn failed: %s\n", diag.message);
        return 1;
    }
    printf("Crashing actor spawned with ID: %llu\n", actor1_id);
    
    /* Spawn second actor */
    KainActorSpawnConfig config2;
    kain_actor_spawn_config_init(&config2);
    config2.bootstrap_fn = linked_actor_bootstrap;
    strncpy(config2.name, "linked", KAIN_ACTOR_NAME_MAX);
    
    KainActorId actor2_id = kain_actor_spawn(&config2, &diag);
    
    if (actor2_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Actor 2 spawn failed: %s\n", diag.message);
        return 1;
    }
    printf("Linked actor spawned with ID: %llu\n", actor2_id);
    
    /* Create link between actors */
    int result = kain_actor_link(actor1_id, actor2_id, &diag);
    if (result != 0) {
        printf("FAIL: Link creation failed: %s\n", diag.message);
        return 1;
    }
    printf("Link established between actors\n\n");
    
    /* Wait for crash propagation */
    sleep(3);
    
    /* Check states */
    KainActorState state1 = kain_actor_get_state(actor1_id);
    KainActorState state2 = kain_actor_get_state(actor2_id);
    
    printf("\nActor 1 state: %d (expected FAILED=6)\n", state1);
    printf("Actor 2 state: %d (should be terminated due to link)\n", state2);
    
    if (state1 != KAIN_ACTOR_STATE_FAILED) {
        printf("FAIL: Actor 1 should be in FAILED state\n");
        return 1;
    }
    
    /* Test unlink functionality with new actors */
    printf("\n--- Testing unlink ---\n");
    
    KainActorSpawnConfig config3;
    kain_actor_spawn_config_init(&config3);
    config3.bootstrap_fn = linked_actor_bootstrap;
    strncpy(config3.name, "actor3", KAIN_ACTOR_NAME_MAX);
    
    KainActorId actor3_id = kain_actor_spawn(&config3, &diag);
    
    KainActorSpawnConfig config4;
    kain_actor_spawn_config_init(&config4);
    config4.bootstrap_fn = linked_actor_bootstrap;
    strncpy(config4.name, "actor4", KAIN_ACTOR_NAME_MAX);
    
    KainActorId actor4_id = kain_actor_spawn(&config4, &diag);
    
    /* Link and then unlink */
    kain_actor_link(actor3_id, actor4_id, &diag);
    printf("Actors 3 and 4 linked\n");
    
    result = kain_actor_unlink(actor3_id, actor4_id, &diag);
    if (result != 0) {
        printf("FAIL: Unlink failed\n");
        return 1;
    }
    printf("Actors 3 and 4 unlinked\n");
    
    /* Shutdown */
    kain_actor_runtime_shutdown();
    
    printf("\nPASS: Actor links test completed successfully\n");
    return 0;
}
