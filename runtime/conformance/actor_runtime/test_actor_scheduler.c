/*
 * Actor Runtime Conformance Test: Scheduler
 *
 * Tests work-stealing scheduler with worker pool.
 *
 * Requirements: 6.5, 6.6
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

/* Counter for completed actors */
static int g_completed_count = 0;
static int g_overflow_actor_completed = 0;

#ifdef _WIN32
static CRITICAL_SECTION g_count_lock;
#else
static pthread_mutex_t g_count_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Simple actor that increments counter and exits */
static KainActorExitReason counter_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)mailbox;
    int* value = (int*)user_data;
    
    printf("Actor %llu executing with value %d\n", actor_id, *value);
    
    /* Simulate some work */
    for (int i = 0; i < 1000000; i++) {
        /* Busy work */
    }
    
#ifdef _WIN32
    EnterCriticalSection(&g_count_lock);
#else
    pthread_mutex_lock(&g_count_lock);
#endif
    
    g_completed_count++;
    
#ifdef _WIN32
    LeaveCriticalSection(&g_count_lock);
#else
    pthread_mutex_unlock(&g_count_lock);
#endif
    
    printf("Actor %llu completed\n", actor_id);
    return KAIN_ACTOR_EXIT_NORMAL;
}

static KainActorExitReason blocking_actor_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    const char* label = (const char*)user_data;
    KainActorMessage msg;
    KainDiagnostic diag;

    printf("Blocking actor %s (%llu) waiting on mailbox\n", label ? label : "unnamed", actor_id);

    while (kain_actor_receive(mailbox, &msg, &diag) == 0) {
        if (msg.data != NULL) {
            free(msg.data);
        }
    }

    printf("Blocking actor %s (%llu) exiting after mailbox close\n", label ? label : "unnamed", actor_id);
    return KAIN_ACTOR_EXIT_SHUTDOWN;
}

static KainActorExitReason overflow_probe_bootstrap(
    KainActorId actor_id,
    KainActorMailbox* mailbox,
    void* user_data
) {
    (void)mailbox;
    (void)user_data;

    printf("Overflow probe actor %llu completed via overflow path\n", actor_id);

#ifdef _WIN32
    EnterCriticalSection(&g_count_lock);
#else
    pthread_mutex_lock(&g_count_lock);
#endif
    g_overflow_actor_completed++;
#ifdef _WIN32
    LeaveCriticalSection(&g_count_lock);
#else
    pthread_mutex_unlock(&g_count_lock);
#endif

    return KAIN_ACTOR_EXIT_NORMAL;
}

