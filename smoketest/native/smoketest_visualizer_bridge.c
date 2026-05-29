#include "smoketest_visualizer_bridge.h"

#ifdef _WIN32

#if defined(_MSC_VER) || defined(__clang__)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#endif

#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SMOKETEST_VISUALIZER_CLASS_NAME "KainSmoketestVisualizerWindow"

enum {
    SMOKETEST_VIEW_OVERVIEW = 0,
    SMOKETEST_VIEW_RUNTIME = 1,
    SMOKETEST_VIEW_STDLIB = 2,
    SMOKETEST_VIEW_TELEMETRY = 3,
    SMOKETEST_VIEW_COUNT = 4
};

typedef struct {
    HINSTANCE instance;
    HWND hwnd;
    HDC dc;
    HGLRC glrc;
    HFONT font;
    GLuint font_list_base;
    int width;
    int height;
    int frame_budget;
    int should_close;
    int total_tracks;
    int passed_tracks;
    int composition_checksum;
    int semantics_tracks;
    int systems_tracks;
    int gpu_tracks;
    int stdlib_tracks;
    int interop_tracks;
    int telemetry_tracks;
    int ui_tracks;
    int patch_journal;
    int entangle_propagations;
    int converge_mismatches;
    int pulse_count;
    int actor_enqueued;
    int ui_hash;
    int ui_draws;
    int graphics_draws;
    int current_view;
    int hover_view;
    int auto_cycle;
    int mouse_x;
    int mouse_y;
} SmoketestVisualizerState;

static SmoketestVisualizerState g_visualizer_state = {0};
static int g_frames_presented = 0;
static int g_cells_drawn = 0;
static char g_last_error[256] = "ok";

static void smoketest_copy_text(char* out_text, size_t out_cap, const char* text) {
    if (!out_text || out_cap == 0) {
        return;
    }
    if (!text) {
        text = "";
    }
    snprintf(out_text, out_cap, "%s", text);
}

static void smoketest_set_error(const char* text) {
    smoketest_copy_text(g_last_error, sizeof(g_last_error), text);
}

static float smoketest_clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float smoketest_ratio(int value, int max_value) {
    if (max_value <= 0) {
        return 0.0f;
    }
    return smoketest_clampf((float)value / (float)max_value, 0.0f, 1.0f);
}

static float smoketest_ring_unit(int value, int modulus) {
    int lane;
    if (modulus <= 1) {
        return 0.0f;
    }
    lane = value % modulus;
    if (lane < 0) {
        lane += modulus;
    }
    return (float)lane / (float)(modulus - 1);
}

static void smoketest_draw_quad(float left, float top, float right, float bottom, float red, float green, float blue, float alpha) {
    glColor4f(red, green, blue, alpha);
    glBegin(GL_QUADS);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
}

static void smoketest_draw_outline(float left, float top, float right, float bottom, float thickness, float red, float green, float blue, float alpha) {
    smoketest_draw_quad(left, top, right, top + thickness, red, green, blue, alpha);
    smoketest_draw_quad(left, bottom - thickness, right, bottom, red, green, blue, alpha);
    smoketest_draw_quad(left, top, left + thickness, bottom, red, green, blue, alpha);
    smoketest_draw_quad(right - thickness, top, right, bottom, red, green, blue, alpha);
}

static void smoketest_draw_scanlines(int width, int height) {
    int y;
    glBegin(GL_LINES);
    for (y = 0; y < height; y += 4) {
        float alpha = (y % 8 == 0) ? 0.05f : 0.025f;
        glColor4f(0.05f, 0.08f, 0.10f, alpha);
        glVertex2f(0.0f, (float)y + 0.5f);
        glVertex2f((float)width, (float)y + 0.5f);
    }
    glEnd();
}

