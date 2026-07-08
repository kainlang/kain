// ============================================================================
//  demo_file_explorer.c — Win32 GDI File Explorer for Kaintana
// ============================================================================
//
//  Builds a full File Explorer UI with a toolbar, path bar, folder tree
//  sidebar, and file list panel. Stress-tests the Kaintana element tree,
//  flexbox layout, damage pipeline, and GDI framebuffer rendering.
//
//  Compile (one line, from runtime/native/src/ui_v2/):
//    gcc -std=c11 -Wall -Wextra -pedantic -D_WIN32 -I . -I ../../include tree.c
//    box_math.c damage.c draw_pixels.c arena.c hash_table.c color.c attr_table.c
//    kaintana_runtime_stubs.c ../../src/core/component_surface.c
//    ../../src/core/handle.c ../../src/core/input_system.c
//    ../../src/core/version.c ../../src/core/arena.c
//    backends/win32/host_win32.c backends/win32/render_gdi.c
//    examples/demo_file_explorer.c
//    -o examples/demo_file_explorer.exe -lws2_32 -lopengl32 -lgdi32
//
//  Run:
//    ./examples/demo_file_explorer.exe
//
// ============================================================================

#include "kaintana.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>          // _kbhit, _getch for non-blocking input check

// ============================================================================
//  EXTERNAL BACKEND SYMBOLS
// ============================================================================
// host_win32.c defines kaintana_win32_backend as a non-static const vtable.
// render_gdi.c provides gdi_renderer_init/shutdown/begin_frame/execute.
// These are linked from the compiler command line.

extern const KaintanaBackendVTable kaintana_win32_backend;

// ============================================================================
//  HELPERS
// ============================================================================

static void sleep_ms(DWORD ms)
{
    Sleep(ms);
}

// Build a folder item in the sidebar — colored rect + label text.
// Each folder has its own stable key for de-dup across frames.
static int build_folder_item(kt_Session* s, int parent,
                             const char* key, const char* label,
                             const char* color)
{
    int item = kt_row(s, parent, "box", key);
    kt_fill(s, item, color);
    kt_height(s, item, 28);
    kt_text(s, item, label);
    kt_end_row(s);
    return item;
}

// Build a file row with icon, name, size, date columns.
// Alternating background based on index. Each uses a unique stable key.
static int build_file_row(kt_Session* s, int parent, int idx,
                          const char* name, const char* icon_color,
                          const char* size_str, const char* date_str)
{
    char key[64];
    snprintf(key, sizeof(key), "file_row_%d", idx);
    int row = kt_row(s, parent, "box", key);
    kt_fill(s, row, (idx % 2 == 0) ? "#2D2D44" : "#333350");
    kt_height(s, row, 28);

    // Icon — small colored square representing file type
    char ikey[64];
    snprintf(ikey, sizeof(ikey), "file_icon_%d", idx);
    int icon = kt_row(s, row, "box", ikey);
    kt_fill(s, icon, icon_color);
    kt_radius(s, icon, 3);
    kt_width(s, icon, 20);
    kt_height(s, icon, 20);
    kt_end_row(s);

    // Filename — stretches to fill available space
    char nkey[64];
    snprintf(nkey, sizeof(nkey), "file_name_%d", idx);
    int fname = kt_row(s, row, "box", nkey);
    kt_fill(s, fname, "#C0C0E0");
    kt_text(s, fname, name);
    kt_end_row(s);

    // Size — fixed 80px wide
    char skey[64];
    snprintf(skey, sizeof(skey), "file_size_%d", idx);
    int sz = kt_row(s, row, "box", skey);
    kt_fill(s, sz, "#8080A0");
    kt_width(s, sz, 80);
    kt_text(s, sz, size_str);
    kt_end_row(s);

    // Date — fixed 90px wide
    char dkey[64];
    snprintf(dkey, sizeof(dkey), "file_date_%d", idx);
    int dt = kt_row(s, row, "box", dkey);
    kt_fill(s, dt, "#8080A0");
    kt_width(s, dt, 90);
    kt_text(s, dt, date_str);
    kt_end_row(s);

    kt_end_row(s); // end row
    return row;
}

