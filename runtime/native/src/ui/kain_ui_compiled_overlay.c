#include "../../include/kain_runtime_ui.h"

#ifdef _WIN32
static const char* kain_ui_overlay_value_or_default(const char* value, const char* fallback) {
    return (value && value[0]) ? value : fallback;
}

static const KainUiCompiledNode* kain_ui_overlay_find_root_node(const KainUiCompiledBundle* bundle) {
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

static const KainUiCompiledNode* kain_ui_overlay_find_primary_panel_node(const KainUiCompiledBundle* bundle) {
    const KainUiCompiledNode* root_node = kain_ui_overlay_find_root_node(bundle);
    const KainUiCompiledNode* panel_node;

    if (root_node && (root_node->kind == KAIN_UI_COMPILED_NODE_PANEL || root_node->kind == KAIN_UI_COMPILED_NODE_OVERLAY)) {
        return root_node;
    }

    panel_node = kain_ui_compiled_bundle_find_first_kind(bundle, KAIN_UI_COMPILED_NODE_PANEL);
    if (panel_node) {
        return panel_node;
    }

    return kain_ui_compiled_bundle_find_first_kind(bundle, KAIN_UI_COMPILED_NODE_OVERLAY);
}

static const KainUiCompiledNode* kain_ui_overlay_find_primary_viewport_node(const KainUiCompiledBundle* bundle) {
    const KainUiCompiledNode* root_node = kain_ui_overlay_find_root_node(bundle);
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

static const char* kain_ui_overlay_resolve_panel_title(const KainUiCompiledBundle* bundle, const char* fallback_title) {
    const KainUiCompiledNode* panel_node = kain_ui_overlay_find_primary_panel_node(bundle);

    if (panel_node && panel_node->title[0]) {
        return panel_node->title;
    }

    return kain_ui_overlay_value_or_default(
        bundle ? bundle->primary_panel_title : NULL,
        fallback_title
    );
}

static const char* kain_ui_overlay_resolve_viewport_title(const KainUiCompiledBundle* bundle, const char* fallback_title) {
    const KainUiCompiledNode* viewport_node = kain_ui_overlay_find_primary_viewport_node(bundle);

    if (viewport_node && viewport_node->title[0]) {
        return viewport_node->title;
    }

    return kain_ui_overlay_value_or_default(
        bundle ? bundle->primary_viewport_title : NULL,
        fallback_title
    );
}

static const char* kain_ui_overlay_resolve_scene_name(const KainUiCompiledBundle* bundle, const char* fallback_scene) {
    const KainUiCompiledNode* viewport_node = kain_ui_overlay_find_primary_viewport_node(bundle);
    const KainUiCompiledNode* root_node = kain_ui_overlay_find_root_node(bundle);

    if (viewport_node && viewport_node->scene[0]) {
        return viewport_node->scene;
    }
    if (root_node && root_node->scene[0]) {
        return root_node->scene;
    }

    return kain_ui_overlay_value_or_default(
        bundle ? bundle->primary_viewport_scene : NULL,
        fallback_scene
    );
}

static void kain_ui_overlay_push_line(
    const char** lines,
    int* line_count,
    const char* value
) {
    if (!lines || !line_count || !value || !value[0]) {
        return;
    }
    if (*line_count >= KAIN_UI_COMPILED_OVERLAY_MAX_LINES) {
        return;
    }
    lines[*line_count] = value;
    *line_count += 1;
}

static void kain_ui_overlay_format_node_line(
    char* out,
    size_t out_cap,
    const KainUiCompiledNode* node,
    const char* fallback_label,
    const char* suffix
) {
    const char* label;

    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!node) {
        return;
    }

    label = node->title[0] ? node->title : fallback_label;
    if (node->text[0]) {
        snprintf(out, out_cap, "%s | %s%s%s", label, node->text, suffix ? " | " : "", suffix ? suffix : "");
    } else if (suffix && suffix[0]) {
        snprintf(out, out_cap, "%s | %s", label, suffix);
    } else {
        snprintf(out, out_cap, "%s", label);
    }
}

void kain_ui_overlay_make_default_theme(const KainViewportProfile* profile, float panel_alpha, KainUiOverlayTheme* theme) {
    if (!theme) {
        return;
    }

    ZeroMemory(theme, sizeof(*theme));
    theme->panel_color[0] = 0.03f;
    theme->panel_color[1] = 0.05f;
    theme->panel_color[2] = 0.09f;
    theme->panel_color[3] = panel_alpha > 0.01f ? panel_alpha : 0.82f;
    theme->title_color[0] = 0.94f;
    theme->title_color[1] = 0.97f;
    theme->title_color[2] = 1.0f;
    theme->title_color[3] = 1.0f;
    theme->text_color[0] = 0.94f;
    theme->text_color[1] = 0.97f;
    theme->text_color[2] = 1.0f;
    theme->text_color[3] = 1.0f;
    theme->crosshair_color[0] = 0.95f;
    theme->crosshair_color[1] = 0.96f;
    theme->crosshair_color[2] = 1.0f;
    theme->crosshair_color[3] = 0.9f;
    theme->padding_x = 16.0f;
    theme->title_y = 28.0f;
    theme->subtitle_y = 48.0f;
    theme->line_y_start = 68.0f;
    theme->line_gap = 20.0f;

    if (profile) {
        theme->accent_color[0] = profile->accent_a[0];
        theme->accent_color[1] = profile->accent_a[1];
        theme->accent_color[2] = profile->accent_a[2];
        theme->accent_color[3] = 0.96f;
    } else {
        theme->accent_color[0] = 0.35f;
        theme->accent_color[1] = 0.72f;
        theme->accent_color[2] = 0.98f;
        theme->accent_color[3] = 0.96f;
    }
}

void kain_ui_compiled_overlay_render(
    KainWin32GlSurface* surface,
    int viewport_width,
    int viewport_height,
    const KainUiCompiledBundle* bundle,
    const KainUiCompiledOverlaySpec* spec
) {
    KainUiOverlayTheme theme;
    KainUiOverlayPanel panel;
    const char* lines[KAIN_UI_COMPILED_OVERLAY_MAX_LINES];
    char generated_lines[4][KAIN_UI_COMPILED_BUNDLE_MAX_TEXT];
    int line_count = 0;
    int generated_count = 0;
    const char* panel_title;
    const char* viewport_title;
    const char* scene_name;
    const char* subtitle_line;
    const KainUiCompiledNode* panel_node = NULL;
    const KainUiCompiledNode* viewport_node = NULL;
    const KainUiCompiledNode* inspector = NULL;
    const KainUiCompiledNode* tree = NULL;
    const KainUiCompiledNode* timeline = NULL;

    if (!surface || !spec || viewport_width <= 0 || viewport_height <= 0) {
        return;
    }

    panel_title = kain_ui_overlay_value_or_default(spec->fallback_title, "KAIN NATIVE APP");
    subtitle_line = spec->fallback_subtitle;
    viewport_title = spec->profile ? spec->profile->label : "native viewport";
    scene_name = spec->profile ? spec->profile->id : "default";

    if (bundle && bundle->loaded) {
        panel_node = kain_ui_overlay_find_primary_panel_node(bundle);
        viewport_node = kain_ui_overlay_find_primary_viewport_node(bundle);
        inspector = kain_ui_compiled_bundle_find_first_kind(bundle, KAIN_UI_COMPILED_NODE_INSPECTOR);
        tree = kain_ui_compiled_bundle_find_first_kind(bundle, KAIN_UI_COMPILED_NODE_TREE);
        timeline = kain_ui_compiled_bundle_find_first_kind(bundle, KAIN_UI_COMPILED_NODE_TIMELINE);
        panel_title = kain_ui_overlay_resolve_panel_title(bundle, panel_title);
        viewport_title = kain_ui_overlay_resolve_viewport_title(bundle, viewport_title);
        scene_name = kain_ui_overlay_resolve_scene_name(bundle, scene_name);
        snprintf(generated_lines[generated_count], sizeof(generated_lines[generated_count]), "compiled ui  |  %s  |  scene %s", viewport_title, scene_name);
        kain_ui_overlay_push_line(lines, &line_count, generated_lines[generated_count]);
        generated_count += 1;
    } else if (subtitle_line && subtitle_line[0]) {
        kain_ui_overlay_push_line(lines, &line_count, subtitle_line);
    }

    if (spec->live_lines && spec->live_line_count > 0) {
        int index;
        for (index = 0; index < spec->live_line_count && line_count < KAIN_UI_COMPILED_OVERLAY_MAX_LINES; ++index) {
            kain_ui_overlay_push_line(lines, &line_count, spec->live_lines[index]);
        }
    }

    if (spec->show_help) {
        int index;
        if (spec->help_lines && spec->help_line_count > 0) {
            for (index = 0; index < spec->help_line_count && line_count < KAIN_UI_COMPILED_OVERLAY_MAX_LINES; ++index) {
                kain_ui_overlay_push_line(lines, &line_count, spec->help_lines[index]);
            }
        }

        if (bundle && bundle->loaded) {
            if (inspector && generated_count < 4 && line_count < KAIN_UI_COMPILED_OVERLAY_MAX_LINES) {
                kain_ui_overlay_format_node_line(
                    generated_lines[generated_count],
                    sizeof(generated_lines[generated_count]),
                    inspector,
                    "compiled inspector",
                    tree && tree->title[0] ? "asset tree active" : NULL
                );
                kain_ui_overlay_push_line(lines, &line_count, generated_lines[generated_count]);
                generated_count += 1;
            }

            if (timeline && generated_count < 4 && line_count < KAIN_UI_COMPILED_OVERLAY_MAX_LINES) {
                kain_ui_overlay_format_node_line(
                    generated_lines[generated_count],
                    sizeof(generated_lines[generated_count]),
                    timeline,
                    "timeline",
                    "playback surface ready"
                );
                kain_ui_overlay_push_line(lines, &line_count, generated_lines[generated_count]);
                generated_count += 1;
            } else if (viewport_node && generated_count < 4 && line_count < KAIN_UI_COMPILED_OVERLAY_MAX_LINES) {
                kain_ui_overlay_format_node_line(
                    generated_lines[generated_count],
                    sizeof(generated_lines[generated_count]),
                    viewport_node,
                    "viewport",
                    scene_name
                );
                kain_ui_overlay_push_line(lines, &line_count, generated_lines[generated_count]);
                generated_count += 1;
            }
        } else if (spec->fallback_hint && spec->fallback_hint[0]) {
            kain_ui_overlay_push_line(lines, &line_count, spec->fallback_hint);
        }
    }

    kain_ui_overlay_make_default_theme(spec->profile, spec->panel_alpha, &theme);
    panel.x = spec->x;
    panel.y = spec->y;
    panel.width = spec->width > 1.0f ? spec->width : 420.0f;
    panel.height = 54.0f + (line_count > 0 ? (float)line_count * theme.line_gap : 0.0f);
    if (panel.height < 72.0f) {
        panel.height = 72.0f;
    }
    panel.title = panel_title;
    panel.subtitle = NULL;
    panel.lines = lines;
    panel.line_count = line_count;

    kain_ui_overlay_begin(viewport_width, viewport_height);
    kain_ui_overlay_draw_panel(surface, &theme, &panel);
    if (spec->draw_crosshair) {
        kain_ui_overlay_draw_crosshair(viewport_width, viewport_height, theme.crosshair_color);
    }
    kain_ui_overlay_end();
}
#endif
