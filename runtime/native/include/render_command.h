#ifndef KAIN_RENDER_COMMAND_H
#define KAIN_RENDER_COMMAND_H

#include <stdint.h>
#include <stdbool.h>
#include "kain_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

// ══════════════════════════════════════════════════════════════════════════
//  render_command.h — Flat Render Command Array (Clay-inspired architecture)
// ══════════════════════════════════════════════════════════════════════════
//  Decouples layout from rendering. A layout phase builds a flat array of
//  RenderCommands, which are then z-sorted and dispatched to the backend
//  renderer. This is the same data-model pattern as Clay_RenderCommandArray:
//
//    Layout phase → RenderCommandArray → z-sort → Execute on KainRenderer
//
//  Commands are self-contained (no tree references), enabling trivial
//  renderer swapping, command batching, and GPU backend support.
// ══════════════════════════════════════════════════════════════════════════

// ── Command types (mirrors Clay_RenderCommandType architecture) ─────────

typedef enum RenderCommandType {
    CMD_NONE = 0,          // Skip / uninitialized
    CMD_FILL_RECT,         // Solid fill rectangle (optional corner radius)
    CMD_STROKE_RECT,       // Rectangle outline / border
    CMD_FILL_CIRCLE,       // Filled circle
    CMD_STROKE_CIRCLE,     // Circle outline
    CMD_TEXT,              // Text string at position
    CMD_GRADIENT_RECT,     // Linear horizontal gradient rectangle
    CMD_SCISSOR_START,     // Push clip rectangle onto clip stack
    CMD_SCISSOR_END,       // Pop clip rectangle from clip stack
} RenderCommandType;

// ── Render command (self-contained; no tree pointers) ───────────────────

typedef struct RenderCommand {
    RenderCommandType type;       // Which primitive to draw
    kainRect          bounds;     // Bounding box (culling, hit-testing, layout)
    kainColor         color;      // Primary color (fill, stroke, text)
    kainColor         color_b;    // Secondary color (gradient end, etc.)
    float             thickness;  // Stroke width for CMD_STROKE_*, outline
    float             radius;     // Corner radius for CMD_FILL_RECT
    int64_t           font_id;    // Font resource ID for CMD_TEXT
    const char*       text;       // UTF-8 text string (borrowed pointer)
    float             font_size;  // Font pixel size for CMD_TEXT
    int16_t           z_index;    // Draw order (higher = on top; sorted ascending)
} RenderCommand;

// ── Command array (fixed-capacity flat array) ─────────────────────────

#define MAX_RENDER_COMMANDS 8192

typedef struct RenderCommandArray {
    RenderCommand commands[MAX_RENDER_COMMANDS];
    int           count;
    bool          sorted;         // true if commands are in ascending z-order
} RenderCommandArray;

// ── Lifecycle ───────────────────────────────────────────────────────────

// Initialize an empty command array.
void render_cmd_init(RenderCommandArray* arr);

// Clear all commands for the next frame (resets count to 0, sorted to false).
void render_cmd_clear(RenderCommandArray* arr);

// ── Adding commands ─────────────────────────────────────────────────────

// Add a raw RenderCommand. Returns the index, or -1 if the array is full.
int render_cmd_add(RenderCommandArray* arr, const RenderCommand* cmd);

// Convenience helpers for each command type. All return the command index
// or -1 if the array is full.

int render_cmd_fill_rect(RenderCommandArray* arr, kainRect bounds,
                          kainColor color, float radius, int16_t z);

int render_cmd_stroke_rect(RenderCommandArray* arr, kainRect bounds,
                            kainColor color, float thickness, int16_t z);

int render_cmd_fill_circle(RenderCommandArray* arr, kainPoint center,
                            float radius, kainColor color, int16_t z);

int render_cmd_stroke_circle(RenderCommandArray* arr, kainPoint center,
                              float radius, float thickness,
                              kainColor color, int16_t z);

int render_cmd_text(RenderCommandArray* arr, kainRect bounds,
                     const char* text, int64_t font_id, float font_size,
                     kainColor color, int16_t z);

int render_cmd_gradient(RenderCommandArray* arr, kainRect bounds,
                         kainColor a, kainColor b, int16_t z);

int render_cmd_scissor_start(RenderCommandArray* arr, kainRect bounds,
                              int16_t z);

int render_cmd_scissor_end(RenderCommandArray* arr, int16_t z);

// ── Sorting ─────────────────────────────────────────────────────────────

// Sort commands by ascending z_index (stable insertion sort).
// Command arrays are typically mostly in order already (inserted in tree
// traversal order), so this is O(n) in the common case.
void render_cmd_sort(RenderCommandArray* arr);

// ── Execution ───────────────────────────────────────────────────────────

typedef struct KainSoftwareRenderer KainSoftwareRenderer;

// Execute all commands on a KainSoftwareRenderer.
// Commands outside the framebuffer are culled (skipped).
// Caller should sort before calling, or the first cull pass marks sorted=false
// and a sort is performed.
void render_cmd_execute(const RenderCommandArray* arr,
                         KainSoftwareRenderer* renderer,
                         int fb_width, int fb_height);

// ── Batching (bonus optimization) ───────────────────────────────────────

// After sorting, merge consecutive identical CMD_FILL_RECT commands:
// if cmd[i] and cmd[i+1] are both CMD_FILL_RECT with the same color and
// no corner radius, the bounds are merged and cmd[i+1] is removed.
// Returns the new command count after merging.
int render_cmd_batch(RenderCommandArray* arr);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_RENDER_COMMAND_H */
