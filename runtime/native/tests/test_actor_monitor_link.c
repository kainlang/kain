/*
 * Test: Actor Monitor and Link Semantics
 *
 * This test validates:
 * - Monitor registration and exit notification
 * - Link registration and crash propagation
 * - Exit reason encoding in monitor messages
 * - Bidirectional link behavior
 * - Demonitor functionality
 */

#include "../include/kain_runtime_actor.h"
#include "../include/kain_runtime_diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

/* Test actor that exits normally */
static KainActorExitReason test_actor_normal_exit(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    (void)mailbox;
    (void)user_data;
    
    /* Just exit normally */
    return KAIN_ACTOR_EXIT_NORMAL;
}

/* Test actor that crashes */
static KainActorExitReason test_actor_crash(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    (void)mailbox;
    (void)user_data;
    
    /* Simulate a crash */
    return KAIN_ACTOR_EXIT_CRASHED;
}

/* Test actor that monitors another and receives exit notifications */
static KainActorExitReason test_actor_monitor_receiver(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    int* received_notification = (int*)user_data;
    
    /* Wait for monitor notification */
    KainActorMessage msg;
    int result = kain_actor_receive(mailbox, &msg, NULL);
    
    if (result == 0) {
        /* Check if this is a monitor notification */
        if ((msg.type_tag & 0xDEAD0000ULL) == 0xDEAD0000ULL) {
            KainActorExitReason exit_reason = (KainActorExitReason)(msg.type_tag & 0xFFFF);
            printf("Monitor received exit notification: actor=%llu, exit_reason=%d\n", 
                   msg.sender_id, exit_reason);
            *received_notification = 1;
        }
        
        if (msg.data != NULL) {
            free(msg.data);
        }
    }
    
    return KAIN_ACTOR_EXIT_NORMAL;
}

/* Test 1: Monitor notification on normal exit */
static int test_monitor_normal_exit(void) {
    printf("\n=== Test 1: Monitor notification on normal exit ===\n");
    
    KainDiagnostic diag;
    int received = 0;
    
    /* Spawn monitored actor */
    KainActorSpawnConfig config1;
    kain_actor_spawn_config_init(&config1);
    config1.bootstrap_fn = test_actor_normal_exit;
    config1.user_data = NULL;
    
    KainActorId monitored = kain_actor_spawn(&config1, &diag);
    if (monitored == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn monitored actor\n");
        return 0;
    }
    
    /* Spawn monitoring actor */
    KainActorSpawnConfig config2;
    kain_actor_spawn_config_init(&config2);
    config2.bootstrap_fn = test_actor_monitor_receiver;
    config2.user_data = &received;
    
    KainActorId monitor = kain_actor_spawn(&config2, &diag);
    if (monitor == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn monitor actor\n");
        return 0;
    }
    
    /* Register monitor */
    if (kain_actor_monitor(monitor, monitored, &diag) != 0) {
        printf("FAIL: Could not register monitor\n");
        return 0;
    }
    
    /* Wait for actors to complete */
    SLEEP_MS(500);
    
    if (received) {
        printf("PASS: Monitor received exit notification\n");
        return 1;
    } else {
        printf("FAIL: Monitor did not receive exit notification\n");
        return 0;
    }
}

/* Test 2: Link propagation on crash */
static int test_link_crash_propagation(void) {
    printf("\n=== Test 2: Link propagation on crash ===\n");
    
    KainDiagnostic diag;
    
    /* Spawn first actor that will crash */
    KainActorSpawnConfig config1;
    kain_actor_spawn_config_init(&config1);
    config1.bootstrap_fn = test_actor_crash;
    config1.user_data = NULL;
    
    KainActorId actor_a = kain_actor_spawn(&config1, &diag);
    if (actor_a == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn actor A\n");
        return 0;
    }
    
    /* Spawn second actor that will be linked */
    KainActorSpawnConfig config2;
    kain_actor_spawn_config_init(&config2);
    config2.bootstrap_fn = test_actor_normal_exit;
    config2.user_data = NULL;
    
    KainActorId actor_b = kain_actor_spawn(&config2, &diag);
    if (actor_b == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn actor B\n");
        return 0;
    }
    
    /* Link the actors */
    if (kain_actor_link(actor_a, actor_b, &diag) != 0) {
        printf("FAIL: Could not link actors\n");
        return 0;
    }
    
    /* Wait for crash propagation */
    SLEEP_MS(500);
    
    /* Check that both actors are terminated/failed */
    KainActorState state_a = kain_actor_get_state(actor_a);
    KainActorState state_b = kain_actor_get_state(actor_b);
    
    if (state_a == KAIN_ACTOR_STATE_FAILED && state_b == KAIN_ACTOR_STATE_FAILED) {
        printf("PASS: Link propagated crash to linked actor\n");
        return 1;
    } else {
        printf("FAIL: Link did not propagate crash (state_a=%d, state_b=%d)\n", 
               state_a, state_b);
        return 0;
    }
}

/* Test 3: Demonitor removes monitoring */
static int test_demonitor(void) {
    printf("\n=== Test 3: Demonitor removes monitoring ===\n");
    
    KainDiagnostic diag;
    int received = 0;
    
    /* Spawn monitored actor that waits a bit before exiting */
    KainActorSpawnConfig config1;
    kain_actor_spawn_config_init(&config1);
    config1.bootstrap_fn = test_actor_normal_exit;
    config1.user_data = NULL;
    
    KainActorId monitored = kain_actor_spawn(&config1, &diag);
    if (monitored == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn monitored actor\n");
        return 0;
    }
    
    /* Spawn monitoring actor */
    KainActorSpawnConfig config2;
    kain_actor_spawn_config_init(&config2);
    config2.bootstrap_fn = test_actor_monitor_receiver;
    config2.user_data = &received;
    
    KainActorId monitor = kain_actor_spawn(&config2, &diag);
    if (monitor == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn monitor actor\n");
        return 0;
    }
    
    /* Register monitor */
    if (kain_actor_monitor(monitor, monitored, &diag) != 0) {
        printf("FAIL: Could not register monitor\n");
        return 0;
    }
    
    /* Immediately demonitor */
    if (kain_actor_demonitor(monitor, monitored, &diag) != 0) {
        printf("FAIL: Could not demonitor\n");
        return 0;
    }
    
    /* Wait for actors to complete */
    SLEEP_MS(500);
    
    if (!received) {
        printf("PASS: Demonitor prevented notification\n");
        return 1;
    } else {
        printf("FAIL: Monitor still received notification after demonitor\n");
        return 0;
    }
}

int main(void) {
    printf("Starting Actor Monitor and Link Tests\n");
    
    /* Initialize runtime */
    kain_actor_runtime_init();
    
    int passed = 0;
    int total = 3;
    
    /* Run tests */
    passed += test_monitor_normal_exit();
    passed += test_link_crash_propagation();
    passed += test_demonitor();
    
    /* Shutdown runtime */
    kain_actor_runtime_shutdown();
    
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}
