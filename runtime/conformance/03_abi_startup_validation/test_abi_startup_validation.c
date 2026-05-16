/*
 * KAIN Runtime ABI and Startup Validation Test
 *
 * Validates:
 * - Runtime version exposure (Requirements 1.5, 2.2)
 * - Service registry resolution (Requirements 2.5)
 * - Startup mismatch failures (Requirements 2.2, 2.5)
 * - Required vs optional service reporting (Requirements 13.1)
 *
 * This test covers Task 1.6 of Phase 1: Canonical ABI, Service Tables, and Version Metadata
 */

#include "../../native/include/version.h"
#include "../../native/include/services.h"
#include "../../native/include/diagnostics.h"
#include "../../native/include/contract.h"
#include <stdio.h>
#include <string.h>

#define TEST_PASS(name) printf("  ✅ PASS: %s\n", name)
#define TEST_FAIL(name, ...) do { printf("  ❌ FAIL: " name "\n", ##__VA_ARGS__); return 0; } while(0)

/* Test counters */
static int tests_passed = 0;
static int tests_total = 0;

#define RUN_TEST(test_func) do { \
    tests_total++; \
    printf("\n"); \
    if (test_func()) { \
        tests_passed++; \
    } \
} while(0)

/*
 * Test 1: Runtime Version Exposure
 * Validates that runtime version information is correctly exposed
 */
int test_runtime_version_exposure(void) {
    KainRuntimeVersionInfo info;

    printf("Test 1: Runtime Version Exposure\n");

    /* Get version info */
    if (version_get_info(&info) != 0) {
        TEST_FAIL("Failed to get runtime version info");
    }

    /* Verify ABI version is exposed */
    if (info.abi_version_encoded != RUNTIME_ABI_VERSION_CURRENT) {
        TEST_FAIL("ABI version mismatch: expected 0x%08X, got 0x%08X",
                  RUNTIME_ABI_VERSION_CURRENT, info.abi_version_encoded);
    }

    /* Verify runtime version is exposed */
    if (info.runtime_version_encoded != VERSION_CURRENT) {
        TEST_FAIL("Runtime version mismatch: expected 0x%08X, got 0x%08X",
                  VERSION_CURRENT, info.runtime_version_encoded);
    }

    /* Verify version strings are populated */
    if (strlen(info.abi_version_string) == 0) {
        TEST_FAIL("ABI version string is empty");
    }

    if (strlen(info.runtime_version_string) == 0) {
        TEST_FAIL("Runtime version string is empty");
    }

    if (strlen(info.build_info_string) == 0) {
        TEST_FAIL("Build info string is empty");
    }

    printf("  Runtime Version: %s\n", info.runtime_version_string);
    printf("  ABI Version: %s\n", info.abi_version_string);
    printf("  Build Info: %s\n", info.build_info_string);

    TEST_PASS("Runtime version correctly exposed");
    return 1;
}

/*
 * Test 2: Service Registry Resolution
 * Validates that services can be registered and resolved
 */
int test_service_registry_resolution(void) {
    KainServiceRegistry registry;
    const KainServiceDescriptor* descriptor;

    printf("Test 2: Service Registry Resolution\n");

    kain_service_registry_init(&registry);

    /* Register test services */
    if (kain_service_registry_register(
            &registry,
            "test.service.required",
            "Required Test Service",
            "A required service for testing",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_REQUIRED,
            RUNTIME_ABI_VERSION_CURRENT,
            NULL) != 0) {
        TEST_FAIL("Failed to register required service");
    }

    if (kain_service_registry_register(
            &registry,
            "test.service.optional",
            "Optional Test Service",
            "An optional service for testing",
            KAIN_SERVICE_PROVIDER_NATIVE_CORE,
            KAIN_SERVICE_STATUS_AVAILABLE,
            KAIN_SERVICE_REQUIREMENT_OPTIONAL,
            RUNTIME_ABI_VERSION_CURRENT,
            NULL) != 0) {
        TEST_FAIL("Failed to register optional service");
    }

    /* Test service lookup */
    descriptor = kain_service_registry_lookup(&registry, "test.service.required");
    if (!descriptor) {
        TEST_FAIL("Failed to lookup required service");
    }

    if (strcmp(descriptor->key, "test.service.required") != 0) {
        TEST_FAIL("Lookup returned wrong service");
    }

    /* Test availability check */
    if (!kain_service_registry_is_available(&registry, "test.service.required")) {
        TEST_FAIL("Required service should be available");
    }

    if (!kain_service_registry_is_available(&registry, "test.service.optional")) {
        TEST_FAIL("Optional service should be available");
    }

    /* Test non-existent service */
    if (kain_service_registry_is_available(&registry, "nonexistent.service")) {
        TEST_FAIL("Non-existent service should not be available");
    }

    printf("  Registered services: %d\n", registry.service_count);
    printf("  Required service resolved: %s\n", descriptor->name);

    TEST_PASS("Service registry resolution works correctly");
    return 1;
}

