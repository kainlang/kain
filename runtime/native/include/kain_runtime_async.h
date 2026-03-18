#ifndef KAIN_RUNTIME_ASYNC_H
#define KAIN_RUNTIME_ASYNC_H

#include "kain_runtime_base.h"
#include "kain_runtime_diagnostics.h"
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
    atomic_int wake_requested;
    atomic_int timer_fired;
    atomic_int cancelled;
    atomic_int state_snapshot;
} KainTaskRuntimeState;

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

#endif /* KAIN_RUNTIME_ASYNC_H */
