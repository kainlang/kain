// Ultra granular: each step of kt_row
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

    struct kt_Session_t* sess = (struct kt_Session_t*)s;
    printf("E node_count=%d capacity=%d\n", sess->node_count, sess->node_capacity); fflush(stdout);
    printf("F nodes ptr=%p\n", (void*)sess->nodes); fflush(stdout);

    // Manual node_alloc equivalent
    if (sess->node_count >= sess->node_capacity) {
        printf("FAIL: node_capacity exceeded\n"); fflush(stdout);
        kt_free(s);
        return 1;
    }
    printf("G\n"); fflush(stdout);

    int32_t idx = sess->node_count++;
    printf("H idx=%d\n", idx); fflush(stdout);

    KaintanaNode* n = &sess->nodes[idx];
    printf("I n=%p\n", (void*)n); fflush(stdout);

    memset(n, 0, sizeof(KaintanaNode));
    printf("J memset done\n"); fflush(stdout);

    n->parent_index = -1; n->first_child = -1; n->next_sibling = -1;
    n->layout_arena_index = -1; n->state_payload_offset = -1;
    n->flags = 1; // KT_NODE_VISIBLE
    printf("K fields set\n"); fflush(stdout);

    kt_end_row(s);
    printf("L end_row done\n"); fflush(stdout);

    kt_end(s);
    printf("M\n"); fflush(stdout);

    kt_free(s);
    printf("N\n"); fflush(stdout);
    return 0;
}
