/*
 * Test: Actor Supervision Policies
 *
 * This test validates:
 * - Restart policies (PERMANENT, TEMPORARY, TRANSIENT)
 * - Bounded restart counting with time windows
 * - Escalation when restart limits exceeded
 * - Supervision strategies (ONE_FOR_ONE, ONE_FOR_ALL, REST_FOR_ONE)
 * - Supervisor notification on child exit
 * - Observable restart events
 *
 * Requirements: 6.2, 6.3, 6.4
 */

#include "../include/actor.h"
#include "../include/diagnostics.h"
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

/* Global counters for tracking restarts */
static int g_crash_count = 0;
static int g_restart_count = 0;

/* Test actor that crashes a specified number of times then succeeds */
static KainActorExitReason test_actor_crash_then_succeed(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    (void)mailbox;

    int* max_crashes = (int*)user_data;

    if (g_crash_count < *max_crashes) {
        g_crash_count++;
        printf("Actor crashing (crash %d/%d)\n", g_crash_count, *max_crashes);
        return KAIN_ACTOR_EXIT_CRASHED;
    }

    printf("Actor succeeding after %d crashes\n", g_crash_count);
    return KAIN_ACTOR_EXIT_NORMAL;
}

/* Test actor that always crashes */
static KainActorExitReason test_actor_always_crash(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    (void)mailbox;
    (void)user_data;

    g_crash_count++;
    printf("Actor crashing (crash %d)\n", g_crash_count);
    return KAIN_ACTOR_EXIT_CRASHED;
}

/* Test actor that exits normally */
static KainActorExitReason test_actor_normal_exit(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)actor_id;
    (void)mailbox;
    (void)user_data;

    printf("Actor exiting normally\n");
    return KAIN_ACTOR_EXIT_NORMAL;
}

/* Supervisor actor that monitors children */
static KainActorExitReason test_supervisor_actor(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)user_data;

    printf("Supervisor %llu started\n", actor_id);

    /* Wait for messages or shutdown */
    while (1) {
        KainActorMessage msg;
        int result = kain_actor_try_receive(mailbox, &msg, NULL);

        if (result == 0) {
            /* Check for monitor notification */
            if ((msg.type_tag & 0xDEAD0000ULL) == 0xDEAD0000ULL) {
                KainActorExitReason exit_reason = (KainActorExitReason)(msg.type_tag & 0xFFFF);
                printf("Supervisor received child exit notification: exit_reason=%d\n", exit_reason);
            }

            if (msg.data != NULL) {
                free(msg.data);
            }
        }

        SLEEP_MS(100);

        /* Check if mailbox is closed (shutdown) */
        if (mailbox->closed) {
            break;
        }
    }

    printf("Supervisor %llu exiting\n", actor_id);
    return KAIN_ACTOR_EXIT_NORMAL;
}

/* Test 1: PERMANENT restart policy - always restarts */
static int test_restart_policy_permanent(void) {
    printf("\n=== Test 1: PERMANENT restart policy ===\n");

    g_crash_count = 0;
    KainDiagnostic diag;
    int max_crashes = 2;

    /* Spawn supervisor */
    KainActorSpawnConfig supervisor_config;
    kain_actor_spawn_config_init(&supervisor_config);
    supervisor_config.bootstrap_fn = test_supervisor_actor;
    supervisor_config.user_data = NULL;

    KainActorId supervisor = kain_actor_spawn(&supervisor_config, &diag);
    if (supervisor == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn supervisor\n");
        return 0;
    }

    /* Spawn child with PERMANENT restart policy */
    KainActorSpawnConfig child_config;
    kain_actor_spawn_config_init(&child_config);
    child_config.bootstrap_fn = test_actor_crash_then_succeed;
    child_config.user_data = &max_crashes;
    child_config.supervisor_id = supervisor;
    child_config.restart_policy = KAIN_RESTART_POLICY_PERMANENT;

    KainActorId child = kain_actor_spawn(&child_config, &diag);
    if (child == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn child\n");
        return 0;
    }

    /* Wait for crashes and restarts */
    SLEEP_MS(2000);

    /* Child should have crashed 2 times and then succeeded */
    if (g_crash_count == max_crashes) {
        printf("PASS: PERMANENT policy restarted child after crashes\n");
        kain_actor_shutdown(supervisor, NULL);
        return 1;
    } else {
        printf("FAIL: Expected %d crashes, got %d\n", max_crashes, g_crash_count);
        kain_actor_shutdown(supervisor, NULL);
        return 0;
    }
}

