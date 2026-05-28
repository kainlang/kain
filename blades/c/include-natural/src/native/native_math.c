#include "native_math.h"

int native_math_mix(int a, int b) {
    return (a * 9) + (b * 5) + 1;
}

int native_math_fold(int seed, int rounds) {
    int value = seed;
    for (int i = 0; i < rounds; ++i) {
        value = value + (i * 7) - 3;
    }
    return value;
}
