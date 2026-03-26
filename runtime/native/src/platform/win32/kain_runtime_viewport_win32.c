#include "../../../include/kain_runtime_win32.h"
#include "../../../include/kain_runtime_asset.h"
#include "../../../include/kain_runtime_contract.h"
#include "../../../include/kain_runtime_graphics.h"
#include "../../../include/kain_runtime_realtime.h"
#include "../../../include/kain_runtime_ui.h"

#ifdef _WIN32
typedef struct {
    const KainViewportProfile* profile;
    int window_width;
    int window_height;
    int show_help;
    int capture_mouse_on_launch;
    int particle_count;
    double move_speed;
    double sprint_multiplier;
    double jump_velocity;
    double gravity;
    double mouse_sensitivity;
    double eye_height;
    double fog_density;
} KainNativeViewportSettings;

typedef struct {
    KainWin32AppHost host;
    KainWin32GlSurface surface;
    KainWin32MouseCapture mouse_capture;
    KainRuntimeContractBundle runtime_contract;
    KainRuntimeContractValidation contract_validation;
    KainRuntimeGraphicsBundle graphics_bundle;
    KainRuntimeGraphicsValidation graphics_validation;
    KainRuntimeGraphicsExecutionState compute_execution;
    HMODULE gpu_runtime_library;
    void* gpu_runtime_handle;
    KainGpuRuntimeDispatchFn gpu_runtime_dispatch;
    KainGpuRuntimeDestroyFn gpu_runtime_destroy;
    char shader_bundle_path[KAIN_RUNTIME_GRAPHICS_MAX_PATH];
    char compute_residency_path[KAIN_RUNTIME_GRAPHICS_MAX_PATH];
    KainRuntimeRealtimeBundle realtime_bundle;
    KainUiCompiledBundle compiled_ui;
    KainNativeSceneAsset world_asset;
    HWND hwnd;
    int width;
    int height;
    int running;
    int keys[256];
    int previous_space_down;
    double camera_x;
    double camera_y;
    double camera_z;
    double yaw;
    double pitch;
    double velocity_y;
    double world_ground_y;
    double camera_near_clip;
    double camera_far_clip;
    double camera_fov_y_degrees;
    int grounded;
    double total_time;
    double frame_delta;
    double frame_fps;
    double fps_accumulator;
    int fps_frames;
    LARGE_INTEGER perf_freq;
    LARGE_INTEGER prev_counter;
    KainNativeViewportSettings settings;
} KainNativeViewportApp;

static void kain_native_viewport_apply_profile_defaults(
    KainNativeViewportSettings* settings,
    const KainViewportProfile* profile
);
static void kain_native_viewport_apply_realtime_presentation(KainNativeViewportApp* app);
static void kain_native_viewport_apply_realtime_camera(KainNativeViewportApp* app);

static void kain_native_viewport_try_load_compiled_ui(KainNativeViewportApp* app) {
    if (!app) {
        return;
    }

    if ((app->contract_validation.downgraded_optional_mask & KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED) != 0u) {
        kain_ui_compiled_bundle_init(&app->compiled_ui);
        return;
    }

    if (kain_ui_compiled_bundle_load_from_env(KAIN_UI_COMPILED_BUNDLE_ENV, &app->compiled_ui)) {
        if (!app->realtime_bundle.loaded && app->compiled_ui.primary_viewport_scene[0]) {
            const KainViewportProfile* profile =
                kain_find_viewport_profile(app->compiled_ui.primary_viewport_scene);
            if (profile) {
                kain_native_viewport_apply_profile_defaults(&app->settings, profile);
            }
        }
    } else {
        kain_ui_compiled_bundle_init(&app->compiled_ui);
    }
}

static void kain_native_viewport_try_load_runtime_contract(KainNativeViewportApp* app) {
    if (!app) {
        return;
    }

    if (!kain_runtime_contract_load_for_current_process(
            KAIN_RUNTIME_CONTRACT_ENV,
            &app->runtime_contract
        )) {
        kain_runtime_contract_init(&app->runtime_contract);
    }
}

static void kain_native_viewport_try_load_realtime_bundle(KainNativeViewportApp* app) {
    if (!app) {
        return;
    }

    if (!kain_runtime_realtime_load_for_current_process(
            KAIN_RUNTIME_REALTIME_ENV,
            &app->realtime_bundle
        )) {
        kain_runtime_realtime_init(&app->realtime_bundle);
        return;
    }

    if (app->realtime_bundle.primary_scene[0]) {
        const KainViewportProfile* profile =
            kain_find_viewport_profile(app->realtime_bundle.primary_scene);
        if (profile) {
            kain_native_viewport_apply_profile_defaults(&app->settings, profile);
        }
    }
    kain_native_viewport_apply_realtime_presentation(app);
}

static void kain_native_viewport_try_load_graphics_bundle(KainNativeViewportApp* app) {
    if (!app) {
        return;
    }

    if (!kain_runtime_graphics_load_for_current_process(
            KAIN_RUNTIME_GRAPHICS_ENV,
            &app->graphics_bundle
        )) {
        kain_runtime_graphics_init(&app->graphics_bundle);
        kain_runtime_graphics_validation_init(&app->graphics_validation);
        kain_runtime_graphics_execution_state_init(&app->compute_execution);
        return;
    }

    if (!kain_runtime_graphics_validate_bundle(&app->graphics_bundle, &app->graphics_validation)) {
        fprintf(
            stderr,
            "[KAIN][raw-native-viewport] graphics bundle degraded: %s\n",
            app->graphics_validation.reason[0] ? app->graphics_validation.reason : "unknown"
        );
    }
}

static int kain_native_viewport_resolve_file(
    const char* env_name,
    const char* fallback_file_name,
    char* out_path,
    size_t out_cap
) {
    char* explicit_path = NULL;
    char module_path[MAX_PATH];
    char* file_name = NULL;

    if (!out_path || out_cap == 0 || !fallback_file_name) {
        return 0;
    }
    out_path[0] = '\0';

    if (env_name) {
        explicit_path = kain_env_dup(env_name);
        if (explicit_path && explicit_path[0]) {
            strncpy_s(out_path, out_cap, explicit_path, _TRUNCATE);
            kain_env_free(explicit_path);
            return 1;
        }
        kain_env_free(explicit_path);
    }

    if (!GetModuleFileNameA(NULL, module_path, (DWORD)sizeof(module_path))) {
        return 0;
    }
    file_name = strrchr(module_path, '\\');
    if (!file_name) {
        return 0;
    }
    file_name[1] = '\0';
    if (strnlen_s(module_path, sizeof(module_path)) + strlen(fallback_file_name) + 1 >= out_cap) {
        return 0;
    }
    strcpy_s(out_path, out_cap, module_path);
    strcat_s(out_path, out_cap, fallback_file_name);
    return GetFileAttributesA(out_path) != INVALID_FILE_ATTRIBUTES;
}

static unsigned int kain_native_viewport_optional_service_mask(void) {
    unsigned int mask = 0u;
    char* compiled_ui_path = kain_env_dup(KAIN_UI_COMPILED_BUNDLE_ENV);
    char* world_asset_path = kain_env_dup(KAIN_NATIVE_WORLD_ASSET_ENV);
    if (compiled_ui_path && compiled_ui_path[0]) {
        mask |= KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED;
    }
    if (world_asset_path && world_asset_path[0]) {
        mask |= KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF;
    }
    mask |= KAIN_RUNTIME_SERVICE_GFX_COMPUTE;
    kain_env_free(compiled_ui_path);
    kain_env_free(world_asset_path);
    return mask;
}

