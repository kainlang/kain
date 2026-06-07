#include "../include/base.h"
#include "../include/diagnostics.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int runtime_get_last_diagnostic(KainDiagnostic* out);
extern void runtime_clear_last_diagnostic(void);

static uint64_t g_foreign_payload[4];

static int expect_true(int condition, const char* label) {
    if (!condition) {
        fprintf(stderr, "rc pointer provenance test failed: %s\n", label);
        return 0;
    }
    return 1;
}

int main(void) {
    KainDiagnostic diag;
    void* foreign_ptr = (void*)&g_foreign_payload[0];
    char* live = NULL;

    runtime_clear_last_diagnostic();
    if (!expect_true(kain_rc_is_tracked_pointer(foreign_ptr) == 0, "static payload is not tracked")) {
        return 1;
    }
    rc_retain(foreign_ptr);
    rc_release(foreign_ptr);
    if (!expect_true(runtime_get_last_diagnostic(&diag) == 0, "foreign pointers do not emit RC diagnostics")) {
        return 2;
    }

    live = string_new("alive");
    if (!expect_true(live != NULL, "tracked string allocation succeeds")) {
        return 3;
    }
    if (!expect_true(kain_rc_is_tracked_pointer(live) == 1, "live RC allocation is tracked")) {
        return 4;
    }

    runtime_clear_last_diagnostic();
    rc_release(live);
    if (!expect_true(runtime_get_last_diagnostic(&diag) == 0, "first live release is clean")) {
        return 5;
    }
    if (!expect_true(kain_rc_is_tracked_pointer(live) == 1, "recently freed RC payload stays tracked")) {
        return 6;
    }

    runtime_clear_last_diagnostic();
    rc_release(live);
    if (!expect_true(runtime_get_last_diagnostic(&diag) == 1, "release-after-free emits a diagnostic")) {
        return 7;
    }
    if (!expect_true(diag.code == KAIN_DIAG_CODE_MEMORY_INVALID_POINTER, "release-after-free uses invalid pointer code")) {
        return 8;
    }
    if (!expect_true(strstr(diag.message, "after free") != NULL, "release-after-free message stays specific")) {
        return 9;
    }

    return 0;
}
