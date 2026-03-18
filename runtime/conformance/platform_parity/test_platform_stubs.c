/*
 * Platform Parity Conformance Test: Unsupported Platform Stubs
 */

#include "../../native/include/kain_runtime_platform.h"
#include "../../native/include/kain_runtime_diagnostics.h"
#include <stdio.h>
#include <string.h>

static int require(int condition, const char* message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static int check_stub(KainPlatformKind kind, const char* kind_name) {
    KainPlatformDescriptor descriptor;
    KainDiagnostic diag;
    unsigned int required_mask = KAIN_PLATFORM_SERVICE_APP_HOST |
        KAIN_PLATFORM_SERVICE_INPUT |
        KAIN_PLATFORM_SERVICE_GRAPHICS;

    kain_platform_describe_kind(kind, &descriptor);
    kain_diagnostic_init(&diag);

    printf("Testing stub: %s\n", kind_name);
    printf("Diagnostic: %s\n", descriptor.diagnostic);

    if (!require(descriptor.supported == 0, "stub platform should be marked unsupported")) {
        return 0;
    }
    if (!require(kain_platform_supports_kind(kind, required_mask) == 0, "stub platform should not support app/input/graphics")) {
        return 0;
    }
    if (!require(kain_platform_require_kind(kind, required_mask, &diag) != 0, "require should fail for stub platform")) {
        return 0;
    }
    if (!require(diag.subsystem == KAIN_DIAG_SUBSYSTEM_PLATFORM, "diagnostic subsystem should be platform")) {
        return 0;
    }
    if (!require(diag.code == KAIN_DIAG_CODE_PLATFORM_UNSUPPORTED, "diagnostic code should indicate unsupported platform")) {
        return 0;
    }
    if (!require(strstr(diag.detail, "app_host") != NULL, "detail should mention missing app_host")) {
        return 0;
    }
    if (!require(strstr(diag.detail, "input") != NULL, "detail should mention missing input")) {
        return 0;
    }
    if (!require(strstr(diag.detail, "graphics") != NULL, "detail should mention missing graphics")) {
        return 0;
    }

    return 1;
}

int main(void) {
    if (!check_stub(KAIN_PLATFORM_KIND_LINUX, "linux")) {
        return 1;
    }

    if (!check_stub(KAIN_PLATFORM_KIND_MACOS, "macos")) {
        return 1;
    }

    printf("PASS: Platform stub diagnostics test completed successfully\n");
    return 0;
}
