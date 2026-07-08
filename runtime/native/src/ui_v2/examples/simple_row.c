// Even simpler: create session, begin, kt_row with empty key
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
    kt_backend_register(s, "terminal", &kaintana_terminal_backend);
    kt_backend_select(s, "terminal");

    printf("D\n"); fflush(stdout);
    kt_begin(s, 16.0);

    printf("E\n"); fflush(stdout);
    // Try kt_row with empty key (no hash table)
    int root = kt_row(s, 0, "box", "");
    printf("F root=%d\n", root); fflush(stdout);

    kt_end_row(s);
    printf("G\n"); fflush(stdout);
    kt_end(s);
    printf("H\n"); fflush(stdout);
    kt_present(s);
    printf("I count=%d\n", kt_cmd_count(s)); fflush(stdout);

    kt_free(s);
    printf("Z\n"); fflush(stdout);
    return 0;
}