static int kain_native_viewport_prepare_compute_executor(KainNativeViewportApp* app) {
    char runtime_library_path[KAIN_RUNTIME_GRAPHICS_MAX_PATH];
    KainGpuRuntimeCreateFn gpu_runtime_create;

    if (!app) {
        return 0;
    }

    kain_runtime_graphics_execution_state_init(&app->compute_execution);
    if (app->graphics_bundle.shader_compute_ref_count <= 0) {
        return 1;
    }

    if (!kain_native_viewport_resolve_file(
            "KAIN_SHADER_BUNDLE_PATH",
            "kain_shader_bundle.json",
            app->shader_bundle_path,
            sizeof(app->shader_bundle_path)
        )) {
        snprintf(
            app->graphics_validation.reason,
            sizeof(app->graphics_validation.reason),
            "missing shader bundle sidecar for compute execution"
        );
        return 0;
    }
    if (!kain_native_viewport_resolve_file(
            KAIN_COMPUTE_RESIDENCY_ENV,
            "kain_compute_residency.json",
            app->compute_residency_path,
            sizeof(app->compute_residency_path)
        )) {
        snprintf(
            app->graphics_validation.reason,
            sizeof(app->graphics_validation.reason),
            "missing compute residency sidecar for compute execution"
        );
        return 0;
    }
    if (!kain_native_viewport_resolve_file(
            KAIN_GPU_RUNTIME_LIBRARY_ENV,
            KAIN_GPU_RUNTIME_WINDOWS_DLL,
            runtime_library_path,
            sizeof(runtime_library_path)
        )) {
        snprintf(
            app->graphics_validation.reason,
            sizeof(app->graphics_validation.reason),
            "missing %s for compute execution",
            KAIN_GPU_RUNTIME_WINDOWS_DLL
        );
        return 0;
    }

    app->gpu_runtime_library = LoadLibraryA(runtime_library_path);
    if (!app->gpu_runtime_library) {
        snprintf(
            app->graphics_validation.reason,
            sizeof(app->graphics_validation.reason),
            "failed to load compute runtime library %s",
            runtime_library_path
        );
        return 0;
    }

    gpu_runtime_create = (KainGpuRuntimeCreateFn)GetProcAddress(
        app->gpu_runtime_library,
        "kain_gpu_runtime_create"
    );
    app->gpu_runtime_dispatch = (KainGpuRuntimeDispatchFn)GetProcAddress(
        app->gpu_runtime_library,
        "kain_gpu_runtime_dispatch_primary_compute"
    );
    app->gpu_runtime_destroy = (KainGpuRuntimeDestroyFn)GetProcAddress(
        app->gpu_runtime_library,
        "kain_gpu_runtime_destroy"
    );
    if (!gpu_runtime_create || !app->gpu_runtime_dispatch || !app->gpu_runtime_destroy) {
        snprintf(
            app->graphics_validation.reason,
            sizeof(app->graphics_validation.reason),
            "compute runtime library is missing required exports"
        );
        FreeLibrary(app->gpu_runtime_library);
        app->gpu_runtime_library = NULL;
        return 0;
    }

    app->gpu_runtime_handle = gpu_runtime_create(NULL);
    if (!app->gpu_runtime_handle) {
        snprintf(
            app->graphics_validation.reason,
            sizeof(app->graphics_validation.reason),
            "compute runtime failed to initialize a Vulkan executor"
        );
        FreeLibrary(app->gpu_runtime_library);
        app->gpu_runtime_library = NULL;
        return 0;
    }

    return 1;
}

static void kain_native_viewport_emit_contract_diagnostics(
    const char* app_label,
    const KainRuntimeContractValidation* validation
) {
    int i;
    if (!validation) {
        return;
    }
    if (validation->fatal_error && validation->fatal_message[0]) {
        fprintf(stderr, "[KAIN][%s] %s\n", app_label, validation->fatal_message);
    }
    for (i = 0; i < validation->warning_count; ++i) {
        if (validation->warnings[i][0]) {
            fprintf(stderr, "[KAIN][%s] warning: %s\n", app_label, validation->warnings[i]);
        }
    }
}

static int kain_native_viewport_validate_runtime_contract(KainNativeViewportApp* app) {
    unsigned int optional_service_mask;
    if (!app) {
        return 0;
    }
    optional_service_mask = kain_native_viewport_optional_service_mask();
    if (!kain_runtime_contract_validate_startup(
            &app->runtime_contract,
            KAIN_RUNTIME_SERVICE_CORE_MASK,
            optional_service_mask,
            &app->contract_validation
        )) {
        kain_native_viewport_emit_contract_diagnostics("raw-native-viewport", &app->contract_validation);
        MessageBoxA(
            NULL,
            app->contract_validation.fatal_message[0]
                ? app->contract_validation.fatal_message
                : "Raw native viewport startup validation failed.",
            "Kain Runtime Contract Error",
            MB_OK | MB_ICONERROR
        );
        return 0;
    }
    kain_native_viewport_emit_contract_diagnostics("raw-native-viewport", &app->contract_validation);
    return 1;
}

static KainNativeViewportSettings kain_load_viewport_settings(void) {
    KainNativeViewportSettings settings;
    char* profile_name = kain_env_dup("KAIN_NATIVE_SCENE_PROFILE");
    const KainViewportProfile* profile = kain_find_viewport_profile(profile_name);
    kain_env_free(profile_name);
    settings.profile = profile;
    settings.show_help = kain_env_flag("KAIN_NATIVE_SHOW_HELP", 1);
    settings.capture_mouse_on_launch = kain_env_flag("KAIN_NATIVE_CAPTURE_MOUSE", 1);
    {
        char* value = NULL;
        value = kain_env_dup("KAIN_NATIVE_WINDOW_WIDTH");
        settings.window_width = (value && value[0])
            ? kain_env_int("KAIN_NATIVE_WINDOW_WIDTH", profile->default_width)
            : profile->default_width;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_WINDOW_HEIGHT");
        settings.window_height = (value && value[0])
            ? kain_env_int("KAIN_NATIVE_WINDOW_HEIGHT", profile->default_height)
            : profile->default_height;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_PARTICLE_COUNT");
        settings.particle_count = (value && value[0])
            ? kain_env_int("KAIN_NATIVE_PARTICLE_COUNT", profile->particle_count)
            : profile->particle_count;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_MOVE_SPEED");
        settings.move_speed = (value && value[0])
            ? kain_env_double("KAIN_NATIVE_MOVE_SPEED", profile->move_speed)
            : profile->move_speed;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_SPRINT_MULTIPLIER");
        settings.sprint_multiplier = (value && value[0])
            ? kain_env_double("KAIN_NATIVE_SPRINT_MULTIPLIER", profile->sprint_multiplier)
            : profile->sprint_multiplier;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_JUMP_VELOCITY");
        settings.jump_velocity = (value && value[0])
            ? kain_env_double("KAIN_NATIVE_JUMP_VELOCITY", profile->jump_velocity)
            : profile->jump_velocity;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_GRAVITY");
        settings.gravity = (value && value[0])
            ? kain_env_double("KAIN_NATIVE_GRAVITY", profile->gravity)
            : profile->gravity;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_MOUSE_SENSITIVITY");
        settings.mouse_sensitivity = (value && value[0])
            ? kain_env_double("KAIN_NATIVE_MOUSE_SENSITIVITY", profile->mouse_sensitivity)
            : profile->mouse_sensitivity;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_EYE_HEIGHT");
        settings.eye_height = (value && value[0])
            ? kain_env_double("KAIN_NATIVE_EYE_HEIGHT", profile->eye_height)
            : profile->eye_height;
        kain_env_free(value);
        value = kain_env_dup("KAIN_NATIVE_FOG_DENSITY");
        settings.fog_density = (value && value[0])
            ? kain_env_double("KAIN_NATIVE_FOG_DENSITY", profile->fog_density)
            : profile->fog_density;
        kain_env_free(value);
    }
    if (settings.window_width < 640) settings.window_width = 640;
    if (settings.window_height < 360) settings.window_height = 360;
    if (settings.particle_count < 24) settings.particle_count = 24;
    return settings;
}

static int kain_native_viewport_env_has_value(const char* name) {
    char* value = kain_env_dup(name);
    int has_value = value && value[0];
    kain_env_free(value);
    return has_value;
}

