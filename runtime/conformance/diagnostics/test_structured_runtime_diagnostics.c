/*
 * KAIN Native Runtime - Structured Diagnostics Tests
 *
 * Validates structured diagnostic creation, formatting, and collector
 * behavior using the canonical runtime diagnostics service.
 */

#include "../../native/include/kain_runtime_diagnostics.h"
#include "../../native/include/kain_runtime_version.h"

#include <stdio.h>
#include <string.h>

static int test_diagnostic_record_creation(void) {
    KainDiagnostic diag;
    char buffer[1024];

    printf("Test 1: Diagnostic Record Creation\n");

    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_CONTRACT,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
        "Required runtime service is missing",
        "service=platform.app-host",
        "runtime/contracts/native.runtime_contract.json"
    );

    if (diag.subsystem != KAIN_DIAG_SUBSYSTEM_CONTRACT) {
        printf("  FAIL: subsystem mismatch\n");
        return 1;
    }

    if (diag.severity != KAIN_DIAG_SEVERITY_ERROR) {
        printf("  FAIL: severity mismatch\n");
        return 1;
    }

    if (diag.code != KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE) {
        printf("  FAIL: code mismatch\n");
        return 1;
    }

    if (strcmp(diag.message, "Required runtime service is missing") != 0) {
        printf("  FAIL: message mismatch\n");
        return 1;
    }

    if (strcmp(diag.detail, "service=platform.app-host") != 0) {
        printf("  FAIL: detail mismatch\n");
        return 1;
    }

    if (strcmp(diag.source_path, "runtime/contracts/native.runtime_contract.json") != 0) {
        printf("  FAIL: source path mismatch\n");
        return 1;
    }

    if (diag.runtime_abi_version != KAIN_RUNTIME_ABI_VERSION_CURRENT) {
        printf(
            "  FAIL: runtime ABI mismatch (expected 0x%08X, got 0x%08X)\n",
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            diag.runtime_abi_version
        );
        return 1;
    }

    if (kain_diagnostic_format(&diag, buffer, sizeof(buffer)) <= 0) {
        printf("  FAIL: diagnostic formatting failed\n");
        return 1;
    }

    if (strstr(buffer, "CONTRACT") == NULL) {
        printf("  FAIL: formatted diagnostic missing subsystem name\n");
        return 1;
    }

    if (strstr(buffer, "ERROR") == NULL) {
        printf("  FAIL: formatted diagnostic missing severity name\n");
        return 1;
    }

    if (strstr(buffer, "Required runtime service is missing") == NULL) {
        printf("  FAIL: formatted diagnostic missing message\n");
        return 1;
    }

    if (strstr(buffer, "service=platform.app-host") == NULL) {
        printf("  FAIL: formatted diagnostic missing detail\n");
        return 1;
    }

    if (strstr(buffer, "runtime/contracts/native.runtime_contract.json") == NULL) {
        printf("  FAIL: formatted diagnostic missing source path\n");
        return 1;
    }

    if (strstr(buffer, "Code: 1004") == NULL) {
        printf("  FAIL: formatted diagnostic missing stable code\n");
        return 1;
    }

    printf("  PASS: diagnostic record and formatting are correct\n");
    return 0;
}

