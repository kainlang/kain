/*
 * KAIN Native Runtime Async Implementation
 *
 * This file implements the native async/task/timer lane for KAIN. The model
 * is intentionally self-contained so the runtime can validate futures, wake
 * handles, cancellation, and timer delivery without depending on actor
 * internals or shared manifest edits.
 */

#include "../../include/async.h"
#include "../../include/attrition.h"
#include "../../include/batch_queue.h"
#include "../../include/base.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sched.h>
#include <sys/time.h>
#endif

#define KAIN_ASYNC_SOURCE_PATH "runtime/native/src/core/async.c"
#define KAIN_ASYNC_MAX_TASKS 256
#define KAIN_ASYNC_MAX_TIMERS 256
#define KAIN_ASYNC_SLOT_WORD_BITS 64u
#define KAIN_ASYNC_TASK_WORD_COUNT (KAIN_ASYNC_MAX_TASKS / KAIN_ASYNC_SLOT_WORD_BITS)
#define KAIN_ASYNC_TIMER_WORD_COUNT (KAIN_ASYNC_MAX_TIMERS / KAIN_ASYNC_SLOT_WORD_BITS)
#define KAIN_ASYNC_TASK_INDEX_CAPACITY 512u
#define KAIN_ASYNC_TASK_INDEX_MASK (KAIN_ASYNC_TASK_INDEX_CAPACITY - 1u)
#define KAIN_ASYNC_TIMER_INDEX_CAPACITY 512u
#define KAIN_ASYNC_TIMER_INDEX_MASK (KAIN_ASYNC_TIMER_INDEX_CAPACITY - 1u)
#define KAIN_ASYNC_BATCH_QUEUE_CAPACITY 4096u
#define KAIN_ASYNC_REF_INVALID_SLOT UINT32_MAX

/* Packed flags for KainAsyncTaskRecord */
#define KAIN_ASYNC_FLAG_IN_USE              0x00000001u
#define KAIN_ASYNC_FLAG_CANCEL_REQUESTED    0x00000002u
#define KAIN_ASYNC_FLAG_RUN_ENQUEUED         0x00000004u
#define KAIN_ASYNC_FLAG_COMPLETION_ENQUEUED  0x00000008u
#define KAIN_ASYNC_FLAG_COMPLETION_FIRED     0x00000010u
#define KAIN_ASYNC_FLAG_COMPLETION_DEFERRED  0x00000020u
#define KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED 0x00000040u
#define KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE    0x00000080u
#define KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE 0x00000100u

/*
 * Blocked mask: single-bit-test replacement for the 4-way OR in
 * kain_async_task_is_blocked_locked. Packs continuation_blocked,
 * dependency_wait_active, child_wait_active, and completion_deferred
 * into one uint32_t test.
 * Proof: z3/proofs-experimental/async-packed-flags-blocked-mask.smt2 (unsat)
 */
#define KAIN_ASYNC_FLAG_BLOCKED_MASK \
    (KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED | \
     KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE | \
     KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE | \
     KAIN_ASYNC_FLAG_COMPLETION_DEFERRED)
#if (KAIN_ASYNC_MAX_TASKS % KAIN_ASYNC_SLOT_WORD_BITS) != 0
#error "KAIN_ASYNC_MAX_TASKS must be divisible by 64 for occupancy-word indexing."
#endif
#if (KAIN_ASYNC_MAX_TIMERS % KAIN_ASYNC_SLOT_WORD_BITS) != 0
#error "KAIN_ASYNC_MAX_TIMERS must be divisible by 64 for occupancy-word indexing."
#endif
#if (KAIN_ASYNC_TASK_INDEX_CAPACITY & KAIN_ASYNC_TASK_INDEX_MASK) != 0
#error "KAIN_ASYNC_TASK_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (KAIN_ASYNC_TIMER_INDEX_CAPACITY & KAIN_ASYNC_TIMER_INDEX_MASK) != 0
#error "KAIN_ASYNC_TIMER_INDEX_CAPACITY must be a power of two for masked probing."
#endif

#if defined(_MSC_VER)
#define KAIN_THREAD_LOCAL __declspec(thread)
#else
#define KAIN_THREAD_LOCAL _Thread_local
#endif

struct KainAsyncTaskRecord;

typedef struct KainTaskHandle {
    KainTaskId id;
    struct KainAsyncTaskRecord* owner;
} KainTaskHandle;

typedef struct {
    unsigned long long delay_ms;
    KainTimerId timer_id;
    int armed;
} KainAsyncSleepState;

#ifdef _WIN32
typedef CRITICAL_SECTION KainAsyncMutex;
typedef CONDITION_VARIABLE KainAsyncCondVar;
#else
typedef pthread_mutex_t KainAsyncMutex;
typedef pthread_cond_t KainAsyncCondVar;
#endif

typedef struct {
    uint32_t slot;
    KainTaskId id;
} KainAsyncTaskRef;

typedef enum {
    KAIN_ASYNC_BATCH_OP_RUN_TASK = 1u,
    KAIN_ASYNC_BATCH_OP_COMPLETE_TASK = 2u,
} KainAsyncBatchOpKind;

typedef struct KainAsyncTaskRecord {
    uint32_t flags;
    KainTaskId id;
    KainTaskSpawnConfig config;
    KainTaskState state;
    void* result;
    KainFutureContext future_context;
    KainTaskRuntimeState runtime_state;
    KainTaskHandle handle;
    KainAsyncSleepState sleep_state;
    KainAsyncTaskRef parent_ref;
    KainAsyncTaskRef continuation_ref;
    uint64_t wait_dependency_bits[KAIN_ASYNC_TASK_WORD_COUNT];
    uint64_t live_child_bits[KAIN_ASYNC_TASK_WORD_COUNT];
    unsigned int dependency_wait_count;
    unsigned int live_child_count;
    unsigned int completed_child_count;
    KainTaskWaitMode dependency_wait_mode;
    KainTaskWaitMode child_wait_mode;
    KainTaskCompletionCallback completion_callback;
    void* completion_user_data;
    KainAsyncMutex lock;
    KainAsyncCondVar cond;
} KainAsyncTaskRecord;

typedef struct {
    int in_use;
    KainTimerId id;
    void* wake_handle;
    unsigned long long delay_ms;
    atomic_int cancelled;
    atomic_int fired;
    atomic_int started;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
} KainAsyncTimerRecord;

static KainAsyncTaskRecord g_async_tasks[KAIN_ASYNC_MAX_TASKS];
static KainAsyncTimerRecord g_async_timers[KAIN_ASYNC_MAX_TIMERS];
static uint64_t g_async_task_occupancy_words[KAIN_ASYNC_TASK_WORD_COUNT];
static uint64_t g_async_timer_occupancy_words[KAIN_ASYNC_TIMER_WORD_COUNT];
static uint32_t g_async_task_index[KAIN_ASYNC_TASK_INDEX_CAPACITY];
static uint32_t g_async_timer_index[KAIN_ASYNC_TIMER_INDEX_CAPACITY];
static KainBatchQueue g_async_batch_queue;
static KainBatchQueueEntry g_async_batch_active_entries[KAIN_ASYNC_BATCH_QUEUE_CAPACITY];
static KainBatchQueueEntry g_async_batch_pending_entries[KAIN_ASYNC_BATCH_QUEUE_CAPACITY];
static KainAsyncMutex g_async_global_lock;
static KainTaskId g_async_next_task_id = 1;
static KainTimerId g_async_next_timer_id = 1;
static KAIN_THREAD_LOCAL KainTaskId g_async_current_task_id = KAIN_TASK_ID_INVALID;
static atomic_uint_least64_t g_attrition_async_task_live_count = 0;
static atomic_uint_least64_t g_attrition_async_task_peak_count = 0;
static atomic_uint_least64_t g_attrition_async_task_spawn_count = 0;
static atomic_uint_least64_t g_attrition_async_task_exit_count = 0;
static atomic_uint_least64_t g_attrition_async_task_stale_reject_count = 0;
static atomic_uint_least64_t g_attrition_async_timer_live_count = 0;
static atomic_uint_least64_t g_attrition_async_timer_peak_count = 0;
static atomic_uint_least64_t g_attrition_async_timer_spawn_count = 0;
static atomic_uint_least64_t g_attrition_async_timer_exit_count = 0;
static atomic_uint_least64_t g_attrition_async_timer_cancel_count = 0;
static atomic_uint_least64_t g_attrition_async_timer_stale_reject_count = 0;

#ifdef _WIN32
static INIT_ONCE g_async_init_once = INIT_ONCE_STATIC_INIT;
#else
static pthread_once_t g_async_init_once = PTHREAD_ONCE_INIT;
#endif

static int kain_async_task_is_terminal(KainTaskState state);

static void kain_async_attrition_update_peak(
    atomic_uint_least64_t* peak_counter,
    uint64_t candidate
) {
    uint64_t current_peak = atomic_load_explicit(peak_counter, memory_order_relaxed);
    while (candidate > current_peak &&
           !atomic_compare_exchange_weak_explicit(
               peak_counter,
               &current_peak,
               candidate,
               memory_order_relaxed,
               memory_order_relaxed)) {
    }
}

