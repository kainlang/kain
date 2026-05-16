/*
 * KAIN Native Runtime Actor Implementation
 *
 * This file implements the actor runtime for the KAIN native lane.
 * It provides actor spawn, mailbox operations, supervision, monitoring,
 * registry, and scheduler integration.
 *
 *
 */

#include "../../include/actor.h"
#include "../../include/base.h"
#include "../../include/diagnostics.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <sched.h>
#endif

#if defined(_MSC_VER)
#define KAIN_THREAD_LOCAL __declspec(thread)
#else
#define KAIN_THREAD_LOCAL _Thread_local
#endif

/* Global actor table */
#define KAIN_ACTOR_TABLE_SIZE KAIN_ACTOR_TABLE_CAPACITY
#define KAIN_ACTOR_TABLE_WORD_BITS 64u
#define KAIN_ACTOR_TABLE_WORD_COUNT (KAIN_ACTOR_TABLE_SIZE / KAIN_ACTOR_TABLE_WORD_BITS)
#define KAIN_ACTOR_REPLY_PORT_WAIT_SPINS 256u
#define KAIN_ACTOR_MAILBOX_NODE_CACHE_LIMIT 1024u
#if (KAIN_ACTOR_TABLE_SIZE % KAIN_ACTOR_TABLE_WORD_BITS) != 0
#error "KAIN_ACTOR_TABLE_SIZE must be a multiple of 64 for occupancy-word indexing."
#endif

