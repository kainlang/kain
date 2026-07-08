// ============================================================================
//  kain_input.c — Input Pipeline Implementation
//  ============================================================================
//  Thin wrapper over the existing abi_ui_push_event / abi_ui_poll_event
//  ring buffer in ui_system.c. Maps string event kinds to the
//  KainInputEventKind enum and delegates hit-testing to abi_ui_hit_test.
//
//  Does NOT create new event infrastructure — delegates everything to
//  the existing ui_system ABI.
//
//  Part of the Kain UI substrate (KUIF Phase 1).
//  ============================================================================

#include "kain_input.h"
#include "../../include/ui_system.h"
#include <stdlib.h>
#include <string.h>

struct KainInputPipeline {
    int64_t session_id;
};

// ── Lifecycle ──────────────────────────────────────────────────────

KainInputPipeline* kain_input_pipeline_create(int64_t session_id) {
    KainInputPipeline* p = (KainInputPipeline*)calloc(1, sizeof(KainInputPipeline));
    if (p) p->session_id = session_id;
    return p;
}

void kain_input_pipeline_destroy(KainInputPipeline* p) {
    free(p);
}

// ── Event kind ↔ string mapping ───────────────────────────────────

static KainInputEventKind map_event_kind(const char* kind_str) {
    if (!kind_str) return KAIN_INPUT_NONE;
    if (strcmp(kind_str, "key_down")      == 0) return KAIN_INPUT_KEY_DOWN;
    if (strcmp(kind_str, "key_up")        == 0) return KAIN_INPUT_KEY_UP;
    if (strcmp(kind_str, "text")          == 0) return KAIN_INPUT_TEXT;
    if (strcmp(kind_str, "pointer_down")  == 0) return KAIN_INPUT_POINTER_DOWN;
    if (strcmp(kind_str, "pointer_up")    == 0) return KAIN_INPUT_POINTER_UP;
    if (strcmp(kind_str, "pointer_move")  == 0) return KAIN_INPUT_POINTER_MOVE;
    if (strcmp(kind_str, "axis")          == 0) return KAIN_INPUT_POINTER_WHEEL;
    if (strcmp(kind_str, "focus_in")      == 0) return KAIN_INPUT_FOCUS_IN;
    if (strcmp(kind_str, "focus_out")     == 0) return KAIN_INPUT_FOCUS_OUT;
    if (strcmp(kind_str, "drag")          == 0) return KAIN_INPUT_DRAG;
    if (strcmp(kind_str, "drop")          == 0) return KAIN_INPUT_DROP;
    return KAIN_INPUT_NONE;
}

static const char* map_event_kind_name(KainInputEventKind kind) {
    switch (kind) {
        case KAIN_INPUT_KEY_DOWN:      return "key_down";
        case KAIN_INPUT_KEY_UP:        return "key_up";
        case KAIN_INPUT_TEXT:          return "text";
        case KAIN_INPUT_POINTER_DOWN:  return "pointer_down";
        case KAIN_INPUT_POINTER_UP:    return "pointer_up";
        case KAIN_INPUT_POINTER_MOVE:  return "pointer_move";
        case KAIN_INPUT_POINTER_WHEEL: return "axis";
        case KAIN_INPUT_FOCUS_IN:      return "focus_in";
        case KAIN_INPUT_FOCUS_OUT:     return "focus_out";
        case KAIN_INPUT_DRAG:          return "drag";
        case KAIN_INPUT_DROP:          return "drop";
        default:                       return "none";
    }
}

// ── Event collection ───────────────────────────────────────────────

bool kain_input_poll_event(KainInputPipeline* p, KainInputEvent* out_event) {
    if (!p || !out_event) return false;

    if (abi_ui_poll_event(p->session_id)) {
        // Read current event from the ABI accessors
        const char* kind_str = abi_ui_event_kind(p->session_id);
        out_event->kind     = map_event_kind(kind_str);
        out_event->x        = (float)abi_ui_event_x(p->session_id);
        out_event->y        = (float)abi_ui_event_y(p->session_id);
        out_event->key_code = abi_ui_event_key_code(p->session_id);
        out_event->delta_x  = 0.0f;
        out_event->delta_y  = 0.0f;
        out_event->device_id    = 0;
        out_event->timestamp_ms = 0;

        const char* text = abi_ui_event_text(p->session_id);
        if (text && text[0]) {
            size_t maxlen = sizeof(out_event->text) - 1;
            strncpy(out_event->text, text, maxlen);
            out_event->text[maxlen] = '\0';
        } else {
            out_event->text[0] = '\0';
        }
        return true;
    }
    return false;
}

void kain_input_push_event(KainInputPipeline* p, const KainInputEvent* event) {
    if (!p || !event) return;

    const char* kind_str = map_event_kind_name(event->kind);
    abi_ui_push_event(
        p->session_id,
        kind_str,
        0,                      // target_node_id = 0 (no target filtering)
        (double)event->x,
        (double)event->y,
        event->key_code,
        event->text[0] ? event->text : NULL
    );
}

// ── Hit testing ────────────────────────────────────────────────────

int64_t kain_input_hit_test(KainInputPipeline* p, float x, float y) {
    if (!p) return -1;
    return abi_ui_hit_test(p->session_id, (double)x, (double)y);
}

// ── Utility ────────────────────────────────────────────────────────

const char* kain_input_event_type_name(KainInputEventKind kind) {
    return map_event_kind_name(kind);
}