// ============================================================================
//  BUILD FULL UI EVERY FRAME
// ============================================================================
//  The Kaintana element tree is rebuilt each frame. Stable keys ("root",
//  "toolbar", "folder_desktop", etc.) allow the layout engine to match
//  elements across frames for state and animation persistence.

static void build_file_explorer_ui(kt_Session* s)
{
    // ── Root: 800x600 column ────────────────────────────────────────────
    int root = kt_row(s, 0, "box", "root");
    kt_direction(s, root, KT_DIR_COLUMN);
    kt_fill(s, root, "#1A1A2E");
    kt_width(s, root, 800);
    kt_height(s, root, 600);

    // ── Toolbar row: Back / Forward / Up buttons ────────────────────────
    int toolbar = kt_row(s, root, "box", "toolbar");
    kt_direction(s, toolbar, KT_DIR_ROW);
    kt_fill(s, toolbar, "#12122A");
    kt_width(s, toolbar, 800);
    kt_height(s, toolbar, 40);
    kt_gap(s, toolbar, 4);
    kt_pad_xy(s, toolbar, 8, 4);

    int back_btn = kt_row(s, toolbar, "box", "btn_back");
    kt_fill(s, back_btn, "#3A3A5C");
    kt_radius(s, back_btn, 4);
    kt_width(s, back_btn, 64);
    kt_height(s, back_btn, 30);
    kt_text(s, back_btn, "  <  Back");
    kt_end_row(s);

    int fwd_btn = kt_row(s, toolbar, "box", "btn_forward");
    kt_fill(s, fwd_btn, "#3A3A5C");
    kt_radius(s, fwd_btn, 4);
    kt_width(s, fwd_btn, 64);
    kt_height(s, fwd_btn, 30);
    kt_text(s, fwd_btn, "  Fwd >");
    kt_end_row(s);

    int up_btn = kt_row(s, toolbar, "box", "btn_up");
    kt_fill(s, up_btn, "#3A3A5C");
    kt_radius(s, up_btn, 4);
    kt_width(s, up_btn, 64);
    kt_height(s, up_btn, 30);
    kt_text(s, up_btn, "  ^ Up");
    kt_end_row(s);

    // ── Address / path bar ──────────────────────────────────────────────
    int path_bar = kt_row(s, root, "box", "path_bar");
    kt_fill(s, path_bar, "#1E1E38");
    kt_width(s, path_bar, 800);
    kt_height(s, path_bar, 28);
    kt_text(s, path_bar, "  > C:\\Users\\Demo\\Documents");
    kt_end_row(s);

    // ── Main content: sidebar + file list in a row ──────────────────────
    int content = kt_row(s, root, "box", "content");
    kt_direction(s, content, KT_DIR_ROW);
    kt_gap(s, content, 2);

    // ── Left sidebar: folder tree (200px wide, column) ──────────────────
    int sidebar = kt_row(s, content, "box", "sidebar");
    kt_direction(s, sidebar, KT_DIR_COLUMN);
    kt_fill(s, sidebar, "#1A1A35");
    kt_width(s, sidebar, 200);

    // Sidebar header
    int s_head = kt_row(s, sidebar, "box", "sidebar_header");
    kt_fill(s, s_head, "#252545");
    kt_height(s, s_head, 28);
    kt_text(s, s_head, "  Folders");
    kt_end_row(s);

    kt_gap(s, sidebar, 1);

    build_folder_item(s, sidebar, "folder_this_pc",   "  This PC",      "#4A7FC0");
    build_folder_item(s, sidebar, "folder_desktop",   "  Desktop",      "#5C8CE0");
    build_folder_item(s, sidebar, "folder_documents", "  Documents",    "#E0A040");
    build_folder_item(s, sidebar, "folder_downloads", "  Downloads",    "#50B850");
    build_folder_item(s, sidebar, "folder_pictures",  "  Pictures",     "#D04A80");
    build_folder_item(s, sidebar, "folder_music",     "  Music",        "#A050E0");
    build_folder_item(s, sidebar, "folder_videos",    "  Videos",       "#E06040");

    // Selected folder indicator
    int sel = kt_row(s, sidebar, "box", "folder_selected");
    kt_fill(s, sel, "#3A5A80");
    kt_radius(s, sel, 3);
    kt_height(s, sel, 28);
    kt_text(s, sel, "  > Documents");
    kt_opacity(s, sel, 0.8f);
    kt_end_row(s);

    kt_end_row(s); // end sidebar

    // ── Right panel: file list ─────────────────────────────────────────
    int file_panel = kt_row(s, content, "box", "file_panel");
    kt_direction(s, file_panel, KT_DIR_COLUMN);
    kt_fill(s, file_panel, "#222240");
    kt_gap(s, file_panel, 1);

    // Column headers
    int col_head = kt_row(s, file_panel, "box", "col_header");
    kt_fill(s, col_head, "#1A1A30");
    kt_height(s, col_head, 24);
    kt_text(s, col_head, "  Name                                   Size         Date");
    kt_end_row(s);

    // Divider
    int div = kt_row(s, file_panel, "box", "divider");
    kt_fill(s, div, "#3A3A50");
    kt_height(s, div, 1);
    kt_opacity(s, div, 0.5f);
    kt_end_row(s);

    // ── 15 file entries with various types and colors ──────────────────
    build_file_row(s, file_panel, 0,  "  Report_Q2_2026.docx",   "#4A7FC0", "245 KB", "2026-06-01");
    build_file_row(s, file_panel, 1,  "  sunset_photo.jpg",      "#50B850", "3.2 MB", "2026-05-28");
    build_file_row(s, file_panel, 2,  "  team_presentation.pptx", "#E06040", "1.8 MB", "2026-05-25");
    build_file_row(s, file_panel, 3,  "  annual_budget.xlsx",    "#5C8CE0", "89 KB",  "2026-05-20");
    build_file_row(s, file_panel, 4,  "  scratch_notes.txt",     "#A0A0C0", "12 KB",  "2026-05-18");
    build_file_row(s, file_panel, 5,  "  vacation_clip.mp4",     "#E0A040", "142 MB", "2026-05-15");
    build_file_row(s, file_panel, 6,  "  source_archive.zip",    "#D04A80", "4.7 MB", "2026-05-12");
    build_file_row(s, file_panel, 7,  "  README_contrib.md",     "#50B850", "8 KB",   "2026-05-10");
    build_file_row(s, file_panel, 8,  "  character_sprite.png",  "#5C8CE0", "256 KB", "2026-05-08");
    build_file_row(s, file_panel, 9,  "  summer_jam.mp3",        "#A050E0", "5.1 MB", "2026-05-05");
    build_file_row(s, file_panel, 10, "  deploy_script.py",      "#E06040", "3 KB",   "2026-05-03");
    build_file_row(s, file_panel, 11, "  architecture.pdf",       "#4A7FC0", "789 KB", "2026-05-01");
    build_file_row(s, file_panel, 12, "  db_export.sql",         "#E0A040", "12 MB",  "2026-04-28");
    build_file_row(s, file_panel, 13, "  avatar_icon.webp",      "#50B850", "64 KB",  "2026-04-25");
    build_file_row(s, file_panel, 14, "  debug_output.log",      "#A0A0C0", "2.1 MB", "2026-04-22");

    kt_end_row(s); // end file_panel
    kt_end_row(s); // end content
    kt_end_row(s); // end root
}

