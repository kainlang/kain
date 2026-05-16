/*
 * Actor Runtime Conformance Test: Monitors
 *
 * Tests monitor relationships and exit notifications.
 *
 * Requirements: 6.3
 */

#include "../../native/include/actor.h"
#include "../../native/include/diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

/* Global flag to track if monitor notification was received */
static int g_monitor_notification_received = 0;

static void copy_actor_name(char* dest, const char* src) {
    snprintf(dest, KAIN_ACTOR_NAME_MAX, "%s", src);
}

/* Monitored actor that exits after a short delay */
static KainActorExitReason monitored_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)mailbox;
    (void)user_data;
    printf("Monitored actor %llu starting\n", actor_id);
    sleep(1);
    printf("Monitored actor %llu exiting normally\n", actor_id);
    return KAIN_ACTOR_EXIT_NORMAL;
}

/* Monitoring actor that waits for exit notification */
static KainActorExitReason monitoring_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)user_data;
    printf("Monitoring actor %llu starting\n", actor_id);

    KainActorMessage msg;
    KainDiagnostic diag;

    /* Wait for monitor notification (type_tag 0xDEAD0000 | exit_reason) */
    int result = kain_actor_receive(mailbox, &msg, &diag);
    if (result == 0) {
        printf("Monitoring actor %llu received notification with type_tag: 0x%llx\n",
               actor_id, msg.type_tag);
        if ((msg.type_tag & 0xFFFF0000ULL) == 0xDEAD0000ULL) {
            printf("Monitor notification received from actor %llu\n", msg.sender_id);
            g_monitor_notification_received = 1;
        }
        if (msg.data != NULL) {
            free(msg.data);
        }
    }

    printf("Monitoring actor %llu exiting\n", actor_id);
    return KAIN_ACTOR_EXIT_NORMAL;
}

int main(void) {
    printf("=== Actor Runtime Smoke Test: Monitors ===\n\n");

    /* Initialize actor runtime */
    kain_actor_runtime_init();

    /* Spawn monitoring actor */
    KainActorSpawnConfig monitor_config;
    kain_actor_spawn_config_init(&monitor_config);
    monitor_config.bootstrap_fn = monitoring_actor_bootstrap;
    copy_actor_name(monitor_config.name, "monitor");

    KainDiagnostic diag;
    KainActorId monitor_id = kain_actor_spawn(&monitor_config, &diag);

    if (monitor_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Monitor actor spawn failed: %s\n", diag.message);
        return 1;
    }
    printf("Monitor actor spawned with ID: %llu\n", monitor_id);

    /* Spawn monitored actor */
    KainActorSpawnConfig monitored_config;
    kain_actor_spawn_config_init(&monitored_config);
    monitored_config.bootstrap_fn = monitored_actor_bootstrap;
    copy_actor_name(monitored_config.name, "monitored");

    KainActorId monitored_id = kain_actor_spawn(&monitored_config, &diag);

    if (monitored_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Monitored actor spawn failed: %s\n", diag.message);
        return 1;
    }
    printf("Monitored actor spawned with ID: %llu\n", monitored_id);

    /* Set up monitor relationship */
    int result = kain_actor_monitor(monitor_id, monitored_id, &diag);
    if (result != 0) {
        printf("FAIL: Monitor setup failed: %s\n", diag.message);
        return 1;
    }
    printf("Monitor relationship established\n\n");

    /* Wait for actors to complete */
    sleep(3);

    /* Verify monitor notification was received */
    if (!g_monitor_notification_received) {
        printf("FAIL: Monitor notification was not received\n");
        return 1;
    }
    printf("\nMonitor notification verified\n");

    /* Shutdown */
    kain_actor_runtime_shutdown();

    printf("\nPASS: Actor monitors test completed successfully\n");
    return 0;
}
