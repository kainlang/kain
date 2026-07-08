// kt_row WITHOUT kt_begin
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

    // Call kt_row WITHOUT kt_begin
    printf("D calling kt_row\n"); fflush(stdout);
    int r = kt_row(s, 0, "box", "");
    printf("E kt_row=%d\n", r); fflush(stdout);

    printf("F\n"); fflush(stdout);
    kt_end_row(s);

    printf("G\n"); fflush(stdout);
    kt_free(s);
    printf("H\n"); fflush(stdout);
    return 0;
}