static int test_collector_aggregation(void) {
    KainDiagnosticCollector collector;
    char summary[256];

    printf("Test 2: Diagnostic Collector Aggregation\n");

    kain_diagnostic_collector_init(&collector);

    if (collector.count != 0 || collector.error_count != 0 ||
        collector.warning_count != 0 || collector.fatal_count != 0) {
        printf("  FAIL: collector did not initialize cleanly\n");
        return 1;
    }

    if (kain_diagnostic_collector_add_new(
            &collector,
            KAIN_DIAG_SUBSYSTEM_CONTRACT,
            KAIN_DIAG_SEVERITY_INFO,
            KAIN_DIAG_CODE_SUCCESS,
            "Runtime contract found",
            NULL,
            "runtime/contracts/native.runtime_contract.json") != 0) {
        printf("  FAIL: failed to add info diagnostic\n");
        return 1;
    }

    if (kain_diagnostic_collector_add_new(
            &collector,
            KAIN_DIAG_SUBSYSTEM_CONTRACT,
            KAIN_DIAG_SEVERITY_WARNING,
            KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
            "Optional runtime service downgraded",
            "service=asset.gltf",
            "runtime/contracts/native.runtime_contract.json") != 0) {
        printf("  FAIL: failed to add warning diagnostic\n");
        return 1;
    }

    if (kain_diagnostic_collector_add_new(
            &collector,
            KAIN_DIAG_SUBSYSTEM_CONTRACT,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
            "Required runtime service missing",
            "service=platform.viewport",
            "runtime/contracts/native.runtime_contract.json") != 0) {
        printf("  FAIL: failed to add error diagnostic\n");
        return 1;
    }

    if (kain_diagnostic_collector_add_new(
            &collector,
            KAIN_DIAG_SUBSYSTEM_COMPATIBILITY,
            KAIN_DIAG_SEVERITY_FATAL,
            KAIN_DIAG_CODE_COMPAT_VERSION_MISMATCH,
            "Runtime ABI mismatch",
            "runtime=0.1.0 contract=0.2.0",
            "runtime/contracts/native.runtime_contract.json") != 0) {
        printf("  FAIL: failed to add fatal diagnostic\n");
        return 1;
    }

    if (collector.count != 4) {
        printf("  FAIL: expected 4 diagnostics, got %d\n", collector.count);
        return 1;
    }

    if (kain_diagnostic_collector_count_by_severity(&collector, KAIN_DIAG_SEVERITY_INFO) != 1) {
        printf("  FAIL: info count mismatch\n");
        return 1;
    }

    if (kain_diagnostic_collector_count_by_severity(&collector, KAIN_DIAG_SEVERITY_WARNING) != 1) {
        printf("  FAIL: warning count mismatch\n");
        return 1;
    }

    if (kain_diagnostic_collector_count_by_severity(&collector, KAIN_DIAG_SEVERITY_ERROR) != 1) {
        printf("  FAIL: error count mismatch\n");
        return 1;
    }

    if (kain_diagnostic_collector_count_by_severity(&collector, KAIN_DIAG_SEVERITY_FATAL) != 1) {
        printf("  FAIL: fatal count mismatch\n");
        return 1;
    }

    if (!kain_diagnostic_collector_has_errors(&collector)) {
        printf("  FAIL: collector should report errors\n");
        return 1;
    }

    if (!kain_diagnostic_collector_has_fatals(&collector)) {
        printf("  FAIL: collector should report fatals\n");
        return 1;
    }

    if (kain_diagnostic_collector_format_summary(&collector, summary, sizeof(summary)) <= 0) {
        printf("  FAIL: collector summary formatting failed\n");
        return 1;
    }

    if (strstr(summary, "4 total") == NULL ||
        strstr(summary, "1 errors") == NULL ||
        strstr(summary, "1 warnings") == NULL ||
        strstr(summary, "1 fatals") == NULL) {
        printf("  FAIL: collector summary missing expected counts\n");
        return 1;
    }

    kain_diagnostic_collector_clear(&collector);
    if (collector.count != 0 || collector.error_count != 0 ||
        collector.warning_count != 0 || collector.fatal_count != 0) {
        printf("  FAIL: collector did not clear cleanly\n");
        return 1;
    }

    printf("  PASS: collector aggregation behaves correctly\n");
    return 0;
}

int main(void) {
    int failures = 0;

    printf("=== KAIN Native Runtime - Structured Diagnostics Tests ===\n\n");

    failures += test_diagnostic_record_creation();
    failures += test_collector_aggregation();

    printf("\n=== Test Summary ===\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("TESTS FAILED: %d failures\n", failures);
    return 1;
}
