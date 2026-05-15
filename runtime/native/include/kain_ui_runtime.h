#ifndef KAIN_UI_RUNTIME_H
#define KAIN_UI_RUNTIME_H

#include "kain_runtime_ui.h"
#include "kain_runtime_diagnostics.h"

#define KAIN_UI_RUNTIME_MAX_COMPONENTS KAIN_UI_COMPILED_BUNDLE_MAX_NODES
#define KAIN_UI_RUNTIME_MAX_ISSUES 16
#define KAIN_UI_RUNTIME_MAX_EVENT_TEXT 160

#define KAIN_UI_RUNTIME_KEY_TAB 9u
#define KAIN_UI_RUNTIME_KEY_ENTER 13u
#define KAIN_UI_RUNTIME_KEY_ESCAPE 27u
#define KAIN_UI_RUNTIME_KEY_BACKSPACE 8u
#define KAIN_UI_RUNTIME_KEY_DELETE 46u

typedef enum {
    KAIN_UI_RUNTIME_CAP_NONE = 0u,
    KAIN_UI_RUNTIME_CAP_BUNDLE_VALIDATED = 1u << 0,
    KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS = 1u << 1,
    KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING = 1u << 2,
    KAIN_UI_RUNTIME_CAP_EVENT_ROUTING = 1u << 3,
    KAIN_UI_RUNTIME_CAP_EDITABLE_CONTROLS = 1u << 4,
    KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT = 1u << 5,
    KAIN_UI_RUNTIME_CAP_STATE_PERSISTENCE = 1u << 6,
} KainUiRuntimeCapabilityFlags;

typedef enum {
    KAIN_UI_RUNTIME_EVENT_NONE = 0,
    KAIN_UI_RUNTIME_EVENT_FOCUS_REQUEST,
    KAIN_UI_RUNTIME_EVENT_FOCUS_NEXT,
    KAIN_UI_RUNTIME_EVENT_FOCUS_PREV,
    KAIN_UI_RUNTIME_EVENT_BLUR,
    KAIN_UI_RUNTIME_EVENT_POINTER_DOWN,
    KAIN_UI_RUNTIME_EVENT_POINTER_UP,
    KAIN_UI_RUNTIME_EVENT_POINTER_MOVE,
    KAIN_UI_RUNTIME_EVENT_KEY_DOWN,
    KAIN_UI_RUNTIME_EVENT_KEY_UP,
    KAIN_UI_RUNTIME_EVENT_TEXT_INPUT,
    KAIN_UI_RUNTIME_EVENT_EDIT_COMMIT,
    KAIN_UI_RUNTIME_EVENT_EDIT_CANCEL,
} KainUiRuntimeEventKind;

typedef struct {
    KainUiRuntimeEventKind kind;
    unsigned long long target_component_id;
    unsigned int key_code;
    unsigned int modifiers;
    int x;
    int y;
    int delta_x;
    int delta_y;
    char text[KAIN_UI_RUNTIME_MAX_EVENT_TEXT];
} KainUiRuntimeEvent;

typedef struct {
    int handled;
    int focus_changed;
    int edit_changed;
    int dirty_changed;
    unsigned long long target_component_id;
    unsigned long long focused_component_id;
    unsigned long long editable_component_id;
    unsigned int routed_event_kind;
    char note[KAIN_UI_COMPILED_BUNDLE_MAX_TEXT];
} KainUiRuntimeEventResult;

typedef struct {
    unsigned long long id;
    unsigned long long parent_id;
    int has_parent;
    unsigned int depth;
    int node_index;
    KainUiCompiledNodeKind kind;
    unsigned int capability_flags;
    int focusable;
    int editable;
    int dirty;
    unsigned int revision;
    unsigned int dirty_reason_mask;
    unsigned int last_event_kind;
    char role[32];
    char title[KAIN_UI_COMPILED_BUNDLE_MAX_TITLE];
    char text[KAIN_UI_COMPILED_BUNDLE_MAX_TEXT];
    char tag[KAIN_UI_COMPILED_BUNDLE_MAX_TAG];
    char scene[KAIN_UI_COMPILED_BUNDLE_MAX_TAG];
    char layout_kind[KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT];
    char persistent_layout_id[KAIN_UI_COMPILED_BUNDLE_MAX_LAYOUT_ID];
    char value[KAIN_UI_COMPILED_BUNDLE_MAX_TEXT];
    size_t value_length;
    size_t cursor;
} KainUiRuntimeComponentState;

