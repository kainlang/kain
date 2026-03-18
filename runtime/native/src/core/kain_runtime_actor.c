/*
 * KAIN Native Runtime Actor Implementation
 *
 * This file implements the actor runtime for the KAIN native lane.
 * It provides actor spawn, mailbox operations, supervision, monitoring,
 * registry, and scheduler integration.
 *
 * Requirements: 5.2, 5.3, 5.4, 5.5, 6.1, 6.2, 6.3, 6.4
 */

#include "../../include/kain_runtime_actor.h"
#include "../../include/kain_runtime_base.h"
#include "../../include/kain_runtime_diagnostics.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Global actor table */
#define KAIN_ACTOR_TABLE_SIZE 1024

typedef struct {
    KainActorState_Internal* actors[KAIN_ACTOR_TABLE_SIZE];
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
    KainActorId next_id;
} KainActorTable;

static KainActorTable g_actor_table = {0};
static int g_actor_runtime_initialized = 0;

/* Actor registry */
#define KAIN_ACTOR_REGISTRY_SIZE 256

typedef struct KainActorRegistryEntry {
    char name[KAIN_ACTOR_NAME_MAX];
    KainActorId actor_id;
    struct KainActorRegistryEntry* next;
} KainActorRegistryEntry;

typedef struct {
    KainActorRegistryEntry* buckets[KAIN_ACTOR_REGISTRY_SIZE];
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
} KainActorRegistry;

static KainActorRegistry g_actor_registry = {0};

/* Actor scheduler - work queue for actor execution */
#define KAIN_SCHEDULER_WORKER_COUNT 4
#define KAIN_SCHEDULER_USE_POOLED 1  /* 1 = use worker pool, 0 = thread-per-actor */

typedef struct {
    KainActorSchedulerNode* head;
    KainActorSchedulerNode* tail;
    int shutdown;
    int active_workers;
    
#ifdef _WIN32
    CRITICAL_SECTION lock;
    HANDLE work_available;
    HANDLE* worker_threads;
    DWORD* worker_thread_ids;
#else
    pthread_mutex_t lock;
    pthread_cond_t work_available;
    pthread_t* worker_threads;
#endif
} KainActorScheduler;

static KainActorScheduler g_scheduler = {0};

/* Forward declarations */
static void kain_actor_cleanup(KainActorState_Internal* actor);
static void kain_actor_notify_monitors(KainActorState_Internal* actor);
static void kain_actor_propagate_links(KainActorState_Internal* actor);
static unsigned int kain_actor_registry_hash(const char* name);
static void kain_scheduler_init(void);
static void kain_scheduler_shutdown(void);
static void kain_scheduler_enqueue(KainActorId actor_id);
static KainActorId kain_scheduler_dequeue(void);

/*
 * Initialize Actor Runtime
 *
 * Must be called before any actor operations.
 */
void kain_actor_runtime_init(void) {
    if (g_actor_runtime_initialized) {
        return;
    }

#ifdef _WIN32
    InitializeCriticalSection(&g_actor_table.lock);
    InitializeCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_init(&g_actor_table.lock, NULL);
    pthread_mutex_init(&g_actor_registry.lock, NULL);
#endif

    g_actor_table.next_id = 1;
    memset(g_actor_table.actors, 0, sizeof(g_actor_table.actors));
    memset(g_actor_registry.buckets, 0, sizeof(g_actor_registry.buckets));

    /* Initialize scheduler if using pooled mode */
    if (KAIN_SCHEDULER_USE_POOLED) {
        kain_scheduler_init();
    }

    g_actor_runtime_initialized = 1;
}

/*
 * Shutdown Actor Runtime
 *
 * Terminates all actors and cleans up resources.
 */
