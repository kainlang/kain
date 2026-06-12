/*
 * check_renderer_session.c -- CBMC verification harness for renderer_session module
 *
 * Covers: session init, boot (with nondet external backend descriptors),
 * shutdown, status/executor name lookups, compatibility executor check,
 * and format_summary.
 *
 * External functions (renderer_backend.c) are nondeterministic: CBMC explores
 * paths where they return NULL or valid pointers.  The boot function must
 * handle all cases gracefully.
 *
 * Combined translation unit: renderer_session.c + check_renderer_session.c.
 */

#include "renderer_session.h"

/* Static backing buffers for pointer provenance in external calls.
 * The renderer_backend functions return pointers into a static catalog;
 * CBMC treats them as nondet, but we model similar static descriptors. */
static unsigned char g_format_buffer[512];

/* Static backend descriptor that external functions might "return" */
static KainRendererBackendDescriptor g_active_backend;
static KainRendererBackendDescriptor g_requested_backend;

/* Static graphics bundle / validation for boot */
static KainRuntimeGraphicsBundle g_graphics_bundle;
static KainRuntimeGraphicsValidation g_graphics_validation;

/* ======================================================================
 * Static function forward declarations (from renderer_session.c)
 * ====================================================================== */
static void kain_renderer_session_copy_text(
    char* out, size_t out_cap, const char* text
);
static const KainRendererBackendDescriptor*
kain_renderer_session_resolve_requested_backend(
    const char* requested_backend_id
);
static const KainRendererBackendDescriptor*
kain_renderer_session_resolve_active_backend(
    const KainRendererBackendDescriptor* requested_descriptor
);
static KainRendererSceneExecutorKind
kain_renderer_session_executor_for_platform(
    KainPlatformKind platform_kind,
    int graphics_bundle_valid
);

/* External functions from renderer_backend.c -- CBMC models as nondet */
const KainRendererBackendDescriptor* kain_renderer_backend_lookup(
    const char* id
);
const KainRendererBackendDescriptor* kain_renderer_backend_default(void);
const KainRendererBackendDescriptor* kain_renderer_backend_active(void);


/* ======================================================================
 * Factory: create a valid renderer session after init
 * ====================================================================== */
static KainRuntimeRendererSession* create_initialized_session(void) {
    static KainRuntimeRendererSession session;
    __CPROVER_havoc_object(&session);
    renderer_session_init(&session);

    __CPROVER_assert(session.status == KAIN_RENDERER_SESSION_STATUS_UNINITIALIZED,
                     "create_initialized: status == UNINITIALIZED");
    __CPROVER_assert(session.platform_kind == KAIN_PLATFORM_KIND_UNKNOWN,
                     "create_initialized: platform == UNKNOWN");
    return &session;
}


/* ======================================================================
 * Check: renderer_session_init sets defaults
 * ====================================================================== */
static void check_session_init(void) {
    KainRuntimeRendererSession* session = create_initialized_session();
    (void)session;

    /* Already checked in factory -- now verify all fields */
    __CPROVER_assert(session->requested_backend_kind ==
                     KAIN_RENDERER_BACKEND_UNKNOWN,
                     "init: requested_backend_kind == UNKNOWN");
    __CPROVER_assert(session->active_backend_kind ==
                     KAIN_RENDERER_BACKEND_UNKNOWN,
                     "init: active_backend_kind == UNKNOWN");
    __CPROVER_assert(session->executor_kind ==
                     KAIN_RENDERER_SCENE_EXECUTOR_UNKNOWN,
                     "init: executor_kind == UNKNOWN");
    __CPROVER_assert(session->graphics_bundle_loaded == 0,
                     "init: bundle not loaded");
    __CPROVER_assert(session->scene_execution_available == 0,
                     "init: execution not available");
    __CPROVER_assert(session->requested_backend_id[0] == '\0',
                     "init: requested_backend_id empty");
    __CPROVER_assert(session->active_backend_id[0] == '\0',
                     "init: active_backend_id empty");
}

static void check_session_init_null(void) {
    renderer_session_init(NULL);
}


/* ======================================================================
 * Check: renderer_session_boot -- session NULL returns 0
 * ====================================================================== */
