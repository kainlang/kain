// Manually replicate node_alloc logic
#include "kaintana.h"
#include "backends/terminal/host_terminal.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Forward declare internal functions we need
extern void kain_arena_init_func(void* arena, int id, void* start, size_t size, int memtype);
extern void* kain_arena_alloc_lo_func(void* arena, size_t size, size_t alignment);

int main(void) {
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #endif

    printf("1\n"); fflush(stdout);
    kt_init();

    printf("2\n"); fflush(stdout);
    kt_Session* s = kt_make("test", 80, 24);
    if (!s) { printf("FAIL\n"); return 1; }
    printf("3\n"); fflush(stdout);

    // Access internal session struct (we know the layout)
    // Not portable but this is a debug test
    printf("4\n"); fflush(stdout);
    
    // Instead of calling kt_row, try the simplest test:
    // Just write a single byte to somewhere in the nodes array
    // to verify the arena allocation worked
    printf("5: calling kt_row\n"); fflush(stdout);
    int r = kt_row(s, 0, "box", "");
    printf("6: r=%d\n", r); fflush(stdout);
    
    kt_free(s);
    printf("7\n"); fflush(stdout);
    return 0;
}