void kain_actor_runtime_shutdown(void) {
    if (!g_actor_runtime_initialized) {
        return;
    }

    /* Shutdown scheduler first if using pooled mode */
    if (KAIN_SCHEDULER_USE_POOLED) {
        kain_scheduler_shutdown();
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif

    /* Terminate all actors */
    for (int i = 0; i < KAIN_ACTOR_TABLE_SIZE; i++) {
        if (g_actor_table.actors[i] != NULL) {
            kain_actor_cleanup(g_actor_table.actors[i]);
            free(g_actor_table.actors[i]);
            g_actor_table.actors[i] = NULL;
        }
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
    DeleteCriticalSection(&g_actor_table.lock);
    DeleteCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
    pthread_mutex_destroy(&g_actor_table.lock);
    pthread_mutex_destroy(&g_actor_registry.lock);
#endif

    g_actor_runtime_initialized = 0;
}

/*
 * Mailbox Operations
 */

static int kain_actor_mailbox_init(KainActorMailbox* mailbox, size_t capacity) {
    mailbox->head = NULL;
    mailbox->tail = NULL;
    mailbox->capacity = capacity;
    mailbox->count = 0;
    mailbox->closed = 0;

#ifdef _WIN32
    InitializeCriticalSection(&mailbox->lock);
    mailbox->not_empty = CreateEvent(NULL, FALSE, FALSE, NULL);
    mailbox->not_full = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (mailbox->not_empty == NULL || mailbox->not_full == NULL) {
        return -1;
    }
#else
    pthread_mutex_init(&mailbox->lock, NULL);
    pthread_cond_init(&mailbox->not_empty, NULL);
    pthread_cond_init(&mailbox->not_full, NULL);
#endif

    return 0;
}

static void kain_actor_mailbox_destroy(KainActorMailbox* mailbox) {
    /* Free all messages */
    MessageNode* node = mailbox->head;
    while (node != NULL) {
        MessageNode* next = node->next;
        if (node->data != NULL) {
            free(node->data);
        }
        free(node);
        node = next;
    }

#ifdef _WIN32
    DeleteCriticalSection(&mailbox->lock);
    CloseHandle(mailbox->not_empty);
    CloseHandle(mailbox->not_full);
#else
    pthread_mutex_destroy(&mailbox->lock);
    pthread_cond_destroy(&mailbox->not_empty);
    pthread_cond_destroy(&mailbox->not_full);
#endif
}

/*
 * Actor Table Operations
 */

static KainActorState_Internal* kain_actor_table_get(KainActorId actor_id) {
    if (actor_id == KAIN_ACTOR_ID_INVALID || actor_id >= KAIN_ACTOR_TABLE_SIZE) {
        return NULL;
    }
    return g_actor_table.actors[actor_id];
}

static KainActorId kain_actor_table_insert(KainActorState_Internal* actor) {
#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif

    KainActorId id = KAIN_ACTOR_ID_INVALID;
    
    /* Find free slot */
    for (size_t i = 1; i < KAIN_ACTOR_TABLE_SIZE; i++) {
        if (g_actor_table.actors[i] == NULL) {
            id = i;
            actor->actor_id = id;
            g_actor_table.actors[i] = actor;
            break;
        }
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif

    return id;
}

static void kain_actor_table_remove(KainActorId actor_id) {
    if (actor_id == KAIN_ACTOR_ID_INVALID || actor_id >= KAIN_ACTOR_TABLE_SIZE) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif

    g_actor_table.actors[actor_id] = NULL;

#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif
}

/*
 * Actor Bootstrap Thread Function
 */

#ifdef _WIN32
static DWORD WINAPI kain_actor_thread_proc(LPVOID param) {
#else
static void* kain_actor_thread_proc(void* param) {
#endif
    KainActorState_Internal* actor = (KainActorState_Internal*)param;
    
    /* Update state to running */
    actor->state = KAIN_ACTOR_STATE_RUNNING;
    
    /* Call bootstrap function */
    KainActorExitReason exit_reason = actor->bootstrap_fn(
        actor->actor_id,
        &actor->mailbox,
        actor->user_data
    );
    
    /* Update exit state */
    actor->exit_reason = exit_reason;
    actor->state = (exit_reason == KAIN_ACTOR_EXIT_NORMAL) 
        ? KAIN_ACTOR_STATE_TERMINATED 
        : KAIN_ACTOR_STATE_FAILED;
    
    /* Notify monitors and propagate links */
    kain_actor_notify_monitors(actor);
    if (exit_reason != KAIN_ACTOR_EXIT_NORMAL) {
        kain_actor_propagate_links(actor);
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/*
 * Actor Spawn Configuration
 */

void kain_actor_spawn_config_init(KainActorSpawnConfig* config) {
    memset(config, 0, sizeof(KainActorSpawnConfig));
    config->mailbox_capacity = KAIN_MAILBOX_DEFAULT_CAPACITY;
    config->restart_policy = KAIN_RESTART_POLICY_TEMPORARY;
    config->supervisor_id = KAIN_ACTOR_ID_INVALID;
}

/*
 * Actor Spawn
 */

KainActorId kain_actor_spawn(
    const KainActorSpawnConfig* config,
    KainDiagnostic* diag
) {
    if (!g_actor_runtime_initialized) {
        kain_actor_runtime_init();
    }

    if (config == NULL || config->bootstrap_fn == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Invalid spawn configuration");
            snprintf(diag->detail, sizeof(diag->detail), "Bootstrap function is required");
        }
        return KAIN_ACTOR_ID_INVALID;
    }

    /* Allocate actor state */
    KainActorState_Internal* actor = (KainActorState_Internal*)calloc(1, sizeof(KainActorState_Internal));
    if (actor == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Actor allocation failed");
        }
        return KAIN_ACTOR_ID_INVALID;
    }

    /* Initialize actor state */
    actor->state = KAIN_ACTOR_STATE_INITIALIZING;
    actor->exit_reason = KAIN_ACTOR_EXIT_NORMAL;
    actor->bootstrap_fn = config->bootstrap_fn;
    actor->user_data = config->user_data;
    actor->monitors = NULL;
    actor->links = NULL;
    actor->in_scheduler_queue = 0;
    
    if (config->name[0] != '\0') {
        strncpy(actor->name, config->name, KAIN_ACTOR_NAME_MAX - 1);
        actor->name[KAIN_ACTOR_NAME_MAX - 1] = '\0';
    }

    /* Initialize mailbox */
    if (kain_actor_mailbox_init(&actor->mailbox, config->mailbox_capacity) != 0) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Mailbox initialization failed");
        }
        free(actor);
        return KAIN_ACTOR_ID_INVALID;
    }

    /* Set supervisor if provided */
    if (config->supervisor_id != KAIN_ACTOR_ID_INVALID) {
        actor->supervisor.supervisor_id = config->supervisor_id;
        actor->supervisor.restart_policy = config->restart_policy;
        actor->supervisor.strategy = KAIN_SUPERVISION_STRATEGY_ONE_FOR_ONE;
        actor->supervisor.restart_count = 0;
        actor->supervisor.last_restart_time = 0;
    } else {
        actor->supervisor.supervisor_id = KAIN_ACTOR_ID_INVALID;
    }

    /* Insert into actor table */
    KainActorId actor_id = kain_actor_table_insert(actor);
    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Actor table full");
        }
        kain_actor_mailbox_destroy(&actor->mailbox);
        free(actor);
        return KAIN_ACTOR_ID_INVALID;
    }

    /* Spawn thread or enqueue to scheduler */
    if (KAIN_SCHEDULER_USE_POOLED) {
        /* Use scheduler work queue */
        kain_scheduler_enqueue(actor_id);
    } else {
        /* Use dedicated thread per actor */
#ifdef _WIN32
        actor->thread_handle = CreateThread(NULL, 0, kain_actor_thread_proc, actor, 0, &actor->thread_id);
        if (actor->thread_handle == NULL) {
            if (diag != NULL) {
                kain_diagnostic_init(diag);
                diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
                diag->severity = KAIN_DIAG_SEVERITY_ERROR;
                diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
                snprintf(diag->message, sizeof(diag->message), "Thread creation failed");
            }
            kain_actor_table_remove(actor_id);
            kain_actor_mailbox_destroy(&actor->mailbox);
            free(actor);
            return KAIN_ACTOR_ID_INVALID;
        }
#else
        int result = pthread_create(&actor->thread, NULL, kain_actor_thread_proc, actor);
        if (result != 0) {
            if (diag != NULL) {
                kain_diagnostic_init(diag);
                diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
                diag->severity = KAIN_DIAG_SEVERITY_ERROR;
                diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
                snprintf(diag->message, sizeof(diag->message), "Thread creation failed");
            }
            kain_actor_table_remove(actor_id);
            kain_actor_mailbox_destroy(&actor->mailbox);
            free(actor);
            return KAIN_ACTOR_ID_INVALID;
        }
#endif
    }

    return actor_id;
}