static uint64_t kain_async_popcount_u64(uint64_t value) {
    value = value - ((value >> 1u) & UINT64_C(0x5555555555555555));
    value = (value & UINT64_C(0x3333333333333333)) + ((value >> 2u) & UINT64_C(0x3333333333333333));
    value = (value + (value >> 4u)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return (value * UINT64_C(0x0101010101010101)) >> 56u;
}

static void kain_async_mutex_init(KainAsyncMutex* mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
#else
    pthread_mutex_init(mutex, NULL);
#endif
}

static void kain_async_mutex_lock(KainAsyncMutex* mutex) {
#ifdef _WIN32
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

static void kain_async_mutex_unlock(KainAsyncMutex* mutex) {
#ifdef _WIN32
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

static void kain_async_cond_init(KainAsyncCondVar* cond) {
#ifdef _WIN32
    InitializeConditionVariable(cond);
#else
    pthread_cond_init(cond, NULL);
#endif
}

static void kain_async_cond_signal(KainAsyncCondVar* cond) {
#ifdef _WIN32
    WakeConditionVariable(cond);
#else
    pthread_cond_broadcast(cond);
#endif
}

static void kain_async_cond_wait(KainAsyncCondVar* cond, KainAsyncMutex* mutex) {
#ifdef _WIN32
    SleepConditionVariableCS(cond, mutex, INFINITE);
#else
    pthread_cond_wait(cond, mutex);
#endif
}

static KainAsyncTaskRef kain_async_task_ref_invalid(void) {
    KainAsyncTaskRef ref;
    ref.slot = KAIN_ASYNC_REF_INVALID_SLOT;
    ref.id = KAIN_TASK_ID_INVALID;
    return ref;
}

static uint32_t kain_async_task_slot(const KainAsyncTaskRecord* task) {
    return (uint32_t)(task - g_async_tasks);
}

static uint64_t kain_async_task_slot_bit(uint32_t slot) {
    return UINT64_C(1) << (slot & 63u);
}

static int kain_async_task_ref_is_valid(KainAsyncTaskRef ref) {
    return ref.slot != KAIN_ASYNC_REF_INVALID_SLOT && ref.id != KAIN_TASK_ID_INVALID;
}

static KainAsyncTaskRef kain_async_task_ref_from_task(const KainAsyncTaskRecord* task) {
    KainAsyncTaskRef ref = kain_async_task_ref_invalid();
    if (task != NULL) {
        ref.slot = kain_async_task_slot(task);
        ref.id = task->id;
    }
    return ref;
}

static KainAsyncTaskRecord* kain_async_task_from_ref_locked(KainAsyncTaskRef ref) {
    if (!kain_async_task_ref_is_valid(ref) || ref.slot >= KAIN_ASYNC_MAX_TASKS) {
        return NULL;
    }
    if (!(g_async_tasks[ref.slot].flags & KAIN_ASYNC_FLAG_IN_USE) || g_async_tasks[ref.slot].id != ref.id) {
        return NULL;
    }
    return &g_async_tasks[ref.slot];
}

static int kain_async_task_bitset_test(const uint64_t* bitset, uint32_t slot) {
    return bitset != NULL &&
           slot < KAIN_ASYNC_MAX_TASKS &&
           (bitset[slot / KAIN_ASYNC_SLOT_WORD_BITS] & kain_async_task_slot_bit(slot)) != 0u;
}

static int kain_async_task_bitset_set(uint64_t* bitset, uint32_t slot) {
    uint64_t* word;
    uint64_t bit;
    if (bitset == NULL || slot >= KAIN_ASYNC_MAX_TASKS) {
        return 0;
    }
    word = &bitset[slot / KAIN_ASYNC_SLOT_WORD_BITS];
    bit = kain_async_task_slot_bit(slot);
    if ((*word & bit) != 0u) {
        return 0;
    }
    *word |= bit;
    return 1;
}

static int kain_async_task_bitset_clear(uint64_t* bitset, uint32_t slot) {
    uint64_t* word;
    uint64_t bit;
    if (bitset == NULL || slot >= KAIN_ASYNC_MAX_TASKS) {
        return 0;
    }
    word = &bitset[slot / KAIN_ASYNC_SLOT_WORD_BITS];
    bit = kain_async_task_slot_bit(slot);
    if ((*word & bit) == 0u) {
        return 0;
    }
    *word &= ~bit;
    return 1;
}

static void kain_async_task_sync_runtime_flags_locked(KainAsyncTaskRecord* task) {
    if (task == NULL) {
        return;
    }
    atomic_store_explicit(&task->runtime_state.child_wait_count, task->live_child_count, memory_order_release);
    atomic_store_explicit(
        &task->runtime_state.dependency_wait_count,
        task->dependency_wait_count,
        memory_order_release);
    atomic_store_explicit(
        &task->runtime_state.continuation_blocked,
        (task->flags & KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED) != 0,
        memory_order_release);
    atomic_store_explicit(
        &task->runtime_state.completion_deferred,
        (task->flags & KAIN_ASYNC_FLAG_COMPLETION_DEFERRED) != 0,
        memory_order_release);
}

static int kain_async_task_is_blocked_locked(const KainAsyncTaskRecord* task) {
    /*
     * Proof: z3/proofs-experimental/async-packed-flags-blocked-mask.smt2 (unsat)
     * BLOCKED_MASK = CONTINUATION_BLOCKED|DEPENDENCY_WAIT_ACTIVE|CHILD_WAIT_ACTIVE|COMPLETION_DEFERRED
     */
    return task != NULL &&
           (task->flags & KAIN_ASYNC_FLAG_BLOCKED_MASK) != 0;
}

static void kain_async_task_mark_ready_locked(KainAsyncTaskRecord* task) {
    if (task == NULL || kain_async_task_is_terminal(task->state)) {
        return;
    }
    task->state = KAIN_TASK_STATE_READY;
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_READY, memory_order_release);
    kain_async_cond_signal(&task->cond);
}

static void kain_async_task_mark_pending_locked(KainAsyncTaskRecord* task) {
    if (task == NULL || kain_async_task_is_terminal(task->state)) {
        return;
    }
    task->state = KAIN_TASK_STATE_PENDING;
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_PENDING, memory_order_release);
}

static void kain_async_batch_drain_entry(const KainBatchQueueEntry* entry, void* user_data);

static int kain_async_schedule_task_run_locked(KainAsyncTaskRecord* task) {
    KainBatchQueueEntry entry;
    if (task == NULL || (task->flags & KAIN_ASYNC_FLAG_RUN_ENQUEUED) || kain_async_task_is_terminal(task->state)) {
        return 0;
    }
    task->flags |= KAIN_ASYNC_FLAG_RUN_ENQUEUED;
    entry.kind = KAIN_ASYNC_BATCH_OP_RUN_TASK;
    entry.arg0 = (uint64_t)task->id;
    entry.arg1 = 0u;
    entry.ptr0 = NULL;
    if (kain_batch_queue_enqueue(&g_async_batch_queue, &entry) != 0) {
        task->flags &= ~KAIN_ASYNC_FLAG_RUN_ENQUEUED;
        return -1;
    }
    return 0;
}

static int kain_async_schedule_task_completion_locked(KainAsyncTaskRecord* task) {
    KainBatchQueueEntry entry;
    if (task == NULL || (task->flags & KAIN_ASYNC_FLAG_COMPLETION_ENQUEUED) ||
        (task->flags & KAIN_ASYNC_FLAG_COMPLETION_FIRED)) {
        return 0;
    }
    task->flags |= KAIN_ASYNC_FLAG_COMPLETION_ENQUEUED;
    entry.kind = KAIN_ASYNC_BATCH_OP_COMPLETE_TASK;
    entry.arg0 = (uint64_t)task->id;
    entry.arg1 = 0u;
    entry.ptr0 = NULL;
    if (kain_batch_queue_enqueue(&g_async_batch_queue, &entry) != 0) {
        task->flags &= ~KAIN_ASYNC_FLAG_COMPLETION_ENQUEUED;
        return -1;
    }
    return 0;
}

static void kain_async_batch_begin(void) {
    kain_batch_queue_lock(&g_async_batch_queue);
}

static void kain_async_batch_end(void) {
    kain_batch_queue_unlock_and_drain(&g_async_batch_queue, kain_async_batch_drain_entry, NULL);
}

static void kain_async_sleep_for_ms(unsigned long long delay_ms) {
    kain_attrition_sleep_for_millis(delay_ms);
}

static void kain_async_set_diag(
    KainDiagnostic* diag,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail
) {
    if (!diag) {
        return;
    }

    kain_diagnostic_create(
        diag,
        KAIN_DIAG_SUBSYSTEM_ASYNC,
        severity,
        code,
        message,
        detail,
        KAIN_ASYNC_SOURCE_PATH
    );
}

static void kain_async_runtime_init_impl(void) {
    memset(g_async_tasks, 0, sizeof(g_async_tasks));
    memset(g_async_timers, 0, sizeof(g_async_timers));
    memset(g_async_task_occupancy_words, 0, sizeof(g_async_task_occupancy_words));
    memset(g_async_timer_occupancy_words, 0, sizeof(g_async_timer_occupancy_words));
    memset(g_async_task_index, 0, sizeof(g_async_task_index));
    memset(g_async_timer_index, 0, sizeof(g_async_timer_index));
    kain_async_mutex_init(&g_async_global_lock);
    kain_batch_queue_init(
        &g_async_batch_queue,
        g_async_batch_active_entries,
        g_async_batch_pending_entries,
        KAIN_ASYNC_BATCH_QUEUE_CAPACITY
    );
    g_async_next_task_id = 1;
    g_async_next_timer_id = 1;
}

#ifdef _WIN32
static BOOL CALLBACK kain_async_runtime_init_once(PINIT_ONCE init_once, PVOID parameter, PVOID* context) {
    (void)init_once;
    (void)parameter;
    (void)context;
    kain_async_runtime_init_impl();
    return TRUE;
}
#else
static void kain_async_runtime_init_once(void) {
    kain_async_runtime_init_impl();
}
#endif

static void kain_async_ensure_initialized(void) {
#ifdef _WIN32
    InitOnceExecuteOnce(&g_async_init_once, kain_async_runtime_init_once, NULL, NULL);
#else
    pthread_once(&g_async_init_once, kain_async_runtime_init_once);
#endif
}

/*
 * Proofs:
 * - runtime/native/src/core/z3/proofs-experimental/async-handle-index-probe-bounds.smt2
 * - runtime/native/src/core/z3/proofs-experimental/actor-table-debruijn-hash-distinct.smt2
 *
 * The solver owns the index math for both task and timer registries: masked
 * probes must stay in bounds, and the one-hot low-bit decoder is shared with
 * the already-proved actor occupancy path.
 */
static uint64_t kain_async_mix_id(uint64_t id) {
    uint64_t x = id;
    x ^= x >> 30u;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27u;
    x *= UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31u;
    return x;
}

static uint64_t kain_async_isolate_low_bit_u64(uint64_t value) {
    return value & (0u - value);
}

static unsigned int kain_async_low_bit_index_u64(uint64_t one_hot) {
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
    return debruijn_index[(one_hot * UINT64_C(0x03f79d71b4cb0a89)) >> 58u];
}

static uint32_t kain_async_index_start_slot(uint64_t id, uint32_t mask) {
    return (uint32_t)(kain_async_mix_id(id) & mask);
}

static int kain_async_index_insert(
    uint32_t* index_table,
    uint32_t index_capacity,
    uint32_t index_mask,
    uint64_t id,
    uint32_t slot
) {
    uint32_t start_index = kain_async_index_start_slot(id, index_mask);
    /* Z3-PROVED: encoded_slot != 0 for all valid slots [0, MAX_TASKS-1].
     * sentinel=0 collision is impossible since slot+1 in [1,256] can never
     * reach 0 via u32 addition when slot < KAIN_ASYNC_MAX_TASKS=256.
     * Proof: z3/proofs/async-encoded-slot-no-sentinel-collision.yaml (unsat) */
    uint32_t encoded_slot = slot + 1u;
    uint32_t probe;
    /* Z3-PROVED: (start_index + probe) & index_mask always in [0, capacity-1]
     * for any u32 start/probe when capacity is a power-of-two (enforced by
     * compile-time #if at async.c:40-44). Bitwise AND with mask cannot exceed mask.
     * Proof: z3/proofs/async-index-probe-candidate-always-in-bounds.yaml (unsat) */
    for (probe = 0u; probe < index_capacity; ++probe) {
        uint32_t candidate_index = (start_index + probe) & index_mask;
        uint32_t candidate = index_table[candidate_index];
        if (candidate == 0u || candidate == encoded_slot) {
            index_table[candidate_index] = encoded_slot;
            return 1;
        }
    }
    return 0;
}

static int kain_async_find_free_task_slot(uint32_t* out_slot) {
    uint32_t word_index;
    if (out_slot == 0) {
        return 0;
    }
    for (word_index = 0u; word_index < KAIN_ASYNC_TASK_WORD_COUNT; ++word_index) {
        uint64_t free_mask = ~g_async_task_occupancy_words[word_index];
        if (free_mask != 0u) {
        /* Z3-PROVED: word_index * 64 + bit_index in [0, MAX_TASKS-1] = [0, 255]
         * for word_index in [0,3] and bit_index in [0,63]. Cannot OOB the task table.
         * Proof: z3/proofs/async-free-slot-index-within-max-tasks.yaml (unsat) */
            *out_slot = word_index * KAIN_ASYNC_SLOT_WORD_BITS + (uint32_t)kain_async_low_bit_index_u64(
                kain_async_isolate_low_bit_u64(free_mask)
            );
            return 1;
        }
    }
    return 0;
}

static int kain_async_find_free_timer_slot(uint32_t* out_slot) {
    uint32_t word_index;
    if (out_slot == 0) {
        return 0;
    }
    for (word_index = 0u; word_index < KAIN_ASYNC_TIMER_WORD_COUNT; ++word_index) {
        uint64_t free_mask = ~g_async_timer_occupancy_words[word_index];
        if (free_mask != 0u) {
            *out_slot = word_index * KAIN_ASYNC_SLOT_WORD_BITS + (uint32_t)kain_async_low_bit_index_u64(
                kain_async_isolate_low_bit_u64(free_mask)
            );
            return 1;
        }
    }
    return 0;
}

static void kain_async_rebuild_task_index(void) {
    uint32_t slot;
    memset(g_async_task_index, 0, sizeof(g_async_task_index));
    for (slot = 0u; slot < KAIN_ASYNC_MAX_TASKS; ++slot) {
        if (g_async_tasks[slot].flags & KAIN_ASYNC_FLAG_IN_USE) {
            (void)kain_async_index_insert(
                g_async_task_index,
                KAIN_ASYNC_TASK_INDEX_CAPACITY,
                KAIN_ASYNC_TASK_INDEX_MASK,
                (uint64_t)g_async_tasks[slot].id,
                slot
            );
        }
    }
}

static void kain_async_rebuild_timer_index(void) {
    uint32_t slot;
    memset(g_async_timer_index, 0, sizeof(g_async_timer_index));
    for (slot = 0u; slot < KAIN_ASYNC_MAX_TIMERS; ++slot) {
        if (g_async_timers[slot].in_use) {  /* timer uses its own in_use, not packed */
            (void)kain_async_index_insert(
                g_async_timer_index,
                KAIN_ASYNC_TIMER_INDEX_CAPACITY,
                KAIN_ASYNC_TIMER_INDEX_MASK,
                (uint64_t)g_async_timers[slot].id,
                slot
            );
        }
    }
}

static KainAsyncTaskRecord* kain_async_find_task_locked(KainTaskId task_id) {
    uint32_t start_index;
    uint32_t probe;

    if (task_id == KAIN_TASK_ID_INVALID) {
        return NULL;
    }

    start_index = kain_async_index_start_slot((uint64_t)task_id, KAIN_ASYNC_TASK_INDEX_MASK);
    for (probe = 0u; probe < KAIN_ASYNC_TASK_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & KAIN_ASYNC_TASK_INDEX_MASK;
        uint32_t encoded_slot = g_async_task_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            break;
        }
        slot = encoded_slot - 1u;
        if (slot < KAIN_ASYNC_MAX_TASKS &&
            (g_async_tasks[slot].flags & KAIN_ASYNC_FLAG_IN_USE) &&
            g_async_tasks[slot].id == task_id) {
            return &g_async_tasks[slot];
        }
    }

    return NULL;
}

