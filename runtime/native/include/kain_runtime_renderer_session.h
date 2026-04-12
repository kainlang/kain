#ifndef KAIN_RUNTIME_RENDERER_SESSION_H
#define KAIN_RUNTIME_RENDERER_SESSION_H

#include "kain_runtime_graphics.h"
#include "kain_runtime_platform.h"
#include "kain_runtime_renderer_backend.h"

#define KAIN_RUNTIME_RENDERER_SESSION_MAX_ID 32
#define KAIN_RUNTIME_RENDERER_SESSION_MAX_NAME 64
#define KAIN_RUNTIME_RENDERER_SESSION_MAX_SCENE 96
#define KAIN_RUNTIME_RENDERER_SESSION_MAX_SUMMARY 192
#define KAIN_RUNTIME_RENDERER_SESSION_MAX_DIAGNOSTIC 256

typedef enum {
    KAIN_RENDERER_SCENE_EXECUTOR_UNKNOWN = 0,
    KAIN_RENDERER_SCENE_EXECUTOR_VENDOR_DIRECT,
    KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_GL,
    KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE,
    KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY,
} KainRendererSceneExecutorKind;

typedef enum {
    KAIN_RENDERER_SESSION_STATUS_UNINITIALIZED = 0,
    KAIN_RENDERER_SESSION_STATUS_READY,
    KAIN_RENDERER_SESSION_STATUS_DEGRADED,
    KAIN_RENDERER_SESSION_STATUS_FAILED,
} KainRendererSessionStatus;

typedef struct {
    KainPlatformKind platform_kind;
    KainRendererBackendKind requested_backend_kind;
    KainRendererBackendKind active_backend_kind;
    KainRendererSceneExecutorKind executor_kind;
    KainRendererSessionStatus status;
    int vendor_declared_available;
    int vendor_probe_passed;
    int vendor_start_passed;
    int graphics_bundle_loaded;
    int graphics_bundle_valid;
    int scene_execution_available;
    int used_compatibility_executor;
    char requested_backend_id[KAIN_RUNTIME_RENDERER_SESSION_MAX_ID];
    char active_backend_id[KAIN_RUNTIME_RENDERER_SESSION_MAX_ID];
    char active_service_key[KAIN_RUNTIME_RENDERER_SESSION_MAX_NAME];
    char vendor_runtime_name[KAIN_RUNTIME_RENDERER_SESSION_MAX_NAME];
    char vendor_version[KAIN_RUNTIME_RENDERER_SESSION_MAX_NAME];
    char scene_name[KAIN_RUNTIME_RENDERER_SESSION_MAX_SCENE];
    char summary[KAIN_RUNTIME_RENDERER_SESSION_MAX_SUMMARY];
    char diagnostic[KAIN_RUNTIME_RENDERER_SESSION_MAX_DIAGNOSTIC];
} KainRuntimeRendererSession;

void kain_runtime_renderer_session_init(KainRuntimeRendererSession* session);
int kain_runtime_renderer_session_boot(
    const KainRuntimeGraphicsBundle* graphics_bundle,
    const KainRuntimeGraphicsValidation* graphics_validation,
    KainPlatformKind platform_kind,
    const char* requested_backend_id,
    KainRuntimeRendererSession* session
);
void kain_runtime_renderer_session_shutdown(KainRuntimeRendererSession* session);
const char* kain_runtime_renderer_session_status_name(
    KainRendererSessionStatus status
);
const char* kain_runtime_renderer_scene_executor_name(
    KainRendererSceneExecutorKind executor_kind
);
int kain_runtime_renderer_session_should_use_gl_compat(
    const KainRuntimeRendererSession* session
);
void kain_runtime_renderer_session_format_summary(
    const KainRuntimeRendererSession* session,
    char* out,
    size_t out_cap
);

#endif /* KAIN_RUNTIME_RENDERER_SESSION_H */