/*
 * Actor Send Message
 */

int kain_actor_send(
    KainActorId target_id,
    const KainActorMessage* message,
    KainDiagnostic* diag
) {
    KainActorState_Internal* actor = kain_actor_table_get(target_id);
    if (actor == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_NOT_FOUND;
            snprintf(diag->message, sizeof(diag->message), "Actor not found");
        }
        return -1;
    }

    KainActorMailbox* mailbox = &actor->mailbox;

#ifdef _WIN32
    EnterCriticalSection(&mailbox->lock);
#else
    pthread_mutex_lock(&mailbox->lock);
#endif

    /* Check if mailbox is closed */
    if (mailbox->closed) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_MAILBOX_CLOSED;
            snprintf(diag->message, sizeof(diag->message), "Mailbox is closed");
        }
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
#else
        pthread_mutex_unlock(&mailbox->lock);
#endif
        return -1;
    }

    /* Check capacity (bounded mailbox) */
    if (mailbox->capacity > 0 && mailbox->count >= mailbox->capacity) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_MAILBOX_FULL;
            snprintf(diag->message, sizeof(diag->message), "Mailbox is full");
        }
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
#else
        pthread_mutex_unlock(&mailbox->lock);
#endif
        return -1;
    }

    /* Create message node */
    MessageNode* node = (MessageNode*)malloc(sizeof(MessageNode));
    if (node == NULL) {
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
#else
        pthread_mutex_unlock(&mailbox->lock);
#endif
        return -1;
    }

    node->type_tag = message->type_tag;
    node->next = NULL;

    /* Copy message data */
    if (message->data != NULL && message->data_size > 0) {
        node->data = malloc(message->data_size);
        if (node->data == NULL) {
            free(node);
#ifdef _WIN32
            LeaveCriticalSection(&mailbox->lock);
#else
            pthread_mutex_unlock(&mailbox->lock);
#endif
            return -1;
        }
        memcpy(node->data, message->data, message->data_size);
    } else {
        node->data = NULL;
    }

    /* Add to queue */
    if (mailbox->tail == NULL) {
        mailbox->head = node;
        mailbox->tail = node;
    } else {
        mailbox->tail->next = node;
        mailbox->tail = node;
    }
    mailbox->count++;

    /* Signal not empty */
