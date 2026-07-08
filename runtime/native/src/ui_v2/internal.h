// ============================================================================
//  internal.h — Private types for the Kaintana C substrate.
//
//  THIS IS NOT A PUBLIC HEADER. Included only by tree.c, box_math.c,
//  damage.c, draw_pixels.c, arena.c, hash_table.c, and backends.
//  Everything here is subject to change without notice.
//
//  The session struct (KaintanaSession) is the single big context for
//  a Kaintana UI session. It owns:
//    - Frame arena (64KB bump allocator, O(1) per-frame cleanup)
//    - Node tree (flat arena of KaintanaNode, 32B each, 2 per cache line)
//    - Layout SoA (parallel arrays indexed by node.layout_arena_index)
//    - Stable key hash table (FNV-1a open-addressing, 4096 slots)
//    - Element stack (for kt_row/kt_end_row nesting, max 64 depth)
//    - Damage pipeline (3 phase heaps + 64-rect accumulator)
//    - Draw batch (write-pointer command buffer)
//    - State map (string key → i64/f64/string persistence)
//    - Input state (current frame's input snapshot)
//    - Core runtime integration (arena, vtable session_id, input_sid)
// ============================================================================
#ifndef KAINTANA_INTERNAL_H
#define KAINTANA_INTERNAL_H

#include "kaintana.h"
#include "arena.h"          // KainArena, kain_arena_init, kain_arena_alloc_lo
#include "input_system.h"   // abi_input_begin_frame, abi_input_push_event
#include "handle.h"          // KainHandleTable, kain_handle_table_acquire/resolve
#include "version.h"         // version_check_abi_compatibility

#include <string.h>         // memset, memcpy
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
//  SIZE & CAPACITY CONSTANTS
// ============================================================================

#define KAINTANA_ARENA_SIZE             (1024 * 1024) // 1MB — supports 4096 nodes (128KB) + layouts (448KB) + caches (96KB) + overhead
#define KAINTANA_MAX_NODES              4096          // Max elements per frame
#define KAINTANA_MAX_DEPTH              64            // Max element stack depth
#define KAINTANA_HASH_SLOTS             4096          // Must be power of 2
#define KAINTANA_HASH_MAX_LOAD          256           // alpha = 0.0625
#define KAINTANA_DAMAGE_MAX_RECTS       64            // Max damage rects before merge
#define KAINTANA_DRAW_BATCH_INIT        128           // Initial cmd capacity
#define KAINTANA_STATE_ENTRIES          128           // Max state key-value pairs
#define KAINTANA_MAX_CHILDREN           1024          // Max children per container
#define KAINTANA_GENERATION_RESET       0xFFFFFFFF    // Sentinel for cache invalidation

// ============================================================================
//  KaintanaNode — Element tree node, 32 bytes (2 per cache line)
// ============================================================================
//  Nodes form a tree stored in a flat arena. Children are linked through
//  first_child→next_sibling singly-linked list. Stable keys are resolved
//  via FNV-1a hash through a separate open-addressing table.
//
//  Memory layout (must be 32 bytes total):
//    Offset  Size  Field
//    0       8     stable_key_hash
//    8       4     parent_index
//    12      4     first_child
//    16      4     next_sibling
//    20      4     layout_arena_index
//    24      2     invalidation_flags
//    26      1     visibility_flags
//    27      1     flags
//    28      2     state_payload_offset
//    30      2     [padding/expansion]
//    ─────── 32 bytes
// ============================================================================
typedef struct KaintanaNode {
    uint64_t    stable_key_hash;        // FNV-1a hash of the stable key string
    int32_t     parent_index;           // -1 for root
    int32_t     first_child;            // Index of first child (-1 = none)
    int32_t     next_sibling;           // Index of next sibling (-1 = none)
    int32_t     layout_arena_index;     // Index into parallel layout SoA (-1 = unallocated)
    uint16_t    invalidation_flags;     // Dirty reason bits (KaintanaInvalidationReason)
    uint8_t     visibility_flags;       // Visibility state
    uint8_t     flags;                  // Misc flags
    int16_t     state_payload_offset;   // Offset into state payload blob (-1 = none)
    int16_t     _padding;
} KaintanaNode;
KT_STATIC_ASSERT(sizeof(KaintanaNode) == 32, kaintana_node_size_32);

