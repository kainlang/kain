/*
 * Test: Runtime Version Information API
 * 
 * Validates that the runtime version API correctly exposes:
 * - ABI version (major.minor.patch)
 * - Runtime version (major.minor.patch)
 * - Build information (date/time)
 * - Formatted version strings
 */

#include "../../native/include/kain_runtime_version.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    KainRuntimeVersionInfo info;
    int result;
    
    printf("=== KAIN Runtime Version Information Test ===\n\n");
    
    /* Test 1: Get version info */
    printf("Test 1: kain_runtime_version_get_info()\n");
    result = kain_runtime_version_get_info(&info);
    if (result != 0) {
        printf("  ❌ FAIL: kain_runtime_version_get_info() returned %d\n", result);
        return 1;
    }
    printf("  ✅ PASS: Version info retrieved successfully\n\n");
    
    /* Test 2: Verify ABI version fields */
    printf("Test 2: ABI Version Fields\n");
    printf("  ABI Version: %u.%u.%u (encoded: 0x%08X)\n",
           info.abi_version_major,
           info.abi_version_minor,
           info.abi_version_patch,
           info.abi_version_encoded);
    
    if (info.abi_version_major != KAIN_RUNTIME_ABI_VERSION_MAJOR ||
        info.abi_version_minor != KAIN_RUNTIME_ABI_VERSION_MINOR ||
        info.abi_version_patch != KAIN_RUNTIME_ABI_VERSION_PATCH) {
        printf("  ❌ FAIL: ABI version mismatch\n");
        return 1;
    }
    
    if (info.abi_version_encoded != KAIN_RUNTIME_ABI_VERSION_CURRENT) {
        printf("  ❌ FAIL: Encoded ABI version mismatch\n");
        return 1;
    }
    printf("  ✅ PASS: ABI version fields match constants\n\n");
    
    /* Test 3: Verify runtime version fields */
    printf("Test 3: Runtime Version Fields\n");
    printf("  Runtime Version: %u.%u.%u (encoded: 0x%08X)\n",
           info.runtime_version_major,
           info.runtime_version_minor,
           info.runtime_version_patch,
           info.runtime_version_encoded);
    
    if (info.runtime_version_major != KAIN_RUNTIME_VERSION_MAJOR ||
        info.runtime_version_minor != KAIN_RUNTIME_VERSION_MINOR ||
        info.runtime_version_patch != KAIN_RUNTIME_VERSION_PATCH) {
        printf("  ❌ FAIL: Runtime version mismatch\n");
        return 1;
    }
    
    if (info.runtime_version_encoded != KAIN_RUNTIME_VERSION_CURRENT) {
        printf("  ❌ FAIL: Encoded runtime version mismatch\n");
        return 1;
    }
    printf("  ✅ PASS: Runtime version fields match constants\n\n");
    
    /* Test 4: Verify formatted strings */
    printf("Test 4: Formatted Version Strings\n");
    printf("  ABI Version String: '%s'\n", info.abi_version_string);
    printf("  Runtime Version String: '%s'\n", info.runtime_version_string);
    printf("  Build Info: '%s'\n", info.build_info_string);
    
    if (strlen(info.abi_version_string) == 0) {
        printf("  ❌ FAIL: ABI version string is empty\n");
        return 1;
    }
    
    if (strlen(info.runtime_version_string) == 0) {
        printf("  ❌ FAIL: Runtime version string is empty\n");
        return 1;
    }
    
    if (strlen(info.build_info_string) == 0) {
        printf("  ❌ FAIL: Build info string is empty\n");
        return 1;
    }
    printf("  ✅ PASS: All version strings are populated\n\n");
    
    /* Test 5: Test version formatting functions */
    printf("Test 5: Version Formatting Functions\n");
    {
        char buffer[64];
        int written;
        
        written = kain_runtime_version_format_abi(
            KAIN_RUNTIME_ABI_VERSION_CURRENT,
            buffer,
            sizeof(buffer)
        );
        
        if (written < 0) {
            printf("  ❌ FAIL: kain_runtime_version_format_abi() failed\n");
            return 1;
        }
        
        printf("  Formatted ABI: '%s'\n", buffer);
        
        written = kain_runtime_version_format_runtime(
            KAIN_RUNTIME_VERSION_CURRENT,
            buffer,
            sizeof(buffer)
        );
        
        if (written < 0) {
            printf("  ❌ FAIL: kain_runtime_version_format_runtime() failed\n");
            return 1;
        }
        
        printf("  Formatted Runtime: '%s'\n", buffer);
        printf("  ✅ PASS: Version formatting functions work\n\n");
    }
    
    /* Test 6: Test ABI compatibility checking */
    printf("Test 6: ABI Compatibility Checking\n");
    {
        unsigned int same_version = KAIN_RUNTIME_ABI_VERSION_CURRENT;
        unsigned int compatible_version = KAIN_RUNTIME_ABI_VERSION_ENCODE(0, 0, 0);
        unsigned int incompatible_major = KAIN_RUNTIME_ABI_VERSION_ENCODE(1, 0, 0);
        unsigned int incompatible_minor = KAIN_RUNTIME_ABI_VERSION_ENCODE(0, 2, 0);
        
        if (!kain_runtime_version_check_abi_compatibility(same_version)) {
            printf("  ❌ FAIL: Same version should be compatible\n");
            return 1;
        }
        printf("  ✅ Same version is compatible\n");
        
        if (!kain_runtime_version_check_abi_compatibility(compatible_version)) {
            printf("  ❌ FAIL: 0.0.0 should be compatible with 0.1.0\n");
            return 1;
        }
        printf("  ✅ Lower minor version is compatible\n");
        
        if (kain_runtime_version_check_abi_compatibility(incompatible_major)) {
            printf("  ❌ FAIL: Different major version should be incompatible\n");
            return 1;
        }
        printf("  ✅ Different major version is incompatible\n");
        
        if (kain_runtime_version_check_abi_compatibility(incompatible_minor)) {
            printf("  ❌ FAIL: Higher minor version should be incompatible\n");
            return 1;
        }
        printf("  ✅ Higher minor version is incompatible\n");
        printf("  ✅ PASS: ABI compatibility checking works correctly\n\n");
    }
    
    /* Test 7: Test print function */
    printf("Test 7: Print Version Information\n");
    kain_runtime_version_print_info();
    printf("  ✅ PASS: Print function executed\n\n");
    
    printf("=== All Tests Passed ===\n");
    return 0;
}