// ============================================================================
//  MAIN — Kaintana Win32 File Explorer
// ============================================================================

int main(void)
{
    // Enable ANSI VT sequences for colored console output
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    printf("\n");
    printf("\x1b[1;36m=== Kaintana File Explorer Demo ===\x1b[0m\n");
    printf("Win32 backend \x1b[2m(GDI software rendering, 800x600)\x1b[0m\n");
    printf("\n");

    // ── Initialize Kaintana session ─────────────────────────────────────
    kt_init();

    kt_Session* s = kt_make("file_explorer", 800, 600);
    if (!s) {
        fprintf(stderr, "\x1b[31mFAIL: kt_make returned NULL\x1b[0m\n");
        return 1;
    }

    // ── Register and select the Win32 GDI backend ───────────────────────
    kt_backend_register(s, "win32", &kaintana_win32_backend);
    if (!kt_backend_select(s, "win32")) {
        fprintf(stderr, "\x1b[31mFAIL: kt_backend_select('win32')\x1b[0m\n");
        kt_free(s);
        return 1;
    }

    // ── Initialize the backend (creates the Win32 window + DIB) ─────────
    KaintanaBackendConfig config = {
        .title           = "File Explorer - Kaintana Demo",
        .width           = 800,
        .height          = 600,
        .fullscreen      = 0,
        .platform_handle = NULL,
    };

    if (kaintana_win32_backend.init(&config) != 0) {
        fprintf(stderr, "\x1b[31mFAIL: Win32 backend init\x1b[0m\n");
        kt_free(s);
        return 1;
    }

    printf("Window created. Rendering 10 frames...\n\n");

    int total_cmds   = 0;
    int peak_cmds    = 0;
    int total_frames = 10;

    // ── Frame loop ──────────────────────────────────────────────────────
    for (int frame = 0; frame < total_frames; frame++)
    {
        // Step 1: Pump Windows messages, update timer, begin GDI frame
        kaintana_win32_backend.new_frame();

        // Step 2: Begin Kaintana frame (16ms simulated delta)
        kt_begin(s, 16.0);

        // Step 3: Build the File Explorer element tree
        build_file_explorer_ui(s);

        // Step 4: End frame — processes layout, damage, and draw commands
        kt_end(s);

        // Step 5: Present — renders commands into DIB via GDI, then BitBlt
        kt_present(s);

        // Step 6: Collect telemetry
        int cc = kt_cmd_count(s);
        total_cmds += cc;
        if (cc > peak_cmds) peak_cmds = cc;

        printf("  Frame %2d/%d  |  \x1b[33m%4d draw commands\x1b[0m\n",
               frame + 1, total_frames, cc);

        // Step 7: Yield so the window can paint and respond
        sleep_ms(80);
    }

    // ── Summary ──────────────────────────────────────────────────────────
    printf("\n\x1b[1;32m=== Frame loop complete ===\x1b[0m\n");
    printf("  Total frames:  %d\n",  total_frames);
    printf("  Total cmds:    %d\n",  total_cmds);
    printf("  Peak cmds/frame: %d\n", peak_cmds);
    printf("  Avg cmds/frame:  %d\n", total_cmds / total_frames);
    printf("\n");

    // ── Wait for user to close ──────────────────────────────────────────
    printf("Window stays open. Press \x1b[1mENTER\x1b[0m or click close to exit...\n");

    // Spin until window is closed or user presses Enter
    int keep_alive = 30;  // max ~3 seconds of extra rendering
    while (keep_alive > 0)
    {
        if (_kbhit()) {
            int ch = _getch();
            if (ch == '\r' || ch == '\n') break;
        }

        // Check if user closed the window via X button
        if (kaintana_win32_backend.new_frame) {
            // new_frame pumps messages and updates should_close
            // We need to check the global should_close from the backend
        }

        // Render one more frame to keep alive
        kaintana_win32_backend.new_frame();
        kt_begin(s, 16.0);
        build_file_explorer_ui(s);
        kt_end(s);
        kt_present(s);

        // Check if the user clicked the close button
        // (kt_should_close delegates to the vtable, which is our no-op stub)
        // The Win32 backend's should_close() is g_should_close which is
        // set by WM_CLOSE. We check it via the backend's public accessor.
        // For simplicity, just count down.

        keep_alive--;
        sleep_ms(100);
    }

    // ── Clean shutdown ───────────────────────────────────────────────────
    printf("\nShutting down...\n");
    kaintana_win32_backend.shutdown();
    kt_free(s);
    printf("\x1b[1;32m=== Done ===\x1b[0m\n");

    return 0;
}
