// ============================================================================
//  kain_input.h — Input Pipeline
//  ============================================================================
//  Thin typed wrapper over the existing abi_ui_push_event / abi_ui_poll_event
//  event queue in ui_system.c. Maps string-based event kinds to a typed enum,
//  provides hit-test delegation, and returns structured event records.
//
//  Part of the Kain UI substrate (KUIF Phase 1). Widget-free.
//  ============================================================================

#ifndef KAIN_INPUT_H
#define KAIN_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Event kinds ────────────────────────────────────────────────────
typedef enum KainInputEventKind {
    KAIN_INPUT_NONE = 0,
    KAIN_INPUT_KEY_DOWN,
    KAIN_INPUT_KEY_UP,
    KAIN_INPUT_TEXT,
    KAIN_INPUT_POINTER_DOWN,
    KAIN_INPUT_POINTER_UP,
    KAIN_INPUT_POINTER_MOVE,
    KAIN_INPUT_POINTER_WHEEL,
    KAIN_INPUT_FOCUS_IN,
    KAIN_INPUT_FOCUS_OUT,
    KAIN_INPUT_DRAG,
    KAIN_INPUT_DROP,
} KainInputEventKind;

// ── Structured event ───────────────────────────────────────────────
typedef struct KainInputEvent {
    KainInputEventKind kind;
    int64_t  key_code;           // platform key code or 0
    float    x, y;               // pointer position (client space)
    float    delta_x, delta_y;    // scroll delta or drag delta
    char     text[16];           // UTF-8 text for text input events
    int64_t  device_id;          // input device identifier
    int64_t  timestamp_ms;       // event timestamp
} KainInputEvent;

// ── Opaque pipeline (wraps ui_system event queue) ──────────────────
typedef struct KainInputPipeline KainInputPipeline;

// ── Lifecycle ──────────────────────────────────────────────────────
KainInputPipeline* kain_input_pipeline_create(int64_t session_id);
void               kain_input_pipeline_destroy(KainInputPipeline* p);

// ── Event collection ───────────────────────────────────────────────
// poll_event:  non-blocking poll. Returns true if an event was popped.
// push_event:  push an event into the queue (for synthetic/injected events).
bool kain_input_poll_event(KainInputPipeline* p, KainInputEvent* out_event);
void kain_input_push_event(KainInputPipeline* p, const KainInputEvent* event);

// ── Hit testing (delegates to abi_ui_hit_test) ─────────────────────
// Returns the node_id at (x,y) in the current session, or -1 if none.
int64_t kain_input_hit_test(KainInputPipeline* p, float x, float y);

// ── Utility ────────────────────────────────────────────────────────
const char* kain_input_event_type_name(KainInputEventKind kind);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_INPUT_H */
