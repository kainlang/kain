// Test: does accessing sess->nodes[1] hang?
// We include internal.h to directly access session struct fields
#include "kaintana.h"
#include "internal.h"
#include "backends/terminal/host_terminal.c"
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
    if (!s) { printf("FAIL\n"); return 1; }
    printf("3\n"); fflush(stdout);

    struct kt_Session_t* sess = (struct kt_Session_t*)s;
    printf("4 nodes=%p count=%d cap=%d\n", (void*)sess->nodes, sess->node_count, sess->node_capacity);
    printf("5 arena: start=%p low=%p end=%p\n", (void*)sess->arena.start, (void*)sess->arena.low, (void*)sess->arena.end);
    fflush(stdout);

    // Directly access nodes[1] (the second node)
    printf("6 accessing nodes[1]...\n"); fflush(stdout);
    KaintanaNode* n = &sess->nodes[1];
    printf("7 n=%p\n", (void*)n); fflush(stdout);

    // Write to it
    printf("8 memset...\n"); fflush(stdout);
    memset(n, 0, sizeof(KaintanaNode));
    printf("9 memset done\n"); fflush(stdout);

    // Now call kt_row to see if it works
    printf("10 kt_row...\n"); fflush(stdout);

    // But wait - we've corrupted the session state by directly modifying nodes[1]
    // without going through the proper API. Let's just test if kt_row fails or hangs.
    int r = kt_row(s, 0, "box", "testkey");
    printf("11 kt_row=%d\n", r); fflush(stdout);

    kt_free(s);
    printf("12\n"); fflush(stdout);
    return 0;
}
