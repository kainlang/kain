// ============================================================================
//  tree.c — ABI ingestion, vtable implementation, and session lifecycle.
//  Section index: Hash | Arena | Vtable | Session | Frame | Element | Layout
//                 Style | State | Draw | Input | Backend | Internal
//
//  P0 WIRING (2026-07-05):
//    P0-1/P0-17: Virtual alloc for session struct, arena-alloc for all per-frame data
//    P0-8/9/10:  Input ABI session + action bindings + poll_event wire
//    P0-11:      Diagnostics emit for invalid attr, layout overflow, render errors
//    P0-14/15:   Handle table for node mapping + state key handles
//    P0-18:      ui.kaintana service registration
// ============================================================================
#include "internal.h"
#include <stdlib.h>
#include <float.h>
#include <string.h>
#include <stdio.h>
#include "diagnostics.h"
#include "version.h"
#include "hash_table.h"
#include "virtual_alloc.h"
#include "services.h"

// ── Named constants for capacities, limits, and defaults ──
#define KAINTANA_HEAP_COUNT 3
#define KAINTANA_NATIVE_SCALE_MIN 0.1f
#define KAINTANA_NATIVE_SCALE_MAX 10.0f
#define KAINTANA_DEFAULT_DELTA_MS 16.0
// NOTE: Should come from backend for multi-pointer support (future).
#define KAINTANA_DEFAULT_POINTER_ID "p0"

// P0-14: Handle kind for node mapping (generation-tagged)
#define KAINTANA_HANDLE_KIND_NODE 3

// PreUpdate=asc=1, Prepass=desc=-1, PostUpdate=asc=1
static const int kaintana_heap_sort_dir[KAINTANA_HEAP_COUNT] = {1, -1, 1};

// ── Session ID mapping for vtable slot dispatch ──
// Maps opaque vtable session_id (from slot 0 session_create) to
// kt_Session* pointers. Allows multiple sessions to coexist.
#define MAX_SESSIONS 8
static struct { int64_t sid; kt_Session* sess; } g_session_map[MAX_SESSIONS];
static int g_session_count = 0;

static kt_Session* session_by_sid(int64_t sid) {
    for (int i = 0; i < g_session_count; i++) {
        if (g_session_map[i].sid == sid) return g_session_map[i].sess;
    }
    return NULL;
}

static void session_register(int64_t sid, kt_Session* s) {
    if (g_session_count < MAX_SESSIONS) {
        g_session_map[g_session_count].sid = sid;
        g_session_map[g_session_count].sess = s;
        g_session_count++;
    }
}

static void session_unregister(int64_t sid) {
    for (int i = 0; i < g_session_count; i++) {
        if (g_session_map[i].sid == sid) {
            g_session_map[i] = g_session_map[--g_session_count];
            return;
        }
    }
}

// Hash table functions moved to hash_table.h / hash_table.c

// ============================================================================
//  SECTION 2: ARENA HELPERS
//  NOTE: kaintana__arena_push and kaintana__arena_reset are defined in arena.c.
//  This section is intentionally empty to avoid duplicate symbols.
// ============================================================================

// ============================================================================
//  SECTION 3: VTABLE SLOTS (forward declarations)
// ============================================================================
static int64_t v_session_create(const char* n, int64_t w, int64_t h);
static void    v_session_destroy(int64_t id);
static int64_t v_element_begin(int64_t sid, int64_t p, const char* k, const char* sk);
static void    v_element_end(int64_t sid, int64_t e);
static void    v_element_set_text(int64_t sid, int64_t e, const char* t);
static void    v_element_set_attr_i64(int64_t sid, int64_t e, const char* k, int64_t v);
static void    v_element_set_attr_f64(int64_t sid, int64_t e, const char* k, double v);
static void    v_element_set_attr_string(int64_t sid, int64_t e, const char* k, const char* v);
static int64_t v_state_get_i64(int64_t sid, const char* k);
static void    v_state_set_i64(int64_t sid, const char* k, int64_t v);
static void    v_begin_frame(int64_t sid, double d);
static void    v_end_frame(int64_t sid);
static void    v_present(int64_t sid);
static int64_t v_poll_event(int64_t sid, void* o, int64_t ms);
static int64_t v_should_close(int64_t sid);
static int64_t v_window_open(int64_t sid, const char* t, int64_t w, int64_t h);
static int64_t v_host_pump(int64_t sid);
static void    v_session_attach_platform(int64_t sid, void* h);
static const KainGpuSurfaceExtension* v_get_gpu_extension(int64_t sid);
static double  v_state_get_f64(int64_t sid, const char* k);
static void    v_state_set_f64(int64_t sid, const char* k, double v);
static const char* v_state_get_string(int64_t sid, const char* k);
static void    v_state_set_string(int64_t sid, const char* k, const char* v);
static void    v_element_set_callback(int64_t sid, int64_t e, const char* ev, void* fn);

// Stub definitions for slots 15-18, 23
static int64_t v_window_open(int64_t sid, const char* t, int64_t w, int64_t h)
    { (void)sid;(void)t;(void)w;(void)h; return 0; }
static int64_t v_host_pump(int64_t sid)               { (void)sid; return 0; }
static void    v_session_attach_platform(int64_t sid, void* h) { (void)sid;(void)h; }
static const KainGpuSurfaceExtension* v_get_gpu_extension(int64_t sid)
    { (void)sid; return NULL; }
static void    v_element_set_callback(int64_t sid, int64_t e, const char* ev, void* fn)
    { (void)sid;(void)e;(void)ev;(void)fn; }

