#ifndef RENDERER_SESSION_H
#define RENDERER_SESSION_H

#include "graphics_bundle.h"
#include "platform.h"
#include "renderer_backend.h"

#define RENDERER_SESSION_MAX_ID 32
#define RENDERER_SESSION_MAX_NAME 64
#define RENDERER_SESSION_MAX_SCENE 96
#define RENDERER_SESSION_MAX_SUMMARY 192
#define RENDERER_SESSION_MAX_DIAGNOSTIC 256

typedef enum {
    KAIN_RENDERER_SCENE_EXECUTOR_UNKNOWN = 0,
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
    int backend_declared_available;
    int backend_probe_passed;
    int backend_session_ready;
    int graphics_bundle_loaded;
    int graphics_bundle_valid;
    int scene_execution_available;
    int used_compatibility_executor;
    char requested_backend_id[RENDERER_SESSION_MAX_ID];
    char active_backend_id[RENDERER_SESSION_MAX_ID];
    char active_service_key[RENDERER_SESSION_MAX_NAME];
    char backend_runtime_name[RENDERER_SESSION_MAX_NAME];
    char backend_version[RENDERER_SESSION_MAX_NAME];
    char scene_name[RENDERER_SESSION_MAX_SCENE];
    char summary[RENDERER_SESSION_MAX_SUMMARY];
    char diagnostic[RENDERER_SESSION_MAX_DIAGNOSTIC];
} KainRuntimeRendererSession;

void renderer_session_init(KainRuntimeRendererSession* session);
int renderer_session_boot(
    const KainRuntimeGraphicsBundle* graphics_bundle,
    const KainRuntimeGraphicsValidation* graphics_validation,
    KainPlatformKind platform_kind,
    const char* requested_backend_id,
    KainRuntimeRendererSession* session
);
void renderer_session_shutdown(KainRuntimeRendererSession* session);
const char* renderer_session_status_name(
    KainRendererSessionStatus status
);
const char* renderer_scene_executor_name(
    KainRendererSceneExecutorKind executor_kind
);
int renderer_session_should_use_compatibility_executor(
    const KainRuntimeRendererSession* session
);
void renderer_session_format_summary(
    const KainRuntimeRendererSession* session,
    char* out,
    size_t out_cap
);

#endif /* RENDERER_SESSION_H */
