// ============================================================================
//  hello_kaintana_no_wait.c — Same as hello_kaintana.c but with no
//  getchar() waits. Runs all 3 frames immediately and exits.
// ============================================================================
#include "kaintana.h"
#include "backends/terminal/host_terminal.c"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void) {
    // Enable ANSI on Windows 10+
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #endif

    printf("\n=== Kaintana UI Demo (non-interactive) ===\n\n");

    kt_init();
    kt_Session* s = kt_make("demo", 80, 24);
    if (!s) { fprintf(stderr, "Failed to create session\n"); return 1; }

    kt_backend_register(s, "terminal", &kaintana_terminal_backend);
    kt_backend_select(s, "terminal");

    // ── Frame 1: Red rectangle ─────────
    kt_begin(s, 16.0);
    int root = kt_row(s, 0, "box", "red_box");
    kt_fill(s, root, "#FF4444");
    kt_width(s, root, 20);
    kt_height(s, root, 10);
    kt_end_row(s);
    kt_end(s);
    kt_present(s);
    printf("Frame 1: Red rectangle\n");

    // ── Frame 2: Blue rectangle ───────
    kt_begin(s, 16.0);
    root = kt_row(s, 0, "box", "row");
    kt_direction(s, root, 1);
    kt_gap(s, root, 2);

    int red = kt_row(s, root, "box", "red");
    kt_fill(s, red, "#FF4444");
    kt_width(s, red, 40);
    kt_height(s, red, 8);
    kt_radius(s, red, 2);
    kt_end_row(s);

    int blue = kt_row(s, root, "box", "blue");
    kt_fill(s, blue, "#4488FF");
    kt_width(s, blue, 40);
    kt_height(s, blue, 8);
    kt_radius(s, blue, 2);
    kt_end_row(s);

    kt_end_row(s);
    kt_end(s);
    kt_present(s);
    printf("Frame 2: Red + Blue stacked\n");

    // ── Frame 3: Themed UI ────────────
    kt_begin(s, 16.0);
    root = kt_row(s, 0, "box", "app");
    kt_direction(s, root, 1);
    kt_fill(s, root, "#1A1A2E");
    kt_width(s, root, 80);
    kt_height(s, root, 24);

    int title = kt_row(s, root, "box", "title");
    kt_fill(s, title, "#16213E");
    kt_width(s, title, 80);
    kt_height(s, title, 3);
    kt_end_row(s);

    int content = kt_row(s, root, "box", "content");
    kt_direction(s, content, 0);
    kt_gap(s, content, 2);

    int btn_panel = kt_row(s, content, "box", "btn_panel");
    kt_fill(s, btn_panel, "#0F3460");
    kt_width(s, btn_panel, 25);
    kt_height(s, btn_panel, 18);
    kt_radius(s, btn_panel, 2);
    int btn = kt_row(s, btn_panel, "box", "button");
    kt_fill(s, btn, "#533483");
    kt_width(s, btn, 20);
    kt_height(s, btn, 4);
    kt_radius(s, btn, 2);
    kt_end_row(s);
    kt_end_row(s);

    int panel = kt_row(s, content, "box", "panel");
    kt_fill(s, panel, "#16213E");
    kt_width(s, panel, 50);
    kt_height(s, panel, 18);
    kt_radius(s, panel, 2);
    kt_end_row(s);

    kt_end_row(s);
    kt_end_row(s);
    kt_end(s);
    kt_present(s);
    printf("Frame 3: Themed UI demo\n");

    printf("Commands per frame:\n");
    printf("  Frame 3 command count: %d\n", kt_cmd_count(s));

    kt_free(s);
    printf("=== Done ===\n");
    return 0;
}