static void smoketest_draw_text(SmoketestVisualizerState* state, float x, float y, float red, float green, float blue, const char* text) {
    GLubyte glyph_indices[256];
    size_t text_length;
    size_t index;

    if (!state || !state->font_list_base || !text) {
        return;
    }
    text_length = strlen(text);
    if (text_length == 0) {
        return;
    }
    if (text_length > sizeof(glyph_indices)) {
        text_length = sizeof(glyph_indices);
    }
    for (index = 0; index < text_length; index += 1) {
        unsigned char glyph = (unsigned char)text[index];
        if (glyph < 32u || glyph > 127u) {
            glyph = (unsigned char)'?';
        }
        glyph_indices[index] = (GLubyte)(glyph - 32u);
    }
    glColor3f(red, green, blue);
    glRasterPos2f(x, y);
    glListBase(state->font_list_base);
    glCallLists((GLsizei)text_length, GL_UNSIGNED_BYTE, glyph_indices);
}

static void smoketest_draw_bar(
    SmoketestVisualizerState* state,
    float left,
    float top,
    float width,
    float height,
    float ratio,
    float red,
    float green,
    float blue,
    const char* label,
    const char* value_text
) {
    float clamped = smoketest_clampf(ratio, 0.0f, 1.0f);
    smoketest_draw_quad(left, top, left + width, top + height, 0.08f, 0.10f, 0.14f, 0.96f);
    smoketest_draw_quad(left, top, left + (width * clamped), top + height, red, green, blue, 0.92f);
    smoketest_draw_outline(left, top, left + width, top + height, 1.0f, red * 0.85f, green * 0.85f, blue * 0.85f, 1.0f);
    smoketest_draw_text(state, left + 10.0f, top + 18.0f, 0.96f, 0.97f, 1.0f, label);
    smoketest_draw_text(state, left + width - 120.0f, top + 18.0f, 0.85f, 0.92f, 0.98f, value_text);
}

static const char* smoketest_view_name(int view) {
    switch (view) {
        case SMOKETEST_VIEW_RUNTIME:
            return "RUNTIME";
        case SMOKETEST_VIEW_STDLIB:
            return "STDLIB";
        case SMOKETEST_VIEW_TELEMETRY:
            return "TELEMETRY";
        case SMOKETEST_VIEW_OVERVIEW:
        default:
            return "OVERVIEW";
    }
}

static const char* smoketest_view_description(int view) {
    switch (view) {
        case SMOKETEST_VIEW_RUNTIME:
            return "runtime counters, pulse pressure, actor enqueue volume, and proof-health bars";
        case SMOKETEST_VIEW_STDLIB:
            return "album category spread with the stdlib slab highlighted as the deepest public surface";
        case SMOKETEST_VIEW_TELEMETRY:
            return "Kain-authored std::ui hash, draw pressure, graphics draw count, and completion telemetry";
        case SMOKETEST_VIEW_OVERVIEW:
        default:
            return "whole-album progress, category topology, and interactive OpenGL proof that smoketest is no longer headless";
    }
}

static void smoketest_button_rect(const SmoketestVisualizerState* state, int index, RECT* rect) {
    int width = 132;
    int height = 30;
    int gap = 12;
    int total_width = (SMOKETEST_VIEW_COUNT * width) + ((SMOKETEST_VIEW_COUNT - 1) * gap);
    int left;
    int top;

    if (!rect) {
        return;
    }
    if (!state) {
        SetRect(rect, 0, 0, 0, 0);
        return;
    }

    left = (state->width - total_width) / 2;
    top = state->height - 50;
    SetRect(rect, left + (index * (width + gap)), top, left + (index * (width + gap)) + width, top + height);
}

static int smoketest_hit_button(const SmoketestVisualizerState* state, int x, int y) {
    int index;
    for (index = 0; index < SMOKETEST_VIEW_COUNT; index += 1) {
        RECT rect;
        smoketest_button_rect(state, index, &rect);
        if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) {
            return index;
        }
    }
    return -1;
}

static void smoketest_trim_newline(char* text) {
    size_t length;
    if (!text) {
        return;
    }
    length = strlen(text);
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r' || text[length - 1] == ' ' || text[length - 1] == '\t')) {
        text[length - 1] = '\0';
        length -= 1;
    }
}

