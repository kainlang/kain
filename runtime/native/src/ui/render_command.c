// ============================================================================
//  render_command.c — Flat Render Command Array Implementation
// ============================================================================
//  Implements the flat command array model ported from Clay's architecture:
//
//    Layout phase → RenderCommandArray → z-sort → batch → Execute
//
//  Commands are self-contained (no tree pointers) and all rendering state
//  is captured inline so the renderer never needs to walk a retained tree.
//  This is the same data-model pattern as Clay_RenderCommandArray: a flat
//  array of typed commands with bounding boxes, sorted by z-index, executed
//  sequentially by a backend renderer.
//
//  ============================================================================

#include "render_command.h"
#include "kain_render_software.h"
#include "kain_geometry.h"
#include <string.h>

// ── Initialization ───────────────────────────────────────────────────────

void render_cmd_init(RenderCommandArray* arr) {
    if (!arr) return;
    arr->count = 0;
    arr->sorted = false;
}

void render_cmd_clear(RenderCommandArray* arr) {
    if (!arr) return;
    arr->count = 0;
    arr->sorted = false;
}

// ── Adding raw commands ──────────────────────────────────────────────────

int render_cmd_add(RenderCommandArray* arr, const RenderCommand* cmd) {
    if (!arr || !cmd) return -1;
    if (arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    arr->commands[idx] = *cmd;
    arr->sorted = false;
    return idx;
}

// ── Convenience helpers ──────────────────────────────────────────────────

int render_cmd_fill_rect(RenderCommandArray* arr, kainRect bounds,
                          kainColor color, float radius, int16_t z) {
    if (!arr || arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    RenderCommand* cmd = &arr->commands[idx];
    cmd->type      = CMD_FILL_RECT;
    cmd->bounds    = bounds;
    cmd->color     = color;
    cmd->radius    = radius;
    cmd->z_index   = z;
    // Zero out unused fields
    cmd->color_b   = KAIN_COLOR_TRANSPARENT;
    cmd->thickness = 0.0f;
    cmd->font_id   = 0;
    cmd->text      = NULL;
    cmd->font_size = 0.0f;
    arr->sorted    = false;
    return idx;
}

int render_cmd_stroke_rect(RenderCommandArray* arr, kainRect bounds,
                            kainColor color, float thickness, int16_t z) {
    if (!arr || arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    RenderCommand* cmd = &arr->commands[idx];
    cmd->type      = CMD_STROKE_RECT;
    cmd->bounds    = bounds;
    cmd->color     = color;
    cmd->thickness = thickness;
    cmd->z_index   = z;
    cmd->color_b   = KAIN_COLOR_TRANSPARENT;
    cmd->radius    = 0.0f;
    cmd->font_id   = 0;
    cmd->text      = NULL;
    cmd->font_size = 0.0f;
    arr->sorted    = false;
    return idx;
}

int render_cmd_fill_circle(RenderCommandArray* arr, kainPoint center,
                            float radius, kainColor color, int16_t z) {
    if (!arr || arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    RenderCommand* cmd = &arr->commands[idx];
    cmd->type      = CMD_FILL_CIRCLE;
    // Store center.x in bounds.x, center.y in bounds.y, radius in bounds.w
    cmd->bounds    = kain_rect_make(center.x, center.y, radius, 0.0f);
    cmd->color     = color;
    cmd->z_index   = z;
    cmd->color_b   = KAIN_COLOR_TRANSPARENT;
    cmd->thickness = 0.0f;
    cmd->radius    = 0.0f;
    cmd->font_id   = 0;
    cmd->text      = NULL;
    cmd->font_size = 0.0f;
    arr->sorted    = false;
    return idx;
}

int render_cmd_stroke_circle(RenderCommandArray* arr, kainPoint center,
                              float radius, float thickness,
                              kainColor color, int16_t z) {
    if (!arr || arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    RenderCommand* cmd = &arr->commands[idx];
    cmd->type      = CMD_STROKE_CIRCLE;
    // Store center.x in bounds.x, center.y in bounds.y, radius in bounds.w
    cmd->bounds    = kain_rect_make(center.x, center.y, radius, 0.0f);
    cmd->color     = color;
    cmd->thickness = thickness;
    cmd->z_index   = z;
    cmd->color_b   = KAIN_COLOR_TRANSPARENT;
    cmd->radius    = 0.0f;
    cmd->font_id   = 0;
    cmd->text      = NULL;
    cmd->font_size = 0.0f;
    arr->sorted    = false;
    return idx;
}

int render_cmd_text(RenderCommandArray* arr, kainRect bounds,
                     const char* text, int64_t font_id, float font_size,
                     kainColor color, int16_t z) {
    if (!arr || arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    RenderCommand* cmd = &arr->commands[idx];
    cmd->type      = CMD_TEXT;
    cmd->bounds    = bounds;
    cmd->color     = color;
    cmd->font_id   = font_id;
    cmd->font_size = font_size;
    cmd->text      = text;
    cmd->z_index   = z;
    cmd->color_b   = KAIN_COLOR_TRANSPARENT;
    cmd->thickness = 0.0f;
    cmd->radius    = 0.0f;
    arr->sorted    = false;
    return idx;
}

int render_cmd_gradient(RenderCommandArray* arr, kainRect bounds,
                         kainColor a, kainColor b, int16_t z) {
    if (!arr || arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    RenderCommand* cmd = &arr->commands[idx];
    cmd->type      = CMD_GRADIENT_RECT;
    cmd->bounds    = bounds;
    cmd->color     = a;
    cmd->color_b   = b;
    cmd->z_index   = z;
    cmd->thickness = 0.0f;
    cmd->radius    = 0.0f;
    cmd->font_id   = 0;
    cmd->text      = NULL;
    cmd->font_size = 0.0f;
    arr->sorted    = false;
    return idx;
}

int render_cmd_scissor_start(RenderCommandArray* arr, kainRect bounds,
                              int16_t z) {
    if (!arr || arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    RenderCommand* cmd = &arr->commands[idx];
    cmd->type      = CMD_SCISSOR_START;
    cmd->bounds    = bounds;
    cmd->z_index   = z;
    cmd->color     = KAIN_COLOR_TRANSPARENT;
    cmd->color_b   = KAIN_COLOR_TRANSPARENT;
    cmd->thickness = 0.0f;
    cmd->radius    = 0.0f;
    cmd->font_id   = 0;
    cmd->text      = NULL;
    cmd->font_size = 0.0f;
    arr->sorted    = false;
    return idx;
}

int render_cmd_scissor_end(RenderCommandArray* arr, int16_t z) {
    if (!arr || arr->count >= MAX_RENDER_COMMANDS) return -1;
    int idx = arr->count++;
    RenderCommand* cmd = &arr->commands[idx];
    cmd->type      = CMD_SCISSOR_END;
    cmd->z_index   = z;
    // Zero bounds — scissor_end carries no bounding box
    cmd->bounds    = kain_rect_make(0.0f, 0.0f, 0.0f, 0.0f);
    cmd->color     = KAIN_COLOR_TRANSPARENT;
    cmd->color_b   = KAIN_COLOR_TRANSPARENT;
    cmd->thickness = 0.0f;
    cmd->radius    = 0.0f;
    cmd->font_id   = 0;
    cmd->text      = NULL;
    cmd->font_size = 0.0f;
    arr->sorted    = false;
    return idx;
}

// ── Stable insertion sort by z_index ────────────────────────────────────
//  Command arrays are usually well-ordered (children added after parents),
//  so insertion sort is O(n) in the common case and fast even at worst case
//  for small command counts (< 8192 is small for modern CPUs).
//  This is a stable sort — equal z_index values keep their insertion order.

void render_cmd_sort(RenderCommandArray* arr) {
    if (!arr || arr->sorted || arr->count < 2) return;

    for (int i = 1; i < arr->count; i++) {
        RenderCommand key = arr->commands[i];
        int j = i - 1;

        while (j >= 0 && arr->commands[j].z_index > key.z_index) {
            arr->commands[j + 1] = arr->commands[j];
            j--;
        }
        arr->commands[j + 1] = key;
    }

    arr->sorted = true;
}

// ── Culling: check if a rect is entirely outside the framebuffer ────────

static bool cmd_is_offscreen(const RenderCommand* cmd, int fb_w, int fb_h) {
    // SCISSOR_END and NONE always execute (no bounds to cull)
    if (cmd->type == CMD_SCISSOR_END || cmd->type == CMD_NONE)
        return false;

    // Commands with zero-area bounds but valid position (e.g. circles with
    // radius encoded in bounds.w) need special handling: only cull if the
    // primitive is definitively outside the framebuffer.

    // Conservative bounding-box test:
    // The command is offscreen if its entire bounding box lies outside
    // the framebuffer. We compute the axis-aligned extent of the primitive.
    float x0 = cmd->bounds.x;
    float y0 = cmd->bounds.y;
    float x1 = cmd->bounds.x + cmd->bounds.w;
    float y1 = cmd->bounds.y + cmd->bounds.h;

    // For CMD_FILL_CIRCLE and CMD_STROKE_CIRCLE, bounds.w = radius.
    // Compute the full AABB of the circle.
    if (cmd->type == CMD_FILL_CIRCLE || cmd->type == CMD_STROKE_CIRCLE) {
        float r = cmd->bounds.w; // radius stored in bounds.w
        x0 = cmd->bounds.x - r;
        y0 = cmd->bounds.y - r;
        x1 = cmd->bounds.x + r;
        y1 = cmd->bounds.y + r;
    }

    // Cull if entirely outside framebuffer
    if (x1 < 0.0f || y1 < 0.0f) return true;
    if (x0 > (float)fb_w || y0 > (float)fb_h) return true;

    return false;
}

// ── Execution ────────────────────────────────────────────────────────────
//  Dispatches each RenderCommand to the matching kain_render_* primitive.

void render_cmd_execute(const RenderCommandArray* arr,
                         KainSoftwareRenderer* renderer,
                         int fb_width, int fb_height) {
    if (!arr || !renderer) return;
    if (arr->count == 0) return;

    for (int i = 0; i < arr->count; i++) {
        const RenderCommand* cmd = &arr->commands[i];

        // Cull commands entirely outside the framebuffer
        if (cmd_is_offscreen(cmd, fb_width, fb_height))
            continue;

        switch (cmd->type) {

        case CMD_FILL_RECT:
            if (cmd->radius > 0.0f) {
                kain_render_fill_rounded_rect(renderer,
                    cmd->bounds, cmd->radius, cmd->color);
            } else {
                kain_render_fill_rect(renderer, cmd->bounds, cmd->color);
            }
            break;

        case CMD_STROKE_RECT:
            kain_render_stroke_rect(renderer,
                cmd->bounds, cmd->thickness, cmd->color);
            break;

        case CMD_FILL_CIRCLE: {
            // bounds.x = center.x, bounds.y = center.y, bounds.w = radius
            kainPoint center = kain_point_make(cmd->bounds.x, cmd->bounds.y);
            kain_render_fill_circle(renderer, center, cmd->bounds.w, cmd->color);
            break;
        }

        case CMD_STROKE_CIRCLE: {
            kainPoint center = kain_point_make(cmd->bounds.x, cmd->bounds.y);
            kain_render_stroke_circle(renderer, center, cmd->bounds.w,
                                       cmd->thickness, cmd->color);
            break;
        }

        case CMD_TEXT:
            kain_render_text(renderer,
                kain_point_make(cmd->bounds.x, cmd->bounds.y),
                cmd->text, cmd->font_id, cmd->font_size, cmd->color);
            break;

        case CMD_GRADIENT_RECT: {
            // Two-color gradient: pass color (start) and color_b (end)
            // with stops at 0.0 and 1.0
            kainColor colors[2];
            float stops[2];
            colors[0] = cmd->color;
            colors[1] = cmd->color_b;
            stops[0]  = 0.0f;
            stops[1]  = 1.0f;
            kain_render_gradient_rect(renderer, cmd->bounds,
                                       colors, stops, 2);
            break;
        }

        case CMD_SCISSOR_START:
            kain_render_push_clip(renderer, cmd->bounds);
            break;

        case CMD_SCISSOR_END:
            kain_render_pop_clip(renderer);
            break;

        case CMD_NONE:
        default:
            // Skip uninitialized / unknown commands
            break;
        }
    }
}

// ── Batching (optimization) ─────────────────────────────────────────────
//  After z-sorting, consecutive CMD_FILL_RECT commands with the same color
//  and no corner radius can be merged into a single larger rectangle.
//  This reduces draw-call count and improves fill-coverage efficiency.

int render_cmd_batch(RenderCommandArray* arr) {
    if (!arr || arr->count < 2) return arr ? arr->count : 0;

    int write_idx = 0;

    for (int read_idx = 1; read_idx < arr->count; read_idx++) {
        RenderCommand* prev = &arr->commands[write_idx];
        RenderCommand* curr = &arr->commands[read_idx];

        // Check if prev and curr can be merged:
        // Both must be CMD_FILL_RECT with same color, no radius, same z
        if (prev->type == CMD_FILL_RECT &&
            curr->type == CMD_FILL_RECT &&
            prev->z_index == curr->z_index &&
            prev->radius == 0.0f &&
            curr->radius == 0.0f &&
            prev->color.r == curr->color.r &&
            prev->color.g == curr->color.g &&
            prev->color.b == curr->color.b &&
            prev->color.a == curr->color.a)
        {
            // Merge: expand prev bounds to include curr bounds
            kainRect merged = kain_rect_union(prev->bounds, curr->bounds);
            prev->bounds = merged;
            // curr is consumed — skip it (don't advance write_idx)
        } else {
            // Cannot merge — advance write_idx and copy curr forward
            write_idx++;
            if (write_idx != read_idx) {
                arr->commands[write_idx] = arr->commands[read_idx];
            }
        }
    }

    arr->count = write_idx + 1;
    return arr->count;
}
