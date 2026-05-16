/*
 * Platform Parity Conformance Test: Current Descriptor
 */

#include "../../native/include/platform.h"
#include <stdio.h>
#include <string.h>

static int require(int condition, const char* message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    KainPlatformDescriptor descriptor;
    unsigned int current_mask;
    char mask_text[128];

    kain_platform_describe_current(&descriptor);
    current_mask = kain_platform_current_service_mask();

    printf("Current platform kind: %s\n", kain_platform_kind_name(descriptor.kind));
    printf("Current platform name: %s\n", descriptor.name);
    printf("Current platform family: %s\n", descriptor.family);

    if (!require(descriptor.supported == 1, "current platform should be marked supported")) {
        return 1;
    }
    if (!require(descriptor.kind == kain_platform_current_kind(), "descriptor kind should match current kind")) {
        return 1;
    }
    if (!require(current_mask != 0, "current platform should advertise at least one capability")) {
        return 1;
    }
    if (!require((current_mask & KAIN_PLATFORM_SERVICE_FILESYSTEM) != 0, "filesystem capability should be present")) {
        return 1;
    }
    if (!require((current_mask & KAIN_PLATFORM_SERVICE_PROCESS) != 0, "process capability should be present")) {
        return 1;
    }
    if (!require((current_mask & KAIN_PLATFORM_SERVICE_TIMERS) != 0, "timer capability should be present")) {
        return 1;
    }

    kain_platform_format_service_mask(current_mask, mask_text, sizeof(mask_text));
    printf("Capability mask: %s\n", mask_text);

    if (!require(strlen(descriptor.diagnostic) > 0, "descriptor diagnostic should not be empty")) {
        return 1;
    }

    printf("PASS: Current platform descriptor test completed successfully\n");
    return 0;
}
