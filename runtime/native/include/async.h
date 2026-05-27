#ifndef ASYNC_H
#define ASYNC_H

#include "base.h"
#include "diagnostics.h"
#include <stdatomic.h>
#include <stddef.h>

/*
 * KAIN Native Runtime Async ABI
 *
 * This header defines the canonical async/task/timer runtime ABI for the
 * KAIN native runtime. It provides declarations for async task execution,
 * futures, wake/poll mechanics, timers, and scheduler integration.
 *
 * Async Model Features:
 * - Task/future creation and execution
 * - Wake/poll mechanics for async operations
 * - Task cancellation and cleanup
 * - Timer registration and wake integration
 * - Integration with actor runtime and scheduler
 * - Async value ownership and lifetime rules
 */

/* Task ID Type */
typedef unsigned long long KainTaskId;

#define KAIN_TASK_ID_INVALID 0

/* Task State */
typedef enum {
    KAIN_TASK_STATE_PENDING = 0,
    KAIN_TASK_STATE_READY,
    KAIN_TASK_STATE_RUNNING,
    KAIN_TASK_STATE_COMPLETED,
    KAIN_TASK_STATE_CANCELLED,
    KAIN_TASK_STATE_FAILED,
} KainTaskState;

/* Poll Result */
typedef enum {
    KAIN_POLL_PENDING = 0,
    KAIN_POLL_READY,
    KAIN_POLL_ERROR,
} KainPollResult;

/* Timer ID Type */
typedef unsigned long long KainTimerId;

#define KAIN_TIMER_ID_INVALID 0

/*
 * Task Handle
 *
 * Opaque handle to an async task. Used for polling, cancellation, and
 * result retrieval.
 */
typedef struct KainTaskHandle KainTaskHandle;

/*
 * Future Context
 *
 * Context passed to async functions during polling. Contains wake handle
 * and other runtime state needed for async operations. The runtime_data
 * pointer is owned by the runtime and currently points at a
 * KainTaskRuntimeState snapshot for the task being polled.
 */
typedef struct {
    void* wake_handle;
    void* runtime_data;
} KainFutureContext;

/*
 * Task Runtime State
 *
 * Runtime-owned state snapshot exposed to async task functions through the
 * future context. This is intended for cooperative polling, wake accounting,
 * cancellation observation, and timer-driven behavior.
 */
typedef struct {
    KainTaskId task_id;
    atomic_uint poll_count;
    atomic_uint wake_count;
    atomic_uint timer_count;
    atomic_uint child_wait_count;
    atomic_uint dependency_wait_count;
    atomic_int wake_requested;
    atomic_int timer_fired;
    atomic_int cancelled;
    atomic_int continuation_blocked;
    atomic_int completion_deferred;
    atomic_int state_snapshot;
} KainTaskRuntimeState;

/* Dependency Wait Mode */
typedef enum {
    KAIN_TASK_WAIT_MODE_ALL = 0,
    KAIN_TASK_WAIT_MODE_ANY = 1,
} KainTaskWaitMode;

/*
 * Completion Callback
 *
 * Invoked exactly once when a task transitions into a terminal state. The
 * callback fires for success, cancellation, and failure after the runtime
 * has updated its graph bookkeeping for that task.
 */
typedef void (*KainTaskCompletionCallback)(
    KainTaskId task_id,
    KainTaskState final_state,
    void* result,
    void* user_data
);

/*
 * Async Task Function
 *
 * Entry point for async task execution. Called by the runtime to poll
 * the task. Should return PENDING if not ready, READY if complete.
 *
 * Parameters:
 *   context - Future context with wake handle
 *   user_data - User-provided data passed during spawn
 *   result - Output parameter for task result (when READY)
 *
 * Returns:
 *   Poll result indicating task state
 */
typedef KainPollResult (*KainAsyncTaskFn)(
    KainFutureContext* context,
    void* user_data,
    void** result
);

/*
 * Task Spawn Configuration
 *
 * Configuration for spawning a new async task.
 */
typedef struct {
    KainAsyncTaskFn task_fn;
    void* user_data;
    size_t result_size;
    KainTaskId parent_task_id;
    KainTaskId continuation_of_task_id;
    KainTaskWaitMode child_wait_mode;
    KainTaskCompletionCallback completion_callback;
    void* completion_user_data;
} KainTaskSpawnConfig;

/*
 * Initialize Task Spawn Configuration
 *
 * Sets default values for task spawn configuration.
 */
void kain_task_spawn_config_init(KainTaskSpawnConfig* config);

/*
 * Spawn Async Task
 *
 * Creates and schedules a new async task. Returns the task ID on success,
 * KAIN_TASK_ID_INVALID on failure. Populates diagnostic on error.
 */
KainTaskId kain_task_spawn(
    const KainTaskSpawnConfig* config,
    KainDiagnostic* diag
);

/*
 * Poll Task
 *
 * Polls a task to check if it's ready. Returns the poll result.
 * If READY, the result pointer is populated.
 */
