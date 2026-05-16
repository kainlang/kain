/*
 * KAIN Native Runtime - Diagnostic Collection Tests
 *
 * Tests for diagnostic collector and startup validation result APIs.
 * Validates diagnostic aggregation, reporting, and batch operations.
 */

#include "diagnostics.h"
#include "version.h"
#include "base.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/*
 * Test: Diagnostic Collector Initialization
 */
int test_collector_init(void) {
    KainDiagnosticCollector collector;
    int failures = 0;

    printf("TEST: Diagnostic Collector Initialization\n");

    kain_diagnostic_collector_init(&collector);

    if (collector.count != 0) {
        printf("  FAIL: Expected count=0, got %d\n", collector.count);
        failures++;
    }

    if (collector.error_count != 0) {
        printf("  FAIL: Expected error_count=0, got %d\n", collector.error_count);
        failures++;
    }

    if (collector.warning_count != 0) {
        printf("  FAIL: Expected warning_count=0, got %d\n", collector.warning_count);
        failures++;
    }

    if (collector.fatal_count != 0) {
        printf("  FAIL: Expected fatal_count=0, got %d\n", collector.fatal_count);
        failures++;
    }

    if (failures == 0) {
        printf("  PASS: Collector initialized correctly\n");
    }

    return failures;
}

/*
 * Test: Adding Diagnostics to Collector
 */
int test_collector_add(void) {
    KainDiagnosticCollector collector;
    KainDiagnostic diag;
    int result;
    int failures = 0;

    printf("\nTEST: Adding Diagnostics to Collector\n");

    kain_diagnostic_collector_init(&collector);

    /* Add an error diagnostic */
    kain_diagnostic_create(&diag,
        KAIN_DIAG_SUBSYSTEM_CONTRACT,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_CONTRACT_NOT_FOUND,
        "Contract file not found",
        "Expected contract.json in bundle",
        "/path/to/bundle");

    result = kain_diagnostic_collector_add(&collector, &diag);

    if (result != 0) {
        printf("  FAIL: Failed to add diagnostic (result=%d)\n", result);
        failures++;
    }

    if (collector.count != 1) {
        printf("  FAIL: Expected count=1, got %d\n", collector.count);
        failures++;
    }

    if (collector.error_count != 1) {
        printf("  FAIL: Expected error_count=1, got %d\n", collector.error_count);
        failures++;
    }

    /* Add a warning diagnostic */
    kain_diagnostic_create(&diag,
        KAIN_DIAG_SUBSYSTEM_PLATFORM,
        KAIN_DIAG_SEVERITY_WARNING,
        KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
        "Optional service unavailable",
        "Service 'platform.window' is not available",
        NULL);

    result = kain_diagnostic_collector_add(&collector, &diag);

    if (result != 0) {
        printf("  FAIL: Failed to add warning diagnostic (result=%d)\n", result);
        failures++;
    }

    if (collector.count != 2) {
        printf("  FAIL: Expected count=2, got %d\n", collector.count);
        failures++;
    }

    if (collector.warning_count != 1) {
        printf("  FAIL: Expected warning_count=1, got %d\n", collector.warning_count);
        failures++;
    }

    if (failures == 0) {
        printf("  PASS: Diagnostics added correctly\n");
    }

    return failures;
}

/*
 * Test: Collector Add New (Convenience Function)
 */
