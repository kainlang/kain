/*
 * KAIN Native Runtime Primitive Error Path Tests
 *
 * This test validates that primitive error paths (printf + exit, null returns)
 * have been replaced with structured diagnostics while preserving compatibility.
 */

#include "../../native/include/kain_runtime_base.h"
#include "../../native/include/kain_runtime_diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* External functions from kain_runtime_core.c */
extern void* kain_alloc_rc(size_t size, long long type_tag);
extern KainArray* array_new(long long cap);
extern KainMap* map_new(void);
extern void* mq_new(void);
extern char* file_read(char* path);
extern void file_write(char* path, char* content);
extern long long socket_connect(char* host, long long port);
extern int kain_runtime_get_last_diagnostic(KainDiagnostic* out);
extern void kain_runtime_clear_last_diagnostic(void);
extern void rc_release(void* ptr);

/* Test that allocation failures emit diagnostics */
int test_allocation_diagnostics(void) {
    KainDiagnostic diag;
    
    /* Note: We can't easily force malloc to fail in a test, but we can
     * verify that the diagnostic API exists and works */
    
    /* Clear any previous diagnostic */
    kain_runtime_clear_last_diagnostic();
    
    /* Successful allocation should not emit diagnostic */
    void* ptr = kain_alloc_rc(64, 0);
    TEST_ASSERT(ptr != NULL, "Small allocation should succeed");
    
    int has_diag = kain_runtime_get_last_diagnostic(&diag);
    TEST_ASSERT(has_diag == 0, "Successful allocation should not emit diagnostic");
    
    rc_release(ptr);
    
    TEST_PASS("Allocation diagnostic API works correctly");
    return 0;
}

/* Test that file operations emit diagnostics on failure */
int test_file_operation_diagnostics(void) {
    KainDiagnostic diag;
    
    /* Clear any previous diagnostic */
    kain_runtime_clear_last_diagnostic();
    
    /* Try to read non-existent file */
    char* content = file_read("/nonexistent/path/that/does/not/exist.txt");
    TEST_ASSERT(content == NULL, "Reading non-existent file should return NULL");
    
    /* Check that diagnostic was emitted */
    int has_diag = kain_runtime_get_last_diagnostic(&diag);
    TEST_ASSERT(has_diag == 1, "Failed file read should emit diagnostic");
    TEST_ASSERT(diag.subsystem == KAIN_DIAG_SUBSYSTEM_PLATFORM,
        "File read diagnostic should be PLATFORM subsystem");
    TEST_ASSERT(diag.severity >= KAIN_DIAG_SEVERITY_ERROR,
        "File read diagnostic should be ERROR or higher");
    TEST_ASSERT(diag.code == KAIN_DIAG_CODE_PLATFORM_SERVICE_UNAVAILABLE,
        "File read diagnostic should have correct error code");
    
    /* Clear diagnostic */
    kain_runtime_clear_last_diagnostic();
    
    /* Try to write to invalid path */
    file_write("/invalid/path/that/cannot/be/written.txt", "test");
    
    /* Check that diagnostic was emitted */
    has_diag = kain_runtime_get_last_diagnostic(&diag);
    TEST_ASSERT(has_diag == 1, "Failed file write should emit diagnostic");
    TEST_ASSERT(diag.subsystem == KAIN_DIAG_SUBSYSTEM_PLATFORM,
        "File write diagnostic should be PLATFORM subsystem");
    
    TEST_PASS("File operation diagnostics work correctly");
    return 0;
}

/* Test that socket operations emit diagnostics on failure */
int test_socket_operation_diagnostics(void) {
    KainDiagnostic diag;
    
    /* Clear any previous diagnostic */
    kain_runtime_clear_last_diagnostic();
    
    /* Try to connect to invalid host */
    long long sock = socket_connect("invalid.host.that.does.not.exist.local", 12345);
    TEST_ASSERT(sock == -1, "Connecting to invalid host should return -1");
    
    /* Check that diagnostic was emitted */
    int has_diag = kain_runtime_get_last_diagnostic(&diag);
    TEST_ASSERT(has_diag == 1, "Failed socket connect should emit diagnostic");
    TEST_ASSERT(diag.subsystem == KAIN_DIAG_SUBSYSTEM_PLATFORM,
        "Socket diagnostic should be PLATFORM subsystem");
    TEST_ASSERT(diag.severity >= KAIN_DIAG_SEVERITY_ERROR,
        "Socket diagnostic should be ERROR or higher");
    
    TEST_PASS("Socket operation diagnostics work correctly");
    return 0;
}

/* Test that data structure allocation emits diagnostics */
int test_data_structure_diagnostics(void) {
    /* These should succeed and not emit diagnostics */
    kain_runtime_clear_last_diagnostic();
    
    KainArray* arr = array_new(10);
    TEST_ASSERT(arr != NULL, "Array allocation should succeed");
    rc_release(arr);
    
    KainMap* map = map_new();
    TEST_ASSERT(map != NULL, "Map allocation should succeed");
    rc_release(map);
    
    void* mq = mq_new();
    TEST_ASSERT(mq != NULL, "Message queue allocation should succeed");
    free(mq);
    
    TEST_PASS("Data structure allocation works correctly");
    return 0;
}

/* Test diagnostic formatting includes error codes */
int test_diagnostic_formatting_includes_codes(void) {
    KainDiagnostic diag;
    char buffer[512];
    
    /* Create a diagnostic with an error code */
    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_ACTOR,
        KAIN_DIAG_SEVERITY_ERROR,
        KAIN_DIAG_CODE_ACTOR_SPAWN_FAILED,
        "Test error message",
        "Test detail",
        NULL
    );
    
    /* Format the diagnostic */
    int written = kain_diagnostic_format(&diag, buffer, sizeof(buffer));
    TEST_ASSERT(written > 0, "Diagnostic formatting should write characters");
    
    /* Check that the formatted output includes the code */
    TEST_ASSERT(strstr(buffer, "Code:") != NULL || strstr(buffer, "3001") != NULL,
        "Formatted diagnostic should include error code");
    
    TEST_PASS("Diagnostic formatting includes error codes");
    return 0;
}

int main(void) {
    int result = 0;
    
    printf("=== KAIN Native Runtime Primitive Error Path Tests ===\n\n");
    
    result |= test_allocation_diagnostics();
    result |= test_file_operation_diagnostics();
    result |= test_socket_operation_diagnostics();
    result |= test_data_structure_diagnostics();
    result |= test_diagnostic_formatting_includes_codes();
    
    if (result == 0) {
        printf("\n=== All tests passed ===\n");
    } else {
        printf("\n=== Some tests failed ===\n");
    }
    
    return result;
}
