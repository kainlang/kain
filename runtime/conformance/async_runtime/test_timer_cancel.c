#include "async.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <errno.h>
#include <time.h>

typedef struct {
    KainTimerId timer_id;
    unsigned int polls;
} TimerCancelState;

static void test_sleep_ms(unsigned long long delay_ms) {
#ifdef _WIN32
    while (delay_ms > 0) {
        DWORD chunk = delay_ms > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : (DWORD)delay_ms;
        Sleep(chunk);
        delay_ms -= chunk;
    }
#else
    while (delay_ms > 0) {
        struct timespec req;
        req.tv_sec = (time_t)(delay_ms / 1000ULL);
        req.tv_nsec = (long)((delay_ms % 1000ULL) * 1000000ULL);
        while (nanosleep(&req, &req) != 0 && errno == EINTR) {
            /* retry */
        }
        break;
    }
#endif
}

static KainPollResult timer_cancel_task_fn(
    KainFutureContext* context,
    void* user_data,
    void** result
) {
    TimerCancelState* state = (TimerCancelState*)user_data;
    KainTaskRuntimeState* runtime = (KainTaskRuntimeState*)context->runtime_data;
    int* value = NULL;

    if (!state || !runtime) {
        return KAIN_POLL_ERROR;
    }

    state->polls++;
    if (state->polls == 1) {
        state->timer_id = kain_timer_register(250, context->wake_handle, NULL);
        if (state->timer_id == KAIN_TIMER_ID_INVALID) {
            return KAIN_POLL_ERROR;
        }
        return KAIN_POLL_PENDING;
    }

    if (atomic_load_explicit(&runtime->timer_fired, memory_order_acquire) == 0) {
        return KAIN_POLL_PENDING;
    }

    value = (int*)malloc(sizeof(int));
    if (!value) {
        return KAIN_POLL_ERROR;
    }

    *value = 77;
    *result = value;
    return KAIN_POLL_READY;
}

static int fail(const char* message) {
    fprintf(stderr, "test_timer_cancel: %s\n", message);
    return 1;
}

int main(void) {
    KainTaskSpawnConfig config;
    KainDiagnostic diag;
    TimerCancelState state = {0};
    KainTaskId task_id;
    void* result = NULL;

    kain_task_spawn_config_init(&config);
    config.task_fn = timer_cancel_task_fn;
    config.user_data = &state;

    task_id = kain_task_spawn(&config, &diag);
    if (task_id == KAIN_TASK_ID_INVALID) {
        kain_diagnostic_print(&diag);
        return fail("spawn failed");
    }

    if (kain_task_poll(task_id, &result, &diag) != KAIN_POLL_PENDING) {
        kain_diagnostic_print(&diag);
        return fail("initial poll should arm the timer and remain pending");
    }

    if (state.timer_id == KAIN_TIMER_ID_INVALID) {
        return fail("timer was not registered");
    }

    if (kain_timer_cancel(state.timer_id, &diag) != 0) {
        kain_diagnostic_print(&diag);
        return fail("timer cancellation failed");
    }

    test_sleep_ms(100);

    if (kain_task_poll(task_id, &result, &diag) != KAIN_POLL_PENDING) {
        kain_diagnostic_print(&diag);
        return fail("cancelled timer should not wake the task");
    }

    if (kain_task_get_state(task_id) != KAIN_TASK_STATE_PENDING) {
        return fail("task should still be pending after timer cancellation");
    }

    if (kain_task_cancel(task_id, &diag) != 0) {
        kain_diagnostic_print(&diag);
        return fail("task cancellation failed");
    }

    if (kain_task_get_state(task_id) != KAIN_TASK_STATE_CANCELLED) {
        return fail("task should transition to CANCELLED");
    }

    if (kain_task_await(task_id, &result, &diag) == 0) {
        return fail("await should not succeed on a cancelled task");
    }

    printf("test_timer_cancel: PASS\n");
    return 0;
}
