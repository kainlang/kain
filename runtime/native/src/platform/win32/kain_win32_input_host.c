#include "../../../include/kain_runtime_win32.h"

#ifdef _WIN32
static void kain_win32_mouse_capture_sync(KainWin32MouseCapture* capture) {
    if (!capture || !capture->hwnd) {
        return;
    }

    if (capture->pointer_locked || capture->drag_capture_count > 0) {
        SetCapture(capture->hwnd);
    } else {
        ReleaseCapture();
    }

    if (capture->pointer_locked && !capture->cursor_hidden) {
        ShowCursor(FALSE);
        capture->cursor_hidden = 1;
    } else if (!capture->pointer_locked && capture->cursor_hidden) {
        ShowCursor(TRUE);
        capture->cursor_hidden = 0;
    }
}

static void kain_win32_mouse_capture_recenter(KainWin32MouseCapture* capture) {
    RECT rect;
    POINT center;

    if (!capture || !capture->hwnd) {
        return;
    }

    GetClientRect(capture->hwnd, &rect);
    center.x = (rect.right - rect.left) / 2;
    center.y = (rect.bottom - rect.top) / 2;
    ClientToScreen(capture->hwnd, &center);
    SetCursorPos(center.x, center.y);
}

void kain_win32_mouse_capture_bind(KainWin32MouseCapture* capture, HWND hwnd) {
    if (!capture) {
        return;
    }

    capture->hwnd = hwnd;
}

void kain_win32_mouse_capture_set_pointer_lock(KainWin32MouseCapture* capture, int enabled) {
    if (!capture) {
        return;
    }

    capture->pointer_locked = enabled ? 1 : 0;
    if (capture->pointer_locked) {
        kain_win32_mouse_capture_recenter(capture);
    }
    kain_win32_mouse_capture_sync(capture);
}

void kain_win32_mouse_capture_begin_drag(KainWin32MouseCapture* capture, HWND hwnd) {
    if (!capture) {
        return;
    }

    if (hwnd) {
        capture->hwnd = hwnd;
    }
    capture->drag_capture_count += 1;
    kain_win32_mouse_capture_sync(capture);
}

void kain_win32_mouse_capture_end_drag(KainWin32MouseCapture* capture) {
    if (!capture) {
        return;
    }

    if (capture->drag_capture_count > 0) {
        capture->drag_capture_count -= 1;
    }
    kain_win32_mouse_capture_sync(capture);
}

void kain_win32_mouse_capture_release_all(KainWin32MouseCapture* capture) {
    if (!capture) {
        return;
    }

    capture->pointer_locked = 0;
    capture->drag_capture_count = 0;
    kain_win32_mouse_capture_sync(capture);
}

int kain_win32_mouse_capture_sample_relative(KainWin32MouseCapture* capture, int* delta_x, int* delta_y) {
    RECT rect;
    POINT center;
    POINT cursor;

    if (delta_x) {
        *delta_x = 0;
    }
    if (delta_y) {
        *delta_y = 0;
    }
    if (!capture || !capture->pointer_locked || !capture->hwnd) {
        return 0;
    }

    GetClientRect(capture->hwnd, &rect);
    center.x = (rect.right - rect.left) / 2;
    center.y = (rect.bottom - rect.top) / 2;
    ClientToScreen(capture->hwnd, &center);
    GetCursorPos(&cursor);

    if (delta_x) {
        *delta_x = cursor.x - center.x;
    }
    if (delta_y) {
        *delta_y = cursor.y - center.y;
    }

    SetCursorPos(center.x, center.y);
    return 1;
}
#endif