/*
 * Test 3: Required Service Validation
 * Validates that missing required services are detected
 */
int test_required_service_validation(void) {
    KainServiceRegistry registry;
    KainDiagnostic diagnostics[8];
    int diagnostic_count = 0;
    int failures;

    printf("Test 3: Required Service Validation\n");

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
        RUNTIME_ABI_VERSION_CURRENT,
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
        RUNTIME_ABI_VERSION_CURRENT,
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
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );

    /* Validate required services */
    failures = kain_service_registry_validate_required(
        &registry,
        diagnostics,
        8,
        &diagnostic_count
    );

    if (failures != 1) {
        TEST_FAIL("Expected 1 failure, got %d", failures);
    }

    if (diagnostic_count != 1) {
        TEST_FAIL("Expected 1 diagnostic, got %d", diagnostic_count);
    }

    if (diagnostics[0].code != KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE) {
        TEST_FAIL("Expected error code %d, got %d",
                  KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE, diagnostics[0].code);
    }

    printf("  Required failures detected: %d\n", failures);
    printf("  Diagnostic: %s\n", diagnostics[0].message);

    TEST_PASS("Required service validation works correctly");
    return 1;
}

/*
 * Test 4: Optional Service Reporting
 * Validates that optional services are correctly reported
 */
int test_optional_service_reporting(void) {
    KainServiceRegistry registry;
    int required_count, optional_count;
    int available_count, unavailable_count;

    printf("Test 4: Optional Service Reporting\n");

    kain_service_registry_init(&registry);

    /* Register mix of required and optional services */
    kain_service_registry_register(
        &registry,
        "test.required.1",
        "Required 1",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );

    kain_service_registry_register(
        &registry,
        "test.required.2",
        "Required 2",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );

    kain_service_registry_register(
        &registry,
        "test.optional.1",
        "Optional 1",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );

    kain_service_registry_register(
        &registry,
        "test.optional.2",
        "Optional 2",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_UNAVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );

    /* Count by requirement */
    required_count = kain_service_registry_count_by_requirement(
        &registry, KAIN_SERVICE_REQUIREMENT_REQUIRED);
    optional_count = kain_service_registry_count_by_requirement(
        &registry, KAIN_SERVICE_REQUIREMENT_OPTIONAL);

    if (required_count != 2) {
        TEST_FAIL("Expected 2 required services, got %d", required_count);
    }

    if (optional_count != 2) {
        TEST_FAIL("Expected 2 optional services, got %d", optional_count);
    }

    /* Count by status */
    available_count = kain_service_registry_count_by_status(
        &registry, KAIN_SERVICE_STATUS_AVAILABLE);
    unavailable_count = kain_service_registry_count_by_status(
        &registry, KAIN_SERVICE_STATUS_UNAVAILABLE);

    if (available_count != 3) {
        TEST_FAIL("Expected 3 available services, got %d", available_count);
    }

    if (unavailable_count != 1) {
        TEST_FAIL("Expected 1 unavailable service, got %d", unavailable_count);
    }

    printf("  Required services: %d\n", required_count);
    printf("  Optional services: %d\n", optional_count);
    printf("  Available services: %d\n", available_count);
    printf("  Unavailable services: %d\n", unavailable_count);

    TEST_PASS("Optional service reporting works correctly");
    return 1;
}

/*
 * Test 5: ABI Version Compatibility Checking
 * Validates that ABI compatibility is correctly checked
 */
