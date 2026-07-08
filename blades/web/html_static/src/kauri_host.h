// kauri_host.h — Frameless WebView2 host window
// Include from Kain:  include "kauri_host.h" as host
// Discovers kauri_host.c beside it automatically.

#ifndef KAURI_HOST_H
#define KAURI_HOST_H

#include <stdint.h>

// Create a frameless WebView2 window loading the given URL.
// width/height: initial window size in pixels (0 = 800x600 default).
// Returns 0 on success, nonzero on error.
int kauri_host_init(const char* url, int32_t width, int32_t height);

// Non-blocking message pump. Call from the Kain event loop.
// Processes window messages without blocking.
// Returns 0 while window is alive, 1 when window has been closed.
int kauri_host_poll(void);

// Set a custom title for the frameless window (visible via accessibility/taskbar).
void kauri_host_set_title(const char* title);

// Clean up and destroy the window.
void kauri_host_shutdown(void);

#endif // KAURI_HOST_H
