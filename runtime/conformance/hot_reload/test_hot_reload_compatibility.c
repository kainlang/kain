#include "../../native/include/kain_runtime_compatibility.h"

#include <stdio.h>
#include <string.h>

static int test_compatible_bundle(void) {
    KainBundleCompatibilityMetadata metadata;
    KainCompatibilityValidationResult result;

    kain_bundle_compat_metadata_init(&metadata);
    strcpy(metadata.bundle_id, "runtime.hot_reload.ok");
    metadata.bundle_version_major = 1;
    metadata.bundle_version_minor = 0;
    metadata.bundle_version_patch = 0;

    if (kain_bundle_validate_compatibility(&metadata, &result) != 0) {
        fprintf(stderr, "expected compatible bundle to validate successfully\n");
        return 0;
    }
    if (!result.compatible || !result.abi_compatible || !result.runtime_compatible) {
        fprintf(stderr, "compatible bundle returned invalid flags\n");
        return 0;
    }
    return 1;
}

static int test_abi_mismatch_rejected(void) {
    KainBundleCompatibilityMetadata metadata;
    KainCompatibilityValidationResult result;

    kain_bundle_compat_metadata_init(&metadata);
    strcpy(metadata.bundle_id, "runtime.hot_reload.abi_mismatch");
    metadata.required_abi_version = KAIN_RUNTIME_ABI_VERSION_ENCODE(1, 0, 0);

    if (kain_bundle_validate_compatibility(&metadata, &result) == 0) {
        fprintf(stderr, "expected ABI mismatch to fail validation\n");
        return 0;
    }
    if (result.abi_compatible) {
        fprintf(stderr, "ABI mismatch should not be marked compatible\n");
        return 0;
    }
    return 1;
}

static int test_runtime_mismatch_rejected(void) {
    KainBundleCompatibilityMetadata metadata;
    KainCompatibilityValidationResult result;

    kain_bundle_compat_metadata_init(&metadata);
    strcpy(metadata.bundle_id, "runtime.hot_reload.runtime_mismatch");
    metadata.required_runtime_version = KAIN_RUNTIME_VERSION_ENCODE(0, 2, 0);

    if (kain_bundle_validate_compatibility(&metadata, &result) == 0) {
        fprintf(stderr, "expected runtime mismatch to fail validation\n");
        return 0;
    }
    if (result.runtime_compatible) {
        fprintf(stderr, "runtime mismatch should not be marked compatible\n");
        return 0;
    }
    return 1;
}

static int test_validation_formatting(void) {
    KainBundleCompatibilityMetadata metadata;
    KainCompatibilityValidationResult result;
    char buffer[512];

    kain_bundle_compat_metadata_init(&metadata);
    strcpy(metadata.bundle_id, "runtime.hot_reload.format");
    if (kain_bundle_validate_compatibility(&metadata, &result) != 0) {
        fprintf(stderr, "formatting precondition failed\n");
        return 0;
    }
    if (kain_compat_format_validation_result(&result, buffer, sizeof(buffer)) <= 0) {
        fprintf(stderr, "expected formatted validation output\n");
        return 0;
    }
    if (strstr(buffer, "compatible=1") == NULL) {
        fprintf(stderr, "formatted validation output missing compatibility state\n");
        return 0;
    }
    return 1;
}

int main(void) {
    if (!test_compatible_bundle()) {
        return 1;
    }
    if (!test_abi_mismatch_rejected()) {
        return 1;
    }
    if (!test_runtime_mismatch_rejected()) {
        return 1;
    }
    if (!test_validation_formatting()) {
        return 1;
    }

    printf("PASS: compatibility validation tests completed successfully\n");
    return 0;
}