// ── Vtable singleton — slot order is ABSOLUTE ─────────────────────────────
static const KaintanaComponentSurface kaintana_vtable = {
    .session_create=v_session_create, .session_destroy=v_session_destroy,
    .element_begin=v_element_begin, .element_end=v_element_end,
    .element_set_text=v_element_set_text,
    .element_set_attr_i64=v_element_set_attr_i64,
    .element_set_attr_f64=v_element_set_attr_f64,
    .element_set_attr_string=v_element_set_attr_string,
    .state_get_i64=v_state_get_i64, .state_set_i64=v_state_set_i64,
    .begin_frame=v_begin_frame, .end_frame=v_end_frame, .present=v_present,
    .poll_event=v_poll_event, .should_close=v_should_close,
    .window_open=v_window_open, .host_pump=v_host_pump,
    .session_attach_platform=v_session_attach_platform,
    .get_gpu_extension=v_get_gpu_extension,
    .state_get_f64=v_state_get_f64, .state_set_f64=v_state_set_f64,
    .state_get_string=v_state_get_string, .state_set_string=v_state_set_string,
    .element_set_callback=v_element_set_callback,
};

// ============================================================================
//  SECTION 4: SESSION LIFECYCLE
// ============================================================================
void kt_init(void) {
    version_check_abi_compatibility((unsigned int)KT_API_VERSION);
    kain_component_surface_register(KAINTANA_SURFACE_NAME, &kaintana_vtable);
    // P0-18: Register ui.kaintana service
    KainServiceRegistry* reg = kain_service_registry_global();
    kain_service_registry_register(reg,
        "ui.kaintana", "Kaintana UI System",
        "Kaintana native UI substrate — element tree, layout, damage, rendering",
        KAIN_SERVICE_PROVIDER_NATIVE_CORE, KAIN_SERVICE_STATUS_AVAILABLE,
        KAIN_SERVICE_REQUIREMENT_OPTIONAL, RUNTIME_ABI_VERSION_CURRENT, NULL);
}

kt_Session* kt_make(const char* name, int w, int h) {
    size_t sess_size = sizeof(struct kt_Session_t);
    struct kt_Session_t* sess = (struct kt_Session_t*)
        kain_virtual_reserve_and_commit(sess_size, 64, KAIN_MEMTYPE_DEFAULT);
    if (!sess) return NULL;
    memset(sess, 0, sess_size);

    kain_arena_init(&sess->arena, KAIN_ARENA_MAIN, sess->arena_buffer,
                     sizeof(sess->arena_buffer), KAIN_MEMTYPE_DEFAULT);

    sess->node_capacity = KAINTANA_MAX_NODES;
    sess->nodes = kain_arena_alloc_lo(&sess->arena,
        sess->node_capacity * sizeof(KaintanaNode), _Alignof(KaintanaNode));
    sess->node_count = 1;
    // Root node (index 0) — must initialize first_child/next_sibling to -1
    // to prevent infinite loops in sibling traversal (nodes[0] is zero from memset
    // which gives 0 as a valid index, causing kt_row's while loop to spin forever).
    sess->nodes[0].first_child = -1;
    sess->nodes[0].next_sibling = -1;
    sess->nodes[0].parent_index = -1;
    sess->nodes[0].flags |= KT_NODE_VISIBLE;

    sess->layout_capacity = KAINTANA_MAX_NODES;
    sess->layouts = kain_arena_alloc_lo(&sess->arena,
        sess->layout_capacity * sizeof(KaintanaLayout), _Alignof(KaintanaLayout));
    sess->layout_count = 1;

    sess->layout_caches = kain_arena_alloc_lo(&sess->arena,
        sess->node_capacity * sizeof(KaintanaLayoutCache), _Alignof(KaintanaLayoutCache));
    memset(sess->layout_caches, 0, sess->node_capacity * sizeof(KaintanaLayoutCache));
    sess->layout_generation = 1;

    memset(sess->hash_slots, 0, sizeof(sess->hash_slots));
    memset(sess->hash_values, 0xFF, sizeof(sess->hash_values));
    sess->hash_occupied_count = 0;
    kain_handle_table_init(&sess->handle_table, sess->handle_slots, KAINTANA_HASH_SLOTS);

    sess->elem_stack.depth = -1;
    for (int i = 0; i < KAINTANA_HEAP_COUNT; i++) {
        sess->heaps[i].indices = NULL; sess->heaps[i].count = 0;
        sess->heaps[i].capacity = 0; sess->heaps[i].sort_dir = kaintana_heap_sort_dir[i];
    }
    sess->draw_batch.buf = NULL; sess->draw_batch.count = 0;
    sess->draw_batch.capacity = 0; sess->draw_batch.write_ptr = NULL;
    memset(&sess->draw_batch.last, 0, sizeof(sess->draw_batch.last));
    sess->damage.count = 0; sess->damage.overflowed = false;
    sess->state_count = 0;
    memset(&sess->input, 0, sizeof(sess->input));
    sess->input.active_id = -1; sess->input.hovered_id = -1;
    sess->input.clicked_id = -1;
    for (int _b = 0; _b < 5; _b++) sess->input.click_press_node[_b] = -1;
    sess->vtable_session_id = 0;
    sess->frame_number = 0; sess->frame_delta_ms = KAINTANA_DEFAULT_DELTA_MS; sess->frame_time_ms = 0.0;

    // -- DPI & scaling initialization --
    sess->native_scale_x = KT_DEFAULT_SCALE;
    sess->native_scale_y = KT_DEFAULT_SCALE;
    sess->user_zoom = 1.0f;
    sess->scale_changed = false;
    // CRITICAL: assign vtable BEFORE calling session_create
    sess->vtable = &kaintana_vtable;
    sess->vtable_session_id = sess->vtable->session_create(name, w, h);
    session_register(sess->vtable_session_id, (kt_Session*)sess);

    // P0-8: Create input session for ABI-bound event processing
    sess->input_sid = abi_input_session_create("kaintana");

    // Store backend config for automatic init() in kt_backend_select
    sess->backend_config.title = name;
    sess->backend_config.width = w;
    sess->backend_config.height = h;
    sess->window_width = w;
    sess->window_height = h;
    sess->should_close = 0;
    sess->backend_config.fullscreen = 0;
    sess->backend_config.platform_handle = NULL;

    return (kt_Session*)sess;
}

