#include "../../native/include/compatibility.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void copy_text(char* out, size_t out_size, const char* text) {
    size_t length;

    if (!out || out_size == 0) {
        return;
    }
    if (!text) {
        out[0] = '\0';
        return;
    }

    length = strlen(text);
    if (length >= out_size) {
        length = out_size - 1;
    }

    memcpy(out, text, length);
    out[length] = '\0';
}

static int g_migration_calls = 0;

static int successful_migration(KainMigrationContext* context, KainDiagnostic* diag) {
    (void)diag;
    if (!context || !context->old_metadata || !context->new_metadata) {
        return -1;
    }
    g_migration_calls += 1;
    return 0;
}

static int test_install_activate_snapshot_restore(void) {
    KainBundleCompatibilityMetadata metadata;
    KainBundleHandle* handle;
    KainDiagnostic diag;
    void* snapshot;

    kain_diagnostic_init(&diag);
    kain_bundle_compat_metadata_init(&metadata);
    copy_text(metadata.bundle_id, sizeof(metadata.bundle_id), "runtime.hot_reload.lifecycle");

    handle = kain_bundle_install("bundle_v1.knb", &metadata, &diag);
    if (!handle) {
        fprintf(stderr, "install failed: %s\n", diag.message);
        return 0;
    }
    if (kain_bundle_activate(handle, &diag) != 0) {
        fprintf(stderr, "activate failed: %s\n", diag.message);
        return 0;
    }
    if (kain_bundle_get_state(handle) != KAIN_BUNDLE_STATE_ACTIVE) {
        fprintf(stderr, "bundle should be active after activation\n");
        return 0;
    }

    snapshot = kain_bundle_snapshot_state(handle, &diag);
    if (!snapshot) {
        fprintf(stderr, "snapshot failed: %s\n", diag.message);
        return 0;
    }
    if (kain_bundle_deactivate(handle, &diag) != 0) {
        fprintf(stderr, "deactivate failed: %s\n", diag.message);
        return 0;
    }
    if (kain_bundle_restore_state(handle, snapshot, &diag) != 0) {
        fprintf(stderr, "restore failed: %s\n", diag.message);
        return 0;
    }
    if (kain_bundle_get_state(handle) != KAIN_BUNDLE_STATE_ACTIVE) {
        fprintf(stderr, "bundle restore should return bundle to active state\n");
        return 0;
    }

    kain_bundle_free_state_snapshot(snapshot);
    if (kain_bundle_uninstall(handle, &diag) != 0) {
        fprintf(stderr, "uninstall failed: %s\n", diag.message);
        return 0;
    }
    return 1;
}

static int test_update_requires_migration_hook(void) {
    KainBundleCompatibilityMetadata metadata;
    KainBundleCompatibilityMetadata update;
    KainBundleHandle* handle;
    KainDiagnostic diag;

    kain_diagnostic_init(&diag);
    kain_bundle_compat_metadata_init(&metadata);
    copy_text(metadata.bundle_id, sizeof(metadata.bundle_id), "runtime.hot_reload.migration");

    handle = kain_bundle_install("bundle_v1.knb", &metadata, &diag);
    if (!handle) {
        fprintf(stderr, "install failed: %s\n", diag.message);
        return 0;
    }

    kain_bundle_compat_metadata_init(&update);
    copy_text(update.bundle_id, sizeof(update.bundle_id), "runtime.hot_reload.migration");
    update.bundle_version_minor = 1;
    update.migration_requirement = KAIN_MIGRATION_MANUAL;

    if (kain_bundle_update(handle, "bundle_v2.knb", &update, NULL, &diag) == 0) {
        fprintf(stderr, "expected update without migration hook to fail\n");
        return 0;
    }
    if (diag.code != KAIN_DIAG_CODE_COMPAT_MIGRATION_FAILED) {
        fprintf(stderr, "expected migration failure diagnostic code\n");
        return 0;
    }

    g_migration_calls = 0;
    if (kain_bundle_update(handle, "bundle_v2.knb", &update, successful_migration, &diag) != 0) {
        fprintf(stderr, "expected update with migration hook to succeed: %s\n", diag.message);
        return 0;
    }
    if (g_migration_calls != 1) {
        fprintf(stderr, "migration hook should be called exactly once\n");
        return 0;
    }

    if (kain_bundle_uninstall(handle, &diag) != 0) {
        fprintf(stderr, "uninstall failed: %s\n", diag.message);
        return 0;
    }
    return 1;
}

int main(void) {
    if (!test_install_activate_snapshot_restore()) {
        return 1;
    }
    if (!test_update_requires_migration_hook()) {
        return 1;
    }

    printf("PASS: lifecycle compatibility tests completed successfully\n");
    return 0;
}
