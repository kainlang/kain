/*
 * Reflection conformance smoke: invalid payload handling.
 *
 * Validates the native runtime rejects malformed payloads and surfaces
 * reflection diagnostics for parse and schema errors.
 */

#include "../../native/include/kain_runtime_reflection.h"
#include "../../native/include/kain_runtime_diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_error(
    const char* label,
    int result,
    const KainDiagnostic* diag,
    int expected_code,
    KainDiagSeverity minimum_severity
) {
    char buffer[512];

    assert(result != 0);
    assert(diag != NULL);
    buffer[0] = '\0';
    kain_diagnostic_format(diag, buffer, sizeof(buffer));
    fprintf(stderr, "  %s -> %s\n", label ? label : "case", buffer);
    assert(diag->severity >= minimum_severity);
    assert(diag->code == expected_code);
    assert(diag->subsystem == KAIN_DIAG_SUBSYSTEM_REFLECTION);
}

int main(void) {
    KainReflectionPayload* payload = NULL;
    KainDiagnostic diag;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("TEST: invalid reflection payload inputs\n");

    kain_diagnostic_init(&diag);
    expect_error(
        "null-json",
        kain_reflection_load_from_json(NULL, &payload, &diag),
        &diag,
        KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
        KAIN_DIAG_SEVERITY_ERROR
    );

    kain_diagnostic_init(&diag);
    expect_error(
        "truncated-json",
        kain_reflection_load_from_json("{", &payload, &diag),
        &diag,
        KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
        KAIN_DIAG_SEVERITY_ERROR
    );

    kain_diagnostic_init(&diag);
    expect_error(
        "missing-schema-version",
        kain_reflection_load_from_json("{\"types\":[]}", &payload, &diag),
        &diag,
        KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
        KAIN_DIAG_SEVERITY_ERROR
    );

    kain_diagnostic_init(&diag);
    expect_error(
        "missing-type-id",
        kain_reflection_load_from_json(
            "{\"schema_version\":1,\"types\":[{\"name\":\"Point\",\"kind\":\"struct\",\"fields\":[]}],\"items\":[]}",
            &payload,
            &diag
        ),
        &diag,
        KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
        KAIN_DIAG_SEVERITY_ERROR
    );

    kain_diagnostic_init(&diag);
    expect_error(
        "missing-item-name",
        kain_reflection_load_from_json(
            "{\"schema_version\":1,\"types\":[],\"items\":[{\"item_id\":1,\"kind\":\"struct\",\"module_path\":\"app\",\"type_id\":1}]}",
            &payload,
            &diag
        ),
        &diag,
        KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
        KAIN_DIAG_SEVERITY_ERROR
    );

    kain_diagnostic_init(&diag);
    expect_error(
        "missing-path",
        kain_reflection_load_from_path("does/not/exist.json", &payload, &diag),
        &diag,
        KAIN_DIAG_CODE_REFLECTION_NOT_FOUND,
        KAIN_DIAG_SEVERITY_ERROR
    );

    printf("PASS: invalid reflection payload handling\n");
    return 0;
}
