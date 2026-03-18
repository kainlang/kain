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

/* Counters for child restarts */
static int g_child_start_count = 0;
static int g_bounded_child_start_count = 0;

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

/* Child actor that crashes forever to exercise restart limits */
static KainActorExitReason bounded_crash_child_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)mailbox;
    (void)user_data;

    g_bounded_child_start_count++;
    printf("Bounded child %llu crashing (attempt %d)\n", actor_id, g_bounded_child_start_count);
    return KAIN_ACTOR_EXIT_CRASHED;
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
    
    while (1) {
        int result = kain_actor_receive(mailbox, &msg, &diag);
        if (result != 0) {
            break;
        }
        
        printf("Supervisor %llu received notification (type: 0x%llx)\n", 
               actor_id, msg.type_tag);
        
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

int main(void) {
    printf("=== Actor Runtime Smoke Test: Supervision ===\n\n");
    
    /* Initialize actor runtime */
    kain_actor_runtime_init();
    
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
    
    /* Test 1: Permanent restart policy */
    printf("\n--- Test 1: Permanent Restart Policy ---\n");

    g_bounded_child_start_count = 0;

    KainActorSpawnConfig child_config;
    kain_actor_spawn_config_init(&child_config);
    child_config.bootstrap_fn = bounded_crash_child_bootstrap;
    child_config.supervisor_id = supervisor_id;
    child_config.restart_policy = KAIN_RESTART_POLICY_PERMANENT;
    strncpy(child_config.name, "permanent_child", KAIN_ACTOR_NAME_MAX);
    
    KainActorId child_id = kain_actor_spawn(&child_config, &diag);
    
    if (child_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Child spawn failed: %s\n", diag.message);
        return 1;
    }
    printf("Child spawned with ID: %llu (restart policy: PERMANENT)\n\n", child_id);
    
    /* Wait for crash loop and restart limit to settle */
    sleep(3);

    KainActorSupervisionSnapshot perm_snapshot;
    if (kain_actor_get_supervision_snapshot(child_id, &perm_snapshot, &diag) != 0) {
        printf("FAIL: Could not read permanent child supervision snapshot: %s\n", diag.message);
        return 1;
    }

    printf("Permanent child starts: %d\n", g_bounded_child_start_count);
    printf("Permanent child restart count: %d\n", perm_snapshot.restart_count);
    printf("Permanent child last exit reason: %d\n", perm_snapshot.last_child_exit_reason);

    if (perm_snapshot.restart_count != 1) {
        printf("FAIL: Permanent child should report one restart on its original generation\n");
        return 1;
    }
    if (g_bounded_child_start_count < KAIN_SUPERVISION_MAX_RESTARTS + 1) {
        printf("FAIL: Permanent child should have restarted until the configured limit\n");
        return 1;
    }
    if (perm_snapshot.last_child_exit_reason != KAIN_ACTOR_EXIT_CRASHED) {
        printf("FAIL: Permanent child should record a crashed child exit\n");
        return 1;
    }

    KainActorSupervisionSnapshot supervisor_snapshot;
    if (kain_actor_get_supervision_snapshot(supervisor_id, &supervisor_snapshot, &diag) != 0) {
        printf("FAIL: Could not read supervisor supervision snapshot: %s\n", diag.message);
        return 1;
    }

    printf("Supervisor observed child exits: %zu\n", supervisor_snapshot.observed_child_exit_count);
    printf("Supervisor restart-limit hits: %d\n", supervisor_snapshot.supervision_limit_hits);

    if (supervisor_snapshot.observed_child_exit_count < KAIN_SUPERVISION_MAX_RESTARTS + 1) {
        printf("FAIL: Supervisor should observe the bounded restart loop\n");
        return 1;
    }
    if (supervisor_snapshot.supervision_limit_hits < 1) {
        printf("FAIL: Supervisor should record at least one restart-limit hit\n");
        return 1;
    }
    if (supervisor_snapshot.last_observed_child_exit_reason != KAIN_ACTOR_EXIT_CRASHED) {
        printf("FAIL: Supervisor should record the last child exit as crashed\n");
        return 1;
    }
    if (kain_actor_get_state(child_id) != KAIN_ACTOR_STATE_FAILED) {
        printf("FAIL: Permanent child should remain in FAILED state after its initial crash\n");
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
    
    KainActorSupervisionSnapshot temp_snapshot;
    if (kain_actor_get_supervision_snapshot(temp_id, &temp_snapshot, &diag) != 0) {
        printf("FAIL: Could not read temporary child supervision snapshot: %s\n", diag.message);
        return 1;
    }

    KainActorState temp_state = kain_actor_get_state(temp_id);
    printf("Temporary child state: %d\n", temp_state);
    printf("Temporary child restart count: %d\n", temp_snapshot.restart_count);

    if (temp_snapshot.restart_count != 0) {
        printf("FAIL: Temporary child should not restart\n");
        return 1;
    }
    if (temp_snapshot.last_child_exit_reason != KAIN_ACTOR_EXIT_NORMAL) {
        printf("FAIL: Temporary child should record a normal exit\n");
        return 1;
    }
    if (temp_snapshot.restart_limit_hit) {
        printf("FAIL: Temporary child should not hit the restart limit\n");
        return 1;
    }
    
    /* Test 3: Transient restart policy */
    printf("\n--- Test 3: Transient Restart Policy ---\n");
    
    g_child_start_count = 0;

    KainActorSpawnConfig transient_config;
    kain_actor_spawn_config_init(&transient_config);
    transient_config.bootstrap_fn = supervised_child_bootstrap;
    transient_config.supervisor_id = supervisor_id;
    transient_config.restart_policy = KAIN_RESTART_POLICY_TRANSIENT;
    strncpy(transient_config.name, "transient_child", KAIN_ACTOR_NAME_MAX);
    
    KainActorId transient_id = kain_actor_spawn(&transient_config, &diag);
    printf("Transient child spawned with ID: %llu\n", transient_id);
    
    sleep(2);
    
    KainActorSupervisionSnapshot transient_snapshot;
    if (kain_actor_get_supervision_snapshot(transient_id, &transient_snapshot, &diag) != 0) {
        printf("FAIL: Could not read transient child supervision snapshot: %s\n", diag.message);
        return 1;
    }

    KainActorState transient_state = kain_actor_get_state(transient_id);
    printf("Transient child state: %d\n", transient_state);
    printf("Transient child restart count: %d\n", transient_snapshot.restart_count);

    if (transient_snapshot.restart_count != 1) {
        printf("FAIL: Transient child should restart once after an abnormal exit\n");
        return 1;
    }
    if (transient_snapshot.last_child_exit_reason != KAIN_ACTOR_EXIT_CRASHED) {
        printf("FAIL: Transient child should record a crashed child exit\n");
        return 1;
    }
    if (transient_snapshot.restart_limit_hit) {
        printf("FAIL: Transient child should not hit the restart limit in this smoke\n");
        return 1;
    }

    /* Shutdown */
    kain_actor_runtime_shutdown();
    
    printf("\nPASS: Actor supervision test completed successfully\n");
    printf("Note: Supervision restart behavior is now observable through snapshots\n");
    return 0;
}
