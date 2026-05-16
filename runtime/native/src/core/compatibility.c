#include "../../include/compatibility.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define KAIN_BUNDLE_PATH_MAX 512
#define KAIN_COMPAT_SNAPSHOT_MAGIC 0x4B41494Eu

typedef struct {
    unsigned int magic;
    KainBundleCompatibilityMetadata metadata;
    KainBundleState state;
    unsigned int generation;
} KainBundleStateSnapshot;

struct KainBundleHandle {
    char bundle_path[KAIN_BUNDLE_PATH_MAX];
    KainBundleCompatibilityMetadata metadata;
    KainBundleState state;
    unsigned int generation;
};

static void kain_copy_text(char* out, size_t out_cap, const char* text) {
    size_t length;

    if (!out || out_cap == 0) {
        return;
    }
    if (!text) {
        out[0] = '\0';
        return;
    }

    length = strlen(text);
    if (length >= out_cap) {
        length = out_cap - 1;
    }

    memcpy(out, text, length);
    out[length] = '\0';
}

static int version_is_compatible(unsigned int required_runtime_version) {
    unsigned int current_major = VERSION_GET_MAJOR(VERSION_CURRENT);
    unsigned int current_minor = VERSION_GET_MINOR(VERSION_CURRENT);
    unsigned int current_patch = VERSION_GET_PATCH(VERSION_CURRENT);
    unsigned int required_major = VERSION_GET_MAJOR(required_runtime_version);
    unsigned int required_minor = VERSION_GET_MINOR(required_runtime_version);
    unsigned int required_patch = VERSION_GET_PATCH(required_runtime_version);

    if (required_runtime_version == 0u) {
        return 1;
    }
    if (current_major != required_major) {
        return 0;
    }
    if (current_minor > required_minor) {
        return 1;
    }
    if (current_minor < required_minor) {
        return 0;
    }
    return current_patch >= required_patch;
}

static void kain_compat_set_diag(
    KainDiagnostic* diag,
    int code,
    const char* message,
    const char* detail
) {
    if (!diag) {
        return;
    }
    kain_diagnostic_create(
        diag,
        KAIN_DIAG_SUBSYSTEM_COMPATIBILITY,
        KAIN_DIAG_SEVERITY_ERROR,
        code,
        message,
        detail,
        NULL
    );
}

void kain_bundle_compat_metadata_init(KainBundleCompatibilityMetadata* metadata) {
    if (!metadata) {
        return;
    }
    memset(metadata, 0, sizeof(*metadata));
    metadata->required_abi_version = RUNTIME_ABI_VERSION_CURRENT;
    metadata->required_runtime_version = VERSION_CURRENT;
    metadata->compat_class = KAIN_COMPAT_CLASS_EXPERIMENTAL;
    metadata->migration_requirement = KAIN_MIGRATION_NONE;
}

int kain_bundle_check_abi_compatibility(unsigned int required_abi_version) {
    if (required_abi_version == 0u) {
        return 1;
    }
    return version_check_abi_compatibility(required_abi_version);
}

int kain_bundle_check_runtime_compatibility(unsigned int required_runtime_version) {
    return version_is_compatible(required_runtime_version);
}

