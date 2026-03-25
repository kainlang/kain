/*
 * KAIN Native Runtime Error Code Stability Tests
 *
 * This test validates that error codes remain stable and within their
 * designated ranges. It ensures that the error code families defined in
 * NATIVE_RUNTIME_ERROR_CODES.md are correctly implemented.
 */

#include "../../native/include/kain_runtime_diagnostics.h"
#include <stdio.h>
#include <stdlib.h>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

#define TEST_PASS(message) \
    do { \
        printf("PASS: %s\n", message); \
    } while (0)

/* Test that error code bases are correctly defined */
int test_error_code_bases(void) {
    TEST_ASSERT(KAIN_DIAG_CODE_CONTRACT_BASE == 1000, 
        "Contract error base should be 1000");
    TEST_ASSERT(KAIN_DIAG_CODE_REFLECTION_BASE == 2000, 
        "Reflection error base should be 2000");
    TEST_ASSERT(KAIN_DIAG_CODE_ACTOR_BASE == 3000, 
        "Actor error base should be 3000");
    TEST_ASSERT(KAIN_DIAG_CODE_ASYNC_BASE == 4000, 
        "Async error base should be 4000");
    TEST_ASSERT(KAIN_DIAG_CODE_UI_BASE == 5000, 
        "UI error base should be 5000");
    TEST_ASSERT(KAIN_DIAG_CODE_GFX_BASE == 6000, 
        "Graphics error base should be 6000");
    TEST_ASSERT(KAIN_DIAG_CODE_PLATFORM_BASE == 7000, 
        "Platform error base should be 7000");
    TEST_ASSERT(KAIN_DIAG_CODE_HOST_BRIDGE_BASE == 8000, 
        "Host bridge error base should be 8000");
    TEST_ASSERT(KAIN_DIAG_CODE_MEMORY_BASE == 9000, 
        "Memory error base should be 9000");
    TEST_ASSERT(KAIN_DIAG_CODE_COMPATIBILITY_BASE == 10000, 
        "Compatibility error base should be 10000");
    
    TEST_PASS("Error code bases are correctly defined");
    return 0;
}

/* Test that specific error codes are within their designated ranges */
int test_error_code_ranges(void) {
    /* Contract codes (1000-1999) */
    TEST_ASSERT(KAIN_DIAG_CODE_CONTRACT_NOT_FOUND >= 1000 && 
                KAIN_DIAG_CODE_CONTRACT_NOT_FOUND < 2000,
        "CONTRACT_NOT_FOUND should be in range 1000-1999");
    TEST_ASSERT(KAIN_DIAG_CODE_CONTRACT_ABI_MISMATCH >= 1000 && 
                KAIN_DIAG_CODE_CONTRACT_ABI_MISMATCH < 2000,
        "CONTRACT_ABI_MISMATCH should be in range 1000-1999");
    
    /* Reflection codes (2000-2999) */
    TEST_ASSERT(KAIN_DIAG_CODE_REFLECTION_NOT_FOUND >= 2000 && 
                KAIN_DIAG_CODE_REFLECTION_NOT_FOUND < 3000,
        "REFLECTION_NOT_FOUND should be in range 2000-2999");
    
    /* Actor codes (3000-3999) */
    TEST_ASSERT(KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED >= 3000 && 
                KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED < 4000,
        "ACTOR_SPAWN_FAILED should be in range 3000-3999");
    TEST_ASSERT(KAIN_DIAG_CODE_ACTOR_MAILBOX_FULL >= 3000 && 
                KAIN_DIAG_CODE_ACTOR_MAILBOX_FULL < 4000,
        "ACTOR_MAILBOX_FULL should be in range 3000-3999");
    
    /* Async codes (4000-4999) */
    TEST_ASSERT(KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED >= 4000 && 
                KAIN_DIAG_CODE_ASYNC_TASK_SPAWN_FAILED < 5000,
        "ASYNC_TASK_SPAWN_FAILED should be in range 4000-4999");
    
    /* UI codes (5000-5999) */
    TEST_ASSERT(KAIN_DIAG_CODE_UI_BUNDLE_NOT_FOUND >= 5000 && 
                KAIN_DIAG_CODE_UI_BUNDLE_NOT_FOUND < 6000,
        "UI_BUNDLE_NOT_FOUND should be in range 5000-5999");
    
    /* Graphics codes (6000-6999) */
    TEST_ASSERT(KAIN_DIAG_CODE_GFX_SHADER_LOAD_FAILED >= 6000 && 
                KAIN_DIAG_CODE_GFX_SHADER_LOAD_FAILED < 7000,
        "GFX_SHADER_LOAD_FAILED should be in range 6000-6999");
    
    /* Platform codes (7000-7999) */
    TEST_ASSERT(KAIN_DIAG_CODE_PLATFORM_UNSUPPORTED >= 7000 && 
                KAIN_DIAG_CODE_PLATFORM_UNSUPPORTED < 8000,
        "PLATFORM_UNSUPPORTED should be in range 7000-7999");
    
    /* Host bridge codes (8000-8999) */
    TEST_ASSERT(KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED >= 8000 && 
                KAIN_DIAG_CODE_HOST_BRIDGE_LOAD_FAILED < 9000,
        "HOST_BRIDGE_LOAD_FAILED should be in range 8000-8999");
    
    /* Memory codes (9000-9999) */
    TEST_ASSERT(KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED >= 9000 && 
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED < 10000,
        "MEMORY_ALLOC_FAILED should be in range 9000-9999");
    
    /* Compatibility codes (10000-10999) */
    TEST_ASSERT(KAIN_DIAG_CODE_COMPAT_VERSION_MISMATCH >= 10000 && 
                KAIN_DIAG_CODE_COMPAT_VERSION_MISMATCH < 11000,
        "COMPAT_VERSION_MISMATCH should be in range 10000-10999");
    
    TEST_PASS("All error codes are within their designated ranges");
    return 0;
}

