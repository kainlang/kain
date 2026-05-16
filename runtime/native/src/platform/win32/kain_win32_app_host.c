#include "../../../include/win32.h"

#ifdef _WIN32
static LRESULT CALLBACK kain_win32_app_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    KainWin32AppHost* host = (KainWin32AppHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        CREATESTRUCTA* create_struct = (CREATESTRUCTA*)l_param;
        host = (KainWin32AppHost*)create_struct->lpCreateParams;
        if (!host) {
            return FALSE;
        }
        host->hwnd = hwnd;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)host);
        return TRUE;
    }

    if (host && msg == WM_SIZE) {
        host->width = LOWORD(l_param) > 0 ? LOWORD(l_param) : host->width;
        host->height = HIWORD(l_param) > 0 ? HIWORD(l_param) : host->height;
    }

    if (host && host->config && host->config->on_message) {
        int handled = 0;
        LRESULT result = host->config->on_message(host, host->user_data, hwnd, msg, w_param, l_param, &handled);
        if (handled) {
            return result;
        }
    }

    switch (msg) {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (host) {
                host->running = 0;
            }
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
    }

    return DefWindowProcA(hwnd, msg, w_param, l_param);
}

int kain_win32_app_run(KainWin32AppHost* host, const KainWin32AppConfig* config, void* user_data) {
    WNDCLASSA wc;
    MSG msg;
    ATOM registered_class = 0;
    const char* class_name;
    const char* window_title;
    int width;
    int height;
    int show_command;
    int sleep_millis;
    double min_frame_delta;
    double max_frame_delta;

    if (!host || !config || !config->class_name || !config->class_name[0] || !config->on_frame) {
        return 0;
    }

    ZeroMemory(host, sizeof(*host));
    ZeroMemory(&wc, sizeof(wc));

    class_name = config->class_name;
    window_title = (config->window_title && config->window_title[0]) ? config->window_title : class_name;
    width = config->default_width > 0 ? config->default_width : 1280;
    height = config->default_height > 0 ? config->default_height : 720;
    show_command = config->show_command != 0 ? config->show_command : SW_SHOW;
    sleep_millis = config->sleep_millis >= 0 ? config->sleep_millis : 1;
    min_frame_delta = config->min_frame_delta > 0.0 ? config->min_frame_delta : 0.001;
    max_frame_delta = config->max_frame_delta > min_frame_delta ? config->max_frame_delta : 0.050;

    host->config = config;
    host->user_data = user_data;
    host->instance = GetModuleHandleA(NULL);
    host->width = width;
    host->height = height;
    host->running = 1;

    wc.style = config->class_style != 0 ? config->class_style : (CS_HREDRAW | CS_VREDRAW | CS_OWNDC);
    wc.lpfnWndProc = kain_win32_app_window_proc;
    wc.hInstance = host->instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = class_name;

    registered_class = RegisterClassA(&wc);
    if (!registered_class && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return 0;
    }

    host->hwnd = CreateWindowExA(
        config->window_ex_style,
        class_name,
        window_title,
        config->window_style != 0 ? config->window_style : (WS_OVERLAPPEDWINDOW | WS_VISIBLE),
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        NULL,
        NULL,
        host->instance,
        host
    );
    if (!host->hwnd) {
        if (registered_class) {
            UnregisterClassA(class_name, host->instance);
        }
        return 0;
    }

    ShowWindow(host->hwnd, show_command);
    UpdateWindow(host->hwnd);

    if (config->on_init && !config->on_init(host, user_data)) {
        if (config->on_shutdown) {
            config->on_shutdown(host, user_data);
        }
        if (host->hwnd && IsWindow(host->hwnd)) {
            DestroyWindow(host->hwnd);
        }
        if (registered_class) {
            UnregisterClassA(class_name, host->instance);
        }
        return 0;
    }

    kain_win32_frame_timer_begin(
        &host->perf_freq,
        &host->prev_counter,
        &host->fps_accumulator,
        &host->fps_frames,
        &host->frame_fps
    );

    while (host->running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                host->running = 0;
            } else {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
        if (!host->running) break;

        host->frame_delta = kain_win32_frame_timer_step(
            &host->perf_freq,
            &host->prev_counter,
            &host->fps_accumulator,
            &host->fps_frames,
            &host->frame_fps,
            min_frame_delta,
            max_frame_delta
        );
        config->on_frame(host, user_data, host->frame_delta);
        Sleep((DWORD)sleep_millis);
    }

    if (config->on_shutdown) {
        config->on_shutdown(host, user_data);
    }
    if (host->hwnd && IsWindow(host->hwnd)) {
        DestroyWindow(host->hwnd);
    }
    if (registered_class) {
        UnregisterClassA(class_name, host->instance);
    }
    return 1;
}

void kain_win32_app_request_close(KainWin32AppHost* host) {
    if (!host) {
        return;
    }
    host->running = 0;
}
#endif