static KainAsyncTimerRecord* kain_async_find_timer_locked(KainTimerId timer_id) {
    uint32_t start_index;
    uint32_t probe;

    if (timer_id == KAIN_TIMER_ID_INVALID) {
        return NULL;
    }

    start_index = kain_async_index_start_slot((uint64_t)timer_id, KAIN_ASYNC_TIMER_INDEX_MASK);
    for (probe = 0u; probe < KAIN_ASYNC_TIMER_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & KAIN_ASYNC_TIMER_INDEX_MASK;
        uint32_t encoded_slot = g_async_timer_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            break;
        }
        slot = encoded_slot - 1u;
        if (slot < KAIN_ASYNC_MAX_TIMERS &&
            g_async_timers[slot].in_use &&
            g_async_timers[slot].id == timer_id) {
            return &g_async_timers[slot];
        }
    }

    return NULL;
}

static void kain_async_destroy_task_sync_primitives(KainAsyncTaskRecord* task) {
    if (task == NULL) {
        return;
    }
#ifdef _WIN32
    DeleteCriticalSection(&task->lock);
#else
    pthread_cond_destroy(&task->cond);
    pthread_mutex_destroy(&task->lock);
#endif
}

static void kain_async_release_task_record_locked(KainAsyncTaskRecord* task) {
    uint32_t slot;
    uint64_t bit;
    KainAsyncTaskRef released_ref;
    uint32_t scan_slot;
    if (task == NULL || !(task->flags & KAIN_ASYNC_FLAG_IN_USE)) {
        return;
    }
    slot = (uint32_t)(task - g_async_tasks);
    bit = UINT64_C(1) << (slot & 63u);
    released_ref = kain_async_task_ref_from_task(task);
    if (task->sleep_state.timer_id != KAIN_TIMER_ID_INVALID) {
        KainAsyncTimerRecord* timer = kain_async_find_timer_locked(task->sleep_state.timer_id);
        if (timer != NULL && timer->wake_handle == &task->handle) {
            atomic_store_explicit(&timer->cancelled, 1, memory_order_release);
        }
        task->sleep_state.timer_id = KAIN_TIMER_ID_INVALID;
        task->sleep_state.armed = 0;
    }
    task->handle.owner = NULL;
    task->handle.id = KAIN_TASK_ID_INVALID;
    if (task->result != NULL) {
        kain_task_result_cleanup(task->result);
        task->result = NULL;
    }
    for (scan_slot = 0u; scan_slot < KAIN_ASYNC_MAX_TASKS; ++scan_slot) {
        KainAsyncTaskRecord* other = &g_async_tasks[scan_slot];
        if (!(other->flags & KAIN_ASYNC_FLAG_IN_USE) || other == task) {
            continue;
        }
        kain_async_mutex_lock(&other->lock);
        if (kain_async_task_ref_is_valid(other->parent_ref) &&
            other->parent_ref.slot == released_ref.slot &&
            other->parent_ref.id == released_ref.id) {
            other->parent_ref = kain_async_task_ref_invalid();
        }
        if (kain_async_task_ref_is_valid(other->continuation_ref) &&
            other->continuation_ref.slot == released_ref.slot &&
            other->continuation_ref.id == released_ref.id) {
            other->continuation_ref = kain_async_task_ref_invalid();
            other->flags &= ~KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED;
        }
        if (kain_async_task_bitset_clear(other->wait_dependency_bits, slot)) {
            if (other->dependency_wait_count != 0u) {
                other->dependency_wait_count -= 1u;
            }
            if (other->dependency_wait_count == 0u) {
                other->flags &= ~KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE;
            }
        }
        if (kain_async_task_bitset_clear(other->live_child_bits, slot) &&
            other->live_child_count != 0u) {
            other->live_child_count -= 1u;
            if ((other->flags & KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE) &&
                (other->live_child_count == 0u ||
                 (other->child_wait_mode == KAIN_TASK_WAIT_MODE_ANY &&
                  other->completed_child_count != 0u))) {
                other->flags &= ~KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE;
            }
        }
        kain_async_task_sync_runtime_flags_locked(other);
        kain_async_mutex_unlock(&other->lock);
    }
    g_async_task_occupancy_words[slot / KAIN_ASYNC_SLOT_WORD_BITS] &= ~bit;
    kain_async_destroy_task_sync_primitives(task);
    memset(task, 0, sizeof(*task));
    kain_async_rebuild_task_index();
}

static void kain_async_release_timer_record_locked(KainAsyncTimerRecord* timer) {
    uint32_t slot;
    uint64_t bit;
    if (timer == NULL || !timer->in_use) {
        return;
    }
    slot = (uint32_t)(timer - g_async_timers);
    bit = UINT64_C(1) << (slot & 63u);
    g_async_timer_occupancy_words[slot / KAIN_ASYNC_SLOT_WORD_BITS] &= ~bit;
    memset(timer, 0, sizeof(*timer));
    kain_async_rebuild_timer_index();
}

static void kain_async_release_timer_record(KainAsyncTimerRecord* timer) {
    KainTimerId timer_id;
    if (timer == NULL || !timer->in_use) {
        return;
    }
    timer_id = timer->id;
    kain_async_mutex_lock(&g_async_global_lock);
    if (timer->in_use && timer->id == timer_id) {
        kain_async_release_timer_record_locked(timer);
    }
    kain_async_mutex_unlock(&g_async_global_lock);
    atomic_fetch_sub_explicit(&g_attrition_async_timer_live_count, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_attrition_async_timer_exit_count, 1u, memory_order_relaxed);
    kain_attrition_note_async_timer_exit((uint64_t)timer_id);
}

static KainAsyncTaskRecord* kain_async_find_task(KainTaskId task_id) {
    KainAsyncTaskRecord* record = NULL;

    if (task_id == KAIN_TASK_ID_INVALID) {
        return NULL;
    }

    kain_async_ensure_initialized();
    kain_async_mutex_lock(&g_async_global_lock);
    record = kain_async_find_task_locked(task_id);
    kain_async_mutex_unlock(&g_async_global_lock);
    return record;
}

