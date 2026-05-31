#pragma once

// Simple functions with basic types that the C ABI classifier can handle
int win32_window_create(const char* title, int width, int height);
void win32_window_show(void* hwnd);
int win32_window_message_loop(void);
void win32_window_destroy(void* hwnd);
int win32_message_box(const char* text, const char* caption);
