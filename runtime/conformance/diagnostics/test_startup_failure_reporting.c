/*
 * KAIN Native Runtime - Startup Failure Reporting Tests
 *
 * Validates structured startup diagnostics, contract failure reporting, and
 * degraded optional-service reporting through the canonical validation APIs.
 */

#include "../../native/include/kain_runtime_contract.h"
#include "../../native/include/kain_runtime_diagnostics.h"
#include "../../native/include/kain_runtime_services.h"
#include "../../native/include/kain_runtime_version.h"
#include "../../native/include/kain_runtime_win32.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char* out, size_t out_cap, const char* text) {
    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!text) {
        return;
    }
    strncpy(out, text, out_cap - 1);
    out[out_cap - 1] = '\0';
}

static int count_service_bits(KainRuntimeServiceMask mask) {
    int count = 0;

    while (mask != 0) {
        count += (mask & UINT64_C(1)) != 0 ? 1 : 0;
        mask >>= 1;
    }
    return count;
}

static int test_required_service_failure_reporting(void) {
    KainRuntimeContractBundle bundle;
    KainRuntimeContractValidation validation;
    KainStartupValidationResult result;
    char report[1024];
    int passed;

    printf("Test 1: Required Service Failure Reporting\n");

#ifdef _WIN32
    kain_env_set_flag(KAIN_RUNTIME_CONTRACT_STRICT_ENV, 1);
#endif

    kain_runtime_contract_init(&bundle);
    bundle.loaded = 1;
    bundle.target_is_llvm = 1;
    bundle.required_abi_version = KAIN_RUNTIME_ABI_VERSION_CURRENT;
    bundle.service_mask = 0;
    copy_text(bundle.target, sizeof(bundle.target), "llvm");
    copy_text(bundle.load_origin, sizeof(bundle.load_origin), "test");
    copy_text(bundle.source_path, sizeof(bundle.source_path), "runtime/contracts/missing-required.runtime_contract.json");

    kain_runtime_contract_validation_init(&validation);
    passed = kain_runtime_contract_validate_startup(
        &bundle,
        KAIN_RUNTIME_SERVICE_CORE_MASK,
        KAIN_RUNTIME_SERVICE_OPTIONAL_MASK,
        &validation
    );

    if (passed != 0) {
        printf("  FAIL: strict startup validation should have failed\n");
        return 1;
    }

    if (!validation.contract_present) {
        printf("  FAIL: validation should report that the contract is present\n");
        return 1;
    }

    if (!validation.fatal_error) {
        printf("  FAIL: validation should report a fatal startup error\n");
        return 1;
    }

    if (validation.missing_required_mask != KAIN_RUNTIME_SERVICE_CORE_MASK) {
        printf("  FAIL: missing required mask mismatch\n");
        return 1;
    }

    if (strstr(validation.fatal_message, "missing required services") == NULL) {
        printf("  FAIL: fatal message should mention missing required services\n");
        return 1;
    }

    kain_startup_validation_result_init(&result);
    if (kain_runtime_contract_validate_startup_enhanced(
            &bundle,
            KAIN_RUNTIME_SERVICE_CORE_MASK,
            KAIN_RUNTIME_SERVICE_OPTIONAL_MASK,
            &result)) {
        printf("  FAIL: enhanced validation should have failed\n");
        return 1;
    }

    if (result.validation_passed) {
        printf("  FAIL: enhanced result should report failure\n");
        return 1;
    }

    if (result.diagnostics.count != 1) {
        printf("  FAIL: expected exactly one fatal diagnostic, got %d\n", result.diagnostics.count);
        return 1;
    }

    if (result.diagnostics.fatal_count != 1) {
        printf("  FAIL: expected one fatal diagnostic in the collector\n");
        return 1;
    }

    if (result.diagnostics.diagnostics[0].subsystem != KAIN_DIAG_SUBSYSTEM_CONTRACT) {
        printf("  FAIL: diagnostic subsystem should be CONTRACT\n");
        return 1;
    }

    if (result.diagnostics.diagnostics[0].severity != KAIN_DIAG_SEVERITY_FATAL) {
        printf("  FAIL: diagnostic severity should be FATAL\n");
        return 1;
    }

    if (result.diagnostics.diagnostics[0].code != KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE) {
        printf("  FAIL: diagnostic code should be CONTRACT_MISSING_SERVICE\n");
        return 1;
    }

    if (strcmp(result.diagnostics.diagnostics[0].source_path, bundle.source_path) != 0) {
        printf("  FAIL: diagnostic source path should match the bundle path\n");
        return 1;
    }

    if (result.bundle_abi_version != KAIN_RUNTIME_ABI_VERSION_CURRENT) {
        printf("  FAIL: bundle ABI version should be preserved in the result\n");
        return 1;
    }

    if (kain_startup_validation_result_format(&result, report, sizeof(report)) <= 0) {
        printf("  FAIL: failed to format startup validation report\n");
        return 1;
    }

    if (strstr(report, "FAILED") == NULL) {
        printf("  FAIL: formatted report should show FAILED status\n");
        return 1;
    }

    if (strstr(report, "1 total") == NULL || strstr(report, "1 fatals") == NULL) {
        printf("  FAIL: formatted report should summarize the fatal diagnostic\n");
        return 1;
    }

    printf("  PASS: strict startup failures are reported with structured diagnostics\n");
    return 0;
}