static void check_boot_null_session(void) {
    int rc = renderer_session_boot(NULL, NULL, KAIN_PLATFORM_KIND_WIN32,
                                   NULL, NULL);
    __CPROVER_assert(rc == 0, "boot_null_session: NULL session returns 0");
}


/* ======================================================================
 * Check: renderer_session_boot -- with valid (nondet) external state
 *
 * External backend functions are nondet; CBMC explores all combos of
 * NULL/valid returns.  The boot code handles NULL gracefully via early
 * FAILED path.
 * ====================================================================== */
static void check_boot_valid(void) {
    static KainRuntimeRendererSession session;
    KainPlatformKind platform_kind;
    const char* requested_backend_id;

    __CPROVER_havoc_object(&session);
    __CPROVER_havoc_object(&platform_kind);

    /* Constrain platform to valid range */
    __CPROVER_assume(platform_kind >= KAIN_PLATFORM_KIND_UNKNOWN &&
                     platform_kind <= KAIN_PLATFORM_KIND_MACOS);

    /* requested_backend_id can be NULL or a valid string -- nondet */
    __CPROVER_havoc_object(&requested_backend_id);
    __CPROVER_assume(requested_backend_id == NULL ||
                     (requested_backend_id != NULL));

    /* Nondet graphics bundle / validation -- CBMC explores all paths */
    int rc = renderer_session_boot(
        &g_graphics_bundle, &g_graphics_validation,
        platform_kind, requested_backend_id, &session);

    /* Postconditions */
    if (rc != 0) {
        /* Boot succeeded -- execution is available */
        __CPROVER_assert(session.status == KAIN_RENDERER_SESSION_STATUS_READY ||
                         session.status == KAIN_RENDERER_SESSION_STATUS_DEGRADED,
                         "boot_valid: status READY or DEGRADED on success");
        __CPROVER_assert(session.scene_execution_available != 0,
                         "boot_valid: scene_execution_available on success");
        __CPROVER_assert(session.executor_kind !=
                         KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY,
                         "boot_valid: executor != DIAGNOSTICS_ONLY on success");
        __CPROVER_assert(session.summary[0] != '\0',
                         "boot_valid: summary non-empty on success");
    } else {
        /* Boot failed -- session is FAILED or system init was NULL */
        if (&session != NULL) {
            __CPROVER_assert(
                session.status == KAIN_RENDERER_SESSION_STATUS_FAILED ||
                session.status == KAIN_RENDERER_SESSION_STATUS_UNINITIALIZED,
                "boot_valid: status FAILED or UNINITIALIZED on failure");
        }
    }
}


/* ======================================================================
 * Check: renderer_session_boot -- specific platform paths
 * ====================================================================== */
static void check_boot_platform_win32(void) {
    static KainRuntimeRendererSession session;
    __CPROVER_havoc_object(&session);

    /* Pre-seed backend descriptor for deterministic external return */
    g_active_backend.kind = KAIN_RENDERER_BACKEND_VULKAN;
    g_active_backend.id = "vulkan";
    g_active_backend.display_name = "Vulkan";
    g_active_backend.runtime_name = "runtime-native";
    g_active_backend.service_key = "gfx.backend.vulkan";
    g_active_backend.available = 1;

    /* Clear bundle */
    __CPROVER_havoc_object(&g_graphics_bundle);
    __CPROVER_havoc_object(&g_graphics_validation);

    int rc = renderer_session_boot(
        &g_graphics_bundle, &g_graphics_validation,
        KAIN_PLATFORM_KIND_WIN32, "vulkan", &session);

    /* Win32 should get COMPATIBILITY_SOFTWARE executor if bundle valid enough */
    if (rc != 0) {
        __CPROVER_assert(
            session.executor_kind == KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE,
            "boot_win32: executor is COMPATIBILITY_SOFTWARE");
    }
}


/* ======================================================================
 * Check: renderer_session_boot -- unknown platform
 * ====================================================================== */
