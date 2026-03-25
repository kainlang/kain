/*
 * KAIN Runtime Service Registry Conformance Test
 *
 * Tests the canonical service registry implementation including:
 * - Registry initialization
 * - Service registration
 * - Service lookup
 * - Service availability checking
 * - Service validation
 * - Integration with contract validation
 */

#include "../../native/include/kain_runtime_services.h"
#include "../../native/include/kain_runtime_diagnostics.h"
#include "../../native/include/kain_runtime_contract.h"
#include "../../native/include/kain_runtime_version.h"
#include <stdio.h>
#include <string.h>

#define TEST_PASS(name) printf("  ✅ PASS: %s\n", name)
#define TEST_FAIL(name) printf("  ❌ FAIL: %s\n", name)

int test_registry_init(void) {
    KainServiceRegistry registry;
    
    printf("\nTest 1: Registry Initialization\n");
    
    kain_service_registry_init(&registry);
    
    if (!registry.initialized) {
        TEST_FAIL("Registry not marked as initialized");
        return 0;
    }
    
    if (registry.service_count != 0) {
        TEST_FAIL("Registry service count should be 0 after init");
        return 0;
    }
    
    TEST_PASS("Registry initialized correctly");
    return 1;
}

int test_service_registration(void) {
    KainServiceRegistry registry;
    int result;
    
    printf("\nTest 2: Service Registration\n");
    
    kain_service_registry_init(&registry);
    
    /* Register a test service */
    result = kain_service_registry_register(
        &registry,
        "test.service",
        "Test Service",
        "A test service for validation",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    if (result != 0) {
        TEST_FAIL("Service registration failed");
        return 0;
    }
    
    if (registry.service_count != 1) {
        TEST_FAIL("Service count should be 1 after registration");
        return 0;
    }
    
    /* Try to register duplicate service */
    result = kain_service_registry_register(
        &registry,
        "test.service",
        "Duplicate Service",
        "Should fail",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    if (result == 0) {
        TEST_FAIL("Duplicate service registration should fail");
        return 0;
    }
    
    TEST_PASS("Service registration works correctly");
    return 1;
}

int test_service_lookup(void) {
    KainServiceRegistry registry;
    const KainServiceDescriptor* descriptor;
    
    printf("\nTest 3: Service Lookup\n");
    
    kain_service_registry_init(&registry);
    
    kain_service_registry_register(
        &registry,
        "test.lookup",
        "Lookup Test Service",
        "Service for lookup testing",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        0x00000100,
        NULL
    );
    
    /* Lookup existing service */
    descriptor = kain_service_registry_lookup(&registry, "test.lookup");
    if (!descriptor) {
        TEST_FAIL("Failed to lookup registered service");
        return 0;
    }
    
    if (strcmp(descriptor->key, "test.lookup") != 0) {
        TEST_FAIL("Lookup returned wrong service");
        return 0;
    }
    
    /* Lookup non-existent service */
    descriptor = kain_service_registry_lookup(&registry, "nonexistent.service");
    if (descriptor != NULL) {
        TEST_FAIL("Lookup should return NULL for non-existent service");
        return 0;
    }
    
    TEST_PASS("Service lookup works correctly");
    return 1;
}

int test_service_availability(void) {
    KainServiceRegistry registry;
    
    printf("\nTest 4: Service Availability Checking\n");
    
    kain_service_registry_init(&registry);
    
    /* Register available service */
    kain_service_registry_register(
        &registry,
        "test.available",
        "Available Service",
        "Available service",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    /* Register unavailable service */
    kain_service_registry_register(
        &registry,
        "test.unavailable",
        "Unavailable Service",
        "Unavailable service",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_UNAVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        0x00000100,
        NULL
    );
    
    if (!kain_service_registry_is_available(&registry, "test.available")) {
        TEST_FAIL("Available service should be available");
        return 0;
    }
    
    if (kain_service_registry_is_available(&registry, "test.unavailable")) {
        TEST_FAIL("Unavailable service should not be available");
        return 0;
    }
    
    if (kain_service_registry_is_available(&registry, "nonexistent")) {
        TEST_FAIL("Non-existent service should not be available");
        return 0;
    }
    
    TEST_PASS("Service availability checking works correctly");
    return 1;
}

int test_service_counting(void) {
    KainServiceRegistry registry;
    int count;
    
    printf("\nTest 5: Service Counting\n");
    
    kain_service_registry_init(&registry);
    
    /* Register services with different statuses */
    kain_service_registry_register(
        &registry,
        "test.available1",
        "Available 1",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    kain_service_registry_register(
        &registry,
        "test.available2",
        "Available 2",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        0x00000100,
        NULL
    );
    
    kain_service_registry_register(
        &registry,
        "test.unavailable",
        "Unavailable",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_UNAVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        0x00000100,
        NULL
    );
    
    /* Count by status */
    count = kain_service_registry_count_by_status(&registry, KAIN_SERVICE_STATUS_AVAILABLE);
    if (count != 2) {
        printf("    Expected 2 available services, got %d\n", count);
        TEST_FAIL("Count by status (available) incorrect");
        return 0;
    }
    
    count = kain_service_registry_count_by_status(&registry, KAIN_SERVICE_STATUS_UNAVAILABLE);
    if (count != 1) {
        printf("    Expected 1 unavailable service, got %d\n", count);
        TEST_FAIL("Count by status (unavailable) incorrect");
        return 0;
    }
    
    /* Count by requirement */
    count = kain_service_registry_count_by_requirement(&registry, KAIN_SERVICE_REQUIREMENT_REQUIRED);
    if (count != 1) {
        printf("    Expected 1 required service, got %d\n", count);
        TEST_FAIL("Count by requirement (required) incorrect");
        return 0;
    }
    
    count = kain_service_registry_count_by_requirement(&registry, KAIN_SERVICE_REQUIREMENT_OPTIONAL);
    if (count != 2) {
        printf("    Expected 2 optional services, got %d\n", count);
        TEST_FAIL("Count by requirement (optional) incorrect");
        return 0;
    }
    
    TEST_PASS("Service counting works correctly");
    return 1;
}

int test_required_service_validation(void) {
    KainServiceRegistry registry;
    KainDiagnostic diagnostics[8];
    int diagnostic_count = 0;
    int failures;
    
    printf("\nTest 6: Required Service Validation\n");
    
    kain_service_registry_init(&registry);
    
    /* Register required available service */
    kain_service_registry_register(
        &registry,
        "test.required.ok",
        "Required OK",
        NULL,
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
        "Required Missing",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_UNAVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    /* Register optional unavailable service (should not fail) */
    kain_service_registry_register(
        &registry,
        "test.optional.missing",
        "Optional Missing",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_UNAVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        0x00000100,
        NULL
    );
    
    failures = kain_service_registry_validate_required(
        &registry,
        diagnostics,
        8,
        &diagnostic_count
    );
    
    if (failures != 1) {
        printf("    Expected 1 failure, got %d\n", failures);
        TEST_FAIL("Should have 1 required service failure");
        return 0;
    }
    
    if (diagnostic_count != 1) {
        printf("    Expected 1 diagnostic, got %d\n", diagnostic_count);
        TEST_FAIL("Should have 1 diagnostic");
        return 0;
    }
    
    if (diagnostics[0].code != KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE) {
        printf("    Expected code %d, got %d\n",
            KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE, diagnostics[0].code);
        TEST_FAIL("Diagnostic should have correct error code");
        return 0;
    }
    
    TEST_PASS("Required service validation works correctly");
    return 1;
}

int test_contract_integration(void) {
    KainServiceRegistry* registry;
    
    printf("\nTest 7: Contract Integration\n");
    
    /* Get global registry */
    registry = kain_service_registry_global();
    if (!registry) {
        TEST_FAIL("Failed to get global registry");
        return 0;
    }
    
    /* Populate with native services */
    kain_runtime_contract_populate_service_registry(registry);
    
    if (registry->service_count != 5) {
        printf("    Expected 5 services, got %d\n", registry->service_count);
        TEST_FAIL("Should have 5 native services registered");
        return 0;
    }
    
    /* Check that all expected services are available */
    if (!kain_service_registry_is_available(registry, KAIN_SERVICE_KEY_PLATFORM_APP_HOST)) {
        TEST_FAIL("platform.app-host should be available");
        return 0;
    }
    
    if (!kain_service_registry_is_available(registry, KAIN_SERVICE_KEY_PLATFORM_INPUT)) {
        TEST_FAIL("platform.input should be available");
        return 0;
    }
    
    if (!kain_service_registry_is_available(registry, KAIN_SERVICE_KEY_GFX_VIEWPORT)) {
        TEST_FAIL("gfx.viewport should be available");
        return 0;
    }
    
    if (!kain_service_registry_is_available(registry, KAIN_SERVICE_KEY_ASSET_GLTF)) {
        TEST_FAIL("asset.gltf should be available");
        return 0;
    }
    
    if (!kain_service_registry_is_available(registry, KAIN_SERVICE_KEY_UI_BUNDLE)) {
        TEST_FAIL("ui.bundle should be available");
        return 0;
    }
    
    /* Test legacy service key mapping */
    if (!kain_runtime_contract_is_service_available("native.app-host")) {
        TEST_FAIL("Legacy native.app-host key should work");
        return 0;
    }
    
    if (!kain_runtime_contract_is_service_available("native.input")) {
        TEST_FAIL("Legacy native.input key should work");
        return 0;
    }
    
    TEST_PASS("Contract integration works correctly");
    return 1;
}

int test_service_registry_print(void) {
    KainServiceRegistry registry;
    
    printf("\nTest 8: Service Registry Printing\n");
    
    kain_service_registry_init(&registry);
    
    kain_service_registry_register(
        &registry,
        "test.print",
        "Print Test Service",
        "Service for print testing",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        0x00000100,
        NULL
    );
    
    printf("  Printing registry:\n");
    kain_service_registry_print(&registry);
    
    TEST_PASS("Service registry printing executed");
    return 1;
}

int main(void) {
    int passed = 0;
    int total = 8;
    
    printf("=== KAIN Runtime Service Registry Test ===\n");
    
    if (test_registry_init()) passed++;
    if (test_service_registration()) passed++;
    if (test_service_lookup()) passed++;
    if (test_service_availability()) passed++;
    if (test_service_counting()) passed++;
    if (test_required_service_validation()) passed++;
    if (test_contract_integration()) passed++;
    if (test_service_registry_print()) passed++;
    
    printf("\n=== Test Results: %d/%d Passed ===\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}

