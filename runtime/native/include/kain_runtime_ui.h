#ifndef KAIN_RUNTIME_UI_H
#define KAIN_RUNTIME_UI_H

#include "kain_runtime_win32.h"

#define KAIN_UI_COMPILED_BUNDLE_ENV "KAIN_NATIVE_UI_BUNDLE"
#define KAIN_UI_COMPILED_BUNDLE_MAX_NODES 128
#define KAIN_UI_COMPILED_BUNDLE_MAX_TITLE 160
#define KAIN_UI_COMPILED_BUNDLE_MAX_TEXT 320
#define KAIN_UI_COMPILED_BUNDLE_MAX_TAG 64
#define KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT 32
#define KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT_ID 96
#define KAIN_UI_COMPILED_BUNDLE_MAX_TAB_GROUPS 32
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
    char dock_placement[KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT];
    int has_split_ratio;
    float split_ratio;
    int resizable;
    char persistent_layout_id[KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT_ID];
    char tab_group_id[KAIN_UI_COMPILED_BUNDLE_MAX_TAG];
    char tab_label[KAIN_UI_COMPILED_BUNDLE_MAX_TITLE];
    int has_tab_order;
    int tab_order;
    int tab_default_active;
    int tab_closable;
    int tab_is_active;
    int child_count;
} KainUiCompiledNode;

typedef struct {
    char id[KAIN_UI_COMPILED_BUNDLE_MAX_TAG];
    char active_tab_layout_id[KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT_ID];
    int tab_count;
} KainUiCompiledTabGroup;

typedef struct {
    int loaded;
    int has_root_id;
    unsigned long long root_id;
    char window_title[KAIN_UI_COMPILED_BUNDLE_MAX_TITLE];
    int tab_group_count;
    KainUiCompiledTabGroup tab_groups[KAIN_UI_COMPILED_BUNDLE_MAX_TAB_GROUPS];
    int node_count;
    KainUiCompiledNode nodes[KAIN_UI_COMPILED_BUNDLE_MAX_NODES];
} KainUiCompiledBundle;

void kain_ui_compiled_bundle_init(KainUiCompiledBundle* bundle);
int kain_ui_compiled_bundle_load_from_json(const char* json, KainUiCompiledBundle* bundle);
int kain_ui_compiled_bundle_load_from_path(const char* path, KainUiCompiledBundle* bundle);
int kain_ui_compiled_bundle_load_from_env(const char* env_name, KainUiCompiledBundle* bundle);
const KainUiCompiledNode* kain_ui_compiled_bundle_find_first_kind(
    const KainUiCompiledBundle* bundle,
    KainUiCompiledNodeKind kind
);

#endif
