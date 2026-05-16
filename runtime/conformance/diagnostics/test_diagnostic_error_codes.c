/*
 * KAIN Native Runtime - Diagnostic Error Code Stability Tests
 *
 * Validates that the canonical diagnostics error-code families remain stable
 * and preserve the runtime's structured reporting contract.
 */

#include "../../native/include/diagnostics.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char* message) {
    if (!condition) {
        printf("  FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

static int test_stable_family_bases(void) {
    int failures = 0;

    printf("Test 1: Stable Family Bases\n");

    failures += check(KAIN_DIAG_CODE_SUCCESS == 0, "success code must remain 0");
    failures += check(KAIN_DIAG_CODE_GENERIC_ERROR == 1, "generic error code must remain 1");
    failures += check(KAIN_DIAG_CODE_CONTRACT_BASE == 1000, "contract base must remain 1000");
    failures += check(KAIN_DIAG_CODE_REFLECTION_BASE == 2000, "reflection base must remain 2000");
    failures += check(KAIN_DIAG_CODE_ACTOR_BASE == 3000, "actor base must remain 3000");
    failures += check(KAIN_DIAG_CODE_ASYNC_BASE == 4000, "async base must remain 4000");
    failures += check(KAIN_DIAG_CODE_UI_BASE == 5000, "ui base must remain 5000");
    failures += check(KAIN_DIAG_CODE_GFX_BASE == 6000, "gfx base must remain 6000");
    failures += check(KAIN_DIAG_CODE_PLATFORM_BASE == 7000, "platform base must remain 7000");
    failures += check(KAIN_DIAG_CODE_HOST_BRIDGE_BASE == 8000, "host bridge base must remain 8000");
    failures += check(KAIN_DIAG_CODE_MEMORY_BASE == 9000, "memory base must remain 9000");
    failures += check(KAIN_DIAG_CODE_COMPATIBILITY_BASE == 10000, "compatibility base must remain 10000");

    if (failures == 0) {
        printf("  PASS: family bases are stable\n");
    }

    return failures;
}

static int test_family_ranges_and_values(void) {
    int failures = 0;

    printf("Test 2: Family Ranges and Representative Values\n");

    failures += check(
        KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE >= KAIN_DIAG_CODE_CONTRACT_BASE &&
        KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE < KAIN_DIAG_CODE_REFLECTION_BASE,
        "contract missing-service code should stay within the contract family"
    );
    failures += check(KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE == 1004, "contract missing-service code must remain 1004");
    failures += check(KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED == 2002, "reflection parse code must remain 2002");
    failures += check(KAIN_DIAG_CODE_ACTOR_SUPERVISOR_FAILED == 3012, "actor supervisor code must remain 3012");
    failures += check(KAIN_DIAG_CODE_ASYNC_TIMER_FAILED == 4003, "async timer code must remain 4003");
    failures += check(KAIN_DIAG_CODE_UI_COMPONENT_INIT_FAILED == 5004, "ui component init code must remain 5004");
    failures += check(KAIN_DIAG_CODE_GFX_BINDING_FAILED == 6004, "gfx binding code must remain 6004");
    failures += check(KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE == 7003, "platform service unavailable code must remain 7003");
    failures += check(KAIN_DIAG_CODE_HOST_BRIDGE_ABI_MISMATCH == 8002, "host bridge ABI mismatch code must remain 8002");
    failures += check(KAIN_DIAG_CODE_MEMORY_ALIGNMENT_ERROR == 9003, "memory alignment code must remain 9003");
    failures += check(KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE == 10003, "compatibility update code must remain 10003");

    if (failures == 0) {
        printf("  PASS: representative error-code values are stable\n");
    }

    return failures;
}

static int test_name_mappings(void) {
    int failures = 0;

    printf("Test 3: Name Mappings\n");

    failures += check(
        strcmp(kain_diagnostic_subsystem_name(KAIN_DIAG_SUBSYSTEM_CONTRACT), "CONTRACT") == 0,
        "contract subsystem name must remain CONTRACT"
    );
    failures += check(
        strcmp(kain_diagnostic_subsystem_name(KAIN_DIAG_SUBSYSTEM_REFLECTION), "REFLECTION") == 0,
        "reflection subsystem name must remain REFLECTION"
    );
    failures += check(
        strcmp(kain_diagnostic_subsystem_name(KAIN_DIAG_SUBSYSTEM_ACTOR), "ACTOR") == 0,
        "actor subsystem name must remain ACTOR"
    );
    failures += check(
        strcmp(kain_diagnostic_subsystem_name(KAIN_DIAG_SUBSYSTEM_COMPATIBILITY), "COMPATIBILITY") == 0,
        "compatibility subsystem name must remain COMPATIBILITY"
    );
    failures += check(
        strcmp(kain_diagnostic_severity_name(KAIN_DIAG_SEVERITY_INFO), "INFO") == 0,
        "info severity name must remain INFO"
    );
    failures += check(
        strcmp(kain_diagnostic_severity_name(KAIN_DIAG_SEVERITY_WARNING), "WARNING") == 0,
        "warning severity name must remain WARNING"
    );
    failures += check(
        strcmp(kain_diagnostic_severity_name(KAIN_DIAG_SEVERITY_ERROR), "ERROR") == 0,
        "error severity name must remain ERROR"
    );
    failures += check(
        strcmp(kain_diagnostic_severity_name(KAIN_DIAG_SEVERITY_FATAL), "FATAL") == 0,
        "fatal severity name must remain FATAL"
    );

    if (failures == 0) {
        printf("  PASS: name mappings remain stable\n");
    }

    return failures;
}

int main(void) {
    int failures = 0;

    printf("=== KAIN Native Runtime - Diagnostic Error Code Stability Tests ===\n\n");

    failures += test_stable_family_bases();
    failures += test_family_ranges_and_values();
    failures += test_name_mappings();

    printf("\n=== Test Summary ===\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("TESTS FAILED: %d failures\n", failures);
    return 1;
}