#ifdef _WIN32
    SetEvent(mailbox->not_empty);
    LeaveCriticalSection(&mailbox->lock);
#else
    pthread_cond_signal(&mailbox->not_empty);
    pthread_mutex_unlock(&mailbox->lock);
#endif

    return 0;
}

/*
 * Actor Receive Message
 */

int kain_actor_receive(
    KainActorMailbox* mailbox,
    KainActorMessage* message,
    KainDiagnostic* diag
) {
    if (mailbox == NULL || message == NULL) {
        return -1;
    }

#ifdef _WIN32
    EnterCriticalSection(&mailbox->lock);
#else
    pthread_mutex_lock(&mailbox->lock);
#endif

    /* Wait for message */
    while (mailbox->head == NULL && !mailbox->closed) {
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
        WaitForSingleObject(mailbox->not_empty, INFINITE);
        EnterCriticalSection(&mailbox->lock);
#else
        pthread_cond_wait(&mailbox->not_empty, &mailbox->lock);
#endif
    }

    /* Check if closed */
    if (mailbox->closed && mailbox->head == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_MAILBOX_CLOSED;
            snprintf(diag->message, sizeof(diag->message), "Mailbox is closed");
        }
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
#else
        pthread_mutex_unlock(&mailbox->lock);
#endif
        return -1;
    }

    /* Dequeue message */
    MessageNode* node = mailbox->head;
    mailbox->head = node->next;
    if (mailbox->head == NULL) {
        mailbox->tail = NULL;
    }
    mailbox->count--;

    /* Copy message data */
    message->type_tag = node->type_tag;
    message->data = node->data;
    message->data_size = 0; /* Size not stored in MessageNode */
    message->sender_id = KAIN_ACTOR_ID_INVALID; /* Not tracked yet */

    free(node);

    /* Signal not full */
#ifdef _WIN32
    SetEvent(mailbox->not_full);
    LeaveCriticalSection(&mailbox->lock);
#else
    pthread_cond_signal(&mailbox->not_full);
    pthread_mutex_unlock(&mailbox->lock);
#endif

    return 0;
}

/*
 * Actor Try Receive (Non-blocking)
 */

int kain_actor_try_receive(
    KainActorMailbox* mailbox,
    KainActorMessage* message,
    KainDiagnostic* diag
) {
    if (mailbox == NULL || message == NULL) {
        return -1;
    }

#ifdef _WIN32
    EnterCriticalSection(&mailbox->lock);
#else
    pthread_mutex_lock(&mailbox->lock);
#endif

    /* Check if empty */
    if (mailbox->head == NULL) {
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
#else
        pthread_mutex_unlock(&mailbox->lock);
#endif
        return 1; /* Empty */
    }

    /* Dequeue message */
    MessageNode* node = mailbox->head;
    mailbox->head = node->next;
    if (mailbox->head == NULL) {
        mailbox->tail = NULL;
    }
    mailbox->count--;

    /* Copy message data */
    message->type_tag = node->type_tag;
    message->data = node->data;
    message->data_size = 0;
    message->sender_id = KAIN_ACTOR_ID_INVALID;

    free(node);

#ifdef _WIN32
    SetEvent(mailbox->not_full);
    LeaveCriticalSection(&mailbox->lock);
#else
    pthread_cond_signal(&mailbox->not_full);
    pthread_mutex_unlock(&mailbox->lock);
#endif

    return 0;
}