int main(void) {
    printf("=== Actor Runtime Smoke Test: Scheduler ===\n\n");
    KainDiagnostic diag;
    
#ifdef _WIN32
    InitializeCriticalSection(&g_count_lock);
#endif
    
    /* Initialize actor runtime (will init scheduler) */
    kain_actor_runtime_init();
    
    printf("Spawning 20 actors to test scheduler work distribution...\n\n");
    
    /* Spawn multiple actors to test scheduler */
    #define NUM_ACTORS 20
    KainActorId actor_ids[NUM_ACTORS];
    int actor_values[NUM_ACTORS];
    
    for (int i = 0; i < NUM_ACTORS; i++) {
        actor_values[i] = i;
        
        KainActorSpawnConfig config;
        kain_actor_spawn_config_init(&config);
        config.bootstrap_fn = counter_actor_bootstrap;
        config.user_data = &actor_values[i];
        snprintf(config.name, KAIN_ACTOR_NAME_MAX, "actor_%d", i);
        
        actor_ids[i] = kain_actor_spawn(&config, &diag);
        
        if (actor_ids[i] == KAIN_ACTOR_ID_INVALID) {
            printf("FAIL: Actor %d spawn failed: %s\n", i, diag.message);
            return 1;
        }
    }
    
    printf("All actors spawned, waiting for completion...\n\n");
    
    /* Wait for all actors to complete */
    sleep(5);
    
    /* Check completion count */
    printf("\nCompleted actors: %d / %d\n", g_completed_count, NUM_ACTORS);
    
    if (g_completed_count != NUM_ACTORS) {
        printf("FAIL: Not all actors completed\n");
        return 1;
    }

    KainActorSchedulerSnapshot scheduler_snapshot;
    kain_actor_scheduler_snapshot(&scheduler_snapshot);

    printf("Scheduler queue depth: %zu\n", scheduler_snapshot.queue_depth);
    printf("Scheduler max queue depth: %zu\n", scheduler_snapshot.max_queue_depth);
    printf("Scheduler enqueued/dequeued: %zu/%zu\n",
           scheduler_snapshot.total_enqueued,
           scheduler_snapshot.total_dequeued);
    printf("Scheduler workers active: %d/%d\n",
           scheduler_snapshot.active_workers,
           scheduler_snapshot.worker_count);
    printf("Scheduler busy workers: %zu (max %zu)\n",
           scheduler_snapshot.busy_workers,
           scheduler_snapshot.max_busy_workers);

    if (scheduler_snapshot.queue_depth != 0) {
        printf("FAIL: Scheduler queue should be empty after all actors complete\n");
        return 1;
    }
    if (scheduler_snapshot.total_enqueued != scheduler_snapshot.total_dequeued) {
        printf("FAIL: Scheduler enqueue and dequeue counters should stay balanced\n");
        return 1;
    }
    if (scheduler_snapshot.total_enqueued == 0) {
        printf("FAIL: Scheduler should record at least one queued actor under load\n");
        return 1;
    }
    if (scheduler_snapshot.max_queue_depth == 0) {
        printf("FAIL: Scheduler should have observed a non-zero queue depth\n");
        return 1;
    }
    if (scheduler_snapshot.worker_count != 4) {
        printf("FAIL: Scheduler worker count should remain fixed at the pooled size\n");
        return 1;
    }
    if (scheduler_snapshot.max_busy_workers == 0) {
        printf("FAIL: Scheduler should record busy worker activity\n");
        return 1;
    }
    
    /* Verify actor states */
    int terminated_count = 0;
    for (int i = 0; i < NUM_ACTORS; i++) {
        KainActorState state = kain_actor_get_state(actor_ids[i]);
        if (state == KAIN_ACTOR_STATE_TERMINATED) {
            terminated_count++;
        }
    }
    
    printf("Actors in TERMINATED state: %d / %d\n", terminated_count, NUM_ACTORS);

    printf("\nSpawning blocking actors to saturate the pooled scheduler...\n\n");

    #define NUM_BLOCKING_ACTORS 4
    KainActorId blocking_ids[NUM_BLOCKING_ACTORS];
    const char* blocking_labels[NUM_BLOCKING_ACTORS] = {
        "block_0",
        "block_1",
        "block_2",
        "block_3"
    };

    for (int i = 0; i < NUM_BLOCKING_ACTORS; i++) {
        KainActorSpawnConfig blocking_config;
        kain_actor_spawn_config_init(&blocking_config);
        blocking_config.bootstrap_fn = blocking_actor_bootstrap;
        blocking_config.user_data = (void*)blocking_labels[i];
        snprintf(blocking_config.name, KAIN_ACTOR_NAME_MAX, "blocking_%d", i);

        blocking_ids[i] = kain_actor_spawn(&blocking_config, &diag);
        if (blocking_ids[i] == KAIN_ACTOR_ID_INVALID) {
            printf("FAIL: Blocking actor %d spawn failed: %s\n", i, diag.message);
            return 1;
        }
    }

    sleep(1);

    KainActorSpawnConfig overflow_config;
    kain_actor_spawn_config_init(&overflow_config);
    overflow_config.bootstrap_fn = overflow_probe_bootstrap;
    snprintf(overflow_config.name, KAIN_ACTOR_NAME_MAX, "overflow_probe");

    KainActorId overflow_actor_id = kain_actor_spawn(&overflow_config, &diag);
    if (overflow_actor_id == KAIN_ACTOR_ID_INVALID) {
        printf("FAIL: Overflow probe actor spawn failed: %s\n", diag.message);
        return 1;
    }

    sleep(1);

    KainActorSchedulerSnapshot overflow_snapshot;
    kain_actor_scheduler_snapshot(&overflow_snapshot);

    printf("Overflow probe completions: %d\n", g_overflow_actor_completed);
    printf("Scheduler overflow thread spawns: %zu\n", overflow_snapshot.overflow_thread_spawns);

    if (g_overflow_actor_completed != 1) {
        printf("FAIL: Overflow probe actor should complete even while pooled workers are blocked\n");
        return 1;
    }
    if (overflow_snapshot.overflow_thread_spawns < 1) {
        printf("FAIL: Scheduler should record at least one overflow thread spawn under saturation\n");
        return 1;
    }

    for (int i = 0; i < NUM_BLOCKING_ACTORS; i++) {
        if (kain_actor_shutdown(blocking_ids[i], &diag) != 0) {
            printf("FAIL: Could not shut down blocking actor %d: %s\n", i, diag.message);
            return 1;
        }
    }

    sleep(1);
    
    /* Shutdown */
    kain_actor_runtime_shutdown();
    
#ifdef _WIN32
    DeleteCriticalSection(&g_count_lock);
#endif
    
    printf("\nPASS: Actor scheduler test completed successfully\n");
    printf("Note: Scheduler uses worker pool to avoid unbounded thread creation\n");
    return 0;
}