typedef struct {
    int valid;
    int loaded;
    int overlay_compatible;
    int root_present;
    int focusable_count;
    int editable_count;
    int component_count;
    int error_count;
    int warning_count;
    unsigned int capability_flags;
    char summary[KAIN_UI_COMPILED_BUNDLE_MAX_TEXT];
    int issue_count;
    KainDiagnostic issues[KAIN_UI_RUNTIME_MAX_ISSUES];
} KainUiRuntimeValidationReport;

typedef struct {
    int initialized;
    int loaded;
    unsigned int capability_flags;
    unsigned long long focused_component_id;
    unsigned long long active_edit_component_id;
    unsigned long long hovered_component_id;
    unsigned int sequence;
    unsigned int dirty_component_count;
    KainUiCompiledBundle bundle;
    KainUiRuntimeValidationReport validation;
    int component_count;
    KainUiRuntimeComponentState components[KAIN_UI_RUNTIME_MAX_COMPONENTS];
} KainUiRuntimeState;

typedef struct {
    int preserve_focus;
    int preserve_active_edit_component;
    int preserve_hovered_component;
    int preserve_component_values;
    int preserve_dirty_state;
} KainUiRuntimeReloadOptions;

typedef struct {
    int attempted;
    int applied;
    int compatible;
    int preserved_focus;
    int preserved_active_edit_component;
    int preserved_hovered_component;
    int transferred_component_count;
    unsigned int previous_sequence;
    unsigned int next_sequence;
    char summary[KAIN_UI_COMPILED_BUNDLE_MAX_TEXT];
    KainUiRuntimeValidationReport validation;
} KainUiRuntimeReloadReport;

void kain_ui_runtime_state_init(KainUiRuntimeState* state);
void kain_ui_runtime_validation_init(KainUiRuntimeValidationReport* report);
void kain_ui_runtime_reload_options_init(KainUiRuntimeReloadOptions* options);
void kain_ui_runtime_reload_report_init(KainUiRuntimeReloadReport* report);
int kain_ui_runtime_validate_bundle(const KainUiCompiledBundle* bundle, KainUiRuntimeValidationReport* report);
int kain_ui_runtime_state_load_bundle(KainUiRuntimeState* state, const KainUiCompiledBundle* bundle);
int kain_ui_runtime_state_load_from_json(KainUiRuntimeState* state, const char* json);
int kain_ui_runtime_state_load_from_path(KainUiRuntimeState* state, const char* path);
int kain_ui_runtime_state_load_from_env(KainUiRuntimeState* state, const char* env_name);
int kain_ui_runtime_reload_bundle(
    KainUiRuntimeState* state,
    const KainUiCompiledBundle* bundle,
    const KainUiRuntimeReloadOptions* options,
    KainUiRuntimeReloadReport* report
);
int kain_ui_runtime_reload_from_path(
    KainUiRuntimeState* state,
    const char* path,
    const KainUiRuntimeReloadOptions* options,
    KainUiRuntimeReloadReport* report
);
const KainUiRuntimeComponentState* kain_ui_runtime_find_component(
    const KainUiRuntimeState* state,
    unsigned long long component_id
);
const KainUiRuntimeComponentState* kain_ui_runtime_find_first_kind(
    const KainUiRuntimeState* state,
    KainUiCompiledNodeKind kind
);
const KainUiRuntimeComponentState* kain_ui_runtime_find_first_focusable(const KainUiRuntimeState* state);
const KainUiRuntimeComponentState* kain_ui_runtime_find_first_editable(const KainUiRuntimeState* state);
int kain_ui_runtime_request_focus(KainUiRuntimeState* state, unsigned long long component_id);
int kain_ui_runtime_clear_focus(KainUiRuntimeState* state);
int kain_ui_runtime_mark_dirty(
    KainUiRuntimeState* state,
    unsigned long long component_id,
    unsigned int dirty_reason_mask
);
int kain_ui_runtime_route_event(
    KainUiRuntimeState* state,
    const KainUiRuntimeEvent* event,
    KainUiRuntimeEventResult* result
);
int kain_ui_runtime_has_capability(const KainUiRuntimeState* state, unsigned int capability_mask);
unsigned int kain_ui_runtime_state_capabilities(const KainUiRuntimeState* state);

#endif
