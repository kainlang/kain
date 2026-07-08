// Minimal: init + make + begin only
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
    if (!s) { printf("FAIL: kt_make NULL\n"); return 1; }

    printf("C\n"); fflush(stdout);
    // Just test begin and end
    kt_begin(s, 16.0);
    printf("D begin done\n"); fflush(stdout);

    kt_free(s);
    printf("E freed\n"); fflush(stdout);
    return 0;
}
