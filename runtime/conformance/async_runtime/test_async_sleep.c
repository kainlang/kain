#include "async.h"
#include <stdio.h>

static int fail(const char* message) {
    fprintf(stderr, "test_async_sleep: %s\n", message);
    return 1;
}

int main(void) {
    KainDiagnostic diag;
    KainTaskId task_id;
    void* result = (void*)0x1;

    task_id = kain_async_sleep(30, &diag);
    if (task_id == KAIN_TASK_ID_INVALID) {
        kain_diagnostic_print(&diag);
        return fail("async sleep helper failed");
    }

    if (kain_task_await(task_id, &result, &diag) != 0) {
        kain_diagnostic_print(&diag);
        return fail("await failed for async sleep");
    }

    if (result != NULL) {
        return fail("async sleep should not produce a result payload");
    }

    if (kain_task_get_state(task_id) != KAIN_TASK_STATE_COMPLETED) {
        return fail("sleep task should finish in COMPLETED state");
    }

    printf("test_async_sleep: PASS\n");
    return 0;
}