/*
 * Actor Shutdown
 */

int kain_actor_shutdown(
    KainActorId actor_id,
    KainDiagnostic* diag
) {
    KainActorState_Internal* actor = kain_actor_table_get(actor_id);
    if (actor == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_NOT_FOUND;
            snprintf(diag->message, sizeof(diag->message), "Actor not found");
        }
        return -1;
    }

    /* Mark mailbox as closed */
    actor->mailbox.closed = 1;
    actor->state = KAIN_ACTOR_STATE_SHUTTING_DOWN;

    /* Signal mailbox to wake up waiting receive */
#ifdef _WIN32
    SetEvent(actor->mailbox.not_empty);
#else
    pthread_cond_broadcast(&actor->mailbox.not_empty);
#endif

    return 0;
}

/*
 * Actor Kill
 */

int kain_actor_kill(
    KainActorId actor_id,
    KainDiagnostic* diag
) {
    KainActorState_Internal* actor = kain_actor_table_get(actor_id);
    if (actor == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_NOT_FOUND;
            snprintf(diag->message, sizeof(diag->message), "Actor not found");
        }
        return -1;
    }

    actor->exit_reason = KAIN_ACTOR_EXIT_KILLED;
    actor->state = KAIN_ACTOR_STATE_FAILED;
    actor->mailbox.closed = 1;

    /* Only terminate thread if using thread-per-actor mode */
    if (!KAIN_SCHEDULER_USE_POOLED) {
#ifdef _WIN32
        if (actor->thread_handle != NULL) {
            TerminateThread(actor->thread_handle, 1);
            CloseHandle(actor->thread_handle);
        }
#else
        pthread_cancel(actor->thread);
#endif
    }

    kain_actor_cleanup(actor);
    kain_actor_table_remove(actor_id);

    return 0;
}

/*
 * Get Actor State
 */

KainActorState kain_actor_get_state(KainActorId actor_id) {
    KainActorState_Internal* actor = kain_actor_table_get(actor_id);
    if (actor == NULL) {
        return KAIN_ACTOR_STATE_UNINITIALIZED;
    }
    return actor->state;
}

/*
 * Actor Cleanup
 */

static void kain_actor_cleanup(KainActorState_Internal* actor) {
    if (actor == NULL) {
        return;
    }

    /* Destroy mailbox */
    kain_actor_mailbox_destroy(&actor->mailbox);

    /* Free monitors */
    KainActorMonitor* monitor = actor->monitors;
    while (monitor != NULL) {
        KainActorMonitor* next = monitor->next;
        free(monitor);
        monitor = next;
    }

    /* Free links */
    KainActorLink* link = actor->links;
    while (link != NULL) {
        KainActorLink* next = link->next;
        free(link);
        link = next;
    }
}

/*
 * Monitor Operations
 */

int kain_actor_monitor(
    KainActorId monitor_id,
    KainActorId monitored_id,
    KainDiagnostic* diag
) {
    KainActorState_Internal* monitor_actor = kain_actor_table_get(monitor_id);
    KainActorState_Internal* monitored_actor = kain_actor_table_get(monitored_id);

    if (monitor_actor == NULL || monitored_actor == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_NOT_FOUND;
            snprintf(diag->message, sizeof(diag->message), "Actor not found");
        }
        return -1;
    }

    /* Create monitor record */
    KainActorMonitor* monitor = (KainActorMonitor*)malloc(sizeof(KainActorMonitor));
    if (monitor == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_MONITOR_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Monitor allocation failed");
        }
        return -1;
    }

    monitor->monitor_id = monitor_id;
    monitor->monitored_id = monitored_id;
    monitor->next = monitor_actor->monitors;
    monitor_actor->monitors = monitor;

    return 0;
}

static void kain_actor_notify_monitors(KainActorState_Internal* actor) {
    /* Find all actors monitoring this one and send notifications */
    for (int i = 0; i < KAIN_ACTOR_TABLE_SIZE; i++) {
        KainActorState_Internal* other = g_actor_table.actors[i];
        if (other == NULL) continue;

        KainActorMonitor* monitor = other->monitors;
        while (monitor != NULL) {
            if (monitor->monitored_id == actor->actor_id) {
                /* Send exit notification message */
                KainActorMessage msg = {0};
                msg.type_tag = 0xDEAD; /* Special monitor notification tag */
                msg.sender_id = actor->actor_id;
                kain_actor_send(other->actor_id, &msg, NULL);
            }
            monitor = monitor->next;
        }
    }
}