int test_abi_compatibility_checking(void) {
    unsigned int same_version, compatible_version;
    unsigned int incompatible_major, incompatible_minor;

    printf("Test 5: ABI Version Compatibility Checking\n");

    same_version = RUNTIME_ABI_VERSION_CURRENT;
    compatible_version = RUNTIME_ABI_VERSION_ENCODE(0, 0, 0);
    incompatible_major = RUNTIME_ABI_VERSION_ENCODE(1, 0, 0);
    incompatible_minor = RUNTIME_ABI_VERSION_ENCODE(0, 2, 0);

    /* Test same version */
    if (!version_check_abi_compatibility(same_version)) {
        TEST_FAIL("Same version should be compatible");
    }

    /* Test compatible lower minor version */
    if (!version_check_abi_compatibility(compatible_version)) {
        TEST_FAIL("Lower minor version should be compatible");
    }

    /* Test incompatible major version */
    if (version_check_abi_compatibility(incompatible_major)) {
        TEST_FAIL("Different major version should be incompatible");
    }

    /* Test incompatible higher minor version */
    if (version_check_abi_compatibility(incompatible_minor)) {
        TEST_FAIL("Higher minor version should be incompatible");
    }

    printf("  Same version: compatible\n");
    printf("  Lower minor: compatible\n");
    printf("  Different major: incompatible\n");
    printf("  Higher minor: incompatible\n");

    TEST_PASS("ABI compatibility checking works correctly");
    return 1;
}

/*
 * Test 6: Startup Mismatch Detection
 * Validates that startup mismatches are detected and reported
 */
int test_startup_mismatch_detection(void) {
    KainServiceRegistry registry;
    KainDiagnostic diagnostics[8];
    int diagnostic_count = 0;
    int failures;

    printf("Test 6: Startup Mismatch Detection\n");

    kain_service_registry_init(&registry);

    /* Register services with mismatched ABI versions */
    kain_service_registry_register(
        &registry,
        "test.service.old_abi",
        "Old ABI Service",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_ENCODE(0, 0, 0),  /* Old ABI */
        NULL
    );

    kain_service_registry_register(
        &registry,
        "test.service.current_abi",
        "Current ABI Service",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );

    /* Validate - should pass since both services are available */
    failures = kain_service_registry_validate_required(
        &registry,
        diagnostics,
        8,
        &diagnostic_count
    );

    if (failures != 0) {
        TEST_FAIL("Should have no failures when all required services are available");
    }

    /* Now test with unavailable required service */
    kain_service_registry_init(&registry);

    kain_service_registry_register(
        &registry,
        "test.service.missing",
        "Missing Service",
        NULL,
        KAIN_SERVICE_PROVIDER_NATIVE_CORE,
        KAIN_SERVICE_STATUS_UNAVAILABLE,
        KAIN_SERVICE_REQUIREMENT_REQUIRED,
        RUNTIME_ABI_VERSION_CURRENT,
        NULL
    );

    diagnostic_count = 0;
    failures = kain_service_registry_validate_required(
        &registry,
        diagnostics,
        8,
        &diagnostic_count
    );

    if (failures != 1) {
        TEST_FAIL("Expected 1 failure for missing required service, got %d", failures);
    }

    if (diagnostic_count != 1) {
        TEST_FAIL("Expected 1 diagnostic, got %d", diagnostic_count);
    }

    /* Verify diagnostic contains proper information */
    if (diagnostics[0].subsystem != KAIN_DIAG_SUBSYSTEM_CONTRACT) {
        TEST_FAIL("Expected CONTRACT subsystem, got %d", diagnostics[0].subsystem);
    }

    if (diagnostics[0].severity != KAIN_DIAG_SEVERITY_ERROR) {
        TEST_FAIL("Expected ERROR severity, got %d", diagnostics[0].severity);
    }

    printf("  Mismatch detected: %s\n", diagnostics[0].message);
    printf("  Diagnostic code: %d\n", diagnostics[0].code);

    TEST_PASS("Startup mismatch detection works correctly");
    return 1;
}

/*
 * Test 7: Global Service Registry Integration
 * Validates that the global registry is properly initialized
 */
