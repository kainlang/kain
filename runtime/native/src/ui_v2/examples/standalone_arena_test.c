// Pure standalone: test arena + node allocation WITHOUT kaintana.h
// This tests if the runtime arena.c functions work correctly standalone.
#include "component_surface.h"
#include "arena.h"
#include "handle.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Minimal session struct (not the full KaintanaSession)
typedef struct {
    KainArena arena;
    unsigned char arena_buffer[65536];
    void* nodes;
    int node_count;
    int node_capacity;
} MiniSession;

int main(void) {
    printf("1\n"); fflush(stdout);

    MiniSession* sess = calloc(1, sizeof(MiniSession));
    if (!sess) { printf("FAIL: calloc\n"); return 1; }
    printf("2 sess=%p\n", (void*)sess); fflush(stdout);

    sess->node_capacity = 128;
    printf("3 cap=%d\n", sess->node_capacity); fflush(stdout);

    int r = kain_arena_init(&sess->arena, KAIN_ARENA_MAIN,
                            sess->arena_buffer, sizeof(sess->arena_buffer),
                            KAIN_MEMTYPE_DEFAULT);
    printf("4 arena_init=%d\n", r); fflush(stdout);
    printf("  arena: start=%p low=%p high=%p\n",
           (void*)sess->arena.start, (void*)sess->arena.low,
           (void*)sess->arena.high);

    // Allocate nodes
    sess->nodes = kain_arena_alloc_lo(&sess->arena,
                     sess->node_capacity * 32, 8);  // 32-byte nodes, 8-byte alignment
    printf("5 nodes=%p\n", sess->nodes); fflush(stdout);
    if (!sess->nodes) { printf("FAIL: node alloc\n"); free(sess); return 1; }

    sess->node_count = 1;
    printf("6 count=%d\n", sess->node_count); fflush(stdout);

    // Mark frame
    r = kain_frame_set_marker(&sess->arena);
    printf("7 marker=%d depth=%d\n", r, sess->arena.frame.depth); fflush(stdout);

    // Allocate a node (like node_alloc)
    if (sess->node_count >= sess->node_capacity) { printf("OOM\n"); free(sess); return 1; }
    int idx = sess->node_count++;
    printf("8 idx=%d\n", idx); fflush(stdout);

    unsigned char* n = (unsigned char*)sess->nodes + idx * 32;
    memset(n, 0, 32);
    printf("9 memset done at %p\n", (void*)n); fflush(stdout);
    
    // Simulate what kt_row does after node_alloc
    printf("10 all good\n"); fflush(stdout);

    free(sess);
    printf("11 DONE\n"); fflush(stdout);
    return 0;
}