typedef struct {
    KainActorState_Internal* actors[KAIN_ACTOR_TABLE_SIZE];
    unsigned int generations[KAIN_ACTOR_TABLE_SIZE];
    uint64_t occupancy_words[KAIN_ACTOR_TABLE_WORD_COUNT];
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
#define KAIN_ACTOR_REGISTRY_SIZE KAIN_ACTOR_REGISTRY_CAPACITY

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
#define KAIN_SCHEDULER_WORKER_COUNT KAIN_ACTOR_SCHEDULER_WORKER_COUNT
#define KAIN_SCHEDULER_USE_POOLED 1  /* 1 = use worker pool, 0 = thread-per-actor */
#define KAIN_SCHEDULER_QUEUE_CAPACITY KAIN_ACTOR_TABLE_CAPACITY
#define KAIN_SCHEDULER_QUEUE_MASK (KAIN_SCHEDULER_QUEUE_CAPACITY - 1u)
#if (KAIN_SCHEDULER_QUEUE_CAPACITY & KAIN_SCHEDULER_QUEUE_MASK) != 0
#error "KAIN_SCHEDULER_QUEUE_CAPACITY must be a power of two for masked ring indexing."
#endif

typedef struct {
    KainActorId queue[KAIN_SCHEDULER_QUEUE_CAPACITY];
    size_t enqueue_cursor;
    size_t dequeue_cursor;
    int shutdown;
    int active_workers;
    size_t busy_workers;
    size_t max_busy_workers;
    size_t queue_depth;
    size_t max_queue_depth;
    size_t total_enqueued;
    size_t total_dequeued;
    size_t overflow_thread_spawns;

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
static unsigned long long g_actor_spawn_sequence = 1;
#define KAIN_ACTOR_REPLY_PORT_INLINE_PAYLOAD_CAPACITY 24u

typedef struct {
    KainActorRef actor_ref;
    int completed;
    void* reply_data;
    size_t reply_size;
    int references;
    unsigned char inline_reply_data[KAIN_ACTOR_REPLY_PORT_INLINE_PAYLOAD_CAPACITY];
#ifdef _WIN32
    CRITICAL_SECTION lock;
    HANDLE completed_event;
#else
    pthread_mutex_t lock;
    pthread_cond_t completed_cond;
#endif
} KainActorReplyPortState;

static KAIN_THREAD_LOCAL KainActorReplyPortState* g_actor_reply_port_tls = NULL;

/* Forward declarations */
static void kain_actor_cleanup(KainActorState_Internal* actor);
static void kain_actor_notify_monitors(KainActorState_Internal* actor);
static void kain_actor_propagate_links(KainActorState_Internal* actor);
static unsigned int kain_actor_registry_hash(const char* name);
static void kain_scheduler_init(void);
static void kain_scheduler_shutdown(void);
static void kain_scheduler_ready_actor(KainActorState_Internal* actor);
static KainActorId kain_scheduler_dequeue(void);
static void kain_scheduler_finish_turn(KainActorState_Internal* actor);
static int kain_actor_spawn_direct_thread(KainActorState_Internal* actor, KainDiagnostic* diag);
static void kain_actor_join_direct_thread(KainActorState_Internal* actor);
static KainActorId kain_actor_spawn_internal(
    const KainActorSpawnConfig* config,
    int force_direct_thread,
    KainDiagnostic* diag
);
static void kain_actor_handle_child_exit(KainActorState_Internal* child);
static int kain_actor_should_restart(KainActorState_Internal* child);
static KainActorId kain_actor_restart_child(KainActorState_Internal* child, KainDiagnostic* diag);
static void kain_actor_escalate_to_supervisor(KainActorState_Internal* child);
static void kain_actor_close_mailbox(KainActorState_Internal* actor);
static void kain_actor_registry_clear(void);
static void kain_actor_runtime_ensure_initialized(void);
static void kain_actor_finalize_exit_state(
    KainActorState_Internal* actor,
    KainActorExitReason bootstrap_exit_reason
);
static void kain_actor_complete_exit_side_effects(KainActorState_Internal* actor);
static void kain_actor_copy_name(char* dest, size_t dest_size, const char* src);
static int kain_actor_mailbox_init(KainActorMailbox* mailbox, size_t capacity);
static void kain_actor_mailbox_destroy(KainActorMailbox* mailbox);
static MessageNode* kain_actor_mailbox_take_cached_node(KainActorMailbox* mailbox);
static void kain_actor_mailbox_cache_or_free_node(KainActorMailbox* mailbox, MessageNode* node);
static int kain_actor_mailbox_append_copied_locked(
    KainActorMailbox* mailbox,
    const KainActorMessage* message
);
static int kain_actor_is_microcell_turn_actor(const KainActorState_Internal* actor);
static void kain_actor_scheduler_busy_worker_begin(void);
static void kain_actor_scheduler_busy_worker_end(void);
static void kain_actor_execute_microcell_turn(
    KainActorState_Internal* actor,
    int count_busy_worker
);
static void kain_actor_ref_zero(KainActorRef* actor_ref);
static void kain_actor_ref_write_live(
    KainActorRef* actor_ref,
    const KainActorState_Internal* actor
);
static int kain_actor_ref_same_identity(const KainActorRef* left, const KainActorRef* right);
static void kain_actor_reply_port_state_acquire(KainActorReplyPortState* state);
static void kain_actor_reply_port_state_release(KainActorReplyPortState* state);
static void kain_actor_reply_port_state_reset(KainActorReplyPortState* state);
static void kain_actor_reply_port_state_reset_with_actor_ref(
    KainActorReplyPortState* state,
    const KainActorRef* actor_ref
);
static int kain_actor_reply_port_state_try_copy_completed(
    KainActorReplyPortState* state,
    void* out_reply_data,
    size_t out_reply_capacity,
    size_t* out_reply_size
);
static int kain_actor_reply_port_state_complete_copied(
    KainActorReplyPortState* state,
    const KainActorRef* expected_actor_ref,
    const void* reply_data,
    size_t reply_size
);
static void kain_actor_reply_port_state_set_actor_ref(
    KainActorReplyPortState* state,
    const KainActorRef* actor_ref
);
static void kain_actor_reply_port_state_copy_actor_ref(
    KainActorReplyPortState* state,
    KainActorRef* out_actor_ref
);
static void kain_actor_reply_port_state_take_actor_ref(
    KainActorReplyPortState* state,
    KainActorRef* out_actor_ref
);
static int kain_actor_reply_port_state_bind_synthetic_actor(KainActorReplyPortState* state);
static int kain_actor_reply_port_state_rearm_synthetic_actor(KainActorReplyPortState* state);
static void kain_actor_reply_port_state_unbind_synthetic_actor(KainActorReplyPortState* state);
static KainActorState_Internal* kain_actor_table_get(KainActorId actor_id);
static KainActorId kain_actor_table_insert(KainActorState_Internal* actor);
static int kain_actor_table_copy_ref(KainActorId actor_id, KainActorRef* out_actor_ref);
static int kain_actor_table_ref_matches_locked(
    const KainActorRef* actor_ref,
    KainActorState_Internal** out_actor
);
static KainActorState_Internal* kain_actor_table_remove_ref(
    const KainActorRef* actor_ref
);

static uint64_t kain_actor_isolate_low_bit_u64(uint64_t value) {
    return value & (0u - value);
}

static unsigned int kain_actor_low_bit_index_u64(uint64_t one_hot) {
    static const unsigned char debruijn_index[64] = {
        0, 1, 48, 2, 57, 49, 28, 3,
        61, 58, 50, 42, 38, 29, 17, 4,
        62, 55, 59, 36, 53, 51, 43, 22,
        45, 39, 33, 30, 24, 18, 12, 5,
        63, 47, 56, 27, 60, 41, 37, 16,
        54, 35, 52, 21, 44, 32, 23, 11,
        46, 26, 40, 15, 34, 20, 31, 10,
        25, 14, 19, 9, 13, 8, 7, 6
    };
    return debruijn_index[(one_hot * 0x03f79d71b4cb0a89ULL) >> 58u];
}

static void kain_actor_reply_port_spin_pause(unsigned int spin_index) {
#ifdef _WIN32
    if ((spin_index & 63u) == 63u) {
        SwitchToThread();
    } else {
        YieldProcessor();
    }
#else
    if ((spin_index & 63u) == 63u) {
        sched_yield();
    } else {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ __volatile__("pause" ::: "memory");
#endif
    }
#endif
}

static void kain_actor_ref_zero(KainActorRef* actor_ref) {
    if (actor_ref == NULL) {
        return;
    }
    actor_ref->actor_id = KAIN_ACTOR_ID_INVALID;
    actor_ref->generation = 0u;
    actor_ref->execution_class = KAIN_ACTOR_EXECUTION_CLASS_INVALID;
    actor_ref->locality_class = KAIN_ACTOR_LOCALITY_INVALID;
}

static void kain_actor_ref_write_live(
    KainActorRef* actor_ref,
    const KainActorState_Internal* actor
) {
    if (actor_ref == NULL) {
        return;
    }
    if (actor == NULL) {
        kain_actor_ref_zero(actor_ref);
        return;
    }
    actor_ref->actor_id = actor->actor_id;
    actor_ref->generation = actor->ref_generation;
    actor_ref->execution_class = actor->execution_class;
    actor_ref->locality_class = actor->locality_class;
}

static int kain_actor_ref_same_identity(const KainActorRef* left, const KainActorRef* right) {
    return left != NULL
        && right != NULL
        && left->actor_id == right->actor_id
        && left->generation == right->generation
        && left->execution_class == right->execution_class
        && left->locality_class == right->locality_class;
}

static void kain_actor_reply_port_state_acquire(KainActorReplyPortState* state) {
    if (state == NULL) {
        return;
    }
#ifdef _WIN32
    EnterCriticalSection(&state->lock);
    state->references++;
    LeaveCriticalSection(&state->lock);
#else
    pthread_mutex_lock(&state->lock);
    state->references++;
    pthread_mutex_unlock(&state->lock);
#endif
}

static void kain_actor_reply_port_state_set_actor_ref(
    KainActorReplyPortState* state,
    const KainActorRef* actor_ref
) {
    if (state == NULL) {
        return;
    }
#ifdef _WIN32
    EnterCriticalSection(&state->lock);
    if (actor_ref != NULL) {
        state->actor_ref = *actor_ref;
    } else {
        kain_actor_ref_zero(&state->actor_ref);
    }
    LeaveCriticalSection(&state->lock);
#else
    pthread_mutex_lock(&state->lock);
    if (actor_ref != NULL) {
        state->actor_ref = *actor_ref;
    } else {
        kain_actor_ref_zero(&state->actor_ref);
    }
    pthread_mutex_unlock(&state->lock);
#endif
}

static void kain_actor_reply_port_state_copy_actor_ref(
    KainActorReplyPortState* state,
    KainActorRef* out_actor_ref
) {
    if (out_actor_ref == NULL) {
        return;
    }
    kain_actor_ref_zero(out_actor_ref);
    if (state == NULL) {
        return;
    }
#ifdef _WIN32
    EnterCriticalSection(&state->lock);
    *out_actor_ref = state->actor_ref;
    LeaveCriticalSection(&state->lock);
#else
    pthread_mutex_lock(&state->lock);
    *out_actor_ref = state->actor_ref;
    pthread_mutex_unlock(&state->lock);
#endif
}

static void kain_actor_reply_port_state_take_actor_ref(
    KainActorReplyPortState* state,
    KainActorRef* out_actor_ref
) {
    if (out_actor_ref == NULL) {
        return;
    }
    kain_actor_ref_zero(out_actor_ref);
    if (state == NULL) {
        return;
    }
#ifdef _WIN32
    EnterCriticalSection(&state->lock);
    *out_actor_ref = state->actor_ref;
    kain_actor_ref_zero(&state->actor_ref);
    LeaveCriticalSection(&state->lock);
#else
    pthread_mutex_lock(&state->lock);
    *out_actor_ref = state->actor_ref;
    kain_actor_ref_zero(&state->actor_ref);
    pthread_mutex_unlock(&state->lock);
#endif
}

static int kain_actor_reply_port_state_uses_inline_payload(
    const KainActorReplyPortState* state,
    const void* reply_data
) {
    return state != NULL
        && reply_data != NULL
        && reply_data == (const void*)state->inline_reply_data;
}

static int kain_actor_reply_port_state_complete_copied(
    KainActorReplyPortState* state,
    const KainActorRef* expected_actor_ref,
    const void* reply_data,
    size_t reply_size
) {
    void* heap_copy = NULL;
    int result = -1;

    if (state == NULL) {
        return -1;
    }
    if (reply_size > 0u && reply_data == NULL) {
        return -1;
    }
    if (reply_size > KAIN_ACTOR_REPLY_PORT_INLINE_PAYLOAD_CAPACITY) {
        heap_copy = malloc(reply_size);
        if (heap_copy == NULL) {
            return -1;
        }
        memcpy(heap_copy, reply_data, reply_size);
    }
#ifdef _WIN32
    EnterCriticalSection(&state->lock);
    if (expected_actor_ref == NULL || kain_actor_ref_same_identity(&state->actor_ref, expected_actor_ref)) {
        result = 0;
    }
    if (result == 0 && !state->completed) {
        state->completed = 1;
        if (reply_size == 0u) {
            state->reply_data = NULL;
            state->reply_size = 0u;
        } else if (heap_copy != NULL) {
            state->reply_data = heap_copy;
            state->reply_size = reply_size;
            heap_copy = NULL;
        } else {
            memcpy(state->inline_reply_data, reply_data, reply_size);
            state->reply_data = state->inline_reply_data;
            state->reply_size = reply_size;
        }
        SetEvent(state->completed_event);
    }
    LeaveCriticalSection(&state->lock);
#else
    pthread_mutex_lock(&state->lock);
    if (expected_actor_ref == NULL || kain_actor_ref_same_identity(&state->actor_ref, expected_actor_ref)) {
        result = 0;
    }
    if (result == 0 && !state->completed) {
        state->completed = 1;
        if (reply_size == 0u) {
            state->reply_data = NULL;
            state->reply_size = 0u;
        } else if (heap_copy != NULL) {
            state->reply_data = heap_copy;
            state->reply_size = reply_size;
            heap_copy = NULL;
        } else {
            memcpy(state->inline_reply_data, reply_data, reply_size);
            state->reply_data = state->inline_reply_data;
            state->reply_size = reply_size;
        }
        pthread_cond_signal(&state->completed_cond);
    }
    pthread_mutex_unlock(&state->lock);
#endif
    if (heap_copy != NULL) {
        free(heap_copy);
    }
    return result;
}

static void kain_actor_reply_port_state_reset_with_actor_ref(
    KainActorReplyPortState* state,
    const KainActorRef* actor_ref
) {
    void* reply_data_to_free = NULL;

    if (state == NULL) {
        return;
    }
#ifdef _WIN32
    EnterCriticalSection(&state->lock);
    reply_data_to_free = state->reply_data;
    state->completed = 0;
    state->reply_data = NULL;
    state->reply_size = 0u;
    memset(state->inline_reply_data, 0, sizeof(state->inline_reply_data));
    if (actor_ref != NULL) {
        state->actor_ref = *actor_ref;
    }
    ResetEvent(state->completed_event);
    LeaveCriticalSection(&state->lock);
#else
    pthread_mutex_lock(&state->lock);
    reply_data_to_free = state->reply_data;
    state->completed = 0;
    state->reply_data = NULL;
    state->reply_size = 0u;
    memset(state->inline_reply_data, 0, sizeof(state->inline_reply_data));
    if (actor_ref != NULL) {
        state->actor_ref = *actor_ref;
    }
    pthread_mutex_unlock(&state->lock);
#endif
    if (
        reply_data_to_free != NULL
        && !kain_actor_reply_port_state_uses_inline_payload(state, reply_data_to_free)
    ) {
        free(reply_data_to_free);
    }
}

static void kain_actor_reply_port_state_reset(KainActorReplyPortState* state) {
    kain_actor_reply_port_state_reset_with_actor_ref(state, NULL);
}

static int kain_actor_reply_port_state_try_copy_completed(
    KainActorReplyPortState* state,
    void* out_reply_data,
    size_t out_reply_capacity,
    size_t* out_reply_size
) {
    int completed = 0;

    if (state == NULL) {
        return 0;
    }

#ifdef _WIN32
    EnterCriticalSection(&state->lock);
    if (state->completed) {
        completed = 1;
        if (out_reply_size != NULL) {
            *out_reply_size = state->reply_size;
        }
        if (out_reply_data != NULL && out_reply_capacity > 0u && state->reply_data != NULL) {
            size_t bytes_to_copy = state->reply_size < out_reply_capacity
                ? state->reply_size
                : out_reply_capacity;
            if (bytes_to_copy > 0u) {
                memcpy(out_reply_data, state->reply_data, bytes_to_copy);
            }
        }
    }
    LeaveCriticalSection(&state->lock);
#else
    pthread_mutex_lock(&state->lock);
    if (state->completed) {
        completed = 1;
        if (out_reply_size != NULL) {
            *out_reply_size = state->reply_size;
        }
        if (out_reply_data != NULL && out_reply_capacity > 0u && state->reply_data != NULL) {
            size_t bytes_to_copy = state->reply_size < out_reply_capacity
                ? state->reply_size
                : out_reply_capacity;
            if (bytes_to_copy > 0u) {
                memcpy(out_reply_data, state->reply_data, bytes_to_copy);
            }
        }
    }
    pthread_mutex_unlock(&state->lock);
#endif

    return completed;
}

static int kain_actor_reply_port_state_wait(
    KainActorReplyPortState* state,
    long long timeout_ms,
    void* out_reply_data,
    size_t out_reply_capacity,
    size_t* out_reply_size
) {
    unsigned int spin_index;
    int completed = 0;

    if (state == NULL) {
        return 0;
    }
    if (out_reply_size != NULL) {
        *out_reply_size = 0u;
    }

    if (kain_actor_reply_port_state_try_copy_completed(
            state,
            out_reply_data,
            out_reply_capacity,
            out_reply_size
        )) {
        return 1;
    }

    if (timeout_ms != 0) {
        for (spin_index = 0u; spin_index < KAIN_ACTOR_REPLY_PORT_WAIT_SPINS; ++spin_index) {
            kain_actor_reply_port_spin_pause(spin_index);
            if (kain_actor_reply_port_state_try_copy_completed(
                    state,
                    out_reply_data,
                    out_reply_capacity,
                    out_reply_size
                )) {
                return 1;
            }
        }
    }

#ifdef _WIN32
    {
        DWORD wait_millis = timeout_ms < 0
            ? 0u
            : (timeout_ms > 0xFFFFFFFFLL ? 0xFFFFFFFFu : (DWORD)timeout_ms);
        DWORD wait_result = WaitForSingleObject(state->completed_event, wait_millis);
        if (wait_result == WAIT_OBJECT_0) {
            completed = kain_actor_reply_port_state_try_copy_completed(
                state,
                out_reply_data,
                out_reply_capacity,
                out_reply_size
            );
        }
    }
#else
    pthread_mutex_lock(&state->lock);
    if (!state->completed) {
        struct timespec deadline;
        int wait_status;

        clock_gettime(CLOCK_REALTIME, &deadline);
        if (timeout_ms < 0) {
            timeout_ms = 0;
        }
        deadline.tv_sec += (time_t)(timeout_ms / 1000LL);
        deadline.tv_nsec += (long)((timeout_ms % 1000LL) * 1000000LL);
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000L;
        }

        wait_status = 0;
        while (!state->completed && wait_status == 0) {
            wait_status = pthread_cond_timedwait(
                &state->completed_cond,
                &state->lock,
                &deadline
            );
        }
    }

    if (state->completed) {
        completed = 1;
        if (out_reply_size != NULL) {
            *out_reply_size = state->reply_size;
        }
        if (out_reply_data != NULL && out_reply_capacity > 0u && state->reply_data != NULL) {
            size_t bytes_to_copy = state->reply_size < out_reply_capacity
                ? state->reply_size
                : out_reply_capacity;
            if (bytes_to_copy > 0u) {
                memcpy(out_reply_data, state->reply_data, bytes_to_copy);
            }
        }
    }
    pthread_mutex_unlock(&state->lock);
#endif

    return completed;
}

static void kain_actor_reply_port_state_release(KainActorReplyPortState* state) {
    int should_destroy = 0;
    void* reply_data_to_free = NULL;

    if (state == NULL) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&state->lock);
    if (state->references > 0) {
        state->references--;
        should_destroy = state->references == 0;
        if (should_destroy) {
            reply_data_to_free = state->reply_data;
            state->reply_data = NULL;
            state->reply_size = 0u;
        }
    }
    LeaveCriticalSection(&state->lock);
    if (should_destroy) {
        CloseHandle(state->completed_event);
        DeleteCriticalSection(&state->lock);
        if (
            reply_data_to_free != NULL
            && !kain_actor_reply_port_state_uses_inline_payload(state, reply_data_to_free)
        ) {
            free(reply_data_to_free);
        }
        free(state);
    }
#else
    pthread_mutex_lock(&state->lock);
    if (state->references > 0) {
        state->references--;
        should_destroy = state->references == 0;
        if (should_destroy) {
            reply_data_to_free = state->reply_data;
            state->reply_data = NULL;
            state->reply_size = 0u;
        }
    }
    pthread_mutex_unlock(&state->lock);
    if (should_destroy) {
        pthread_cond_destroy(&state->completed_cond);
        pthread_mutex_destroy(&state->lock);
        if (
            reply_data_to_free != NULL
            && !kain_actor_reply_port_state_uses_inline_payload(state, reply_data_to_free)
        ) {
            free(reply_data_to_free);
        }
        free(state);
    }
#endif
}