static void smoketest_apply_kv(SmoketestVisualizerState* state, const char* key, const char* value) {
    int parsed;
    if (!state || !key || !value) {
        return;
    }
    parsed = atoi(value);
    if (strcmp(key, "total_tracks") == 0) {
        state->total_tracks = parsed;
    } else if (strcmp(key, "passed_tracks") == 0) {
        state->passed_tracks = parsed;
    } else if (strcmp(key, "composition_checksum") == 0) {
        state->composition_checksum = parsed;
    } else if (strcmp(key, "semantics_tracks") == 0) {
        state->semantics_tracks = parsed;
    } else if (strcmp(key, "systems_tracks") == 0) {
        state->systems_tracks = parsed;
    } else if (strcmp(key, "gpu_tracks") == 0) {
        state->gpu_tracks = parsed;
    } else if (strcmp(key, "stdlib_tracks") == 0) {
        state->stdlib_tracks = parsed;
    } else if (strcmp(key, "interop_tracks") == 0) {
        state->interop_tracks = parsed;
    } else if (strcmp(key, "telemetry_tracks") == 0) {
        state->telemetry_tracks = parsed;
    } else if (strcmp(key, "ui_tracks") == 0) {
        state->ui_tracks = parsed;
    } else if (strcmp(key, "patch_journal") == 0) {
        state->patch_journal = parsed;
    } else if (strcmp(key, "entangle_propagations") == 0) {
        state->entangle_propagations = parsed;
    } else if (strcmp(key, "converge_mismatches") == 0) {
        state->converge_mismatches = parsed;
    } else if (strcmp(key, "pulse_count") == 0) {
        state->pulse_count = parsed;
    } else if (strcmp(key, "actor_enqueued") == 0) {
        state->actor_enqueued = parsed;
    } else if (strcmp(key, "ui_hash") == 0) {
        state->ui_hash = parsed;
    } else if (strcmp(key, "ui_draws") == 0) {
        state->ui_draws = parsed;
    } else if (strcmp(key, "graphics_draws") == 0) {
        state->graphics_draws = parsed;
    }
}

static int smoketest_load_input_file(SmoketestVisualizerState* state, const char* path) {
    FILE* file;
    char line[256];
    if (!state || !path || !path[0]) {
        smoketest_set_error("missing visualizer input path");
        return 0;
    }
    file = fopen(path, "rb");
    if (!file) {
        smoketest_set_error("failed to open visualizer input file");
        return 0;
    }
    while (fgets(line, sizeof(line), file)) {
        char* equals = strchr(line, '=');
        char* value;
        if (!equals) {
            continue;
        }
        *equals = '\0';
        value = equals + 1;
        smoketest_trim_newline(line);
        smoketest_trim_newline(value);
        smoketest_apply_kv(state, line, value);
    }
    fclose(file);
    return 1;
}

static int smoketest_boot_font(SmoketestVisualizerState* state) {
    HFONT font;
    GLuint list_base;

    if (!state || !state->dc) {
        return 0;
    }

    font = (HFONT)GetStockObject(OEM_FIXED_FONT);
    if (!font) {
        font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
    }
    if (!font) {
        smoketest_set_error("failed to acquire fixed font");
        return 0;
    }

    list_base = glGenLists(96);
    if (!list_base) {
        smoketest_set_error("glGenLists failed");
        return 0;
    }

    SelectObject(state->dc, font);
    if (!wglUseFontBitmapsA(state->dc, 32, 96, list_base)) {
        glDeleteLists(list_base, 96);
        smoketest_set_error("wglUseFontBitmaps failed");
        return 0;
    }

    state->font = font;
    state->font_list_base = list_base;
    return 1;
}

static void smoketest_shutdown_window(SmoketestVisualizerState* state) {
    if (!state) {
        return;
    }
    if (state->font_list_base) {
        glDeleteLists(state->font_list_base, 96);
        state->font_list_base = 0;
        state->font = NULL;
    }
    if (state->glrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(state->glrc);
        state->glrc = NULL;
    }
    if (state->hwnd && state->dc) {
        ReleaseDC(state->hwnd, state->dc);
        state->dc = NULL;
    }
    if (state->hwnd) {
        DestroyWindow(state->hwnd);
        state->hwnd = NULL;
    }
}