void kt_free(kt_Session* s) {
    if (!s) return;
    struct kt_Session_t* sess = kaintana__session(s);
    if (sess->vtable && sess->vtable_session_id)
        sess->vtable->session_destroy(sess->vtable_session_id);
    if (sess->input_sid)
        abi_input_session_destroy(sess->input_sid);
    session_unregister(sess->vtable_session_id);
    kain_virtual_release(sess, sizeof(struct kt_Session_t));
}

static int64_t v_session_create(const char* n, int64_t w, int64_t h)
    { (void)n;(void)w;(void)h; return 1; }
static void v_session_destroy(int64_t id) { (void)id; }

// ============================================================================
//  SECTION 5: FRAME LOOP
// ============================================================================
void kt_begin(kt_Session* s, double delta_ms) {
    struct kt_Session_t* sess = kaintana__session(s);

    // ── Reset node tree links (prevent sibling-cycle hang on frame reuse) ─
    // Tree structure is rebuilt fresh each frame. Stable-key nodes persist
    // but their parent/child/sibling links from the previous frame must be
    // cleared to avoid kt_row's sibling walker creating infinite loops.
    sess->nodes[0].first_child = -1;
    for (int i = 1; i < sess->node_count; i++) {
        sess->nodes[i].parent_index = -1;
        sess->nodes[i].first_child  = -1;
        sess->nodes[i].next_sibling = -1;
    }
    sess->frame_delta_ms = delta_ms;
    sess->frame_time_ms += delta_ms;        // Accumulate running time
    sess->frame_number++;
    sess->layout_generation++;
    // Propagate frame timing to input state
    sess->input.delta_ms = delta_ms;
    sess->input.time_ms = sess->frame_time_ms;

    // Clear per-frame text input buffer (accumulated via kt_input_text)
    memset(sess->input.text_input, 0, sizeof(sess->input.text_input));
    sess->input.text_len = 0;

    // ── DPI scale change invalidation ───────────────────────
    if (sess->scale_changed) {
        sess->layout_generation++;
        sess->scale_changed = false;
    }
    sess->elem_stack.depth = -1;           // reset nesting tracker
    sess->damage.count = 0;                // reset damage accumulator
    sess->damage.overflowed = false;
    kaintana__arena_mark(s);
    if (sess->input_sid)
        abi_input_begin_frame(sess->input_sid, delta_ms);
}

void kt_end(kt_Session* s) {
    (void)s;  // sess not needed — all kaintana__* calls take s directly
    kaintana__damage_process(s);
    kaintana__layout_pass1(s);
    kaintana__layout_pass2(s);
    kaintana__hit_test(s);          // pointer→node matching after layout, before draw
    kaintana__draw_generate(s);
    kaintana__draw_merge(s);
    kaintana__arena_release(s);  // AFTER vtable end_frame
}

void kt_present(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (sess->backend && sess->backend->render) {
        // Convert internal draw commands to public kt_Cmd format
        // (KaintanaInternalDrawCmd != kt_Cmd — different layout/sizes)
        int count = sess->draw_batch.count;
        kt_Cmd* cmds = NULL;
        if (count > 0) {
            cmds = (kt_Cmd*)kaintana__arena_alloc(s,
                (size_t)count * sizeof(kt_Cmd), _Alignof(kt_Cmd));
            if (cmds) {
                for (int i = 0; i < count; i++) {
                    KaintanaInternalDrawCmd* ic = &sess->draw_batch.buf[i];
                    kt_Cmd* cmd = &cmds[i];
                    memset(cmd, 0, sizeof(kt_Cmd));
                    cmd->type       = (kt_CmdType)(ic->type < 6 ? ic->type : 0);
                    cmd->bounds.x   = (float)ic->x;
                    cmd->bounds.y   = (float)ic->y;
                    cmd->bounds.w   = (float)ic->w;
                    cmd->bounds.h   = (float)ic->h;
                    cmd->color      = ic->color;
                    cmd->color_b    = ic->color_b;
                    cmd->radius     = (float)ic->corner_radius / 256.0f;
                    cmd->thickness  = 0.0f;
                    cmd->text_id    = ic->data_offset;
                    cmd->image_id   = ic->texture_handle;
                }
            }
        }
        sess->draw_data.cmds = cmds ? cmds : (const kt_Cmd*)&sess->draw_batch.last;
        sess->draw_data.cmd_count = count;
        sess->backend->render(&sess->draw_data);
    }
}

int kt_should_close(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    return sess->should_close ? 1 : 0;
}

static void v_begin_frame(int64_t sid, double d) {
    kt_Session* s = session_by_sid(sid);
    if (s) kt_begin(s, d);
}
static void v_end_frame(int64_t sid) {
    kt_Session* s = session_by_sid(sid);
    if (s) kt_end(s);
}
static void v_present(int64_t sid) {
    kt_Session* s = session_by_sid(sid);
    if (s) kt_present(s);
}
static int64_t v_should_close(int64_t sid) {
    kt_Session* s = session_by_sid(sid);
    return s ? (int64_t)kt_should_close(s) : 0;
}

// ============================================================================
//  SECTION 5B: DPI & SCALE FACTOR
// ============================================================================

float kt_scale_factor_x(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess) return 1.0f;
    return sess->native_scale_x * sess->user_zoom;
}

float kt_scale_factor_y(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess) return 1.0f;
    return sess->native_scale_y * sess->user_zoom;
}

float kt_native_scale_x(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess) return 1.0f;
    return sess->native_scale_x;
}

float kt_native_scale_y(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess) return 1.0f;
    return sess->native_scale_y;
}

void kt_set_native_scale(kt_Session* s, float sx, float sy) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess) return;
    sess->native_scale_x = fmaxf(KAINTANA_NATIVE_SCALE_MIN, fminf(KAINTANA_NATIVE_SCALE_MAX, sx));
    sess->native_scale_y = fmaxf(KAINTANA_NATIVE_SCALE_MIN, fminf(KAINTANA_NATIVE_SCALE_MAX, sy));
    sess->scale_changed = true;
}

