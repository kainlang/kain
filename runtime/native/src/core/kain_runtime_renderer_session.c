#include "../../include/kain_runtime_renderer_session.h"

#include <stdio.h>
#include <string.h>

static void kain_renderer_session_copy_text(
    char* out,
    size_t out_cap,
    const char* text
) {
    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!text) {
        return;
    }
    snprintf(out, out_cap, "%s", text);
}

static const KainRendererBackendDescriptor*
kain_renderer_session_resolve_requested_backend(
    const char* requested_backend_id
) {
    const KainRendererBackendDescriptor* requested_descriptor = NULL;

    if (requested_backend_id && requested_backend_id[0]) {
        requested_descriptor = kain_renderer_backend_lookup(requested_backend_id);
    }
    if (!requested_descriptor) {
        requested_descriptor = kain_renderer_backend_active();
    }
    if (!requested_descriptor) {
        requested_descriptor = kain_renderer_backend_default();
    }

    return requested_descriptor;
}

static const KainRendererBackendDescriptor*
kain_renderer_session_resolve_active_backend(
    const KainRendererBackendDescriptor* requested_descriptor
) {
    const KainRendererBackendDescriptor* fallback_descriptor =
        kain_renderer_backend_default();

    if (requested_descriptor && requested_descriptor->available) {
        return requested_descriptor;
    }
    if (fallback_descriptor && fallback_descriptor->available) {
        return fallback_descriptor;
    }
    if (requested_descriptor) {
        return requested_descriptor;
    }
    return fallback_descriptor;
}

static KainRendererSceneExecutorKind kain_renderer_session_executor_for_platform(
    KainPlatformKind platform_kind,
    int graphics_bundle_valid
) {
    (void)graphics_bundle_valid;

    switch (platform_kind) {
        case KAIN_PLATFORM_KIND_WIN32:
        case KAIN_PLATFORM_KIND_LINUX:
            return KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE;
        default:
            return KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY;
    }
}

void kain_runtime_renderer_session_init(KainRuntimeRendererSession* session) {
    if (!session) {
        return;
    }

    memset(session, 0, sizeof(*session));
    session->status = KAIN_RENDERER_SESSION_STATUS_UNINITIALIZED;
    session->platform_kind = KAIN_PLATFORM_KIND_UNKNOWN;
    session->requested_backend_kind = KAIN_RENDERER_BACKEND_UNKNOWN;
    session->active_backend_kind = KAIN_RENDERER_BACKEND_UNKNOWN;
    session->executor_kind = KAIN_RENDERER_SCENE_EXECUTOR_UNKNOWN;
}