static LRESULT CALLBACK smoketest_window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    SmoketestVisualizerState* state = (SmoketestVisualizerState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTA* create = (CREATESTRUCTA*)l_param;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
            return DefWindowProcA(hwnd, message, w_param, l_param);
        }
        case WM_CLOSE:
            if (state) {
                state->should_close = 1;
            }
            return 0;
        case WM_DESTROY:
            if (state) {
                state->should_close = 1;
            }
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (state) {
                state->width = LOWORD(l_param) > 0 ? (int)LOWORD(l_param) : state->width;
                state->height = HIWORD(l_param) > 0 ? (int)HIWORD(l_param) : state->height;
            }
            return 0;
        case WM_MOUSEMOVE:
            if (state) {
                state->mouse_x = GET_X_LPARAM(l_param);
                state->mouse_y = GET_Y_LPARAM(l_param);
                state->hover_view = smoketest_hit_button(state, state->mouse_x, state->mouse_y);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (state) {
                int hit = smoketest_hit_button(state, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
                if (hit >= 0) {
                    state->current_view = hit;
                    state->auto_cycle = 0;
                    SetFocus(hwnd);
                }
            }
            return 0;
        case WM_KEYDOWN:
            if (state) {
                switch (w_param) {
                    case '1':
                        state->current_view = SMOKETEST_VIEW_OVERVIEW;
                        state->auto_cycle = 0;
                        return 0;
                    case '2':
                        state->current_view = SMOKETEST_VIEW_RUNTIME;
                        state->auto_cycle = 0;
                        return 0;
                    case '3':
                        state->current_view = SMOKETEST_VIEW_STDLIB;
                        state->auto_cycle = 0;
                        return 0;
                    case '4':
                        state->current_view = SMOKETEST_VIEW_TELEMETRY;
                        state->auto_cycle = 0;
                        return 0;
                    case VK_SPACE:
                        state->auto_cycle = state->auto_cycle ? 0 : 1;
                        return 0;
                    default:
                        break;
                }
            }
            break;
        default:
            return DefWindowProcA(hwnd, message, w_param, l_param);
    }
    return DefWindowProcA(hwnd, message, w_param, l_param);
}

static int smoketest_register_class(HINSTANCE instance) {
    WNDCLASSA window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = smoketest_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = SMOKETEST_VISUALIZER_CLASS_NAME;
    if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        smoketest_set_error("failed to register smoketest visualizer window");
        return 0;
    }
    return 1;
}

static int smoketest_boot_context(SmoketestVisualizerState* state) {
    PIXELFORMATDESCRIPTOR pfd;
    int pixel_format;

    if (!state || !state->hwnd) {
        smoketest_set_error("missing window for GL boot");
        return 0;
    }

    state->dc = GetDC(state->hwnd);
    if (!state->dc) {
        smoketest_set_error("failed to acquire window device context");
        return 0;
    }

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    pixel_format = ChoosePixelFormat(state->dc, &pfd);
    if (!pixel_format) {
        smoketest_set_error("ChoosePixelFormat failed");
        return 0;
    }
    if (!SetPixelFormat(state->dc, pixel_format, &pfd)) {
        smoketest_set_error("SetPixelFormat failed");
        return 0;
    }
    state->glrc = wglCreateContext(state->dc);
    if (!state->glrc) {
        smoketest_set_error("wglCreateContext failed");
        return 0;
    }
    if (!wglMakeCurrent(state->dc, state->glrc)) {
        smoketest_set_error("wglMakeCurrent failed");
        return 0;
    }

    glDisable(GL_DITHER);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!smoketest_boot_font(state)) {
        return 0;
    }
    return 1;
}

