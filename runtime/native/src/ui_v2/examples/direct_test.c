// Test arena and node functions directly
#include "kaintana.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    printf("1\n"); fflush(stdout);
    kt_init();

    printf("2\n"); fflush(stdout);
    kt_Session* s = kt_make("test", 80, 24);
    if (!s) { printf("FAIL: kt_make NULL\n"); return 1; }

    printf("3\n"); fflush(stdout);
    struct kt_Session_t* sess = (struct kt_Session_t*)s;
    printf("  nodes=%p count=%d cap=%d\n", (void*)sess->nodes, sess->node_count, sess->node_capacity);
    printf("  arena: start=%p low=%p high=%p end=%p\n",
           (void*)sess->arena.start, (void*)sess->arena.low,
           (void*)sess->arena.high, (void*)sess->arena.end);
    printf("  frame depth=%d\n", sess->arena.frame.depth);
    fflush(stdout);

    printf("4\n"); fflush(stdout);
    kt_begin(s, 16.0);
    printf("  after begin: frame depth=%d\n", sess->arena.frame.depth);
    fflush(stdout);

    // Directly call node_alloc via internal function
    printf("5: calling kt_row\n"); fflush(stdout);
    int r = kt_row(s, 0, "box", "");
    printf("6: kt_row returned %d\n", r); fflush(stdout);

    printf("7\n"); fflush(stdout);
    kt_end_row(s);

    printf("8\n"); fflush(stdout);
    kt_end(s);

    printf("9\n"); fflush(stdout);
    kt_free(s);

    printf("10 DONE\n"); fflush(stdout);
    return 0;
}
