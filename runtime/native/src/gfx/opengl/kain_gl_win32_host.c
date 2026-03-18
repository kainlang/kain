#include "../../../include/kain_runtime_win32.h"
#include "../../../include/kain_runtime_graphics.h"

#ifdef _WIN32
int kain_win32_gl_boot(HWND hwnd, HDC* dc, HGLRC* glrc) {
    PIXELFORMATDESCRIPTOR pfd;
    int pixel_format;

    if (!hwnd || !dc || !glrc) {
        return 0;
    }

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    *dc = GetDC(hwnd);
    if (!*dc) {
        return 0;
    }

    pixel_format = ChoosePixelFormat(*dc, &pfd);
    if (!pixel_format) {
        ReleaseDC(hwnd, *dc);
        *dc = NULL;
        return 0;
    }

    if (!SetPixelFormat(*dc, pixel_format, &pfd)) {
        ReleaseDC(hwnd, *dc);
        *dc = NULL;
        return 0;
    }

    *glrc = wglCreateContext(*dc);
    if (!*glrc) {
        ReleaseDC(hwnd, *dc);
        *dc = NULL;
        return 0;
    }

    if (!wglMakeCurrent(*dc, *glrc)) {
        wglDeleteContext(*glrc);
        *glrc = NULL;
        ReleaseDC(hwnd, *dc);
        *dc = NULL;
        return 0;
    }

    glDisable(GL_DITHER);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_POINT_SMOOTH);
    return 1;
}

void kain_win32_gl_shutdown(HWND hwnd, HDC* dc, HGLRC* glrc, GLuint* font_base, int* font_ready) {
    if (font_ready && *font_ready && font_base) {
        glDeleteLists(*font_base, 96);
        *font_base = 0;
        *font_ready = 0;
    }

    if (glrc && *glrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(*glrc);
        *glrc = NULL;
    }

    if (hwnd && dc && *dc) {
        ReleaseDC(hwnd, *dc);
        *dc = NULL;
    }
}

void kain_win32_gl_ensure_font(HDC dc, GLuint* font_base, int* font_ready, int pixel_height) {
    HFONT font;
    HGDIOBJ old_font;

    if (!dc || !font_base || !font_ready || *font_ready) {
        return;
    }

    *font_base = glGenLists(96);
    font = CreateFontA(
        -pixel_height,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        FF_DONTCARE | DEFAULT_PITCH,
        "Consolas"
    );
    old_font = SelectObject(dc, font);
    wglUseFontBitmapsA(dc, 32, 96, *font_base);
    SelectObject(dc, old_font);
    DeleteObject(font);
    *font_ready = 1;
}

void kain_win32_gl_draw_text(GLuint font_base, int font_ready, float x, float y, const char* text) {
    if (!font_ready || !text) {
        return;
    }

    glRasterPos2f(x, y);
    glListBase(font_base - 32);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
}

int kain_win32_gl_surface_boot(HWND hwnd, KainWin32GlSurface* surface, int font_pixel_height) {
    if (!surface) {
        return 0;
    }

    ZeroMemory(surface, sizeof(*surface));
    surface->font_pixel_height = font_pixel_height > 0 ? font_pixel_height : 16;
    if (!kain_win32_gl_boot(hwnd, &surface->dc, &surface->glrc)) {
        return 0;
    }

    kain_win32_gl_ensure_font(surface->dc, &surface->font_base, &surface->font_ready, surface->font_pixel_height);
    return 1;
}

void kain_win32_gl_surface_shutdown(HWND hwnd, KainWin32GlSurface* surface) {
    if (!surface) {
        return;
    }

    kain_win32_gl_shutdown(hwnd, &surface->dc, &surface->glrc, &surface->font_base, &surface->font_ready);
    surface->font_pixel_height = 0;
}

void kain_win32_gl_surface_present(KainWin32GlSurface* surface) {
    if (!surface || !surface->dc) {
        return;
    }

    SwapBuffers(surface->dc);
}

void kain_win32_gl_surface_draw_text(KainWin32GlSurface* surface, float x, float y, const char* text) {
    if (!surface) {
        return;
    }

    kain_win32_gl_draw_text(surface->font_base, surface->font_ready, x, y, text);
}

void kain_win32_frame_timer_begin(LARGE_INTEGER* perf_freq, LARGE_INTEGER* prev_counter, double* fps_accumulator, int* fps_frames, double* frame_fps) {
    if (!perf_freq || !prev_counter) {
        return;
    }

    QueryPerformanceFrequency(perf_freq);
    QueryPerformanceCounter(prev_counter);

    if (fps_accumulator) {
        *fps_accumulator = 0.0;
    }
    if (fps_frames) {
        *fps_frames = 0;
    }
    if (frame_fps) {
        *frame_fps = 0.0;
    }
}

double kain_win32_frame_timer_step(LARGE_INTEGER* perf_freq, LARGE_INTEGER* prev_counter, double* fps_accumulator, int* fps_frames, double* frame_fps, double min_dt, double max_dt) {
    LARGE_INTEGER current_counter;
    double frame_delta;

    if (!perf_freq || !prev_counter || perf_freq->QuadPart == 0) {
        return min_dt;
    }

    QueryPerformanceCounter(&current_counter);
    frame_delta = (double)(current_counter.QuadPart - prev_counter->QuadPart) / (double)perf_freq->QuadPart;
    prev_counter->QuadPart = current_counter.QuadPart;
    frame_delta = kain_clampd(frame_delta, min_dt, max_dt);

    if (fps_accumulator) {
        *fps_accumulator += frame_delta;
    }
    if (fps_frames) {
        *fps_frames += 1;
    }
    if (fps_accumulator && fps_frames && frame_fps && *fps_accumulator >= 0.25) {
        *frame_fps = (double)(*fps_frames) / *fps_accumulator;
        *fps_accumulator = 0.0;
        *fps_frames = 0;
    }

    return frame_delta;
}

int kain_win32_gl_surface_supports_graphics_bundle(const KainRuntimeGraphicsBundle* bundle) {
    KainRuntimeGraphicsValidation validation;
    if (!bundle) {
        return 0;
    }
    if (!kain_runtime_graphics_validate_bundle(bundle, &validation)) {
        return 0;
    }
    return validation.gl_lane_ready;
}
#endif
