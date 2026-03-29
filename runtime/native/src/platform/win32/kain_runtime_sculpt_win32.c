#include "../../../include/kain_runtime_win32.h"
#include "../../../include/kain_runtime_contract.h"
#include "../../../include/kain_runtime_realtime.h"
#include "../../../include/kain_runtime_ui.h"

#ifdef _WIN32
#define KAIN_SCULPT_MAX_RESOLUTION 96

typedef struct {
    const KainViewportProfile* profile;
    int window_width;
    int window_height;
    int show_help;
    int show_wireframe;
    int show_particles;
    int particle_count;
    int grid_resolution;
    double world_extent;
    double brush_radius;
    double brush_strength;
    double brush_falloff;
    double orbit_yaw;
    double orbit_pitch;
    double orbit_distance;
} KainNativeSculptSettings;

typedef struct {
    KainWin32AppHost host;
    KainWin32GlSurface surface;
    KainWin32MouseCapture mouse_capture;
    KainRuntimeContractBundle runtime_contract;
    KainRuntimeContractValidation contract_validation;
    KainRuntimeRealtimeBundle realtime_bundle;
    KainUiCompiledBundle compiled_ui;
    HWND hwnd;
    int width;
    int height;
    int running;
    int left_down;
    int right_down;
    int mouse_x;
    int mouse_y;
    int last_mouse_x;
    int last_mouse_y;
    int hover_valid;
    double hover_x;
    double hover_z;
    double total_time;
    double frame_delta;
    double frame_fps;
    double fps_accumulator;
    int fps_frames;
    LARGE_INTEGER perf_freq;
    LARGE_INTEGER prev_counter;
    KainNativeSculptSettings settings;
    double* heights;
} KainNativeSculptApp;

static const KainUiCompiledNode* kain_native_sculpt_find_root_node(const KainUiCompiledBundle* bundle) {
    int index;

    if (!bundle || !bundle->loaded) {
        return NULL;
    }

    if (bundle->has_root_id) {
        for (index = 0; index < bundle->node_count; ++index) {
            if (bundle->nodes[index].id == bundle->root_id) {
                return &bundle->nodes[index];
            }
        }
    }

    for (index = 0; index < bundle->node_count; ++index) {
        if (!bundle->nodes[index].has_parent) {
            return &bundle->nodes[index];
        }
    }

    return bundle->node_count > 0 ? &bundle->nodes[0] : NULL;
}

static const KainUiCompiledNode* kain_native_sculpt_find_primary_viewport_node(const KainUiCompiledBundle* bundle) {
    const KainUiCompiledNode* root_node = kain_native_sculpt_find_root_node(bundle);
    const KainUiCompiledNode* viewport_node;

    if (root_node && (root_node->kind == KAIN_UI_COMPILED_NODE_VIEWPORT3D || root_node->kind == KAIN_UI_COMPILED_NODE_VIEWPORT2D)) {
        return root_node;
    }

    viewport_node = kain_ui_compiled_bundle_find_first_kind(bundle, KAIN_UI_COMPILED_NODE_VIEWPORT3D);
    if (viewport_node) {
        return viewport_node;
    }

    return kain_ui_compiled_bundle_find_first_kind(bundle, KAIN_UI_COMPILED_NODE_VIEWPORT2D);
}

static const char* kain_native_sculpt_resolve_compiled_scene(const KainUiCompiledBundle* bundle) {
    const KainUiCompiledNode* viewport_node = kain_native_sculpt_find_primary_viewport_node(bundle);
    const KainUiCompiledNode* root_node = kain_native_sculpt_find_root_node(bundle);

    if (viewport_node && viewport_node->scene[0]) {
        return viewport_node->scene;
    }
    if (root_node && root_node->scene[0]) {
        return root_node->scene;
    }

    return NULL;
}

static void kain_native_sculpt_try_load_compiled_ui(KainNativeSculptApp* app) {
    if (!app) {
        return;
    }

    if ((app->contract_validation.downgraded_optional_mask & KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED) != 0u) {
        kain_ui_compiled_bundle_init(&app->compiled_ui);
        return;
    }

    if (kain_ui_compiled_bundle_load_from_env(KAIN_UI_COMPILED_BUNDLE_ENV, &app->compiled_ui)) {
        const char* viewport_scene = kain_native_sculpt_resolve_compiled_scene(&app->compiled_ui);
        if (!app->realtime_bundle.loaded && viewport_scene && viewport_scene[0]) {
            app->settings.profile = kain_find_viewport_profile(viewport_scene);
        }
    } else {
        kain_ui_compiled_bundle_init(&app->compiled_ui);
    }
}

