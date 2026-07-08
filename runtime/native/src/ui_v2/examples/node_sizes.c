#include "kaintana.h"
#include <stdio.h>

// The actual internal.h defines KaintanaNode, but we can't include internal.h
// because it pulls in too much. Let's just check alignment/size of a dummy.
typedef struct {
    uint64_t    a;
    int32_t     b, c, d, e;
    uint16_t    f;
    uint8_t     g, h;
    int16_t     i, j;
} TestNode;

int main() {
    printf("sizeof(TestNode) = %zu\n", sizeof(TestNode));
    printf("offsetof(a)=%zu offsetof(b)=%zu offsetof(c)=%zu offsetof(d)=%zu\n",
           offsetof(TestNode, a), offsetof(TestNode, b),
           offsetof(TestNode, c), offsetof(TestNode, d));
    return 0;
}
