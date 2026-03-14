#include "../../include/kain_runtime_ui.h"

#ifdef _WIN32
void kain_ui_overlay_begin(int viewport_width, int viewport_height) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)viewport_width, (double)viewport_height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void kain_ui_overlay_end(void) {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void kain_ui_overlay_draw_panel(KainWin32GlSurface* surface, const KainUiOverlayTheme* theme, const KainUiOverlayPanel* panel) {
    int line_index;
    float current_y;

    if (!surface || !theme || !panel) {
        return;
    }

    kain_gl_draw_rect(
        panel->x,
        panel->y,
        panel->width,
        panel->height,
        theme->panel_color[0],
        theme->panel_color[1],
        theme->panel_color[2],
        theme->panel_color[3]
    );
    kain_gl_draw_rect(
        panel->x,
        panel->y,
        panel->width,
        4.0f,
        theme->accent_color[0],
        theme->accent_color[1],
        theme->accent_color[2],
        theme->accent_color[3]
    );

    glColor4f(theme->title_color[0], theme->title_color[1], theme->title_color[2], theme->title_color[3]);
    if (panel->title) {
        kain_win32_gl_surface_draw_text(surface, panel->x + theme->padding_x, panel->y + theme->title_y, panel->title);
    }
    if (panel->subtitle) {
        glColor4f(theme->text_color[0], theme->text_color[1], theme->text_color[2], theme->text_color[3]);
        kain_win32_gl_surface_draw_text(surface, panel->x + theme->padding_x, panel->y + theme->subtitle_y, panel->subtitle);
    }

    if (!panel->lines || panel->line_count <= 0) {
        return;
    }

    glColor4f(theme->text_color[0], theme->text_color[1], theme->text_color[2], theme->text_color[3]);
    current_y = panel->y + theme->line_y_start;
    for (line_index = 0; line_index < panel->line_count; ++line_index) {
        const char* line = panel->lines[line_index];
        if (!line || !line[0]) {
            continue;
        }
        kain_win32_gl_surface_draw_text(surface, panel->x + theme->padding_x, current_y, line);
        current_y += theme->line_gap;
    }
}

void kain_ui_overlay_draw_crosshair(int viewport_width, int viewport_height, const float color[4]) {
    float r = 0.95f;
    float g = 0.96f;
    float b = 1.0f;
    float a = 0.9f;

    if (color) {
        r = color[0];
        g = color[1];
        b = color[2];
        a = color[3];
    }

    kain_gl_draw_rect((float)(viewport_width * 0.5f - 8.0f), (float)(viewport_height * 0.5f - 1.0f), 16.0f, 2.0f, r, g, b, a);
    kain_gl_draw_rect((float)(viewport_width * 0.5f - 1.0f), (float)(viewport_height * 0.5f - 8.0f), 2.0f, 16.0f, r, g, b, a);
}
#endif
