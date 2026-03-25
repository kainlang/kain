#ifndef KAIN_RUNTIME_COMPATIBILITY_H
#define KAIN_RUNTIME_COMPATIBILITY_H

#include "kain_runtime_version.h"
#include "kain_runtime_diagnostics.h"
#include <stddef.h>

/*
 * KAIN Native Runtime Compatibility and Hot Reload ABI
 *
 * This header defines the canonical compatibility, versioning, and hot reload
 * ABI for the KAIN native runtime. It provides declarations for bundle
 * compatibility checking, version validation, migration hooks, and lifecycle
 * management for runtime updates.
 *
 * Compatibility Features:
 * - Bundle compatibility class validation
 * - Runtime/bundle ABI version matching
 * - Migration metadata and hooks
 * - Install/update/uninstall lifecycle APIs
 * - State transfer boundaries for hot reload
 * - Compatibility diagnostics and rejection rules
 */

/* Compatibility Class */
typedef enum {
    KAIN_COMPAT_CLASS_UNKNOWN = 0,
    KAIN_COMPAT_CLASS_STABLE,       /* Stable API, backward compatible */
    KAIN_COMPAT_CLASS_EXPERIMENTAL, /* Experimental, may break */
    KAIN_COMPAT_CLASS_DEPRECATED,   /* Deprecated, will be removed */
    KAIN_COMPAT_CLASS_INTERNAL,     /* Internal, no compatibility guarantees */
} KainCompatibilityClass;

/* Bundle Lifecycle State */
typedef enum {
    KAIN_BUNDLE_STATE_UNINSTALLED = 0,
    KAIN_BUNDLE_STATE_INSTALLED,
    KAIN_BUNDLE_STATE_ACTIVE,
    KAIN_BUNDLE_STATE_SUSPENDED,
    KAIN_BUNDLE_STATE_FAILED,
} KainBundleState;

/* Migration Requirement */
typedef enum {
    KAIN_MIGRATION_NONE = 0,        /* No migration needed */
    KAIN_MIGRATION_AUTOMATIC,       /* Automatic migration available */
    KAIN_MIGRATION_MANUAL,          /* Manual migration required */
    KAIN_MIGRATION_INCOMPATIBLE,    /* Incompatible, cannot migrate */
} KainMigrationRequirement;

/* String Buffer Sizes */
#define KAIN_COMPAT_BUNDLE_ID_MAX       128
#define KAIN_COMPAT_VERSION_STRING_MAX  64
#define KAIN_COMPAT_REASON_MAX          256

/*
 * Bundle Compatibility Metadata
 *
 * Metadata describing a bundle's compatibility requirements and capabilities.
 */
typedef struct {
    char bundle_id[KAIN_COMPAT_BUNDLE_ID_MAX];
    unsigned int bundle_version_major;
    unsigned int bundle_version_minor;
    unsigned int bundle_version_patch;
    unsigned int required_abi_version;
    unsigned int required_runtime_version;
    KainCompatibilityClass compat_class;
    int requires_migration;
    KainMigrationRequirement migration_requirement;
    unsigned int service_requirements_mask;
} KainBundleCompatibilityMetadata;

/*
 * Compatibility Validation Result
 *
 * Result of compatibility validation between a bundle and the runtime.
 */
typedef struct {
    int compatible;
    int abi_compatible;
    int runtime_compatible;
    int services_compatible;
    int migration_available;
    KainMigrationRequirement migration_requirement;
    char incompatibility_reason[KAIN_COMPAT_REASON_MAX];
    unsigned int runtime_abi_version;
    unsigned int bundle_abi_version;
    unsigned int missing_services_mask;
} KainCompatibilityValidationResult;

/*
 * Bundle Handle
 *
 * Opaque handle to an installed bundle. Used for lifecycle operations.
 */
typedef struct KainBundleHandle KainBundleHandle;

/*
 * Migration Context
 *
 * Context passed to migration hooks during bundle updates.
 */
typedef struct {
    const KainBundleCompatibilityMetadata* old_metadata;
    const KainBundleCompatibilityMetadata* new_metadata;
    void* old_state;
    void* new_state;
    void* runtime_data;
} KainMigrationContext;

/*
 * Migration Hook Function
 *
 * Called during bundle updates to migrate state from old to new version.
 * Returns 0 on success, non-zero on error.
 */
typedef int (*KainMigrationHookFn)(
    KainMigrationContext* context,
    KainDiagnostic* diag
);

