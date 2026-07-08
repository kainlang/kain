// Minimal test: init only, no rendering
#include "kaintana.h"
#include "backends/terminal/host_terminal.c"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void) {
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #endif

    printf("Step 1: Starting...\n"); fflush(stdout);

    kt_init();
    printf("Step 2: init done\n"); fflush(stdout);

    kt_Session* s = kt_make("minimal", 80, 24);
    if (!s) { fprintf(stderr, "FAIL: kt_make returned NULL\n"); return 1; }
    printf("Step 3: session created\n"); fflush(stdout);

    kt_free(s);
    printf("Step 4: freed\n"); fflush(stdout);
    printf("=== OK ===\n");
    return 0;
}