int test_collector_add_new(void) {
    KainDiagnosticCollector collector;
    int result;
    int failures = 0;

    printf("\nTEST: Collector Add New (Convenience)\n");

    kain_diagnostic_collector_init(&collector);

    result = kain_diagnostic_collector_add_new(&collector,
        KAIN_DIAG_SUBSYSTEM_ACTOR,
        KAIN_DIAG_SEVERITY_FATAL,
        KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED,
        "Actor spawn failed",
        "Failed to allocate actor state",
        NULL);

    if (result != 0) {
        printf("  FAIL: Failed to add new diagnostic (result=%d)\n", result);
        failures++;
    }

    if (collector.count != 1) {
        printf("  FAIL: Expected count=1, got %d\n", collector.count);
        failures++;
    }

    if (collector.fatal_count != 1) {
        printf("  FAIL: Expected fatal_count=1, got %d\n", collector.fatal_count);
        failures++;
    }

    /* Verify the diagnostic was created correctly */
    if (collector.diagnostics[0].subsystem != KAIN_DIAG_SUBSYSTEM_ACTOR) {
        printf("  FAIL: Wrong subsystem\n");
        failures++;
    }

    if (collector.diagnostics[0].severity != KAIN_DIAG_SEVERITY_FATAL) {
        printf("  FAIL: Wrong severity\n");
        failures++;
    }

    if (collector.diagnostics[0].code != KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED) {
        printf("  FAIL: Wrong code\n");
        failures++;
    }

    if (failures == 0) {
        printf("  PASS: Add new convenience function works correctly\n");
    }

    return failures;
}

/*
 * Test: Collector Error Detection
 */
int test_collector_has_errors(void) {
    KainDiagnosticCollector collector;
    int failures = 0;

    printf("\nTEST: Collector Error Detection\n");

    kain_diagnostic_collector_init(&collector);

    if (kain_diagnostic_collector_has_errors(&collector)) {
        printf("  FAIL: Empty collector should not have errors\n");
        failures++;
    }

    /* Add a warning - should not trigger has_errors */
    kain_diagnostic_collector_add_new(&collector,
        KAIN_DIAG_SUBSYSTEM_UI,
        KAIN_DIAG_SEVERITY_WARNING,
        KAIN_DIAG_CODE_UI_COMPONENT_INIT_FAILED,
        "Component init warning",
        NULL, NULL);

    if (kain_diagnostic_collector_has_errors(&collector)) {
        printf("  FAIL: Warnings should not trigger has_errors\n");
        failures++;
    }

    /* Add an error - should trigger has_errors */
    kain_diagnostic_collector_add_new(&collector,
        KAIN_DIAG_SUBSYSTEM_GFX,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_GFX_SHADER_LOAD_FAILED,
        "Shader load failed",
        NULL, NULL);

    if (!kain_diagnostic_collector_has_errors(&collector)) {
        printf("  FAIL: Error should trigger has_errors\n");
        failures++;
    }

    if (failures == 0) {
        printf("  PASS: Error detection works correctly\n");
    }

    return failures;
}

/*
 * Test: Collector Fatal Detection
 */
int test_collector_has_fatals(void) {
    KainDiagnosticCollector collector;
    int failures = 0;

    printf("\nTEST: Collector Fatal Detection\n");

    kain_diagnostic_collector_init(&collector);

    if (kain_diagnostic_collector_has_fatals(&collector)) {
        printf("  FAIL: Empty collector should not have fatals\n");
        failures++;
    }

    /* Add an error - should not trigger has_fatals */
    kain_diagnostic_collector_add_new(&collector,
        KAIN_DIAG_SUBSYSTEM_MEMORY,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
        "Memory allocation failed",
        NULL, NULL);

    if (kain_diagnostic_collector_has_fatals(&collector)) {
        printf("  FAIL: Errors should not trigger has_fatals\n");
        failures++;
    }

    /* Add a fatal - should trigger has_fatals */
    kain_diagnostic_collector_add_new(&collector,
        KAIN_DIAG_SUBSYSTEM_COMPATIBILITY,
        KAIN_DIAG_SEVERITY_FATAL,
        KAIN_DIAG_CODE_COMPAT_VERSION_MISMATCH,
        "Fatal version mismatch",
        NULL, NULL);

    if (!kain_diagnostic_collector_has_fatals(&collector)) {
        printf("  FAIL: Fatal should trigger has_fatals\n");
        failures++;
    }

    if (failures == 0) {
        printf("  PASS: Fatal detection works correctly\n");
    }

    return failures;
}

/*
 * Test: Collector Clear
 */