static int test_optional_service_downgrade_reporting(void) {
    KainRuntimeContractBundle bundle;
    KainStartupValidationResult result;
    KainRuntimeContractValidation validation;
    char report[1024];
    int expected_optional_service_count;
    int passed;

    printf("Test 2: Optional Service Downgrade Reporting\n");
    expected_optional_service_count = count_service_bits(KAIN_RUNTIME_SERVICE_OPTIONAL_MASK);

    kain_runtime_contract_init(&bundle);
    bundle.loaded = 1;
    bundle.target_is_llvm = 1;
    bundle.required_abi_version = KAIN_RUNTIME_ABI_VERSION_CURRENT;
    bundle.service_mask = KAIN_RUNTIME_SERVICE_CORE_MASK;
    copy_text(bundle.target, sizeof(bundle.target), "llvm");
    copy_text(bundle.load_origin, sizeof(bundle.load_origin), "test");
    copy_text(bundle.source_path, sizeof(bundle.source_path), "runtime/contracts/degraded-optional.runtime_contract.json");

    kain_runtime_contract_validation_init(&validation);
    passed = kain_runtime_contract_validate_startup(
        &bundle,
        KAIN_RUNTIME_SERVICE_CORE_MASK,
        KAIN_RUNTIME_SERVICE_OPTIONAL_MASK,
        &validation
    );

    if (passed != 1) {
        printf("  FAIL: validation should succeed when only optional services are missing\n");
        return 1;
    }

    if (validation.fatal_error) {
        printf("  FAIL: optional downgrade should not be fatal\n");
        return 1;
    }

    kain_startup_validation_result_init(&result);
    if (!kain_runtime_contract_validate_startup_enhanced(
            &bundle,
            KAIN_RUNTIME_SERVICE_CORE_MASK,
            KAIN_RUNTIME_SERVICE_OPTIONAL_MASK,
            &result)) {
        printf("  FAIL: enhanced validation should succeed for optional downgrades\n");
        return 1;
    }

    if (!result.validation_passed) {
        printf("  FAIL: enhanced result should report success\n");
        return 1;
    }

    if (result.required_services_available != 4) {
        printf("  FAIL: expected 4 required services available, got %d\n", result.required_services_available);
        return 1;
    }

    if (result.optional_services_available != 0) {
        printf("  FAIL: expected 0 optional services available, got %d\n", result.optional_services_available);
        return 1;
    }

    if (result.optional_services_degraded != expected_optional_service_count) {
        printf(
            "  FAIL: expected %d degraded optional services, got %d\n",
            expected_optional_service_count,
            result.optional_services_degraded
        );
        return 1;
    }

    if (result.diagnostics.count != 1 || result.diagnostics.warning_count != 1) {
        printf("  FAIL: expected one warning diagnostic for degraded optional services\n");
        return 1;
    }

    if (result.diagnostics.diagnostics[0].severity != KAIN_DIAG_SEVERITY_WARNING) {
        printf("  FAIL: optional downgrade diagnostic should be a warning\n");
        return 1;
    }

    if (result.diagnostics.diagnostics[0].code != KAIN_DIAG_CODE_SUCCESS) {
        printf("  FAIL: optional downgrade warning should preserve success code\n");
        return 1;
    }

    if (strstr(result.diagnostics.diagnostics[0].message, "Optional runtime services unavailable") == NULL) {
        printf("  FAIL: warning message should explain the downgrade\n");
        return 1;
    }

    if (kain_startup_validation_result_format(&result, report, sizeof(report)) <= 0) {
        printf("  FAIL: failed to format optional downgrade report\n");
        return 1;
    }

    if (strstr(report, "PASSED") == NULL) {
        printf("  FAIL: formatted report should show PASSED status\n");
        return 1;
    }

    printf("  PASS: optional downgrades are reported without failing startup\n");
    return 0;
}

int main(void) {
    int failures = 0;

    printf("=== KAIN Native Runtime - Startup Failure Reporting Tests ===\n\n");

    failures += test_required_service_failure_reporting();
    failures += test_optional_service_downgrade_reporting();

    printf("\n=== Test Summary ===\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }

    printf("TESTS FAILED: %d failures\n", failures);
    return 1;
}