static int kain_actor_reply_port_state_bind_synthetic_actor(KainActorReplyPortState* state) {
    KainActorState_Internal* actor;
    KainActorRef actor_ref;
    KainActorId actor_id;

    if (state == NULL) {
        return -1;
    }

    actor = (KainActorState_Internal*)calloc(1, sizeof(KainActorState_Internal));
    if (actor == NULL) {
        return -1;
    }

    actor->state = KAIN_ACTOR_STATE_RUNNING;
    actor->exit_reason = KAIN_ACTOR_EXIT_NORMAL;
    actor->execution_class = KAIN_ACTOR_EXECUTION_CLASS_SYNTHETIC_REPLY_PORT;
    actor->locality_class = KAIN_ACTOR_LOCALITY_LOCAL;
    actor->user_data = state;
    actor->supervisor.supervisor_id = KAIN_ACTOR_ID_INVALID;
    actor->spawn_config.mailbox_capacity = 1u;
    kain_actor_copy_name(actor->name, sizeof(actor->name), "kain-reply-port");
    kain_actor_copy_name(
        actor->spawn_config.name,
        sizeof(actor->spawn_config.name),
        "kain-reply-port"
    );

    if (kain_actor_mailbox_init(&actor->mailbox, 1u) != 0) {
        free(actor);
        return -1;
    }

    kain_actor_reply_port_state_acquire(state);
    actor_id = kain_actor_table_insert(actor);
    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        kain_actor_reply_port_state_release(state);
        kain_actor_mailbox_destroy(&actor->mailbox);
        free(actor);
        return -1;
    }

    kain_actor_ref_write_live(&actor_ref, actor);
    kain_actor_reply_port_state_set_actor_ref(state, &actor_ref);
    return 0;
}

static int kain_actor_reply_port_state_rearm_synthetic_actor(KainActorReplyPortState* state) {
    KainActorRef actor_ref;
    KainActorRef next_ref;
    KainActorState_Internal* actor = NULL;
    unsigned int next_generation;
    int rebound_existing_actor = 0;

    if (state == NULL) {
        return -1;
    }

    kain_actor_reply_port_state_copy_actor_ref(state, &actor_ref);
    if (actor_ref.actor_id == KAIN_ACTOR_ID_INVALID) {
        kain_actor_reply_port_state_reset(state);
        return kain_actor_reply_port_state_bind_synthetic_actor(state);
    }

    kain_actor_ref_zero(&next_ref);
#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif
    if (kain_actor_table_ref_matches_locked(&actor_ref, &actor) &&
        actor != NULL &&
        actor->execution_class == KAIN_ACTOR_EXECUTION_CLASS_SYNTHETIC_REPLY_PORT &&
        actor->user_data == state) {
        next_generation = g_actor_table.generations[actor_ref.actor_id] + 1u;
        if (next_generation == 0u) {
            next_generation = 1u;
        }
        g_actor_table.generations[actor_ref.actor_id] = next_generation;
        actor->ref_generation = next_generation;
        actor->state = KAIN_ACTOR_STATE_RUNNING;
        actor->exit_reason = KAIN_ACTOR_EXIT_NORMAL;
        kain_actor_ref_write_live(&next_ref, actor);
        rebound_existing_actor = 1;
    }
#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif

    if (rebound_existing_actor) {
        kain_actor_reply_port_state_reset_with_actor_ref(state, &next_ref);
        return 0;
    }

    kain_actor_reply_port_state_take_actor_ref(state, &actor_ref);
    kain_actor_reply_port_state_reset(state);
    return kain_actor_reply_port_state_bind_synthetic_actor(state);
}

static void kain_actor_reply_port_state_unbind_synthetic_actor(KainActorReplyPortState* state) {
    KainActorRef actor_ref;
    KainActorState_Internal* actor;

    if (state == NULL) {
        return;
    }

    kain_actor_reply_port_state_take_actor_ref(state, &actor_ref);
    if (actor_ref.actor_id == KAIN_ACTOR_ID_INVALID) {
        return;
    }

    actor = kain_actor_table_remove_ref(&actor_ref);
    if (actor == NULL) {
        return;
    }

    actor->state = KAIN_ACTOR_STATE_TERMINATED;
    actor->exit_reason = KAIN_ACTOR_EXIT_SHUTDOWN;
    kain_actor_close_mailbox(actor);
    kain_actor_cleanup(actor);
    free(actor);
}

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
    memset(g_actor_table.generations, 0, sizeof(g_actor_table.generations));
    memset(g_actor_table.occupancy_words, 0, sizeof(g_actor_table.occupancy_words));
    g_actor_table.occupancy_words[0] = 1ULL;
    memset(g_actor_registry.buckets, 0, sizeof(g_actor_registry.buckets));
    g_actor_spawn_sequence = 1;

    /* Initialize scheduler if using pooled mode */
    if (KAIN_SCHEDULER_USE_POOLED) {
        kain_scheduler_init();
    }

    g_actor_runtime_initialized = 1;
}

static void kain_actor_runtime_ensure_initialized(void) {
    if (!g_actor_runtime_initialized) {
        kain_actor_runtime_init();
    }
}

KainActorAbiDescriptor kain_actor_abi_descriptor(void) {
    KainActorAbiDescriptor descriptor;

    descriptor.abi_version = KAIN_ACTOR_ABI_VERSION;
    descriptor.actor_id_bits = (unsigned short)KAIN_ACTOR_ID_BITS;
    descriptor.actor_ref_generation_bits = (unsigned short)KAIN_ACTOR_REF_GENERATION_BITS;
    descriptor.invalid_actor_id = KAIN_ACTOR_ID_INVALID;
    descriptor.default_execution_class = KAIN_ACTOR_EXECUTION_CLASS_MICROCELL;
    descriptor.default_locality_class = KAIN_ACTOR_LOCALITY_LOCAL;
    descriptor.synthetic_reply_port_execution_class =
        KAIN_ACTOR_EXECUTION_CLASS_SYNTHETIC_REPLY_PORT;
    descriptor.synthetic_reply_port_locality_class = KAIN_ACTOR_LOCALITY_LOCAL;
    descriptor.default_mailbox_capacity = KAIN_MAILBOX_DEFAULT_CAPACITY;
    descriptor.unbounded_mailbox_capacity = KAIN_MAILBOX_UNBOUNDED_CAPACITY;
    descriptor.default_ask_timeout_ms = KAIN_ACTOR_DEFAULT_ASK_TIMEOUT_MS;
    descriptor.default_shutdown_grace_ms = KAIN_ACTOR_DEFAULT_SHUTDOWN_GRACE_MS;
    descriptor.supervision_max_restarts = KAIN_SUPERVISION_MAX_RESTARTS;
    descriptor.supervision_restart_window_millis = KAIN_SUPERVISION_RESTART_WINDOW_MILLIS;
    descriptor.actor_name_max = KAIN_ACTOR_NAME_MAX;
    descriptor.scheduler_worker_count = KAIN_ACTOR_SCHEDULER_WORKER_COUNT;
    descriptor.actor_table_capacity = KAIN_ACTOR_TABLE_CAPACITY;
    descriptor.registry_capacity = KAIN_ACTOR_REGISTRY_CAPACITY;
    descriptor.monitor_exit_tag_base = KAIN_ACTOR_MONITOR_EXIT_TAG_BASE;
    descriptor.default_microcell_turn_budget = KAIN_ACTOR_DEFAULT_MICROCELL_TURN_BUDGET;

    return descriptor;
}

int kain_actor_abi_descriptor_is_compatible(const KainActorAbiDescriptor* expected) {
    KainActorAbiDescriptor current;

    if (expected == NULL) {
        return 0;
    }

    current = kain_actor_abi_descriptor();
    return expected->abi_version == current.abi_version &&
           expected->actor_id_bits == current.actor_id_bits &&
           expected->actor_ref_generation_bits == current.actor_ref_generation_bits &&
           expected->invalid_actor_id == current.invalid_actor_id &&
           expected->default_execution_class == current.default_execution_class &&
           expected->default_locality_class == current.default_locality_class &&
           expected->synthetic_reply_port_execution_class ==
               current.synthetic_reply_port_execution_class &&
           expected->synthetic_reply_port_locality_class ==
               current.synthetic_reply_port_locality_class &&
           expected->default_mailbox_capacity == current.default_mailbox_capacity &&
           expected->unbounded_mailbox_capacity == current.unbounded_mailbox_capacity &&
           expected->default_ask_timeout_ms == current.default_ask_timeout_ms &&
           expected->default_shutdown_grace_ms == current.default_shutdown_grace_ms &&
           expected->supervision_max_restarts == current.supervision_max_restarts &&
           expected->supervision_restart_window_millis == current.supervision_restart_window_millis &&
           expected->actor_name_max == current.actor_name_max &&
           expected->scheduler_worker_count == current.scheduler_worker_count &&
           expected->actor_table_capacity == current.actor_table_capacity &&
           expected->registry_capacity == current.registry_capacity &&
           expected->monitor_exit_tag_base == current.monitor_exit_tag_base &&
           expected->default_microcell_turn_budget == current.default_microcell_turn_budget;
}

void kain_actor_ref_from_id(KainActorId actor_id, KainActorRef* out_ref) {
    kain_actor_runtime_ensure_initialized();
    (void)kain_actor_table_copy_ref(actor_id, out_ref);
}