/*
 * Initialize Bundle Compatibility Metadata
 *
 * Sets default values for bundle compatibility metadata.
 */
void kain_bundle_compat_metadata_init(KainBundleCompatibilityMetadata* metadata);

/*
 * Validate Bundle Compatibility
 *
 * Checks if a bundle is compatible with the current runtime. Returns 0 if
 * compatible, non-zero if incompatible. Populates validation result.
 */
int kain_bundle_validate_compatibility(
    const KainBundleCompatibilityMetadata* metadata,
    KainCompatibilityValidationResult* result
);

/*
 * Check ABI Compatibility
 *
 * Checks if a bundle's required ABI version is compatible with the runtime.
 * Returns 1 if compatible, 0 if incompatible.
 */
int kain_bundle_check_abi_compatibility(
    unsigned int required_abi_version
);

/*
 * Check Runtime Version Compatibility
 *
 * Checks if a bundle's required runtime version is compatible.
 * Returns 1 if compatible, 0 if incompatible.
 */
int kain_bundle_check_runtime_compatibility(
    unsigned int required_runtime_version
);

/*
 * Install Bundle
 *
 * Installs a bundle into the runtime. Returns a bundle handle on success,
 * NULL on failure. Populates diagnostic on error.
 */
KainBundleHandle* kain_bundle_install(
    const char* bundle_path,
    const KainBundleCompatibilityMetadata* metadata,
    KainDiagnostic* diag
);

/*
 * Activate Bundle
 *
 * Activates an installed bundle, making it available for use. Returns 0 on
 * success, non-zero on error.
 */
int kain_bundle_activate(
    KainBundleHandle* handle,
    KainDiagnostic* diag
);

/*
 * Deactivate Bundle
 *
 * Deactivates an active bundle, suspending its operation. Returns 0 on
 * success, non-zero on error.
 */
int kain_bundle_deactivate(
    KainBundleHandle* handle,
    KainDiagnostic* diag
);

/*
 * Update Bundle
 *
 * Updates an installed bundle to a new version. Validates compatibility and
 * runs migration hooks if needed. Returns 0 on success, non-zero on error.
 */
int kain_bundle_update(
    KainBundleHandle* handle,
    const char* new_bundle_path,
    const KainBundleCompatibilityMetadata* new_metadata,
    KainMigrationHookFn migration_hook,
    KainDiagnostic* diag
);

/*
 * Uninstall Bundle
 *
 * Uninstalls a bundle from the runtime, cleaning up all resources.
 * Returns 0 on success, non-zero on error.
 */
int kain_bundle_uninstall(
    KainBundleHandle* handle,
    KainDiagnostic* diag
);

/*
 * Get Bundle State
 *
 * Returns the current state of a bundle.
 */
KainBundleState kain_bundle_get_state(const KainBundleHandle* handle);

/*
 * Get Bundle Metadata
 *
 * Retrieves the compatibility metadata for an installed bundle.
 * Returns NULL if handle is invalid.
 */
const KainBundleCompatibilityMetadata* kain_bundle_get_metadata(
    const KainBundleHandle* handle
);

/*
 * Snapshot Bundle State
 *
 * Creates a snapshot of a bundle's runtime state for migration or hot reload.
 * Returns a state handle on success, NULL on failure.
 */
void* kain_bundle_snapshot_state(
    KainBundleHandle* handle,
    KainDiagnostic* diag
);

/*
 * Restore Bundle State
 *
 * Restores a bundle's runtime state from a snapshot. Returns 0 on success,
 * non-zero on error.
 */
int kain_bundle_restore_state(
    KainBundleHandle* handle,
    void* state_snapshot,
    KainDiagnostic* diag
);

/*
 * Free State Snapshot
 *
 * Releases resources associated with a state snapshot.
 */
void kain_bundle_free_state_snapshot(void* state_snapshot);

/*
 * Format Compatibility Validation Result
 *
 * Formats a compatibility validation result as a human-readable string.
 * Returns number of characters written (excluding null terminator).
 */
int kain_compat_format_validation_result(
    const KainCompatibilityValidationResult* result,
    char* out,
    size_t out_size
);

/*
 * Print Compatibility Validation Result
 *
 * Prints a compatibility validation result to stdout for diagnostics.
 */
void kain_compat_print_validation_result(
    const KainCompatibilityValidationResult* result
);

#endif /* KAIN_RUNTIME_COMPATIBILITY_H */