void kt_set_zoom(kt_Session* s, float zoom) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (!sess) return;
    sess->user_zoom = fmaxf(KT_ZOOM_MIN, fminf(KT_ZOOM_MAX, zoom));
    sess->scale_changed = true;
}
// ============================================================================
//  SECTION 6: ELEMENT TREE
// ============================================================================
// P0-11: Helper to emit a UI diagnostic
static void kaintana__diag_ui(int code, KainDiagSeverity sev,
                               const char* msg, const char* detail)
{
    KainDiagnostic diag;
    kain_diagnostic_init(&diag);
    kain_diagnostic_create(&diag, KAIN_DIAG_SUBSYSTEM_UI, sev,
        code, msg, detail, __FILE__);
    kain_diagnostic_print(&diag);
}

static int32_t node_alloc(struct kt_Session_t* sess) {
    if (sess->node_count >= sess->node_capacity) {
        // P0-11: Emit layout overflow diagnostic
        kaintana__diag_ui(KT_DIAG_CODE_UI_LAYOUT_OVERFLOW, KAIN_DIAG_SEVERITY_ERROR,
            "Node capacity exhausted — increase KAINTANA_MAX_NODES or reduce element count",
            "node_alloc failure");
        return -1;
    }
    int32_t idx = sess->node_count++;
    KaintanaNode* n = &sess->nodes[idx];
    memset(n, 0, sizeof(KaintanaNode));
    n->parent_index = -1; n->first_child = -1; n->next_sibling = -1;
    n->flags |= KT_NODE_VISIBLE;
    // Allocate layout index immediately so attr setters (called during
    // tree building) can write to KaintanaLayout fields.
    // Previously deferred to kaintana__layout_pass1() at kt_end(),
    // which caused v_element_set_attr_f64 to early-return (BUG-008).
    n->layout_arena_index = sess->layout_count++;
    if (n->layout_arena_index >= sess->layout_capacity) {
        n->layout_arena_index = -1;
    } else {
        memset(&sess->layouts[n->layout_arena_index], 0, sizeof(KaintanaLayout));
        // Set non-zero defaults
        sess->layouts[n->layout_arena_index].opacity = 1.0f;
    }
    n->state_payload_offset = -1;
    return idx;
}

int kt_row(kt_Session* s, int parent, const char* kind, const char* key) {
    struct kt_Session_t* sess = kaintana__session(s);
    (void)kind;
    int32_t idx;
    if (key && key[0]) {
        uint64_t h = kaintana_hash_fnv1a(key);
        idx = kaintana__hash_lookup(s, h);
        if (idx < 0) {
            idx = node_alloc(sess); if (idx < 0) return -1;
            sess->nodes[idx].stable_key_hash = h;
            // P0-14: Acquire generation-tagged handle for stable-key node
            KainRuntimeHandle node_h = kain_handle_table_acquire(
                &sess->handle_table, KAINTANA_HANDLE_KIND_NODE, &sess->nodes[idx]);
            (void)node_h;  // Handle stored in handle table for future resolve
            kaintana__hash_insert(s, h, idx);
        }
    } else {
        idx = node_alloc(sess); if (idx < 0) return -1;
    }

    KaintanaNode* n = &sess->nodes[idx];
    if (parent >= 0 && parent < sess->node_count) {
        n->parent_index = parent;
        KaintanaNode* p = &sess->nodes[parent];
        if (p->first_child < 0) {
            p->first_child = idx;
        } else {
            int32_t last = p->first_child;
            while (sess->nodes[last].next_sibling >= 0)
                last = sess->nodes[last].next_sibling;
            sess->nodes[last].next_sibling = idx;
        }
    }

    if (sess->elem_stack.depth < KAINTANA_MAX_DEPTH - 1)
        sess->elem_stack.stack[++sess->elem_stack.depth] = idx;
    n->invalidation_flags |= KT_INVALIDATE_CHILD_ORDER;
    return idx;
}

void kt_end_row(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (sess->elem_stack.depth >= 0) sess->elem_stack.depth--;
}

void kt_text(kt_Session* s, int elem, const char* text) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (elem < 0 || elem >= sess->node_count) return;
    KaintanaNode* n = &sess->nodes[(int32_t)elem];
    if (n->layout_arena_index < 0) return;
    KaintanaLayout* l = &sess->layouts[n->layout_arena_index];
    if (!text) text = "";
    size_t len = strlen(text) + 1;
    char* dst = (char*)kaintana__arena_alloc(s, len, 1);
    if (!dst) return;
    memcpy(dst, text, len);
    l->text_content = dst;
}

static int64_t v_element_begin(int64_t sid, int64_t p, const char* k, const char* sk) {
    kt_Session* s = session_by_sid(sid);
    if (!s) return -1;
    return kt_row(s, (int)p, k, sk ? sk : "");
}
static void v_element_end(int64_t sid, int64_t e) {
    (void)e;
    kt_Session* s = session_by_sid(sid);
    if (s) kt_end_row(s);
}
static void v_element_set_text(int64_t sid, int64_t e, const char* t) {
    kt_Session* s = session_by_sid(sid);
    if (s) kt_text(s, (int)e, t);
}

// ============================================================================
//  SECTION 7: LAYOUT + STYLE ATTRIBUTES
// ============================================================================
#define ATTR_KV(s,e,k,v) do { struct kt_Session_t* _s=kaintana__session(s); \
    if(_s->vtable&&_s->vtable_session_id) \
        _s->vtable->element_set_attr_i64(_s->vtable_session_id,(e),(k),(v)); }while(0)
#define ATTR_KF(s,e,k,v) do { struct kt_Session_t* _s=kaintana__session(s); \
    if(_s->vtable&&_s->vtable_session_id) \
        _s->vtable->element_set_attr_f64(_s->vtable_session_id,(e),(k),(v)); }while(0)
