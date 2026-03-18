#include "kain_runtime_async.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    KainTaskId seen_task_id;
    unsigned int polls;
} BasicTaskState;

static KainPollResult basic_task_fn(
    KainFutureContext* context,
    void* user_data,
    void** result
) {
    BasicTaskState* state = (BasicTaskState*)user_data;
    int* value = (int*)malloc(sizeof(int));

    (void)context;
    if (!state || !value) {
        return KAIN_POLL_ERROR;
    }

    state->polls++;
    state->seen_task_id = kain_task_current_id();
    *value = 1234;
    *result = value;
    return KAIN_POLL_READY;
}

static int fail(const char* message) {
    fprintf(stderr, "test_task_spawn_basic: %s\n", message);
    return 1;
}

int main(void) {
    KainTaskSpawnConfig config;
    KainDiagnostic diag;
    BasicTaskState state = {0};
    KainTaskId task_id;
    void* result = NULL;

    kain_task_spawn_config_init(&config);
    config.task_fn = basic_task_fn;
    config.user_data = &state;

    task_id = kain_task_spawn(&config, &diag);
    if (task_id == KAIN_TASK_ID_INVALID) {
        kain_diagnostic_print(&diag);
        return fail("spawn failed");
    }

    if (kain_task_get_state(task_id) != KAIN_TASK_STATE_READY) {
        return fail("task should start in READY state");
    }

    if (kain_task_poll(task_id, &result, &diag) != KAIN_POLL_READY) {
        kain_diagnostic_print(&diag);
        return fail("poll did not return READY");
    }

    if (state.polls != 1) {
        return fail("task function should run exactly once");
    }

    if (state.seen_task_id != task_id) {
        return fail("current task id was not visible inside task function");
    }

    if (result == NULL || *((int*)result) != 1234) {
        return fail("task result was not populated");
    }

    if (kain_task_get_state(task_id) != KAIN_TASK_STATE_COMPLETED) {
        return fail("task should end in COMPLETED state");
    }

    if (kain_task_await(task_id, &result, &diag) != 0) {
        kain_diagnostic_print(&diag);
        return fail("await should succeed on a completed task");
    }

    if (result == NULL || *((int*)result) != 1234) {
        return fail("await lost the completed result");
    }

    kain_task_result_cleanup(result);
    printf("test_task_spawn_basic: PASS\n");
    return 0;
}