// ── KaintanaNode flags ────────────────────────────────────────────────────
enum {
    KT_NODE_INTERACTIVE    = 1 << 0,    // Can receive pointer events
    KT_NODE_HIT_TESTED     = 1 << 1,    // Hit test ran this frame
    KT_NODE_DRAGGING       = 1 << 2,    // Currently being dragged
    KT_NODE_HOVERED        = 1 << 3,    // Pointer is inside bounds
    KT_NODE_DIRTY_STRUCT   = 1 << 4,    // Structural change (child added/removed)
    KT_NODE_IN_HEAP        = 1 << 5,    // Already in a phase heap (dedup guard)
    KT_NODE_VISIBLE        = 1 << 6,    // Node is visible and should be rendered
    KT_NODE_COLLAPSED      = 1 << 7     // Children are hidden
};

// ── KaintanaInvalidationReason — damage pipeline dirty flag bits ─────────
enum KaintanaInvalidationReason {
    KT_INVALIDATE_LAYOUT        = 1 << 0,  // Re-run prepass + arrange + paint
    KT_INVALIDATE_PREPASS       = 1 << 1,  // Re-run desired-size computation
    KT_INVALIDATE_PAINT         = 1 << 2,  // Re-generate draw commands only
    KT_INVALIDATE_CHILD_ORDER   = 1 << 3,  // Children added/removed/reordered
    KT_INVALIDATE_VISIBILITY    = 1 << 4,  // Show/hide changed
    KT_INVALIDATE_VOLATILITY    = 1 << 5,  // Value changes each frame (fast animation)
    KT_INVALIDATE_ALL           = 0xFFFF   // Full rebuild
};

// ── Visibility flags ─────────────────────────────────────────────────────
enum {
    KT_VISIBLE_DEFAULT    = 0,            // Visible, participates in layout
    KT_VISIBLE_HIDDEN     = 1,            // Hidden, still takes layout space
    KT_VISIBLE_COLLAPSED  = 2,            // Hidden, excluded from layout flow
    KT_VISIBLE_ABSENT     = 3             // Not yet created (transient)
};

// ── KaintanaSizingMode — matches Yoga's MeasureMode ──────────────────────
//     Must be defined BEFORE KaintanaLayout which uses it.
typedef enum KaintanaSizingMode {
    KT_SIZE_STRETCH_FIT    = 0,    // Parent forces exact size (MeasureMode::Exactly)
    KT_SIZE_MAX_CONTENT    = 1,    // Child determines size (MeasureMode::Undefined)
    KT_SIZE_FIT_CONTENT    = 2     // Child grows to fit but shrinks to avoid overflow
} KaintanaSizingMode;

// ============================================================================
//  KaintanaLayout — Per-node layout data (SoA), stored in parallel arena
// ============================================================================
//  These are stored as a flat array indexed by node.layout_arena_index.
//  The SoA (Structure-of-Arrays) layout is GPU-friendly: the layout arena
//  can be uploaded as a uniform buffer without topology data.
// ============================================================================
typedef struct KaintanaLayout {
    // ── Desired size (from bottom-up prepass) ────────────────
    float   desired_width;         // Natural width based on content
    float   desired_height;        // Natural height based on content

    // ── Resolved size (from top-down arrange pass) ───────────
    float   resolved_x;            // Final absolute x position
    float   resolved_y;            // Final absolute y position
    float   resolved_width;        // Final width after constraint solving
    float   resolved_height;       // Final height after constraint solving

    // ── Padding (edge insets inside border) ───────────────────
    float   pad_left, pad_right, pad_top, pad_bottom;

    // ── Margin (edge insets outside border) ───────────────────
    float   margin_left, margin_right, margin_top, margin_bottom;

    // ── Flexbox ────────────────────────────────────────────────
    float   flex_grow;             // Flex grow factor (0 = no grow)
    float   flex_shrink;           // Flex shrink factor (0 = no shrink)
    float   flex_basis;            // Flex basis size

    // ── Sizing constraints ─────────────────────────────────────
    float   min_width, max_width;
    float   min_height, max_height;

    // ── Visual ─────────────────────────────────────────────────
    float   corner_radius;         // Uniform corner radius
    float   opacity;               // 0.0 — 1.0

    // ── Visual style from attr_table ──────────────────────────
    uint32_t    fill_color;            // Parsed fill color (premultiplied ARGB)
    uint32_t    stroke_color;          // Parsed stroke color (premultiplied ARGB)
    float       stroke_width;          // Stroke width

    // ── Text content (arena-backed, NULL = no text) ────────────
    const char* text_content;        // Arena-allocated text string

    // ── Sizing mode (from parent's available space hint) ─────
    int8_t      width_mode;            // KaintanaSizingMode
    int8_t      height_mode;
    int8_t      direction;            // KaintanaLayoutDir (0=row, 1=col, 2=row-rev, 3=col-rev)
    int8_t      justify_content;      // KaintanaJustify (0=flex-start, 1=center, etc.)
    int8_t      align_items;          // KaintanaAlign (0=stretch, 1=flex-start, etc.)
    int8_t      _pad;
} KaintanaLayout;
// Layout size is not fixed — depends on flexbox fields.
// The SoA arena is indexed by node.layout_arena_index, not sizeof() critical.

