// Bare minimum: kt_row without end_row, end, present
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

    printf("A\n"); fflush(stdout);
    kt_init();

    printf("B\n"); fflush(stdout);
    kt_Session* s = kt_make("test", 80, 24);
    if (!s) { printf("FAIL\n"); return 1; }

    printf("C\n"); fflush(stdout);
    kt_begin(s, 16.0);
    printf("D\n"); fflush(stdout);

    printf("E: calling kt_row\n"); fflush(stdout);
    int r = kt_row(s, 0, "box", "");
    printf("F: kt_row=%d\n", r); fflush(stdout);

    kt_free(s);
    printf("G\n"); fflush(stdout);
    return 0;
}