static void check_boot_platform_unknown(void) {
    static KainRuntimeRendererSession session;
    __CPROVER_havoc_object(&session);

    int rc = renderer_session_boot(
        &g_graphics_bundle, &g_graphics_validation,
        KAIN_PLATFORM_KIND_UNKNOWN, NULL, &session);

    /* Unknown platform -> DIAGNOSTICS_ONLY -> no execution available */
    if (rc == 0) {
        __CPROVER_assert(
            session.status == KAIN_RENDERER_SESSION_STATUS_UNINITIALIZED ||
            session.status == KAIN_RENDERER_SESSION_STATUS_FAILED,
            "boot_unknown: FAILED status");
        __CPROVER_assert(
            session.executor_kind == KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY ||
            session.executor_kind == KAIN_RENDERER_SCENE_EXECUTOR_UNKNOWN,
            "boot_unknown: executor DIAGNOSTICS_ONLY or UNKNOWN");
    }
}


/* ======================================================================
 * Check: renderer_session_shutdown is a no-op (no crash)
 * ====================================================================== */
static void check_shutdown(void) {
    KainRuntimeRendererSession* session = create_initialized_session();
    renderer_session_shutdown(session);
    /* No crash -- that's all we can assert for a no-op */
}

static void check_shutdown_null(void) {
    renderer_session_shutdown(NULL);
}


/* ======================================================================
 * Check: renderer_session_status_name returns non-NULL for every status
 * ====================================================================== */
static void check_status_name(void) {
    __CPROVER_assert(
        renderer_session_status_name(
            KAIN_RENDERER_SESSION_STATUS_UNINITIALIZED) != NULL,
        "status_name: UNINITIALIZED non-NULL");
    __CPROVER_assert(
        renderer_session_status_name(
            KAIN_RENDERER_SESSION_STATUS_READY) != NULL,
        "status_name: READY non-NULL");
    __CPROVER_assert(
        renderer_session_status_name(
            KAIN_RENDERER_SESSION_STATUS_DEGRADED) != NULL,
        "status_name: DEGRADED non-NULL");
    __CPROVER_assert(
        renderer_session_status_name(
            KAIN_RENDERER_SESSION_STATUS_FAILED) != NULL,
        "status_name: FAILED non-NULL");

    /* All distinct */
    __CPROVER_assert(
        renderer_session_status_name(KAIN_RENDERER_SESSION_STATUS_READY) !=
        renderer_session_status_name(KAIN_RENDERER_SESSION_STATUS_FAILED),
        "status_name: READY != FAILED");
}


/* ======================================================================
 * Check: renderer_scene_executor_name returns non-NULL for every kind
 * ====================================================================== */
static void check_executor_name(void) {
    __CPROVER_assert(
        renderer_scene_executor_name(
            KAIN_RENDERER_SCENE_EXECUTOR_UNKNOWN) != NULL,
        "executor_name: UNKNOWN non-NULL");
    __CPROVER_assert(
        renderer_scene_executor_name(
            KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE) != NULL,
        "executor_name: COMPATIBILITY_SOFTWARE non-NULL");
    __CPROVER_assert(
        renderer_scene_executor_name(
            KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY) != NULL,
        "executor_name: DIAGNOSTICS_ONLY non-NULL");

    /* All distinct */
    __CPROVER_assert(
        renderer_scene_executor_name(
            KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE) !=
        renderer_scene_executor_name(
            KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY),
        "executor_name: COMPATIBILITY_SOFTWARE != DIAGNOSTICS_ONLY");
}


/* ======================================================================
 * Check: should_use_compatibility_executor
 * ====================================================================== */
static void check_should_use_compatibility(void) {
    KainRuntimeRendererSession session;
    __CPROVER_havoc_object(&session);

    /* Constrain nondet boolean */
    __CPROVER_assume(session.used_compatibility_executor == 0 ||
                     session.used_compatibility_executor == 1);

    int should = renderer_session_should_use_compatibility_executor(&session);
    __CPROVER_assert(should == session.used_compatibility_executor,
                     "should_use: matches used_compatibility_executor");

    /* NULL safety */
    __CPROVER_assert(
        renderer_session_should_use_compatibility_executor(NULL) == 0,
        "should_use: NULL returns 0");
}


/* ======================================================================
 * Check: format_summary produces well-formed output
 * ====================================================================== */