// ============================================================================
//  KaintanaLayoutCache — 1-slot generation-tagged layout cache
// ============================================================================
//  Single-slot cache (Yoga's CachedMeasurement). Keyed by available width
//  and height plus generation counter. Invalidated when generation changes.
// ============================================================================
typedef struct KaintanaLayoutCache {
    bool     valid;
    uint32_t generation;            // Matches session->layout_generation
    float    available_width;
    float    available_height;
    float    measured_width;
    float    measured_height;
} KaintanaLayoutCache;

// ============================================================================
//  KaintanaInternalDrawCmd — Packed internal draw command, 32 bytes
// ============================================================================
//  This is the runtime's internal representation. Backends consume this
//  and translate to their own format (kain_Cmd for software, vertices for GPU).
//  Packed to 32 bytes for efficient GPU upload.
// ============================================================================
typedef struct KaintanaInternalDrawCmd {
    uint32_t    type;               // KaintanaCmdType
    uint32_t    color;              // Premultiplied ARGB (primary fill/stroke)
    uint32_t    color_b;            // Secondary color (gradient end, etc.)
    int16_t     x, y;               // Position
    uint16_t    w, h;               // Size
    uint16_t    corner_radius;      // Fixed-point 8.8 (1/256 precision)
    uint8_t     opacity;            // 0-255, multiplied into alpha at render time
    uint8_t     blend_mode;         // KaintanaBlendMode
    int32_t     texture_handle;     // Texture handle (-1 = none)
    int32_t     data_offset;        // Offset into aux data (glyph UVs, vertices)
} KaintanaInternalDrawCmd;
KT_STATIC_ASSERT(sizeof(KaintanaInternalDrawCmd) == 32,
                 kaintana_drawcmd_size_32);

// ============================================================================
//  KaintanaDrawBatch — Write-pointer command buffer
// ============================================================================
//  Flat array with reservation-based write pointer (ImGui pattern).
//  Auto-merge at insertion: if the new command matches the last on clip+texture,
//  element counts are merged instead of appended.
// ============================================================================
typedef struct KaintanaDrawBatch {
    KaintanaInternalDrawCmd*    buf;            // Command buffer
    int32_t                     count;          // Current command count
    int32_t                     capacity;       // Allocated capacity
    KaintanaInternalDrawCmd*    write_ptr;      // Reservation pointer
    KaintanaInternalDrawCmd     last;           // Last emitted command (for merge)
} KaintanaDrawBatch;

// ============================================================================
//  KaintanaPhaseHeap — Set-like array for damage pipeline phases
// ============================================================================
//  Three heaps: PreUpdate (structural), Prepass (desired size, bottom-up),
//  PostUpdate (arrange+paint, top-down). Duplicates prevented by per-node
//  flag bit (KT_NODE_IN_HEAP). Sorting uses insertion sort (<64) or
//  counting sort by depth (>=64).
// ============================================================================
typedef struct KaintanaPhaseHeap {
    int32_t*    indices;            // Flat array of node indices
    int32_t     count;              // Current count
    int32_t     capacity;           // Allocated capacity
    int32_t     sort_dir;           // 1 = ascending (parents first), -1 = descending (children first)
} KaintanaPhaseHeap;

// ============================================================================
//  KaintanaDamageAccumulator — Rect merge for invalidation
// ============================================================================
//  Clay-style 64-rect damage accumulator. When full, finds closest pair
//  and merges them. Produces a single union rect for full-redraw fallback.
// ============================================================================
typedef struct KaintanaDamageAccumulator {
    kt_Rect     rects[KAINTANA_DAMAGE_MAX_RECTS];
    int32_t     count;
    kt_Rect     combined;           // Union of all non-overflowed rects
    bool        overflowed;         // True when merging was forced
} KaintanaDamageAccumulator;

// ============================================================================
//  KaintanaStateEntry — Single state key-value pair
// ============================================================================
//  String-keyed state for kt_put/kt_get family. Types discriminated by
//  a type tag. Values stored in a flat array in the session.
// ============================================================================
typedef struct KaintanaStateEntry {
    char        key[48];            // State key string
    uint8_t     type;               // 0=i64, 1=f64, 2=string
    uint8_t     _pad[3];
    union {
        int64_t     i64_val;
        double      f64_val;
        char        str_val[64];    // Inline string storage (heap for longer)
    } data;
} KaintanaStateEntry;