static void kain_native_sculpt_try_load_runtime_contract(KainNativeSculptApp* app) {
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

static void kain_native_sculpt_try_load_realtime_bundle(KainNativeSculptApp* app) {
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

static unsigned int kain_native_sculpt_optional_service_mask(void) {
    unsigned int mask = 0u;
    char* compiled_ui_path = kain_env_dup(KAIN_UI_COMPILED_BUNDLE_ENV);
    if (compiled_ui_path && compiled_ui_path[0]) {
        mask |= KAIN_RUNTIME_SERVICE_NATIVE_UI_COMPILED;
    }
    kain_env_free(compiled_ui_path);
    return mask;
}

static void kain_native_sculpt_emit_contract_diagnostics(
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

static int kain_native_sculpt_validate_runtime_contract(KainNativeSculptApp* app) {
    unsigned int optional_service_mask;
    if (!app) {
        return 0;
    }
    optional_service_mask = kain_native_sculpt_optional_service_mask();
    if (!kain_runtime_contract_validate_startup(
            &app->runtime_contract,
            KAIN_RUNTIME_SERVICE_CORE_MASK,
            optional_service_mask,
            &app->contract_validation
        )) {
        kain_native_sculpt_emit_contract_diagnostics("raw-native-sculpt", &app->contract_validation);
        MessageBoxA(
            NULL,
            app->contract_validation.fatal_message[0]
                ? app->contract_validation.fatal_message
                : "Raw native sculpt startup validation failed.",
            "Kain Runtime Contract Error",
            MB_OK | MB_ICONERROR
        );
        return 0;
    }
    kain_native_sculpt_emit_contract_diagnostics("raw-native-sculpt", &app->contract_validation);
    return 1;
}

static KainNativeSculptSettings kain_load_sculpt_settings(void) {
    KainNativeSculptSettings settings;
    char* profile_name = kain_env_dup("KAIN_SCULPT_PROFILE");
    const KainViewportProfile* profile = kain_find_viewport_profile(profile_name);
    kain_env_free(profile_name);
    settings.profile = profile;
    settings.window_width = kain_env_int("KAIN_SCULPT_WINDOW_WIDTH", 1440);
    settings.window_height = kain_env_int("KAIN_SCULPT_WINDOW_HEIGHT", 900);
    settings.show_help = kain_env_flag("KAIN_SCULPT_SHOW_HELP", 1);
    settings.show_wireframe = kain_env_flag("KAIN_SCULPT_SHOW_WIREFRAME", 1);
    settings.show_particles = kain_env_flag("KAIN_SCULPT_SHOW_PARTICLES", 1);
    settings.particle_count = kain_env_int("KAIN_SCULPT_PARTICLE_COUNT", 120);
    settings.grid_resolution = kain_env_int("KAIN_SCULPT_GRID_RESOLUTION", 72);
    settings.world_extent = kain_env_double("KAIN_SCULPT_WORLD_EXTENT", 9.5);
    settings.brush_radius = kain_env_double("KAIN_SCULPT_BRUSH_RADIUS", 1.15);
    settings.brush_strength = kain_env_double("KAIN_SCULPT_BRUSH_STRENGTH", 1.55);
    settings.brush_falloff = kain_env_double("KAIN_SCULPT_BRUSH_FALLOFF", 2.1);
    settings.orbit_yaw = kain_env_double("KAIN_SCULPT_ORBIT_YAW", 0.8);
    settings.orbit_pitch = kain_env_double("KAIN_SCULPT_ORBIT_PITCH", 0.6);
    settings.orbit_distance = kain_env_double("KAIN_SCULPT_ORBIT_DISTANCE", 17.0);
    if (settings.window_width < 900) settings.window_width = 900;
    if (settings.window_height < 600) settings.window_height = 600;
    if (settings.grid_resolution < 24) settings.grid_resolution = 24;
    if (settings.grid_resolution > KAIN_SCULPT_MAX_RESOLUTION) settings.grid_resolution = KAIN_SCULPT_MAX_RESOLUTION;
    if (settings.particle_count < 0) settings.particle_count = 0;
    if (settings.brush_radius < 0.15) settings.brush_radius = 0.15;
    if (settings.brush_strength < 0.05) settings.brush_strength = 0.05;
    if (settings.orbit_distance < 6.0) settings.orbit_distance = 6.0;
    return settings;
}

static int kain_sculpt_vertex_index(const KainNativeSculptApp* app, int x, int z) {
    return z * (app->settings.grid_resolution + 1) + x;
}

static double kain_sculpt_get_height(const KainNativeSculptApp* app, int x, int z) {
    int max_index = app->settings.grid_resolution;
    if (x < 0) x = 0;
    if (z < 0) z = 0;
    if (x > max_index) x = max_index;
    if (z > max_index) z = max_index;
    return app->heights[kain_sculpt_vertex_index(app, x, z)];
}

static void kain_sculpt_seed_mesh(KainNativeSculptApp* app) {
    int x;
    int z;
    int resolution = app->settings.grid_resolution;
    double extent = app->settings.world_extent;
    double step = (extent * 2.0) / (double)resolution;
    for (z = 0; z <= resolution; ++z) {
        for (x = 0; x <= resolution; ++x) {
            double world_x = -extent + (x * step);
            double world_z = -extent + (z * step);
            double radial = sqrt((world_x * world_x) + (world_z * world_z));
            double mound = exp(-(radial * radial) / 22.0) * 3.8;
            double ridge = sin(world_x * 0.75) * cos(world_z * 0.55) * 0.25;
            app->heights[kain_sculpt_vertex_index(app, x, z)] = mound + ridge;
        }
    }
}

static void kain_sculpt_reset_mesh(KainNativeSculptApp* app) {
    if (!app || !app->heights) return;
    kain_sculpt_seed_mesh(app);
}

static KainVec3 kain_sculpt_vertex_position(const KainNativeSculptApp* app, int x, int z) {
    double extent = app->settings.world_extent;
    double step = (extent * 2.0) / (double)app->settings.grid_resolution;
    return kain_vec3_make(
        -extent + (x * step),
        app->heights[kain_sculpt_vertex_index(app, x, z)],
        -extent + (z * step)
    );
}

static KainVec3 kain_sculpt_vertex_normal(const KainNativeSculptApp* app, int x, int z) {
    double extent = app->settings.world_extent;
    double step = (extent * 2.0) / (double)app->settings.grid_resolution;
    double left = kain_sculpt_get_height(app, x - 1, z);
    double right = kain_sculpt_get_height(app, x + 1, z);
    double down = kain_sculpt_get_height(app, x, z - 1);
    double up = kain_sculpt_get_height(app, x, z + 1);
    KainVec3 normal = kain_vec3_make(left - right, 2.0 * step, down - up);
    return kain_vec3_normalize(normal);
}

static KainVec3 kain_sculpt_camera_eye(const KainNativeSculptApp* app) {
    double cp = cos(app->settings.orbit_pitch);
    return kain_vec3_make(
        sin(app->settings.orbit_yaw) * cp * app->settings.orbit_distance,
        3.1 + sin(app->settings.orbit_pitch) * app->settings.orbit_distance,
        cos(app->settings.orbit_yaw) * cp * app->settings.orbit_distance
    );
}

static int kain_sculpt_compute_hover(KainNativeSculptApp* app) {
    KainVec3 eye = kain_sculpt_camera_eye(app);
    KainVec3 target = kain_vec3_make(0.0, 2.2, 0.0);
    KainVec3 forward = kain_vec3_normalize(kain_vec3_sub(target, eye));
    KainVec3 right = kain_vec3_normalize(kain_vec3_cross(forward, kain_vec3_make(0.0, 1.0, 0.0)));
    KainVec3 up = kain_vec3_normalize(kain_vec3_cross(right, forward));
    double aspect = (double)app->width / (double)app->height;
    double half_tan = tan((60.0 * M_PI) / 360.0);
    double ndc_x = ((double)app->mouse_x / (double)app->width) * 2.0 - 1.0;
    double ndc_y = 1.0 - ((double)app->mouse_y / (double)app->height) * 2.0;
    KainVec3 ray = kain_vec3_add(forward, kain_vec3_add(kain_vec3_scale(right, ndc_x * half_tan * aspect), kain_vec3_scale(up, ndc_y * half_tan)));
    double t;
    KainVec3 hit;
    ray = kain_vec3_normalize(ray);
    if (fabs(ray.y) < 0.00001) {
        app->hover_valid = 0;
        return 0;
    }
    t = -eye.y / ray.y;
    if (t <= 0.0) {
        app->hover_valid = 0;
        return 0;
    }
    hit = kain_vec3_add(eye, kain_vec3_scale(ray, t));
    if (fabs(hit.x) > app->settings.world_extent || fabs(hit.z) > app->settings.world_extent) {
        app->hover_valid = 0;
        return 0;
    }
    app->hover_x = hit.x;
    app->hover_z = hit.z;
    app->hover_valid = 1;
    return 1;
}

static void kain_sculpt_apply_brush(KainNativeSculptApp* app, double dt) {
    int x;
    int z;
    int resolution;
    double extent;
    double step;
    int smooth_mode;
    double sign;
    if (!app->left_down || app->right_down) return;
    if (!kain_sculpt_compute_hover(app)) return;

    resolution = app->settings.grid_resolution;
    extent = app->settings.world_extent;
    step = (extent * 2.0) / (double)resolution;
    smooth_mode = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    sign = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 ? -1.0 : 1.0;

    for (z = 0; z <= resolution; ++z) {
        for (x = 0; x <= resolution; ++x) {
            int index = kain_sculpt_vertex_index(app, x, z);
            double world_x = -extent + (x * step);
            double world_z = -extent + (z * step);
            double dx = world_x - app->hover_x;
            double dz = world_z - app->hover_z;
            double distance = sqrt((dx * dx) + (dz * dz));
            if (distance < app->settings.brush_radius) {
                double falloff = 1.0 - (distance / app->settings.brush_radius);
                double shaped = pow(falloff, app->settings.brush_falloff);
                if (smooth_mode) {
                    double average =
                        kain_sculpt_get_height(app, x - 1, z) +
                        kain_sculpt_get_height(app, x + 1, z) +
                        kain_sculpt_get_height(app, x, z - 1) +
                        kain_sculpt_get_height(app, x, z + 1);
                    average *= 0.25;
                    app->heights[index] += (average - app->heights[index]) * shaped * dt * (app->settings.brush_strength * 5.0);
                } else {
                    app->heights[index] += sign * shaped * dt * (app->settings.brush_strength * 2.6);
                }
                app->heights[index] = kain_clampd(app->heights[index], -4.5, 8.0);
            }
        }
    }
}

static void kain_sculpt_render_mesh(const KainNativeSculptApp* app) {
    int x;
    int z;
    const KainViewportProfile* profile = app->settings.profile;
    glBegin(GL_TRIANGLES);
    for (z = 0; z < app->settings.grid_resolution; ++z) {
        for (x = 0; x < app->settings.grid_resolution; ++x) {
            KainVec3 p00 = kain_sculpt_vertex_position(app, x, z);
            KainVec3 p10 = kain_sculpt_vertex_position(app, x + 1, z);
            KainVec3 p01 = kain_sculpt_vertex_position(app, x, z + 1);
            KainVec3 p11 = kain_sculpt_vertex_position(app, x + 1, z + 1);
            KainVec3 n00 = kain_sculpt_vertex_normal(app, x, z);
            KainVec3 n10 = kain_sculpt_vertex_normal(app, x + 1, z);
            KainVec3 n01 = kain_sculpt_vertex_normal(app, x, z + 1);
            KainVec3 n11 = kain_sculpt_vertex_normal(app, x + 1, z + 1);
            float tint_a = (float)kain_clampd((p00.y + 4.5) / 12.0, 0.0, 1.0);
            float tint_b = (float)kain_clampd((p11.y + 4.5) / 12.0, 0.0, 1.0);

            glColor3f(0.16f + (profile->accent_a[0] * tint_a * 0.55f), 0.18f + (profile->accent_a[1] * tint_a * 0.35f), 0.20f + (profile->accent_b[2] * tint_a * 0.20f));
            glNormal3d(n00.x, n00.y, n00.z);
            glVertex3d(p00.x, p00.y, p00.z);
            glNormal3d(n10.x, n10.y, n10.z);
            glVertex3d(p10.x, p10.y, p10.z);
            glNormal3d(n11.x, n11.y, n11.z);
            glVertex3d(p11.x, p11.y, p11.z);

            glColor3f(0.18f + (profile->accent_b[0] * tint_b * 0.45f), 0.16f + (profile->accent_b[1] * tint_b * 0.28f), 0.19f + (profile->accent_a[2] * tint_b * 0.22f));
            glNormal3d(n00.x, n00.y, n00.z);
            glVertex3d(p00.x, p00.y, p00.z);
            glNormal3d(n11.x, n11.y, n11.z);
            glVertex3d(p11.x, p11.y, p11.z);
            glNormal3d(n01.x, n01.y, n01.z);
            glVertex3d(p01.x, p01.y, p01.z);
        }
    }
    glEnd();
}

static void kain_sculpt_render_wireframe(const KainNativeSculptApp* app) {
    int x;
    int z;
    if (!app->settings.show_wireframe) return;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.86f, 0.92f, 1.0f, 0.24f);
    for (z = 0; z < app->settings.grid_resolution; ++z) {
        glBegin(GL_LINE_STRIP);
        for (x = 0; x <= app->settings.grid_resolution; ++x) {
            KainVec3 p = kain_sculpt_vertex_position(app, x, z);
            glVertex3d(p.x, p.y + 0.01, p.z);
        }
        glEnd();
    }
    for (x = 0; x < app->settings.grid_resolution; ++x) {
        glBegin(GL_LINE_STRIP);
        for (z = 0; z <= app->settings.grid_resolution; ++z) {
            KainVec3 p = kain_sculpt_vertex_position(app, x, z);
            glVertex3d(p.x, p.y + 0.01, p.z);
        }
        glEnd();
    }
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void kain_sculpt_render_particles(const KainNativeSculptApp* app) {
    int i;
    const KainViewportProfile* profile = app->settings.profile;
    if (!app->settings.show_particles) return;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    for (i = 0; i < app->settings.particle_count; ++i) {
        double orbit = ((double)i / (double)(app->settings.particle_count + 1)) * M_PI * 2.0;
        double spin = app->total_time * (0.45 + ((i % 9) * 0.05));
        double radius = 3.2 + fmod((double)i * 0.27, app->settings.world_extent * 0.7);
        double px = cos(orbit + spin) * radius;
        double py = 1.0 + fmod((double)i * 0.09 + app->total_time * 0.7, 5.6);
        double pz = sin(orbit - spin) * radius;
        float alpha = 0.28f + (float)(0.32 * (0.5 + 0.5 * sin(app->total_time * 1.9 + i)));
        if ((i & 1) == 0) {
            glColor4f(profile->accent_a[0], profile->accent_a[1], profile->accent_a[2], alpha);
        } else {
            glColor4f(profile->accent_b[0], profile->accent_b[1], profile->accent_b[2], alpha);
        }
        glVertex3f((GLfloat)px, (GLfloat)py, (GLfloat)pz);
    }
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

static void kain_sculpt_render_brush_ring(const KainNativeSculptApp* app) {
    int segment;
    if (!app->hover_valid) return;
    glDisable(GL_LIGHTING);
    glColor4f(0.95f, 0.96f, 1.0f, 0.85f);
    glBegin(GL_LINE_LOOP);
    for (segment = 0; segment < 48; ++segment) {
        double angle = ((double)segment / 48.0) * M_PI * 2.0;
        glVertex3d(app->hover_x + cos(angle) * app->settings.brush_radius, 0.08, app->hover_z + sin(angle) * app->settings.brush_radius);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

static void kain_sculpt_render_overlay(KainNativeSculptApp* app) {
    char subtitle_line[256];
    char stats_line[256];
    char realtime_line[256];
    char contract_line[256];
    char validation_line[256];
    const char* mode = "raise";
    const char* live_lines[4];
    const char* help_lines[2];
    KainUiCompiledOverlaySpec overlay_spec;
    const KainViewportProfile* profile = app->settings.profile;

    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) mode = "smooth";
    else if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) mode = "carve";
    snprintf(stats_line, sizeof(stats_line), "fps %.1f  |  brush %.2f  |  strength %.2f  |  mode %s", app->frame_fps, app->settings.brush_radius, app->settings.brush_strength, mode);
    snprintf(subtitle_line, sizeof(subtitle_line), "%s  |  native llvm exe  |  sculpt foundation", profile->label);
    if (app->realtime_bundle.loaded) {
        snprintf(
            realtime_line,
            sizeof(realtime_line),
            "realtime %s via %s  |  viewport %s  |  materials %d",
            app->realtime_bundle.target[0] ? app->realtime_bundle.target : "unknown",
            app->realtime_bundle.load_origin[0] ? app->realtime_bundle.load_origin : "path",
            app->realtime_bundle.primary_scene[0] ? app->realtime_bundle.primary_scene : "none",
            app->realtime_bundle.material_count
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
    if (app->runtime_contract.loaded) {
        snprintf(
            contract_line,
            sizeof(contract_line),
            "contract %s via %s  |  core %d/3  |  items %d",
            app->runtime_contract.target[0] ? app->runtime_contract.target : "unknown",
            app->runtime_contract.load_origin[0] ? app->runtime_contract.load_origin : "path",
            app->runtime_contract.core_service_count,
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
    live_lines[0] = stats_line;
    live_lines[1] = realtime_line;
    live_lines[2] = contract_line;
    live_lines[3] = validation_line;
    help_lines[0] = "LMB sculpt  |  Shift+LMB carve  |  Ctrl+LMB smooth  |  RMB orbit";
    help_lines[1] = "Wheel / [ ] radius  |  - = strength  |  Tab wireframe  |  P particles  |  R reset";

    ZeroMemory(&overlay_spec, sizeof(overlay_spec));
    overlay_spec.profile = profile;
    overlay_spec.x = 18.0f;
    overlay_spec.y = 18.0f;
    overlay_spec.width = 460.0f;
    overlay_spec.panel_alpha = 0.84f;
    overlay_spec.show_help = app->settings.show_help;
    overlay_spec.draw_crosshair = 0;
    overlay_spec.fallback_title = "KAIN SCULPT LAB";
    overlay_spec.fallback_subtitle = subtitle_line;
    overlay_spec.live_lines = live_lines;
    overlay_spec.live_line_count = 4;
    overlay_spec.help_lines = help_lines;
    overlay_spec.help_line_count = 2;
    overlay_spec.fallback_hint = app->contract_validation.warning_count > 0
        ? app->contract_validation.warnings[0]
        : (app->runtime_contract.loaded
            ? "Runtime contract validated. This sculpt lab is running on the raw Kain native lane."
            : "No runtime contract was loaded. Keep the *.runtime_contract.json sidecar beside the exe for native-lane validation.");
    kain_ui_compiled_overlay_render(&app->surface, app->width, app->height, &app->compiled_ui, &overlay_spec);
}

static void kain_sculpt_render_scene(KainNativeSculptApp* app) {
    const KainViewportProfile* profile = app->settings.profile;
    GLfloat fog_color[4];
    KainVec3 eye = kain_sculpt_camera_eye(app);
    KainVec3 target = kain_vec3_make(0.0, 2.2, 0.0);
    glViewport(0, 0, app->width, app->height);
    glClearColor(profile->clear_color[0] * 0.75f, profile->clear_color[1] * 0.75f, profile->clear_color[2] * 0.85f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_FOG);
    fog_color[0] = profile->fog_color[0];
    fog_color[1] = profile->fog_color[1];
    fog_color[2] = profile->fog_color[2];
    fog_color[3] = profile->fog_color[3];
    glFogfv(GL_FOG_COLOR, fog_color);
    glFogf(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, (GLfloat)(profile->fog_density * 0.7));
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, profile->ambient_light);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, profile->diffuse_light);
    glLightfv(GL_LIGHT0, GL_POSITION, profile->light_position);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    kain_gl_perspective(60.0, (double)app->width / (double)app->height, 0.1, 120.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    kain_gl_look_at(eye, target, kain_vec3_make(0.0, 1.0, 0.0));

    glDisable(GL_LIGHTING);
    glColor3f(0.12f, 0.13f, 0.17f);
    glBegin(GL_QUADS);
    glVertex3f((GLfloat)-app->settings.world_extent, -0.02f, (GLfloat)-app->settings.world_extent);
    glVertex3f((GLfloat)app->settings.world_extent, -0.02f, (GLfloat)-app->settings.world_extent);
    glVertex3f((GLfloat)app->settings.world_extent, -0.02f, (GLfloat)app->settings.world_extent);
    glVertex3f((GLfloat)-app->settings.world_extent, -0.02f, (GLfloat)app->settings.world_extent);
    glEnd();
    glEnable(GL_LIGHTING);

    kain_sculpt_render_mesh(app);
    kain_sculpt_render_wireframe(app);
    kain_sculpt_render_particles(app);
    kain_sculpt_render_brush_ring(app);
    kain_sculpt_render_overlay(app);
}

static int kain_sculpt_init_gl(KainNativeSculptApp* app) {
    return kain_win32_gl_surface_boot(app->hwnd, &app->surface, 16);
}

static void kain_sculpt_shutdown_gl(KainNativeSculptApp* app) {
    if (!app) return;
    kain_win32_gl_surface_shutdown(app->hwnd, &app->surface);
}

static int kain_native_sculpt_host_init(KainWin32AppHost* host, void* user_data) {
    KainNativeSculptApp* app = (KainNativeSculptApp*)user_data;
    if (!host || !app) return 0;
    app->hwnd = host->hwnd;
    app->width = host->width;
    app->height = host->height;
    kain_win32_mouse_capture_bind(&app->mouse_capture, app->hwnd);
    return kain_sculpt_init_gl(app);
}

static void kain_native_sculpt_host_frame(KainWin32AppHost* host, void* user_data, double frame_delta) {
    KainNativeSculptApp* app = (KainNativeSculptApp*)user_data;
    if (!host || !app) return;
    app->frame_delta = frame_delta;
    app->frame_fps = host->frame_fps;
    app->total_time += frame_delta;
    kain_sculpt_apply_brush(app, frame_delta);
    kain_sculpt_compute_hover(app);
    kain_sculpt_render_scene(app);
    kain_win32_gl_surface_present(&app->surface);
}

static void kain_native_sculpt_host_shutdown(KainWin32AppHost* host, void* user_data) {
    KainNativeSculptApp* app = (KainNativeSculptApp*)user_data;
    (void)host;
    if (!app) return;
    kain_win32_mouse_capture_release_all(&app->mouse_capture);
    kain_sculpt_shutdown_gl(app);
}

static LRESULT kain_native_sculpt_host_message(
    KainWin32AppHost* host,
    void* user_data,
    HWND hwnd,
    UINT msg,
    WPARAM w_param,
    LPARAM l_param,
    int* handled
) {
    KainNativeSculptApp* app = (KainNativeSculptApp*)user_data;
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
        case WM_MOUSEMOVE:
            app->mouse_x = GET_X_LPARAM(l_param);
            app->mouse_y = GET_Y_LPARAM(l_param);
            if (app->right_down) {
                int dx = app->mouse_x - app->last_mouse_x;
                int dy = app->mouse_y - app->last_mouse_y;
                app->settings.orbit_yaw += dx * 0.009;
                app->settings.orbit_pitch += dy * 0.009;
                app->settings.orbit_pitch = kain_clampd(app->settings.orbit_pitch, -1.2, 1.2);
            }
            app->last_mouse_x = app->mouse_x;
            app->last_mouse_y = app->mouse_y;
            *handled = 1;
            return 0;
        case WM_MOUSEWHEEL:
            {
                short delta = GET_WHEEL_DELTA_WPARAM(w_param);
                app->settings.brush_radius += delta > 0 ? 0.12 : -0.12;
                app->settings.brush_radius = kain_clampd(app->settings.brush_radius, 0.15, app->settings.world_extent * 0.5);
            }
            *handled = 1;
            return 0;
        case WM_LBUTTONDOWN:
            kain_win32_mouse_capture_begin_drag(&app->mouse_capture, hwnd);
            app->left_down = 1;
            app->mouse_x = GET_X_LPARAM(l_param);
            app->mouse_y = GET_Y_LPARAM(l_param);
            app->last_mouse_x = app->mouse_x;
            app->last_mouse_y = app->mouse_y;
            *handled = 1;
            return 0;
        case WM_LBUTTONUP:
            app->left_down = 0;
            kain_win32_mouse_capture_end_drag(&app->mouse_capture);
            *handled = 1;
            return 0;
        case WM_RBUTTONDOWN:
            kain_win32_mouse_capture_begin_drag(&app->mouse_capture, hwnd);
            app->right_down = 1;
            app->mouse_x = GET_X_LPARAM(l_param);
            app->mouse_y = GET_Y_LPARAM(l_param);
            app->last_mouse_x = app->mouse_x;
            app->last_mouse_y = app->mouse_y;
            *handled = 1;
            return 0;
        case WM_RBUTTONUP:
            app->right_down = 0;
            kain_win32_mouse_capture_end_drag(&app->mouse_capture);
            *handled = 1;
            return 0;
        case WM_KEYDOWN:
            switch (w_param) {
                case VK_OEM_4:
                    app->settings.brush_radius = kain_clampd(app->settings.brush_radius - 0.12, 0.15, app->settings.world_extent * 0.5);
                    break;
                case VK_OEM_6:
                    app->settings.brush_radius = kain_clampd(app->settings.brush_radius + 0.12, 0.15, app->settings.world_extent * 0.5);
                    break;
                case VK_OEM_MINUS:
                    app->settings.brush_strength = kain_clampd(app->settings.brush_strength - 0.10, 0.05, 6.0);
                    break;
                case VK_OEM_PLUS:
                    app->settings.brush_strength = kain_clampd(app->settings.brush_strength + 0.10, 0.05, 6.0);
                    break;
                case 'R':
                    kain_sculpt_reset_mesh(app);
                    break;
                case 'P':
                    app->settings.show_particles = !app->settings.show_particles;
                    break;
                case VK_TAB:
                    app->settings.show_wireframe = !app->settings.show_wireframe;
                    break;
                case VK_F1:
                    app->settings.show_help = !app->settings.show_help;
                    break;
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

static void kain_run_native_sculpt_lab(double x, double y, const char* window_title) {
    KainNativeSculptApp app;
    KainWin32AppConfig config;
    const char* resolved_window_title = window_title;
    int vertex_count;
    (void)x;
    (void)y;

    ZeroMemory(&app, sizeof(app));
    ZeroMemory(&config, sizeof(config));
    app.settings = kain_load_sculpt_settings();
    kain_native_sculpt_try_load_runtime_contract(&app);
    if (!kain_native_sculpt_validate_runtime_contract(&app)) {
        return;
    }
    kain_native_sculpt_try_load_realtime_bundle(&app);
    kain_native_sculpt_try_load_compiled_ui(&app);
    app.width = app.settings.window_width;
    app.height = app.settings.window_height;
    app.running = 1;
    vertex_count = (app.settings.grid_resolution + 1) * (app.settings.grid_resolution + 1);
    app.heights = (double*)calloc((size_t)vertex_count, sizeof(double));
    if (!app.heights) {
        return;
    }
    kain_sculpt_seed_mesh(&app);
    if (app.compiled_ui.loaded && app.compiled_ui.window_title[0]) {
        resolved_window_title = app.compiled_ui.window_title;
    }
    config.class_name = "KainNativeSculptWindowClass";
    config.window_title = resolved_window_title;
    config.default_width = app.width;
    config.default_height = app.height;
    config.sleep_millis = 1;
    config.min_frame_delta = 0.001;
    config.max_frame_delta = 0.050;
    config.on_init = kain_native_sculpt_host_init;
    config.on_frame = kain_native_sculpt_host_frame;
    config.on_shutdown = kain_native_sculpt_host_shutdown;
    config.on_message = kain_native_sculpt_host_message;

    kain_win32_app_run(&app.host, &config, &app);
    if (app.heights) {
        free(app.heights);
        app.heights = NULL;
    }
}
#endif

void spawn_native_sculpt_lab(double x, double y) {
#ifdef _WIN32
    kain_run_native_sculpt_lab(x, y, "KAIN Native Sculpt Lab");
#else
    printf("[KAIN] spawn_native_sculpt_lab is currently only implemented on Windows. Requested at { x: %.2f, y: %.2f }\n", x, y);
#endif
}
