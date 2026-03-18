#include "kain_runtime_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

typedef struct {
    void* wake_handle;
    unsigned int polls;
} WakeTaskState;

static KainPollResult wake_task_fn(
    KainFutureContext* context,
    void* user_data,
    void** result
) {
    WakeTaskState* state = (WakeTaskState*)user_data;
    KainTaskRuntimeState* runtime = (KainTaskRuntimeState*)context->runtime_data;
    int* value = NULL;

    if (!state || !runtime) {
        return KAIN_POLL_ERROR;
    }

    state->polls++;
    if (state->polls == 1) {
        state->wake_handle = context->wake_handle;
        return KAIN_POLL_PENDING;
    }

    if (atomic_load_explicit(&runtime->wake_count, memory_order_acquire) == 0) {
        return KAIN_POLL_PENDING;
    }

    value = (int*)malloc(sizeof(int));
    if (!value) {
        return KAIN_POLL_ERROR;
    }

    *value = 4242;
    *result = value;
    return KAIN_POLL_READY;
}

static int fail(const char* message) {
    fprintf(stderr, "test_task_wake_poll: %s\n", message);
    return 1;
}

int main(void) {
    KainTaskSpawnConfig config;
    KainDiagnostic diag;
    WakeTaskState state = {0};
    KainTaskId task_id;
    void* result = NULL;

    kain_task_spawn_config_init(&config);
    config.task_fn = wake_task_fn;
    config.user_data = &state;

    task_id = kain_task_spawn(&config, &diag);
    if (task_id == KAIN_TASK_ID_INVALID) {
        kain_diagnostic_print(&diag);
        return fail("spawn failed");
    }

    if (kain_task_poll(task_id, &result, &diag) != KAIN_POLL_PENDING) {
        kain_diagnostic_print(&diag);
        return fail("initial poll should return PENDING");
    }

    if (state.wake_handle == NULL) {
        return fail("task did not expose a wake handle");
    }

    if (kain_task_wake(state.wake_handle, &diag) != 0) {
        kain_diagnostic_print(&diag);
        return fail("manual wake failed");
    }

    if (kain_task_get_state(task_id) != KAIN_TASK_STATE_READY) {
        return fail("wake should move task back to READY");
    }

    if (kain_task_poll(task_id, &result, &diag) != KAIN_POLL_READY) {
        kain_diagnostic_print(&diag);
        return fail("second poll should complete the task");
    }

    if (result == NULL || *((int*)result) != 4242) {
        return fail("wake-driven result was not populated");
    }

    if (kain_task_await(task_id, &result, &diag) != 0) {
        kain_diagnostic_print(&diag);
        return fail("await should succeed after wake-driven completion");
    }

    kain_task_result_cleanup(result);
    printf("test_task_wake_poll: PASS\n");
    return 0;
}
