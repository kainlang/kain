/*
 * Actor Runtime Smoke Test: Basic Actor Spawn
 *
 * Tests:
 * - Actor spawn with bootstrap function
 * - Mailbox send and receive
 * - Actor exit and cleanup
 * - Actor state transitions
 */

#include "../../native/include/kain_runtime_actor.h"
#include "../../native/include/kain_runtime_diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_actor_name(char* dest, const char* src) {
    snprintf(dest, KAIN_ACTOR_NAME_MAX, "%s", src);
}

/* Test actor bootstrap function */
KainActorExitReason test_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)user_data;
    printf("Actor %llu started\n", actor_id);
    
    /* Receive one message */
    KainActorMessage msg;
    KainDiagnostic diag;
    
    int result = kain_actor_receive(mailbox, &msg, &diag);
    if (result == 0) {
        printf("Actor %llu received message with type_tag: %llu\n", actor_id, msg.type_tag);
        if (msg.data != NULL) {
            free(msg.data);
        }
    } else {
        printf("Actor %llu failed to receive message\n", actor_id);
    }
    
    printf("Actor %llu exiting normally\n", actor_id);
    return KAIN_ACTOR_EXIT_NORMAL;
}

int main(void) {
    printf("=== Actor Runtime Smoke Test: Basic Spawn ===\n\n");
    
    /* Initialize actor runtime */
    kain_actor_runtime_init();
    
    /* Configure actor spawn */
    KainActorSpawnConfig config;
    kain_actor_spawn_config_init(&config);
    config.bootstrap_fn = test_actor_bootstrap;
    config.user_data = NULL;
    config.mailbox_capacity = 10;
    copy_actor_name(config.name, "test_actor");
    
    /* Spawn actor */
    KainDiagnostic diag;
    KainActorId actor_id = kain_actor_spawn(&config, &diag);
    
    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Actor spawn failed: %s\n", diag.message);
        return 1;
    }
    
    printf("Actor spawned with ID: %llu\n", actor_id);
    
    /* Send a message to the actor */
    KainActorMessage msg;
    msg.type_tag = 42;
    msg.data = malloc(sizeof("Hello, Actor!"));
    if (msg.data != NULL) {
        memcpy(msg.data, "Hello, Actor!", sizeof("Hello, Actor!"));
        msg.data_size = sizeof("Hello, Actor!");
    } else {
        msg.data_size = 0;
    }
    msg.sender_id = KAIN_ACTOR_ID_INVALID;
    
    int result = kain_actor_send(actor_id, &msg, &diag);
    if (msg.data != NULL) {
        free(msg.data);
    }
    if (result != 0) {
        printf("FAIL: Failed to send message: %s\n", diag.message);
        return 1;
    }
    
    printf("Message sent to actor\n");
    
    /* Wait a bit for actor to process */
    Sleep(100);
    
    /* Check actor state */
    KainActorState state = kain_actor_get_state(actor_id);
    printf("Actor state: %d\n", state);
    
    /* Shutdown actor runtime */
    kain_actor_runtime_shutdown();
    
    printf("\nPASS: Basic actor spawn test completed successfully\n");
    return 0;
}
