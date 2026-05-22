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

typedef struct KainAsyncTaskRecord {
    int in_use;
    KainTaskId id;
    KainTaskSpawnConfig config;
    KainTaskState state;
    int cancel_requested;
    void* result;
    KainFutureContext future_context;
    KainTaskRuntimeState runtime_state;
    KainTaskHandle handle;
    KainAsyncSleepState sleep_state;
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
        if (g_async_tasks[slot].in_use) {
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
        if (g_async_timers[slot].in_use) {
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
            g_async_tasks[slot].in_use &&
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
    if (task == NULL || !task->in_use) {
        return;
    }
    slot = (uint32_t)(task - g_async_tasks);
    bit = UINT64_C(1) << (slot & 63u);
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
        record->in_use = 1;
        record->id = g_async_next_task_id++;
        record->state = KAIN_TASK_STATE_READY;
        record->handle.id = record->id;
        record->handle.owner = record;
        kain_async_mutex_init(&record->lock);
        kain_async_cond_init(&record->cond);

        atomic_init(&record->runtime_state.poll_count, 0);
        atomic_init(&record->runtime_state.wake_count, 0);
        atomic_init(&record->runtime_state.timer_count, 0);
        atomic_init(&record->runtime_state.wake_requested, 0);
        atomic_init(&record->runtime_state.timer_fired, 0);
        atomic_init(&record->runtime_state.cancelled, 0);
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
    return state == KAIN_TASK_STATE_COMPLETED ||
           state == KAIN_TASK_STATE_CANCELLED ||
           state == KAIN_TASK_STATE_FAILED;
}

static KainPollResult kain_async_execute_task(KainAsyncTaskRecord* task, void** result, KainDiagnostic* diag) {
    KainPollResult poll_result;
    void* produced_result = NULL;
    KainTaskState post_state = KAIN_TASK_STATE_FAILED;
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
    if (task->state == KAIN_TASK_STATE_CANCELLED) {
        kain_async_mutex_unlock(&task->lock);
        if (result) {
            *result = NULL;
        }
        kain_async_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED,
            "Async task is cancelled",
            "Polling a cancelled async task is not allowed."
        );
        return KAIN_POLL_ERROR;
    }

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

    task->state = KAIN_TASK_STATE_RUNNING;
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_RUNNING, memory_order_release);
    atomic_fetch_add_explicit(&task->runtime_state.poll_count, 1, memory_order_relaxed);
    g_async_current_task_id = task->id;
    kain_async_mutex_unlock(&task->lock);

    poll_result = task->config.task_fn(&task->future_context, task->config.user_data, &produced_result);

    g_async_current_task_id = KAIN_TASK_ID_INVALID;
    kain_async_mutex_lock(&task->lock);

    if (task->cancel_requested || atomic_load_explicit(&task->runtime_state.cancelled, memory_order_acquire) != 0) {
        task->state = KAIN_TASK_STATE_CANCELLED;
        atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_CANCELLED, memory_order_release);
        atomic_store_explicit(&task->runtime_state.cancelled, 1, memory_order_release);
        if (produced_result != NULL) {
            kain_task_result_cleanup(produced_result);
            produced_result = NULL;
        }
        if (result) {
            *result = NULL;
        }
        kain_async_cond_signal(&task->cond);
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
        task->result = produced_result;
        task->state = KAIN_TASK_STATE_COMPLETED;
        atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_COMPLETED, memory_order_release);
        if (result) {
            *result = produced_result;
        }
        kain_async_cond_signal(&task->cond);
        kain_async_mutex_unlock(&task->lock);
        return KAIN_POLL_READY;
    }

    if (poll_result == KAIN_POLL_PENDING) {
        wake_requested = atomic_load_explicit(&task->runtime_state.wake_requested, memory_order_acquire) != 0;
        post_state = wake_requested ? KAIN_TASK_STATE_READY : KAIN_TASK_STATE_PENDING;
        task->state = post_state;
        atomic_store_explicit(&task->runtime_state.state_snapshot, post_state, memory_order_release);
        if (result) {
            *result = NULL;
        }
        if (wake_requested) {
            kain_async_cond_signal(&task->cond);
        }
        kain_async_mutex_unlock(&task->lock);
        return KAIN_POLL_PENDING;
    }

    task->state = KAIN_TASK_STATE_FAILED;
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_FAILED, memory_order_release);
    if (result) {
        *result = NULL;
    }
    kain_async_cond_signal(&task->cond);
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
    if (task->id != handle->id || !task->in_use) {
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
        task->state = KAIN_TASK_STATE_READY;
        atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_READY, memory_order_release);
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

void kain_task_spawn_config_init(KainTaskSpawnConfig* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));
}

KainTaskId kain_task_spawn(
    const KainTaskSpawnConfig* config,
    KainDiagnostic* diag
) {
    KainAsyncTaskRecord* task;

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

    task->config = *config;
    task->state = KAIN_TASK_STATE_READY;
    task->cancel_requested = 0;
    task->result = NULL;
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_READY, memory_order_release);
    return task->id;
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
    KainAsyncTaskRecord* task = kain_async_find_task(task_id);
    KainTimerId timer_id_to_cancel = KAIN_TIMER_ID_INVALID;

    if (!task) {
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

    kain_async_mutex_lock(&task->lock);
    if (kain_async_task_is_terminal(task->state)) {
        kain_async_mutex_unlock(&task->lock);
        return 0;
    }
    task->cancel_requested = 1;
    if (task->sleep_state.timer_id != KAIN_TIMER_ID_INVALID) {
        timer_id_to_cancel = task->sleep_state.timer_id;
        task->sleep_state.timer_id = KAIN_TIMER_ID_INVALID;
        task->sleep_state.armed = 0;
    }
    task->state = KAIN_TASK_STATE_CANCELLED;
    atomic_store_explicit(&task->runtime_state.cancelled, 1, memory_order_release);
    atomic_store_explicit(&task->runtime_state.state_snapshot, KAIN_TASK_STATE_CANCELLED, memory_order_release);
    kain_async_cond_signal(&task->cond);
    kain_async_mutex_unlock(&task->lock);

    if (timer_id_to_cancel != KAIN_TIMER_ID_INVALID) {
        kain_async_mutex_lock(&g_async_global_lock);
        {
            KainAsyncTimerRecord* timer = kain_async_find_timer_locked(timer_id_to_cancel);
            if (timer != NULL && timer->id == timer_id_to_cancel) {
                atomic_store_explicit(&timer->cancelled, 1, memory_order_release);
                atomic_fetch_add_explicit(&g_attrition_async_timer_cancel_count, 1u, memory_order_relaxed);
                kain_attrition_note_async_timer_cancel((uint64_t)timer_id_to_cancel);
            }
        }
        kain_async_mutex_unlock(&g_async_global_lock);
    }

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
    task->cancel_requested = 0;
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
    if (task == NULL || !task->in_use || task->id != task_id) {
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
        if (!task->in_use) {
            continue;
        }
        if (task->cancel_requested) {
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
