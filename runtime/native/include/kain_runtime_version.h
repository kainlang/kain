#ifndef KAIN_RUNTIME_VERSION_H
#define KAIN_RUNTIME_VERSION_H

#include <stddef.h>

/*
 * KAIN Native Runtime ABI Versioning
 *
 * This header defines the canonical ABI version constants and runtime version
 * metadata for the KAIN native runtime. These constants are used for:
 * - Startup validation and compatibility checking
 * - Bundle/runtime version matching
 * - Hot reload compatibility validation
 * - Diagnostic reporting
 *
 * ABI Version Scheme:
 * - MAJOR: Incremented for breaking ABI changes (incompatible)
 * - MINOR: Incremented for backward-compatible additions
 * - PATCH: Incremented for bug fixes and non-ABI changes
 *
 * Runtime Version Scheme:
 * - Tracks the overall runtime implementation version
 * - May differ from ABI version (runtime can evolve without breaking ABI)
 */

/* Canonical ABI Version */
#define KAIN_RUNTIME_ABI_VERSION_MAJOR 0
#define KAIN_RUNTIME_ABI_VERSION_MINOR 1
#define KAIN_RUNTIME_ABI_VERSION_PATCH 0

/* Runtime Implementation Version */
#define KAIN_RUNTIME_VERSION_MAJOR 0
#define KAIN_RUNTIME_VERSION_MINOR 1
#define KAIN_RUNTIME_VERSION_PATCH 0

/* Build Metadata */
#define KAIN_RUNTIME_BUILD_DATE __DATE__
#define KAIN_RUNTIME_BUILD_TIME __TIME__

/* Version String Formatting */
#define KAIN_RUNTIME_VERSION_STRING_MAX 64
#define KAIN_RUNTIME_BUILD_INFO_STRING_MAX 128

/* Composite Version Macros */
#define KAIN_RUNTIME_ABI_VERSION_ENCODE(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

#define KAIN_RUNTIME_ABI_VERSION_CURRENT \
    KAIN_RUNTIME_ABI_VERSION_ENCODE( \
        KAIN_RUNTIME_ABI_VERSION_MAJOR, \
        KAIN_RUNTIME_ABI_VERSION_MINOR, \
        KAIN_RUNTIME_ABI_VERSION_PATCH \
    )

#define KAIN_RUNTIME_VERSION_ENCODE(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

#define KAIN_RUNTIME_VERSION_CURRENT \
    KAIN_RUNTIME_VERSION_ENCODE( \
        KAIN_RUNTIME_VERSION_MAJOR, \
        KAIN_RUNTIME_VERSION_MINOR, \
        KAIN_RUNTIME_VERSION_PATCH \
    )

/* Version Extraction Macros */
#define KAIN_RUNTIME_VERSION_GET_MAJOR(version) (((version) >> 16) & 0xFF)
#define KAIN_RUNTIME_VERSION_GET_MINOR(version) (((version) >> 8) & 0xFF)
#define KAIN_RUNTIME_VERSION_GET_PATCH(version) ((version) & 0xFF)

/* Compatibility Check Macros */
#define KAIN_RUNTIME_ABI_COMPATIBLE(required_major, required_minor) \
    (KAIN_RUNTIME_ABI_VERSION_MAJOR == (required_major) && \
     KAIN_RUNTIME_ABI_VERSION_MINOR >= (required_minor))

#define KAIN_RUNTIME_ABI_EXACT_MATCH(major, minor, patch) \
    (KAIN_RUNTIME_ABI_VERSION_MAJOR == (major) && \
     KAIN_RUNTIME_ABI_VERSION_MINOR == (minor) && \
     KAIN_RUNTIME_ABI_VERSION_PATCH == (patch))

/*
 * Runtime Version Information Structure
 *
 * This structure provides programmatic access to runtime version and build
 * information. It is populated by kain_runtime_version_get_info() and used
 * for startup validation, diagnostics, and compatibility reporting.
 */
typedef struct {
    /* ABI Version */
    unsigned int abi_version_major;
    unsigned int abi_version_minor;
    unsigned int abi_version_patch;
    unsigned int abi_version_encoded;

    /* Runtime Version */
    unsigned int runtime_version_major;
    unsigned int runtime_version_minor;
    unsigned int runtime_version_patch;
    unsigned int runtime_version_encoded;

    /* Build Information */
    char build_date[32];
    char build_time[32];

    /* Formatted Strings */
    char abi_version_string[KAIN_RUNTIME_VERSION_STRING_MAX];
    char runtime_version_string[KAIN_RUNTIME_VERSION_STRING_MAX];
    char build_info_string[KAIN_RUNTIME_BUILD_INFO_STRING_MAX];
} KainRuntimeVersionInfo;

/*
 * Get Runtime Version Information
 *
 * Populates the provided KainRuntimeVersionInfo structure with current
 * runtime version, ABI version, and build metadata.
 *
 * Parameters:
 *   info - Pointer to KainRuntimeVersionInfo structure to populate
 *
 * Returns:
 *   0 on success, non-zero on error
 */
int kain_runtime_version_get_info(KainRuntimeVersionInfo* info);

/*
 * Format ABI Version String
 *
 * Formats the ABI version as "major.minor.patch" into the provided buffer.
 *
 * Parameters:
 *   abi_version_encoded - Encoded ABI version (from KAIN_RUNTIME_ABI_VERSION_ENCODE)
 *   out - Output buffer
 *   out_size - Size of output buffer
 *
 * Returns:
 *   Number of characters written (excluding null terminator), or -1 on error
 */
int kain_runtime_version_format_abi(
    unsigned int abi_version_encoded,
    char* out,
    size_t out_size
);

/*
 * Format Runtime Version String
 *
 * Formats the runtime version as "major.minor.patch" into the provided buffer.
 *
 * Parameters:
 *   runtime_version_encoded - Encoded runtime version
 *   out - Output buffer
 *   out_size - Size of output buffer
 *
 * Returns:
 *   Number of characters written (excluding null terminator), or -1 on error
 */
int kain_runtime_version_format_runtime(
    unsigned int runtime_version_encoded,
    char* out,
    size_t out_size
);

/*
 * Check ABI Compatibility
 *
 * Checks if the current runtime ABI is compatible with the required ABI version.
 * Compatible means: same major version, current minor >= required minor.
 *
 * Parameters:
 *   required_abi_version_encoded - Required ABI version (encoded)
 *
 * Returns:
 *   1 if compatible, 0 if incompatible
 */
int kain_runtime_version_check_abi_compatibility(
    unsigned int required_abi_version_encoded
);

/*
 * Print Runtime Version Information
 *
 * Prints runtime version, ABI version, and build information to stdout.
 * Useful for diagnostics and startup logging.
 */
void kain_runtime_version_print_info(void);

#endif /* KAIN_RUNTIME_VERSION_H */
