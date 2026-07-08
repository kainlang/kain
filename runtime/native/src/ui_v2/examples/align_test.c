#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>

int main() {
    printf("_Alignof(uint64_t) = %zu\n", _Alignof(uint64_t));
    printf("_Alignof(int32_t) = %zu\n", _Alignof(int32_t));
    printf("_Alignof(void*) = %zu\n", _Alignof(void*));
    printf("_Alignof(float) = %zu\n", _Alignof(float));
    
    struct Test {
        uint64_t a;
        int32_t b, c, d, e;
        uint16_t f;
        uint8_t g, h;
        int16_t i, j;
    };
    printf("sizeof(struct Test) = %zu\n", sizeof(struct Test));
    printf("_Alignof(struct Test) = %zu\n", _Alignof(struct Test));
    return 0;
}
