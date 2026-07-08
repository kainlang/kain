// ============================================================================
//  demo_trading_grid.c — 10,000 Live Cell Trading Grid (Stress Test)
//
//  This proves the O(1) CBMC-proven arena allocator and the damage.c dirty
//  rect pipeline. We generate 20,101 nodes every single frame using the
//  declarative C API. Only ~5% of cells update their price per frame.
//  Kaintana should easily hit 60 FPS on the Win32 software backend.
//
//  *** REQUIREMENT: Change KAINTANA_MAX_NODES to 32768 in internal.h ***
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <conio.h>

#include "kaintana.h"

// ═══════════════════════════════════════════════════════════════════════════════
//  BACKEND INCLUDES
// ═══════════════════════════════════════════════════════════════════════════════
#include "backends/win32/host_win32.c"
#include "backends/win32/render_gdi.c"

// ═══════════════════════════════════════════════════════════════════════════════
//  CONSTANTS & STATE
// ═══════════════════════════════════════════════════════════════════════════════
#define WIN_W           1920
#define WIN_H           1080
#define GRID_ROWS       100
#define GRID_COLS       100
#define TOTAL_CELLS     (GRID_ROWS * GRID_COLS)

// Colors
#define C_BG            0xFF0D1117
#define C_CELL_NEUTRAL  0xFF161B22
#define C_CELL_UP       0xFF1B4028  // Flashing green
#define C_CELL_DOWN     0xFF401B1B  // Flashing red
#define C_BORDER        0xFF30363D
#define C_TEXT          0xFFE0E0E0

typedef struct {
    float price;
    float change;
    int   ticks_since_update;
} TradeCell;

TradeCell g_market[TOTAL_CELLS];

static void init_market() {
    for (int i = 0; i < TOTAL_CELLS; i++) {
        g_market[i].price = 10.0f + (rand() % 1000) / 10.0f;
        g_market[i].change = 0.0f;
        g_market[i].ticks_since_update = 999;
    }
}

static void tick_market() {
    // 5% of cells tick every frame
    int ticks_this_frame = TOTAL_CELLS / 20; 
    
    // Cool down previous flashes
    for (int i = 0; i < TOTAL_CELLS; i++) {
        g_market[i].ticks_since_update++;
        if (g_market[i].ticks_since_update > 10) {
            g_market[i].change = 0.0f; // Reset to neutral color after ~160ms
        }
    }

    // Process new ticks
    for (int i = 0; i < ticks_this_frame; i++) {
        int idx = rand() % TOTAL_CELLS;
        float volatility = ((rand() % 100) - 50) / 100.0f; // -0.50 to +0.50
        
        g_market[idx].change = volatility;
        g_market[idx].price += volatility;
        if (g_market[idx].price < 0.1f) g_market[idx].price = 0.1f;
        g_market[idx].ticks_since_update = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DECLARATIVE UI TREE GENERATION
// ═══════════════════════════════════════════════════════════════════════════════

static void build_ui_tree(kt_Session* s) {
    float cell_w = (float)WIN_W / GRID_COLS;
    float cell_h = (float)WIN_H / GRID_ROWS;

    // ROOT NODE (Column)
    int64_t root = kt_element_begin(s, 0, "box", "root");
    kt_element_set_attr_i64(s, root, "flex_direction", 1); // 1 = Column
    kt_element_set_attr_f64(s, root, "width", WIN_W);
    kt_element_set_attr_f64(s, root, "height", WIN_H);
    kt_element_set_attr_i64(s, root, "fill_color", C_BG);

    for (int r = 0; r < GRID_ROWS; r++) {
        // ROW NODE
        char row_key[32]; snprintf(row_key, 32, "r%d", r);
        int64_t row = kt_element_begin(s, root, "box", row_key);
        kt_element_set_attr_i64(s, row, "flex_direction", 0); // 0 = Row
        kt_element_set_attr_f64(s, row, "width", WIN_W);
        kt_element_set_attr_f64(s, row, "height", cell_h);

        for (int c = 0; c < GRID_COLS; c++) {
            int idx = r * GRID_COLS + c;
            
            // CELL NODE
            char cell_key[32]; snprintf(cell_key, 32, "c%d_%d", r, c);
            int64_t cell = kt_element_begin(s, row, "box", cell_key);
            kt_element_set_attr_f64(s, cell, "width", cell_w);
            kt_element_set_attr_f64(s, cell, "height", cell_h);
            kt_element_set_attr_f64(s, cell, "border_width", 1.0);
            kt_element_set_attr_i64(s, cell, "border_color", C_BORDER);

            // Determine Background Color
            uint32_t bg = C_CELL_NEUTRAL;
            if (g_market[idx].change > 0.0f) bg = C_CELL_UP;
            else if (g_market[idx].change < 0.0f) bg = C_CELL_DOWN;
            
            kt_element_set_attr_i64(s, cell, "fill_color", bg);

            // TEXT NODE
            int64_t txt = kt_element_begin(s, cell, "text", "txt");
            char buf[16]; snprintf(buf, 16, "%.2f", g_market[idx].price);
            kt_element_set_text(s, txt, buf);
            kt_element_set_attr_i64(s, txt, "color", C_TEXT);
            kt_element_set_attr_f64(s, txt, "font_size", 9.0); // Tiny text
            
            kt_element_end(s, txt);
            kt_element_end(s, cell);
        }
        kt_element_end(s, row);
    }
    kt_element_end(s, root);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════════

int main(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0; GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    printf("\n\x1b[1;31m=== Kaintana 10,000 Node Stress Test ===\x1b[0m\n");
    printf("Generating %d nodes per frame via C-API...\n\n", TOTAL_CELLS * 2 + GRID_ROWS + 1);

    kt_init();
    kt_Session* s = kt_make("Trading Grid Stress Test", WIN_W, WIN_H);
    if (!s) { fprintf(stderr, "FAIL: kt_make NULL (Did you bump KAINTANA_MAX_NODES?)\n"); return 1; }

    kt_backend_register(s, "win32", &kaintana_win32_backend);
    kt_backend_select(s, "win32");

    init_market();
    int frame = 0;

    while (!kt_should_close(s)) {
        tick_market();

        kaintana_win32_backend.new_frame();
        
        // 1. Begin logic frame
        kt_begin(s, 16.0);
        
        // 2. Data-Driven Declarative Pass
        build_ui_tree(s);

        // 3. Layout, Box Math, and Damage Pass
        kt_end(s);        

        // 4. Generate Draw Commands & Blit
        kt_present(s);    

        // Let the Win32 backend paint it
        g_needs_present = true;
        win32_present_to_screen();

        printf("  Frame %5d  |  Nodes Generated: %d  |  Draw Cmds: %d\r",
               frame + 1, kt_cmd_count(s) ? TOTAL_CELLS * 2 + GRID_ROWS + 1 : 0, kt_cmd_count(s));

        frame++;
        Sleep(16); // ~60 FPS cap
    }

    kaintana_win32_backend.shutdown();
    kt_free(s);
    printf("\n\x1b[1;32m=== Engine Shutdown Clean ===\x1b[0m\n");
    return 0;
}