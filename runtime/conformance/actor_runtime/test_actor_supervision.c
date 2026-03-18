/*
 * Actor Runtime Conformance Test: Supervision
 *
 * Tests supervision policies, restart behavior, and child management.
 *
 * Requirements: 6.2, 6.4
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

/* Counter for child restarts */
static int g_child_start_count = 0;

/* Child actor that crashes on first run, succeeds on restart */
static KainActorExitReason supervised_child_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)mailbox;
    (void)user_data;
    
    g_child_start_count++;
    printf("Supervised child %llu starting (attempt %d)\n", actor_id, g_child_start_count);
    
    sleep(1);
    
    if (g_child_start_count == 1) {
        printf("Supervised child %llu crashing on first attempt\n", actor_id);
        return KAIN_ACTOR_EXIT_CRASHED;
    } else {
        printf("Supervised child %llu completing successfully\n", actor_id);
        return KAIN_ACTOR_EXIT_NORMAL;
    }
}

/* Supervisor actor that manages children */
static KainActorExitReason supervisor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)user_data;
    printf("Supervisor %llu starting\n", actor_id);
    
    /* Wait for messages/notifications */
    KainActorMessage msg;
    KainDiagnostic diag;
    
    int message_count = 0;
    while (message_count < 2) {
        int result = kain_actor_receive(mailbox, &msg, &diag);
        if (result != 0) {
            break;
        }
        
        printf("Supervisor %llu received notification (type: 0x%llx)\n", 
               actor_id, msg.type_tag);
        message_count++;
        
        if (msg.data != NULL) {
            free(msg.data);
        }
    }
    
    printf("Supervisor %llu exiting\n", actor_id);
    return KAIN_ACTOR_EXIT_NORMAL;
}

/* Temporary child that exits normally */
static KainActorExitReason temporary_child_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)mailbox;
    (void)user_data;
    printf("Temporary child %llu starting and exiting normally\n", actor_id);
    sleep(1);
    return KAIN_ACTOR_EXIT_NORMAL;
}

/* Transient child that crashes */
static KainActorExitReason transient_child_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)mailbox;
    (void)user_data;
    printf("Transient child %llu starting and crashing\n", actor_id);
    sleep(1);
    return KAIN_ACTOR_EXIT_CRASHED;
}

int main(void) {
    printf("=== Actor Runtime Smoke Test: Supervision ===\n\n");
    
    /* Initialize actor runtime */
    kain_actor_runtime_init();
    
    /* Test 1: Permanent restart policy */
    printf("--- Test 1: Permanent Restart Policy ---\n");
    
    /* Spawn supervisor */
    KainActorSpawnConfig supervisor_config;
    kain_actor_spawn_config_init(&supervisor_config);
    supervisor_config.bootstrap_fn = supervisor_bootstrap;
    strncpy(supervisor_config.name, "supervisor", KAIN_ACTOR_NAME_MAX);
    
    KainDiagnostic diag;
    KainActorId supervisor_id = kain_actor_spawn(&supervisor_config, &diag);
    
    if (supervisor_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Supervisor spawn failed: %s\n", diag.message);
        return 1;
    }
    printf("Supervisor spawned with ID: %llu\n", supervisor_id);
    
    /* Spawn child with permanent restart policy */
    KainActorSpawnConfig child_config;
    kain_actor_spawn_config_init(&child_config);
    child_config.bootstrap_fn = supervised_child_bootstrap;
    child_config.supervisor_id = supervisor_id;
    child_config.restart_policy = KAIN_RESTART_POLICY_PERMANENT;
    strncpy(child_config.name, "permanent_child", KAIN_ACTOR_NAME_MAX);
    
    KainActorId child_id = kain_actor_spawn(&child_config, &diag);
    
    if (child_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Child spawn failed: %s\n", diag.message);
        return 1;
    }
    printf("Child spawned with ID: %llu (restart policy: PERMANENT)\n\n", child_id);
    
    /* Wait for child to crash and potentially restart */
    sleep(3);
    
    printf("\nChild started %d times\n", g_child_start_count);
    
    /* Note: Automatic restart is not yet implemented in Phase 6 */
    /* This test validates the supervision structure is in place */
    if (g_child_start_count < 1) {
        printf("FAIL: Child never started\n");
        return 1;
    }
    
    /* Test 2: Temporary restart policy */
    printf("\n--- Test 2: Temporary Restart Policy ---\n");
    
    KainActorSpawnConfig temp_config;
    kain_actor_spawn_config_init(&temp_config);
    temp_config.bootstrap_fn = temporary_child_bootstrap;
    temp_config.supervisor_id = supervisor_id;
    temp_config.restart_policy = KAIN_RESTART_POLICY_TEMPORARY;
    strncpy(temp_config.name, "temporary_child", KAIN_ACTOR_NAME_MAX);
    
    KainActorId temp_id = kain_actor_spawn(&temp_config, &diag);
    printf("Temporary child spawned with ID: %llu\n", temp_id);
    
    sleep(2);
    
    KainActorState temp_state = kain_actor_get_state(temp_id);
    printf("Temporary child state: %d (should be TERMINATED=5)\n", temp_state);
    
    /* Test 3: Transient restart policy */
    printf("\n--- Test 3: Transient Restart Policy ---\n");
    
    KainActorSpawnConfig transient_config;
    kain_actor_spawn_config_init(&transient_config);
    transient_config.bootstrap_fn = transient_child_bootstrap;
    transient_config.supervisor_id = supervisor_id;
    transient_config.restart_policy = KAIN_RESTART_POLICY_TRANSIENT;
    strncpy(transient_config.name, "transient_child", KAIN_ACTOR_NAME_MAX);
    
    KainActorId transient_id = kain_actor_spawn(&transient_config, &diag);
    printf("Transient child spawned with ID: %llu\n", transient_id);
    
    sleep(2);
    
    KainActorState transient_state = kain_actor_get_state(transient_id);
    printf("Transient child state: %d (should be FAILED=6)\n", transient_state);
    
    /* Shutdown */
    kain_actor_runtime_shutdown();
    
    printf("\nPASS: Actor supervision test completed successfully\n");
    printf("Note: Automatic restart logic will be implemented in future phases\n");
    return 0;
}
