// ============================================================================
//  hello_kaintana.c — First Kaintana UI Demo
//
//  Builds a simple UI with colored rectangles and renders to the terminal.
//  Zero platform dependencies — pure ANSI escape codes to stdout.
//  Works in any terminal (Windows Terminal, xterm, gnome-terminal, etc.).
//
//  Compile:
//    gcc -std=c11 -I ../../include -I ..  ../tree.c ../box_math.c
//        ../damage.c ../draw_pixels.c ../arena.c ../hash_table.c
//        ../color.c ../attr_table.c ../backends/terminal/host_terminal.c
//        hello_kaintana.c -o hello_kaintana.exe
//
//  Run:
//    ./hello_kaintana.exe
//
//  You should see colored blocks rendered in your terminal.
// ============================================================================
#include "kaintana.h"
#include "backends/terminal/host_terminal.c"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ── Helper: wait for keypress ───────────────────
static void wait_for_key(void) {
    printf("\nPress ENTER to quit...\n");
    getchar();
}

// ── Main ────────────────────────────────────────
int main(void) {
    // Enable ANSI on Windows 10+
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #endif

    printf("\n=== Kaintana UI Demo ===\n\n");

    kt_init();
    kt_Session* s = kt_make("demo", 80, 24);
    if (!s) { fprintf(stderr, "Failed to create session\n"); return 1; }

    // Register and select the terminal backend
    kt_backend_register(s, "terminal", &kaintana_terminal_backend);
    kt_backend_select(s, "terminal");

    // ── Frame 1: A single red rectangle ─────────
    kt_begin(s, 16.0);
    int root = kt_row(s, 0, "box", "red_box");
    kt_fill(s, root, "#FF4444");
    kt_width(s, root, 20);
    kt_height(s, root, 10);
    kt_end_row(s);
    kt_end(s);
    kt_present(s);

    printf("Frame 1: Red rectangle (%d x %d) at (0, 0)\n",
           kt_cmd_count(s) > 0 ? 20 : 0,
           kt_cmd_count(s) > 0 ? 10 : 0);
    printf("  Commands: %d\n", kt_cmd_count(s));
    wait_for_key();

    // ── Frame 2: Add a blue rectangle ─────────
    kt_begin(s, 16.0);
    root = kt_row(s, 0, "box", "row");
    kt_direction(s, root, 1);  // column
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

    printf("Frame 2: Red + Blue stacked with gap\n");
    printf("  Commands: %d\n", kt_cmd_count(s));
    wait_for_key();

    // ── Frame 3: Themed demo UI ────────────────
    kt_begin(s, 16.0);
    root = kt_row(s, 0, "box", "app");
    kt_direction(s, root, 1);
    kt_fill(s, root, "#1A1A2E");  // dark background
    kt_width(s, root, 80);
    kt_height(s, root, 24);

    // Title bar
    int title = kt_row(s, root, "box", "title");
    kt_fill(s, title, "#16213E");
    kt_width(s, title, 80);
    kt_height(s, title, 3);
    kt_end_row(s);

    // Content area with a button and a panel
    int content = kt_row(s, root, "box", "content");
    kt_direction(s, content, 0);  // row
    kt_gap(s, content, 2);

    // Button panel (left side)
    int btn_panel = kt_row(s, content, "box", "btn_panel");
    kt_fill(s, btn_panel, "#0F3460");
    kt_width(s, btn_panel, 25);
    kt_height(s, btn_panel, 18);
    kt_radius(s, btn_panel, 2);

    // A button inside
    int btn = kt_row(s, btn_panel, "box", "button");
    kt_fill(s, btn, "#533483");
    kt_width(s, btn, 20);
    kt_height(s, btn, 4);
    kt_radius(s, btn, 2);
    kt_end_row(s); // end button

    kt_end_row(s); // end btn_panel

    // Main panel (right side)
    int panel = kt_row(s, content, "box", "panel");
    kt_fill(s, panel, "#16213E");
    kt_width(s, panel, 50);
    kt_height(s, panel, 18);
    kt_radius(s, panel, 2);
    kt_end_row(s); // end panel

    kt_end_row(s); // end content
    kt_end_row(s); // end root (app)
    kt_end(s);
    kt_present(s);

    printf("Frame 3: Themed UI demo\n");
    printf("  Commands: %d\n", kt_cmd_count(s));
    wait_for_key();

    // ── Cleanup ─────────────────────────────────
    kt_free(s);
    printf("=== Done ===\n");
    return 0;
}