static KainAsyncTaskRecord* kain_async_allocate_task_record(void) {
    uint32_t slot;
    uint64_t bit;
    KainAsyncTaskRecord* record = NULL;

    kain_async_ensure_initialized();
    kain_async_mutex_lock(&g_async_global_lock);
    if (kain_async_find_free_task_slot(&slot)) {
        record = &g_async_tasks[slot];
        memset(record, 0, sizeof(*record));
        record->flags = KAIN_ASYNC_FLAG_IN_USE;
        record->id = g_async_next_task_id++;
        record->state = KAIN_TASK_STATE_READY;
        record->handle.id = record->id;
        record->handle.owner = record;
        record->parent_ref = kain_async_task_ref_invalid();
        record->continuation_ref = kain_async_task_ref_invalid();
        record->dependency_wait_mode = KAIN_TASK_WAIT_MODE_ALL;
        record->child_wait_mode = KAIN_TASK_WAIT_MODE_ALL;
        kain_async_mutex_init(&record->lock);
        kain_async_cond_init(&record->cond);

        atomic_init(&record->runtime_state.poll_count, 0);
        atomic_init(&record->runtime_state.wake_count, 0);
        atomic_init(&record->runtime_state.timer_count, 0);
        atomic_init(&record->runtime_state.child_wait_count, 0);
        atomic_init(&record->runtime_state.dependency_wait_count, 0);
        atomic_init(&record->runtime_state.wake_requested, 0);
        atomic_init(&record->runtime_state.timer_fired, 0);
        atomic_init(&record->runtime_state.cancelled, 0);
        atomic_init(&record->runtime_state.continuation_blocked, 0);
        atomic_init(&record->runtime_state.completion_deferred, 0);
        atomic_init(&record->runtime_state.state_snapshot, KAIN_TASK_STATE_READY);
        record->runtime_state.task_id = record->id;

        record->future_context.wake_handle = &record->handle;
        record->future_context.runtime_data = &record->runtime_state;
        bit = UINT64_C(1) << (slot & 63u);
        g_async_task_occupancy_words[slot / KAIN_ASYNC_SLOT_WORD_BITS] |= bit;
        if (!kain_async_index_insert(
                g_async_task_index,
                KAIN_ASYNC_TASK_INDEX_CAPACITY,
                KAIN_ASYNC_TASK_INDEX_MASK,
                (uint64_t)record->id,
                slot)) {
            g_async_task_occupancy_words[slot / KAIN_ASYNC_SLOT_WORD_BITS] &= ~bit;
            kain_async_destroy_task_sync_primitives(record);
            memset(record, 0, sizeof(*record));
            record = NULL;
        } else {
            uint64_t live_count = atomic_fetch_add_explicit(
                                      &g_attrition_async_task_live_count,
                                      1u,
                                      memory_order_relaxed) + 1u;
            atomic_fetch_add_explicit(&g_attrition_async_task_spawn_count, 1u, memory_order_relaxed);
            kain_async_attrition_update_peak(&g_attrition_async_task_peak_count, live_count);
        }
    }
    kain_async_mutex_unlock(&g_async_global_lock);
    if (record != NULL) {
        kain_attrition_note_async_task_spawn((uint64_t)record->id);
    }
    return record;
}

static KainAsyncTimerRecord* kain_async_allocate_timer_record(void) {
    uint32_t slot;
    uint64_t bit;
    KainAsyncTimerRecord* record = NULL;

    kain_async_ensure_initialized();
    kain_async_mutex_lock(&g_async_global_lock);
    if (kain_async_find_free_timer_slot(&slot)) {
        record = &g_async_timers[slot];
        memset(record, 0, sizeof(*record));
        record->in_use = 1;
        record->id = g_async_next_timer_id++;
        atomic_init(&record->cancelled, 0);
        atomic_init(&record->fired, 0);
        atomic_init(&record->started, 0);
        bit = UINT64_C(1) << (slot & 63u);
        g_async_timer_occupancy_words[slot / KAIN_ASYNC_SLOT_WORD_BITS] |= bit;
        {
            uint64_t live_count = atomic_fetch_add_explicit(
                                      &g_attrition_async_timer_live_count,
                                      1u,
                                      memory_order_relaxed) + 1u;
            atomic_fetch_add_explicit(&g_attrition_async_timer_spawn_count, 1u, memory_order_relaxed);
            kain_async_attrition_update_peak(&g_attrition_async_timer_peak_count, live_count);
        }
    }
    kain_async_mutex_unlock(&g_async_global_lock);
    if (record != NULL) {
        kain_attrition_note_async_timer_spawn((uint64_t)record->id);
    }
    return record;
}

static KainAsyncTimerRecord* kain_async_find_timer(KainTimerId timer_id) {
    KainAsyncTimerRecord* record = NULL;

    if (timer_id == KAIN_TIMER_ID_INVALID) {
        return NULL;
    }

    kain_async_ensure_initialized();
    kain_async_mutex_lock(&g_async_global_lock);
    record = kain_async_find_timer_locked(timer_id);
    kain_async_mutex_unlock(&g_async_global_lock);
    return record;
}

static KainPollResult kain_async_execute_task(KainAsyncTaskRecord* task, void** result, KainDiagnostic* diag);

static int kain_async_task_signal_handle(void* wake_handle, KainDiagnostic* diag);

static KainPollResult kain_async_sleep_task_fn(
    KainFutureContext* context,
    void* user_data,
    void** result
) {
    KainAsyncSleepState* sleep_state = (KainAsyncSleepState*)user_data;
    KainTaskRuntimeState* runtime_state = (KainTaskRuntimeState*)context->runtime_data;

    if (!sleep_state || !runtime_state) {
        return KAIN_POLL_ERROR;
    }

    if (!sleep_state->armed) {
        sleep_state->timer_id = kain_timer_register(sleep_state->delay_ms, context->wake_handle, NULL);
        if (sleep_state->timer_id == KAIN_TIMER_ID_INVALID) {
            return KAIN_POLL_ERROR;
        }
        sleep_state->armed = 1;
        return KAIN_POLL_PENDING;
    }

    if (atomic_load_explicit(&runtime_state->timer_fired, memory_order_acquire) == 0) {
        return KAIN_POLL_PENDING;
    }

    if (result) {
        *result = NULL;
    }
    return KAIN_POLL_READY;
}

static int kain_async_task_is_terminal(KainTaskState state) {
    /*
     * Proof: z3/proofs-experimental/async-terminal-state-branchless.smt2 (unsat)
     * Terminal states: COMPLETED=3, CANCELLED=4, FAILED=5.
     * All > RUNNING=2. Single compare replaces 3-way OR.
     */
    return state > KAIN_TASK_STATE_RUNNING;
}

static void kain_async_complete_task_locked(
    KainAsyncTaskRecord* task,
    KainTaskState final_state,
    void* produced_result,
    void** result
) {
    if (task == NULL) {
        return;
    }
    if (final_state == KAIN_TASK_STATE_CANCELLED || final_state == KAIN_TASK_STATE_FAILED) {
        if (produced_result != NULL) {
            kain_task_result_cleanup(produced_result);
            produced_result = NULL;
        }
        if (task->result != NULL) {
            kain_task_result_cleanup(task->result);
            task->result = NULL;
        }
    } else {
        task->result = produced_result;
    }
    task->flags &= ~(KAIN_ASYNC_FLAG_COMPLETION_DEFERRED |
                      KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE |
                      KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE |
                      KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED);
    task->state = final_state;
    kain_async_task_sync_runtime_flags_locked(task);
    atomic_store_explicit(&task->runtime_state.state_snapshot, final_state, memory_order_release);
    if (result != NULL) {
        *result = final_state == KAIN_TASK_STATE_COMPLETED ? task->result : NULL;
    }
    kain_async_cond_signal(&task->cond);
    (void)kain_async_schedule_task_completion_locked(task);
}

static KainPollResult kain_async_execute_task(KainAsyncTaskRecord* task, void** result, KainDiagnostic* diag) {
    KainPollResult poll_result;
    void* produced_result = NULL;
    int wake_requested = 0;

    if (!task || !task->config.task_fn) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async task is not runnable",
            "The task record or task function was missing."
        );
        return KAIN_POLL_ERROR;
    }

    kain_async_mutex_lock(&task->lock);
    if (task->state == KAIN_TASK_STATE_COMPLETED) {
        if (result) {
            *result = task->result;
        }
        kain_async_mutex_unlock(&task->lock);
        return KAIN_POLL_READY;
    }

    if (task->state == KAIN_TASK_STATE_FAILED) {
        kain_async_mutex_unlock(&task->lock);
        if (result) {
            *result = NULL;
        }
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_WAKE_FAILED,
            "Async task failed",
            "The task record is already in a failed state."
        );
        return KAIN_POLL_ERROR;
    }

    if (task->state == KAIN_TASK_STATE_RUNNING) {
        if (result) {
            *result = NULL;
        }
        kain_async_mutex_unlock(&task->lock);
        return KAIN_POLL_PENDING;
    }

    if (((task->flags & KAIN_ASYNC_FLAG_CANCEL_REQUESTED) ||
         atomic_load_explicit(&task->runtime_state.cancelled, memory_order_acquire) != 0) &&
        task->state != KAIN_TASK_STATE_RUNNING) {
        kain_async_complete_task_locked(task, KAIN_TASK_STATE_CANCELLED, NULL, result);
        kain_async_mutex_unlock(&task->lock);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED,
            "Async task cancelled",
            "The task was cancelled before it could continue executing."
        );
        return KAIN_POLL_ERROR;
    }

    if ((task->flags & KAIN_ASYNC_FLAG_COMPLETION_DEFERRED) &&
        !(task->flags & KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE)) {
        kain_async_complete_task_locked(task, KAIN_TASK_STATE_COMPLETED, task->result, result);
        kain_async_mutex_unlock(&task->lock);
        return KAIN_POLL_READY;
    }

    if (task->flags & KAIN_ASYNC_FLAG_BLOCKED_MASK) {
        if (result) {
            *result = NULL;
        }
        kain_async_task_mark_pending_locked(task);
        kain_async_mutex_unlock(&task->lock);
        return KAIN_POLL_PENDING;
    }

    task->state = KAIN_TASK_STATE_RUNNING;
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_RUNNING, memory_order_release);
    atomic_fetch_add_explicit(&task->runtime_state.poll_count, 1, memory_order_relaxed);
    atomic_store_explicit(&task->runtime_state.wake_requested, 0, memory_order_release);
    g_async_current_task_id = task->id;
    kain_async_mutex_unlock(&task->lock);

    poll_result = task->config.task_fn(&task->future_context, task->config.user_data, &produced_result);

    g_async_current_task_id = KAIN_TASK_ID_INVALID;
    kain_async_mutex_lock(&task->lock);

    if ((task->flags & KAIN_ASYNC_FLAG_CANCEL_REQUESTED) ||
        atomic_load_explicit(&task->runtime_state.cancelled, memory_order_acquire) != 0) {
        task->flags |= KAIN_ASYNC_FLAG_CANCEL_REQUESTED;
        atomic_store_explicit(&task->runtime_state.cancelled, 1, memory_order_release);
        kain_async_complete_task_locked(task, KAIN_TASK_STATE_CANCELLED, produced_result, result);
        kain_async_mutex_unlock(&task->lock);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED,
            "Async task cancelled",
            "The task was cancelled while it was executing."
        );
        return KAIN_POLL_ERROR;
    }

    if (poll_result == KAIN_POLL_READY) {
        if (task->flags & KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE) {
            task->result = produced_result;
            task->flags |= KAIN_ASYNC_FLAG_COMPLETION_DEFERRED;
            kain_async_task_sync_runtime_flags_locked(task);
            if (result) {
                *result = NULL;
            }
            kain_async_task_mark_pending_locked(task);
            kain_async_mutex_unlock(&task->lock);
            return KAIN_POLL_PENDING;
        }
        kain_async_complete_task_locked(task, KAIN_TASK_STATE_COMPLETED, produced_result, result);
        kain_async_mutex_unlock(&task->lock);
        return KAIN_POLL_READY;
    }

    if (poll_result == KAIN_POLL_PENDING) {
        wake_requested = atomic_load_explicit(&task->runtime_state.wake_requested, memory_order_acquire) != 0;
        if (wake_requested && !kain_async_task_is_blocked_locked(task)) {
            kain_async_task_mark_ready_locked(task);
        } else {
            kain_async_task_mark_pending_locked(task);
        }
        kain_async_task_sync_runtime_flags_locked(task);
        if (result) {
            *result = NULL;
        }
        kain_async_mutex_unlock(&task->lock);
        return KAIN_POLL_PENDING;
    }

    kain_async_complete_task_locked(task, KAIN_TASK_STATE_FAILED, produced_result, result);
    kain_async_mutex_unlock(&task->lock);
    kain_async_set_diag(
        diag,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_ASYNC_WAKE_FAILED,
        "Async task poll failed",
        "The task function returned an invalid poll result."
    );
    return KAIN_POLL_ERROR;
}