int kain_bundle_validate_compatibility(
    const KainBundleCompatibilityMetadata* metadata,
    KainCompatibilityValidationResult* result
) {
    if (!metadata || !result) {
        return -1;
    }

    memset(result, 0, sizeof(*result));
    result->runtime_abi_version = RUNTIME_ABI_VERSION_CURRENT;
    result->bundle_abi_version = metadata->required_abi_version;
    result->migration_requirement = metadata->migration_requirement;

    result->abi_compatible = kain_bundle_check_abi_compatibility(metadata->required_abi_version);
    result->runtime_compatible =
        kain_bundle_check_runtime_compatibility(metadata->required_runtime_version);
    result->services_compatible = 1;
    result->migration_available =
        metadata->migration_requirement == KAIN_MIGRATION_NONE ||
        metadata->migration_requirement == KAIN_MIGRATION_AUTOMATIC;

    if (metadata->compat_class == KAIN_COMPAT_CLASS_INTERNAL) {
        result->abi_compatible =
            metadata->required_abi_version == 0u ||
            metadata->required_abi_version == RUNTIME_ABI_VERSION_CURRENT;
        result->runtime_compatible =
            metadata->required_runtime_version == 0u ||
            metadata->required_runtime_version == VERSION_CURRENT;
    }

    result->compatible =
        result->abi_compatible &&
        result->runtime_compatible &&
        result->services_compatible &&
        metadata->migration_requirement != KAIN_MIGRATION_INCOMPATIBLE;

    if (!result->abi_compatible) {
        kain_copy_text(
            result->incompatibility_reason,
            sizeof(result->incompatibility_reason),
            "Bundle ABI version is not compatible with the current runtime."
        );
        return 1;
    }
    if (!result->runtime_compatible) {
        kain_copy_text(
            result->incompatibility_reason,
            sizeof(result->incompatibility_reason),
            "Bundle runtime version requirement is newer than the current runtime."
        );
        return 1;
    }
    if (metadata->migration_requirement == KAIN_MIGRATION_INCOMPATIBLE) {
        kain_copy_text(
            result->incompatibility_reason,
            sizeof(result->incompatibility_reason),
            "Bundle update is explicitly marked as incompatible."
        );
        return 1;
    }

    kain_copy_text(
        result->incompatibility_reason,
        sizeof(result->incompatibility_reason),
        "Bundle is compatible with the current runtime."
    );
    return 0;
}

KainBundleHandle* kain_bundle_install(
    const char* bundle_path,
    const KainBundleCompatibilityMetadata* metadata,
    KainDiagnostic* diag
) {
    KainCompatibilityValidationResult validation;
    KainBundleHandle* handle;

    if (!bundle_path || !metadata) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE,
            "Bundle installation failed",
            "Bundle path and metadata are required."
        );
        return NULL;
    }

    if (kain_bundle_validate_compatibility(metadata, &validation) != 0) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_VERSION_MISMATCH,
            "Bundle installation rejected by compatibility rules",
            validation.incompatibility_reason
        );
        return NULL;
    }

    handle = (KainBundleHandle*)calloc(1, sizeof(*handle));
    if (!handle) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Bundle installation failed",
            "Unable to allocate bundle handle."
        );
        return NULL;
    }

    kain_copy_text(handle->bundle_path, sizeof(handle->bundle_path), bundle_path);
    handle->metadata = *metadata;
    handle->state = KAIN_BUNDLE_STATE_INSTALLED;
    handle->generation = 1u;
    return handle;
}

int kain_bundle_activate(KainBundleHandle* handle, KainDiagnostic* diag) {
    if (!handle) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE,
            "Bundle activation failed",
            "Bundle handle is null."
        );
        return -1;
    }
    if (handle->state != KAIN_BUNDLE_STATE_INSTALLED &&
        handle->state != KAIN_BUNDLE_STATE_SUSPENDED) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE,
            "Bundle activation failed",
            "Bundle must be installed or suspended before activation."
        );
        return -1;
    }
    handle->state = KAIN_BUNDLE_STATE_ACTIVE;
    return 0;
}

int kain_bundle_deactivate(KainBundleHandle* handle, KainDiagnostic* diag) {
    if (!handle) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE,
            "Bundle deactivation failed",
            "Bundle handle is null."
        );
        return -1;
    }
    if (handle->state != KAIN_BUNDLE_STATE_ACTIVE) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE,
            "Bundle deactivation failed",
            "Bundle is not active."
        );
        return -1;
    }
    handle->state = KAIN_BUNDLE_STATE_SUSPENDED;
    return 0;
}

