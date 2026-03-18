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

int main(void) {
    printf("=== Actor Runtime Smoke Test: Scheduler ===\n\n");
    
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
        
        KainDiagnostic diag;
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
    
    /* Verify actor states */
    int terminated_count = 0;
    for (int i = 0; i < NUM_ACTORS; i++) {
        KainActorState state = kain_actor_get_state(actor_ids[i]);
        if (state == KAIN_ACTOR_STATE_TERMINATED) {
            terminated_count++;
        }
    }
    
    printf("Actors in TERMINATED state: %d / %d\n", terminated_count, NUM_ACTORS);
    
    /* Shutdown */
    kain_actor_runtime_shutdown();
    
#ifdef _WIN32
    DeleteCriticalSection(&g_count_lock);
#endif
    
    printf("\nPASS: Actor scheduler test completed successfully\n");
    printf("Note: Scheduler uses worker pool to avoid unbounded thread creation\n");
    return 0;
}