static int kain_async_task_signal_handle(void* wake_handle, KainDiagnostic* diag) {
    KainTaskHandle* handle = (KainTaskHandle*)wake_handle;
    KainAsyncTaskRecord* task = NULL;

    if (!handle || !handle->owner) {
        atomic_fetch_add_explicit(&g_attrition_async_task_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_task_stale_reject(
            (uint64_t)(handle != NULL ? handle->id : KAIN_TASK_ID_INVALID));
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_WAKE_FAILED,
            "Invalid wake handle",
            "The supplied wake handle was null or detached from a task."
        );
        return -1;
    }

    task = handle->owner;
    kain_async_mutex_lock(&task->lock);
    if (task->id != handle->id || !(task->flags & KAIN_ASYNC_FLAG_IN_USE)) {
        kain_async_mutex_unlock(&task->lock);
        atomic_fetch_add_explicit(&g_attrition_async_task_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_task_stale_reject((uint64_t)handle->id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_WAKE_FAILED,
            "Invalid wake handle",
            "The wake handle does not map to a live async task."
        );
        return -1;
    }

    if (kain_async_task_is_terminal(task->state)) {
        kain_async_mutex_unlock(&task->lock);
        return 0;
    }

    atomic_fetch_add_explicit(&task->runtime_state.wake_count, 1, memory_order_relaxed);
    atomic_store_explicit(&task->runtime_state.wake_requested, 1, memory_order_release);
    if (task->state != KAIN_TASK_STATE_RUNNING) {
        kain_async_task_mark_ready_locked(task);
        (void)kain_async_schedule_task_run_locked(task);
    }
    kain_async_cond_signal(&task->cond);
    kain_async_mutex_unlock(&task->lock);
    return 0;
}

#ifdef _WIN32
static DWORD WINAPI kain_async_timer_thread_proc(LPVOID parameter) {
    KainAsyncTimerRecord* timer = (KainAsyncTimerRecord*)parameter;
#else
static void* kain_async_timer_thread_proc(void* parameter) {
    KainAsyncTimerRecord* timer = (KainAsyncTimerRecord*)parameter;
#endif
    if (!timer) {
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    atomic_store_explicit(&timer->started, 1, memory_order_release);
    kain_async_sleep_for_ms(timer->delay_ms);

    if (atomic_load_explicit(&timer->cancelled, memory_order_acquire) != 0) {
        kain_async_release_timer_record(timer);
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    atomic_store_explicit(&timer->fired, 1, memory_order_release);
    if (timer->wake_handle != NULL) {
        KainTaskHandle* handle = (KainTaskHandle*)timer->wake_handle;
        KainAsyncTaskRecord* task = (handle != NULL) ? handle->owner : NULL;
        if (task != NULL) {
            kain_async_mutex_lock(&task->lock);
            atomic_fetch_add_explicit(&task->runtime_state.timer_count, 1, memory_order_relaxed);
            atomic_store_explicit(&task->runtime_state.timer_fired, 1, memory_order_release);
            kain_async_mutex_unlock(&task->lock);
        }
        kain_async_task_signal_handle(timer->wake_handle, NULL);
    }
    kain_async_release_timer_record(timer);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void kain_async_destroy_new_task_record(KainAsyncTaskRecord* task) {
    KainTaskId task_id;
    if (task == NULL || !(task->flags & KAIN_ASYNC_FLAG_IN_USE)) {
        return;
    }
    task_id = task->id;
    kain_async_mutex_lock(&g_async_global_lock);
    if ((task->flags & KAIN_ASYNC_FLAG_IN_USE) && task->id == task_id) {
        kain_async_release_task_record_locked(task);
    }
    kain_async_mutex_unlock(&g_async_global_lock);
    atomic_fetch_sub_explicit(&g_attrition_async_task_live_count, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_attrition_async_task_exit_count, 1u, memory_order_relaxed);
    kain_attrition_note_async_task_exit((uint64_t)task_id);
}

static void kain_async_handle_task_completion(KainTaskId task_id) {
    KainAsyncTaskRecord* task = NULL;
    KainTaskCompletionCallback completion_callback = NULL;
    void* completion_user_data = NULL;
    void* completion_result = NULL;
    KainTaskState final_state = KAIN_TASK_STATE_FAILED;
    KainAsyncTaskRef completed_ref = kain_async_task_ref_invalid();
    uint32_t completed_slot = KAIN_ASYNC_REF_INVALID_SLOT;
    uint32_t slot;

    kain_async_mutex_lock(&g_async_global_lock);
    task = kain_async_find_task_locked(task_id);
    if (task == NULL) {
        kain_async_mutex_unlock(&g_async_global_lock);
        return;
    }
    kain_async_mutex_lock(&task->lock);
    if (!kain_async_task_is_terminal(task->state) ||
        (task->flags & KAIN_ASYNC_FLAG_COMPLETION_FIRED)) {
        task->flags &= ~KAIN_ASYNC_FLAG_COMPLETION_ENQUEUED;
        kain_async_mutex_unlock(&task->lock);
        kain_async_mutex_unlock(&g_async_global_lock);
        return;
    }
    task->flags |= KAIN_ASYNC_FLAG_COMPLETION_FIRED;
    task->flags &= ~KAIN_ASYNC_FLAG_COMPLETION_ENQUEUED;
    completion_callback = task->completion_callback;
    completion_user_data = task->completion_user_data;
    completion_result = task->result;
    final_state = task->state;
    completed_ref = kain_async_task_ref_from_task(task);
    completed_slot = kain_async_task_slot(task);
    kain_async_mutex_unlock(&task->lock);
    for (slot = 0u; slot < KAIN_ASYNC_MAX_TASKS; ++slot) {
        KainAsyncTaskRecord* other = &g_async_tasks[slot];
        int touched = 0;
        if (!(other->flags & KAIN_ASYNC_FLAG_IN_USE) || other == task) {
            continue;
        }
        kain_async_mutex_lock(&other->lock);

        if (kain_async_task_bitset_clear(other->live_child_bits, completed_slot)) {
            if (other->live_child_count != 0u) {
                other->live_child_count -= 1u;
            }
            other->completed_child_count += 1u;
            if ((other->flags & KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE) &&
                (other->child_wait_mode == KAIN_TASK_WAIT_MODE_ANY ||
                 other->live_child_count == 0u)) {
                other->flags &= ~KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE;
            }
            touched = 1;
        }

        if (kain_async_task_ref_is_valid(other->continuation_ref) &&
            other->continuation_ref.slot == completed_ref.slot &&
            other->continuation_ref.id == completed_ref.id) {
            other->continuation_ref = kain_async_task_ref_invalid();
            other->flags &= ~KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED;
            touched = 1;
        }

        if ((other->flags & KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE) &&
            kain_async_task_bitset_test(other->wait_dependency_bits, completed_slot)) {
            if (other->dependency_wait_mode == KAIN_TASK_WAIT_MODE_ANY) {
                memset(other->wait_dependency_bits, 0, sizeof(other->wait_dependency_bits));
                other->dependency_wait_count = 0u;
                other->flags &= ~KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE;
            } else {
                if (kain_async_task_bitset_clear(other->wait_dependency_bits, completed_slot) &&
                    other->dependency_wait_count != 0u) {
                    other->dependency_wait_count -= 1u;
                }
                if (other->dependency_wait_count == 0u) {
                    other->flags &= ~KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE;
                }
            }
            touched = 1;
        }

        if (touched) {
            kain_async_task_sync_runtime_flags_locked(other);
            if ((other->flags & KAIN_ASYNC_FLAG_COMPLETION_DEFERRED) &&
                !(other->flags & KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE)) {
                kain_async_complete_task_locked(other, KAIN_TASK_STATE_COMPLETED, other->result, NULL);
            } else if (!kain_async_task_is_terminal(other->state) &&
                       other->state != KAIN_TASK_STATE_RUNNING &&
                       ((other->flags & KAIN_ASYNC_FLAG_CANCEL_REQUESTED) ||
                        !kain_async_task_is_blocked_locked(other))) {
                kain_async_task_mark_ready_locked(other);
                (void)kain_async_schedule_task_run_locked(other);
            }
        }
        kain_async_mutex_unlock(&other->lock);
    }
    kain_async_mutex_unlock(&g_async_global_lock);

    if (completion_callback != NULL) {
        completion_callback(task_id, final_state, completion_result, completion_user_data);
    }
}

static void kain_async_batch_drain_entry(const KainBatchQueueEntry* entry, void* user_data) {
    (void)user_data;
    if (entry == NULL) {
        return;
    }

    if (entry->kind == KAIN_ASYNC_BATCH_OP_RUN_TASK) {
        KainAsyncTaskRecord* task = kain_async_find_task((KainTaskId)entry->arg0);
        if (task == NULL) {
            return;
        }
        kain_async_mutex_lock(&task->lock);
        task->flags &= ~KAIN_ASYNC_FLAG_RUN_ENQUEUED;
        kain_async_mutex_unlock(&task->lock);
        (void)kain_async_execute_task(task, NULL, NULL);
    } else if (entry->kind == KAIN_ASYNC_BATCH_OP_COMPLETE_TASK) {
        kain_async_handle_task_completion((KainTaskId)entry->arg0);
    }
}

void kain_task_spawn_config_init(KainTaskSpawnConfig* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->child_wait_mode = KAIN_TASK_WAIT_MODE_ALL;
}

int kain_task_batch_lock(KainDiagnostic* diag) {
    (void)diag;
    kain_async_ensure_initialized();
    kain_async_batch_begin();
    return 0;
}

int kain_task_batch_unlock(KainDiagnostic* diag) {
    (void)diag;
    kain_async_ensure_initialized();
    kain_async_batch_end();
    return 0;
}

int kain_task_set_completion_callback(
    KainTaskId task_id,
    KainTaskCompletionCallback completion_callback,
    void* completion_user_data,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* task = kain_async_find_task(task_id);
    if (task == NULL) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async task callback registration failed",
            "The requested task id does not exist."
        );
        return -1;
    }

    kain_async_mutex_lock(&task->lock);
    task->completion_callback = completion_callback;
    task->completion_user_data = completion_user_data;
    kain_async_mutex_unlock(&task->lock);
    return 0;
}

int kain_task_add_child(
    KainTaskId parent_task_id,
    KainTaskId child_task_id,
    KainTaskWaitMode wait_mode,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* parent = NULL;
    KainAsyncTaskRecord* child = NULL;
    int status = -1;

    if (parent_task_id == KAIN_TASK_ID_INVALID ||
        child_task_id == KAIN_TASK_ID_INVALID ||
        parent_task_id == child_task_id) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async child link failed",
            "Parent and child ids must both be live and distinct."
        );
        return -1;
    }

    kain_async_ensure_initialized();
    kain_async_batch_begin();
    kain_async_mutex_lock(&g_async_global_lock);
    parent = kain_async_find_task_locked(parent_task_id);
    child = kain_async_find_task_locked(child_task_id);
    if (parent == NULL || child == NULL) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async child link failed",
            "The parent or child task no longer exists."
        );
        goto finish;
    }

    if (parent < child) {
        kain_async_mutex_lock(&parent->lock);
        kain_async_mutex_lock(&child->lock);
    } else {
        kain_async_mutex_lock(&child->lock);
        kain_async_mutex_lock(&parent->lock);
    }

    if (kain_async_task_is_terminal(parent->state)) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async child link failed",
            "Cannot attach a child to a terminal parent task."
        );
        goto finish_with_locks;
    }

    if (kain_async_task_ref_is_valid(child->parent_ref) &&
        (child->parent_ref.id != parent->id ||
         child->parent_ref.slot != kain_async_task_slot(parent))) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async child link failed",
            "The child is already attached to a different parent."
        );
        goto finish_with_locks;
    }

    child->parent_ref = kain_async_task_ref_from_task(parent);
    parent->child_wait_mode = wait_mode;
    if (!kain_async_task_is_terminal(child->state)) {
        if (kain_async_task_bitset_set(parent->live_child_bits, kain_async_task_slot(child))) {
            parent->live_child_count += 1u;
        }
        parent->flags = (parent->flags & ~KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE) |
            (wait_mode == KAIN_TASK_WAIT_MODE_ANY
                ? ((parent->completed_child_count == 0u && parent->live_child_count != 0u)
                   ? KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE : 0u)
                : (parent->live_child_count != 0u
                   ? KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE : 0u));
        if ((parent->flags & KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE) && parent->state != KAIN_TASK_STATE_RUNNING) {
            kain_async_task_mark_pending_locked(parent);
        }
    } else {
        parent->completed_child_count += 1u;
        if (wait_mode == KAIN_TASK_WAIT_MODE_ANY) {
            parent->flags &= ~KAIN_ASYNC_FLAG_CHILD_WAIT_ACTIVE;
        }
    }
    kain_async_task_sync_runtime_flags_locked(parent);
    status = 0;