int kain_bundle_update(
    KainBundleHandle* handle,
    const char* new_bundle_path,
    const KainBundleCompatibilityMetadata* new_metadata,
    KainMigrationHookFn migration_hook,
    KainDiagnostic* diag
) {
    KainCompatibilityValidationResult validation;
    KainMigrationContext context;
    KainBundleCompatibilityMetadata previous_metadata;
    KainBundleState previous_state;

    if (!handle || !new_bundle_path || !new_metadata) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE,
            "Bundle update failed",
            "Bundle handle, path, and metadata are required."
        );
        return -1;
    }

    if (kain_bundle_validate_compatibility(new_metadata, &validation) != 0) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE,
            "Bundle update rejected",
            validation.incompatibility_reason
        );
        return -1;
    }

    if (new_metadata->migration_requirement == KAIN_MIGRATION_MANUAL && !migration_hook) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_MIGRATION_FAILED,
            "Bundle update requires a migration hook",
            "Manual migration was requested but no migration hook was provided."
        );
        return -1;
    }

    previous_metadata = handle->metadata;
    previous_state = handle->state;
    if (migration_hook) {
        memset(&context, 0, sizeof(context));
        context.old_metadata = &previous_metadata;
        context.new_metadata = new_metadata;
        context.runtime_data = handle;
        if (migration_hook(&context, diag) != 0) {
            if (diag && diag->code == KAIN_DIAG_CODE_SUCCESS) {
                kain_compat_set_diag(
                    diag,
                    KAIN_DIAG_CODE_COMPAT_MIGRATION_FAILED,
                    "Migration hook failed during bundle update",
                    "Migration hook returned a non-zero status."
                );
            }
            return -1;
        }
    }

    kain_copy_text(handle->bundle_path, sizeof(handle->bundle_path), new_bundle_path);
    handle->metadata = *new_metadata;
    handle->state = previous_state == KAIN_BUNDLE_STATE_UNINSTALLED
        ? KAIN_BUNDLE_STATE_INSTALLED
        : previous_state;
    handle->generation += 1u;
    return 0;
}

int kain_bundle_uninstall(KainBundleHandle* handle, KainDiagnostic* diag) {
    if (!handle) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_INCOMPATIBLE_UPDATE,
            "Bundle uninstall failed",
            "Bundle handle is null."
        );
        return -1;
    }
    handle->state = KAIN_BUNDLE_STATE_UNINSTALLED;
    free(handle);
    return 0;
}

KainBundleState kain_bundle_get_state(const KainBundleHandle* handle) {
    if (!handle) {
        return KAIN_BUNDLE_STATE_FAILED;
    }
    return handle->state;
}

const KainBundleCompatibilityMetadata* kain_bundle_get_metadata(
    const KainBundleHandle* handle
) {
    if (!handle) {
        return NULL;
    }
    return &handle->metadata;
}

void* kain_bundle_snapshot_state(KainBundleHandle* handle, KainDiagnostic* diag) {
    KainBundleStateSnapshot* snapshot;

    if (!handle) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_MIGRATION_FAILED,
            "Bundle snapshot failed",
            "Bundle handle is null."
        );
        return NULL;
    }

    snapshot = (KainBundleStateSnapshot*)calloc(1, sizeof(*snapshot));
    if (!snapshot) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Bundle snapshot failed",
            "Unable to allocate bundle snapshot."
        );
        return NULL;
    }

    snapshot->magic = KAIN_COMPAT_SNAPSHOT_MAGIC;
    snapshot->metadata = handle->metadata;
    snapshot->state = handle->state;
    snapshot->generation = handle->generation;
    return snapshot;
}

int kain_bundle_restore_state(
    KainBundleHandle* handle,
    void* state_snapshot,
    KainDiagnostic* diag
) {
    KainBundleStateSnapshot* snapshot = (KainBundleStateSnapshot*)state_snapshot;

    if (!handle || !snapshot || snapshot->magic != KAIN_COMPAT_SNAPSHOT_MAGIC) {
        kain_compat_set_diag(
            diag,
            KAIN_DIAG_CODE_COMPAT_MIGRATION_FAILED,
            "Bundle restore failed",
            "State snapshot is missing or invalid."
        );
        return -1;
    }

    handle->metadata = snapshot->metadata;
    handle->state = snapshot->state;
    handle->generation = snapshot->generation;
    return 0;
}

void kain_bundle_free_state_snapshot(void* state_snapshot) {
    free(state_snapshot);
}

int kain_compat_format_validation_result(
    const KainCompatibilityValidationResult* result,
    char* out,
    size_t out_size
) {
    if (!result || !out || out_size == 0) {
        return -1;
    }

    return snprintf(
        out,
        out_size,
        "compatible=%d abi=%d runtime=%d services=%d migration=%d reason=%s",
        result->compatible,
        result->abi_compatible,
        result->runtime_compatible,
        result->services_compatible,
        result->migration_requirement,
        result->incompatibility_reason
    );
}

void kain_compat_print_validation_result(
    const KainCompatibilityValidationResult* result
) {
    char buffer[512];
    if (kain_compat_format_validation_result(result, buffer, sizeof(buffer)) >= 0) {
        printf("%s\n", buffer);
    }
}
