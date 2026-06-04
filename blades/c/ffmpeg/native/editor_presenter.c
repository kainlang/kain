#include "editor_presenter.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    KAIN_EDITOR_MAX_PRESENTERS = 8,
    KAIN_EDITOR_ERROR_CAP = 512
};

typedef struct KainEditorPresenter {
    int used;
    int width;
    int height;
    int frame_count;
    long long frame_hash;
    uint32_t* pixels;
#if defined(_WIN32)
    HWND hwnd;
#endif
} KainEditorPresenter;

static KainEditorPresenter G_PRESENTERS[KAIN_EDITOR_MAX_PRESENTERS];
static char G_ERROR[KAIN_EDITOR_ERROR_CAP] = "ok";
static int G_STATUS = 0;

static int set_status(int status, const char* message) {
    G_STATUS = status;
    snprintf(G_ERROR, sizeof(G_ERROR), "%s", message && message[0] ? message : "ok");
    return status;
}

static int presenter_slot(void) {
    for (int index = 0; index < KAIN_EDITOR_MAX_PRESENTERS; ++index) {
        if (!G_PRESENTERS[index].used) {
            return index;
        }
    }
    return -1;
}

static KainEditorPresenter* presenter_from_handle(int handle) {
    int index = handle - 1;
    if (index < 0 || index >= KAIN_EDITOR_MAX_PRESENTERS || !G_PRESENTERS[index].used) {
        set_status(-2001, "invalid presenter handle");
        return NULL;
    }
    return &G_PRESENTERS[index];
}

static long long hash_pixels(const uint32_t* pixels, int pixel_count, int playhead_ms, long long frame_checksum, int clip_count) {
    uint64_t hash = 1469598103934665603ull;
    for (int i = 0; i < pixel_count; ++i) {
        hash ^= (uint64_t)pixels[i];
        hash *= 1099511628211ull;
    }
    hash ^= (uint64_t)playhead_ms;
    hash *= 1099511628211ull;
    hash ^= (uint64_t)frame_checksum;
    hash *= 1099511628211ull;
    hash ^= (uint64_t)clip_count;
    return (long long)(hash & 0x7fffffffffffffffLL);
}

static int ensure_pixels(KainEditorPresenter* presenter, int width, int height) {
    int pixel_count = width * height;
    if (pixel_count <= 0) {
        return set_status(-2002, "invalid presenter dimensions");
    }
    if (presenter->pixels && presenter->width == width && presenter->height == height) {
        return 0;
    }
    free(presenter->pixels);
    presenter->pixels = (uint32_t*)calloc((size_t)pixel_count, sizeof(uint32_t));
    if (!presenter->pixels) {
        return set_status(-2003, "failed to allocate presenter pixel buffer");
    }
    presenter->width = width;
    presenter->height = height;
    return 0;
}

static void unpack_words(KainEditorPresenter* presenter, const uint64_t* words, int width, int height, int word_count) {
    int pixel_count = width * height;
    for (int pixel = 0; pixel < pixel_count; ++pixel) {
        if (pixel >= word_count) {
            presenter->pixels[pixel] = 0;
            continue;
        }
        uint64_t packed = words[pixel];
        uint8_t r = (uint8_t)(packed & 0xffu);
        uint8_t g = (uint8_t)((packed >> 8) & 0xffu);
        uint8_t b = (uint8_t)((packed >> 16) & 0xffu);
        presenter->pixels[pixel] = ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    }
}

static void overlay_timeline(KainEditorPresenter* presenter, int playhead_ms, int clip_count) {
    int height = presenter->height;
    int width = presenter->width;
    int bar_top = height > 40 ? height - 34 : 0;
    int safe_clips = clip_count <= 0 ? 1 : clip_count;
    int playhead_x = width > 0 ? (playhead_ms / 17) % width : 0;
    for (int y = bar_top; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t color = 0x00141418u;
            if ((x / 16) % safe_clips == 0) {
                color = 0x00395f48u;
            }
            if (x == playhead_x || x == playhead_x + 1) {
                color = 0x0000f0ffu;
            }
            presenter->pixels[y * width + x] = color;
        }
    }
}

#if defined(_WIN32)
static const char* WINDOW_CLASS = "KainFfmpegEditorPresenter";