finish_with_locks:
    if (parent < child) {
        kain_async_mutex_unlock(&child->lock);
        kain_async_mutex_unlock(&parent->lock);
    } else {
        kain_async_mutex_unlock(&parent->lock);
        kain_async_mutex_unlock(&child->lock);
    }

finish:
    kain_async_mutex_unlock(&g_async_global_lock);
    kain_async_batch_end();
    return status;
}

int kain_task_add_continuation(
    KainTaskId antecedent_task_id,
    KainTaskId continuation_task_id,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* antecedent = NULL;
    KainAsyncTaskRecord* continuation = NULL;
    KainTaskId inherited_parent_id = KAIN_TASK_ID_INVALID;
    int antecedent_terminal = 0;
    int status = -1;

    if (antecedent_task_id == KAIN_TASK_ID_INVALID ||
        continuation_task_id == KAIN_TASK_ID_INVALID ||
        antecedent_task_id == continuation_task_id) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async continuation link failed",
            "Antecedent and continuation ids must both be live and distinct."
        );
        return -1;
    }

    kain_async_ensure_initialized();
    kain_async_batch_begin();
    kain_async_mutex_lock(&g_async_global_lock);
    antecedent = kain_async_find_task_locked(antecedent_task_id);
    continuation = kain_async_find_task_locked(continuation_task_id);
    if (antecedent == NULL || continuation == NULL) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async continuation link failed",
            "The antecedent or continuation task no longer exists."
        );
        goto finish_continuation;
    }

    if (antecedent < continuation) {
        kain_async_mutex_lock(&antecedent->lock);
        kain_async_mutex_lock(&continuation->lock);
    } else {
        kain_async_mutex_lock(&continuation->lock);
        kain_async_mutex_lock(&antecedent->lock);
    }

    if (kain_async_task_ref_is_valid(continuation->continuation_ref) &&
        (continuation->continuation_ref.id != antecedent->id ||
         continuation->continuation_ref.slot != kain_async_task_slot(antecedent))) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async continuation link failed",
            "The continuation is already chained to a different antecedent."
        );
        goto finish_continuation_with_locks;
    }

    continuation->continuation_ref = kain_async_task_ref_from_task(antecedent);
    antecedent_terminal = kain_async_task_is_terminal(antecedent->state);
    if (antecedent_terminal) {
        continuation->flags &= ~KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED;
    } else {
        continuation->flags |= KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED;
    }
    if (continuation->flags & KAIN_ASYNC_FLAG_CONTINUATION_BLOCKED) {
        kain_async_task_mark_pending_locked(continuation);
    } else if (!kain_async_task_is_terminal(continuation->state) &&
               continuation->state != KAIN_TASK_STATE_RUNNING &&
               !kain_async_task_is_blocked_locked(continuation)) {
        kain_async_task_mark_ready_locked(continuation);
        (void)kain_async_schedule_task_run_locked(continuation);
    }
    kain_async_task_sync_runtime_flags_locked(continuation);
    if (!kain_async_task_ref_is_valid(continuation->parent_ref) &&
        kain_async_task_ref_is_valid(antecedent->parent_ref)) {
        inherited_parent_id = antecedent->parent_ref.id;
    }
    status = 0;

finish_continuation_with_locks:
    if (antecedent < continuation) {
        kain_async_mutex_unlock(&continuation->lock);
        kain_async_mutex_unlock(&antecedent->lock);
    } else {
        kain_async_mutex_unlock(&antecedent->lock);
        kain_async_mutex_unlock(&continuation->lock);
    }

finish_continuation:
    kain_async_mutex_unlock(&g_async_global_lock);
    if (status == 0 && inherited_parent_id != KAIN_TASK_ID_INVALID) {
        status = kain_task_add_child(
            inherited_parent_id,
            continuation_task_id,
            KAIN_TASK_WAIT_MODE_ALL,
            diag
        );
    }
    kain_async_batch_end();
    return status;
}

int kain_task_add_wait_dependencies(
    KainTaskId waiter_task_id,
    const KainTaskId* dependency_task_ids,
    size_t dependency_task_count,
    KainTaskWaitMode wait_mode,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* waiter = NULL;
    int status = -1;
    int any_terminal = 0;
    size_t index;

    if (waiter_task_id == KAIN_TASK_ID_INVALID ||
        dependency_task_ids == NULL ||
        dependency_task_count == 0u) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async dependency link failed",
            "A waiter and at least one dependency task id are required."
        );
        return -1;
    }

    kain_async_ensure_initialized();
    kain_async_batch_begin();
    kain_async_mutex_lock(&g_async_global_lock);
    waiter = kain_async_find_task_locked(waiter_task_id);
    if (waiter == NULL) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async dependency link failed",
            "The waiter task no longer exists."
        );
        goto finish_wait;
    }

    kain_async_mutex_lock(&waiter->lock);
    memset(waiter->wait_dependency_bits, 0, sizeof(waiter->wait_dependency_bits));
    waiter->dependency_wait_mode = wait_mode;
    waiter->dependency_wait_count = 0u;
    waiter->flags &= ~KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE;

    for (index = 0u; index < dependency_task_count; ++index) {
        KainAsyncTaskRecord* dependency;
        int dependency_terminal = 0;
        if (dependency_task_ids[index] == KAIN_TASK_ID_INVALID ||
            dependency_task_ids[index] == waiter_task_id) {
            continue;
        }
        dependency = kain_async_find_task_locked(dependency_task_ids[index]);
        if (dependency == NULL) {
            continue;
        }
        kain_async_mutex_lock(&dependency->lock);
        dependency_terminal = kain_async_task_is_terminal(dependency->state);
        kain_async_mutex_unlock(&dependency->lock);
        if (dependency_terminal) {
            any_terminal = 1;
            if (wait_mode == KAIN_TASK_WAIT_MODE_ANY) {
                break;
            }
            continue;
        }
        if (kain_async_task_bitset_set(waiter->wait_dependency_bits, kain_async_task_slot(dependency))) {
            waiter->dependency_wait_count += 1u;
        }
    }

    if (wait_mode == KAIN_TASK_WAIT_MODE_ANY && any_terminal) {
        memset(waiter->wait_dependency_bits, 0, sizeof(waiter->wait_dependency_bits));
        waiter->dependency_wait_count = 0u;
        waiter->flags &= ~KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE;
    } else {
        if (waiter->dependency_wait_count != 0u) {
            waiter->flags |= KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE;
        } else {
            waiter->flags &= ~KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE;
        }
    }
    if (waiter->flags & KAIN_ASYNC_FLAG_DEPENDENCY_WAIT_ACTIVE) {
        kain_async_task_mark_pending_locked(waiter);
    } else if (!kain_async_task_is_terminal(waiter->state) &&
               waiter->state != KAIN_TASK_STATE_RUNNING &&
               !kain_async_task_is_blocked_locked(waiter)) {
        kain_async_task_mark_ready_locked(waiter);
        (void)kain_async_schedule_task_run_locked(waiter);
    }
    kain_async_task_sync_runtime_flags_locked(waiter);
    kain_async_mutex_unlock(&waiter->lock);
    status = 0;

finish_wait:
    kain_async_mutex_unlock(&g_async_global_lock);
    kain_async_batch_end();
    return status;
}