KainPollResult kain_task_poll(
    KainTaskId task_id,
    void** result,
    KainDiagnostic* diag
);

/*
 * Await Task (Blocking)
 *
 * Blocks until the task completes. Returns 0 on success, non-zero on error.
 * Populates result pointer with task result.
 */
int kain_task_await(
    KainTaskId task_id,
    void** result,
    KainDiagnostic* diag
);

/*
 * Cancel Task
 *
 * Requests cancellation of a task. Returns 0 on success, non-zero on error.
 * The task may not cancel immediately.
 */
int kain_task_cancel(
    KainTaskId task_id,
    KainDiagnostic* diag
);

/*
 * Register Completion Callback
 *
 * Installs or replaces the completion callback for a live task. The callback
 * will fire exactly once when the task becomes completed, cancelled, or
 * failed.
 */
int kain_task_set_completion_callback(
    KainTaskId task_id,
    KainTaskCompletionCallback completion_callback,
    void* completion_user_data,
    KainDiagnostic* diag
);

/*
 * Add Child Task Relationship
 *
 * Links a child to a parent task. The parent will sleep until the child wait
 * condition is satisfied.
 *
 * Parameters:
 *   parent_task_id - Parent task to block on child completion
 *   child_task_id - Child task that contributes to the parent's child wait
 *   wait_mode - ALL waits for every child; ANY resumes after one child
 */
int kain_task_add_child(
    KainTaskId parent_task_id,
    KainTaskId child_task_id,
    KainTaskWaitMode wait_mode,
    KainDiagnostic* diag
);

/*
 * Add Continuation Relationship
 *
 * Schedules continuation_task_id to run once antecedent_task_id reaches a
 * terminal state. If the antecedent has already completed, the continuation
 * is scheduled immediately.
 */
int kain_task_add_continuation(
    KainTaskId antecedent_task_id,
    KainTaskId continuation_task_id,
    KainDiagnostic* diag
);

/*
 * Add Wait Dependencies
 *
 * Puts waiter_task_id to sleep until its dependency wait condition is
 * satisfied by one or more dependency tasks reaching a terminal state.
 *
 * Parameters:
 *   waiter_task_id - Task that should be resumed later
 *   dependency_task_ids - Array of task ids the waiter depends on
 *   dependency_task_count - Number of entries in dependency_task_ids
 *   wait_mode - ALL waits for every dependency; ANY resumes after one
 */
int kain_task_add_wait_dependencies(
    KainTaskId waiter_task_id,
    const KainTaskId* dependency_task_ids,
    size_t dependency_task_count,
    KainTaskWaitMode wait_mode,
    KainDiagnostic* diag
);

/*
 * Batch Lock / Unlock
 *
 * Defers async queue fanout until the outermost unlock so callers can mutate
 * multiple graph edges without interleaving wake and callback drains.
 */
int kain_task_batch_lock(KainDiagnostic* diag);
int kain_task_batch_unlock(KainDiagnostic* diag);

/*
 * Get Task State
 *
 * Returns the current state of a task.
 */
KainTaskState kain_task_get_state(KainTaskId task_id);

/*
 * Wake Task
 *
 * Wakes a pending task, signaling that it should be polled again.
 * Called by the runtime or by async operations when they complete.
 * Returns 0 on success, non-zero on error.
 */
int kain_task_wake(
    void* wake_handle,
    KainDiagnostic* diag
);

/*
 * Register Timer
 *
 * Registers a timer that will wake a task after the specified delay.
 * Returns the timer ID on success, KAIN_TIMER_ID_INVALID on failure.
 *
 * Parameters:
 *   delay_ms - Delay in milliseconds
 *   wake_handle - Wake handle to trigger when timer fires
 *   diag - Diagnostic output on error
 */
KainTimerId kain_timer_register(
    unsigned long long delay_ms,
    void* wake_handle,
    KainDiagnostic* diag
);

/*
 * Cancel Timer
 *
 * Cancels a registered timer. Returns 0 on success, non-zero on error.
 * If the timer has already fired, this is a no-op.
 */
int kain_timer_cancel(
    KainTimerId timer_id,
    KainDiagnostic* diag
);

/*
 * Sleep (Async)
 *
 * Creates an async sleep operation. Returns a task ID that will complete
 * after the specified delay. This is a convenience wrapper around timer
 * registration.
 */
KainTaskId kain_async_sleep(
    unsigned long long delay_ms,
    KainDiagnostic* diag
);

/*
 * Yield Task
 *
 * Yields execution to allow other tasks to run. Returns 0 on success.
 * This is a cooperative scheduling hint.
 */
int kain_task_yield(KainDiagnostic* diag);

/*
 * Get Current Task ID
 *
 * Returns the ID of the currently executing task, or KAIN_TASK_ID_INVALID
 * if not running in a task context.
 */
KainTaskId kain_task_current_id(void);

/*
 * Task Result Cleanup
 *
 * Cleans up resources associated with a completed task result.
 * Should be called after retrieving a task result.
 */
void kain_task_result_cleanup(void* result);

#endif /* ASYNC_H */