/* Test that specific error codes have stable values */
int test_error_code_stability(void) {
    /* These values must never change to maintain backward compatibility */
    TEST_ASSERT(KAIN_DIAG_CODE_SUCCESS == 0, 
        "SUCCESS code must be 0");
    TEST_ASSERT(KAIN_DIAG_CODE_GENERIC_ERROR == 1, 
        "GENERIC_ERROR code must be 1");
    
    /* Contract codes */
    TEST_ASSERT(KAIN_DIAG_CODE_CONTRACT_NOT_FOUND == 1001, 
        "CONTRACT_NOT_FOUND must be 1001");
    TEST_ASSERT(KAIN_DIAG_CODE_CONTRACT_PARSE_FAILED == 1002, 
        "CONTRACT_PARSE_FAILED must be 1002");
    TEST_ASSERT(KAIN_DIAG_CODE_CONTRACT_INVALID_SCHEMA == 1003, 
        "CONTRACT_INVALID_SCHEMA must be 1003");
    TEST_ASSERT(KAIN_DIAG_CODE_CONTRACT_MISSING_SERVICE == 1004, 
        "CONTRACT_MISSING_SERVICE must be 1004");
    TEST_ASSERT(KAIN_DIAG_CODE_CONTRACT_ABI_MISMATCH == 1005, 
        "CONTRACT_ABI_MISMATCH must be 1005");
    
    /* Actor codes */
    TEST_ASSERT(KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED == 3001, 
        "ACTOR_SPAWN_FAILED must be 3001");
    TEST_ASSERT(KAIN_DIAG_CODE_ACTOR_MAILBOX_FULL == 3002, 
        "ACTOR_MAILBOX_FULL must be 3002");
    
    /* Memory codes */
    TEST_ASSERT(KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED == 9001, 
        "MEMORY_ALLOC_FAILED must be 9001");
    TEST_ASSERT(KAIN_DIAG_CODE_MEMORY_INVALID_POINTER == 9002, 
        "MEMORY_INVALID_POINTER must be 9002");
    
    TEST_PASS("Error codes have stable values");
    return 0;
}

/* Test diagnostic creation with various error codes */
int test_diagnostic_with_error_codes(void) {
    KainDiagnostic diag;
    
    /* Test contract error */
    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_CONTRACT,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_CONTRACT_NOT_FOUND,
        "Contract not found",
        "Test detail",
        "/test/path.json"
    );
    TEST_ASSERT(diag.code == KAIN_DIAG_CODE_CONTRACT_NOT_FOUND,
        "Diagnostic should preserve error code");
    TEST_ASSERT(diag.subsystem == KAIN_DIAG_SUBSYSTEM_CONTRACT,
        "Diagnostic should preserve subsystem");
    
    /* Test actor error */
    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_ACTOR,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED,
        "Actor spawn failed",
        NULL,
        NULL
    );
    TEST_ASSERT(diag.code == KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED,
        "Diagnostic should preserve actor error code");
    
    /* Test memory error */
    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_MEMORY,
        KAIN_DIAG_SEVERITY_FATAL,
        KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
        "Memory allocation failed",
        "Out of memory",
        NULL
    );
    TEST_ASSERT(diag.code == KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
        "Diagnostic should preserve memory error code");
    TEST_ASSERT(diag.severity == KAIN_DIAG_SEVERITY_FATAL,
        "Diagnostic should preserve severity");
    
    TEST_PASS("Diagnostics correctly preserve error codes");
    return 0;
}

int main(void) {
    int result = 0;
    
    printf("=== KAIN Native Runtime Error Code Stability Tests ===\n\n");
    
    result |= test_error_code_bases();
    result |= test_error_code_ranges();
    result |= test_error_code_stability();
    result |= test_diagnostic_with_error_codes();
    
    if (result == 0) {
        printf("\n=== All tests passed ===\n");
    } else {
        printf("\n=== Some tests failed ===\n");
    }
    
    return result;
}