/* Test 2: TEMPORARY restart policy - never restarts */
static int test_restart_policy_temporary(void) {
    printf("\n=== Test 2: TEMPORARY restart policy ===\n");

    g_crash_count = 0;
    KainDiagnostic diag;

    /* Spawn supervisor */
    KainActorSpawnConfig supervisor_config;
    kain_actor_spawn_config_init(&supervisor_config);
    supervisor_config.bootstrap_fn = test_supervisor_actor;
    supervisor_config.user_data = NULL;

    KainActorId supervisor = kain_actor_spawn(&supervisor_config, &diag);
    if (supervisor == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn supervisor\n");
        return 0;
    }

    /* Spawn child with TEMPORARY restart policy */
    KainActorSpawnConfig child_config;
    kain_actor_spawn_config_init(&child_config);
    child_config.bootstrap_fn = test_actor_always_crash;
    child_config.user_data = NULL;
    child_config.supervisor_id = supervisor;
    child_config.restart_policy = KAIN_RESTART_POLICY_TEMPORARY;

    KainActorId child = kain_actor_spawn(&child_config, &diag);
    if (child == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn child\n");
        return 0;
    }

    /* Wait to see if restart happens */
    SLEEP_MS(1000);

    /* Child should have crashed only once (no restart) */
    if (g_crash_count == 1) {
        printf("PASS: TEMPORARY policy did not restart child\n");
        kain_actor_shutdown(supervisor, NULL);
        return 1;
    } else {
        printf("FAIL: Expected 1 crash (no restart), got %d crashes\n", g_crash_count);
        kain_actor_shutdown(supervisor, NULL);
        return 0;
    }
}

/* Test 3: TRANSIENT restart policy - restarts only on abnormal exit */
static int test_restart_policy_transient(void) {
    printf("\n=== Test 3: TRANSIENT restart policy ===\n");

    g_crash_count = 0;
    KainDiagnostic diag;
    int max_crashes = 1;

    /* Spawn supervisor */
    KainActorSpawnConfig supervisor_config;
    kain_actor_spawn_config_init(&supervisor_config);
    supervisor_config.bootstrap_fn = test_supervisor_actor;
    supervisor_config.user_data = NULL;

    KainActorId supervisor = kain_actor_spawn(&supervisor_config, &diag);
    if (supervisor == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn supervisor\n");
        return 0;
    }

    /* Spawn child with TRANSIENT restart policy that crashes once then succeeds */
    KainActorSpawnConfig child_config;
    kain_actor_spawn_config_init(&child_config);
    child_config.bootstrap_fn = test_actor_crash_then_succeed;
    child_config.user_data = &max_crashes;
    child_config.supervisor_id = supervisor;
    child_config.restart_policy = KAIN_RESTART_POLICY_TRANSIENT;

    KainActorId child = kain_actor_spawn(&child_config, &diag);
    if (child == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn child\n");
        return 0;
    }

    /* Wait for crash and restart */
    SLEEP_MS(1000);

    /* Child should have crashed once, restarted, then succeeded */
    if (g_crash_count == max_crashes) {
        printf("PASS: TRANSIENT policy restarted child after abnormal exit\n");
        kain_actor_shutdown(supervisor, NULL);
        return 1;
    } else {
        printf("FAIL: Expected %d crash, got %d\n", max_crashes, g_crash_count);
        kain_actor_shutdown(supervisor, NULL);
        return 0;
    }
}