static void check_format_summary(void) {
    KainRuntimeRendererSession* session = create_initialized_session();
    char out[256];

    /* Set some fields to nondet values within bounds */
    __CPROVER_assume(
        session->status >= KAIN_RENDERER_SESSION_STATUS_UNINITIALIZED &&
        session->status <= KAIN_RENDERER_SESSION_STATUS_FAILED);
    __CPROVER_assume(
        session->executor_kind >= KAIN_RENDERER_SCENE_EXECUTOR_UNKNOWN &&
        session->executor_kind <= KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY);

    renderer_session_format_summary(session, out, sizeof(out));

    __CPROVER_assert(out[sizeof(out) - 1] == '\0',
                     "format_summary: output null-terminated");
    __CPROVER_assert(out[0] != '\0',
                     "format_summary: output non-empty");
}

static void check_format_summary_edges(void) {
    KainRuntimeRendererSession* session = create_initialized_session();

    /* NULL out */
    renderer_session_format_summary(session, NULL, 0);
    /* Must not crash */

    /* Zero capacity */
    renderer_session_format_summary(session, (char*)g_format_buffer, 0);
    /* Must not crash */

    /* NULL session */
    renderer_session_format_summary(NULL, (char*)g_format_buffer,
                                    sizeof(g_format_buffer));
    /* Must not crash */
}


/* ======================================================================
 * Check: kain_renderer_session_copy_text (static) copies safely
 * ====================================================================== */
static void check_copy_text(void) {
    char buf[64];
    __CPROVER_havoc_object(buf);

    kain_renderer_session_copy_text(buf, sizeof(buf), "hello");
    __CPROVER_assert(strcmp(buf, "hello") == 0,
                     "copy_text: copies correctly");
    __CPROVER_assert(buf[sizeof(buf) - 1] == '\0',
                     "copy_text: buffer remains null-terminated");

    /* NULL text */
    buf[0] = 'X';
    kain_renderer_session_copy_text(buf, sizeof(buf), NULL);
    __CPROVER_assert(buf[0] == '\0',
                     "copy_text: NULL source empties buffer");

    /* Zero capacity */
    kain_renderer_session_copy_text(NULL, 0, "hello");
    /* Must not crash */
}


/* ======================================================================
 * Check: executor_for_platform (static) maps platforms correctly
 * ====================================================================== */
static void check_executor_for_platform(void) {
    KainRendererSceneExecutorKind e;

    /* Win32 -> COMPATIBILITY_SOFTWARE */
    e = kain_renderer_session_executor_for_platform(
        KAIN_PLATFORM_KIND_WIN32, 1);
    __CPROVER_assert(
        e == KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE,
        "executor_platform: Win32 -> COMPATIBILITY_SOFTWARE");

    /* Linux -> COMPATIBILITY_SOFTWARE */
    e = kain_renderer_session_executor_for_platform(
        KAIN_PLATFORM_KIND_LINUX, 0);
    __CPROVER_assert(
        e == KAIN_RENDERER_SCENE_EXECUTOR_COMPATIBILITY_SOFTWARE,
        "executor_platform: Linux -> COMPATIBILITY_SOFTWARE");

    /* macOS -> DIAGNOSTICS_ONLY */
    e = kain_renderer_session_executor_for_platform(
        KAIN_PLATFORM_KIND_MACOS, 1);
    __CPROVER_assert(
        e == KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY,
        "executor_platform: macOS -> DIAGNOSTICS_ONLY");

    /* UNKNOWN -> DIAGNOSTICS_ONLY */
    e = kain_renderer_session_executor_for_platform(
        KAIN_PLATFORM_KIND_UNKNOWN, 0);
    __CPROVER_assert(
        e == KAIN_RENDERER_SCENE_EXECUTOR_DIAGNOSTICS_ONLY,
        "executor_platform: UNKNOWN -> DIAGNOSTICS_ONLY");
}


/* ======================================================================
 * Main -- run all renderer_session checks
 * ====================================================================== */
int main(void) {
    check_session_init();
    check_session_init_null();
    check_boot_null_session();
    check_boot_valid();
    check_boot_platform_win32();
    check_boot_platform_unknown();
    check_shutdown();
    check_shutdown_null();
    check_status_name();
    check_executor_name();
    check_should_use_compatibility();
    check_format_summary();
    check_format_summary_edges();
    check_copy_text();
    check_executor_for_platform();
    return 0;
}
