/*
 * KAIN Native Runtime - Service Validation with Diagnostic Collector
 *
 * Tests the integration between service registry validation and the new
 * diagnostic collector APIs.
 */

#include "kain_runtime_services.h"
#include "kain_runtime_diagnostics.h"
#include "kain_runtime_base.h"
#include <stdio.h>
#include <string.h>

/*
 * Test: Service Validation with Collector
 */
int test_service_validation_with_collector(void) {
    KainServiceRegistry registry;
    KainDiagnosticCollector collector;
    int failures_detected;
    int test_failures = 0;
    
    printf("TEST: Service Validation with Diagnostic Collector\n");
    
    kain_service_registry_init(&registry);
    kain_diagnostic_collector_init(&collector);
    
    /* Register required available service */
    kain_service_registry_register(
        &registry,
        "test.required.ok",
        "Required OK Service",
        "This service is required and available",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    /* Register required unavailable service */
    kain_service_registry_register(
        &registry,
        "test.required.missing",
        "Required Missing Service",
        "This service is required but unavailable",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_UNAVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    /* Register required degraded service */
    kain_service_registry_register(
        &registry,
        "test.required.degraded",
        "Required Degraded Service",
        "This service is required but degraded",
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32,
        KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    /* Register optional unavailable service (should not generate diagnostic) */
    kain_service_registry_register(
        &registry,
        "test.optional.missing",
        "Optional Missing Service",
        "This service is optional and unavailable",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_UNAVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        0x00000100,
        NULL
    );
    
    /* Validate required services */
    failures_detected = kain_service_registry_validate_required_collector(&registry, &collector);
    
    /* Should detect 2 failures (missing and degraded) */
    if (failures_detected != 2) {
        printf("  FAIL: Expected 2 failures, got %d\n", failures_detected);
        test_failures++;
    }
    
    /* Should have 2 diagnostics in collector */
    if (collector.count != 2) {
        printf("  FAIL: Expected 2 diagnostics, got %d\n", collector.count);
        test_failures++;
    }
    
    /* All diagnostics should be errors */
    if (collector.error_count != 2) {
        printf("  FAIL: Expected 2 errors, got %d\n", collector.error_count);
        test_failures++;
    }
    
    /* Should have errors */
    if (!kain_diagnostic_collector_has_errors(&collector)) {
        printf("  FAIL: Collector should have errors\n");
        test_failures++;
    }
    
    if (test_failures == 0) {
        printf("  PASS: Service validation with collector works correctly\n");
        printf("\n--- Collected Diagnostics ---\n");
        kain_diagnostic_collector_print_all(&collector);
    }
    
    return test_failures;
}

/*
 * Test: Startup Validation Integration
 */
int test_startup_validation_integration(void) {
    KainServiceRegistry registry;
    KainStartupValidationResult result;
    int test_failures = 0;
    
    printf("\nTEST: Startup Validation Integration\n");
    
    kain_service_registry_init(&registry);
    kain_startup_validation_result_init(&result);
    
    /* Register some services */
    kain_service_registry_register(&registry, "base.memory", "Base Memory", NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE, KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED, 0x00000100, NULL);
    
    kain_service_registry_register(&registry, "base.diagnostics", "Base Diagnostics", NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE, KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED, 0x00000100, NULL);
    
    kain_service_registry_register(&registry, "platform.window", "Platform Window", NULL,
        KAIN_SERVICE_PROVIDER_PLATFORM_WIN32, KAIN_SERVICE_STATUS_DEGRADED,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL, 0x00000100, NULL);
    
    /* Populate result */
    result.runtime_abi_version = KAIN_RUNTIME_ABI_VERSION_CURRENT;
    result.runtime_version = KAIN_RUNTIME_VERSION_CURRENT;
    result.bundle_abi_version = KAIN_RUNTIME_ABI_VERSION_CURRENT;
    
    /* Count services */
    result.required_services_available = kain_service_registry_count_by_requirement(
        &registry, KAIN_SERVICE_REQUIREMENT_REQUIRED);
    result.optional_services_available = 0;
    result.optional_services_degraded = 1;
    
    /* Validate required services */
    int failures = kain_service_registry_validate_required_collector(&registry, &result.diagnostics);
    
    result.validation_passed = (failures == 0);
    
    if (failures == 0) {
        strncpy(result.summary, "All required services available", sizeof(result.summary) - 1);
    } else {
        snprintf(result.summary, sizeof(result.summary),
            "Validation failed: %d required services unavailable", failures);
    }
    
    /* Add warning for degraded optional service */
    kain_diagnostic_collector_add_new(&result.diagnostics,
        KAIN_DIAG_SUBSYSTEM_PLATFORM,
        KAIN_DIAG_SEVERITY_WARNING,
        KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
        "Optional service degraded",
        "Service 'platform.window' is running in degraded mode",
        NULL);
    
    /* Verify result */
    if (!result.validation_passed) {
        printf("  FAIL: Validation should have passed\n");
        test_failures++;
    }
    
    if (result.diagnostics.warning_count != 1) {
        printf("  FAIL: Expected 1 warning, got %d\n", result.diagnostics.warning_count);
        test_failures++;
    }
    
    if (test_failures == 0) {
        printf("  PASS: Startup validation integration works correctly\n");
        printf("\n--- Startup Validation Report ---\n");
        kain_startup_validation_result_print(&result);
    }
    
    return test_failures;
}

/*
 * Main Test Runner
 */
int main(void) {
    int total_failures = 0;
    
    printf("=== Service Validation with Diagnostic Collector Tests ===\n\n");
    
    total_failures += test_service_validation_with_collector();
    total_failures += test_startup_validation_integration();
    
    printf("\n=== Test Summary ===\n");
    if (total_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("TESTS FAILED: %d failures\n", total_failures);
        return 1;
    }
}