#define ATTR_KS(s,e,k,v) do { struct kt_Session_t* _s=kaintana__session(s); \
    if(_s->vtable&&_s->vtable_session_id) \
        _s->vtable->element_set_attr_string(_s->vtable_session_id,(e),(k),(v)); }while(0)

void kt_width(kt_Session* s, int e, float v)     { ATTR_KF(s,e,"layout.width",v); }
void kt_height(kt_Session* s, int e, float v)    { ATTR_KF(s,e,"layout.height",v); }
void kt_pad(kt_Session* s, int e, float v)       { ATTR_KF(s,e,"layout.pad",v); }
void kt_pad_xy(kt_Session* s, int e, float x, float y) {
    ATTR_KF(s,e,"layout.pad_x",x); ATTR_KF(s,e,"layout.pad_y",y);
}
void kt_gap(kt_Session* s, int e, float v)       { ATTR_KF(s,e,"layout.gap",v); }
void kt_direction(kt_Session* s, int e, int v)   { ATTR_KV(s,e,"layout.dir",v); }
void kt_fill(kt_Session* s, int e, const char* c){ ATTR_KS(s,e,"fill",c); }
void kt_stroke(kt_Session* s, int e, const char* c, float w) {
    ATTR_KS(s,e,"stroke",c); ATTR_KF(s,e,"stroke_width",w);
}
void kt_radius(kt_Session* s, int e, float v)    { ATTR_KF(s,e,"radius",v); }
void kt_opacity(kt_Session* s, int e, float v)   { ATTR_KF(s,e,"opacity",v); }
void kt_font(kt_Session* s, int e, float v)      { ATTR_KF(s,e,"font_size",v); }

static void v_element_set_attr_i64(int64_t sid, int64_t e, const char* k, int64_t v) {
    kt_Session* s = session_by_sid(sid);
    if (!s) return;
    int idx = kaintana__attr_lookup(k);
    if (idx < 0) {
        // P0-11: Emit invalid attribute diagnostic for unknown i64 attribute
        kaintana__diag_ui(KT_DIAG_CODE_UI_INVALID_ATTRIBUTE, KAIN_DIAG_SEVERITY_WARNING,
            "Unknown integer attribute", k);
        return;
    }
    const KaintanaAttrEntry* entry = kaintana__attr_get_entry(idx);
    if (!entry) return;
    kaintana__node_mark_dirty(s, (int)e, entry->invalidation);

    // Visibility: store directly on node
    if (strcmp(k, "visibility") == 0) {
        KaintanaNode* node = kaintana__node(s, (int32_t)e);
        if (node) node->visibility_flags = (uint8_t)(v & 0xFF);
        return;
    }

    // Interactive: set/clear KT_NODE_INTERACTIVE flag
    if (strcmp(k, "interactive") == 0) {
        KaintanaNode* node = kaintana__node(s, (int32_t)e);
        if (node) {
            if (v) node->flags |= KT_NODE_INTERACTIVE;
            else   node->flags &= (uint8_t)~KT_NODE_INTERACTIVE;
        }
        return;
    }

    // Layout integer attributes — store directly on KaintanaLayout fields
    // (layout.dir, layout.justify, layout.align, layout.wrap)
    KaintanaNode* node = kaintana__node(s, (int32_t)e);
    if (!node || node->layout_arena_index < 0) return;
    KaintanaLayout* l = kaintana__layout(s, node->layout_arena_index);
    if (!l) return;

    if (strcmp(k, "layout.dir") == 0) {
        l->direction = (int8_t)(v & 0x03);  // 0-3 valid
    } else if (strcmp(k, "layout.justify") == 0) {
        l->justify_content = (int8_t)(v & 0x07);  // 0-5 valid
    } else if (strcmp(k, "layout.align") == 0) {
        l->align_items = (int8_t)(v & 0x07);  // 0-5 valid
    }
    // layout.wrap, text_align: no dedicated field yet
}

static void v_element_set_attr_f64(int64_t sid, int64_t e, const char* k, double v) {
    kt_Session* s = session_by_sid(sid);
    if (!s) return;
    int idx = kaintana__attr_lookup(k);
    if (idx < 0) {
        // P0-11: Emit invalid attribute diagnostic for unknown f64 attribute
        kaintana__diag_ui(KT_DIAG_CODE_UI_INVALID_ATTRIBUTE, KAIN_DIAG_SEVERITY_WARNING,
            "Unknown float attribute", k);
        return;
    }
    const KaintanaAttrEntry* entry = kaintana__attr_get_entry(idx);
    if (!entry) return;
    kaintana__node_mark_dirty(s, (int)e, entry->invalidation);

    // Get layout pointer for layout field writes
    KaintanaNode* node = kaintana__node(s, (int32_t)e);
    if (!node) return;
    // layout_arena_index is now allocated in node_alloc() (BUG-008 fix).
    // Still guard defensively in case of capacity exhaustion.
    if (node->layout_arena_index < 0) return;
    KaintanaLayout* l = kaintana__layout(s, node->layout_arena_index);
    if (!l) return;

    // Dispatch by key name — map to KaintanaLayout fields
    if (strcmp(k, "layout.flex_grow") == 0) {
        l->flex_grow = (float)v;
    } else if (strcmp(k, "layout.flex_shrink") == 0) {
        l->flex_shrink = (float)v;
    } else if (strcmp(k, "layout.flex_basis") == 0) {
        l->flex_basis = (float)v;
    } else if (strcmp(k, "layout.pad") == 0) {
        float f = (float)v;
        l->pad_left = l->pad_right = l->pad_top = l->pad_bottom = f;
    } else if (strcmp(k, "layout.pad_x") == 0) {
        float f = (float)v;
        l->pad_left = l->pad_right = f;
    } else if (strcmp(k, "layout.pad_y") == 0) {
        float f = (float)v;
        l->pad_top = l->pad_bottom = f;
    } else if (strcmp(k, "layout.margin") == 0) {
        float f = (float)v;
        l->margin_left = l->margin_right = l->margin_top = l->margin_bottom = f;
    } else if (strcmp(k, "layout.margin_left") == 0) {
        l->margin_left = (float)v;
    } else if (strcmp(k, "layout.margin_right") == 0) {
        l->margin_right = (float)v;
    } else if (strcmp(k, "layout.margin_top") == 0) {
        l->margin_top = (float)v;
    } else if (strcmp(k, "layout.margin_bottom") == 0) {
        l->margin_bottom = (float)v;
    } else if (strcmp(k, "layout.min_width") == 0) {
        l->min_width = (float)v;
    } else if (strcmp(k, "layout.max_width") == 0) {
        l->max_width = (float)v;
    } else if (strcmp(k, "layout.min_height") == 0) {
        l->min_height = (float)v;
    } else if (strcmp(k, "layout.max_height") == 0) {
        l->max_height = (float)v;
    } else if (strcmp(k, "layout.width") == 0) {
        // Fixed width = set both min and max
        l->min_width = l->max_width = (float)v;
    } else if (strcmp(k, "layout.height") == 0) {
        // Fixed height = set both min and max
        l->min_height = l->max_height = (float)v;
    } else if (strcmp(k, "opacity") == 0) {
        l->opacity = (float)v;
    } else if (strcmp(k, "radius") == 0) {
        l->corner_radius = (float)v;
    } else if (strcmp(k, "stroke_width") == 0) {
        l->stroke_width = (float)v;
}
} // End of v_element_set_attr_f64 function body