static int smoketest_create_window(SmoketestVisualizerState* state, const char* title) {
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT rect;

    if (!state) {
        smoketest_set_error("missing window state");
        return 0;
    }
    if (!smoketest_register_class(state->instance)) {
        return 0;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = state->width > 0 ? state->width : 1440;
    rect.bottom = state->height > 0 ? state->height : 880;
    AdjustWindowRect(&rect, style, FALSE);

    state->hwnd = CreateWindowExA(
        0,
        SMOKETEST_VISUALIZER_CLASS_NAME,
        title && title[0] ? title : "Kain Smoketest Visualizer",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        state->instance,
        state
    );
    if (!state->hwnd) {
        smoketest_set_error("failed to create smoketest visualizer window");
        return 0;
    }

    ShowWindow(state->hwnd, SW_SHOW);
    UpdateWindow(state->hwnd);
    return smoketest_boot_context(state);
}

static void smoketest_update_title(SmoketestVisualizerState* state) {
    char title[256];
    if (!state || !state->hwnd) {
        return;
    }
    snprintf(
        title,
        sizeof(title),
        "Kain Smoketest // %s // tracks=%d/%d // checksum=%d",
        smoketest_view_name(state->current_view),
        state->passed_tracks,
        state->total_tracks,
        state->composition_checksum
    );
    SetWindowTextA(state->hwnd, title);
}

static void smoketest_render_frame(SmoketestVisualizerState* state, int frame_index) {
    static const char* category_names[7] = {
        "Semantics",
        "Systems",
        "GPU",
        "Stdlib",
        "Interop",
        "Telemetry",
        "UI"
    };
    int category_values[7];
    int i;
    float phase;
    float progress_ratio;
    float checksum_ratio;
    char text[256];
    char value_text[64];

    category_values[0] = state->semantics_tracks;
    category_values[1] = state->systems_tracks;
    category_values[2] = state->gpu_tracks;
    category_values[3] = state->stdlib_tracks;
    category_values[4] = state->interop_tracks;
    category_values[5] = state->telemetry_tracks;
    category_values[6] = state->ui_tracks;

    progress_ratio = smoketest_ratio(state->passed_tracks, state->total_tracks);
    checksum_ratio = smoketest_ring_unit(state->composition_checksum, 1000000007);
    phase = (float)frame_index * 0.045f;

    glViewport(0, 0, state->width, state->height);
    glClearColor(0.04f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)state->width, (double)state->height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    smoketest_draw_quad(0.0f, 0.0f, (float)state->width, (float)state->height, 0.04f, 0.05f, 0.08f, 1.0f);
    smoketest_draw_quad(0.0f, 0.0f, (float)state->width, 180.0f, 0.08f, 0.10f, 0.16f, 1.0f);
    smoketest_draw_quad(0.0f, 180.0f, (float)state->width, 520.0f, 0.06f, 0.07f, 0.10f, 1.0f);
    smoketest_draw_quad(0.0f, 520.0f, (float)state->width, (float)state->height, 0.05f, 0.07f, 0.09f, 1.0f);
    smoketest_draw_scanlines(state->width, state->height);

    smoketest_draw_quad(42.0f, 38.0f, 706.0f, 154.0f, 0.10f, 0.14f, 0.20f, 0.96f);
    smoketest_draw_quad(734.0f, 38.0f, 1398.0f, 154.0f, 0.11f, 0.12f, 0.18f, 0.96f);
    smoketest_draw_outline(42.0f, 38.0f, 706.0f, 154.0f, 1.0f, 0.25f, 0.47f, 0.62f, 1.0f);
    smoketest_draw_outline(734.0f, 38.0f, 1398.0f, 154.0f, 1.0f, 0.45f, 0.32f, 0.24f, 1.0f);

    smoketest_draw_text(state, 68.0f, 68.0f, 0.98f, 0.97f, 0.92f, "Kain Smoketest Album");
    smoketest_draw_text(state, 68.0f, 94.0f, 0.72f, 0.84f, 0.94f, smoketest_view_description(state->current_view));
    smoketest_draw_text(state, 68.0f, 124.0f, 0.92f, 0.93f, 0.98f, "Press 1-4 to switch views, click buttons, space to auto-cycle.");

    snprintf(text, sizeof(text), "Album progress %d/%d // OpenGL presenter is live", state->passed_tracks, state->total_tracks);
    smoketest_draw_text(state, 760.0f, 70.0f, 0.98f, 0.97f, 0.92f, text);
    snprintf(text, sizeof(text), "Composition checksum %d // ui hash %d", state->composition_checksum, state->ui_hash);
    smoketest_draw_text(state, 760.0f, 96.0f, 0.84f, 0.91f, 0.97f, text);
    snprintf(text, sizeof(text), "graphics draws %d // ui draws %d // view %s", state->graphics_draws, state->ui_draws, smoketest_view_name(state->current_view));
    smoketest_draw_text(state, 760.0f, 122.0f, 0.82f, 0.88f, 0.94f, text);

    smoketest_draw_bar(state, 62.0f, 178.0f, 612.0f, 28.0f, progress_ratio, 0.20f, 0.76f, 0.64f, "Track Completion", "live");
    smoketest_draw_bar(state, 62.0f, 218.0f, 612.0f, 28.0f, checksum_ratio, 0.92f, 0.54f, 0.28f, "Checksum Ring", "ring");

    if (state->current_view == SMOKETEST_VIEW_RUNTIME) {
        snprintf(value_text, sizeof(value_text), "%d", state->patch_journal);
        smoketest_draw_bar(state, 744.0f, 178.0f, 612.0f, 28.0f, smoketest_ring_unit(state->patch_journal, 256), 0.33f, 0.73f, 0.94f, "Patch Journal", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->entangle_propagations);
        smoketest_draw_bar(state, 744.0f, 218.0f, 612.0f, 28.0f, smoketest_ring_unit(state->entangle_propagations, 256), 0.27f, 0.86f, 0.68f, "Entangle Propagations", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->pulse_count);
        smoketest_draw_bar(state, 744.0f, 258.0f, 612.0f, 28.0f, smoketest_ring_unit(state->pulse_count, 1024), 0.87f, 0.58f, 0.30f, "Pulse Fires", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->actor_enqueued);
        smoketest_draw_bar(state, 744.0f, 298.0f, 612.0f, 28.0f, smoketest_ring_unit(state->actor_enqueued, 2048), 0.84f, 0.42f, 0.56f, "Actor Enqueued", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->converge_mismatches);
        smoketest_draw_bar(state, 744.0f, 338.0f, 612.0f, 28.0f, 1.0f - smoketest_ring_unit(state->converge_mismatches, 8), 0.68f, 0.82f, 0.34f, "Converge Health", value_text);
    } else if (state->current_view == SMOKETEST_VIEW_TELEMETRY) {
        snprintf(value_text, sizeof(value_text), "%d", state->ui_hash);
        smoketest_draw_bar(state, 744.0f, 178.0f, 612.0f, 28.0f, smoketest_ring_unit(state->ui_hash, 1000000007), 0.29f, 0.67f, 0.97f, "UI Frame Hash", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->ui_draws);
        smoketest_draw_bar(state, 744.0f, 218.0f, 612.0f, 28.0f, smoketest_ring_unit(state->ui_draws, 128), 0.25f, 0.84f, 0.71f, "UI Draw Commands", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->graphics_draws);
        smoketest_draw_bar(state, 744.0f, 258.0f, 612.0f, 28.0f, smoketest_ring_unit(state->graphics_draws, 128), 0.92f, 0.54f, 0.29f, "Graphics Draw Commands", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->passed_tracks);
        smoketest_draw_bar(state, 744.0f, 298.0f, 612.0f, 28.0f, progress_ratio, 0.85f, 0.42f, 0.58f, "Succeeded Tracks", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->total_tracks);
        smoketest_draw_bar(state, 744.0f, 338.0f, 612.0f, 28.0f, smoketest_ratio(state->total_tracks, 64), 0.67f, 0.82f, 0.34f, "Album Span", value_text);
    } else {
        snprintf(value_text, sizeof(value_text), "%d", state->total_tracks);
        smoketest_draw_bar(state, 744.0f, 178.0f, 612.0f, 28.0f, smoketest_ratio(state->semantics_tracks + state->systems_tracks, state->total_tracks), 0.29f, 0.67f, 0.97f, "Core Surface", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->stdlib_tracks);
        smoketest_draw_bar(state, 744.0f, 218.0f, 612.0f, 28.0f, smoketest_ratio(state->stdlib_tracks, 22), 0.72f, 0.47f, 0.94f, "Stdlib Slab", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->ui_tracks);
        smoketest_draw_bar(state, 744.0f, 258.0f, 612.0f, 28.0f, smoketest_ratio(state->ui_tracks, 2), 0.27f, 0.86f, 0.68f, "UI Expansion", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->patch_journal);
        smoketest_draw_bar(state, 744.0f, 298.0f, 612.0f, 28.0f, smoketest_ring_unit(state->patch_journal, 256), 0.92f, 0.54f, 0.29f, "Mutation Pressure", value_text);
        snprintf(value_text, sizeof(value_text), "%d", state->ui_hash);
        smoketest_draw_bar(state, 744.0f, 338.0f, 612.0f, 28.0f, smoketest_ring_unit(state->ui_hash, 1000000007), 0.84f, 0.42f, 0.58f, "UI Fingerprint", value_text);
    }

    for (i = 0; i < 7; i += 1) {
        float y = 396.0f + ((float)i * 42.0f);
        float ratio = smoketest_ratio(category_values[i], 22);
        float red = 0.22f + (0.05f * (float)((i + 1) % 3));
        float green = 0.42f + (0.06f * (float)((i + 2) % 3));
        float blue = 0.58f + (0.05f * (float)(i % 3));
        if (state->current_view == SMOKETEST_VIEW_STDLIB && i == 3) {
            red = 0.87f;
            green = 0.50f;
            blue = 0.29f;
            ratio = smoketest_ratio(category_values[i], 22) * (0.82f + (sinf(phase * 3.0f) * 0.12f));
        }
        snprintf(value_text, sizeof(value_text), "%d", category_values[i]);
        smoketest_draw_bar(state, 62.0f, y, 1294.0f, 24.0f, ratio, red, green, blue, category_names[i], value_text);
    }

    smoketest_draw_quad(64.0f, 708.0f, 1358.0f, 738.0f, 0.08f, 0.10f, 0.14f, 0.96f);
    smoketest_draw_outline(64.0f, 708.0f, 1358.0f, 738.0f, 1.0f, 0.24f, 0.32f, 0.44f, 1.0f);
    snprintf(text, sizeof(text), "headless is dead: std::ui authored the dashboard, OpenGL is visualizing the album, and mode %s is currently live", smoketest_view_name(state->current_view));
    smoketest_draw_text(state, 82.0f, 728.0f, 0.95f, 0.97f, 1.0f, text);

    for (i = 0; i < SMOKETEST_VIEW_COUNT; i += 1) {
        RECT rect;
        float left;
        float top;
        float right;
        float bottom;
        float red = 0.18f;
        float green = 0.22f;
        float blue = 0.28f;
        smoketest_button_rect(state, i, &rect);
        left = (float)rect.left;
        top = (float)rect.top;
        right = (float)rect.right;
        bottom = (float)rect.bottom;
        if (state->current_view == i) {
            red = 0.29f;
            green = 0.64f;
            blue = 0.78f;
        } else if (state->hover_view == i) {
            red = 0.23f;
            green = 0.34f;
            blue = 0.48f;
        }
        smoketest_draw_quad(left, top, right, bottom, red, green, blue, 0.94f);
        smoketest_draw_outline(left, top, right, bottom, 1.0f, red + 0.08f, green + 0.08f, blue + 0.08f, 1.0f);
        smoketest_draw_text(state, left + 16.0f, top + 19.0f, 0.98f, 0.98f, 1.0f, smoketest_view_name(i));
    }

    glFinish();
    SwapBuffers(state->dc);
    g_frames_presented += 1;
    g_cells_drawn += 12;
}

int smoketest_visualizer_native_probe(void) {
    return 1;
}

int smoketest_visualizer_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    const char* input_path
) {
    MSG msg;
    int frame_index = 0;
    ZeroMemory(&g_visualizer_state, sizeof(g_visualizer_state));
    g_frames_presented = 0;
    g_cells_drawn = 0;
    smoketest_set_error("ok");

    g_visualizer_state.instance = GetModuleHandleA(NULL);
    g_visualizer_state.width = width > 0 ? width : 1440;
    g_visualizer_state.height = height > 0 ? height : 880;
    g_visualizer_state.frame_budget = frame_budget;
    g_visualizer_state.current_view = SMOKETEST_VIEW_OVERVIEW;
    g_visualizer_state.hover_view = -1;
    g_visualizer_state.auto_cycle = 1;

    if (!smoketest_load_input_file(&g_visualizer_state, input_path)) {
        return -2;
    }

    if (!smoketest_create_window(&g_visualizer_state, title)) {
        smoketest_shutdown_window(&g_visualizer_state);
        return -1;
    }

    while (!g_visualizer_state.should_close) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_visualizer_state.should_close = 1;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (g_visualizer_state.should_close) {
            break;
        }
        if (g_visualizer_state.frame_budget > 0 && frame_index >= g_visualizer_state.frame_budget) {
            break;
        }
        if (g_visualizer_state.auto_cycle && g_visualizer_state.frame_budget <= 0) {
            g_visualizer_state.current_view = (frame_index / 180) % SMOKETEST_VIEW_COUNT;
        } else if (g_visualizer_state.auto_cycle && g_visualizer_state.frame_budget > 0) {
            g_visualizer_state.current_view = (frame_index / 45) % SMOKETEST_VIEW_COUNT;
        }
        smoketest_update_title(&g_visualizer_state);
        smoketest_render_frame(&g_visualizer_state, frame_index);
        Sleep(16);
        frame_index += 1;
    }

    smoketest_shutdown_window(&g_visualizer_state);
    return 0;
}