KainTaskId kain_task_spawn(
    const KainTaskSpawnConfig* config,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* task;
    KainTaskId task_id;

    kain_async_ensure_initialized();
    if (!config || !config->task_fn) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async task spawn failed",
            "The task configuration was missing a task function."
        );
        return KAIN_TASK_ID_INVALID;
    }

    task = kain_async_allocate_task_record();
    if (!task) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async task table full",
            "No additional async tasks can be allocated."
        );
        return KAIN_TASK_ID_INVALID;
    }

    kain_async_mutex_lock(&task->lock);
    task->config = *config;
    task->state = KAIN_TASK_STATE_READY;
    task->flags &= ~KAIN_ASYNC_FLAG_CANCEL_REQUESTED;
    task->result = NULL;
    task->dependency_wait_mode = KAIN_TASK_WAIT_MODE_ALL;
    task->child_wait_mode = config->child_wait_mode;
    task->completion_callback = config->completion_callback;
    task->completion_user_data = config->completion_user_data;
    kain_async_task_sync_runtime_flags_locked(task);
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_READY, memory_order_release);
    task_id = task->id;
    kain_async_mutex_unlock(&task->lock);

    kain_async_batch_begin();
    if (config->parent_task_id != KAIN_TASK_ID_INVALID &&
        kain_task_add_child(config->parent_task_id, task_id, config->child_wait_mode, diag) != 0) {
        kain_async_batch_end();
        kain_async_destroy_new_task_record(task);
        return KAIN_TASK_ID_INVALID;
    }
    if (config->continuation_of_task_id != KAIN_TASK_ID_INVALID &&
        kain_task_add_continuation(config->continuation_of_task_id, task_id, diag) != 0) {
        kain_async_batch_end();
        kain_async_destroy_new_task_record(task);
        return KAIN_TASK_ID_INVALID;
    }
    kain_async_mutex_lock(&task->lock);
    if (!kain_async_task_is_blocked_locked(task) && !kain_async_task_is_terminal(task->state)) {
        (void)kain_async_schedule_task_run_locked(task);
    }
    kain_async_mutex_unlock(&task->lock);
    kain_async_batch_end();
    return task_id;
}

