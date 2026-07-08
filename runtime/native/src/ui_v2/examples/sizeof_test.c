
#include <stdatomic.h>
#include <stdio.h>
int main() {
    printf("sizeof(atomic_uint) = %zu
", sizeof(atomic_uint));
    printf("sizeof(atomic_int) = %zu
", sizeof(atomic_int));
    printf("sizeof(void*) = %zu
", sizeof(void*));
    return 0;
}
