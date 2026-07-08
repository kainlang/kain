// Step-by-step test to find the hanging function
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
    if (!s) { fprintf(stderr, "FAIL: kt_make NULL\n"); return 1; }

    printf("C\n"); fflush(stdout);
    kt_backend_register(s, "terminal", &kaintana_terminal_backend);
    printf("D\n"); fflush(stdout);
    kt_backend_select(s, "terminal");
    printf("E\n"); fflush(stdout);

    printf("F\n"); fflush(stdout);
    kt_begin(s, 16.0);
    printf("G\n"); fflush(stdout);

    int root = kt_row(s, 0, "box", "root");
    printf("H\n"); fflush(stdout);

    kt_fill(s, root, "#FF4444");
    printf("I\n"); fflush(stdout);

    kt_width(s, root, 20);
    printf("J\n"); fflush(stdout);

    kt_height(s, root, 10);
    printf("K\n"); fflush(stdout);

    kt_end_row(s);
    printf("L\n"); fflush(stdout);

    kt_end(s);
    printf("M\n"); fflush(stdout);

    kt_present(s);
    printf("N\n"); fflush(stdout);

    printf("Cmd count: %d\n", kt_cmd_count(s)); fflush(stdout);

    kt_free(s);
    printf("Z\n"); fflush(stdout);
    return 0;
}