int smoketest_visualizer_native_frames_presented(void) {
    return g_frames_presented;
}

int smoketest_visualizer_native_cells_drawn(void) {
    return g_cells_drawn;
}

int smoketest_visualizer_native_write_report(const char* path) {
    FILE* file;
    if (!path || !path[0]) {
        smoketest_set_error("missing report path");
        return -1;
    }
    file = fopen(path, "wb");
    if (!file) {
        smoketest_set_error("failed to open report path");
        return -2;
    }
    fprintf(file, "frames=%d\n", g_frames_presented);
    fprintf(file, "cells=%d\n", g_cells_drawn);
    fprintf(file, "view=%s\n", smoketest_view_name(g_visualizer_state.current_view));
    fprintf(file, "tracks=%d/%d\n", g_visualizer_state.passed_tracks, g_visualizer_state.total_tracks);
    fprintf(file, "checksum=%d\n", g_visualizer_state.composition_checksum);
    fprintf(file, "patch_journal=%d\n", g_visualizer_state.patch_journal);
    fprintf(file, "entangle_propagations=%d\n", g_visualizer_state.entangle_propagations);
    fprintf(file, "converge_mismatches=%d\n", g_visualizer_state.converge_mismatches);
    fprintf(file, "pulse_count=%d\n", g_visualizer_state.pulse_count);
    fprintf(file, "actor_enqueued=%d\n", g_visualizer_state.actor_enqueued);
    fprintf(file, "ui_hash=%d\n", g_visualizer_state.ui_hash);
    fprintf(file, "ui_draws=%d\n", g_visualizer_state.ui_draws);
    fprintf(file, "graphics_draws=%d\n", g_visualizer_state.graphics_draws);
    fprintf(file, "last_error=%s\n", g_last_error);
    fclose(file);
    return 0;
}

#else

int smoketest_visualizer_native_probe(void) {
    return 0;
}

int smoketest_visualizer_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    const char* input_path
) {
    (void)title;
    (void)width;
    (void)height;
    (void)frame_budget;
    (void)input_path;
    return -1;
}

int smoketest_visualizer_native_frames_presented(void) {
    return 0;
}

int smoketest_visualizer_native_cells_drawn(void) {
    return 0;
}

int smoketest_visualizer_native_write_report(const char* path) {
    (void)path;
    return -1;
}

#endif

int smoketest_visualizer_bridge_probe(void) {
    return smoketest_visualizer_native_probe();
}

int smoketest_visualizer_bridge_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    const char* input_path
) {
    return smoketest_visualizer_native_run_window(title, width, height, frame_budget, input_path);
}

int smoketest_visualizer_bridge_frames_presented(void) {
    return smoketest_visualizer_native_frames_presented();
}

int smoketest_visualizer_bridge_cells_drawn(void) {
    return smoketest_visualizer_native_cells_drawn();
}

int smoketest_visualizer_bridge_write_report(const char* path) {
    return smoketest_visualizer_native_write_report(path);
}
