// Write to file for debugging
#include "kaintana.h"
#include "backends/terminal/host_terminal.c"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void) {
    FILE* log = fopen("X:/runtime/native/src/ui_v2/examples/debug_log.txt", "w");
    if (!log) return 1;

    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #endif

    fprintf(log, "A\n"); fflush(log);
    kt_init();
    fprintf(log, "B\n"); fflush(log);

    kt_Session* s = kt_make("test", 80, 24);
    if (!s) { fprintf(log, "FAIL\n"); fclose(log); return 1; }
    fprintf(log, "C sess=%p\n", (void*)s); fflush(log);

    kt_begin(s, 16.0);
    fprintf(log, "D\n"); fflush(log);

    fprintf(log, "E calling kt_row\n"); fflush(log);
    int r = kt_row(s, 0, "box", "");
    fprintf(log, "F kt_row=%d\n", r); fflush(log);

    fprintf(log, "G\n"); fflush(log);
    kt_free(s);
    fprintf(log, "H DONE\n"); fflush(log);
    fclose(log);
    return 0;
}