static void v_element_set_attr_string(int64_t sid, int64_t e, const char* k, const char* v) {
    kt_Session* s = session_by_sid(sid);
    if (!s) return;
    int idx = kaintana__attr_lookup(k);
    if (idx < 0) {
        // P0-11: Emit invalid attribute diagnostic for unknown string attribute
        kaintana__diag_ui(KT_DIAG_CODE_UI_INVALID_ATTRIBUTE, KAIN_DIAG_SEVERITY_WARNING,
            "Unknown string attribute", k);
        return;
    }
    const KaintanaAttrEntry* entry = kaintana__attr_get_entry(idx);
    if (!entry) return;

    // Store parsed colors in layout for use by draw_generate
    if (strcmp(k, "fill") == 0) {
        uint32_t parsed = kt_color_parse_hex(v);
        KaintanaNode* node = kaintana__node(s, (int32_t)e);
        if (node && node->layout_arena_index >= 0) {
            KaintanaLayout* l = kaintana__layout(s, node->layout_arena_index);
            if (l) l->fill_color = parsed;
        }
    } else if (strcmp(k, "stroke") == 0) {
        uint32_t parsed = kt_color_parse_hex(v);
        KaintanaNode* node = kaintana__node(s, (int32_t)e);
        if (node && node->layout_arena_index >= 0) {
            KaintanaLayout* l = kaintana__layout(s, node->layout_arena_index);
            if (l) l->stroke_color = parsed;
        }
    }

    kaintana__node_mark_dirty(s, (int)e, entry->invalidation);
}

// ============================================================================
//  SECTION 8: STATE PERSISTENCE
// ============================================================================
static KaintanaStateEntry* state_find(struct kt_Session_t* sess, const char* key) {
    uint64_t kh = kaintana_hash_fnv1a(key);
    for (int i = 0; i < sess->state_count; i++)
        if (kaintana_hash_fnv1a(sess->state_entries[i].key) == kh
            && strcmp(sess->state_entries[i].key, key) == 0)
            return &sess->state_entries[i];
    if (sess->state_count >= KAINTANA_STATE_ENTRIES) return NULL;
    KaintanaStateEntry* e = &sess->state_entries[sess->state_count++];
    strncpy(e->key, key, sizeof(e->key)-1);
    e->key[sizeof(e->key)-1] = '\0';
    return e;
}

void kt_put(kt_Session* s, const char* k, int64_t v)
    { struct kt_Session_t* sess=kaintana__session(s); KaintanaStateEntry* e=state_find(sess,k); if(e){e->type=0;e->data.i64_val=v; if(sess->elem_stack.depth>=0) kaintana__node_mark_dirty(s,sess->elem_stack.stack[sess->elem_stack.depth],KT_INVALIDATE_LAYOUT|KT_INVALIDATE_PAINT);} }
void kt_put_f(kt_Session* s, const char* k, double v)
    { struct kt_Session_t* sess=kaintana__session(s); KaintanaStateEntry* e=state_find(sess,k); if(e){e->type=1;e->data.f64_val=v; if(sess->elem_stack.depth>=0) kaintana__node_mark_dirty(s,sess->elem_stack.stack[sess->elem_stack.depth],KT_INVALIDATE_LAYOUT|KT_INVALIDATE_PAINT);} }
void kt_put_s(kt_Session* s, const char* k, const char* v) {
    struct kt_Session_t* sess=kaintana__session(s);
    KaintanaStateEntry* e=state_find(sess,k);
    if(e){e->type=2;strncpy(e->data.str_val,v,sizeof(e->data.str_val)-1);
          e->data.str_val[sizeof(e->data.str_val)-1]='\0';
          if(sess->elem_stack.depth>=0) kaintana__node_mark_dirty(s,sess->elem_stack.stack[sess->elem_stack.depth],KT_INVALIDATE_LAYOUT|KT_INVALIDATE_PAINT);}
}
int64_t kt_get(kt_Session* s, const char* k, int64_t fb) {
    struct kt_Session_t* ss=kaintana__session(s); uint64_t kh=kaintana_hash_fnv1a(k);
    for(int i=0;i<ss->state_count;i++){KaintanaStateEntry* e=&ss->state_entries[i];
        if(e->type==0&&kaintana_hash_fnv1a(e->key)==kh&&strcmp(e->key,k)==0)return e->data.i64_val;}
    return fb;
}
double kt_get_f(kt_Session* s, const char* k, double fb) {
    struct kt_Session_t* ss=kaintana__session(s); uint64_t kh=kaintana_hash_fnv1a(k);
    for(int i=0;i<ss->state_count;i++){KaintanaStateEntry* e=&ss->state_entries[i];
        if(e->type==1&&kaintana_hash_fnv1a(e->key)==kh&&strcmp(e->key,k)==0)return e->data.f64_val;}
    return fb;
}
const char* kt_get_s(kt_Session* s, const char* k, const char* fb) {
    struct kt_Session_t* ss=kaintana__session(s); uint64_t kh=kaintana_hash_fnv1a(k);
    for(int i=0;i<ss->state_count;i++){KaintanaStateEntry* e=&ss->state_entries[i];
        if(e->type==2&&kaintana_hash_fnv1a(e->key)==kh&&strcmp(e->key,k)==0)return e->data.str_val;}
    return fb;
}