static LRESULT CALLBACK presenter_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }
    if (message == WM_DESTROY) {
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

static void ensure_window_class(void) {
    static int registered = 0;
    if (registered) {
        return;
    }
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = presenter_window_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = WINDOW_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    registered = 1;
}

static void draw_presenter(KainEditorPresenter* presenter) {
    if (!presenter || !presenter->hwnd || !presenter->pixels) {
        return;
    }
    HDC dc = GetDC(presenter->hwnd);
    if (!dc) {
        return;
    }
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = presenter->width;
    bmi.bmiHeader.biHeight = -presenter->height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(
        dc,
        0,
        0,
        presenter->width,
        presenter->height,
        0,
        0,
        presenter->width,
        presenter->height,
        presenter->pixels,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );
    ReleaseDC(presenter->hwnd, dc);
}
#endif

int editor_presenter_open(const char* title, int width, int height) {
    int slot = presenter_slot();
    if (slot < 0) {
        return set_status(-2004, "presenter table is full");
    }
    KainEditorPresenter* presenter = &G_PRESENTERS[slot];
    memset(presenter, 0, sizeof(*presenter));
    presenter->used = 1;
    presenter->width = width;
    presenter->height = height;

#if defined(_WIN32)
    ensure_window_class();
    presenter->hwnd = CreateWindowExA(
        0,
        WINDOW_CLASS,
        title && title[0] ? title : "Kain FFmpeg Editor",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width + 24,
        height + 64,
        NULL,
        NULL,
        GetModuleHandleA(NULL),
        NULL
    );
    if (!presenter->hwnd) {
        memset(presenter, 0, sizeof(*presenter));
        return set_status(-2005, "failed to create presenter window");
    }
    ShowWindow(presenter->hwnd, SW_SHOW);
    UpdateWindow(presenter->hwnd);
#else
    (void)title;
#endif

    set_status(0, "ok");
    return slot + 1;
}

int editor_presenter_pump(int presenter_handle) {
    KainEditorPresenter* presenter = presenter_from_handle(presenter_handle);
    if (!presenter) {
        return -1;
    }
#if defined(_WIN32)
    MSG msg;
    while (PeekMessageA(&msg, presenter->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
#endif
    return set_status(0, "ok");
}

int editor_presenter_should_close(int presenter_handle) {
    KainEditorPresenter* presenter = presenter_from_handle(presenter_handle);
    if (!presenter) {
        return 1;
    }
#if defined(_WIN32)
    return presenter->hwnd == NULL || !IsWindow(presenter->hwnd);
#else
    return 0;
#endif
}

int editor_presenter_present_rgba_words(
    int presenter_handle,
    long long words_address,
    int width,
    int height,
    int word_count,
    int playhead_ms,
    long long frame_checksum,
    int clip_count
) {
    KainEditorPresenter* presenter = presenter_from_handle(presenter_handle);
    if (!presenter) {
        return -1;
    }
    if (words_address == 0 || word_count <= 0) {
        return set_status(-2006, "invalid rgba word buffer");
    }
    int status = ensure_pixels(presenter, width, height);
    if (status != 0) {
        return status;
    }
    const uint64_t* words = (const uint64_t*)(uintptr_t)words_address;
    unpack_words(presenter, words, width, height, word_count);
    overlay_timeline(presenter, playhead_ms, clip_count);
    presenter->frame_count += 1;
    presenter->frame_hash = hash_pixels(presenter->pixels, width * height, playhead_ms, frame_checksum, clip_count);
#if defined(_WIN32)
    draw_presenter(presenter);
#endif
    return set_status(0, "ok");
}

int editor_presenter_close(int presenter_handle) {
    KainEditorPresenter* presenter = presenter_from_handle(presenter_handle);
    if (!presenter) {
        return -1;
    }
#if defined(_WIN32)
    if (presenter->hwnd && IsWindow(presenter->hwnd)) {
        DestroyWindow(presenter->hwnd);
    }
#endif
    free(presenter->pixels);
    memset(presenter, 0, sizeof(*presenter));
    return set_status(0, "ok");
}

int editor_presenter_frame_count(int presenter_handle) {
    KainEditorPresenter* presenter = presenter_from_handle(presenter_handle);
    return presenter ? presenter->frame_count : 0;
}

long long editor_presenter_frame_hash(int presenter_handle) {
    KainEditorPresenter* presenter = presenter_from_handle(presenter_handle);
    return presenter ? presenter->frame_hash : 0;
}

int editor_presenter_last_status(void) {
    return G_STATUS;
}

const char* editor_presenter_last_error(void) {
    return G_ERROR;
}