int kain_actor_ref_is_live(const KainActorRef* actor_ref) {
    int is_live = 0;

    if (actor_ref == NULL) {
        return 0;
    }

    kain_actor_runtime_ensure_initialized();

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif
    is_live = kain_actor_table_ref_matches_locked(actor_ref, NULL);
#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif
    return is_live;
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

    if (g_actor_reply_port_tls != NULL) {
        KainActorReplyPortState* cached_reply_port = g_actor_reply_port_tls;
        g_actor_reply_port_tls = NULL;
        kain_actor_reply_port_state_unbind_synthetic_actor(cached_reply_port);
        kain_actor_reply_port_state_release(cached_reply_port);
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif

    for (int i = 0; i < KAIN_ACTOR_TABLE_SIZE; i++) {
        if (g_actor_table.actors[i] != NULL) {
            kain_actor_close_mailbox(g_actor_table.actors[i]);
            if (g_actor_table.actors[i]->state == KAIN_ACTOR_STATE_INITIALIZING ||
                g_actor_table.actors[i]->state == KAIN_ACTOR_STATE_RUNNING) {
                g_actor_table.actors[i]->state = KAIN_ACTOR_STATE_SHUTTING_DOWN;
            }
        }
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif

    if (KAIN_SCHEDULER_USE_POOLED) {
        kain_scheduler_shutdown();
    }

    for (int i = 0; i < KAIN_ACTOR_TABLE_SIZE; i++) {
        KainActorState_Internal* actor;
#ifdef _WIN32
        EnterCriticalSection(&g_actor_table.lock);
#else
        pthread_mutex_lock(&g_actor_table.lock);
#endif
        actor = g_actor_table.actors[i];
#ifdef _WIN32
        LeaveCriticalSection(&g_actor_table.lock);
#else
        pthread_mutex_unlock(&g_actor_table.lock);
#endif
        kain_actor_join_direct_thread(actor);
    }

    kain_actor_registry_clear();

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
    mailbox->free_nodes = NULL;
    mailbox->capacity = capacity;
    mailbox->count = 0;
    mailbox->free_node_count = 0;
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
    node = mailbox->free_nodes;
    while (node != NULL) {
        MessageNode* next = node->next;
        free(node);
        node = next;
    }
    mailbox->head = NULL;
    mailbox->tail = NULL;
    mailbox->free_nodes = NULL;
    mailbox->count = 0u;
    mailbox->free_node_count = 0u;

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

static MessageNode* kain_actor_mailbox_take_cached_node(KainActorMailbox* mailbox) {
    MessageNode* node;

    if (mailbox == NULL || mailbox->free_nodes == NULL) {
        return NULL;
    }
    node = mailbox->free_nodes;
    mailbox->free_nodes = node->next;
    mailbox->free_node_count--;
    node->next = NULL;
    return node;
}

static void kain_actor_mailbox_cache_or_free_node(KainActorMailbox* mailbox, MessageNode* node) {
    if (node == NULL) {
        return;
    }
    node->type_tag = 0;
    node->data = NULL;
    node->data_size = 0u;
    node->sender_id = KAIN_ACTOR_ID_INVALID;
    if (mailbox != NULL && mailbox->free_node_count < KAIN_ACTOR_MAILBOX_NODE_CACHE_LIMIT) {
        node->next = mailbox->free_nodes;
        mailbox->free_nodes = node;
        mailbox->free_node_count++;
        return;
    }
    node->next = NULL;
    free(node);
}

static int kain_actor_mailbox_append_copied_locked(
    KainActorMailbox* mailbox,
    const KainActorMessage* message
) {
    MessageNode* node;

    if (mailbox == NULL || message == NULL) {
        return -1;
    }
    if (mailbox->closed) {
        return -2;
    }
    if (mailbox->capacity > 0 && mailbox->count >= mailbox->capacity) {
        return -3;
    }

    node = kain_actor_mailbox_take_cached_node(mailbox);
    if (node == NULL) {
        node = (MessageNode*)malloc(sizeof(MessageNode));
    }
    if (node == NULL) {
        return -4;
    }

    node->type_tag = message->type_tag;
    node->sender_id = message->sender_id;
    node->data_size = message->data_size;
    node->next = NULL;

    if (message->data != NULL && message->data_size > 0) {
        node->data = malloc(message->data_size);
        if (node->data == NULL) {
            kain_actor_mailbox_cache_or_free_node(mailbox, node);
            return -4;
        }
        memcpy(node->data, message->data, message->data_size);
    } else {
        node->data = NULL;
    }

    if (mailbox->tail == NULL) {
        mailbox->head = node;
        mailbox->tail = node;
    } else {
        mailbox->tail->next = node;
        mailbox->tail = node;
    }
    mailbox->count++;
    return 0;
}

static int kain_actor_is_microcell_turn_actor(const KainActorState_Internal* actor) {
    return actor != NULL &&
           actor->entry_kind == KAIN_ACTOR_ENTRY_KIND_MICROCELL_TURN &&
           actor->turn_fn != NULL;
}

static void kain_actor_scheduler_busy_worker_begin(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_lock(&g_scheduler.lock);
#endif
    g_scheduler.busy_workers++;
    if (g_scheduler.busy_workers > g_scheduler.max_busy_workers) {
        g_scheduler.max_busy_workers = g_scheduler.busy_workers;
    }
#ifdef _WIN32
    LeaveCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_unlock(&g_scheduler.lock);
#endif
}

static void kain_actor_scheduler_busy_worker_end(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_lock(&g_scheduler.lock);
#endif
    if (g_scheduler.busy_workers > 0u) {
        g_scheduler.busy_workers--;
    }
#ifdef _WIN32
    LeaveCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_unlock(&g_scheduler.lock);
#endif
}

static void kain_actor_execute_microcell_turn(
    KainActorState_Internal* actor,
    int count_busy_worker
) {
    KainActorTurnStatus turn_status = KAIN_ACTOR_TURN_IDLE;

    if (actor == NULL) {
        return;
    }
    if (actor->state == KAIN_ACTOR_STATE_INITIALIZING) {
        actor->state = KAIN_ACTOR_STATE_RUNNING;
    }
    if (actor->state == KAIN_ACTOR_STATE_RUNNING) {
        if (count_busy_worker) {
            kain_actor_scheduler_busy_worker_begin();
        }

        turn_status = actor->turn_fn(
            actor->actor_id,
            &actor->mailbox,
            actor->user_data,
            actor->microcell_turn_budget
        );

        if (turn_status == KAIN_ACTOR_TURN_STOPPED) {
            kain_actor_finalize_exit_state(actor, KAIN_ACTOR_EXIT_NORMAL);
            kain_actor_complete_exit_side_effects(actor);
        } else if (turn_status == KAIN_ACTOR_TURN_CRASHED) {
            kain_actor_finalize_exit_state(actor, KAIN_ACTOR_EXIT_CRASHED);
            kain_actor_complete_exit_side_effects(actor);
        }

        if (count_busy_worker) {
            kain_actor_scheduler_busy_worker_end();
        }
    } else if (actor->state == KAIN_ACTOR_STATE_SHUTTING_DOWN ||
               actor->state == KAIN_ACTOR_STATE_FAILED) {
        kain_actor_finalize_exit_state(actor, actor->exit_reason);
        kain_actor_complete_exit_side_effects(actor);
    }
    kain_scheduler_finish_turn(actor);
}

static void kain_actor_close_mailbox(KainActorState_Internal* actor) {
    if (actor == NULL) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&actor->mailbox.lock);
#else
    pthread_mutex_lock(&actor->mailbox.lock);
#endif
    actor->mailbox.closed = 1;
#ifdef _WIN32
    SetEvent(actor->mailbox.not_empty);
    SetEvent(actor->mailbox.not_full);
    LeaveCriticalSection(&actor->mailbox.lock);
#else
    pthread_cond_broadcast(&actor->mailbox.not_empty);
    pthread_cond_broadcast(&actor->mailbox.not_full);
    pthread_mutex_unlock(&actor->mailbox.lock);
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

static int kain_actor_table_copy_ref(KainActorId actor_id, KainActorRef* out_actor_ref) {
    int found = 0;

    kain_actor_ref_zero(out_actor_ref);
    if (actor_id == KAIN_ACTOR_ID_INVALID || actor_id >= KAIN_ACTOR_TABLE_SIZE) {
        return 0;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif
    if (g_actor_table.actors[actor_id] != NULL) {
        kain_actor_ref_write_live(out_actor_ref, g_actor_table.actors[actor_id]);
        found = 1;
    }
#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif

    return found;
}

static int kain_actor_table_ref_matches_locked(
    const KainActorRef* actor_ref,
    KainActorState_Internal** out_actor
) {
    KainActorState_Internal* actor;

    if (out_actor != NULL) {
        *out_actor = NULL;
    }
    if (actor_ref == NULL ||
        actor_ref->actor_id == KAIN_ACTOR_ID_INVALID ||
        actor_ref->actor_id >= KAIN_ACTOR_TABLE_SIZE) {
        return 0;
    }

    actor = g_actor_table.actors[actor_ref->actor_id];
    if (actor == NULL) {
        return 0;
    }
    if (g_actor_table.generations[actor_ref->actor_id] != actor_ref->generation ||
        actor->ref_generation != actor_ref->generation ||
        actor->execution_class != actor_ref->execution_class ||
        actor->locality_class != actor_ref->locality_class) {
        return 0;
    }

    if (out_actor != NULL) {
        *out_actor = actor;
    }
    return 1;
}

static KainActorId kain_actor_table_insert(KainActorState_Internal* actor) {
    size_t word_index;
#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif

    KainActorId id = KAIN_ACTOR_ID_INVALID;

    /* Find first free slot with a reserved-invalid-slot bitset. */
    for (word_index = 0u; word_index < KAIN_ACTOR_TABLE_WORD_COUNT; ++word_index) {
        uint64_t free_mask = ~g_actor_table.occupancy_words[word_index];
        if (word_index == 0u) {
            free_mask &= ~1ULL;
        }
        if (free_mask != 0u) {
            uint64_t low_bit = kain_actor_isolate_low_bit_u64(free_mask);
            unsigned int bit_index = kain_actor_low_bit_index_u64(low_bit);
            id = (KainActorId)(word_index * KAIN_ACTOR_TABLE_WORD_BITS + bit_index);
            if (id != KAIN_ACTOR_ID_INVALID && id < KAIN_ACTOR_TABLE_SIZE) {
                unsigned int next_generation = g_actor_table.generations[id] + 1u;
                if (next_generation == 0u) {
                    next_generation = 1u;
                }
                actor->actor_id = id;
                actor->ref_generation = next_generation;
                if (actor->execution_class == KAIN_ACTOR_EXECUTION_CLASS_INVALID) {
                    actor->execution_class = KAIN_ACTOR_EXECUTION_CLASS_MICROCELL;
                }
                if (actor->locality_class == KAIN_ACTOR_LOCALITY_INVALID) {
                    actor->locality_class = KAIN_ACTOR_LOCALITY_LOCAL;
                }
                actor->spawn_sequence = g_actor_spawn_sequence++;
                g_actor_table.actors[id] = actor;
                g_actor_table.generations[id] = next_generation;
                g_actor_table.occupancy_words[word_index] |= low_bit;
            } else {
                id = KAIN_ACTOR_ID_INVALID;
            }
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
    size_t word_index;
    uint64_t bit_mask;
    if (actor_id == KAIN_ACTOR_ID_INVALID || actor_id >= KAIN_ACTOR_TABLE_SIZE) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif

    g_actor_table.actors[actor_id] = NULL;
    word_index = (size_t)(actor_id / KAIN_ACTOR_TABLE_WORD_BITS);
    bit_mask = 1ULL << (unsigned int)(actor_id % KAIN_ACTOR_TABLE_WORD_BITS);
    g_actor_table.occupancy_words[word_index] &= ~bit_mask;

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

    if (actor->state == KAIN_ACTOR_STATE_SHUTTING_DOWN ||
        actor->state == KAIN_ACTOR_STATE_FAILED) {
        kain_actor_finalize_exit_state(actor, actor->exit_reason);
        kain_actor_complete_exit_side_effects(actor);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    /* Update state to running */
    actor->state = KAIN_ACTOR_STATE_RUNNING;

    /* Call bootstrap function */
    KainActorExitReason exit_reason = actor->bootstrap_fn(
        actor->actor_id,
        &actor->mailbox,
        actor->user_data
    );

    kain_actor_finalize_exit_state(actor, exit_reason);
    kain_actor_complete_exit_side_effects(actor);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static KainActorState_Internal* kain_actor_table_remove_ref(const KainActorRef* actor_ref) {
    KainActorState_Internal* actor = NULL;
    size_t word_index;
    uint64_t bit_mask;

    if (actor_ref == NULL ||
        actor_ref->actor_id == KAIN_ACTOR_ID_INVALID ||
        actor_ref->actor_id >= KAIN_ACTOR_TABLE_SIZE) {
        return NULL;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif
    if (kain_actor_table_ref_matches_locked(actor_ref, &actor)) {
        g_actor_table.actors[actor_ref->actor_id] = NULL;
        word_index = (size_t)(actor_ref->actor_id / KAIN_ACTOR_TABLE_WORD_BITS);
        bit_mask = 1ULL << (unsigned int)(actor_ref->actor_id % KAIN_ACTOR_TABLE_WORD_BITS);
        g_actor_table.occupancy_words[word_index] &= ~bit_mask;
    } else {
        actor = NULL;
    }
#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif

    return actor;
}

static void kain_actor_finalize_exit_state(
    KainActorState_Internal* actor,
    KainActorExitReason bootstrap_exit_reason
) {
    KainActorExitReason exit_reason = bootstrap_exit_reason;

    if (actor == NULL) {
        return;
    }

    if (actor->mailbox.closed) {
        if (actor->exit_reason != KAIN_ACTOR_EXIT_NORMAL &&
            bootstrap_exit_reason == KAIN_ACTOR_EXIT_NORMAL) {
            exit_reason = actor->exit_reason;
        } else if (actor->state == KAIN_ACTOR_STATE_SHUTTING_DOWN &&
                   bootstrap_exit_reason == KAIN_ACTOR_EXIT_NORMAL) {
            exit_reason = KAIN_ACTOR_EXIT_SHUTDOWN;
        }
    }

    actor->exit_reason = exit_reason;
    actor->state = (exit_reason == KAIN_ACTOR_EXIT_NORMAL ||
                    exit_reason == KAIN_ACTOR_EXIT_SHUTDOWN)
        ? KAIN_ACTOR_STATE_TERMINATED
        : KAIN_ACTOR_STATE_FAILED;
}

static void kain_actor_complete_exit_side_effects(KainActorState_Internal* actor) {
    if (actor == NULL) {
        return;
    }

    if (actor->supervisor.supervisor_id != KAIN_ACTOR_ID_INVALID) {
        kain_actor_handle_child_exit(actor);
    }

    kain_actor_notify_monitors(actor);
    if (actor->exit_reason != KAIN_ACTOR_EXIT_NORMAL &&
        actor->exit_reason != KAIN_ACTOR_EXIT_SHUTDOWN) {
        kain_actor_propagate_links(actor);
    }
}

/*
 * Actor Spawn Configuration
 */

void kain_actor_spawn_config_init(KainActorSpawnConfig* config) {
    memset(config, 0, sizeof(KainActorSpawnConfig));
    config->mailbox_capacity = KAIN_MAILBOX_DEFAULT_CAPACITY;
    config->supervision_strategy = KAIN_SUPERVISION_STRATEGY_ONE_FOR_ONE;
    config->restart_policy = KAIN_RESTART_POLICY_TEMPORARY;
    config->supervisor_id = KAIN_ACTOR_ID_INVALID;
    config->retain_user_data = 0;
    config->entry_kind = KAIN_ACTOR_ENTRY_KIND_LEGACY_BOOTSTRAP;
    config->microcell_turn_budget = KAIN_ACTOR_DEFAULT_MICROCELL_TURN_BUDGET;
    config->execution_class = KAIN_ACTOR_EXECUTION_CLASS_MICROCELL;
    config->locality_class = KAIN_ACTOR_LOCALITY_LOCAL;
}

void* kain_actor_reply_port_new(void) {
    KainActorReplyPortState* state;

    kain_actor_runtime_ensure_initialized();

    if (g_actor_reply_port_tls != NULL) {
        state = g_actor_reply_port_tls;
        if (kain_actor_reply_port_state_rearm_synthetic_actor(state) != 0) {
            g_actor_reply_port_tls = NULL;
            kain_actor_reply_port_state_release(state);
            return NULL;
        }
        return state;
    }

    state = (KainActorReplyPortState*)calloc(1, sizeof(KainActorReplyPortState));
    if (state == NULL) {
        return NULL;
    }

    kain_actor_ref_zero(&state->actor_ref);
    state->completed = 0;
    state->reply_data = NULL;
    state->reply_size = 0u;
    state->references = 1;
    memset(state->inline_reply_data, 0, sizeof(state->inline_reply_data));
#ifdef _WIN32
    InitializeCriticalSection(&state->lock);
    state->completed_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (state->completed_event == NULL) {
        DeleteCriticalSection(&state->lock);
        free(state);
        return NULL;
    }
#else
    pthread_mutex_init(&state->lock, NULL);
    pthread_cond_init(&state->completed_cond, NULL);
#endif

    if (kain_actor_reply_port_state_bind_synthetic_actor(state) != 0) {
        kain_actor_reply_port_state_release(state);
        return NULL;
    }

    g_actor_reply_port_tls = state;
    return state;
}

KainActorId kain_actor_reply_port_actor_id(void* reply_port_handle) {
    KainActorReplyPortState* state = (KainActorReplyPortState*)reply_port_handle;
    KainActorRef actor_ref;

    if (state == NULL) {
        return KAIN_ACTOR_ID_INVALID;
    }
    kain_actor_reply_port_state_copy_actor_ref(state, &actor_ref);
    return actor_ref.actor_id;
}

void kain_actor_reply_port_actor_ref(void* reply_port_handle, KainActorRef* out_ref) {
    KainActorReplyPortState* state = (KainActorReplyPortState*)reply_port_handle;

    kain_actor_ref_zero(out_ref);
    if (state == NULL || out_ref == NULL) {
        return;
    }
    kain_actor_reply_port_state_copy_actor_ref(state, out_ref);
}

void kain_actor_reply_port_destroy(void* reply_port_handle) {
    KainActorReplyPortState* state = (KainActorReplyPortState*)reply_port_handle;

    if (state == NULL) {
        return;
    }
    if (g_actor_reply_port_tls == state) {
        kain_actor_reply_port_state_unbind_synthetic_actor(state);
        kain_actor_reply_port_state_reset(state);
        return;
    }
    kain_actor_reply_port_state_unbind_synthetic_actor(state);
    kain_actor_reply_port_state_reset(state);
    kain_actor_reply_port_state_release(state);
}

int kain_actor_reply_port_send(
    KainActorId reply_port_actor_id,
    const void* reply_data,
    size_t reply_size
) {
    KainActorRef actor_ref;

    kain_actor_ref_from_id(reply_port_actor_id, &actor_ref);
    if (actor_ref.actor_id == KAIN_ACTOR_ID_INVALID) {
        return -1;
    }
    return kain_actor_reply_port_send_ref(&actor_ref, reply_data, reply_size);
}

int kain_actor_reply_port_send_ref(
    const KainActorRef* reply_port_ref,
    const void* reply_data,
    size_t reply_size
) {
    KainActorState_Internal* actor = NULL;
    KainActorReplyPortState* state;
    int result;

    if (reply_port_ref == NULL) {
        return -1;
    }

    kain_actor_runtime_ensure_initialized();

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif
    if (!kain_actor_table_ref_matches_locked(reply_port_ref, &actor) ||
        actor == NULL ||
        actor->execution_class != KAIN_ACTOR_EXECUTION_CLASS_SYNTHETIC_REPLY_PORT) {
#ifdef _WIN32
        LeaveCriticalSection(&g_actor_table.lock);
#else
        pthread_mutex_unlock(&g_actor_table.lock);
#endif
        return -1;
    }

    state = (KainActorReplyPortState*)actor->user_data;
    if (state != NULL) {
        kain_actor_reply_port_state_acquire(state);
    }
#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif

    if (state == NULL) {
        return -1;
    }

    result = kain_actor_reply_port_state_complete_copied(state, reply_port_ref, reply_data, reply_size);
    kain_actor_reply_port_state_release(state);
    return result;
}

int kain_actor_reply_port_wait(
    void* reply_port_handle,
    long long timeout_ms,
    void* out_reply_data,
    size_t out_reply_capacity,
    size_t* out_reply_size
) {
    KainActorReplyPortState* state = (KainActorReplyPortState*)reply_port_handle;
    int completed;

    if (state == NULL) {
        if (out_reply_size != NULL) {
            *out_reply_size = 0u;
        }
        return -1;
    }

    completed = kain_actor_reply_port_state_wait(
        state,
        timeout_ms,
        out_reply_data,
        out_reply_capacity,
        out_reply_size
    );
    if (!completed) {
        if (g_actor_reply_port_tls == state) {
            kain_actor_reply_port_state_unbind_synthetic_actor(state);
            kain_actor_reply_port_state_reset(state);
            if (out_reply_size != NULL) {
                *out_reply_size = 0u;
            }
            return 1;
        }
        kain_actor_reply_port_state_unbind_synthetic_actor(state);
        kain_actor_reply_port_state_release(state);
        if (out_reply_size != NULL) {
            *out_reply_size = 0u;
        }
        return 1;
    }
    if (!completed && out_reply_size != NULL) {
        *out_reply_size = 0u;
    }
    return completed ? 0 : 1;
}

long long kain_actor_reply_port_wait_i64(void* reply_port_handle, long long timeout_ms) {
    long long reply_value = 0;
    size_t reply_size = 0u;

    if (kain_actor_reply_port_wait(
            reply_port_handle,
            timeout_ms,
            &reply_value,
            sizeof(reply_value),
            &reply_size
        ) != 0) {
        return 0;
    }
    return reply_value;
}

/*
 * Actor Spawn
 */

static KainActorId kain_actor_spawn_internal(
    const KainActorSpawnConfig* config,
    int force_direct_thread,
    KainDiagnostic* diag
) {
    KainActorEntryKind entry_kind;
    unsigned int turn_budget;

    kain_actor_runtime_ensure_initialized();

    if (config == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Invalid spawn configuration");
            snprintf(diag->detail, sizeof(diag->detail), "Spawn configuration is required");
        }
        return KAIN_ACTOR_ID_INVALID;
    }

    entry_kind = config->entry_kind;
    if (entry_kind == KAIN_ACTOR_ENTRY_KIND_INVALID) {
        entry_kind = (config->turn_fn != NULL)
            ? KAIN_ACTOR_ENTRY_KIND_MICROCELL_TURN
            : KAIN_ACTOR_ENTRY_KIND_LEGACY_BOOTSTRAP;
    }
    if ((entry_kind == KAIN_ACTOR_ENTRY_KIND_MICROCELL_TURN && config->turn_fn == NULL) ||
        (entry_kind != KAIN_ACTOR_ENTRY_KIND_MICROCELL_TURN && config->bootstrap_fn == NULL)) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Invalid spawn configuration");
            snprintf(
                diag->detail,
                sizeof(diag->detail),
                entry_kind == KAIN_ACTOR_ENTRY_KIND_MICROCELL_TURN
                    ? "Microcell turn function is required"
                    : "Bootstrap function is required"
            );
        }
        return KAIN_ACTOR_ID_INVALID;
    }

    turn_budget = config->microcell_turn_budget;
    if (turn_budget == 0u) {
        turn_budget = KAIN_ACTOR_DEFAULT_MICROCELL_TURN_BUDGET;
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
    actor->entry_kind = entry_kind;
    actor->bootstrap_fn = config->bootstrap_fn;
    actor->turn_fn = config->turn_fn;
    actor->microcell_turn_budget = turn_budget;
    actor->user_data = config->user_data;
    actor->ref_generation = 0u;
    actor->execution_class = config->execution_class != KAIN_ACTOR_EXECUTION_CLASS_INVALID
        ? config->execution_class
        : KAIN_ACTOR_EXECUTION_CLASS_MICROCELL;
    actor->locality_class = config->locality_class != KAIN_ACTOR_LOCALITY_INVALID
        ? config->locality_class
        : KAIN_ACTOR_LOCALITY_LOCAL;
    actor->monitors = NULL;
    actor->links = NULL;
    actor->in_scheduler_queue = 0;
    actor->in_scheduler_turn = 0;
    actor->spawn_sequence = 0;

    kain_actor_copy_name(actor->name, sizeof(actor->name), config->name);

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
        actor->supervisor.strategy = config->supervision_strategy;
        actor->supervisor.restart_count = 0;
        actor->supervisor.last_restart_time = 0;
        actor->supervisor.restart_window_start = 0;
        actor->supervisor.last_child_exit_reason = KAIN_ACTOR_EXIT_NORMAL;
        actor->supervisor.restart_limit_hit = 0;
    } else {
        actor->supervisor.supervisor_id = KAIN_ACTOR_ID_INVALID;
    }

    /* Store spawn config for potential restart */
    actor->spawn_config.bootstrap_fn = config->bootstrap_fn;
    actor->spawn_config.user_data = config->user_data;
    actor->spawn_config.mailbox_capacity = config->mailbox_capacity;
    actor->spawn_config.supervision_strategy = config->supervision_strategy;
    actor->spawn_config.restart_policy = config->restart_policy;
    actor->spawn_config.supervisor_id = config->supervisor_id;
    actor->spawn_config.retain_user_data = config->retain_user_data;
    actor->spawn_config.entry_kind = entry_kind;
    actor->spawn_config.turn_fn = config->turn_fn;
    actor->spawn_config.microcell_turn_budget = turn_budget;
    actor->spawn_config.execution_class = actor->execution_class;
    actor->spawn_config.locality_class = actor->locality_class;
    kain_actor_copy_name(
        actor->spawn_config.name,
        sizeof(actor->spawn_config.name),
        config->name
    );

    /* The compiler-owned actor state is reference-counted user data.
     * Keep the runtime's own reference for the lifetime of the actor so
     * the LLVM lane can target the canonical actor ABI without inventing a
     * second ownership model. */
    if (actor->user_data != NULL && config->retain_user_data) {
        rc_retain(actor->user_data);
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
        if (actor->user_data != NULL && config->retain_user_data) {
            rc_release(actor->user_data);
        }
        kain_actor_mailbox_destroy(&actor->mailbox);
        free(actor);
        return KAIN_ACTOR_ID_INVALID;
    }

    if (kain_actor_is_microcell_turn_actor(actor)) {
        actor->state = KAIN_ACTOR_STATE_RUNNING;
    } else {
        (void)force_direct_thread;
        /* Legacy bootstrap actors may block inside kain_actor_receive; keep them
         * off the microcell scheduler so LLVM actor turns cannot be starved. */
        if (kain_actor_spawn_direct_thread(actor, diag) != 0) {
            kain_actor_table_remove(actor_id);
            if (actor->user_data != NULL && config->retain_user_data) {
                rc_release(actor->user_data);
            }
            kain_actor_mailbox_destroy(&actor->mailbox);
            free(actor);
            return KAIN_ACTOR_ID_INVALID;
        }
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
    int append_result;
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

    append_result = kain_actor_mailbox_append_copied_locked(mailbox, message);
    if (append_result == -2) {
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
    if (append_result == -3) {
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
    if (append_result != 0) {
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
#else
        pthread_mutex_unlock(&mailbox->lock);
#endif
        return -1;
    }

    if (kain_actor_is_microcell_turn_actor(actor)) {
        kain_scheduler_ready_actor(actor);
    }

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

int kain_actor_ask_send_ref(
    const KainActorRef* target_ref,
    const KainActorMessage* message,
    KainDiagnostic* diag
) {
    KainActorState_Internal* actor = NULL;
    KainActorMailbox* mailbox;
    int append_result;
    int mailbox_was_empty;
    int inline_execute = 0;

    if (target_ref == NULL || message == NULL) {
        return -1;
    }

    kain_actor_runtime_ensure_initialized();

#ifdef _WIN32
    EnterCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_lock(&g_actor_table.lock);
#endif
    if (!kain_actor_table_ref_matches_locked(target_ref, &actor) || actor == NULL) {
#ifdef _WIN32
        LeaveCriticalSection(&g_actor_table.lock);
#else
        pthread_mutex_unlock(&g_actor_table.lock);
#endif
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_NOT_FOUND;
            snprintf(diag->message, sizeof(diag->message), "Actor ref is stale or dead");
        }
        return -1;
    }
#ifdef _WIN32
    LeaveCriticalSection(&g_actor_table.lock);
#else
    pthread_mutex_unlock(&g_actor_table.lock);
#endif

    mailbox = &actor->mailbox;
#ifdef _WIN32
    EnterCriticalSection(&mailbox->lock);
#else
    pthread_mutex_lock(&mailbox->lock);
#endif

    mailbox_was_empty = mailbox->count == 0u;
    append_result = kain_actor_mailbox_append_copied_locked(mailbox, message);
    if (append_result != 0) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            if (append_result == -2) {
                diag->code = KAIN_DIAG_CODE_ACTOR_MAILBOX_CLOSED;
                snprintf(diag->message, sizeof(diag->message), "Mailbox is closed");
            } else if (append_result == -3) {
                diag->code = KAIN_DIAG_CODE_ACTOR_MAILBOX_FULL;
                snprintf(diag->message, sizeof(diag->message), "Mailbox is full");
            } else {
                diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
                snprintf(diag->message, sizeof(diag->message), "Ask enqueue allocation failed");
            }
        }
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
#else
        pthread_mutex_unlock(&mailbox->lock);
#endif
        return -1;
    }

    if (mailbox_was_empty &&
        kain_actor_is_microcell_turn_actor(actor) &&
        actor->execution_class == KAIN_ACTOR_EXECUTION_CLASS_MICROCELL &&
        actor->locality_class == KAIN_ACTOR_LOCALITY_LOCAL &&
        (actor->state == KAIN_ACTOR_STATE_INITIALIZING ||
         actor->state == KAIN_ACTOR_STATE_RUNNING)) {
#ifdef _WIN32
        EnterCriticalSection(&g_scheduler.lock);
#else
        pthread_mutex_lock(&g_scheduler.lock);
#endif
        if (!g_scheduler.shutdown &&
            !actor->in_scheduler_queue &&
            !actor->in_scheduler_turn &&
            mailbox->count == 1u) {
            actor->in_scheduler_turn = 1;
            inline_execute = 1;
        }
#ifdef _WIN32
        LeaveCriticalSection(&g_scheduler.lock);
#else
        pthread_mutex_unlock(&g_scheduler.lock);
#endif
    }

    if (inline_execute) {
#ifdef _WIN32
        LeaveCriticalSection(&mailbox->lock);
#else
        pthread_mutex_unlock(&mailbox->lock);
#endif
        kain_actor_execute_microcell_turn(actor, 0);
        return 0;
    }

    if (kain_actor_is_microcell_turn_actor(actor)) {
        kain_scheduler_ready_actor(actor);
    }

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
    message->data_size = node->data_size;
    message->sender_id = node->sender_id;

    kain_actor_mailbox_cache_or_free_node(mailbox, node);

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
    (void)diag;

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
    message->data_size = node->data_size;
    message->sender_id = node->sender_id;

    kain_actor_mailbox_cache_or_free_node(mailbox, node);

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
    actor->state = KAIN_ACTOR_STATE_SHUTTING_DOWN;
    actor->exit_reason = KAIN_ACTOR_EXIT_SHUTDOWN;
    kain_actor_close_mailbox(actor);
    if (kain_actor_is_microcell_turn_actor(actor)) {
        kain_scheduler_ready_actor(actor);
    }

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
    kain_actor_close_mailbox(actor);
    if (kain_actor_is_microcell_turn_actor(actor)) {
        kain_scheduler_ready_actor(actor);
    }

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

int kain_actor_get_supervision_snapshot(
    KainActorId actor_id,
    KainActorSupervisionSnapshot* snapshot,
    KainDiagnostic* diag
) {
    KainActorState_Internal* actor = kain_actor_table_get(actor_id);
    if (actor == NULL || snapshot == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_NOT_FOUND;
            snprintf(diag->message, sizeof(diag->message), "Actor not found");
        }
        return -1;
    }

    snapshot->supervisor_id = actor->supervisor.supervisor_id;
    snapshot->strategy = actor->supervisor.strategy;
    snapshot->restart_policy = actor->supervisor.restart_policy;
    snapshot->restart_count = actor->supervisor.restart_count;
    snapshot->last_restart_time = actor->supervisor.last_restart_time;
    snapshot->restart_window_start = actor->supervisor.restart_window_start;
    snapshot->last_child_exit_reason = actor->supervisor.last_child_exit_reason;
    snapshot->restart_limit_hit = actor->supervisor.restart_limit_hit;
    snapshot->observed_child_exit_count = actor->observed_child_exit_count;
    snapshot->last_observed_child_id = actor->last_observed_child_id;
    snapshot->last_observed_child_exit_reason = actor->last_observed_child_exit_reason;
    snapshot->supervision_limit_hits = actor->supervision_limit_hits;
    snapshot->restart_attempt_count = actor->restart_attempt_count;
    snapshot->strategy_shutdown_count = actor->strategy_shutdown_count;
    snapshot->escalation_count = actor->escalation_count;
    return 0;
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

    if (actor->execution_class == KAIN_ACTOR_EXECUTION_CLASS_SYNTHETIC_REPLY_PORT &&
        actor->user_data != NULL) {
        KainActorReplyPortState* reply_port_state =
            (KainActorReplyPortState*)actor->user_data;
        actor->user_data = NULL;
        kain_actor_reply_port_state_release(reply_port_state);
    }

    /* Release the compiler-owned actor state once the runtime no longer
     * needs to keep the actor alive. */
    if (actor->user_data != NULL && actor->spawn_config.retain_user_data) {
        rc_release(actor->user_data);
        actor->user_data = NULL;
    }

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

    /* Check if monitor already exists */
    KainActorMonitor* existing = monitor_actor->monitors;
    while (existing != NULL) {
        if (existing->monitored_id == monitored_id) {
            /* Already monitoring */
            return 0;
        }
        existing = existing->next;
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

int kain_actor_demonitor(
    KainActorId monitor_id,
    KainActorId monitored_id,
    KainDiagnostic* diag
) {
    KainActorState_Internal* monitor_actor = kain_actor_table_get(monitor_id);
    if (monitor_actor == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_NOT_FOUND;
            snprintf(diag->message, sizeof(diag->message), "Monitor actor not found");
        }
        return -1;
    }

    /* Find and remove monitor */
    KainActorMonitor** monitor_ptr = &monitor_actor->monitors;
    while (*monitor_ptr != NULL) {
        KainActorMonitor* monitor = *monitor_ptr;
        if (monitor->monitored_id == monitored_id) {
            *monitor_ptr = monitor->next;
            free(monitor);
            return 0;
        }
        monitor_ptr = &monitor->next;
    }

    /* Monitor not found */
    if (diag != NULL) {
        kain_diagnostic_init(diag);
        diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
        diag->severity = KAIN_DIAG_SEVERITY_ERROR;
        diag->code = KAIN_DIAG_CODE_ACTOR_MONITOR_FAILED;
        snprintf(diag->message, sizeof(diag->message), "Monitor relationship not found");
    }
    return -1;
}

static void kain_actor_notify_monitors(KainActorState_Internal* actor) {
    /* Find all actors monitoring this one and send notifications */
    for (int i = 0; i < KAIN_ACTOR_TABLE_SIZE; i++) {
        KainActorState_Internal* other = g_actor_table.actors[i];
        if (other == NULL) continue;

        KainActorMonitor* monitor = other->monitors;
        while (monitor != NULL) {
            if (monitor->monitored_id == actor->actor_id) {
                /* Send exit notification message with exit reason encoded in type_tag */
                KainActorMessage msg = {0};
                msg.type_tag = 0xDEAD0000ULL | (unsigned long long)actor->exit_reason;
                msg.sender_id = actor->actor_id;
                msg.data = NULL;
                msg.data_size = 0;
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

    /* Check if link already exists on actor_a */
    KainActorLink* existing = actor_a_state->links;
    while (existing != NULL) {
        if ((existing->actor_a == actor_a && existing->actor_b == actor_b) ||
            (existing->actor_a == actor_b && existing->actor_b == actor_a)) {
            /* Link already exists */
            return 0;
        }
        existing = existing->next;
    }

    /* Create link record on actor_a */
    KainActorLink* link_a = (KainActorLink*)malloc(sizeof(KainActorLink));
    if (link_a == NULL) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_LINK_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Link allocation failed");
        }
        return -1;
    }

    /* Create link record on actor_b (bidirectional) */
    KainActorLink* link_b = (KainActorLink*)malloc(sizeof(KainActorLink));
    if (link_b == NULL) {
        free(link_a);
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_LINK_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Link allocation failed");
        }
        return -1;
    }

    link_a->actor_a = actor_a;
    link_a->actor_b = actor_b;
    link_a->next = actor_a_state->links;
    actor_a_state->links = link_a;

    link_b->actor_a = actor_a;
    link_b->actor_b = actor_b;
    link_b->next = actor_b_state->links;
    actor_b_state->links = link_b;

    return 0;
}

int kain_actor_unlink(
    KainActorId actor_a,
    KainActorId actor_b,
    KainDiagnostic* diag
) {
    KainActorState_Internal* actor_a_state = kain_actor_table_get(actor_a);
    KainActorState_Internal* actor_b_state = kain_actor_table_get(actor_b);

    int found_a = 0;
    int found_b = 0;

    /* Remove link from actor_a */
    if (actor_a_state != NULL) {
        KainActorLink** link_ptr = &actor_a_state->links;
        while (*link_ptr != NULL) {
            KainActorLink* link = *link_ptr;
            if ((link->actor_a == actor_a && link->actor_b == actor_b) ||
                (link->actor_a == actor_b && link->actor_b == actor_a)) {
                *link_ptr = link->next;
                free(link);
                found_a = 1;
                break;
            }
            link_ptr = &link->next;
        }
    }

    /* Remove link from actor_b */
    if (actor_b_state != NULL) {
        KainActorLink** link_ptr = &actor_b_state->links;
        while (*link_ptr != NULL) {
            KainActorLink* link = *link_ptr;
            if ((link->actor_a == actor_a && link->actor_b == actor_b) ||
                (link->actor_a == actor_b && link->actor_b == actor_a)) {
                *link_ptr = link->next;
                free(link);
                found_b = 1;
                break;
            }
            link_ptr = &link->next;
        }
    }

    if (!found_a && !found_b) {
        if (diag != NULL) {
            kain_diagnostic_init(diag);
            diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
            diag->severity = KAIN_DIAG_SEVERITY_ERROR;
            diag->code = KAIN_DIAG_CODE_ACTOR_LINK_FAILED;
            snprintf(diag->message, sizeof(diag->message), "Link not found");
        }
        return -1;
    }

    return 0;
}

static void kain_actor_propagate_links(KainActorState_Internal* actor) {
    /* Terminate all linked actors on abnormal exit */
    KainActorLink* link = actor->links;
    while (link != NULL) {
        KainActorId other_id = (link->actor_a == actor->actor_id) ? link->actor_b : link->actor_a;

        /* Check if other actor still exists and is not already terminated */
        KainActorState_Internal* other_actor = kain_actor_table_get(other_id);
        if (other_actor != NULL &&
            other_actor->state != KAIN_ACTOR_STATE_TERMINATED &&
            other_actor->state != KAIN_ACTOR_STATE_FAILED) {
            /* Kill linked actor with appropriate exit reason */
            other_actor->exit_reason = KAIN_ACTOR_EXIT_KILLED;
            kain_actor_kill(other_id, NULL);
        }

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

static void kain_actor_registry_clear(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_lock(&g_actor_registry.lock);
#endif

    for (int i = 0; i < KAIN_ACTOR_REGISTRY_SIZE; i++) {
        KainActorRegistryEntry* entry = g_actor_registry.buckets[i];
        while (entry != NULL) {
            KainActorRegistryEntry* next = entry->next;
            free(entry);
            entry = next;
        }
        g_actor_registry.buckets[i] = NULL;
    }

#ifdef _WIN32
    LeaveCriticalSection(&g_actor_registry.lock);
#else
    pthread_mutex_unlock(&g_actor_registry.lock);
#endif
}

static void kain_actor_copy_name(char* dest, size_t dest_size, const char* src) {
    if (dest == NULL || dest_size == 0) {
        return;
    }

    if (src == NULL || src[0] == '\0') {
        dest[0] = '\0';
        return;
    }

    snprintf(dest, dest_size, "%s", src);
}

int kain_actor_registry_register(
    const char* name,
    KainActorId actor_id,
    KainDiagnostic* diag
) {
    kain_actor_runtime_ensure_initialized();

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

    kain_actor_copy_name(entry->name, sizeof(entry->name), name);
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
    kain_actor_runtime_ensure_initialized();

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
    kain_actor_runtime_ensure_initialized();

    (void)diag;

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

void kain_actor_scheduler_snapshot(KainActorSchedulerSnapshot* snapshot) {
    if (snapshot == NULL) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    if (!g_actor_runtime_initialized) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_lock(&g_scheduler.lock);
#endif

    snapshot->queue_depth = g_scheduler.queue_depth;
    snapshot->max_queue_depth = g_scheduler.max_queue_depth;
    snapshot->total_enqueued = g_scheduler.total_enqueued;
    snapshot->total_dequeued = g_scheduler.total_dequeued;
    snapshot->worker_count = KAIN_SCHEDULER_WORKER_COUNT;
    snapshot->active_workers = g_scheduler.active_workers;
    snapshot->busy_workers = g_scheduler.busy_workers;
    snapshot->max_busy_workers = g_scheduler.max_busy_workers;
    snapshot->overflow_thread_spawns = g_scheduler.overflow_thread_spawns;
    snapshot->shutdown = g_scheduler.shutdown;

#ifdef _WIN32
    LeaveCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_unlock(&g_scheduler.lock);
#endif
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
        while (g_scheduler.queue_depth == 0u && !g_scheduler.shutdown) {
#ifdef _WIN32
            LeaveCriticalSection(&g_scheduler.lock);
            WaitForSingleObject(g_scheduler.work_available, INFINITE);
            EnterCriticalSection(&g_scheduler.lock);
#else
            pthread_cond_wait(&g_scheduler.work_available, &g_scheduler.lock);
#endif
        }

        /* Check for shutdown */
        if (g_scheduler.shutdown && g_scheduler.queue_depth == 0u) {
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

        KainActorState_Internal* actor = kain_actor_table_get(actor_id);
        if (actor != NULL && kain_actor_is_microcell_turn_actor(actor)) {
            kain_actor_execute_microcell_turn(actor, 1);
        } else if (actor != NULL && actor->state == KAIN_ACTOR_STATE_INITIALIZING) {
#ifdef _WIN32
            EnterCriticalSection(&g_scheduler.lock);
#else
            pthread_mutex_lock(&g_scheduler.lock);
#endif
            g_scheduler.busy_workers++;
            if (g_scheduler.busy_workers > g_scheduler.max_busy_workers) {
                g_scheduler.max_busy_workers = g_scheduler.busy_workers;
            }
#ifdef _WIN32
            LeaveCriticalSection(&g_scheduler.lock);
#else
            pthread_mutex_unlock(&g_scheduler.lock);
#endif

            actor->state = KAIN_ACTOR_STATE_RUNNING;

            KainActorExitReason exit_reason = actor->bootstrap_fn(
                actor->actor_id,
                &actor->mailbox,
                actor->user_data
            );

            kain_actor_finalize_exit_state(actor, exit_reason);
            kain_actor_complete_exit_side_effects(actor);

#ifdef _WIN32
            EnterCriticalSection(&g_scheduler.lock);
#else
            pthread_mutex_lock(&g_scheduler.lock);
#endif
            if (g_scheduler.busy_workers > 0) {
                g_scheduler.busy_workers--;
            }
#ifdef _WIN32
            LeaveCriticalSection(&g_scheduler.lock);
#else
            pthread_mutex_unlock(&g_scheduler.lock);
#endif
            kain_scheduler_finish_turn(actor);
        } else if (actor != NULL &&
                   (actor->state == KAIN_ACTOR_STATE_SHUTTING_DOWN ||
                    actor->state == KAIN_ACTOR_STATE_FAILED)) {
            kain_actor_finalize_exit_state(actor, actor->exit_reason);
            kain_actor_complete_exit_side_effects(actor);
            kain_scheduler_finish_turn(actor);
        } else if (actor != NULL) {
            kain_scheduler_finish_turn(actor);
        }
    }

#ifdef _WIN32
    EnterCriticalSection(&g_scheduler.lock);
    if (g_scheduler.active_workers > 0) {
        g_scheduler.active_workers--;
    }
    LeaveCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_lock(&g_scheduler.lock);
    if (g_scheduler.active_workers > 0) {
        g_scheduler.active_workers--;
    }
    pthread_mutex_unlock(&g_scheduler.lock);
#endif

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

    memset(g_scheduler.queue, 0, sizeof(g_scheduler.queue));
    g_scheduler.enqueue_cursor = 0u;
    g_scheduler.dequeue_cursor = 0u;
    g_scheduler.shutdown = 0;
    g_scheduler.active_workers = KAIN_SCHEDULER_WORKER_COUNT;
    g_scheduler.busy_workers = 0;
    g_scheduler.max_busy_workers = 0;
    g_scheduler.queue_depth = 0;
    g_scheduler.max_queue_depth = 0;
    g_scheduler.total_enqueued = 0;
    g_scheduler.total_dequeued = 0;
    g_scheduler.overflow_thread_spawns = 0;

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

}

static void kain_scheduler_ready_actor(KainActorState_Internal* actor) {
    KainActorId actor_id;
    if (actor == NULL) {
        return;
    }

    actor_id = actor->actor_id;
    if (actor_id == KAIN_ACTOR_ID_INVALID) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_lock(&g_scheduler.lock);
#endif

    if (g_scheduler.shutdown ||
        actor->in_scheduler_queue ||
        actor->in_scheduler_turn ||
        actor->state == KAIN_ACTOR_STATE_TERMINATED ||
        g_scheduler.queue_depth >= KAIN_SCHEDULER_QUEUE_CAPACITY) {
#ifdef _WIN32
        LeaveCriticalSection(&g_scheduler.lock);
#else
        pthread_mutex_unlock(&g_scheduler.lock);
#endif
        return;
    }

    actor->in_scheduler_queue = 1;
    g_scheduler.queue[g_scheduler.enqueue_cursor & KAIN_SCHEDULER_QUEUE_MASK] = actor_id;
    g_scheduler.enqueue_cursor++;
    g_scheduler.queue_depth++;
    g_scheduler.total_enqueued++;
    if (g_scheduler.queue_depth > g_scheduler.max_queue_depth) {
        g_scheduler.max_queue_depth = g_scheduler.queue_depth;
    }

#ifdef _WIN32
    SetEvent(g_scheduler.work_available);
    LeaveCriticalSection(&g_scheduler.lock);
#else
    pthread_cond_signal(&g_scheduler.work_available);
    pthread_mutex_unlock(&g_scheduler.lock);
#endif
}

static void kain_scheduler_finish_turn(KainActorState_Internal* actor) {
    int should_requeue;
    if (actor == NULL) {
        return;
    }

#ifdef _WIN32
    EnterCriticalSection(&actor->mailbox.lock);
#else
    pthread_mutex_lock(&actor->mailbox.lock);
#endif
    should_requeue = kain_actor_is_microcell_turn_actor(actor) &&
                     actor->state == KAIN_ACTOR_STATE_RUNNING &&
                     actor->mailbox.count > 0u;

#ifdef _WIN32
    EnterCriticalSection(&g_scheduler.lock);
#else
    pthread_mutex_lock(&g_scheduler.lock);
#endif
    actor->in_scheduler_turn = 0;
    if (should_requeue &&
        !g_scheduler.shutdown &&
        !actor->in_scheduler_queue &&
        g_scheduler.queue_depth < KAIN_SCHEDULER_QUEUE_CAPACITY) {
        actor->in_scheduler_queue = 1;
        g_scheduler.queue[g_scheduler.enqueue_cursor & KAIN_SCHEDULER_QUEUE_MASK] = actor->actor_id;
        g_scheduler.enqueue_cursor++;
        g_scheduler.queue_depth++;
        g_scheduler.total_enqueued++;
        if (g_scheduler.queue_depth > g_scheduler.max_queue_depth) {
            g_scheduler.max_queue_depth = g_scheduler.queue_depth;
        }
#ifdef _WIN32
        SetEvent(g_scheduler.work_available);
#else
        pthread_cond_signal(&g_scheduler.work_available);
#endif
    }
#ifdef _WIN32
    LeaveCriticalSection(&g_scheduler.lock);
    LeaveCriticalSection(&actor->mailbox.lock);
#else
    pthread_mutex_unlock(&g_scheduler.lock);
    pthread_mutex_unlock(&actor->mailbox.lock);
#endif
}

static int kain_actor_spawn_direct_thread(KainActorState_Internal* actor, KainDiagnostic* diag) {
    if (actor == NULL) {
        return -1;
    }

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
        return -1;
    }
    actor->direct_thread_started = 1;
#else
    {
        int result = pthread_create(&actor->thread, NULL, kain_actor_thread_proc, actor);
        if (result != 0) {
            if (diag != NULL) {
                kain_diagnostic_init(diag);
                diag->subsystem = KAIN_DIAG_SUBSYSTEM_ACTOR;
                diag->severity = KAIN_DIAG_SEVERITY_ERROR;
                diag->code = KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED;
                snprintf(diag->message, sizeof(diag->message), "Thread creation failed");
            }
            return -1;
        }
        actor->direct_thread_started = 1;
    }
#endif

    return 0;
}

static void kain_actor_join_direct_thread(KainActorState_Internal* actor) {
    if (actor == NULL || !actor->direct_thread_started) {
        return;
    }

#ifdef _WIN32
    if (actor->thread_handle != NULL) {
        if (GetCurrentThreadId() != actor->thread_id) {
            WaitForSingleObject(actor->thread_handle, INFINITE);
        }
        CloseHandle(actor->thread_handle);
        actor->thread_handle = NULL;
    }
    actor->thread_id = 0;
#else
    if (!pthread_equal(pthread_self(), actor->thread)) {
        pthread_join(actor->thread, NULL);
    }
#endif
    actor->direct_thread_started = 0;
}

static KainActorId kain_scheduler_dequeue(void) {
    /* Caller must hold scheduler lock */
    KainActorId actor_id;
    size_t slot_index;

    if (g_scheduler.queue_depth == 0u) {
        return KAIN_ACTOR_ID_INVALID;
    }

    slot_index = g_scheduler.dequeue_cursor & KAIN_SCHEDULER_QUEUE_MASK;
    actor_id = g_scheduler.queue[slot_index];
    g_scheduler.queue[slot_index] = KAIN_ACTOR_ID_INVALID;
    g_scheduler.dequeue_cursor++;
    g_scheduler.queue_depth--;
    g_scheduler.total_dequeued++;
    {
        KainActorState_Internal* actor = kain_actor_table_get(actor_id);
        if (actor != NULL) {
            actor->in_scheduler_queue = 0;
            actor->in_scheduler_turn = 1;
        }
    }

    return actor_id;
}

KainActorId kain_actor_spawn(
    const KainActorSpawnConfig* config,
    KainDiagnostic* diag
) {
    return kain_actor_spawn_internal(config, 0, diag);
}

/*
 * Supervision Policy Implementation
 *
 * Implements restart, shutdown, and escalation policies for supervisors.
 *
 * Requirements: 6.2, 6.3, 6.4
 */

/*
 * Determine if a child actor should be restarted based on restart policy
 */
static int kain_actor_should_restart(KainActorState_Internal* child) {
    if (child == NULL || child->supervisor.supervisor_id == KAIN_ACTOR_ID_INVALID) {
        return 0;
    }

    KainRestartPolicy policy = child->supervisor.restart_policy;
    KainActorExitReason exit_reason = child->exit_reason;

    switch (policy) {
        case KAIN_RESTART_POLICY_PERMANENT:
            /* Always restart, regardless of exit reason */
            return 1;

        case KAIN_RESTART_POLICY_TEMPORARY:
            /* Never restart */
            return 0;

        case KAIN_RESTART_POLICY_TRANSIENT:
            /* Restart only on abnormal exit */
            return (exit_reason != KAIN_ACTOR_EXIT_NORMAL &&
                    exit_reason != KAIN_ACTOR_EXIT_SHUTDOWN);

        default:
            return 0;
    }
}

/*
 * Check if restart limit has been exceeded within the time window
 */
static int kain_actor_restart_limit_exceeded(KainActorState_Internal* child) {
    time_t current_time = time(NULL);

    /* Initialize restart window if this is the first restart */
    if (child->supervisor.restart_window_start == 0) {
        child->supervisor.restart_window_start = current_time;
        child->supervisor.restart_count = 0;
        child->supervisor.restart_limit_hit = 0;
        return 0;
    }

    /* Check if we're still within the restart window */
    time_t window_elapsed = current_time - child->supervisor.restart_window_start;

    if (window_elapsed > KAIN_SUPERVISION_RESTART_WINDOW_SECONDS) {
        /* Window expired, reset counters */
        child->supervisor.restart_window_start = current_time;
        child->supervisor.restart_count = 0;
        child->supervisor.restart_limit_hit = 0;
        return 0;
    }

    /* Check if restart count exceeds limit */
    if (child->supervisor.restart_count >= KAIN_SUPERVISION_MAX_RESTARTS) {
        child->supervisor.restart_limit_hit = 1;
        return 1;
    }
    child->supervisor.restart_limit_hit = 0;
    return 0;
}

/*
 * Restart a child actor with the same configuration
 */
static KainActorId kain_actor_restart_child(KainActorState_Internal* child, KainDiagnostic* diag) {
    if (child == NULL) {
        return KAIN_ACTOR_ID_INVALID;
    }

    /* Create spawn config from stored configuration */
    KainActorSpawnConfig config;
    kain_actor_spawn_config_init(&config);
    config.bootstrap_fn = child->spawn_config.bootstrap_fn;
    config.user_data = child->spawn_config.user_data;
    config.mailbox_capacity = child->spawn_config.mailbox_capacity;
    config.supervision_strategy = child->spawn_config.supervision_strategy;
    config.supervisor_id = child->spawn_config.supervisor_id;
    config.restart_policy = child->spawn_config.restart_policy;
    config.retain_user_data = child->spawn_config.retain_user_data;
    config.entry_kind = child->spawn_config.entry_kind;
    config.turn_fn = child->spawn_config.turn_fn;
    config.microcell_turn_budget = child->spawn_config.microcell_turn_budget;
    config.execution_class = child->spawn_config.execution_class;
    config.locality_class = child->spawn_config.locality_class;

    kain_actor_copy_name(config.name, sizeof(config.name), child->spawn_config.name);

    /* Spawn new actor instance */
    KainActorId new_id = kain_actor_spawn(&config, diag);

    if (new_id != KAIN_ACTOR_ID_INVALID) {
        time_t now = time(NULL);
        child->restart_attempt_count = child->restart_attempt_count + 1;
        child->supervisor.restart_count = child->supervisor.restart_count + 1;
        child->supervisor.last_restart_time = now;
        /* Update restart tracking in the new actor instance */
        KainActorState_Internal* new_actor = kain_actor_table_get(new_id);
        if (new_actor != NULL) {
            new_actor->restart_attempt_count = child->restart_attempt_count;
            new_actor->supervisor.restart_count = child->supervisor.restart_count;
            new_actor->supervisor.last_restart_time = now;
            new_actor->supervisor.restart_window_start = child->supervisor.restart_window_start;
            new_actor->supervisor.last_child_exit_reason = child->exit_reason;
            new_actor->supervisor.restart_limit_hit = child->supervisor.restart_limit_hit;
        }
    }

    return new_id;
}

/*
 * Escalate failure to parent supervisor
 */
static void kain_actor_escalate_to_supervisor(KainActorState_Internal* child) {
    if (child == NULL || child->supervisor.supervisor_id == KAIN_ACTOR_ID_INVALID) {
        return;
    }

    KainActorState_Internal* supervisor = kain_actor_table_get(child->supervisor.supervisor_id);
    if (supervisor == NULL) {
        return;
    }

    /* Terminate supervisor with escalation exit reason */
    supervisor->escalation_count++;
    supervisor->exit_reason = KAIN_ACTOR_EXIT_SUPERVISOR_ESCALATION;
    supervisor->state = KAIN_ACTOR_STATE_FAILED;

    /* Notify supervisor's monitors */
    kain_actor_notify_monitors(supervisor);

    /* Propagate to supervisor's links if abnormal */
    kain_actor_propagate_links(supervisor);

    /* If supervisor also has a supervisor, escalate further */
    if (supervisor->supervisor.supervisor_id != KAIN_ACTOR_ID_INVALID) {
        kain_actor_handle_child_exit(supervisor);
    }
}

/*
 * Apply supervision strategy when a child exits
 */
static void kain_actor_apply_supervision_strategy(
    KainActorState_Internal* supervisor,
    KainActorState_Internal* failed_child
) {
    if (supervisor == NULL || failed_child == NULL) {
        return;
    }

    KainSupervisionStrategy strategy = failed_child->supervisor.strategy;

    switch (strategy) {
        case KAIN_SUPERVISION_STRATEGY_ONE_FOR_ONE:
            /* Only restart the failed child (already handled in kain_actor_handle_child_exit) */
            break;

        case KAIN_SUPERVISION_STRATEGY_ONE_FOR_ALL:
            /* Terminate all children and restart them */
            /* Find all children of this supervisor */
            for (int i = 0; i < KAIN_ACTOR_TABLE_SIZE; i++) {
                KainActorState_Internal* actor = g_actor_table.actors[i];
                if (actor != NULL &&
                    actor->supervisor.supervisor_id == supervisor->actor_id &&
                    actor->actor_id != failed_child->actor_id) {
                    /* Shutdown sibling actor */
                    supervisor->strategy_shutdown_count++;
                    kain_actor_shutdown(actor->actor_id, NULL);
                }
            }
            break;

        case KAIN_SUPERVISION_STRATEGY_REST_FOR_ONE:
            /* Terminate children started after the failed child */
            for (int i = 0; i < KAIN_ACTOR_TABLE_SIZE; i++) {
                KainActorState_Internal* actor = g_actor_table.actors[i];
                if (actor != NULL &&
                    actor->supervisor.supervisor_id == supervisor->actor_id &&
                    actor->actor_id != failed_child->actor_id &&
                    actor->spawn_sequence > failed_child->spawn_sequence) {
                    supervisor->strategy_shutdown_count++;
                    kain_actor_shutdown(actor->actor_id, NULL);
                }
            }
            break;

        default:
            break;
    }
}

/*
 * Handle child actor exit and apply supervision policies
 */
static void kain_actor_handle_child_exit(KainActorState_Internal* child) {
    if (child == NULL || child->supervisor.supervisor_id == KAIN_ACTOR_ID_INVALID) {
        return;
    }

    KainActorState_Internal* supervisor = kain_actor_table_get(child->supervisor.supervisor_id);
    if (supervisor == NULL) {
        /* Supervisor no longer exists, nothing to do */
        return;
    }

    {
        KainActorMessage msg = {0};
        msg.type_tag = 0xDEAD0000ULL | (unsigned long long)child->exit_reason;
        msg.sender_id = child->actor_id;
        kain_actor_send(supervisor->actor_id, &msg, NULL);
    }

    supervisor->observed_child_exit_count++;
    supervisor->last_observed_child_id = child->actor_id;
    supervisor->last_observed_child_exit_reason = child->exit_reason;

    child->supervisor.last_child_exit_reason = child->exit_reason;

    /* Check if child should be restarted */
    if (!kain_actor_should_restart(child)) {
        /* No restart needed, just cleanup */
        return;
    }

    /* Check if restart limit has been exceeded */
    if (kain_actor_restart_limit_exceeded(child)) {
        supervisor->supervision_limit_hits++;
        /* Restart limit exceeded, escalate to parent supervisor */
        kain_actor_escalate_to_supervisor(child);
        return;
    }

    /* Apply supervision strategy */
    kain_actor_apply_supervision_strategy(supervisor, child);

    /* Attempt to restart the child */
    KainDiagnostic diag;
    KainActorId new_id = kain_actor_restart_child(child, &diag);

    if (new_id == KAIN_ACTOR_ID_INVALID) {
        /* Restart failed, escalate to parent supervisor */
        kain_actor_escalate_to_supervisor(child);
    }
}