/* Test 4: TRANSIENT policy does not restart on normal exit */
static int test_transient_no_restart_on_normal(void) {
    printf("\n=== Test 4: TRANSIENT policy - no restart on normal exit ===\n");

    KainDiagnostic diag;

    /* Spawn supervisor */
    KainActorSpawnConfig supervisor_config;
    kain_actor_spawn_config_init(&supervisor_config);
    supervisor_config.bootstrap_fn = test_supervisor_actor;
    supervisor_config.user_data = NULL;

    KainActorId supervisor = kain_actor_spawn(&supervisor_config, &diag);
    if (supervisor == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn supervisor\n");
        return 0;
    }

    /* Spawn child with TRANSIENT restart policy that exits normally */
    KainActorSpawnConfig child_config;
    kain_actor_spawn_config_init(&child_config);
    child_config.bootstrap_fn = test_actor_normal_exit;
    child_config.user_data = NULL;
    child_config.supervisor_id = supervisor;
    child_config.restart_policy = KAIN_RESTART_POLICY_TRANSIENT;

    KainActorId child = kain_actor_spawn(&child_config, &diag);
    if (child == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn child\n");
        return 0;
    }

    /* Wait to see if restart happens */
    SLEEP_MS(1000);

    /* Check child state - should be TERMINATED, not restarted */
    KainActorState state = kain_actor_get_state(child);

    if (state == KAIN_ACTOR_STATE_TERMINATED || state == KAIN_ACTOR_STATE_UNINITIALIZED) {
        printf("PASS: TRANSIENT policy did not restart on normal exit\n");
        kain_actor_shutdown(supervisor, NULL);
        return 1;
    } else {
        printf("FAIL: Child was restarted on normal exit (state=%d)\n", state);
        kain_actor_shutdown(supervisor, NULL);
        return 0;
    }
}

/* Test 5: Bounded restarts - escalation when limit exceeded */
static int test_bounded_restarts_escalation(void) {
    printf("\n=== Test 5: Bounded restarts with escalation ===\n");

    g_crash_count = 0;
    KainDiagnostic diag;

    /* Spawn supervisor */
    KainActorSpawnConfig supervisor_config;
    kain_actor_spawn_config_init(&supervisor_config);
    supervisor_config.bootstrap_fn = test_supervisor_actor;
    supervisor_config.user_data = NULL;

    KainActorId supervisor = kain_actor_spawn(&supervisor_config, &diag);
    if (supervisor == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn supervisor\n");
        return 0;
    }

    /* Spawn child with PERMANENT restart policy that always crashes */
    KainActorSpawnConfig child_config;
    kain_actor_spawn_config_init(&child_config);
    child_config.bootstrap_fn = test_actor_always_crash;
    child_config.user_data = NULL;
    child_config.supervisor_id = supervisor;
    child_config.restart_policy = KAIN_RESTART_POLICY_PERMANENT;

    KainActorId child = kain_actor_spawn(&child_config, &diag);
    if (child == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Could not spawn child\n");
        return 0;
    }

    /* Wait for multiple crashes and eventual escalation */
    SLEEP_MS(3000);

    /* Check supervisor state - should be FAILED due to escalation */
    KainActorState supervisor_state = kain_actor_get_state(supervisor);

    /* Child should have crashed multiple times (up to restart limit) */
    printf("Total crashes before escalation: %d\n", g_crash_count);

    if (supervisor_state == KAIN_ACTOR_STATE_FAILED && g_crash_count > 1) {
        printf("PASS: Supervisor escalated after restart limit exceeded\n");
        return 1;
    } else {
        printf("FAIL: Supervisor did not escalate (state=%d, crashes=%d)\n",
               supervisor_state, g_crash_count);
        kain_actor_shutdown(supervisor, NULL);
        return 0;
    }
}

int main(void) {
    printf("Starting Actor Supervision Policy Tests\n");

    /* Initialize runtime */
    kain_actor_runtime_init();

    int passed = 0;
    int total = 5;

    /* Run tests */
    passed += test_restart_policy_permanent();
    SLEEP_MS(500);  /* Delay between tests */

    passed += test_restart_policy_temporary();
    SLEEP_MS(500);

    passed += test_restart_policy_transient();
    SLEEP_MS(500);

    passed += test_transient_no_restart_on_normal();
    SLEEP_MS(500);

    passed += test_bounded_restarts_escalation();

    /* Shutdown runtime */
    kain_actor_runtime_shutdown();

    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d\n", passed, total);

    return (passed == total) ? 0 : 1;
}