static void kain_native_viewport_apply_profile_defaults(
    KainNativeViewportSettings* settings,
    const KainViewportProfile* profile
) {
    if (!settings || !profile) {
        return;
    }
    settings->profile = profile;
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_WINDOW_WIDTH")) {
        settings->window_width = profile->default_width;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_WINDOW_HEIGHT")) {
        settings->window_height = profile->default_height;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_PARTICLE_COUNT")) {
        settings->particle_count = profile->particle_count;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_MOVE_SPEED")) {
        settings->move_speed = profile->move_speed;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_SPRINT_MULTIPLIER")) {
        settings->sprint_multiplier = profile->sprint_multiplier;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_JUMP_VELOCITY")) {
        settings->jump_velocity = profile->jump_velocity;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_GRAVITY")) {
        settings->gravity = profile->gravity;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_MOUSE_SENSITIVITY")) {
        settings->mouse_sensitivity = profile->mouse_sensitivity;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_EYE_HEIGHT")) {
        settings->eye_height = profile->eye_height;
    }
    if (!kain_native_viewport_env_has_value("KAIN_NATIVE_FOG_DENSITY")) {
        settings->fog_density = profile->fog_density;
    }
    if (settings->window_width < 640) settings->window_width = 640;
    if (settings->window_height < 360) settings->window_height = 360;
    if (settings->particle_count < 24) settings->particle_count = 24;
}

static void kain_native_viewport_apply_realtime_presentation(KainNativeViewportApp* app) {
    const KainViewportProfile* profile = NULL;
    if (!app || !app->realtime_bundle.loaded) {
        return;
    }
    if (app->realtime_bundle.primary_presentation_has_profile) {
        profile = kain_find_viewport_profile(app->realtime_bundle.primary_presentation_profile);
        if (profile) {
            kain_native_viewport_apply_profile_defaults(&app->settings, profile);
        }
    }
    if (app->realtime_bundle.primary_presentation_has_fog_density &&
        !kain_native_viewport_env_has_value("KAIN_NATIVE_FOG_DENSITY")) {
        app->settings.fog_density =
            app->realtime_bundle.primary_presentation_fog_density >= 0.0
                ? app->realtime_bundle.primary_presentation_fog_density
                : app->settings.fog_density;
    }
    if (app->realtime_bundle.primary_presentation_has_particle_budget &&
        !kain_native_viewport_env_has_value("KAIN_NATIVE_PARTICLE_COUNT")) {
        app->settings.particle_count = app->realtime_bundle.primary_presentation_particle_budget;
        if (app->settings.particle_count < 24) {
            app->settings.particle_count = 24;
        }
    }
}

static void kain_native_viewport_apply_realtime_camera(KainNativeViewportApp* app) {
    double target_x;
    double target_y;
    double target_z;
    double to_target_x;
    double to_target_y;
    double to_target_z;
    double horizontal_distance;
    if (!app || !app->realtime_bundle.loaded) {
        return;
    }
    if (app->realtime_bundle.primary_camera_has_position) {
        app->camera_x = app->realtime_bundle.primary_camera_position[0];
        app->camera_y = app->realtime_bundle.primary_camera_position[1];
        app->camera_z = app->realtime_bundle.primary_camera_position[2];
    }
    if (app->realtime_bundle.primary_camera_has_target) {
        target_x = app->realtime_bundle.primary_camera_target[0];
        target_y = app->realtime_bundle.primary_camera_target[1];
        target_z = app->realtime_bundle.primary_camera_target[2];
        to_target_x = target_x - app->camera_x;
        to_target_y = target_y - app->camera_y;
        to_target_z = target_z - app->camera_z;
        horizontal_distance = sqrt((to_target_x * to_target_x) + (to_target_z * to_target_z));
        if (horizontal_distance > 0.0001 || fabs(to_target_y) > 0.0001) {
            app->yaw = atan2(to_target_x, -to_target_z);
            app->pitch = atan2(to_target_y, horizontal_distance);
            app->pitch = kain_clampd(app->pitch, -1.25, 1.25);
        }
    }
    if (app->realtime_bundle.primary_camera_has_fov_y_degrees) {
        app->camera_fov_y_degrees = kain_clampd(
            app->realtime_bundle.primary_camera_fov_y_degrees,
            1.0,
            175.0
        );
    }
    if (app->realtime_bundle.primary_camera_has_near_plane) {
        app->camera_near_clip = kain_clampd(
            app->realtime_bundle.primary_camera_near_plane,
            0.001,
            1000.0
        );
    }
    if (app->realtime_bundle.primary_camera_has_far_plane) {
        app->camera_far_clip = app->realtime_bundle.primary_camera_far_plane;
    }
    if (app->camera_far_clip <= app->camera_near_clip) {
        app->camera_far_clip = app->camera_near_clip + 0.1;
    }
}

static void kain_native_capture_mouse(KainNativeViewportApp* app, int capture) {
    if (!app) return;
    kain_win32_mouse_capture_set_pointer_lock(&app->mouse_capture, capture);
}

static double kain_native_viewport_ground_level(const KainNativeViewportApp* app) {
    if (!app) {
        return 0.0;
    }
    return app->world_ground_y;
}

static double kain_native_viewport_max3(double a, double b, double c) {
    double result = a;
    if (b > result) result = b;
    if (c > result) result = c;
    return result;
}

static void kain_gl_draw_unit_box(void) {
    glBegin(GL_QUADS);

    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);

    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);

    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, -0.5f, 0.5f);

    glEnd();
}

static void kain_gl_draw_box(double x, double y, double z, double sx, double sy, double sz, float r, float g, float b) {
    glPushMatrix();
    glTranslated(x, y, z);
    glScaled(sx, sy, sz);
    glColor3f(r, g, b);
    kain_gl_draw_unit_box();
    glPopMatrix();
}