// ============================================================================
//  KaintanaElementStack — Nesting tracker for kt_row/kt_end_row
// ============================================================================
typedef struct KaintanaElementStack {
    int32_t     stack[KAINTANA_MAX_DEPTH];
    int32_t     depth;              // Current stack depth
} KaintanaElementStack;

// ============================================================================
//  KaintanaInputState — Per-frame input snapshot
// ============================================================================
//  Mirrors kt_Input from kaintana.h. Filled by the backend before
//  kt_begin() via abi_input_push_event().
// ============================================================================
typedef struct KaintanaInputState {
    float   mouse_x, mouse_y;
    float   scroll_dx, scroll_dy;
    int     keys_down[256];          // 1 = pressed
    int     mouse_down[5];           // 1 = currently pressed (0=left,1=right,2=middle)
    char    text_input[32];
    int     text_len;
    int     active_id;               // Element index with pointer capture (-1 = none)
    int     hovered_id;              // Element under pointer after hit test (-1 = none)
    float   delta_ms;                // Frame delta in ms
    double  time_ms;                 // Running time accumulator (ms since session start)
    // ── Per-frame transition tracking ─────────────────────
    int     mouse_pressed_this_frame[5];   // Button was just pressed this frame
    int     mouse_released_this_frame[5];  // Button was just released this frame
    int     click_press_node[5];           // Node index pressed on per button (-1 = none)
    int     clicked_id;                    // Node clicked this frame (-1 = none), valid after kt_end()
} KaintanaInputState;

// ============================================================================
//  KaintanaAttrTable — Data-driven attribute dispatch
// ============================================================================
//  Maps attribute name strings to invalidation reasons and type expectations.
//  Used by tree.c's element_set_attr_* dispatchers. No #define constants.
// ============================================================================
typedef struct KaintanaAttrEntry {
    const char* name;                // Attribute name (e.g. "layout.width")
    uint16_t    invalidation;        // KaintanaInvalidationReason bitmask
    uint8_t     expected_type;       // 0=i64, 1=f64, 2=string
    uint8_t     _pad;
} KaintanaAttrEntry;

// ============================================================================
//  KaintanaSession — THE ONE BIG CONTEXT
// ============================================================================
//  Opaque from the public API (typedef struct kt_Session_t).
//  The full definition lives here so the 6 core .c files can access fields.
//  Size: ~223KB (dominated by 64KB arena buffer + 96KB handle table array
//                 + 32KB hash table + 16KB hash values + 15KB state entries)
//  Heap-allocated via kt_malloc() — not stack. Absolute size not a concern.
// ============================================================================
struct kt_Session_t {
    // ── Core runtime integration ──────────────────────────────
    int64_t                         vtable_session_id;   // Opaque id from slot 0
    int64_t                         input_sid;           // Input system session
    const KainComponentSurface*     vtable;              // Our registered vtable
    const KaintanaBackendVTable*    backend;             // Active render backend
    KaintanaBackendConfig           backend_config;        // Config passed to backend->init() during select

    // ── Frame arena (64KB bump allocator) ─────────────────────
    KainArena                       arena;
    unsigned char                   arena_buffer[KAINTANA_ARENA_SIZE];

    // ── Node tree ──────────────────────────────────────────────
    KaintanaNode*                   nodes;              // Flat node arena
    int32_t                         node_count;
    int32_t                         node_capacity;

    // ── Layout SoA (parallel to nodes) ─────────────────────────
    KaintanaLayout*                 layouts;            // Layout data arena
    int32_t                         layout_count;
    int32_t                         layout_capacity;

    // ── Layout cache ───────────────────────────────────────────
    KaintanaLayoutCache*            layout_caches;      // Per-node 1-slot cache arena
    uint32_t                        layout_generation;  // Incremented each frame

    // ── Stable key hash table (FNV-1a open-addressing) ────────
    uint64_t                        hash_slots[KAINTANA_HASH_SLOTS];
    int32_t                         hash_values[KAINTANA_HASH_SLOTS];
    int32_t                         hash_occupied_count;        // Live entries, enforced <= MAX_LOAD

    // ── Handle table (generation-tagged stable key→node mapping) ─
    KainHandleSlot                  handle_slots[KAINTANA_HASH_SLOTS];
    KainHandleTable                 handle_table;