/*
 * Link Operations
 */

int kain_actor_link(
    KainActorId actor_a,
    KainActorId actor_b,
    KainDiagnostic* diag
) {
    KainActorState_Internal* actor_a_state = kain_actor_table_get(actor_a);
    KainActorState_Internal* actor_b_state = kain_actor_table_get(actor_b);

    if (actor_a_state == NULL || actor_b_state == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_NOT_FOUND;
            snprintf(diag->message, sizeof(diag->message), "Actor not found");
        }
        return -1;
    }

    /* Create link record */
    KainActorLink* link = (KainActorLink*)malloc(sizeof(KainActorLink));
    if (link == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_LINK_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Link allocation failed");
        }
        return -1;
    }

    link->actor_a = actor_a;
    link->actor_b = actor_b;
    link->next = actor_a_state->links;
    actor_a_state->links = link;

    return 0;
}

int kain_actor_unlink(
    KainActorId actor_a,
    KainActorId actor_b,
    KainDiagnostic* diag
) {
    KainActorState_Internal* actor_a_state = kain_actor_table_get(actor_a);
    if (actor_a_state == NULL) {
        return -1;
    }

    /* Find and remove link */
    KainActorLink** link_ptr = &actor_a_state->links;
    while (*link_ptr != NULL) {
        KainActorLink* link = *link_ptr;
        if ((link->actor_a == actor_a && link->actor_b == actor_b) ||
            (link->actor_a == actor_b && link->actor_b == actor_a)) {
            *link_ptr = link->next;
            free(link);
            return 0;
        }
        link_ptr = &link->next;
    }

    return -1;
}

static void kain_actor_propagate_links(KainActorState_Internal* actor) {
    /* Terminate all linked actors */
    KainActorLink* link = actor->links;
    while (link != NULL) {
        KainActorId other_id = (link->actor_a == actor->actor_id) ? link->actor_b : link->actor_a;
        kain_actor_kill(other_id, NULL);
        link = link->next;
    }
}

/*
 * Actor Registry Operations
 */

static unsigned int kain_actor_registry_hash(const char* name) {
    unsigned int hash = 5381;
    int c;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % KAIN_ACTOR_REGISTRY_SIZE;
}

int kain_actor_registry_register(
    const char* name,
    KainActorId actor_id,
    KainDiagnostic* diag
) {
    if (name == NULL || actor_id == KAIN_ACTOR_ID_INVALID) {
        return -1;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_lock(&g_actor_registry.lock);
#endif

    unsigned int bucket = kain_actor_registry_hash(name);

    /* Check if name already exists */
    KainActorRegistryEntry* entry = g_actor_registry.buckets[bucket];
    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            if (diag != NULL) {
                kain_diagnostic_init(diag);
                diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
                diag->severity = KAIN_DIAG_SEVERITY_ERROR;
                diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
                snprintf(diag->message, sizeof(diag->message), "Actor name already registered");
            }
#ifdef _WIN32
            LeaveCriticalSection(&g_actor_registry.lock);
#else
            pthread_mutex_unlock(&g_actor_registry.lock);
#endif
            return -1;
        }
        entry = entry->next;
    }

    /* Create new entry */
    entry = (KainActorRegistryEntry*)malloc(sizeof(KainActorRegistryEntry));
    if (entry == NULL) {
#ifdef _WIN32
        LeaveCriticalSection(&g_actor_registry.lock);
#else
        pthread_mutex_unlock(&g_actor_registry.lock);
#endif
        return -1;
    }

    strncpy(entry->name, name, KAIN_ACTOR_NAME_MAX - 1);
    entry->name[KAIN_ACTOR_NAME_MAX - 1] = '\0';
    entry->actor_id = actor_id;
    entry->next = g_actor_registry.buckets[bucket];
    g_actor_registry.buckets[bucket] = entry;

#ifdef _WIN32
    LeaveCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_unlock(&g_actor_registry.lock);
#endif

    return 0;
}

KainActorId kain_actor_registry_lookup(const char* name) {
    if (name == NULL) {
        return KAIN_ACTOR_ID_INVALID;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_lock(&g_actor_registry.lock);
#endif

    unsigned int bucket = kain_actor_registry_hash(name);
    KainActorRegistryEntry* entry = g_actor_registry.buckets[bucket];

    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            KainActorId id = entry->actor_id;
#ifdef _WIN32
            LeaveCriticalSection(&g_actor_registry.lock);
#else
            pthread_mutex_unlock(&g_actor_registry.lock);
#endif
            return id;
        }
        entry = entry->next;
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_unlock(&g_actor_registry.lock);
#endif

    return KAIN_ACTOR_ID_INVALID;
}

