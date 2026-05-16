#include "async.h"
#include <stdio.h>

typedef struct {
    unsigned int polls;
} CancelTaskState;

static KainPollResult cancel_task_fn(
    KainFutureContext* context,
    void* user_data,
    void** result
) {
    CancelTaskState* state = (CancelTaskState*)user_data;

    (void)context;
    (void)result;

    if (!state) {
        return KAIN_POLL_ERROR;
    }

    state->polls++;
    return KAIN_POLL_PENDING;
}

static int fail(const char* message) {
    fprintf(stderr, "test_task_cancel: %s\n", message);
    return 1;
}

int main(void) {
    KainTaskSpawnConfig config;
    KainDiagnostic diag;
    CancelTaskState state = {0};
    KainTaskId task_id;
    void* result = NULL;

    kain_task_spawn_config_init(&config);
    config.task_fn = cancel_task_fn;
    config.user_data = &state;

    task_id = kain_task_spawn(&config, &diag);
    if (task_id == KAIN_TASK_ID_INVALID) {
        kain_diagnostic_print(&diag);
        return fail("spawn failed");
    }

    if (kain_task_poll(task_id, &result, &diag) != KAIN_POLL_PENDING) {
        kain_diagnostic_print(&diag);
        return fail("task should begin pending");
    }

    if (kain_task_cancel(task_id, &diag) != 0) {
        kain_diagnostic_print(&diag);
        return fail("task cancellation failed");
    }

    if (kain_task_get_state(task_id) != KAIN_TASK_STATE_CANCELLED) {
        return fail("task should be cancelled immediately");
    }

    if (kain_task_poll(task_id, &result, &diag) != KAIN_POLL_ERROR) {
        kain_diagnostic_print(&diag);
        return fail("polling a cancelled task should fail");
    }

    if (kain_task_await(task_id, &result, &diag) == 0) {
        return fail("await should fail after cancellation");
    }

    printf("test_task_cancel: PASS\n");
    return 0;
}