int test_collector_clear(void) {
    KainDiagnosticCollector collector;
    int failures = 0;

    printf("\nTEST: Collector Clear\n");

    kain_diagnostic_collector_init(&collector);

    /* Add some diagnostics */
    kain_diagnostic_collector_add_new(&collector,
        KAIN_DIAG_SUBSYSTEM_ASYNC,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED,
        "Task spawn failed", NULL, NULL);

    kain_diagnostic_collector_add_new(&collector,
        KAIN_DIAG_SUBSYSTEM_ASYNC,
        KAIN_DIAG_SEVERITY_WARNING,
        KAIN_DIAG_CODE_ASYNC_TASK_CANCELLED,
        "Task cancelled", NULL, NULL);

    if (collector.count != 2) {
        printf("  FAIL: Expected count=2 before clear, got %d\n", collector.count);
        failures++;
    }

    /* Clear the collector */
    kain_diagnostic_collector_clear(&collector);

    if (collector.count != 0) {
        printf("  FAIL: Expected count=0 after clear, got %d\n", collector.count);
        failures++;
    }

    if (collector.error_count != 0) {
        printf("  FAIL: Expected error_count=0 after clear, got %d\n", collector.error_count);
        failures++;
    }

    if (collector.warning_count != 0) {
        printf("  FAIL: Expected warning_count=0 after clear, got %d\n", collector.warning_count);
        failures++;
    }

    if (failures == 0) {
        printf("  PASS: Collector cleared correctly\n");
    }

    return failures;
}

/*
 * Test: Startup Validation Result
 */
int test_startup_validation_result(void) {
    KainStartupValidationResult result;
    KainRuntimeVersionInfo version_info;
    int failures = 0;

    printf("\nTEST: Startup Validation Result\n");

    kain_startup_validation_result_init(&result);

    /* Populate version information */
    if (version_get_info(&version_info) == 0) {
        result.runtime_abi_version = version_info.abi_version_encoded;
        result.runtime_version = version_info.runtime_version_encoded;
    }

    result.bundle_abi_version = RUNTIME_ABI_VERSION_CURRENT;
    result.validation_passed = 1;
    result.required_services_available = 5;
    result.optional_services_available = 3;
    result.optional_services_degraded = 1;

    strncpy(result.summary, "Startup validation completed successfully", sizeof(result.summary) - 1);

    /* Add a warning diagnostic */
    kain_diagnostic_collector_add_new(&result.diagnostics,
        KAIN_DIAG_SUBSYSTEM_PLATFORM,
        KAIN_DIAG_SEVERITY_WARNING,
        KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
        "Optional service degraded",
        "Service 'platform.window' is running in degraded mode",
        NULL);

    if (result.diagnostics.count != 1) {
        printf("  FAIL: Expected 1 diagnostic, got %d\n", result.diagnostics.count);
        failures++;
    }

    /* Format and verify the result */
    char buffer[2048];
    int written = kain_startup_validation_result_format(&result, buffer, sizeof(buffer));

    if (written <= 0) {
        printf("  FAIL: Failed to format startup validation result\n");
        failures++;
    }

    if (strstr(buffer, "PASSED") == NULL) {
        printf("  FAIL: Expected 'PASSED' in formatted output\n");
        failures++;
    }

    if (strstr(buffer, "Required Services Available: 5") == NULL) {
        printf("  FAIL: Expected service count in formatted output\n");
        failures++;
    }

    if (failures == 0) {
        printf("  PASS: Startup validation result works correctly\n");
        printf("\n--- Formatted Output ---\n%s\n", buffer);
    }

    return failures;
}

/*
 * Main Test Runner
 */
int main(void) {
    int total_failures = 0;

    printf("=== KAIN Native Runtime - Diagnostic Collection Tests ===\n\n");

    total_failures += test_collector_init();
    total_failures += test_collector_add();
    total_failures += test_collector_add_new();
    total_failures += test_collector_has_errors();
    total_failures += test_collector_has_fatals();
    total_failures += test_collector_clear();
    total_failures += test_startup_validation_result();

    printf("\n=== Test Summary ===\n");
    if (total_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("TESTS FAILED: %d failures\n", total_failures);
        return 1;
    }
}
