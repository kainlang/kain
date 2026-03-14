#ifndef KAIN_RUNTIME_UI_H
#define KAIN_RUNTIME_UI_H

#include "kain_runtime_win32.h"

#ifdef _WIN32
#define KAIN_UI_COMPILED_BUNDLE_ENV "KAIN_NATIVE_UI_BUNDLE"
#define KAIN_UI_COMPILED_BUNDLE_MAX_NODES 128
#define KAIN_UI_COMPILED_BUNDLE_MAX_TITLE 160
#define KAIN_UI_COMPILED_BUNDLE_MAX_TEXT 320
#define KAIN_UI_COMPILED_BUNDLE_MAX_TAG 64
#define KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT 32
#define KAIN_UI_COMPILED_OVERLAY_MAX_LINES 8

typedef struct {
    float panel_color[4];
    float title_color[4];
    float text_color[4];
    float accent_color[4];
    float crosshair_color[4];
    float padding_x;
    float title_y;
    float subtitle_y;
    float line_y_start;
    float line_gap;
} KainUiOverlayTheme;

typedef struct {
    float x;
    float y;
    float width;
    float height;
    const char* title;
    const char* subtitle;
    const char** lines;
    int line_count;
} KainUiOverlayPanel;

typedef enum {
    KAIN_UI_COMPILED_NODE_UNKNOWN = 0,
    KAIN_UI_COMPILED_NODE_ELEMENT,
    KAIN_UI_COMPILED_NODE_COMPONENT_REF,
    KAIN_UI_COMPILED_NODE_TEXT,
    KAIN_UI_COMPILED_NODE_PANEL,
    KAIN_UI_COMPILED_NODE_INSPECTOR,
    KAIN_UI_COMPILED_NODE_GRAPH,
    KAIN_UI_COMPILED_NODE_TIMELINE,
    KAIN_UI_COMPILED_NODE_TABLE,
    KAIN_UI_COMPILED_NODE_TREE,
    KAIN_UI_COMPILED_NODE_VIEWPORT2D,
    KAIN_UI_COMPILED_NODE_VIEWPORT3D,
    KAIN_UI_COMPILED_NODE_OVERLAY,
    KAIN_UI_COMPILED_NODE_SLOT,
} KainUiCompiledNodeKind;

typedef struct {
    unsigned long long id;
    unsigned long long parent_id;
    int has_parent;
    unsigned int depth;
    KainUiCompiledNodeKind kind;
    char title[KAIN_UI_COMPILED_BUNDLE_MAX_TITLE];
    char text[KAIN_UI_COMPILED_BUNDLE_MAX_TEXT];
    char tag[KAIN_UI_COMPILED_BUNDLE_MAX_TAG];
    char scene[KAIN_UI_COMPILED_BUNDLE_MAX_TAG];
    char layout_kind[KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT];
    int child_count;
} KainUiCompiledNode;

typedef struct {
    int loaded;
    int has_root_id;
    unsigned long long root_id;
    char window_title[KAIN_UI_COMPILED_BUNDLE_MAX_TITLE];
    char primary_panel_title[KAIN_UI_COMPILED_BUNDLE_MAX_TITLE];
    char primary_viewport_title[KAIN_UI_COMPILED_BUNDLE_MAX_TITLE];
    char primary_viewport_scene[KAIN_UI_COMPILED_BUNDLE_MAX_TAG];
    int node_count;
    KainUiCompiledNode nodes[KAIN_UI_COMPILED_BUNDLE_MAX_NODES];
} KainUiCompiledBundle;

typedef struct {
    const KainViewportProfile* profile;
    float x;
    float y;
    float width;
    float panel_alpha;
    int show_help;
    int draw_crosshair;
    const char* fallback_title;
    const char* fallback_subtitle;
    const char** live_lines;
    int live_line_count;
    const char** help_lines;
    int help_line_count;
    const char* fallback_hint;
} KainUiCompiledOverlaySpec;

void kain_ui_overlay_begin(int viewport_width, int viewport_height);
void kain_ui_overlay_end(void);
void kain_ui_overlay_draw_panel(KainWin32GlSurface* surface, const KainUiOverlayTheme* theme, const KainUiOverlayPanel* panel);
void kain_ui_overlay_draw_crosshair(int viewport_width, int viewport_height, const float color[4]);
void kain_ui_overlay_make_default_theme(const KainViewportProfile* profile, float panel_alpha, KainUiOverlayTheme* theme);
void kain_ui_compiled_bundle_init(KainUiCompiledBundle* bundle);
int kain_ui_compiled_bundle_load_from_json(const char* json, KainUiCompiledBundle* bundle);
int kain_ui_compiled_bundle_load_from_path(const char* path, KainUiCompiledBundle* bundle);
int kain_ui_compiled_bundle_load_from_env(const char* env_name, KainUiCompiledBundle* bundle);
const KainUiCompiledNode* kain_ui_compiled_bundle_find_first_kind(
    const KainUiCompiledBundle* bundle,
    KainUiCompiledNodeKind kind
);
void kain_ui_compiled_overlay_render(
    KainWin32GlSurface* surface,
    int viewport_width,
    int viewport_height,
    const KainUiCompiledBundle* bundle,
    const KainUiCompiledOverlaySpec* spec
);
#endif

#endif