static int64_t v_state_get_i64(int64_t sid, const char* k) {
    kt_Session* s = session_by_sid(sid);
    return s ? kt_get(s, k, 0) : 0;
}
static void v_state_set_i64(int64_t sid, const char* k, int64_t v) {
    kt_Session* s = session_by_sid(sid);
    if (s) kt_put(s, k, v);
}
static double v_state_get_f64(int64_t sid, const char* k) {
    kt_Session* s = session_by_sid(sid);
    return s ? kt_get_f(s, k, 0.0) : 0.0;
}
static void v_state_set_f64(int64_t sid, const char* k, double v) {
    kt_Session* s = session_by_sid(sid);
    if (s) kt_put_f(s, k, v);
}
static const char* v_state_get_string(int64_t sid, const char* k) {
    kt_Session* s = session_by_sid(sid);
    return s ? kt_get_s(s, k, "") : "";
}
static void v_state_set_string(int64_t sid, const char* k, const char* v) {
    kt_Session* s = session_by_sid(sid);
    if (s) kt_put_s(s, k, v);
}

// ============================================================================
//  SECTION 9: DRAW OUTPUT
// ============================================================================
int kt_cmd_count(kt_Session* s) { return kaintana__session(s)->draw_batch.count; }

kt_Cmd kt_cmd_get(kt_Session* s, int index) {
    struct kt_Session_t* ss = kaintana__session(s);
    kt_Cmd cmd; memset(&cmd, 0, sizeof(cmd));
    if (index >= 0 && index < ss->draw_batch.count) {
        KaintanaInternalDrawCmd* ic = &ss->draw_batch.buf[index];
        cmd.type = (kt_CmdType)(ic->type < 6 ? ic->type : 0);
        cmd.bounds.x = (float)ic->x; cmd.bounds.y = (float)ic->y;
        cmd.bounds.w = (float)ic->w; cmd.bounds.h = (float)ic->h;
        cmd.color = ic->color; cmd.color_b = ic->color_b;
        cmd.radius = (float)ic->corner_radius / 256.0f;
        cmd.text_id = ic->data_offset; cmd.image_id = ic->texture_handle;
    }
    return cmd;
}

// ============================================================================
//  SECTION 10: INPUT FUNNEL
// ============================================================================
void kt_input_mouse_move(kt_Session* s, float x, float y) {
    struct kt_Session_t* ss = kaintana__session(s);
    ss->input.mouse_x = x; ss->input.mouse_y = y;
    if (ss->input_sid)
        abi_input_push_event(ss->input_sid, "pointer", KAINTANA_DEFAULT_POINTER_ID, "move", "", (double)x, "", (double)y);
}
void kt_input_mouse_down(kt_Session* s, int btn) {
    struct kt_Session_t* ss = kaintana__session(s);
    if (btn>=0&&btn<5) {
        if (ss->input.mouse_down[btn] == 0) {
            ss->input.mouse_down[btn]=1;
            ss->input.mouse_pressed_this_frame[btn]=1;
        }
    }
    if (ss->input_sid) { char b[4]; snprintf(b,sizeof(b),"b%d",btn);
        abi_input_push_event(ss->input_sid,"pointer", KAINTANA_DEFAULT_POINTER_ID, "press", b, 1.0, "", 0.0); }
}
void kt_input_mouse_up(kt_Session* s, int btn) {
    struct kt_Session_t* ss = kaintana__session(s);
    if (btn>=0&&btn<5) {
        if (ss->input.mouse_down[btn] == 1) {
            ss->input.mouse_down[btn]=0;
            ss->input.mouse_released_this_frame[btn]=1;
        }
    }
    if (ss->input_sid) { char b[4]; snprintf(b,sizeof(b),"b%d",btn);
        abi_input_push_event(ss->input_sid,"pointer", KAINTANA_DEFAULT_POINTER_ID, "release", b, 0.0, "", 0.0); }
}
void kt_input_scroll(kt_Session* s, float dx, float dy) {
    struct kt_Session_t* ss = kaintana__session(s);
    ss->input.scroll_dx+=dx; ss->input.scroll_dy+=dy;
    if (ss->input_sid)
        abi_input_push_event(ss->input_sid,"pointer", KAINTANA_DEFAULT_POINTER_ID, "wheel", "", (double)dx, "", (double)dy);
}
void kt_input_key_down(kt_Session* s, int k) { if(k>=0&&k<256)kaintana__session(s)->input.keys_down[k]=1; }
void kt_input_key_up(kt_Session* s, int k)   { if(k>=0&&k<256)kaintana__session(s)->input.keys_down[k]=0; }
void kt_input_text(kt_Session* s, const char* t) {
    struct kt_Session_t* ss = kaintana__session(s);
    // Append to buffer, capped at remaining space. Buffer cleared each frame in kt_begin().
    int remaining = (int)sizeof(ss->input.text_input) - 1 - ss->input.text_len;
    if (remaining <= 0) return;
    size_t len = strlen(t);
    size_t cp = (len < (size_t)remaining) ? len : (size_t)remaining;
    memcpy(ss->input.text_input + ss->input.text_len, t, cp);
    ss->input.text_len += (int)cp;
    ss->input.text_input[ss->input.text_len] = '\0';
}
static int64_t v_poll_event(int64_t sid, void* o, int64_t ms) { (void)sid;(void)o;(void)ms; return 0; }