static void kain_gl_draw_orbit_ring(double radius, double y, float r, float g, float b, float alpha) {
    int segment;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, alpha);
    glBegin(GL_LINE_LOOP);
    for (segment = 0; segment < 96; ++segment) {
        double angle = ((double)segment / 96.0) * M_PI * 2.0;
        glVertex3f((GLfloat)(cos(angle) * radius), (GLfloat)y, (GLfloat)(sin(angle) * radius));
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void kain_gl_draw_energy_spokes(double radius, double y, double time_seconds, const float color[3]) {
    int spoke;
    glDisable(GL_LIGHTING);
    glColor4f(color[0], color[1], color[2], 0.72f);
    glBegin(GL_LINES);
    for (spoke = 0; spoke < 18; ++spoke) {
        double angle = ((double)spoke / 18.0) * M_PI * 2.0 + (time_seconds * 0.35);
        double inner_radius = radius * (0.42 + ((spoke % 3) * 0.09));
        glVertex3f((GLfloat)(cos(angle) * inner_radius), (GLfloat)y, (GLfloat)(sin(angle) * inner_radius));
        glVertex3f((GLfloat)(cos(angle) * radius), (GLfloat)y, (GLfloat)(sin(angle) * radius));
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

static int kain_viewport_profile_is(const KainViewportProfile* profile, const char* id) {
    if (!profile || !id || !id[0] || !profile->id || !profile->id[0]) {
        return 0;
    }
    return _stricmp(profile->id, id) == 0;
}

static void kain_gl_render_ground(const KainNativeViewportApp* app) {
    int i;
    double half_extent = 40.0;
    double ground_y = kain_native_viewport_ground_level(app);
    const KainViewportProfile* profile = app->settings.profile;
    if (app->world_asset.loaded) {
        double world_x = fabs(app->world_asset.world_bounds_max.x - app->world_asset.world_bounds_min.x);
        double world_z = fabs(app->world_asset.world_bounds_max.z - app->world_asset.world_bounds_min.z);
        half_extent = kain_clampd(kain_native_viewport_max3(world_x, world_z, 24.0) * 0.7, 24.0, 140.0);
    }
    glColor3f(0.18f, 0.20f, 0.28f);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f((GLfloat)-half_extent, (GLfloat)(ground_y - 0.03), (GLfloat)-half_extent);
    glVertex3f((GLfloat)half_extent, (GLfloat)(ground_y - 0.03), (GLfloat)-half_extent);
    glVertex3f((GLfloat)half_extent, (GLfloat)(ground_y - 0.03), (GLfloat)half_extent);
    glVertex3f((GLfloat)-half_extent, (GLfloat)(ground_y - 0.03), (GLfloat)half_extent);
    glEnd();

    glDisable(GL_LIGHTING);
    glColor4f(profile->accent_a[0], profile->accent_a[1], profile->accent_a[2], app->world_asset.loaded ? 0.10f : 0.45f);
    glBegin(GL_LINES);
    for (i = (int)-half_extent; i <= (int)half_extent; i += app->world_asset.loaded ? 6 : 2) {
        glVertex3f((float)i, (GLfloat)(ground_y + 0.02), (GLfloat)-half_extent);
        glVertex3f((float)i, (GLfloat)(ground_y + 0.02), (GLfloat)half_extent);
        glVertex3f((GLfloat)-half_extent, (GLfloat)(ground_y + 0.02), (float)i);
        glVertex3f((GLfloat)half_extent, (GLfloat)(ground_y + 0.02), (float)i);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

static void kain_gl_render_scene_geometry(const KainNativeViewportApp* app) {
    int i;
    const KainViewportProfile* profile = app->settings.profile;
    double pulse = 0.5 + (sin(app->total_time * 1.7) * 0.5);
    double compute_phase = app->compute_execution.executed ? app->compute_execution.phase : 0.0;

    if (app->world_asset.loaded) {
        kain_native_scene_asset_render(&app->world_asset);
        return;
    }

    if (kain_viewport_profile_is(profile, "tensor_stream_probe")) {
        int relay_index;
        int lane_index;
        double throughput_scale = app->compute_execution.executed
            ? kain_clampd(app->compute_execution.throughput / 200000.0, 0.2, 1.0)
            : 0.28;
        double dispatch_extent = app->graphics_bundle.primary_compute.dispatch_size[0] > 0
            ? kain_clampd((double)app->graphics_bundle.primary_compute.dispatch_size[0] / 8.0, 4.0, 12.0)
            : 5.5;

        kain_gl_draw_box(0.0, 0.45, 0.0, 12.0, 0.9, 12.0, 0.06f, 0.09f, 0.14f);
        kain_gl_draw_box(0.0, 1.12, 0.0, 9.6, 0.10, 9.6, 0.14f, 0.20f, 0.30f);
        kain_gl_draw_box(0.0, 0.82, 0.0, 1.1, 1.6 + compute_phase * 0.8, 1.1, profile->accent_a[0], profile->accent_a[1], profile->accent_a[2]);
        kain_gl_draw_box(0.0, 2.35 + pulse * 0.45, 0.0, 2.6 + throughput_scale, 0.16, 2.6 + throughput_scale, profile->accent_b[0], profile->accent_b[1], profile->accent_b[2]);
        kain_gl_draw_box(0.0, 1.85 + pulse * 0.18, 0.0, 1.4, 0.36, 1.4, 0.16f, 0.36f, 0.54f);

        for (relay_index = 0; relay_index < 6; ++relay_index) {
            double relay_angle = ((double)relay_index / 6.0) * M_PI * 2.0 + (compute_phase * 0.6);
            double relay_radius = 4.6 + ((relay_index % 2) * 1.6);
            double relay_x = cos(relay_angle) * relay_radius;
            double relay_z = sin(relay_angle) * relay_radius;
            double relay_height = 0.9 + ((relay_index % 3) * 0.45) + pulse * 0.22;
            kain_gl_draw_box(relay_x, 0.7 + relay_height * 0.5, relay_z, 0.58, relay_height, 0.58, 0.20f, 0.32f, 0.50f);
            kain_gl_draw_box(relay_x, 1.22 + relay_height, relay_z, 0.26, 0.12, 0.26, profile->accent_b[0], profile->accent_b[1], profile->accent_b[2]);
        }

        for (lane_index = 0; lane_index < 10; ++lane_index) {
            double lane_t = ((double)lane_index / 9.0) * 2.0 - 1.0;
            double lane_x = lane_t * dispatch_extent;
            double lane_height = 0.35 + (sin(app->total_time * 2.2 + lane_index * 0.8 + compute_phase * 4.0) * 0.22 + 0.22);
            kain_gl_draw_box(lane_x, 0.25 + lane_height, -3.6, 0.42, lane_height, 0.42, 0.18f, 0.50f, 0.68f);
            kain_gl_draw_box(lane_x, 0.15 + lane_height * 0.5, 3.6, 0.34, lane_height * 0.75, 0.34, 0.92f, 0.78f, 0.28f);
        }

        kain_gl_draw_orbit_ring(3.8 + throughput_scale, 1.32 + pulse * 0.12, profile->accent_a[0], profile->accent_a[1], profile->accent_a[2], 0.84f);
        kain_gl_draw_orbit_ring(5.6 + compute_phase * 1.4, 0.92, profile->accent_b[0], profile->accent_b[1], profile->accent_b[2], 0.46f);
        kain_gl_draw_energy_spokes(5.8 + throughput_scale * 1.2, 1.02, app->total_time, profile->accent_a);
        return;
    }

    if (kain_viewport_profile_is(profile, "retirement_demo")) {
        int plinth;
        kain_gl_draw_box(0.0, 0.4, 0.0, 9.0, 0.8, 9.0, 0.12f, 0.14f, 0.18f);
        kain_gl_draw_box(0.0, 1.35, 0.0, 2.0, 2.0, 2.0, profile->accent_a[0], profile->accent_a[1], profile->accent_a[2]);
        kain_gl_draw_box(-2.3, 0.72 + pulse * 0.18, 1.3, 0.9, 0.9, 0.9, profile->accent_b[0], profile->accent_b[1], profile->accent_b[2]);
        kain_gl_draw_box(2.2, 1.05, -1.4, 1.0, 2.2, 1.0, 0.28f, 0.34f, 0.42f);
        kain_gl_draw_box(2.2, 2.45, -1.4, 0.52, 0.16, 0.52, profile->accent_b[0], profile->accent_b[1], profile->accent_b[2]);

        for (plinth = 0; plinth < 8; ++plinth) {
            double angle = ((double)plinth / 8.0) * M_PI * 2.0 + 0.35;
            double radius = 4.7;
            double px = cos(angle) * radius;
            double pz = sin(angle) * radius;
            kain_gl_draw_box(px, 0.22, pz, 0.62, 0.44, 0.62, 0.16f, 0.18f, 0.22f);
        }

        kain_gl_draw_orbit_ring(4.4, 0.04, profile->accent_a[0], profile->accent_a[1], profile->accent_a[2], 0.36f);
        return;
    }

    if (kain_viewport_profile_is(profile, "kerr_black_hole")) {
        int shard;
        double singularity_scale = 1.25 + compute_phase * 0.18;
        kain_gl_draw_box(0.0, 0.2, 0.0, singularity_scale, singularity_scale, singularity_scale, 0.02f, 0.02f, 0.04f);
        kain_gl_draw_orbit_ring(3.2 + pulse * 0.6, 0.12, profile->accent_b[0], profile->accent_b[1], profile->accent_b[2], 0.92f);
        kain_gl_draw_orbit_ring(4.6 + pulse * 0.35, -0.04, profile->accent_a[0], profile->accent_a[1], profile->accent_a[2], 0.72f);
        kain_gl_draw_orbit_ring(2.2 + compute_phase * 0.55, 0.0, 0.92f, 0.95f, 1.0f, 0.26f);
        kain_gl_draw_energy_spokes(6.6, 0.08, app->total_time * 1.8, profile->accent_a);

        for (shard = 0; shard < 10; ++shard) {
            double angle = app->total_time * (0.18 + shard * 0.02) + shard * 0.62;
            double radius = 6.0 + ((shard % 3) * 1.4);
            double px = cos(angle) * radius;
            double pz = sin(angle) * radius;
            double py = sin(app->total_time * 0.9 + shard) * 0.6;
            double scale = 0.24 + ((shard % 4) * 0.10);
            kain_gl_draw_box(px, py, pz, scale, 0.08, scale * 2.4, 0.22f, 0.32f, 0.55f);
        }

        kain_gl_draw_box(0.0, 4.8 + pulse * 1.1, 0.0, 0.18, 3.8, 0.18, 0.44f, 0.72f, 1.00f);
        kain_gl_draw_box(0.0, -4.8 - pulse * 1.1, 0.0, 0.18, 3.8, 0.18, 0.58f, 0.84f, 1.00f);
        return;
    }

    if (kain_viewport_profile_is(profile, "magma_terraces")) {
        int tier;
        int vent;
        int shard;
        int bridge;

        for (tier = 0; tier < 5; ++tier) {
            double tier_scale = (double)tier;
            double terrace_size = 26.0 - (tier_scale * 4.2);
            double terrace_y = 0.9 + (tier_scale * 1.35);
            double terrace_bob = sin(app->total_time * (0.75 + tier_scale * 0.1) + tier_scale) * 0.08;
            float shell_mix = 0.26f + (float)(tier * 0.05f);
            float shell_r = shell_mix * profile->accent_b[0];
            float shell_g = shell_mix * profile->accent_b[1];
            float shell_b = shell_mix * profile->accent_b[2];
            float rim_r = 0.18f + (float)(0.12 * tier);
            float rim_g = 0.08f + (float)(0.04 * tier);
            float rim_b = 0.06f + (float)(0.03 * tier);

            kain_gl_draw_box(0.0, terrace_y + terrace_bob, 0.0, terrace_size, 0.65, terrace_size, shell_r, shell_g, shell_b);
            kain_gl_draw_box(0.0, terrace_y + 0.22 + terrace_bob, 0.0, terrace_size - 1.4, 0.16, terrace_size - 1.4, rim_r, rim_g, rim_b);
        }

        kain_gl_draw_box(0.0, 6.5 + pulse * 0.45, 0.0, 3.6, 0.45, 3.6, 0.95f, 0.32f, 0.14f);
        kain_gl_draw_box(0.0, 5.4 + pulse * 0.18, 0.0, 7.2, 0.30, 7.2, 0.32f, 0.08f, 0.05f);

        for (bridge = 0; bridge < 4; ++bridge) {
            double angle = (double)bridge * (M_PI * 0.5);
            double bridge_x = cos(angle) * 11.5;
            double bridge_z = sin(angle) * 11.5;
            double bridge_scale_x = (bridge % 2 == 0) ? 7.8 : 1.2;
            double bridge_scale_z = (bridge % 2 == 0) ? 1.2 : 7.8;
            kain_gl_draw_box(bridge_x, 4.35, bridge_z, bridge_scale_x, 0.22, bridge_scale_z, 0.28f, 0.18f, 0.14f);
            kain_gl_draw_box(bridge_x, 4.70, bridge_z, bridge_scale_x * 0.92, 0.08, bridge_scale_z * 0.92, profile->accent_b[0], profile->accent_b[1], profile->accent_b[2]);
        }

        for (vent = 0; vent < 12; ++vent) {
            double angle = ((double)vent / 12.0) * M_PI * 2.0;
            double ring = 10.0 + ((vent % 3) * 2.6);
            double vent_x = cos(angle) * ring;
            double vent_z = sin(angle) * ring;
            double vent_height = 2.8 + ((vent % 4) * 1.45);
            double vent_tip = 0.18 + sin(app->total_time * 1.8 + vent) * 0.12;
            float vent_r = 0.28f + (float)(0.07 * (vent % 3));
            float vent_g = 0.16f + (float)(0.03 * (vent % 2));
            float vent_b = 0.12f;

            kain_gl_draw_box(vent_x, 1.2 + vent_height * 0.5, vent_z, 1.3, vent_height, 1.3, vent_r, vent_g, vent_b);
            kain_gl_draw_box(vent_x, 1.45 + vent_height + vent_tip, vent_z, 0.55, 0.35, 0.55, profile->accent_a[0], profile->accent_a[1], profile->accent_a[2]);
        }

        for (shard = 0; shard < 7; ++shard) {
            double angle = app->total_time * (0.38 + shard * 0.06) + shard;
            double ring = 7.5 + shard * 1.55;
            double shard_x = cos(angle) * ring;
            double shard_z = sin(angle * 0.92) * ring;
            double shard_y = 8.5 + sin(app->total_time * 1.2 + shard * 0.7) * 1.8 + shard * 0.28;
            double shard_scale = 0.6 + ((shard % 3) * 0.22);

            kain_gl_draw_box(shard_x, shard_y, shard_z, shard_scale, 0.18, shard_scale * 1.7, 0.36f, 0.24f, 0.18f);
            kain_gl_draw_box(shard_x, shard_y + 0.24, shard_z, shard_scale * 0.4, 0.08, shard_scale * 0.9, profile->accent_b[0], profile->accent_b[1], profile->accent_b[2]);
        }

        return;
    }

    kain_gl_draw_box(0.0, 0.5, 0.0, 10.0, 1.0, 10.0, 0.12f, 0.14f, 0.18f);
    kain_gl_draw_box(0.0, 2.5 + pulse * 0.4, 0.0, 1.4, 5.0, 1.4, profile->accent_a[0], profile->accent_a[1], profile->accent_a[2]);
    kain_gl_draw_box(0.0, 5.3 + pulse * 0.7, 0.0, 3.8, 0.3, 3.8, 0.22f, 0.24f, 0.32f);

    for (i = 0; i < 8; ++i) {
        double angle = ((double)i / 8.0) * M_PI * 2.0;
        double radius = 12.0;
        double tower_x = cos(angle) * radius;
        double tower_z = sin(angle) * radius;
        double tower_height = 4.0 + ((i % 3) * 1.6);
        float tint = 0.45f + (float)(0.05 * (i % 4));
        kain_gl_draw_box(tower_x, tower_height * 0.5, tower_z, 1.4, tower_height, 1.4, tint * profile->accent_b[0], tint * profile->accent_b[1], tint * profile->accent_b[2]);
        kain_gl_draw_box(tower_x * 0.66, 0.65, tower_z * 0.66, 1.2, 1.3, 1.2, 0.18f, 0.20f, 0.24f);
    }

    for (i = 0; i < 6; ++i) {
        double lane = -18.0 + (i * 7.0);
        double bob = sin(app->total_time * 1.2 + i) * 0.25;
        kain_gl_draw_box(lane, 1.6 + bob, -14.0, 1.0, 3.2, 1.0, 0.26f, 0.28f, 0.34f);
        kain_gl_draw_box(lane, 1.4 - bob, 14.0, 1.0, 2.8, 1.0, 0.18f, 0.24f, 0.30f);
    }
}

static void kain_gl_render_particles(const KainNativeViewportApp* app) {
    int i;
    const KainViewportProfile* profile = app->settings.profile;
    double compute_phase = app->compute_execution.executed ? app->compute_execution.phase : 0.0;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPointSize(3.5f);
    glBegin(GL_POINTS);
    for (i = 0; i < app->settings.particle_count; ++i) {
        double phase = ((double)i / (double)app->settings.particle_count) * M_PI * 2.0;
        double ring;
        double spin;
        double px;
        double py;
        double pz;
        float glow;

        if (kain_viewport_profile_is(profile, "tensor_stream_probe")) {
            ring = 2.8 + fmod((double)i * 0.21, 4.8) + compute_phase * 1.6;
            spin = app->total_time * (0.9 + ((i % 5) * 0.12));
            px = cos(phase + spin) * ring;
            py = 0.8 + sin(app->total_time * 1.5 + i * 0.12 + compute_phase * 6.0) * 0.8;
            pz = sin(phase - spin * 0.82) * (ring + 1.2);
            glow = 0.52f + (float)(0.40 * sin(app->total_time * 2.8 + i + compute_phase * 8.0));
        } else if (kain_viewport_profile_is(profile, "kerr_black_hole")) {
            ring = 3.0 + fmod((double)i * 0.18, 10.0);
            spin = app->total_time * (1.2 + ((i % 9) * 0.05));
            px = cos(phase + spin) * ring;
            py = sin(app->total_time * 0.7 + i * 0.3) * (0.35 + ((i % 4) * 0.18));
            pz = sin(phase + spin * 1.18) * ring;
            glow = 0.42f + (float)(0.52 * sin(app->total_time * 3.1 + i));
        } else if (kain_viewport_profile_is(profile, "retirement_demo")) {
            ring = 3.4 + fmod((double)i * 0.29, 5.2);
            spin = app->total_time * (0.42 + ((i % 6) * 0.06));
            px = cos(phase + spin) * ring;
            py = 1.2 + fmod((double)i * 0.08 + app->total_time * 0.8, 3.2);
            pz = sin(phase - spin * 0.76) * ring;
            glow = 0.34f + (float)(0.32 * sin(app->total_time * 1.8 + i));
        } else {
            ring = 5.0 + fmod((double)i * 0.37, 8.0);
            spin = app->total_time * (0.5 + ((i % 7) * 0.08));
            px = cos(phase + spin) * ring;
            py = 1.4 + fmod((double)i * 0.11 + app->total_time * 1.3, 5.0);
            pz = sin(phase - spin * 0.9) * ring;
            glow = 0.45f + (float)(0.45 * sin(app->total_time * 2.1 + i));
        }

        if ((i & 1) == 0) {
            glColor4f(profile->accent_a[0], profile->accent_a[1], profile->accent_a[2], glow);
        } else {
            glColor4f(profile->accent_b[0], profile->accent_b[1], profile->accent_b[2], glow);
        }
        glVertex3f((GLfloat)px, (GLfloat)py, (GLfloat)pz);
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void kain_gl_render_overlay(KainNativeViewportApp* app) {
    char subtitle_line[256];
    char stats_line[256];
    char asset_line[256];
    char realtime_line[256];
    char compute_line[256];
    char config_line[256];
    char contract_line[256];
    char validation_line[256];
    const char* live_lines[7];
    const char* help_lines[1];
    KainUiCompiledOverlaySpec overlay_spec;
    const KainViewportProfile* profile = app->settings.profile;
    snprintf(stats_line, sizeof(stats_line), "fps %.1f  |  pos %.1f %.1f %.1f", app->frame_fps, app->camera_x, app->camera_y, app->camera_z);
    snprintf(
        config_line,
        sizeof(config_line),
        "profile %s  |  move %.1f  |  particles %d  |  world scale %.2f",
        profile->id,
        app->settings.move_speed,
        app->settings.particle_count,
        app->world_asset.loaded ? app->world_asset.world_scale : 1.0
    );
    if (app->world_asset.loaded) {
        snprintf(
            asset_line,
            sizeof(asset_line),
            "%s  |  %llu tris  |  %llu meshes",
            app->world_asset.asset_label,
            app->world_asset.triangle_count,
            app->world_asset.mesh_count
        );
        snprintf(subtitle_line, sizeof(subtitle_line), "%s  |  raw native GLB world lane", profile->label);
    } else {
        snprintf(asset_line, sizeof(asset_line), "fallback procedural world  |  no GLB loaded");
        snprintf(subtitle_line, sizeof(subtitle_line), "%s  |  GPU-backed OpenGL lane", profile->label);
    }
    if (app->runtime_contract.loaded) {
        snprintf(
            contract_line,
            sizeof(contract_line),
            "contract %s via %s  |  core %d/3  |  extras %d/3  |  items %d",
            app->runtime_contract.target[0] ? app->runtime_contract.target : "unknown",
            app->runtime_contract.load_origin[0] ? app->runtime_contract.load_origin : "path",
            app->runtime_contract.core_service_count,
            app->runtime_contract.optional_service_count,
            app->runtime_contract.item_count
        );
    } else {
        snprintf(
            contract_line,
            sizeof(contract_line),
            "contract missing  |  expected %s beside the executable or via %s",
            KAIN_RUNTIME_CONTRACT_SIDECAR_SUFFIX,
            KAIN_RUNTIME_CONTRACT_ENV
        );
    }
    if (app->contract_validation.warning_count > 0) {
        snprintf(
            validation_line,
            sizeof(validation_line),
            "validation degraded  |  %s",
            app->contract_validation.warnings[0]
        );
    } else {
        char service_line[160];
        kain_runtime_contract_format_service_mask(
            app->contract_validation.available_service_mask,
            service_line,
            sizeof(service_line)
        );
        snprintf(
            validation_line,
            sizeof(validation_line),
            "validation %s  |  services %s",
            app->contract_validation.strict_mode ? "strict" : "compat",
            service_line
        );
    }
    if (app->realtime_bundle.loaded) {
        snprintf(
            realtime_line,
            sizeof(realtime_line),
            "realtime %s via %s  |  viewport %s  |  materials %d  |  shader refs %d",
            app->realtime_bundle.target[0] ? app->realtime_bundle.target : "unknown",
            app->realtime_bundle.load_origin[0] ? app->realtime_bundle.load_origin : "path",
            app->realtime_bundle.primary_scene[0] ? app->realtime_bundle.primary_scene : "none",
            app->realtime_bundle.material_count,
            app->realtime_bundle.shader_ref_count
        );
    } else {
        snprintf(
            realtime_line,
            sizeof(realtime_line),
            "realtime missing  |  expected %s beside the executable or via %s",
            KAIN_RUNTIME_REALTIME_SIDECAR_SUFFIX,
            KAIN_RUNTIME_REALTIME_ENV
        );
    }
    if (app->compute_execution.executed) {
        snprintf(
            compute_line,
            sizeof(compute_line),
            "compute %s  |  invocations %llu  |  phase %.2f",
            app->graphics_bundle.primary_compute.execution_domain[0]
                ? app->graphics_bundle.primary_compute.execution_domain
                : "compute",
            app->compute_execution.dispatch_invocations,
            app->compute_execution.phase
        );
    } else if (app->graphics_bundle.loaded && app->graphics_validation.reason[0]) {
        snprintf(
            compute_line,
            sizeof(compute_line),
            "compute idle  |  %s",
            app->graphics_validation.reason
        );
    } else {
        snprintf(
            compute_line,
            sizeof(compute_line),
            "compute missing  |  runtime bundle has no executable primary_compute lane"
        );
    }
    live_lines[0] = stats_line;
    live_lines[1] = config_line;
    live_lines[2] = asset_line;
    live_lines[3] = realtime_line;
    live_lines[4] = compute_line;
    live_lines[5] = contract_line;
    live_lines[6] = validation_line;
    help_lines[0] = "WASD move  |  Space jump  |  Shift sprint  |  Click capture mouse  |  Esc release";

    ZeroMemory(&overlay_spec, sizeof(overlay_spec));
    overlay_spec.profile = profile;
    overlay_spec.x = 18.0f;
    overlay_spec.y = 18.0f;
    overlay_spec.width = 420.0f;
    overlay_spec.panel_alpha = 0.78f;
    overlay_spec.show_help = app->settings.show_help;
    overlay_spec.draw_crosshair = 1;
    overlay_spec.fallback_title = "KAIN RAW NATIVE VIEWPORT";
    overlay_spec.fallback_subtitle = subtitle_line;
    overlay_spec.live_lines = live_lines;
    overlay_spec.live_line_count = 7;
    overlay_spec.help_lines = help_lines;
    overlay_spec.help_line_count = 1;
    overlay_spec.fallback_hint = app->contract_validation.warning_count > 0
        ? app->contract_validation.warnings[0]
        : (app->runtime_contract.loaded
            ? (app->world_asset.loaded
                ? "Runtime contract validated. City world is env-driven through KAIN_NATIVE_WORLD_ASSET."
                : "Runtime contract validated. Use KAIN_NATIVE_SCENE_PROFILE to switch starforge / emberfall / luminous_port / magma_terraces / tensor_stream_probe / retirement_demo / kerr_black_hole.")
            : "No runtime contract was loaded. Keep the *.runtime_contract.json sidecar beside the exe for native-lane validation.");
    kain_ui_compiled_overlay_render(&app->surface, app->width, app->height, &app->compiled_ui, &overlay_spec);
}

static void kain_gl_render_frame(KainNativeViewportApp* app) {
    const KainViewportProfile* profile = app->settings.profile;
    GLfloat fog_color[4];
    double compute_phase = app->compute_execution.executed ? app->compute_execution.phase : 0.0;
    double effective_far_clip = app->camera_far_clip + (compute_phase * 18.0);
    double effective_fog_density = app->settings.fog_density * (1.0 + compute_phase * 0.25);
    glViewport(0, 0, app->width, app->height);
    glClearColor(profile->clear_color[0], profile->clear_color[1], profile->clear_color[2], profile->clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_FOG);
    fog_color[0] = profile->fog_color[0];
    fog_color[1] = profile->fog_color[1];
    fog_color[2] = profile->fog_color[2];
    fog_color[3] = profile->fog_color[3];
    glFogfv(GL_FOG_COLOR, fog_color);
    glFogf(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, (GLfloat)effective_fog_density);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, profile->ambient_light);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, profile->diffuse_light);
    glLightfv(GL_LIGHT0, GL_POSITION, profile->light_position);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    kain_gl_perspective(
        app->camera_fov_y_degrees,
        (double)app->width / (double)app->height,
        app->camera_near_clip,
        effective_far_clip
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotated(-app->pitch * 57.295779513, 1.0, 0.0, 0.0);
    glRotated(-app->yaw * 57.295779513, 0.0, 1.0, 0.0);
    glTranslated(-app->camera_x, -app->camera_y, -app->camera_z);

    kain_gl_render_ground(app);
    kain_gl_render_scene_geometry(app);
    kain_gl_render_particles(app);
    kain_gl_render_overlay(app);
}

static void kain_native_viewport_basis(double yaw, double* forward_x, double* forward_z, double* right_x, double* right_z) {
    double fx = -sin(yaw);
    double fz = -cos(yaw);
    double rx = cos(yaw);
    double rz = -sin(yaw);

    if (forward_x) *forward_x = fx;
    if (forward_z) *forward_z = fz;
    if (right_x) *right_x = rx;
    if (right_z) *right_z = rz;
}

static void kain_native_update_camera(KainNativeViewportApp* app, double dt) {
    double move_x = 0.0;
    double move_z = 0.0;
    double forward_x = 0.0;
    double forward_z = 0.0;
    double right_x = 0.0;
    double right_z = 0.0;
    double length;
    double move_speed;
    int space_down;

    if (app->mouse_capture.pointer_locked) {
        int delta_x = 0;
        int delta_y = 0;
        if (kain_win32_mouse_capture_sample_relative(&app->mouse_capture, &delta_x, &delta_y)) {
            app->yaw -= delta_x * app->settings.mouse_sensitivity;
            app->pitch -= delta_y * app->settings.mouse_sensitivity;
        }
        app->pitch = kain_clampd(app->pitch, -1.25, 1.25);
    } else {
        if (app->keys[VK_LEFT]) app->yaw += dt * 1.6;
        if (app->keys[VK_RIGHT]) app->yaw -= dt * 1.6;
        if (app->keys[VK_UP]) app->pitch += dt * 1.2;
        if (app->keys[VK_DOWN]) app->pitch -= dt * 1.2;
        app->pitch = kain_clampd(app->pitch, -1.25, 1.25);
    }

    kain_native_viewport_basis(app->yaw, &forward_x, &forward_z, &right_x, &right_z);

    if (app->keys['W']) {
        move_x += forward_x;
        move_z += forward_z;
    }
    if (app->keys['S']) {
        move_x -= forward_x;
        move_z -= forward_z;
    }
    if (app->keys['A']) {
        move_x -= right_x;
        move_z -= right_z;
    }
    if (app->keys['D']) {
        move_x += right_x;
        move_z += right_z;
    }

    length = sqrt((move_x * move_x) + (move_z * move_z));
    if (length > 0.0001) {
        move_x /= length;
        move_z /= length;
    }

    move_speed = app->settings.move_speed;
    if (app->keys[VK_SHIFT]) {
        move_speed *= app->settings.sprint_multiplier;
    }
    app->camera_x += move_x * move_speed * dt;
    app->camera_z += move_z * move_speed * dt;

    space_down = app->keys[VK_SPACE] != 0;
    if (space_down && !app->previous_space_down && app->grounded) {
        app->velocity_y = app->settings.jump_velocity;
        app->grounded = 0;
    }
    app->previous_space_down = space_down;

    app->velocity_y -= app->settings.gravity * dt;
    app->camera_y += app->velocity_y * dt;
    if (app->camera_y <= app->world_ground_y + app->settings.eye_height) {
        app->camera_y = app->world_ground_y + app->settings.eye_height;
        app->velocity_y = 0.0;
        app->grounded = 1;
    }
}

static int kain_native_init_gl(KainNativeViewportApp* app) {
    return kain_win32_gl_surface_boot(app->hwnd, &app->surface, 16);
}

static void kain_native_shutdown_gl(KainNativeViewportApp* app) {
    if (!app) return;
    kain_win32_gl_surface_shutdown(app->hwnd, &app->surface);
}

static int kain_native_viewport_host_init(KainWin32AppHost* host, void* user_data) {
    KainNativeViewportApp* app = (KainNativeViewportApp*)user_data;
    double to_center_x;
    double to_center_z;
    double world_height;
    if (!host || !app) return 0;
    app->hwnd = host->hwnd;
    app->width = host->width;
    app->height = host->height;
    kain_win32_mouse_capture_bind(&app->mouse_capture, app->hwnd);
    if (!kain_native_init_gl(app)) {
        return 0;
    }
    if ((app->contract_validation.downgraded_optional_mask & KAIN_RUNTIME_SERVICE_NATIVE_ASSET_GLTF) == 0u &&
        kain_native_scene_asset_load_from_env(KAIN_NATIVE_WORLD_ASSET_ENV, &app->world_asset)) {
        app->world_ground_y = app->world_asset.ground_height;
        app->camera_far_clip = app->world_asset.recommended_far_clip;
        world_height = app->world_asset.world_bounds_max.y - app->world_asset.world_bounds_min.y;
        app->camera_x = app->world_asset.world_bounds_min.x - (app->world_asset.recommended_spawn_distance * 0.45);
        app->camera_z = app->world_asset.world_bounds_max.z + app->world_asset.recommended_spawn_distance;
        app->camera_y = app->world_ground_y + app->settings.eye_height + kain_clampd(world_height * 0.12, 0.6, 8.0);
        to_center_x = app->world_asset.world_center.x - app->camera_x;
        to_center_z = app->world_asset.world_center.z - app->camera_z;
        app->yaw = atan2(to_center_x, -to_center_z);
        app->pitch = -0.08;
    }
    kain_native_viewport_apply_realtime_camera(app);
    if (app->settings.capture_mouse_on_launch) {
        kain_native_capture_mouse(app, 1);
    }
    return 1;
}

static void kain_native_viewport_host_frame(KainWin32AppHost* host, void* user_data, double frame_delta) {
    KainNativeViewportApp* app = (KainNativeViewportApp*)user_data;
    if (!host || !app) return;
    app->frame_delta = frame_delta;
    app->frame_fps = host->frame_fps;
    app->total_time += frame_delta;
    if (app->gpu_runtime_handle && app->gpu_runtime_dispatch && app->graphics_bundle.shader_compute_ref_count > 0) {
        KainGpuRuntimeDispatchRequest request;
        KainGpuRuntimeDispatchResult result;
        request.shader_bundle_path = app->shader_bundle_path;
        request.compute_residency_path = app->compute_residency_path;
        request.compute_key = app->graphics_bundle.primary_compute.shader_key;
        ZeroMemory(&result, sizeof(result));
        if (app->gpu_runtime_dispatch(app->gpu_runtime_handle, &request, &result) == 0) {
            app->compute_execution.executed = 1;
            app->compute_execution.dispatch_invocations = result.dispatch_invocations;
            app->compute_execution.accumulated_invocations += result.dispatch_invocations;
            app->compute_execution.tensor_binding_count = (int)result.tensor_binding_count;
            app->compute_execution.stream_binding_count = (int)result.stream_binding_count;
            app->compute_execution.neural_node_count = (int)result.neural_node_count;
            app->compute_execution.phase = fmod(app->total_time, 1.0);
            app->compute_execution.throughput =
                frame_delta > 0.0001 ? ((double)result.dispatch_invocations / frame_delta) : 0.0;
            strncpy_s(
                app->compute_execution.summary,
                sizeof(app->compute_execution.summary),
                result.message,
                _TRUNCATE
            );
        } else {
            fprintf(stderr, "[KAIN][raw-native-viewport] compute dispatch failed: %s\n", result.message);
            app->compute_execution.executed = 0;
            strncpy_s(
                app->compute_execution.summary,
                sizeof(app->compute_execution.summary),
                result.message,
                _TRUNCATE
            );
            app->running = 0;
            if (app->hwnd) {
                PostMessageA(app->hwnd, WM_CLOSE, 0, 0);
            }
        }
    }
    kain_native_update_camera(app, frame_delta);
    kain_gl_render_frame(app);
    kain_win32_gl_surface_present(&app->surface);
}

static void kain_native_viewport_host_shutdown(KainWin32AppHost* host, void* user_data) {
    KainNativeViewportApp* app = (KainNativeViewportApp*)user_data;
    (void)host;
    if (!app) return;
    kain_native_capture_mouse(app, 0);
    if (app->gpu_runtime_destroy && app->gpu_runtime_handle) {
        app->gpu_runtime_destroy(app->gpu_runtime_handle);
        app->gpu_runtime_handle = NULL;
    }
    if (app->gpu_runtime_library) {
        FreeLibrary(app->gpu_runtime_library);
        app->gpu_runtime_library = NULL;
    }
    kain_native_scene_asset_shutdown(&app->world_asset);
    kain_native_shutdown_gl(app);
}

static LRESULT kain_native_viewport_host_message(
    KainWin32AppHost* host,
    void* user_data,
    HWND hwnd,
    UINT msg,
    WPARAM w_param,
    LPARAM l_param,
    int* handled
) {
    KainNativeViewportApp* app = (KainNativeViewportApp*)user_data;
    (void)host;
    if (!app || !handled) {
        return 0;
    }

    switch (msg) {
        case WM_SIZE:
            app->width = LOWORD(l_param) > 0 ? LOWORD(l_param) : app->settings.window_width;
            app->height = HIWORD(l_param) > 0 ? HIWORD(l_param) : app->settings.window_height;
            *handled = 1;
            return 0;
        case WM_SETFOCUS:
            if (app->settings.capture_mouse_on_launch) {
                kain_native_capture_mouse(app, 1);
            }
            *handled = 1;
            return 0;
        case WM_KILLFOCUS:
            kain_native_capture_mouse(app, 0);
            *handled = 1;
            return 0;
        case WM_LBUTTONDOWN:
            app->hwnd = hwnd;
            kain_win32_mouse_capture_bind(&app->mouse_capture, hwnd);
            kain_native_capture_mouse(app, 1);
            *handled = 1;
            return 0;
        case WM_KEYDOWN:
            if (w_param < 256) {
                app->keys[w_param] = 1;
                if (w_param == VK_ESCAPE) {
                    kain_native_capture_mouse(app, 0);
                }
                if (w_param == VK_F1) {
                    app->settings.show_help = !app->settings.show_help;
                }
            }
            *handled = 1;
            return 0;
        case WM_KEYUP:
            if (w_param < 256) {
                app->keys[w_param] = 0;
            }
            *handled = 1;
            return 0;
        case WM_DESTROY:
            app->running = 0;
            kain_win32_mouse_capture_release_all(&app->mouse_capture);
            break;
    }
    return 0;
}

static void kain_run_native_viewport(double x, double y, const char* window_title) {
    KainNativeViewportApp app;
    KainWin32AppConfig config;
    const char* resolved_window_title = window_title;

    ZeroMemory(&app, sizeof(app));
    ZeroMemory(&config, sizeof(config));
    app.settings = kain_load_viewport_settings();
    kain_native_viewport_try_load_runtime_contract(&app);
    if (!kain_native_viewport_validate_runtime_contract(&app)) {
        return;
    }
    kain_native_viewport_try_load_realtime_bundle(&app);
    kain_native_viewport_try_load_graphics_bundle(&app);
    if (!kain_native_viewport_prepare_compute_executor(&app)) {
        MessageBoxA(
            NULL,
            app.graphics_validation.reason[0]
                ? app.graphics_validation.reason
                : "Raw native compute executor initialization failed.",
            "Kain Compute Runtime Error",
            MB_OK | MB_ICONERROR
        );
        if (app.gpu_runtime_handle && app.gpu_runtime_destroy) {
            app.gpu_runtime_destroy(app.gpu_runtime_handle);
            app.gpu_runtime_handle = NULL;
        }
        if (app.gpu_runtime_library) {
            FreeLibrary(app.gpu_runtime_library);
            app.gpu_runtime_library = NULL;
        }
        return;
    }
    kain_native_viewport_try_load_compiled_ui(&app);
    kain_native_scene_asset_init(&app.world_asset);
    app.width = app.settings.window_width;
    app.height = app.settings.window_height;
    app.running = 1;
    app.camera_x = x;
    app.world_ground_y = 0.0;
    app.camera_y = app.world_ground_y + app.settings.eye_height;
    app.camera_z = 24.0 + y;
    app.yaw = 0.0;
    app.pitch = -0.14;
    app.camera_near_clip = 0.1;
    app.camera_far_clip = 120.0;
    app.camera_fov_y_degrees = 72.0;
    app.grounded = 1;
    if (app.compiled_ui.loaded && app.compiled_ui.window_title[0]) {
        resolved_window_title = app.compiled_ui.window_title;
    }
    config.class_name = "KainNativeViewportWindowClass";
    config.window_title = resolved_window_title;
    config.default_width = app.width;
    config.default_height = app.height;
    config.sleep_millis = 1;
    config.min_frame_delta = 0.001;
    config.max_frame_delta = 0.050;
    config.on_init = kain_native_viewport_host_init;
    config.on_frame = kain_native_viewport_host_frame;
    config.on_shutdown = kain_native_viewport_host_shutdown;
    config.on_message = kain_native_viewport_host_message;

    kain_win32_app_run(&app.host, &config, &app);
}
#endif

void spawn_native_viewport(double x, double y) {
#ifdef _WIN32
    kain_run_native_viewport(x, y, "KAIN Native Viewport");
#else
    printf("[KAIN] spawn_native_viewport is currently only implemented on Windows. Requested at { x: %.2f, y: %.2f }\n", x, y);
#endif
}

void spawn_cube(double x, double y) {
#ifdef _WIN32
    kain_run_native_viewport(x, y, "KAIN Cube");
#else
    printf("[KAIN] spawn_cube is currently only implemented on Windows. Requested at { x: %.2f, y: %.2f }\n", x, y);
#endif
}