int test_global_registry_integration(void) {
    KainServiceRegistry* global_registry;
    const int expected_service_count = 31;

    printf("Test 7: Global Service Registry Integration\n");

    /* Get global registry */
    global_registry = kain_service_registry_global();
    if (!global_registry) {
        TEST_FAIL("Failed to get global registry");
    }

    if (!global_registry->initialized) {
        TEST_FAIL("Global registry not initialized");
    }

    kain_service_registry_init(global_registry);

    /* Populate with native services */
    contract_populate_service_registry(global_registry);

    if (global_registry->service_count != expected_service_count) {
        TEST_FAIL("Expected %d native runtime services, got %d",
                  expected_service_count, global_registry->service_count);
    }

    /* Verify expected native services are available */
    if (!kain_service_registry_is_available(global_registry, KAIN_SERVICE_KEY_PLATFORM_APP_HOST)) {
        TEST_FAIL("platform.app-host should be available");
    }

    if (!kain_service_registry_is_available(global_registry, KAIN_SERVICE_KEY_PLATFORM_INPUT)) {
        TEST_FAIL("platform.input should be available");
    }

    if (!kain_service_registry_is_available(global_registry, KAIN_SERVICE_KEY_CONTRACT)) {
        TEST_FAIL("contract should be available");
    }

    if (!kain_service_registry_is_available(global_registry, KAIN_SERVICE_KEY_ACTOR_RUNTIME)) {
        TEST_FAIL("actor.runtime should be available");
    }

    if (!kain_service_registry_is_available(global_registry, KAIN_SERVICE_KEY_ASYNC_TIMERS)) {
        TEST_FAIL("async.timers should be available");
    }

    if (!kain_service_registry_is_available(global_registry, KAIN_SERVICE_KEY_COMPATIBILITY)) {
        TEST_FAIL("compatibility should be available");
    }

    if (!kain_service_registry_is_available(global_registry, KAIN_SERVICE_KEY_SCENE_QUERY)) {
        TEST_FAIL("scene.query should be available");
    }

    if (!kain_service_registry_is_available(global_registry, KAIN_SERVICE_KEY_DEVICE_REFLECTION)) {
        TEST_FAIL("device.reflection should be available");
    }

    if (kain_service_registry_get_status(global_registry, KAIN_SERVICE_KEY_GFX_VIEWPORT) !=
            KAIN_SERVICE_STATUS_DEGRADED) {
        TEST_FAIL("gfx.viewport should be degraded");
    }

    if (kain_service_registry_get_status(global_registry, KAIN_SERVICE_KEY_ASSET_GLTF) !=
            KAIN_SERVICE_STATUS_DEGRADED) {
        TEST_FAIL("asset.gltf should be degraded");
    }

    printf("  Global registry services: %d\n", global_registry->service_count);

    TEST_PASS("Global service registry integration works correctly");
    return 1;
}

/*
 * Test 8: Diagnostic Formatting
 * Validates that diagnostics are properly formatted
 */
int test_diagnostic_formatting(void) {
    KainDiagnostic diag;
    char buffer[512];
    int written;

    printf("Test 8: Diagnostic Formatting\n");

    /* Create a test diagnostic */
    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_CONTRACT,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE,
        "Required service not available",
        "Service 'test.service' is required but unavailable",
        "runtime/contract.json"
    );

    /* Format diagnostic */
    written = kain_diagnostic_format(&diag, buffer, sizeof(buffer));

    if (written <= 0) {
        TEST_FAIL("Failed to format diagnostic");
    }

    if (strlen(buffer) == 0) {
        TEST_FAIL("Formatted diagnostic is empty");
    }

    /* Verify diagnostic contains key information */
    if (strstr(buffer, "CONTRACT") == NULL) {
        TEST_FAIL("Formatted diagnostic missing subsystem");
    }

    if (strstr(buffer, "ERROR") == NULL) {
        TEST_FAIL("Formatted diagnostic missing severity");
    }

    printf("  Formatted diagnostic:\n  %s\n", buffer);

    TEST_PASS("Diagnostic formatting works correctly");
    return 1;
}

/*
 * Main Test Runner
 */
int main(void) {
    printf("=== KAIN Runtime ABI and Startup Validation Test ===\n");
    printf("Task 1.6: Add ABI and startup validation tests\n");
    printf("Requirements: 1.5, 2.2, 2.5, 13.1\n");

    RUN_TEST(test_runtime_version_exposure);
    RUN_TEST(test_service_registry_resolution);
    RUN_TEST(test_required_service_validation);
    RUN_TEST(test_optional_service_reporting);
    RUN_TEST(test_abi_compatibility_checking);
    RUN_TEST(test_startup_mismatch_detection);
    RUN_TEST(test_global_registry_integration);
    RUN_TEST(test_diagnostic_formatting);

    printf("\n=== Test Results: %d/%d Passed ===\n", tests_passed, tests_total);

    if (tests_passed == tests_total) {
        printf("✅ All tests passed!\n");
        return 0;
    } else {
        printf("❌ %d test(s) failed\n", tests_total - tests_passed);
        return 1;
    }
}
