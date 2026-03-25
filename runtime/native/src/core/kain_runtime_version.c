#include "../../include/kain_runtime_version.h"
#include <stdio.h>
#include <string.h>

int kain_runtime_version_get_info(KainRuntimeVersionInfo* info) {
    if (!info) {
        return -1;
    }

    /* Clear the structure */
    memset(info, 0, sizeof(KainRuntimeVersionInfo));

    /* Populate ABI version */
    info->abi_version_major = KAIN_RUNTIME_ABI_VERSION_MAJOR;
    info->abi_version_minor = KAIN_RUNTIME_ABI_VERSION_MINOR;
    info->abi_version_patch = KAIN_RUNTIME_ABI_VERSION_PATCH;
    info->abi_version_encoded = KAIN_RUNTIME_ABI_VERSION_CURRENT;

    /* Populate runtime version */
    info->runtime_version_major = KAIN_RUNTIME_VERSION_MAJOR;
    info->runtime_version_minor = KAIN_RUNTIME_VERSION_MINOR;
    info->runtime_version_patch = KAIN_RUNTIME_VERSION_PATCH;
    info->runtime_version_encoded = KAIN_RUNTIME_VERSION_CURRENT;

    /* Populate build information */
    snprintf(info->build_date, sizeof(info->build_date), "%s", KAIN_RUNTIME_BUILD_DATE);
    snprintf(info->build_time, sizeof(info->build_time), "%s", KAIN_RUNTIME_BUILD_TIME);

    /* Format ABI version string */
    snprintf(
        info->abi_version_string,
        sizeof(info->abi_version_string),
        "%u.%u.%u",
        info->abi_version_major,
        info->abi_version_minor,
        info->abi_version_patch
    );

    /* Format runtime version string */
    snprintf(
        info->runtime_version_string,
        sizeof(info->runtime_version_string),
        "%u.%u.%u",
        info->runtime_version_major,
        info->runtime_version_minor,
        info->runtime_version_patch
    );

    /* Format build info string */
    snprintf(
        info->build_info_string,
        sizeof(info->build_info_string),
        "Built %s %s",
        info->build_date,
        info->build_time
    );

    return 0;
}

int kain_runtime_version_format_abi(
    unsigned int abi_version_encoded,
    char* out,
    size_t out_size
) {
    unsigned int major, minor, patch;

    if (!out || out_size == 0) {
        return -1;
    }

    major = KAIN_RUNTIME_VERSION_GET_MAJOR(abi_version_encoded);
    minor = KAIN_RUNTIME_VERSION_GET_MINOR(abi_version_encoded);
    patch = KAIN_RUNTIME_VERSION_GET_PATCH(abi_version_encoded);

    return snprintf(out, out_size, "%u.%u.%u", major, minor, patch);
}

int kain_runtime_version_format_runtime(
    unsigned int runtime_version_encoded,
    char* out,
    size_t out_size
) {
    unsigned int major, minor, patch;

    if (!out || out_size == 0) {
        return -1;
    }

    major = KAIN_RUNTIME_VERSION_GET_MAJOR(runtime_version_encoded);
    minor = KAIN_RUNTIME_VERSION_GET_MINOR(runtime_version_encoded);
    patch = KAIN_RUNTIME_VERSION_GET_PATCH(runtime_version_encoded);

    return snprintf(out, out_size, "%u.%u.%u", major, minor, patch);
}

int kain_runtime_version_check_abi_compatibility(
    unsigned int required_abi_version_encoded
) {
    unsigned int required_major, required_minor;
    unsigned int current_major, current_minor;

    required_major = KAIN_RUNTIME_VERSION_GET_MAJOR(required_abi_version_encoded);
    required_minor = KAIN_RUNTIME_VERSION_GET_MINOR(required_abi_version_encoded);

    current_major = KAIN_RUNTIME_ABI_VERSION_MAJOR;
    current_minor = KAIN_RUNTIME_ABI_VERSION_MINOR;

    /* Compatible if: same major version, current minor >= required minor */
    if (current_major != required_major) {
        return 0;
    }

    if (current_minor < required_minor) {
        return 0;
    }

    return 1;
}

void kain_runtime_version_print_info(void) {
    KainRuntimeVersionInfo info;

    if (kain_runtime_version_get_info(&info) != 0) {
        printf("Error: Failed to retrieve runtime version information\n");
        return;
    }

    printf("KAIN Native Runtime Version Information:\n");
    printf("  Runtime Version: %s\n", info.runtime_version_string);
    printf("  ABI Version:     %s\n", info.abi_version_string);
    printf("  Build Info:      %s\n", info.build_info_string);
}