// ============================================================================
//  SECTION 11: BACKEND REGISTRY
// ============================================================================
#define MAX_BACKENDS 8
static struct { const char* name; KaintanaBackendVTable vtable; } catalog[MAX_BACKENDS];
static int cat_count = 0;

int kt_backend_register(kt_Session* s, const char* n, const KaintanaBackendVTable* vt) {
    (void)s; if (cat_count>=MAX_BACKENDS) return 0;
    catalog[cat_count].name = n; catalog[cat_count].vtable = *vt; cat_count++; return 1;
}
int kt_backend_select(kt_Session* s, const char* n) {
    struct kt_Session_t* sess = kaintana__session(s);
    for (int i=0;i<cat_count;i++) {
        if (strcmp(catalog[i].name,n)==0) {
            sess->backend=&catalog[i].vtable;
            // Automatically call the backend's init() with session config
            // Pass session pointer so backends can call kt_set_native_scale()
            sess->backend_config.platform_handle = (void*)s;
            if (sess->backend->init) {
                sess->backend->init(&sess->backend_config);
            }
            return 1;
        }
    }
    // P0-11: Emit backend failure diagnostic
    kaintana__diag_ui(KT_DIAG_CODE_UI_BACKEND_FAILURE, KAIN_DIAG_SEVERITY_ERROR,
        "Backend not found or init failed", n);
    return 0;
}
int kt_backend_probe(kt_Session* s) {
    const char* env = getenv("RENDERER_BACKEND");
    if (env && kt_backend_select(s, env)) return 1;
    // Fall back to first registered backend via kt_backend_select which calls init()
    if (cat_count>0) { return kt_backend_select(s, catalog[0].name); }
    return 0;
}

// ============================================================================
//  SECTION 12: INTERNAL API
// ============================================================================
int kaintana__node_find(kt_Session* s, const char* sk)
    { return kaintana__hash_lookup(s, kaintana_hash_fnv1a(sk)); }
// attr_lookup moved to attr_table.c
void kaintana__node_mark_dirty(kt_Session* s, int idx, int reason) {
    struct kt_Session_t* ss = kaintana__session(s);
    while (idx >= 0 && idx < ss->node_count) {
        KaintanaNode* n = &ss->nodes[idx];
        if (n->invalidation_flags & reason) break;
        n->invalidation_flags |= (uint16_t)reason;
        idx = n->parent_index;
    }
}

// Damage and draw functions moved to damage.c / draw_pixels.c

// ============================================================================
//  SECTION 13: HIT TESTING (pointer→node matching)
// ============================================================================
//  Runs after layout_pass2 in kt_end(). Walks visible interactive nodes
//  in reverse allocation order (last = topmost), tests pointer position
//  against resolved bounds. Sets hovered_id and processes click detection.
// ============================================================================

void kaintana__hit_test(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);
    float px = sess->input.mouse_x;
    float py = sess->input.mouse_y;
    int prev_hovered = sess->input.hovered_id;
    int hit_id = -1;

    // Walk nodes in reverse order (last allocated = topmost in Z-order)
    for (int i = sess->node_count - 1; i >= 1; i--) {  // skip root (0)
        KaintanaNode* n = &sess->nodes[i];
        if (!(n->flags & KT_NODE_VISIBLE)) continue;
        if (!(n->flags & KT_NODE_INTERACTIVE)) continue;
        if (n->layout_arena_index < 0) continue;
        KaintanaLayout* l = &sess->layouts[n->layout_arena_index];

        // Point-in-rect with epsilon to avoid float edge-case failures
        // (OpenSwiftUI uses -0.001 inset trick; we use 1e-6f)
        if (px >= l->resolved_x - 1e-6f && px < l->resolved_x + l->resolved_width + 1e-6f &&
            py >= l->resolved_y - 1e-6f && py < l->resolved_y + l->resolved_height + 1e-6f) {
            hit_id = i;
            break;
        }
    }

    // Update hover state on previous and new hovered nodes
    if (prev_hovered != hit_id) {
        if (prev_hovered >= 0 && prev_hovered < sess->node_count)
            sess->nodes[prev_hovered].flags &= (uint8_t)~KT_NODE_HOVERED;
        if (hit_id >= 0 && hit_id < sess->node_count)
            sess->nodes[hit_id].flags |= KT_NODE_HOVERED;
    }
    sess->input.hovered_id = hit_id;

    // Click detection: match press→release on the same node
    // Reset clicked_id at start — set below if a click is detected
    sess->input.clicked_id = -1;
    for (int b = 0; b < 5; b++) {
        if (sess->input.mouse_released_this_frame[b]) {
            if (sess->input.click_press_node[b] == hit_id && hit_id >= 0) {
                sess->input.clicked_id = hit_id;
            }
            sess->input.click_press_node[b] = -1;
        }
        if (sess->input.mouse_pressed_this_frame[b]) {
            sess->input.click_press_node[b] = hit_id;
        }
        // Clear transition flags — they're consumed here
        sess->input.mouse_pressed_this_frame[b] = 0;
        sess->input.mouse_released_this_frame[b] = 0;
    }
}

// ============================================================================
//  SECTION 14: INTERACTION QUERY (public API)
// ============================================================================

int kt_hovered(kt_Session* s, int elem) {
    struct kt_Session_t* sess = kaintana__session(s);
    return (sess->input.hovered_id == elem) ? 1 : 0;
}

int kt_clicked(kt_Session* s, int elem) {
    struct kt_Session_t* sess = kaintana__session(s);
    return (sess->input.clicked_id == elem) ? 1 : 0;
}

int kt_active(kt_Session* s, int elem) {
    struct kt_Session_t* sess = kaintana__session(s);
    return (sess->input.active_id == elem) ? 1 : 0;
}