    // ── Element stack (kt_row/kt_end_row nesting) ─────────────
    KaintanaElementStack            elem_stack;

    // ── Damage pipeline ────────────────────────────────────────
    KaintanaPhaseHeap               heaps[3];           // PreUpdate, Prepass, PostUpdate
    KaintanaDamageAccumulator       damage;             // Rect accumulator
    bool                            needs_full_rebuild; // >30% nodes dirty -> full rebuild

    // ── Draw batch ─────────────────────────────────────────────
    KaintanaDrawBatch               draw_batch;
    kt_DrawData                     draw_data;          // Output (points into batch)

    // ── State persistence ──────────────────────────────────────
    KaintanaStateEntry              state_entries[KAINTANA_STATE_ENTRIES];
    int32_t                         state_count;

    // ── Input state (per-frame snapshot) ───────────────────────
    KaintanaInputState              input;

    // ── Frame counters ─────────────────────────────────────────
    uint32_t                        frame_number;       // Monotonic frame counter
    double                          frame_delta_ms;     // Current frame delta
    double                          frame_time_ms;      // Running time accumulator (ms since session start)

    // -- DPI & scaling ------------------------------------------------------------
    float                           native_scale_x;      // OS-reported horizontal DPI scale (e.g. 1.0, 1.5, 2.0)
    float                           native_scale_y;      // OS-reported vertical DPI scale
    float                           user_zoom;           // User-controlled zoom factor (0.2-5.0, default 1.0)
    bool                            scale_changed;       // Set true by kt_set_native_scale(), cleared after frame invalidation

    // ── Window dimensions (from kt_make) ──────────────────────
    int                             window_width;        // Window width in pixels
    int                             window_height;       // Window height in pixels
    int                             should_close;        // Set to 1 by backend/frontend to request close
};

// ============================================================================
//  INLINE ACCESSORS — Dereference session fields from public handle
// ============================================================================
//  kt_Session* is the opaque pointer from the public API.
//  These inline helpers cast to the full struct for internal use.
// ============================================================================

static inline struct kt_Session_t* kaintana__session(kt_Session* s) {
    return (struct kt_Session_t*)s;
}

// ── Arena helpers ─────────────────────────────────────────────────────────
static inline void* kaintana__arena_alloc(kt_Session* s, size_t size, size_t align) {
    return kain_arena_alloc_lo(&kaintana__session(s)->arena, size, align);
}

static inline void kaintana__arena_mark(kt_Session* s) {
    kain_frame_set_marker(&kaintana__session(s)->arena);
}

static inline void kaintana__arena_release(kt_Session* s) {
    kain_frame_release_to_last_marker(&kaintana__session(s)->arena);
}

// ── Node accessors ─────────────────────────────────────────────────────────
static inline KaintanaNode* kaintana__node(kt_Session* s, int32_t idx) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (idx < 0 || idx >= sess->node_count) return NULL;
    return &sess->nodes[idx];
}

static inline KaintanaLayout* kaintana__layout(kt_Session* s, int32_t idx) {
    struct kt_Session_t* sess = kaintana__session(s);
    if (idx < 0 || idx >= sess->layout_count) return NULL;
    return &sess->layouts[idx];
}

// ── Damage pipeline phase enum ────────────────────────────────────────────
typedef enum kt_Phase {
    KT_PHASE_PRE_UPDATE   = 0,     // Structural changes
    KT_PHASE_PREPASS      = 1,     // Bottom-up desired sizes (children before parents)
    KT_PHASE_POST_UPDATE  = 2      // Top-down arrange + paint (parents before children)
} kt_Phase;

// ── Phase heap helpers ────────────────────────────────────────────────────
static inline KaintanaPhaseHeap* kaintana__heap(kt_Session* s, kt_Phase phase) {
    return &kaintana__session(s)->heaps[(int)phase];
}

static inline void kaintana__heap_push(kt_Session* s, kt_Phase phase, int32_t node_idx) {
    struct kt_Session_t* sess = kaintana__session(s);
    KaintanaPhaseHeap* heap = &sess->heaps[(int)phase];
    if (!(sess->nodes[node_idx].flags & KT_NODE_IN_HEAP)) {
        sess->nodes[node_idx].flags |= KT_NODE_IN_HEAP;
        // Caller (tree.c / damage.c) MUST ensure heap->capacity >= heap->count + 1
        // before calling this. Arena-backed reallocation is in tree.c.
        if (heap->indices && heap->count < heap->capacity) {
            heap->indices[heap->count++] = node_idx;
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif // KAINTANA_INTERNAL_H