int kain_actor_registry_unregister(
    const char* name,
    KainDiagnostic* diag
) {
    if (name == NULL) {
        return -1;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_lock(&g_actor_registry.lock);
#endif

    unsigned int bucket = kain_actor_registry_hash(name);
    KainActorRegistryEntry** entry_ptr = &g_actor_registry.buckets[bucket];

    while (*entry_ptr != NULL) {
        KainActorRegistryEntry* entry = *entry_ptr;
        if (strcmp(entry->name, name) == 0) {
            *entry_ptr = entry->next;
            free(entry);
#ifdef _WIN32
            LeaveCriticalSection(&g_actor_registry.lock);
#else
            pthread_mutex_unlock(&g_actor_registry.lock);
#endif
            return 0;
        }
        entry_ptr = &entry->next;
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_unlock(&g_actor_registry.lock);
#endif

    return -1;
}

/*
 * Mailbox Query Operations
 */

size_t kain_actor_mailbox_count(const KainActorMailbox* mailbox) {
    if (mailbox == NULL) {
        return 0;
    }
    return mailbox->count;
}

size_t kain_actor_mailbox_capacity(const KainActorMailbox* mailbox) {
    if (mailbox == NULL) {
        return 0;
    }
    return mailbox->capacity;
}

int kain_actor_mailbox_is_full(const KainActorMailbox* mailbox) {
    if (mailbox == NULL) {
        return 0;
    }
    if (mailbox->capacity == 0) {
        return 0; /* Unbounded */
    }
    return mailbox->count >= mailbox->capacity;
}

/*
 * Actor Scheduler Implementation
 *
 * Provides a work-stealing scheduler with a fixed pool of worker threads
 * to avoid unbounded thread creation.
 *
 * Requirements: 6.5, 6.6
 */

#ifdef _WIN32
static DWORD WINAPI kain_scheduler_worker_thread(LPVOID param) {
#else
static void* kain_scheduler_worker_thread(void* param) {
#endif
    (void)param;
    
    while (1) {
#ifdef _WIN32
        EnterCriticalSection(&g_scheduler.lock);
#else
        pthread_mutex_lock(&g_scheduler.lock);
#endif
        
        /* Wait for work or shutdown */
        while (g_scheduler.head == NULL && !g_scheduler.shutdown) {
#ifdef _WIN32
            LeaveCriticalSection(&g_scheduler.lock);
            WaitForSingleObject(g_scheduler.work_available, INFINITE);
            EnterCriticalSection(&g_scheduler.lock);
#else
            pthread_cond_wait(&g_scheduler.work_available, &g_scheduler.lock);
#endif
        }
        
        /* Check for shutdown */
        if (g_scheduler.shutdown && g_scheduler.head == NULL) {
#ifdef _WIN32
            LeaveCriticalSection(&g_scheduler.lock);
#else
            pthread_mutex_unlock(&g_scheduler.lock);
#endif
            break;
        }
        
        /* Dequeue actor */
        KainActorId actor_id = kain_scheduler_dequeue();
        
#ifdef _WIN32
        LeaveCriticalSection(&g_scheduler.lock);
#else
        pthread_mutex_unlock(&g_scheduler.lock);
#endif
        
        if (actor_id == KAIN_ACTOR_ID_INVALID) {
            continue;
        }
        
        /* Execute actor bootstrap */
        KainActorState_Internal* actor = kain_actor_table_get(actor_id);
        if (actor != NULL && actor->state == KAIN_ACTOR_STATE_INITIALIZING) {
            actor->state = KAIN_ACTOR_STATE_RUNNING;
            
            KainActorExitReason exit_reason = actor->bootstrap_fn(
                actor->actor_id,
                &actor->mailbox,
                actor->user_data
            );
            
            actor->exit_reason = exit_reason;
            actor->state = (exit_reason == KAIN_ACTOR_EXIT_NORMAL) 
                ? KAIN_ACTOR_STATE_TERMINATED 
                : KAIN_ACTOR_STATE_FAILED;
            
            kain_actor_notify_monitors(actor);
            if (exit_reason != KAIN_ACTOR_EXIT_NORMAL) {
                kain_actor_propagate_links(actor);
            }
        }
    }
    
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void kain_scheduler_init(void) {
#ifdef _WIN32
    InitializeCriticalSection(&g_scheduler.lock);
    g_scheduler.work_available = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_scheduler.worker_threads = (HANDLE*)malloc(sizeof(HANDLE) * KAIN_SCHEDULER_WORKER_COUNT);
    g_scheduler.worker_thread_ids = (DWORD*)malloc(sizeof(DWORD) * KAIN_SCHEDULER_WORKER_COUNT);
#else
    pthread_mutex_init(&g_scheduler.lock, NULL);
    pthread_cond_init(&g_scheduler.work_available, NULL);
    g_scheduler.worker_threads = (pthread_t*)malloc(sizeof(pthread_t) * KAIN_SCHEDULER_WORKER_COUNT);
#endif
    
    g_scheduler.head = NULL;
    g_scheduler.tail = NULL;
    g_scheduler.shutdown = 0;
    g_scheduler.active_workers = KAIN_SCHEDULER_WORKER_COUNT;
    
    /* Spawn worker threads */
    for (int i = 0; i < KAIN_SCHEDULER_WORKER_COUNT; i++) {
#ifdef _WIN32
        g_scheduler.worker_threads[i] = CreateThread(
            NULL, 0, kain_scheduler_worker_thread, NULL, 0, 
            &g_scheduler.worker_thread_ids[i]
        );
#else
        pthread_create(&g_scheduler.worker_threads[i], NULL, 
                      kain_scheduler_worker_thread, NULL);
#endif
    }
}

static void kain_scheduler_shutdown(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_lock(&g_scheduler.lock);
#endif
    
    g_scheduler.shutdown = 1;
    
#ifdef _WIN32
    /* Signal all workers */
    for (int i = 0; i < KAIN_SCHEDULER_WORKER_COUNT; i++) {
        SetEvent(g_scheduler.work_available);
    }
    LeaveCriticalSection(&g_scheduler.lock);
    
    /* Wait for workers to finish */
    WaitForMultipleObjects(KAIN_SCHEDULER_WORKER_COUNT, g_scheduler.worker_threads, TRUE, INFINITE);
    
    /* Cleanup */
    for (int i = 0; i < KAIN_SCHEDULER_WORKER_COUNT; i++) {
        CloseHandle(g_scheduler.worker_threads[i]);
    }
    free(g_scheduler.worker_threads);
    free(g_scheduler.worker_thread_ids);
    CloseHandle(g_scheduler.work_available);
    DeleteCriticalSection(&g_scheduler.lock);
#else
    pthread_cond_broadcast(&g_scheduler.work_available);
    pthread_mutex_unlock(&g_scheduler.lock);
    
    /* Wait for workers to finish */
    for (int i = 0; i < KAIN_SCHEDULER_WORKER_COUNT; i++) {
        pthread_join(g_scheduler.worker_threads[i], NULL);
    }
    
    /* Cleanup */
    free(g_scheduler.worker_threads);
    pthread_mutex_destroy(&g_scheduler.lock);
    pthread_cond_destroy(&g_scheduler.work_available);
#endif
    
    /* Free remaining queue nodes */
    KainActorSchedulerNode* node = g_scheduler.head;
    while (node != NULL) {
        KainActorSchedulerNode* next = node->next;
        free(node);
        node = next;
    }
}

static void kain_scheduler_enqueue(KainActorId actor_id) {
    KainActorSchedulerNode* node = (KainActorSchedulerNode*)malloc(sizeof(KainActorSchedulerNode));
    if (node == NULL) {
        return;
    }
    
    node->actor_id = actor_id;
    node->next = NULL;
    
#ifdef _WIN32
    EnterCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_lock(&g_scheduler.lock);
#endif
    
    if (g_scheduler.tail == NULL) {
        g_scheduler.head = node;
        g_scheduler.tail = node;
    } else {
        g_scheduler.tail->next = node;
        g_scheduler.tail = node;
    }
    
#ifdef _WIN32
    SetEvent(g_scheduler.work_available);
    LeaveCriticalSection(&g_scheduler.lock);
#else
    pthread_cond_signal(&g_scheduler.work_available);
    pthread_mutex_unlock(&g_scheduler.lock);
#endif
}

static KainActorId kain_scheduler_dequeue(void) {
    /* Caller must hold scheduler lock */
    if (g_scheduler.head == NULL) {
        return KAIN_ACTOR_ID_INVALID;
    }
    
    KainActorSchedulerNode* node = g_scheduler.head;
    KainActorId actor_id = node->actor_id;
    
    g_scheduler.head = node->next;
    if (g_scheduler.head == NULL) {
        g_scheduler.tail = NULL;
    }
    
    free(node);
    return actor_id;
}