int kain_runtime_renderer_session_boot(
    const KainRuntimeGraphicsBundle* graphics_bundle,
    const KainRuntimeGraphicsValidation* graphics_validation,
    KainPlatformKind platform_kind,
    const char* requested_backend_id,
    KainRuntimeRendererSession* session
) {
    const KainRendererBackendDescriptor* requested_descriptor;
    const KainRendererBackendDescriptor* active_descriptor;
    int graphics_bundle_valid = 0;

    if (!session) {
        return 0;
    }

    kain_runtime_renderer_session_init(session);
    session->platform_kind = platform_kind;
    session->graphics_bundle_loaded =
        graphics_bundle != NULL && graphics_bundle->loaded;
    session->graphics_bundle_valid =
        graphics_validation != NULL &&
        (graphics_validation->graphics_lane_ready ||
         graphics_validation->has_render_scene ||
         graphics_validation->has_viewport3d);
    if (session->graphics_bundle_valid) {
        graphics_bundle_valid = 1;
    }

    requested_descriptor =
        kain_renderer_session_resolve_requested_backend(requested_backend_id);
    active_descriptor =
        kain_renderer_session_resolve_active_backend(requested_descriptor);

    if (!requested_descriptor || !active_descriptor) {
        session->status = KAIN_RENDERER_SESSION_STATUS_FAILED;
        kain_renderer_session_copy_text(
            session->summary,
            sizeof(session->summary),
            "renderer session could not resolve any backend descriptor"
        );
        kain_renderer_session_copy_text(
            session->diagnostic,
            sizeof(session->diagnostic),
            "renderer backend catalog is empty or invalid"
        );
        return 0;
    }

    session->requested_backend_kind = requested_descriptor->kind;
    session->active_backend_kind = active_descriptor->kind;
    session->backend_declared_available = active_descriptor->available;
    session->backend_probe_passed = active_descriptor->available ? 1 : 0;
    session->backend_session_ready = active_descriptor->available ? 1 : 0;
    kain_renderer_session_copy_text(
        session->requested_backend_id,
        sizeof(session->requested_backend_id),
        requested_descriptor->id
    );
    kain_renderer_session_copy_text(
        session->active_backend_id,
        sizeof(session->active_backend_id),
        active_descriptor->id
    );
    kain_renderer_session_copy_text(
        session->active_service_key,
        sizeof(session->active_service_key),
        active_descriptor->service_key
    );
    kain_renderer_session_copy_text(
        session->backend_runtime_name,
        sizeof(session->backend_runtime_name),
        active_descriptor->runtime_name
    );
    kain_renderer_session_copy_text(
        session->backend_version,
        sizeof(session->backend_version),
        "runtime-owned"
    );

    session->executor_kind = kain_renderer_session_executor_for_platform(
        platform_kind,
        graphics_bundle_valid
    );
    session->scene_execution_available =
        session->executor_kind != KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY;
    session->used_compatibility_executor =
        session->executor_kind == KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE;

    if (graphics_bundle && graphics_bundle->primary_scene[0]) {
        kain_renderer_session_copy_text(
            session->scene_name,
            sizeof(session->scene_name),
            graphics_bundle->primary_scene
        );
    }

    if (session->scene_execution_available) {
        session->status = session->used_compatibility_executor
            ? KAIN_RENDERER_SESSION_STATUS_DEGRADED
            : KAIN_RENDERER_SESSION_STATUS_READY;
    } else {
        session->status = KAIN_RENDERER_SESSION_STATUS_FAILED;
    }

    if (requested_descriptor != active_descriptor) {
        snprintf(
            session->diagnostic,
            sizeof(session->diagnostic),
            "requested backend `%s` is not active on this host; routed through `%s`",
            requested_descriptor->id,
            active_descriptor->id
        );
    } else if (!session->backend_declared_available) {
        snprintf(
            session->diagnostic,
            sizeof(session->diagnostic),
            "backend `%s` is not active on this host",
            active_descriptor->id
        );
    } else if (session->used_compatibility_executor) {
        snprintf(
            session->diagnostic,
            sizeof(session->diagnostic),
            "scene execution is currently routed through the Kain compatibility executor while `%s` owns backend identity and diagnostics",
            active_descriptor->id
        );
    }

    if (session->status == KAIN_RENDERER_SESSION_STATUS_FAILED &&
        !session->scene_execution_available) {
        snprintf(
            session->summary,
            sizeof(session->summary),
            "%s | %s | diagnostics only",
            active_descriptor->display_name,
            kain_runtime_renderer_session_status_name(session->status)
        );
        return 0;
    }

    snprintf(
        session->summary,
        sizeof(session->summary),
        "%s | %s | executor %s%s%s",
        active_descriptor->display_name,
        kain_runtime_renderer_session_status_name(session->status),
        kain_runtime_renderer_scene_executor_name(session->executor_kind),
        session->scene_name[0] ? " | scene " : "",
        session->scene_name[0] ? session->scene_name : ""
    );

    return session->scene_execution_available;
}

void kain_runtime_renderer_session_shutdown(KainRuntimeRendererSession* session) {
    (void)session;
}

const char* kain_runtime_renderer_session_status_name(
    KainRendererSessionStatus status
) {
    switch (status) {
        case KAIN_RENDERER_SESSION_STATUS_READY:
            return "ready";
        case KAIN_RENDERER_SESSION_STATUS_DEGRADED:
            return "degraded";
        case KAIN_RENDERER_SESSION_STATUS_FAILED:
            return "failed";
        case KAIN_RENDERER_SESSION_STATUS_UNINITIALIZED:
        default:
            return "uninitialized";
    }
}

const char* kain_runtime_renderer_scene_executor_name(
    KainRendererSceneExecutorKind executor_kind
) {
    switch (executor_kind) {
        case KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE:
            return "compatibility-software";
        case KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY:
            return "diagnostics-only";
        case KAIN_RENDERER_SCENE_EXECUTOR_UNKNOWN:
        default:
            return "unknown";
    }
}

int kain_runtime_renderer_session_should_use_compatibility_executor(
    const KainRuntimeRendererSession* session
) {
    return session != NULL && session->used_compatibility_executor;
}

void kain_runtime_renderer_session_format_summary(
    const KainRuntimeRendererSession* session,
    char* out,
    size_t out_cap
) {
    if (!out || out_cap == 0) {
        return;
    }

    out[0] = '\0';
    if (!session) {
        return;
    }

    snprintf(
        out,
        out_cap,
        "%s -> %s | %s | %s",
        session->requested_backend_id[0] ? session->requested_backend_id : "auto",
        session->active_backend_id[0] ? session->active_backend_id : "none",
        kain_runtime_renderer_session_status_name(session->status),
        kain_runtime_renderer_scene_executor_name(session->executor_kind)
    );
}
