#include "../../../include/kain_runtime_win32.h"
#include "../../../include/kain_runtime_asset.h"
#include "../../../include/kain_runtime_contract.h"
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
    double camera_far_clip;
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
            app->settings.profile = kain_find_viewport_profile(app->compiled_ui.primary_viewport_scene);
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
            app->settings.profile = profile;
        }
    }
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
    kain_env_free(compiled_ui_path);
    kain_env_free(world_asset_path);
    return mask;
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
    settings.window_width = kain_env_int("KAIN_NATIVE_WINDOW_WIDTH", profile->default_width);
    settings.window_height = kain_env_int("KAIN_NATIVE_WINDOW_HEIGHT", profile->default_height);
    settings.show_help = kain_env_flag("KAIN_NATIVE_SHOW_HELP", 1);
    settings.capture_mouse_on_launch = kain_env_flag("KAIN_NATIVE_CAPTURE_MOUSE", 1);
    settings.particle_count = kain_env_int("KAIN_NATIVE_PARTICLE_COUNT", profile->particle_count);
    settings.move_speed = kain_env_double("KAIN_NATIVE_MOVE_SPEED", profile->move_speed);
    settings.sprint_multiplier = kain_env_double("KAIN_NATIVE_SPRINT_MULTIPLIER", profile->sprint_multiplier);
    settings.jump_velocity = kain_env_double("KAIN_NATIVE_JUMP_VELOCITY", profile->jump_velocity);
    settings.gravity = kain_env_double("KAIN_NATIVE_GRAVITY", profile->gravity);
    settings.mouse_sensitivity = kain_env_double("KAIN_NATIVE_MOUSE_SENSITIVITY", profile->mouse_sensitivity);
    settings.eye_height = kain_env_double("KAIN_NATIVE_EYE_HEIGHT", profile->eye_height);
    settings.fog_density = kain_env_double("KAIN_NATIVE_FOG_DENSITY", profile->fog_density);
    if (settings.window_width < 640) settings.window_width = 640;
    if (settings.window_height < 360) settings.window_height = 360;
    if (settings.particle_count < 24) settings.particle_count = 24;
    return settings;
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

    if (app->world_asset.loaded) {
        kain_native_scene_asset_render(&app->world_asset);
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
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPointSize(3.5f);
    glBegin(GL_POINTS);
    for (i = 0; i < app->settings.particle_count; ++i) {
        double phase = ((double)i / (double)app->settings.particle_count) * M_PI * 2.0;
        double ring = 5.0 + fmod((double)i * 0.37, 8.0);
        double spin = app->total_time * (0.5 + ((i % 7) * 0.08));
        double px = cos(phase + spin) * ring;
        double py = 1.4 + fmod((double)i * 0.11 + app->total_time * 1.3, 5.0);
        double pz = sin(phase - spin * 0.9) * ring;
        float glow = 0.45f + (float)(0.45 * sin(app->total_time * 2.1 + i));
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
    char config_line[256];
    char contract_line[256];
    char validation_line[256];
    const char* live_lines[6];
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
            "contract %s via %s  |  core %d/3  |  extras %d/2  |  items %d",
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
    live_lines[0] = stats_line;
    live_lines[1] = config_line;
    live_lines[2] = asset_line;
    live_lines[3] = realtime_line;
    live_lines[4] = contract_line;
    live_lines[5] = validation_line;
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
    overlay_spec.live_line_count = 6;
    overlay_spec.help_lines = help_lines;
    overlay_spec.help_line_count = 1;
    overlay_spec.fallback_hint = app->contract_validation.warning_count > 0
        ? app->contract_validation.warnings[0]
        : (app->runtime_contract.loaded
            ? (app->world_asset.loaded
                ? "Runtime contract validated. City world is env-driven through KAIN_NATIVE_WORLD_ASSET."
                : "Runtime contract validated. Use KAIN_NATIVE_SCENE_PROFILE to switch starforge / emberfall / luminous_port.")
            : "No runtime contract was loaded. Keep the *.runtime_contract.json sidecar beside the exe for native-lane validation.");
    kain_ui_compiled_overlay_render(&app->surface, app->width, app->height, &app->compiled_ui, &overlay_spec);
}

static void kain_gl_render_frame(KainNativeViewportApp* app) {
    const KainViewportProfile* profile = app->settings.profile;
    GLfloat fog_color[4];
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
    glFogf(GL_FOG_DENSITY, (GLfloat)app->settings.fog_density);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, profile->ambient_light);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, profile->diffuse_light);
    glLightfv(GL_LIGHT0, GL_POSITION, profile->light_position);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    kain_gl_perspective(72.0, (double)app->width / (double)app->height, 0.1, app->camera_far_clip);

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
    kain_native_update_camera(app, frame_delta);
    kain_gl_render_frame(app);
    kain_win32_gl_surface_present(&app->surface);
}

static void kain_native_viewport_host_shutdown(KainWin32AppHost* host, void* user_data) {
    KainNativeViewportApp* app = (KainNativeViewportApp*)user_data;
    (void)host;
    if (!app) return;
    kain_native_capture_mouse(app, 0);
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
    app.camera_far_clip = 120.0;
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