KainPollResult kain_task_poll(
    KainTaskId task_id,
    void** result,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* task = kain_async_find_task(task_id);

    if (!task) {
        if (result) {
            *result = NULL;
        }
        atomic_fetch_add_explicit(&g_attrition_async_task_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_task_stale_reject((uint64_t)task_id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_WAKE_FAILED,
            "Async task not found",
            "The requested task id does not exist."
        );
        return KAIN_POLL_ERROR;
    }

    return kain_async_execute_task(task, result, diag);
}

int kain_task_await(
    KainTaskId task_id,
    void** result,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* task = kain_async_find_task(task_id);

    if (!task) {
        if (result) {
            *result = NULL;
        }
        atomic_fetch_add_explicit(&g_attrition_async_task_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_task_stale_reject((uint64_t)task_id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_WAKE_FAILED,
            "Async task not found",
            "The requested task id does not exist."
        );
        return -1;
    }

    for (;;) {
        KainTaskState state;
        KainPollResult poll_result;
        void* local_result = NULL;

        kain_async_mutex_lock(&task->lock);
        state = task->state;
        kain_async_mutex_unlock(&task->lock);

        if (state == KAIN_TASK_STATE_COMPLETED) {
            if (result) {
                *result = task->result;
            }
            return 0;
        }

        if (state == KAIN_TASK_STATE_CANCELLED || state == KAIN_TASK_STATE_FAILED) {
            if (result) {
                *result = NULL;
            }
            kain_async_set_diag(
                diag,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED,
                "Async task ended before await completed",
                "The task was cancelled or failed while awaiting."
            );
            return -1;
        }

        poll_result = kain_async_execute_task(task, &local_result, diag);
        if (poll_result == KAIN_POLL_READY) {
            if (result) {
                *result = local_result;
            }
            return 0;
        }

        if (poll_result == KAIN_POLL_ERROR) {
            if (result) {
                *result = NULL;
            }
            return -1;
        }

        kain_async_mutex_lock(&task->lock);
        while (task->state == KAIN_TASK_STATE_PENDING && atomic_load_explicit(&task->runtime_state.wake_requested, memory_order_acquire) == 0) {
            kain_async_cond_wait(&task->cond, &task->lock);
        }
        kain_async_mutex_unlock(&task->lock);
    }
}

int kain_task_cancel(
    KainTaskId task_id,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* task = NULL;
    int changed = 0;

    if (task_id == KAIN_TASK_ID_INVALID) {
        atomic_fetch_add_explicit(&g_attrition_async_task_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_task_stale_reject((uint64_t)task_id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED,
            "Async task not found",
            "The requested task id does not exist."
        );
        return -1;
    }

    kain_async_ensure_initialized();
    kain_async_batch_begin();
    kain_async_mutex_lock(&g_async_global_lock);
    task = kain_async_find_task_locked(task_id);
    if (task == NULL) {
        kain_async_mutex_unlock(&g_async_global_lock);
        kain_async_batch_end();
        atomic_fetch_add_explicit(&g_attrition_async_task_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_task_stale_reject((uint64_t)task_id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED,
            "Async task not found",
            "The requested task id does not exist."
        );
        return -1;
    }

    do {
        uint32_t slot;
        changed = 0;
        for (slot = 0u; slot < KAIN_ASYNC_MAX_TASKS; ++slot) {
            KainAsyncTaskRecord* other = &g_async_tasks[slot];
            KainAsyncTaskRecord* parent = NULL;
            KainAsyncTaskRecord* antecedent = NULL;
            int should_cancel = 0;
            if (!(other->flags & KAIN_ASYNC_FLAG_IN_USE)) {
                continue;
            }

            kain_async_mutex_lock(&other->lock);
            if (slot == kain_async_task_slot(task)) {
                should_cancel = 1;
            } else if (!(other->flags & KAIN_ASYNC_FLAG_CANCEL_REQUESTED)) {
                parent = kain_async_task_from_ref_locked(other->parent_ref);
                antecedent = kain_async_task_from_ref_locked(other->continuation_ref);
                should_cancel =
                    (parent != NULL &&
                     atomic_load_explicit(&parent->runtime_state.cancelled, memory_order_acquire) != 0) ||
                    (antecedent != NULL &&
                     atomic_load_explicit(&antecedent->runtime_state.cancelled, memory_order_acquire) != 0);
            }

            if (should_cancel && !(other->flags & KAIN_ASYNC_FLAG_CANCEL_REQUESTED) &&
                !kain_async_task_is_terminal(other->state)) {
                KainTimerId timer_id_to_cancel = KAIN_TIMER_ID_INVALID;
                other->flags |= KAIN_ASYNC_FLAG_CANCEL_REQUESTED;
                atomic_store_explicit(&other->runtime_state.cancelled, 1, memory_order_release);
                if (other->sleep_state.timer_id != KAIN_TIMER_ID_INVALID) {
                    timer_id_to_cancel = other->sleep_state.timer_id;
                    other->sleep_state.timer_id = KAIN_TIMER_ID_INVALID;
                    other->sleep_state.armed = 0;
                }
                if (timer_id_to_cancel != KAIN_TIMER_ID_INVALID) {
                    KainAsyncTimerRecord* timer = kain_async_find_timer_locked(timer_id_to_cancel);
                    if (timer != NULL && timer->id == timer_id_to_cancel) {
                        atomic_store_explicit(&timer->cancelled, 1, memory_order_release);
                        atomic_fetch_add_explicit(&g_attrition_async_timer_cancel_count, 1u, memory_order_relaxed);
                        kain_attrition_note_async_timer_cancel((uint64_t)timer_id_to_cancel);
                    }
                }
                kain_async_task_sync_runtime_flags_locked(other);
                if (other->state != KAIN_TASK_STATE_RUNNING) {
                    kain_async_task_mark_ready_locked(other);
                    (void)kain_async_schedule_task_run_locked(other);
                }
                changed = 1;
            } else if (slot == kain_async_task_slot(task) && !kain_async_task_is_terminal(other->state)) {
                other->flags |= KAIN_ASYNC_FLAG_CANCEL_REQUESTED;
                atomic_store_explicit(&other->runtime_state.cancelled, 1, memory_order_release);
                kain_async_task_sync_runtime_flags_locked(other);
                if (other->state != KAIN_TASK_STATE_RUNNING) {
                    kain_async_task_mark_ready_locked(other);
                    (void)kain_async_schedule_task_run_locked(other);
                }
            }
            kain_async_mutex_unlock(&other->lock);
        }
    } while (changed != 0);
    kain_async_mutex_unlock(&g_async_global_lock);
    kain_async_batch_end();

    kain_async_set_diag(
        diag,
        KAIN_DIAG_SEVERITY_INFO,
        KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED,
        "Async task cancelled",
        "Cancellation was requested successfully."
    );
    return 0;
}

KainTaskState kain_task_get_state(KainTaskId task_id) {
    KainAsyncTaskRecord* task = kain_async_find_task(task_id);
    KainTaskState state = KAIN_TASK_STATE_FAILED;

    if (!task) {
        return KAIN_TASK_STATE_FAILED;
    }

    kain_async_mutex_lock(&task->lock);
    state = task->state;
    kain_async_mutex_unlock(&task->lock);
    return state;
}

int kain_task_wake(
    void* wake_handle,
    KainDiagnostic* diag
) {
    return kain_async_task_signal_handle(wake_handle, diag);
}

KainTimerId kain_timer_register(
    unsigned long long delay_ms,
    void* wake_handle,
    KainDiagnostic* diag
) {
    KainAsyncTimerRecord* timer;

    if (!wake_handle) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TIMER_FAILED,
            "Async timer registration failed",
            "The wake handle was null."
        );
        return KAIN_TIMER_ID_INVALID;
    }

    timer = kain_async_allocate_timer_record();
    if (!timer) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TIMER_FAILED,
            "Async timer table full",
            "No additional timers can be allocated."
        );
        return KAIN_TIMER_ID_INVALID;
    }

    timer->wake_handle = wake_handle;
    timer->delay_ms = delay_ms;
    if (!kain_async_index_insert(
            g_async_timer_index,
            KAIN_ASYNC_TIMER_INDEX_CAPACITY,
            KAIN_ASYNC_TIMER_INDEX_MASK,
            (uint64_t)timer->id,
            (uint32_t)(timer - g_async_timers))) {
        uint32_t slot = (uint32_t)(timer - g_async_timers);
        KainTimerId timer_id = timer->id;
        g_async_timer_occupancy_words[slot / KAIN_ASYNC_SLOT_WORD_BITS] &= ~(UINT64_C(1) << (slot & 63u));
        memset(timer, 0, sizeof(*timer));
        kain_async_rebuild_timer_index();
        atomic_fetch_sub_explicit(&g_attrition_async_timer_live_count, 1u, memory_order_relaxed);
        atomic_fetch_add_explicit(&g_attrition_async_timer_exit_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_timer_exit((uint64_t)timer_id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TIMER_FAILED,
            "Async timer table full",
            "No additional timers can be allocated."
        );
        return KAIN_TIMER_ID_INVALID;
    }

#ifdef _WIN32
    timer->thread_handle = CreateThread(NULL, 0, kain_async_timer_thread_proc, timer, 0, NULL);
    if (!timer->thread_handle) {
        uint32_t slot = (uint32_t)(timer - g_async_timers);
        KainTimerId timer_id = timer->id;
        g_async_timer_occupancy_words[slot / KAIN_ASYNC_SLOT_WORD_BITS] &= ~(UINT64_C(1) << (slot & 63u));
        memset(timer, 0, sizeof(*timer));
        kain_async_rebuild_timer_index();
        atomic_fetch_sub_explicit(&g_attrition_async_timer_live_count, 1u, memory_order_relaxed);
        atomic_fetch_add_explicit(&g_attrition_async_timer_exit_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_timer_exit((uint64_t)timer_id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TIMER_FAILED,
            "Async timer thread creation failed",
            "CreateThread returned NULL."
        );
        return KAIN_TIMER_ID_INVALID;
    }
    CloseHandle(timer->thread_handle);
#else
    if (pthread_create(&timer->thread_handle, NULL, kain_async_timer_thread_proc, timer) != 0) {
        uint32_t slot = (uint32_t)(timer - g_async_timers);
        KainTimerId timer_id = timer->id;
        g_async_timer_occupancy_words[slot / KAIN_ASYNC_SLOT_WORD_BITS] &= ~(UINT64_C(1) << (slot & 63u));
        memset(timer, 0, sizeof(*timer));
        kain_async_rebuild_timer_index();
        atomic_fetch_sub_explicit(&g_attrition_async_timer_live_count, 1u, memory_order_relaxed);
        atomic_fetch_add_explicit(&g_attrition_async_timer_exit_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_timer_exit((uint64_t)timer_id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TIMER_FAILED,
            "Async timer thread creation failed",
            "pthread_create returned an error."
        );
        return KAIN_TIMER_ID_INVALID;
    }
    pthread_detach(timer->thread_handle);
#endif

    return timer->id;
}

int kain_timer_cancel(
    KainTimerId timer_id,
    KainDiagnostic* diag
) {
    KainAsyncTimerRecord* timer = NULL;

    kain_async_ensure_initialized();
    kain_async_mutex_lock(&g_async_global_lock);
    timer = kain_async_find_timer_locked(timer_id);
    if (!timer) {
        kain_async_mutex_unlock(&g_async_global_lock);
        atomic_fetch_add_explicit(&g_attrition_async_timer_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_timer_stale_reject((uint64_t)timer_id);
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TIMER_FAILED,
            "Async timer not found",
            "The requested timer id does not exist."
        );
        return -1;
    }

    atomic_store_explicit(&timer->cancelled, 1, memory_order_release);
    atomic_fetch_add_explicit(&g_attrition_async_timer_cancel_count, 1u, memory_order_relaxed);
    kain_attrition_note_async_timer_cancel((uint64_t)timer_id);
    kain_async_mutex_unlock(&g_async_global_lock);
    kain_async_set_diag(
        diag,
        KAIN_DIAG_SEVERITY_INFO,
        KAIN_DIAG_CODE_SUCCESS,
        "Async timer cancelled",
        "Timer cancellation was recorded."
    );
    return 0;
}

KainTaskId kain_async_sleep(
    unsigned long long delay_ms,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* task;
    void* result = NULL;

    task = kain_async_allocate_task_record();
    if (!task) {
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
            "Async sleep allocation failed",
            "No task slot was available for the sleep helper."
        );
        return KAIN_TASK_ID_INVALID;
    }

    task->config.task_fn = kain_async_sleep_task_fn;
    task->config.user_data = &task->sleep_state;
    task->config.result_size = 0;
    task->sleep_state.delay_ms = delay_ms;
    task->sleep_state.timer_id = KAIN_TIMER_ID_INVALID;
    task->sleep_state.armed = 0;
    task->state = KAIN_TASK_STATE_READY;
    task->flags &= ~KAIN_ASYNC_FLAG_CANCEL_REQUESTED;
    task->result = NULL;
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_READY, memory_order_release);

    if (kain_async_execute_task(task, &result, diag) == KAIN_POLL_ERROR) {
        kain_task_cancel(task->id, NULL);
        return KAIN_TASK_ID_INVALID;
    }

    return task->id;
}

int kain_task_yield(KainDiagnostic* diag) {
    (void)diag;
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
    return 0;
}

KainTaskId kain_task_current_id(void) {
    return g_async_current_task_id;
}

int kain_attrition_async_dispose_task(KainTaskId task_id) {
    KainAsyncTaskRecord* task = NULL;
    KainTaskState state;
    if (task_id == KAIN_TASK_ID_INVALID) {
        atomic_fetch_add_explicit(&g_attrition_async_task_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_task_stale_reject((uint64_t)task_id);
        return -1;
    }

    kain_async_ensure_initialized();
    kain_async_mutex_lock(&g_async_global_lock);
    task = kain_async_find_task_locked(task_id);
    if (task == NULL || !(task->flags & KAIN_ASYNC_FLAG_IN_USE) || task->id != task_id) {
        kain_async_mutex_unlock(&g_async_global_lock);
        atomic_fetch_add_explicit(&g_attrition_async_task_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_async_task_stale_reject((uint64_t)task_id);
        return -1;
    }

    kain_async_mutex_lock(&task->lock);
    state = task->state;
    kain_async_mutex_unlock(&task->lock);
    if (!kain_async_task_is_terminal(state)) {
        kain_async_mutex_unlock(&g_async_global_lock);
        return -2;
    }

    kain_async_release_task_record_locked(task);
    kain_async_mutex_unlock(&g_async_global_lock);
    atomic_fetch_sub_explicit(&g_attrition_async_task_live_count, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_attrition_async_task_exit_count, 1u, memory_order_relaxed);
    kain_attrition_note_async_task_exit((uint64_t)task_id);
    return 0;
}

void kain_attrition_async_counters_reset(void) {
    atomic_store_explicit(&g_attrition_async_task_live_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_task_peak_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_task_spawn_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_task_exit_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_task_stale_reject_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_timer_live_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_timer_peak_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_timer_spawn_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_timer_exit_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_timer_cancel_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_async_timer_stale_reject_count, 0u, memory_order_relaxed);
}

void kain_attrition_async_fill_snapshot(KainAttritionSnapshot* snapshot) {
    size_t slot;
    size_t word_index;
    uint64_t async_task_occupancy_popcount = 0u;
    uint64_t async_timer_occupancy_popcount = 0u;
    uint64_t async_task_cancel_requested_count = 0u;
    uint64_t async_task_sleeping_count = 0u;
    uint64_t async_task_ready_count = 0u;
    uint64_t async_timer_cancelled_count = 0u;
    uint64_t async_timer_fired_count = 0u;
    uint64_t async_timer_started_count = 0u;
    if (snapshot == NULL) {
        return;
    }
    snapshot->async_task_live_count = atomic_load_explicit(&g_attrition_async_task_live_count, memory_order_relaxed);
    snapshot->async_task_peak_count = atomic_load_explicit(&g_attrition_async_task_peak_count, memory_order_relaxed);
    snapshot->async_task_spawn_count = atomic_load_explicit(&g_attrition_async_task_spawn_count, memory_order_relaxed);
    snapshot->async_task_exit_count = atomic_load_explicit(&g_attrition_async_task_exit_count, memory_order_relaxed);
    snapshot->async_task_stale_reject_count = atomic_load_explicit(
        &g_attrition_async_task_stale_reject_count,
        memory_order_relaxed);
    snapshot->async_timer_live_count = atomic_load_explicit(&g_attrition_async_timer_live_count, memory_order_relaxed);
    snapshot->async_timer_peak_count = atomic_load_explicit(&g_attrition_async_timer_peak_count, memory_order_relaxed);
    snapshot->async_timer_spawn_count = atomic_load_explicit(&g_attrition_async_timer_spawn_count, memory_order_relaxed);
    snapshot->async_timer_exit_count = atomic_load_explicit(&g_attrition_async_timer_exit_count, memory_order_relaxed);
    snapshot->async_timer_cancel_count = atomic_load_explicit(&g_attrition_async_timer_cancel_count, memory_order_relaxed);
    snapshot->async_timer_stale_reject_count = atomic_load_explicit(
        &g_attrition_async_timer_stale_reject_count,
        memory_order_relaxed);
    kain_async_ensure_initialized();
    kain_async_mutex_lock(&g_async_global_lock);
    snapshot->async_task_occupancy_low_word = g_async_task_occupancy_words[0];
    snapshot->async_timer_occupancy_low_word = g_async_timer_occupancy_words[0];
    for (word_index = 0u; word_index < KAIN_ASYNC_TASK_WORD_COUNT; ++word_index) {
        async_task_occupancy_popcount += kain_async_popcount_u64(g_async_task_occupancy_words[word_index]);
    }
    for (word_index = 0u; word_index < KAIN_ASYNC_TIMER_WORD_COUNT; ++word_index) {
        async_timer_occupancy_popcount += kain_async_popcount_u64(g_async_timer_occupancy_words[word_index]);
    }
    for (slot = 0u; slot < KAIN_ASYNC_MAX_TASKS; ++slot) {
        KainAsyncTaskRecord* task = &g_async_tasks[slot];
        if (!(task->flags & KAIN_ASYNC_FLAG_IN_USE)) {
            continue;
        }
        if (task->flags & KAIN_ASYNC_FLAG_CANCEL_REQUESTED) {
            async_task_cancel_requested_count += 1u;
        }
        if (task->sleep_state.armed) {
            async_task_sleeping_count += 1u;
        }
        if (task->state == KAIN_TASK_STATE_READY) {
            async_task_ready_count += 1u;
        }
    }
    for (slot = 0u; slot < KAIN_ASYNC_MAX_TIMERS; ++slot) {
        KainAsyncTimerRecord* timer = &g_async_timers[slot];
        if (!timer->in_use) {
            continue;
        }
        if (atomic_load_explicit(&timer->cancelled, memory_order_relaxed) != 0) {
            async_timer_cancelled_count += 1u;
        }
        if (atomic_load_explicit(&timer->fired, memory_order_relaxed) != 0) {
            async_timer_fired_count += 1u;
        }
        if (atomic_load_explicit(&timer->started, memory_order_relaxed) != 0) {
            async_timer_started_count += 1u;
        }
    }
    kain_async_mutex_unlock(&g_async_global_lock);
    snapshot->async_task_occupancy_popcount = async_task_occupancy_popcount;
    snapshot->async_timer_occupancy_popcount = async_timer_occupancy_popcount;
    snapshot->async_task_cancel_requested_count = async_task_cancel_requested_count;
    snapshot->async_task_sleeping_count = async_task_sleeping_count;
    snapshot->async_task_ready_count = async_task_ready_count;
    snapshot->async_timer_cancelled_count = async_timer_cancelled_count;
    snapshot->async_timer_fired_count = async_timer_fired_count;
    snapshot->async_timer_started_count = async_timer_started_count;
}

void kain_task_result_cleanup(void* result) {
    if (result != NULL) {
        free(result);
    }
}
