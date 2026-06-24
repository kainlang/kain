// ============================================================================
//  Minimal Diagnostic — bare Kain session without subclassing
//  ============================================================================
//  Tests only whether abi_ui_host_attach("winit") creates a visible window.
//  No subclassing. Just creates the window, waits 5 seconds, then exits.
//  ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "ui_system.h"

int main(void) {
    printf("=== Minimal Diagnostic ===\n\n");

    // Init
    abi_ui_reset();

    int64_t session = abi_ui_session_create("Diag", 800, 600);
    printf("Session: %lld\n", (long long)session);

    if (abi_ui_window_open(session, "Diagnostic Window", 800, 600) != 0) {
        fprintf(stderr, "FAIL: window_open\n"); return 1;
    }

    printf("Attaching win32 host...\n");
    int64_t status = abi_ui_host_attach(session, "winit");
    printf("host_attach status: %lld\n", (long long)status);

    if (status != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }

    printf("Backend: %s\n", abi_ui_host_backend(session));

    // Wait 5 seconds so user can see the window (if it appears)
    printf("Waiting 5 seconds...\n");
    for (int i = 0; i < 50; i++) {
        abi_ui_host_pump(session);
        Sleep(100);
        if (abi_ui_host_should_close(session)) {
            printf("Window closed early!\n");
            break;
        }
    }

    printf("Shutting down...\n");
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
