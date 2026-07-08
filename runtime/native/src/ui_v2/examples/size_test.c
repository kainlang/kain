#include <stdatomic.h>
#include <stdio.h>
int main() {
    printf("sizeof(atomic_uint) = %zu\n", sizeof(atomic_uint));
    printf("sizeof(atomic_int) = %zu\n", sizeof(atomic_int));
    printf("sizeof(void*) = %zu\n", sizeof(void*));
    return 0;
}
